# Antelope Memo Account Creator

EOSIO/Antelope smart contract that creates accounts on incoming token transfer. Send system token with public key in memo — contract creates account with that key.

## How it works

```
User → transfer(WAX, memo="EOS...") → on_transfer
  → generate name from SHA256(pubkey)
  → inline action: process
    → newaccount + buyrambytes + delegatebw
    → inline action: finalize
      → check balance after all purchases
      → transfer remainder to new account
```

## Features

- **Deterministic naming** — SHA256(pubkey) → 12-char account name
- **Collision-safe** — always creates new account with salt if name exists (never forwards to existing accounts)
- **Configurable** — RAM/CPU/NET amounts via config table
- **Multi-format keys** — supports `PUB_K1_...` and legacy `EOS...`
- **Smart remainder** — calculates actual remainder after RAM purchase (not estimated)
- **Full ownership** — CPU/NET transferred to user (not delegated)
- **Multi-chain** — works on WAX, EOS, Proton (auto-skips CPU/NET staking on Proton)

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
cleos transfer user1 <contract> "1.0000 EOS" "EOS6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5BoDq63"
```

## Config

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| cpu_stake | int64 | 1000 | CPU stake amount (0.1000 with 4 decimals) |
| net_stake | int64 | 1000 | NET stake amount (0.1000 with 4 decimals) |
| ram_bytes | uint32 | 4096 | RAM to buy for new account |

## Security

**Why no forwarding to existing accounts?**

If account name derived from `SHA256(pubkey)` already exists, the contract creates a new account with incremented salt instead of forwarding funds. This prevents attack where malicious actor pre-creates account with name matching a known public key to steal funds.

**Repeated transfers:** If user sends funds twice with same public key, they get two separate accounts. This is intentional — safer than risking funds going to wrong recipient.

**Collision probability:** ~1 in 78 billion (`31^12` possible names vs ~10M existing accounts), but targeted attacks are possible without this protection.

## License

MIT
