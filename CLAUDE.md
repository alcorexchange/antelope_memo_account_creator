# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

EOSIO smart contract written in C++ for EOSIO-based blockchains (EOS, WAX, Proton). Compiles to WebAssembly (WASM) for on-chain execution.

## Build Commands

```bash
# Build the contract (from project root)
cd build && cmake .. && make

# Clean rebuild
rm -rf build/* && cd build && cmake .. && make
```

Build outputs in `build/memo_acc_creator/`:
- `memo_acc_creator.wasm` - WebAssembly binary
- `memo_acc_creator.abi` - Contract ABI

## Deployment & Testing (cleos)

```bash
# Deploy contract
cleos set contract <account> ./build/memo_acc_creator

# Call action
cleos push action <contract> hi '{"nm":"alice"}' -p <account>@active
```

## Architecture

```
src/
├── memo_acc_creator.cpp    # Action implementations
include/
├── memo_acc_creator.hpp    # Contract class definition, action declarations
ricardian/
├── memo_acc_creator.contracts.md  # Ricardian contract documentation (legal terms)
```

**Contract Pattern:**
- Header declares `CONTRACT` class extending `eosio::contract`
- Actions defined with `ACTION` macro
- Action wrappers created with `action_wrapper` template
- Implementation in .cpp file

## Dependencies

- EOSIO CDT (Contract Development Toolkit) - fetched automatically via CMake ExternalProject
