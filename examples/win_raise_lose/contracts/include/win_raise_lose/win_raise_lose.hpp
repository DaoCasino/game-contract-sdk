#pragma once

#include <game-contract-sdk/game_base.hpp>

namespace win_raise_lose {

using eosio::asset;
using eosio::name;
using bytes = std::vector<char>;
using eosio::checksum256;

namespace action {
  const uint8_t roll = 0;
}

namespace decision {
  const uint8_t win = 0;
  const uint8_t lose = 1;
  const uint8_t deposit = 2;
}

class [[eosio::contract]] win_raise_lose : public game_sdk::game {
  public:
    static constexpr uint8_t win_coef = 2;

  public:
    struct [[eosio::table("roll")]] roll_row {
        uint64_t ses_id;
        asset deposit;

        uint64_t primary_key() const { return ses_id; }
    };
    using roll_table = eosio::multi_index<"roll"_n, roll_row>;

  public:
    win_raise_lose(name receiver, name code, eosio::datastream<const char*> ds)
        : game(receiver, code, ds), rolls(_self, _self.value) {}

    virtual void on_new_game(uint64_t ses_id) final;

    virtual void on_action(uint64_t ses_id, uint16_t type, std::vector<game_sdk::param_t> params) final;

    virtual void on_random(uint64_t ses_id, checksum256 rand) final;

    virtual void on_finish(uint64_t ses_id) final;

  private:
    roll_table rolls;
};

} // namespace win_raise_lose
