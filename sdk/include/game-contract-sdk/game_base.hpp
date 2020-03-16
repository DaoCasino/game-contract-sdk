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
#define CONTRACT_ACTION(act_name) [[eosio::action(#act_name)]]
#else
#define CONTRACT_ACTION(act_name)
#endif

namespace game_sdk {

using eosio::name;
using eosio::asset;
using bytes = std::vector<char>;
using eosio::checksum256;
using eosio::time_point;
using eosio::require_auth;


class game: public eosio::contract {
public:
    using eosio::contract::contract;
    static constexpr name player_game_permission = "game"_n;
    static constexpr name platform_signidice_permission = "signidice"_n;
    static constexpr name casino_signidice_permission = "signidice"_n;

    enum class state : uint8_t {
        req_deposit = 0,        // <- init|req_signidice_part_2, -> req_start|req_action
        req_start,              // <- req_deposit, -> req_action|failed
        req_action,             // <- req_start|req_deposit, -> req_signidice_part_1|failed
        req_signidice_part_1,   // <- req_action, -> req_signidice_part_2|falied
        req_signidice_part_2,   // <- req_signidice_part_1, -> finished|req_deposit|req_action|failed
        finished,               // <- req_signidice_part_2
    };

    enum class event_type : uint32_t {
        game_started = 0,
        action_request,
        signidice_part_1_request,
        signidice_part_2_request,
        game_finished,
        game_failed,
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

        struct game_failed {
            EOSLIB_SERIALIZE(game_failed, )
        };
    };

    /* global state variables */
    struct [[eosio::table("global")]] global_row {
        uint64_t session_seq { 0u };
        name platform;
        name events;
        uint32_t session_ttl;
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
        time_point last_update; // <-- last action time

        uint64_t primary_key() const { return req_id; }
    };
    using session_table = eosio::multi_index<"session"_n, session_row>;

public:
    game(name receiver, name code, eosio::datastream<const char*> ds):
        contract(receiver, code, ds),
        global(_self, _self.value),
        sessions(_self, _self.value)
    { }

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
        const auto session_itr = sessions.require_find(req_id, "session with this req_id not found");
        require_auth({session_itr->player, player_game_permission});
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
        require_auth({session_itr->player, player_game_permission});
        eosio::check(session_itr->state == static_cast<uint8_t>(state::req_action), "state should be 'req_action'");

        sessions.modify(session_itr, get_self(), [&](auto& obj){
            obj.state = static_cast<uint8_t>(state::req_signidice_part_1);
        });

        emit_event<events::signidice_part_1_request>(req_id, event_type::signidice_part_1_request, { session_itr->digest });
    }

