#pragma once

#include <game-contract-sdk/game_base.hpp>

namespace simple {

using eosio::name;
using eosio::asset;
using bytes = std::vector<char>;
using eosio::checksum256;

struct action_type {
    uint8_t value;
};

class simple:
    public game_sdk::game {
public:
    simple(name receiver, name code, eosio::datastream<const char*> ds):
        game(receiver, code, ds)
    { }

    virtual void on_new_game(uint64_t ses_id) final;

    virtual void on_action(uint64_t ses_id, uint16_t type, std::vector<uint32_t> params) final;

    virtual void on_random(uint64_t ses_id, checksum256 rand) final;
};

} // namespace simple
