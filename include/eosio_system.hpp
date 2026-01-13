
// "Copyright [2022] <alcor exchange>"
#ifndef INCLUDE_EOSIO_SYSTEM_HPP_
#define INCLUDE_EOSIO_SYSTEM_HPP_

#include <eosio/eosio.hpp>

namespace EOSIOSystem {
static constexpr eosio::name SYSTEM_ACCOUNT = eosio::name("eosio");
static constexpr eosio::name CORE_TOKEN_ACCOUNT = eosio::name("eosio.token");

struct [[eosio::table, eosio::contract("eosio.system")]] user_resources {
  eosio::name owner;
  eosio::asset net_weight;
  eosio::asset cpu_weight;
  int64_t ram_bytes = 0;

  bool is_empty() const { return net_weight.amount == 0 && cpu_weight.amount == 0 && ram_bytes == 0; }
  uint64_t primary_key() const { return owner.value; }

  EOSLIB_SERIALIZE(user_resources, (owner)(net_weight)(cpu_weight)(ram_bytes))
};
typedef eosio::multi_index< "userres"_n, user_resources >      user_resources_table;

struct [[eosio::table, eosio::contract("eosio.system")]] exchange_state {
  eosio::asset supply;

  struct connector {
    eosio::asset balance;
    double weight = .5;

    EOSLIB_SERIALIZE(connector, (balance)(weight))
  };

  connector base;
  connector quote;

  uint64_t primary_key() const { return supply.symbol.raw(); }
};
typedef eosio::multi_index<"rammarket"_n, exchange_state> rammarket_table;

eosio::symbol getCoreSymbol() {
  rammarket_table _rammarket(SYSTEM_ACCOUNT, SYSTEM_ACCOUNT.value);
  eosio::check(_rammarket.begin() != _rammarket.end(), "sanity check: rammarket not found");
  auto _rammarket_itr = _rammarket.begin();
  return _rammarket_itr->quote.balance.symbol;
}

int64_t getExistingRam(eosio::name user) {
  user_resources_table _user_resources(SYSTEM_ACCOUNT, user.value);
  if(_user_resources.begin() == _user_resources.end()) {
    return 0;
  }
  auto _user_resources_itr = _user_resources.begin();
  return _user_resources_itr->ram_bytes;
}
}  // namespace EOSIOSystem
#endif  // INCLUDE_EOSIO_SYSTEM_HPP_
