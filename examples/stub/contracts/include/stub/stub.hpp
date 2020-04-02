#pragma once

#include <game-contract-sdk/game_base.hpp>

namespace stub {

using eosio::name;
using eosio::checksum256;

// simple stub game
class stub: public game_sdk::game {
public:
    static constexpr uint16_t stub_game_action_type { 0u };

public:
    stub(name receiver, name code, eosio::datastream<const char*> ds):
        game(receiver, code, ds)
    { }

    virtual void on_new_game(uint64_t ses_id) final;

    virtual void on_action(uint64_t ses_id, uint16_t type, std::vector<uint32_t> params) final;

    virtual void on_random(uint64_t ses_id, checksum256 rand) final;

    virtual void on_finish(uint64_t ses_id) final;
};

} // namespace stub
