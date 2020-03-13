#pragma once

#include <vector>
#include <variant>

#include <platform/platform.hpp>
#include <casino/casino.hpp>
#include <events/events.hpp>

#include <eosio/eosio.hpp>
#include <eosio/asset.hpp>
#include <eosio/serialize.hpp>
#include <eosio/datastream.hpp>
#include <eosio/crypto.hpp>

#include <game-contract-sdk/rsa.hpp>

#ifndef NOABI
#define GAME_ACTION(act_name) [[eosio::action(#act_name)]]
#define GAME_NOTIFY(act_name) [[eosio::notify(#act_name)]]
#else
#define GAME_ACTION(act_name)
#define GAME_NOTIFY(act_name)
#endif

namespace game_sdk {

using eosio::name;
using eosio::asset;
using bytes = std::vector<char>;
using eosio::checksum256;


//template <typename GameActions>
class game: public eosio::contract {
public:
    using eosio::contract::contract;

    enum class state : uint8_t {
        req_deposit = 0,        // <- init|req_signidice_part_2, -> req_start|req_action
        req_start,              // <- req_deposit, -> req_action|failed
        req_action,             // <- req_start|req_deposit, -> req_signidice_part_1|failed
        req_signidice_part_1,   // <- req_action, -> req_signidice_part_2|falied
        req_signidice_part_2,   // <- req_signidice_part_1, -> finished|req_deposit|req_action|failed
        finished,               // <- req_signidice_part_2
        failed                  // <- req_start|req_action|req_signidice_part_1|req_signidice_part_2
    };

    enum class event_type : uint32_t {
        game_started = 0,
        action_request,
        signidice_part_1_request,
        signidice_part_2_request,
        game_finished
    };

    struct events {
        struct game_started {
            EOSLIB_SERIALIZE(game_started, )
        };

        struct action_request {
            uint8_t action_type;
            EOSLIB_SERIALIZE(action_request, (action_type))
        };

        struct signidice_part_1_request {
            checksum256 digest;
            EOSLIB_SERIALIZE(signidice_part_1_request, (digest))
        };

        struct signidice_part_2_request {
            checksum256 digest;
            EOSLIB_SERIALIZE(signidice_part_2_request, (digest))
        };

        struct game_finished {
            bool player_win;
            EOSLIB_SERIALIZE(game_finished, (player_win))
        };
    };

    /* global state variables */
    struct [[eosio::table("global")]] global_row {
        uint64_t session_seq { 0u };
        name platform;
        name events;
        name beneficiar;
    };
    using global_singleton = eosio::singleton<"global"_n, global_row>;

    /* session struct */
    struct [[eosio::table("session")]] session_row {
        uint64_t req_id;
        uint64_t casino_id;
        uint64_t ses_seq;
        name player;
        uint8_t state;
        asset deposit;
        checksum256 digest; // <- signidice result, set seed value on init

        uint64_t primary_key() const { return req_id; }
    };
    using session_table = eosio::multi_index<"session"_n, session_row>;

public:
    game(name receiver, name code, eosio::datastream<const char*> ds):
        contract(receiver, code, ds),
        global(_self, _self.value),
        sessions(_self, _self.value)
    { }

public:
    /* onlinal contract initialization callback */
    virtual void on_init() { /* do nothing by default */ }; // optional

    /* game session life-cycle callbacks */
    virtual void on_new_game(uint64_t req_id) = 0; // must be overrided
    virtual void on_action(uint64_t req_id, uint16_t type, std::vector<uint32_t> params) = 0; // must be overrided
    virtual void on_random(uint64_t req_id, checksum256 rand) = 0; // must be overrided
    virtual void on_finish(uint64_t req_id, bool player_win) { /* do nothing by default */ } // optional

public:
    /* getters */
    global_row get_global() {
        return global.get_or_default();
    }

    const session_row& get_session(uint64_t req_id) const {
        return sessions.get(req_id);
    }

    /* game sesion state changers */
    void require_action(uint64_t req_id, uint8_t action_type) {
//        eosio::check(action_type < std::variant_size<GameActions>>::value, "invalid action type");

        const auto session_itr = sessions.require_find(req_id, "session with this req_id not found");
        require_auth(session_itr->player); //TODO platform permission
        eosio::check(session_itr->state == static_cast<uint8_t>(state::req_start) ||
                     session_itr->state == static_cast<uint8_t>(state::req_signidice_part_2),
        "state should be 'req_start' or 'req_signidice_part_2'");

        sessions.modify(session_itr, get_self(), [&](auto& obj){
            obj.state = static_cast<uint8_t>(state::req_action);
        });

        emit_event<events::action_request>(req_id, event_type::action_request, { action_type });
    }

