#pragma once

#include <eosio/eosio.hpp>
#include <eosio/asset.hpp>
#include <eosio/crypto.hpp>
#include <eosio/singleton.hpp>
#include "eosio_system.hpp"

using namespace eosio;

// Name generation alphabet (31 characters - valid EOSIO name chars except '.')
static constexpr char NAME_ALPHABET[] = "abcdefghijklmnopqrstuvwxyz12345";
static constexpr uint8_t ALPHABET_SIZE = 31;
static constexpr uint8_t NAME_LENGTH = 12;
static constexpr uint32_t MAX_SALT_ATTEMPTS = 100;

// Authority structures for newaccount action
struct key_weight {
    eosio::public_key key;
    uint16_t weight;
    EOSLIB_SERIALIZE(key_weight, (key)(weight))
};

struct permission_level_weight {
    eosio::permission_level permission;
    uint16_t weight;
    EOSLIB_SERIALIZE(permission_level_weight, (permission)(weight))
};

struct wait_weight {
    uint32_t wait_sec;
    uint16_t weight;
    EOSLIB_SERIALIZE(wait_weight, (wait_sec)(weight))
};

struct authority {
    uint32_t threshold;
    std::vector<key_weight> keys;
    std::vector<permission_level_weight> accounts;
    std::vector<wait_weight> waits;
    EOSLIB_SERIALIZE(authority, (threshold)(keys)(accounts)(waits))
};

CONTRACT memo_acc_creator : public contract {
public:
    using contract::contract;

    // Config table (singleton)
    TABLE config {
        int64_t cpu_stake = 1000;   // 0.1000 (4 decimals)
        int64_t net_stake = 1000;   // 0.1000 (4 decimals)
        uint32_t ram_bytes = 4096;
    };
    typedef singleton<"config"_n, config> config_t;

    // Admin action to update config
    ACTION setconfig(int64_t cpu_stake, int64_t net_stake, uint32_t ram_bytes);

    // Internal actions for processing flow
    ACTION process(name new_account, eosio::public_key pubkey, symbol token_sym);
    ACTION finalize(name new_account, symbol token_sym);

    // Transfer notification handler
    [[eosio::on_notify("eosio.token::transfer")]]
    void on_transfer(name from, name to, asset quantity, std::string memo);

private:
    // Parse public key from string (PUB_K1_... or EOS...)
    eosio::public_key parse_pubkey(const std::string& pubkey_str);

    // Generate account name from public key with optional salt
    name generate_name_with_salt(const std::string& pubkey_str, uint32_t salt);

    // Find available account name (handles collisions)
    name find_available_name(const std::string& pubkey_str);

    // Create authority struct from public key
    authority create_authority(const eosio::public_key& pubkey);

    // Inline action: create new account
    void create_account(name new_account, const eosio::public_key& pubkey);

    // Inline action: buy RAM
    void buy_ram(name receiver, uint32_t bytes);

    // Inline action: delegate bandwidth
    void delegate_bw(name receiver, asset net, asset cpu);

    // Inline action: transfer tokens
    void transfer_tokens(name to, asset quantity, std::string memo);

    // Get token balance
    asset get_balance(name owner, symbol sym);
};
