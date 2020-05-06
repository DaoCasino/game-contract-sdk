#pragma once

#include <game-contract-sdk/game_base.hpp>

namespace multi_stub {

using eosio::checksum256;
using eosio::name;

using session_id_t = uint64_t;

// simple stub game
class [[eosio::contract]] multi_stub : public game_sdk::game {
  public:
    static constexpr uint16_t first_action{0u};
    static constexpr uint16_t second_action{1u};
    static constexpr uint16_t third_action{2u};

    struct [[eosio::table("roll")]] roll_row {
        uint64_t ses_id;
        std::vector<game_sdk::param_t> counter;

        uint64_t primary_key() const { return ses_id; }
    };
    using roll_table = eosio::multi_index<"roll"_n, roll_row>;

  public:
    multi_stub(name receiver, name code, eosio::datastream<const char*> ds)
        : game(receiver, code, ds), rolls(_self, _self.value) {}

    virtual void on_new_game(session_id_t ses_id) final;

    virtual void on_action(session_id_t ses_id, uint16_t type, std::vector<game_sdk::param_t> params) final;

    virtual void on_random(session_id_t ses_id, checksum256 rand) final;

    virtual void on_finish(session_id_t ses_id) final;

  private:
    roll_table rolls;
};

} // namespace multi_stub