    void require_random(uint64_t req_id) {
        const auto session_itr = sessions.require_find(req_id, "session with this req_id not found");
        require_auth(session_itr->player); //TODO platform permission
        eosio::check(session_itr->state == static_cast<uint8_t>(state::req_action), "state should be 'req_action'");

        sessions.modify(session_itr, get_self(), [&](auto& obj){
            obj.state = static_cast<uint8_t>(state::req_signidice_part_1);
        });

        emit_event<events::signidice_part_1_request>(req_id, event_type::signidice_part_1_request, { session_itr->digest });
    }

    void finish_game(uint64_t req_id, bool player_win) {
        const auto session_itr = sessions.require_find(req_id, "session with this req_id not found");
        require_auth(session_itr->player); //TODO platform permission
        eosio::check(session_itr->state == static_cast<uint8_t>(state::req_signidice_part_2) ||
                     session_itr->state == static_cast<uint8_t>(state::req_action),
        "state should be 'req_signidice_part_2' or 'req_action'");

        auto casino = platform::read::get_casino(get_platform(), session_itr->casino_id);

        if (player_win) {
            eosio::action(
                {get_self(),"active"_n},
                casino.contract,
                "onloss"_n,
                std::make_tuple(get_self(), session_itr->player, session_itr->deposit)
            ).send();
            eosio::action(
                {get_self(),"active"_n},
                "eosio.token"_n,
                "transfer"_n,
                std::make_tuple(get_self(), session_itr->player, session_itr->deposit, std::string("player win[game]"))
            ).send();
        }
        else {
            eosio::action(
                {get_self(),"active"_n},
                "eosio.token"_n,
                "transfer"_n,
                std::make_tuple(get_self(), casino.contract, std::string("casino win"))
            ).send();
        }

        sessions.modify(session_itr, get_self(), [&](auto& obj){
            obj.state = static_cast<uint8_t>(state::finished);
        });

        sessions.erase(session_itr);

        emit_event<events::game_finished>(req_id, event_type::game_finished, { player_win });
    }


    /* contract actions */
    GAME_NOTIFY(eosio.token::transfer)
    void on_transfer(name from, name to, asset quantity, std::string memo) {
        if (to != get_self()) {
            return;
        }

        auto req_id = get_req_id(memo);

        eosio::check(quantity.symbol == eosio::symbol("BET",4), "invalid token symbol");

        auto session_itr = sessions.find(req_id);

        if (session_itr == sessions.end()) {
            eosio::check(platform::read::is_active_game(get_platform(), get_self_id()), "game is't listed in platform");

            auto gl = global.get_or_default();
            sessions.emplace(get_self(), [&](auto& row){
                row.req_id = req_id;
                row.ses_seq = gl.session_seq++;
                row.player = from;
                row.deposit = quantity;
                row.state = static_cast<uint8_t>(state::req_start);
            });
            global.set(gl, get_self());
        }
        else {
            eosio::check(session_itr->player == from, "only player can deposit");
            eosio::check(session_itr->state == static_cast<uint8_t>(state::req_deposit), "state should be 'req_deposit'");
            sessions.modify(session_itr, get_self(), [&](auto& row){
                row.deposit += quantity;
                row.state = static_cast<uint8_t>(state::req_action);
            });
        }
    }

/* TODO session TTL
    [[eosio::action("refund")]]
    void refund(uint64_t sender, uint64_t req_id) final {
        require_auth(sender);

        auto session_itr = sessions.require_find(req_id, "session with this req_id not found");
        eosio::check(session_itr->player == sender, "only session sender can refund");
        eosio::check(session_itr->state == failed)

        eosio::action(
            permission_level{get_self(),"active"_n},
            "eosio.token"_n,
            "notify"_n,
            std::make_tuple(get_self(), sender, session_itr->amount, std::string("refund"))
        ).send();

        sessions.erase(session_itr);
    }
*/
    GAME_ACTION(newgame)
//#ifndef NOABI
//    [[eosio::action("newgame"), eosio::contract("game")]]
//#endif
    void new_game(uint64_t req_id, uint64_t casino_id) {
        const auto session_itr = sessions.require_find(req_id, "session with this req_id not found");
        require_auth(session_itr->player);
        eosio::check(session_itr->state == static_cast<uint8_t>(state::req_start), "state should be 'req_start'");
        eosio::check(platform::read::is_active_casino(get_platform(), casino_id), "casino is't listed in platform");

        sessions.modify(session_itr, get_self(), [&](auto& obj){
            obj.casino_id = casino_id;
        });

        on_new_game(req_id);

        emit_event<events::game_started>(req_id, event_type::game_started, { });
    }

