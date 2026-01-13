#include <memo_acc_creator.hpp>

// Base58 alphabet (Bitcoin style)
static const char* BASE58_CHARS = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";

// Base58 decode helper
static std::vector<unsigned char> base58_decode(const std::string& str) {
    std::vector<unsigned char> result;
    result.reserve(str.size());

    for (char c : str) {
        const char* p = strchr(BASE58_CHARS, c);
        check(p != nullptr, "Invalid base58 character");

        int carry = p - BASE58_CHARS;
        for (auto& byte : result) {
            carry += 58 * byte;
            byte = carry % 256;
            carry /= 256;
        }
        while (carry > 0) {
            result.push_back(carry % 256);
            carry /= 256;
        }
    }

    // Handle leading '1's (zeros in base58)
    for (size_t i = 0; i < str.size() && str[i] == '1'; ++i) {
        result.push_back(0);
    }

    std::reverse(result.begin(), result.end());
    return result;
}

// ==================== ACTION: setconfig ====================

ACTION memo_acc_creator::setconfig(int64_t cpu_stake, int64_t net_stake, uint32_t ram_bytes) {
    require_auth(get_self());

    check(cpu_stake >= 0, "cpu_stake must be non-negative");
    check(net_stake >= 0, "net_stake must be non-negative");
    check(ram_bytes >= 2048, "ram_bytes must be at least 2048");

    config_t config_tbl(get_self(), get_self().value);
    config cfg = config_tbl.get_or_default();
    cfg.cpu_stake = cpu_stake;
    cfg.net_stake = net_stake;
    cfg.ram_bytes = ram_bytes;
    config_tbl.set(cfg, get_self());
}

// ==================== NOTIFICATION: on_transfer ====================

void memo_acc_creator::on_transfer(name from, name to, asset quantity, std::string memo) {
    // Skip outgoing and irrelevant transfers
    if (from == get_self() || to != get_self()) {
        return;
    }

    // Validate token is core system token
    symbol core_sym = EOSIOSystem::getCoreSymbol();
    check(quantity.symbol == core_sym, "Only core token accepted");
    check(quantity.is_valid(), "Invalid quantity");
    check(quantity.amount > 0, "Quantity must be positive");

    // Validate memo contains public key
    check(memo.length() > 0, "Memo must contain public key");
    check(
        memo.substr(0, 7) == "PUB_K1_" || memo.substr(0, 3) == "EOS",
        "Memo must be public key (PUB_K1_... or EOS...)"
    );

    // Load config
    config_t config_tbl(get_self(), get_self().value);
    config cfg = config_tbl.get_or_default();

    // Parse public key
    eosio::public_key pubkey = parse_pubkey(memo);

    // Find available account name
    name new_account = find_available_name(memo);

    // Calculate stakes
    asset cpu_stake_asset = asset(cfg.cpu_stake, core_sym);
    asset net_stake_asset = asset(cfg.net_stake, core_sym);

    // Minimum check (rough estimate: stakes + some for RAM)
    int64_t min_required = cfg.cpu_stake + cfg.net_stake + 1000;
    check(quantity.amount >= min_required,
          "Insufficient funds for account creation");

    // Create account
    create_account(new_account, pubkey);

    // Buy RAM
    buy_ram(new_account, cfg.ram_bytes);

    // Delegate CPU/NET (if configured)
    if (cfg.cpu_stake > 0 || cfg.net_stake > 0) {
        delegate_bw(new_account, net_stake_asset, cpu_stake_asset);
    }

    // Transfer remainder to new account
    asset used = cpu_stake_asset + net_stake_asset;
    asset remainder = quantity - used;
    if (remainder.amount > 0) {
        transfer_tokens(new_account, remainder, "Account created");
    }
}

// ==================== PRIVATE: parse_pubkey ====================

eosio::public_key memo_acc_creator::parse_pubkey(const std::string& pubkey_str) {
    std::string key_data;

    if (pubkey_str.substr(0, 7) == "PUB_K1_") {
        key_data = pubkey_str.substr(7);
    } else if (pubkey_str.substr(0, 3) == "EOS") {
        key_data = pubkey_str.substr(3);
    } else {
        check(false, "Invalid public key format");
    }

    std::vector<unsigned char> decoded = base58_decode(key_data);
    check(decoded.size() >= 37, "Invalid public key length"); // 33 key + 4 checksum

    // Create K1 public key (index 0 in variant)
    std::array<char, 33> key_bytes;
    std::copy(decoded.begin(), decoded.begin() + 33, key_bytes.begin());

    // Return K1 variant (index 0)
    return eosio::public_key{std::in_place_index<0>, key_bytes};
}

// ==================== PRIVATE: generate_name_with_salt ====================

name memo_acc_creator::generate_name_with_salt(const std::string& pubkey_str, uint32_t salt) {
    // Prepare data for hashing: pubkey string + salt
    std::string data = pubkey_str;
    if (salt > 0) {
        data += std::to_string(salt);
    }

    // SHA256 hash
    checksum256 hash = sha256(data.data(), data.size());
    auto hash_bytes = hash.extract_as_byte_array();

    // Generate 12-character name
    char name_str[13] = {0};
    for (int i = 0; i < NAME_LENGTH; i++) {
        uint8_t byte = hash_bytes[i];
        uint8_t index = byte % ALPHABET_SIZE;
        name_str[i] = NAME_ALPHABET[index];
    }

    return name(name_str);
}

// ==================== PRIVATE: find_available_name ====================

name memo_acc_creator::find_available_name(const std::string& pubkey_str) {
    uint32_t salt = 0;

    while (salt < MAX_SALT_ATTEMPTS) {
        name candidate = generate_name_with_salt(pubkey_str, salt);

        if (!is_account(candidate)) {
            return candidate;
        }

        salt++;
    }

    check(false, "Could not find available account name after max attempts");
    return name(); // Never reached
}

// ==================== PRIVATE: create_authority ====================

authority memo_acc_creator::create_authority(const eosio::public_key& pubkey) {
    return authority{
        .threshold = 1,
        .keys = {{.key = pubkey, .weight = 1}},
        .accounts = {},
        .waits = {}
    };
}

// ==================== PRIVATE: create_account ====================

void memo_acc_creator::create_account(name new_account, const eosio::public_key& pubkey) {
    authority auth = create_authority(pubkey);

    action(
        permission_level{get_self(), "active"_n},
        "eosio"_n,
        "newaccount"_n,
        std::make_tuple(get_self(), new_account, auth, auth)
    ).send();
}

// ==================== PRIVATE: buy_ram ====================

void memo_acc_creator::buy_ram(name receiver, uint32_t bytes) {
    action(
        permission_level{get_self(), "active"_n},
        "eosio"_n,
        "buyrambytes"_n,
        std::make_tuple(get_self(), receiver, bytes)
    ).send();
}

// ==================== PRIVATE: delegate_bw ====================

void memo_acc_creator::delegate_bw(name receiver, asset net, asset cpu) {
    action(
        permission_level{get_self(), "active"_n},
        "eosio"_n,
        "delegatebw"_n,
        std::make_tuple(get_self(), receiver, net, cpu, false)
    ).send();
}

// ==================== PRIVATE: transfer_tokens ====================

void memo_acc_creator::transfer_tokens(name to, asset quantity, std::string memo) {
    action(
        permission_level{get_self(), "active"_n},
        EOSIOSystem::CORE_TOKEN_ACCOUNT,
        "transfer"_n,
        std::make_tuple(get_self(), to, quantity, memo)
    ).send();
}