    void finish_game(uint64_t req_id, bool player_win) {
        const auto session_itr = sessions.require_find(req_id, "session with this req_id not found");
        require_auth({session_itr->player, player_game_permission});
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

    /* deposit handler */
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
            eosio::check(!is_expired(req_id), "session expired");
            sessions.modify(session_itr, get_self(), [&](auto& row){
                row.deposit += quantity;
                row.state = static_cast<uint8_t>(state::req_action);
            });
        }
    }

    /* contract actions */
    CONTRACT_ACTION(newgame)
    void new_game(uint64_t req_id, uint64_t casino_id) {
        const auto session_itr = sessions.require_find(req_id, "session with this req_id not found");
        require_auth({session_itr->player, player_game_permission});
        eosio::check(session_itr->state == static_cast<uint8_t>(state::req_start), "state should be 'req_start'");
        eosio::check(platform::read::is_active_casino(get_platform(), casino_id), "casino is't listed in platform");
        eosio::check(!is_expired(req_id), "session expired");

        sessions.modify(session_itr, get_self(), [&](auto& obj){
            obj.casino_id = casino_id;
        });

        on_new_game(req_id);

        emit_event<events::game_started>(req_id, event_type::game_started, { });
    }

    CONTRACT_ACTION(gameaction)
    void game_action(uint64_t req_id, uint16_t type, std::vector<uint32_t> params) {
        const auto session_itr = sessions.require_find(req_id, "session with this req_id not found");
        require_auth({session_itr->player, player_game_permission});
        eosio::check(!is_expired(req_id), "session expired");
        eosio::check(session_itr->state == static_cast<uint8_t>(state::req_action) ||
                     session_itr->state == static_cast<uint8_t>(state::req_deposit),
        "state should be 'req_deposit' or 'req_action'");

        sessions.modify(session_itr, get_self(), [&](auto& obj){
            obj.state = static_cast<uint8_t>(state::req_action);
        });

        on_action(req_id, type, params);
    }

    CONTRACT_ACTION(sgdicefirst)
    void signidice_part_1(uint64_t req_id, const std::string& sign) {
        require_auth({get_platform(), platform_signidice_permission});
        const auto session_itr = sessions.require_find(req_id, "session with this req_id not found");
        eosio::check(!is_expired(req_id), "session expired");
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

    CONTRACT_ACTION(sgdicesecond)
    void signidice_part_2(uint64_t req_id, const std::string& sign) {
        const auto session_itr = sessions.require_find(req_id, "session with this req_id not found");
        const auto casino_addr = platform::read::get_casino(get_platform(), session_itr->casino_id).contract;
        require_auth({casino_addr, casino_signidice_permission});
        eosio::check(!is_expired(req_id), "session expired");
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

    CONTRACT_ACTION(close)
    void close(uint64_t req_id) {
        auto session_itr = sessions.require_find(req_id, "session with this req_id not found");
        eosio::check(is_expired(req_id), "session isn't expired, only expired session can be closed");

        eosio::action(
            {get_self(),"active"_n},
            "eosio.token"_n,
            "transfer"_n,
            std::make_tuple(get_self(), session_itr->player, session_itr->deposit, std::string("refund"))
        ).send();

        sessions.erase(session_itr);

        emit_event<events::game_started>(req_id, event_type::game_started, { });
    }

    CONTRACT_ACTION(init)
    void init(name platform, name events, uint32_t session_ttl) {
        require_auth(get_self());
        eosio::check(eosio::is_account(platform), "platform account doesn't exists");
        eosio::check(eosio::is_account(events), "events account doesn't exists");

        auto gl = global.get_or_default();
        gl.platform = platform;
        gl.events = events;
        gl.session_ttl = session_ttl;
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
        return platform::read::get_game(get_platform(), get_self()).id;
    }

    name get_platform() {
        auto gl = global.get_or_default();
        return gl.platform;
    }

    name get_events() {
        auto gl = global.get_or_default();
        return gl.events;
    }

    bool is_expired(uint64_t req_id) {
        const auto session_itr = sessions.require_find(req_id, "session with this req_id not found");
        const auto gl = global.get_or_default();
        return eosio::current_time_point().sec_since_epoch() - session_itr->last_update.sec_since_epoch() > gl.session_ttl;
    }
};


template<typename T, typename... Args>
bool execute_action(eosio::name self, eosio::name code, void (game::*func)(Args...)) {
    using namespace eosio;
    size_t size = action_data_size();

    //using malloc/free here potentially is not exception-safe, although WASM doesn't support exceptions
    constexpr size_t max_stack_buffer_size = 512;
    void* buffer = nullptr;
    if( size > 0 ) {
        buffer = max_stack_buffer_size < size ? malloc(size) : alloca(size);
        read_action_data( buffer, size );
    }

    std::tuple<std::decay_t<Args>...> args;
    datastream<const char*> ds((char*)buffer, size);
    ds >> args;

    T inst(self, code, ds);

    auto f2 = [&]( auto... a ){
        ((&inst)->*func)( a... );
    };

    boost::mp11::tuple_apply( f2, args );
    if ( max_stack_buffer_size < size ) {
        free(buffer);
    }
    return true;
}

} // namespace game_sdk


#define GAME_CONTRACT( TYPE ) \
extern "C" { \
    void apply(uint64_t receiver, uint64_t code, uint64_t action) { \
        if (code == "eosio.token"_n.value && action == "transfer"_n.value) { \
            game_sdk::execute_action<TYPE>(eosio::name(receiver), eosio::name(code), \
                              &TYPE::on_transfer); \
        } \
        else if (code == receiver) { \
            switch (action) { \
                case "init"_n.value: \
                    game_sdk::execute_action<TYPE>(eosio::name(receiver), eosio::name(code), &TYPE::init); \
                    break; \
                case "newgame"_n.value: \
                    game_sdk::execute_action<TYPE>(eosio::name(receiver), eosio::name(code), &TYPE::new_game); \
                    break; \
                case "gameaction"_n.value: \
                    game_sdk::execute_action<TYPE>(eosio::name(receiver), eosio::name(code), &TYPE::game_action); \
                    break; \
                case "sgdicefirst"_n.value: \
                    game_sdk::execute_action<TYPE>(eosio::name(receiver), eosio::name(code), &TYPE::signidice_part_1); \
                    break; \
                case "sgdicesecond"_n.value: \
                    game_sdk::execute_action<TYPE>(eosio::name(receiver), eosio::name(code), &TYPE::signidice_part_2); \
                    break; \
                case "close"_n.value: \
                    game_sdk::execute_action<TYPE>(eosio::name(receiver), eosio::name(code), &TYPE::close); \
                    break; \
                default: \
                    eosio::eosio_exit(1); \
            } \
        } \
        eosio::eosio_exit(0); \
    } \
}

