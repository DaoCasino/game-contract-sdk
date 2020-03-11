#pragma once

#include <platform/platform.hpp>
#include <casino/casino.hpp>
#include <events/events.hpp>

#include <eosio/eosio.hpp>
#include <eosio/asset.hpp>


namespace game_sdk {

using eosio::name;
using eosio::asset;
using bytes = std::vector<char>;


template <typename ...GameActions>
class game : public eosio::contract {
public:
    using eosio::contract::contract;
    using game_action_type = std::variant<GameActions...>;

    enum class state : uint8_t {
        req_deposit = 0,        // <- init|req_signidice_part_2, -> req_start|req_action
        req_start,              // <- req_deposit, -> req_action|failed
        req_action,             // <- req_start|req_deposit, -> req_signidice_part_1|failed
        req_signidice_part_1,   // <- req_action, -> req_signidice_part_2|falied
        req_signidice_part_2,   // <- req_signidice_part_1, -> finished|req_deposit|req_action|failed
        finished,               // <- req_signidice_part_2
        failed                  // <- req_start|req_action|req_signidice_part_1|req_signidice_part_2
    };

    /* global state variables */
    struct [[eosio::table("global")]] global_row {
        uint64_t session_seq { 0u };
        name platform_addr;
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

        uint64_t primary_key() const { return req_id; }
    };
    using session_table = eosio::multi_index<"session"_n, session_row>;

public:
    game(name receiver, name code, eosio::datastream<const char*> ds):
        contract(receiver, code, ds),
        global(_self, _self.value),
        sessions(_self, _self.value),
        deposits(_self, _self.value)
    { }


    template <typename T>
    T get_action() {
        return eosio::unpack_action_data<T>();
    }

    void require_random(uint64_t req_id); // require random generation
    void require_action(uint64_t req_id, uint8_t action_type); // require game action from player
    void finish_game(uint64_t req_id, bool player_win); // finish game
    void fail_game(uint64_t req_id); //fail game

    virtual void on_init() { /* do nothing by default */ }; // optianal

    virtual void on_new_game(uint64_t req_id) = 0; // must be overrided
    virtual void on_action(uint64_t req_id, game_action_type action) = 0; // must be overrided
    virtual void on_random(uint64_t req_id, uint256_t rand) = 0; // must be overrided

//    virtual void on_signidice_part_1(/*TODO*/) { /* do nothing by default */ }; // optional
//    virtual void on_signidice_part_2(/*TODO*/) { /* do nothing by default */ }; // optional

public:
    void require_action(uint64_t req_id, uint8_t action_type) {
        eosio::check(action_type < std::variant_size<game_action_type>::value, "invalid action type");

        const auto session_itr = sessions.require_find(req_id, "session with this req_id not found");
        require_auth(session_itr->player); //TODO platform permission
        eosio::check(session_itr->state == static_cast<uint8_t>(state::req_start) ||
                     session_itr->state == static_cast<uint8_t>(state::req_signidice_part_2),
        "state should be 'req_start' or 'req_signidice_part_2'");

        sessions.modify(session_itr, get_self(), [&](auto& row){
            session_itr->state = static_cast<uint8_t>(state::req_action);
        });

        //TODO emit event
    }

    void require_random(uint64_t req_id) {
        const auto session_itr = sessions.require_find(req_id, "session with this req_id not found");
        require_auth(session_itr->player); //TODO platform permission
        eosio::check(session_itr->state == static_cast<uint8_t>(state::req_action), "state should be 'req_action'");

        sessions.modify(session_itr, get_self(), [&](auto& row){
            session_itr->state = static_cast<uint8_t>(state::req_signidice_part_1);
        });

        //TODO generate seed and emit event
    }

    void finish_game(uint64_t req_id, bool player_win) {
        const auto session_itr = sessions.require_find(req_id, "session with this req_id not found");
        require_auth(session_itr->player); //TODO platform permission
        eosio::check(session_itr->state == static_cast<uint8_t>(state::req_signidice_part_2), "state should be 'req_signidice_part_2'");

        //TODO make transfers and emit event

        sessions.erase(req_id);
    }

    [[eosio::on_notify("eosio.token::transfer")]]
    void on_transfer(name from, name to, asset quantity, std::string memo) final {
        if (to != get_self()) {
            return;
        }

        auto req_id = get_req_id(memo);

        eosio::check(quantity.symbol == eosio::symbol("BET",4), "invalid token symbol");

        auto session_itr = sessions.find(req_id);

        if (session_itr == sessions.end()) {
            auto gl = global.get_or_default();
            sessions.emplace(get_self(), [&](auto& row){
                row.req_id = req_id;
                row.ses_seq = gl.session_seq++;
                row.player = from;
                row.deposit = quantity;
                row.state = static_cast<uint8_t>(state::req_start);
            });
            global.set(gl);
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
/*
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

    [[eosio::action("new_game")]]
    void new_game(uint64_t req_id, uint64_t casino_id) final {
        const auto session_itr = sessions.require_find(req_id, "session with this req_id not found");
        require_auth(session_itr->player);
        eosio::check(session_itr->state == static_cast<uint8_t>(state::req_start), "state should be 'req_start'");

        sessions.modify(session_itr, get_self(), [&](auto& obj){
            session_itr->casino_id = casino_id;
        });

        on_new_game(req_id);

        //TODO emit event new game
    }

    [[eosio::action("game_action")]]
    void game_action(uint64_t req_id, game_action_type action) {
        const auto session_itr = sessions.require_find(req_id, "session with this req_id not found");
        require_auth(session_itr->player); //TODO platform permission

        eosio::check(session_itr->state == static_cast<uint8_t>(state::req_action) ||
                     session_itr->state == static_cast<uint8_t>(state::req_deposit),
        "state should be 'req_deposit' or 'req_action'");

        sessions.modify(session_itr, get_self(), [&](auto& obj){
            session_itr->state = static_cast<uint8_t>(state::req_action);
        });

        on_game_action(req_id, action);
    }

    [[eosio::action("init")]]
    void init(name platform, name beneficiar) {
        auto global.
    }

protected:
    global_singleton global;
    session_table sessions;

private:
    template <typename Event>
    void emit_event(const Event& event) {
        //TODO
    }

    uint64_t get_req_id(const std::string& str) {
        return std::stoul(str);
    }
}

extern "C" {

void apply()

}

} // namespace game_base