    GAME_ACTION(gameaction)
    void game_action(uint64_t req_id, uint16_t type, std::vector<uint32_t> params) {
        const auto session_itr = sessions.require_find(req_id, "session with this req_id not found");
        require_auth(session_itr->player); //TODO platform permission

        eosio::check(session_itr->state == static_cast<uint8_t>(state::req_action) ||
                     session_itr->state == static_cast<uint8_t>(state::req_deposit),
        "state should be 'req_deposit' or 'req_action'");

        sessions.modify(session_itr, get_self(), [&](auto& obj){
            obj.state = static_cast<uint8_t>(state::req_action);
        });

        on_action(req_id, type, params);
    }

    GAME_ACTION(sgdicefirst)
    void signidice_part_1(uint64_t req_id, const std::string& sign) {
        require_auth(get_platform()); //TODO special permission check
        const auto session_itr = sessions.require_find(req_id, "session with this req_id not found");

        eosio::check(session_itr->state == static_cast<uint8_t>(state::req_signidice_part_1),
        "state should be 'req_deposit' or 'req_action'");

        eosio::check(daobet::rsa_verify(session_itr->digest, sign, platform::read::get_rsa_pubkey(get_platform())), "invalid signature");
        auto new_digest = eosio::sha256(sign.data(), sign.size());

        sessions.modify(session_itr, get_self(), [&](auto& obj){
            obj.digest = new_digest;
            obj.state = static_cast<uint8_t>(state::req_signidice_part_2);
        });

        emit_event<events::signidice_part_1_request>(req_id, event_type::signidice_part_1_request, { new_digest });
    }

    GAME_ACTION(sgdicesecond)
    void signidice_part_2(uint64_t req_id, const std::string& sign) {
        const auto session_itr = sessions.require_find(req_id, "session with this req_id not found");
        require_auth(platform::read::get_casino(get_platform(), session_itr->casino_id).contract); //TODO special permission check

        eosio::check(session_itr->state == static_cast<uint8_t>(state::req_signidice_part_2),
        "state should be 'req_deposit' or 'req_action'");

        const auto& cas_rsa_pubkey = platform::read::get_casino(get_platform(), session_itr->casino_id).rsa_pubkey;
        eosio::check(daobet::rsa_verify(session_itr->digest, sign, cas_rsa_pubkey), "invalid signature");
        auto new_digest = eosio::sha256(sign.data(), sign.size());

        sessions.modify(session_itr, get_self(), [&](auto& obj){
            obj.digest = new_digest;
        });

        on_random(req_id, new_digest);
    }

    GAME_ACTION(init)
    void init(name platform, name events) {
        require_auth(get_self());
        eosio::check(eosio::is_account(platform), "platform account doesn't exists");
        eosio::check(eosio::is_account(events), "events account doesn't exists");

        auto gl = global.get_or_default();
        gl.platform = platform;
        gl.events = events;
        global.set(gl, get_self());

        on_init();
    }

private:
    global_singleton global;
    session_table sessions;

private:
    template <typename Event>
    void emit_event(uint64_t req_id, event_type type, const Event& event) {
        const auto session_itr = sessions.require_find(req_id, "session with this req_id not found");
        require_auth(session_itr->player); //TODO platform permission

        auto data_bytes = eosio::pack<Event>(event);

        eosio::action(
            {get_self(),"active"_n},
            get_events(),
            "send"_n,
            std::make_tuple(
                get_self(),
                session_itr->casino_id,
                get_self_id(),
                req_id,
                static_cast<uint32_t>(type),
                data_bytes
            )
        ).send();
    }

    uint64_t get_req_id(const std::string& str) {
        return std::stoul(str);
    }

    uint64_t get_self_id() {
        return 0;
//        return platform::read::get_game(get_platform(), get_self()).id;
    }

    name get_platform() {
        auto gl = global.get_or_default();
        return gl.platform;
    }

    name get_events() {
        auto gl = global.get_or_default();
        return gl.events;
    }
};

} // namespace game_base

#define GAME_ABI( TYPE ) \
extern "C" { \
void apply( uint64_t receiver, uint64_t code, uint64_t action ) { \
    auto self = receiver; \
    if( action == eosio::name("onerror").value) { \
        /* onerror is only valid if it is for the "eosio" code account and authorized by "eosio"'s "active permission */ \
        eosio::check(code == eosio::name("eosio").value, "onerror action's are only valid from the \"eosio\" system account"); \
    } \
    if( code == self || action == eosio::name("onerror").value ) { \
        TYPE thiscontract( eosio::name(self) ); \
        switch( action ) { \
            case eosio::name("gameaction").value: \
                break; \
        } \
    } \
} \
} \
