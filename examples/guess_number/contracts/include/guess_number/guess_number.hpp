#pragma once

#include <game-contract-sdk/game_base.hpp>

namespace guess_number {

using eosio::checksum256;
using eosio::name;
using eosio::asset;

namespace action {
    const uint16_t bet = 0;
    const uint16_t take = 1;
}

class [[eosio::contract]] guess_number : public game_sdk::game {
public:
    static const int MAX_NUM_ROUNDS = 3;

    struct [[eosio::table("state")]] state_row {
        uint64_t ses_id;
        int round;
        asset prev_round_deposit;

        uint64_t primary_key() const { return ses_id; }
    };

    using state_table = eosio::multi_index<"state"_n, state_row>;

    guess_number(name receiver, name code, eosio::datastream<const char*> ds)
        : game(receiver, code, ds),
          state(_self, _self.value) {}

    virtual void on_new_game(uint64_t ses_id) final;

    virtual void on_action(uint64_t ses_id, uint16_t type, std::vector<game_sdk::param_t> params) final;

    virtual void on_random(uint64_t ses_id, checksum256 rand) final;

    virtual void on_finish(uint64_t ses_id) final;
private:
    state_table state;
};

} // namespace guess_number
