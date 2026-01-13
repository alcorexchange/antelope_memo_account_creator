# Antelope Memo Account Creator

EOSIO/Antelope smart contract that creates accounts on incoming token transfer. Send system token with public key in memo — contract creates account with that key.

## How it works

```
User → transfer(EOS/WAX, memo="PUB_K1_...") → Contract
  → generate name from SHA256(pubkey)
  → newaccount + buyrambytes + delegatebw
  → transfer remainder to new account
```

## Features

- **Deterministic naming** — SHA256(pubkey) → 12-char account name
- **Collision handling** — auto-increment salt if name already exists
- **Configurable** — RAM/CPU/NET amounts via config table
- **Multi-format keys** — supports `PUB_K1_...` and legacy `EOS...`

## Build

Requires [EOSIO CDT](https://github.com/AntelopeIO/cdt) (fetched automatically).

```bash
cd build && cmake .. && make
```

## Deploy

```bash
# Deploy contract
cleos set contract <account> ./build/memo_acc_creator

# Add eosio.code permission (required for inline actions)
cleos set account permission <account> active --add-code
```

## Usage

```bash
# Configure (optional)
# Defaults: 0.1 CPU, 0.1 NET, 4096 bytes RAM
cleos push action <contract> setconfig '[1000, 1000, 4096]' -p <contract>@active

# Create account by sending tokens with public key in memo
cleos transfer user1 <contract> "1.0000 EOS" "PUB_K1_6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV"
```

## Config

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| cpu_stake | int64 | 1000 | CPU stake amount (0.1000 with 4 decimals) |
| net_stake | int64 | 1000 | NET stake amount (0.1000 with 4 decimals) |
| ram_bytes | uint32 | 4096 | RAM to buy for new account |

## License

MIT
