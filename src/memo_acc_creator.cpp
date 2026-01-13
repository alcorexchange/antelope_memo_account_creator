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

    // Minimum check (rough estimate: stakes + some for RAM)
    int64_t min_required = cfg.cpu_stake + cfg.net_stake + 1000;
    check(quantity.amount >= min_required,
          "Insufficient funds for account creation");

    // Parse public key
    eosio::public_key pubkey = parse_pubkey(memo);

    // Find available account name
    name new_account = find_available_name(memo);

    // Get current balance BEFORE processing (balance already includes received quantity)
    asset balance_before = get_balance(get_self(), core_sym);

    // Call process action inline
    action(
        permission_level{get_self(), "active"_n},
        get_self(),
        "process"_n,
        std::make_tuple(new_account, pubkey, balance_before)
    ).send();
}

// ==================== ACTION: process ====================

ACTION memo_acc_creator::process(name new_account, eosio::public_key pubkey, asset balance_before) {
    require_auth(get_self());

    // Load config
    config_t config_tbl(get_self(), get_self().value);
    config cfg = config_tbl.get_or_default();

    symbol core_sym = EOSIOSystem::getCoreSymbol();
    asset cpu_stake_asset = asset(cfg.cpu_stake, core_sym);
    asset net_stake_asset = asset(cfg.net_stake, core_sym);

    // Create account
    create_account(new_account, pubkey);

    // Buy RAM
    buy_ram(new_account, cfg.ram_bytes);

    // Delegate CPU/NET (if configured)
    if (cfg.cpu_stake > 0 || cfg.net_stake > 0) {
        delegate_bw(new_account, net_stake_asset, cpu_stake_asset);
    }

    // Call finalize action inline to transfer remainder
    action(
        permission_level{get_self(), "active"_n},
        get_self(),
        "finalize"_n,
        std::make_tuple(new_account, balance_before)
    ).send();
}

// ==================== ACTION: finalize ====================

ACTION memo_acc_creator::finalize(name new_account, asset balance_before) {
    require_auth(get_self());

    symbol core_sym = EOSIOSystem::getCoreSymbol();

    // Get current balance AFTER all purchases
    asset balance_after = get_balance(get_self(), core_sym);

    // Calculate remainder (what's left after RAM purchase and staking)
    asset remainder = balance_after - balance_before;

    // Transfer remainder to new account if positive
    // Note: remainder should be negative or zero normally,
    // but we keep buffer. Only transfer if there's excess.
    // Actually: balance_before was AFTER receiving, so remainder = current - before_spending
    // We want to keep some buffer, transfer only excess above initial balance

    // Simpler: just check if balance > some minimum buffer, transfer the rest
    // Or: transfer (balance_after - small_buffer) to new account

    // For now: if balance_after > 0, transfer most of it keeping small buffer
    int64_t buffer = 100000000; // 1 WAX buffer (8 decimals)
    if (balance_after.amount > buffer) {
        asset to_transfer = balance_after - asset(buffer, core_sym);
        if (to_transfer.amount > 0) {
            transfer_tokens(new_account, to_transfer, "Account created");
        }
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
        std::make_tuple(get_self(), receiver, net, cpu, true)  // transfer ownership
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

// ==================== PRIVATE: get_balance ====================

asset memo_acc_creator::get_balance(name owner, symbol sym) {
    struct account {
        asset balance;
        uint64_t primary_key() const { return balance.symbol.code().raw(); }
    };
    typedef eosio::multi_index<"accounts"_n, account> accounts_t;

    accounts_t accounts(EOSIOSystem::CORE_TOKEN_ACCOUNT, owner.value);
    auto it = accounts.find(sym.code().raw());
    if (it == accounts.end()) {
        return asset(0, sym);
    }
    return it->balance;
}
