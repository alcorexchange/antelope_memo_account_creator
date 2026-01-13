# CLAUDE.md

## Project Overview

EOSIO smart contract that creates accounts on incoming token transfer. User sends system token with public key in memo — contract creates account with that key.

**Flow:**
```
User → transfer(WAX, memo="EOS...") → on_transfer
  → generate name from SHA256(pubkey)
  → inline: process → newaccount + buyrambytes + delegatebw
  → inline: finalize → check balance, transfer remainder
```

## Features

- **Deterministic naming**: SHA256(pubkey) → 12-char name using `abcdefghijklmnopqrstuvwxyz12345`
- **Collision handling**: auto-increment salt if name exists
- **Configurable**: RAM/CPU/NET via singleton config table
- **Key formats**: supports `PUB_K1_...` and legacy `EOS...`

## Build

```bash
cd build && cmake .. && make
```

Output: `build/memo_acc_creator/memo_acc_creator.wasm`, `.abi`

## Deploy & Use

```bash
# Deploy
cleos set contract <account> ./build/memo_acc_creator

# Add eosio.code permission (required for inline actions)
cleos set account permission <account> active --add-code

# Configure (optional, defaults: 0.1 CPU, 0.1 NET, 4096 RAM)
cleos push action <contract> setconfig '[1000, 1000, 4096]' -p <contract>@active

# Create account
cleos transfer user1 <contract> "1.0000 EOS" "EOS6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5BoDq63"
```

## Files

```
include/
├── memo_acc_creator.hpp   # Contract class, config singleton, authority structs
├── eosio_system.hpp       # getCoreSymbol(), CORE_TOKEN_ACCOUNT
src/
├── memo_acc_creator.cpp   # on_transfer handler, name generation, inline actions
```

## Actions & Tables

- `setconfig(cpu_stake, net_stake, ram_bytes)` — admin only, update config
- `process(new_account, pubkey, balance_before)` — internal, creates account
- `finalize(new_account, balance_before)` — internal, transfers remainder
- `config` table (singleton) — stores RAM/CPU/NET settings
- `on_notify("eosio.token::transfer")` — catches incoming transfers

## Dependencies

- EOSIO CDT (fetched via CMake)
