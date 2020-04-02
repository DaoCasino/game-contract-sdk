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
using eosio::symbol;
using bytes = std::vector<char>;
using eosio::checksum256;
using eosio::time_point;
using eosio::require_auth;
using casino::game_params_type;


class game: public eosio::contract {
public:
    using eosio::contract::contract;
    static constexpr name player_game_permission = "game"_n;
    static constexpr name platform_signidice_permission = "signidice"_n;
    static constexpr name casino_signidice_permission = "signidice"_n;
    static constexpr symbol core_symbol = symbol("BET", 4);
    static const asset zero_asset;

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

    /* event types */
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
            asset player_win_amount;
            EOSLIB_SERIALIZE(game_finished, (player_win_amount))
        };

        struct game_failed {
            asset player_win_amount;
            EOSLIB_SERIALIZE(game_failed, (player_win_amount))
        };
    };

    /* global state variables */
    struct [[eosio::table("global"), eosio::contract("game")]] global_row {
        uint64_t session_seq { 0u };
        name platform;
        name events;
        uint32_t session_ttl;
    };
    using global_singleton = eosio::singleton<"global"_n, global_row>;

    /* session struct */
    struct [[eosio::table("session"), eosio::contract("game")]] session_row {
        uint64_t ses_id;
        uint64_t casino_id;
        uint64_t ses_seq;
        name player;
        uint8_t state;
        game_params_type params; // <- game params, copied from casino contract aboid of params changing while active session
        asset deposit; // <- current player deposit amount, increments when new deposit received
        checksum256 digest; // <- signidice result, set seed value on new_game
        time_point last_update; // <-- last action time
        asset last_max_win; // <- last max win value, updates after on_action

        uint64_t primary_key() const { return ses_id; }
    };
    using session_table = eosio::multi_index<"session"_n, session_row>;

public:
    game(name receiver, name code, eosio::datastream<const char*> ds):
        contract(receiver, code, ds),
        sessions(_self, _self.value)
    {
        // load global singleton to memory
        global = global_singleton(_self, _self.value).get_or_default();
    }

    virtual ~game() {
        // store singleton after all operations
        global_singleton(_self, _self.value).set(global, _self);
    }

protected:
    /* onlinal contract initialization callback */
    virtual void on_init() { /* do nothing by default */ }; // optional

    /* game session life-cycle callbacks */
    virtual void on_new_game(uint64_t ses_id) = 0; // must be overridden
    virtual void on_action(uint64_t ses_id, uint16_t type, std::vector<uint32_t> params) = 0; // must be overridden
    virtual void on_random(uint64_t ses_id, checksum256 rand) = 0; // must be overridden
    virtual void on_finish(uint64_t ses_id) { /* do nothing by default */ } // optional

    /* getters */
    const global_row& get_global() const {
        return global;
    }

    const session_row& get_session(uint64_t ses_id) const {
        return sessions.get(ses_id);
    }

    std::optional<uint32_t> get_param_value(uint64_t ses_id, uint16_t param_type) const {
        const auto& session = sessions.get(ses_id);
        const auto itr = std::find_if(session.params.begin(), session.params.end(),
        [&](const auto& item){
            return item.first == param_type;
        });
        return itr == session.params.end() ? std::nullopt : std::optional<uint32_t> { itr->second };
    }

protected:
    /* utility helpers */
    uint128_t rand_u128(const checksum256& rand) const {
        const auto& arr = rand.get_array();
        // use % operation to save original distribution
        const uint128_t left = arr[0] % UINT64_MAX;
        const uint128_t right = arr[1] % UINT64_MAX;

        // just concat parts
        // it's not fair way(don't save original distribution), but more simpler
        return (left << 64) & right;
    }

    uint64_t rand_u64(const checksum256& rand) const {
        auto u128 = rand_u128(rand);
        return u128 % UINT64_MAX;
    }

protected:
    /* game session state changers */
    void require_action(uint64_t ses_id, uint8_t action_type, bool need_random = false) {
        const auto session_itr = sessions.require_find(ses_id, "session with this ses_id not found");
        eosio::check(session_itr->state == static_cast<uint8_t>(state::req_start) ||
                     session_itr->state == static_cast<uint8_t>(state::req_signidice_part_2),
        "state should be 'req_start' or 'req_signidice_part_2'");

        sessions.modify(session_itr, get_self(), [&](auto& obj){
            if (!need_random) {
                obj.state = static_cast<uint8_t>(state::req_action);
            }
            else {
                obj.state = static_cast<uint8_t>(state::req_deposit);
            }
        });

        emit_event<events::action_request>(ses_id, event_type::action_request, { action_type });
    }

    void require_random(uint64_t ses_id) {
        const auto session_itr = sessions.require_find(ses_id, "session with this ses_id not found");
        eosio::check(session_itr->state == static_cast<uint8_t>(state::req_action), "state should be 'req_action'");

        sessions.modify(session_itr, get_self(), [&](auto& obj){
            obj.state = static_cast<uint8_t>(state::req_signidice_part_1);
        });

        emit_event<events::signidice_part_1_request>(ses_id, event_type::signidice_part_1_request, { session_itr->digest });
    }

    void finish_game(uint64_t ses_id, asset player_payout) {
        const auto session_itr = sessions.require_find(ses_id, "session with this ses_id not found");
        eosio::check(session_itr->state == static_cast<uint8_t>(state::req_signidice_part_2) ||
                     session_itr->state == static_cast<uint8_t>(state::req_action),
        "state should be 'req_signidice_part_2' or 'req_action'");

        asset player_win = player_payout - session_itr->deposit;
        eosio::check(player_win <= session_itr->last_max_win, "player win should be less than 'last_max_win'");

        const auto casino_name = get_casino(ses_id);

        /* payout more than deposit */
        if (player_win.amount > 0) {
            // request player win from casino
            transfer_from_casino(casino_name, session_itr->player, player_win);
            // transfer back deposit
            transfer(session_itr->player, session_itr->deposit, "player win[game]");
        }
        else {
            /* player payout more than 0 */
            if (player_payout.amount > 0)
                transfer(session_itr->player, player_payout, "player win[game]");

            /* all remain funds transfers to casino */
            auto to_casino = session_itr->deposit - player_payout;
            if (to_casino.amount > 0)
                transfer(casino_name, to_casino, "casino win");
        }

        sessions.modify(session_itr, get_self(), [&](auto& obj){
            obj.last_update = eosio::current_time_point();
            obj.state = static_cast<uint8_t>(state::finished);
        });

        notify_close_session(ses_id);

        emit_event<events::game_finished>(ses_id, event_type::game_finished, { player_win });

        sessions.erase(session_itr);

        on_finish(ses_id);
    }

    void update_max_win(uint64_t ses_id, asset new_max_win) {
        const auto session_itr = sessions.require_find(ses_id, "session with this ses_id not found");
        require_auth({session_itr->player, player_game_permission});
        eosio::check(session_itr->state == static_cast<uint8_t>(state::req_action), "state should be 'req_action'");

        auto max_win_delta = new_max_win - session_itr->last_max_win;

        notify_update_session(ses_id, max_win_delta);

        sessions.modify(session_itr, get_self(), [&](auto& obj){
            obj.last_max_win = new_max_win;
        });
    }

public:
    /* deposit handler */
    void on_transfer(name from, name to, asset quantity, std::string memo) {
        if (to != get_self()) {
            return;
        }

        const auto ses_id = get_ses_id(memo);

        eosio::check(quantity.symbol == core_symbol, "invalid token symbol");

        const auto session_itr = sessions.find(ses_id);

        if (session_itr == sessions.end()) {
            eosio::check(platform::read::is_active_game(get_platform(), get_self_id()), "game is't listed in platform");

            sessions.emplace(get_self(), [&](auto& row){
                row.ses_id = ses_id;
                row.ses_seq = global.session_seq++;
                row.player = from;
                row.deposit = quantity;
                row.last_update = eosio::current_time_point();
                row.last_max_win = zero_asset;
                row.state = static_cast<uint8_t>(state::req_start);
            });
        }
        else {
            eosio::check(session_itr->player == from, "only player can deposit");
            eosio::check(session_itr->state == static_cast<uint8_t>(state::req_deposit), "state should be 'req_deposit'");
            eosio::check(!is_expired(ses_id), "session expired");
            sessions.modify(session_itr, get_self(), [&](auto& row){
                row.deposit += quantity;
                row.last_update = eosio::current_time_point();
                row.state = static_cast<uint8_t>(state::req_action);
            });
        }
    }

    /* contract actions */
    CONTRACT_ACTION(newgame)
    void new_game(uint64_t ses_id, uint64_t casino_id) {
        const auto session_itr = sessions.require_find(ses_id, "session with this ses_id not found");
        require_auth({session_itr->player, player_game_permission});
        eosio::check(session_itr->state == static_cast<uint8_t>(state::req_start), "state should be 'req_start'");
        eosio::check(platform::read::is_active_casino(get_platform(), casino_id), "casino is't listed in platform");
        eosio::check(!is_expired(ses_id), "session expired");

        const auto casino_addr = get_casino(ses_id);;
        casino::game_table casino_games(casino_addr, casino_addr.value);
        const auto game_params = casino_games.get(get_self_id(), "game isn't listed in casino").params;
        const auto init_digest = calc_seed(ses_id);

        sessions.modify(session_itr, get_self(), [&](auto& obj){
            obj.last_update = eosio::current_time_point();
            obj.casino_id = casino_id;
            obj.digest = init_digest;
            obj.params = game_params;
        });

        emit_event<events::game_started>(ses_id, event_type::game_started, { });

        on_new_game(ses_id);
    }

    CONTRACT_ACTION(gameaction)
    void game_action(uint64_t ses_id, uint16_t type, std::vector<uint32_t> params) {
        const auto session_itr = sessions.require_find(ses_id, "session with this ses_id not found");
        require_auth({session_itr->player, player_game_permission});
        eosio::check(!is_expired(ses_id), "session expired");

        // allow `req_deposit` in case of zero deposit from player
        eosio::check(session_itr->state == static_cast<uint8_t>(state::req_action) ||
                     session_itr->state == static_cast<uint8_t>(state::req_deposit),
        "state should be 'req_deposit' or 'req_action'");

        sessions.modify(session_itr, get_self(), [&](auto& obj){
            obj.last_update = eosio::current_time_point();
            obj.state = static_cast<uint8_t>(state::req_action);
        });

        on_action(ses_id, type, params);
    }

    CONTRACT_ACTION(sgdicefirst)
    void signidice_part_1(uint64_t ses_id, const std::string& sign) {
        require_auth({get_platform(), platform_signidice_permission});
        const auto session_itr = sessions.require_find(ses_id, "session with this ses_id not found");
        eosio::check(!is_expired(ses_id), "session expired");
        eosio::check(session_itr->state == static_cast<uint8_t>(state::req_signidice_part_1),
        "state should be 'req_signidice_part_1'");

        const auto& platform_rsa_key = platform::read::get_rsa_pubkey(get_platform());
        eosio::check(daobet::rsa_verify(session_itr->digest, sign, platform_rsa_key), "invalid signature");

        const auto new_digest = eosio::sha256(sign.data(), sign.size());

        sessions.modify(session_itr, get_self(), [&](auto& obj){
            obj.digest = new_digest;
            obj.last_update = eosio::current_time_point();
            obj.state = static_cast<uint8_t>(state::req_signidice_part_2);
        });

        emit_event<events::signidice_part_1_request>(ses_id, event_type::signidice_part_1_request, { new_digest });
    }

    CONTRACT_ACTION(sgdicesecond)
    void signidice_part_2(uint64_t ses_id, const std::string& sign) {
        const auto session_itr = sessions.require_find(ses_id, "session with this ses_id not found");
        require_auth({get_casino(ses_id), casino_signidice_permission});
        eosio::check(!is_expired(ses_id), "session expired");
        eosio::check(session_itr->state == static_cast<uint8_t>(state::req_signidice_part_2),
        "state should be 'req_signidice_part_2'");

        const auto& cas_rsa_pubkey = platform::read::get_casino(get_platform(), session_itr->casino_id).rsa_pubkey;
        eosio::check(daobet::rsa_verify(session_itr->digest, sign, cas_rsa_pubkey), "invalid signature");
        const auto new_digest = eosio::sha256(sign.data(), sign.size());

        sessions.modify(session_itr, get_self(), [&](auto& obj){
            obj.digest = new_digest;
            obj.last_update = eosio::current_time_point();
        });

        on_random(ses_id, new_digest);
    }

    CONTRACT_ACTION(close)
    void close(uint64_t ses_id) {
        const auto session_itr = sessions.require_find(ses_id, "session with this ses_id not found");
        eosio::check(is_expired(ses_id), "session isn't expired, only expired session can be closed");

        asset player_win = zero_asset;

        switch (static_cast<state>(session_itr->state)) {
        /* if casino doesn't provide signidice we assume that casino lost */
        case state::req_signidice_part_2:
            // transfer deposit to player
            transfer(session_itr->player, session_itr->deposit, "player win [session expired]");
            // request last reported max_win amount funds for player
            transfer_from_casino(get_casino(ses_id), session_itr->player, session_itr->last_max_win);
            player_win = session_itr->last_max_win;
            break;

        /* if platform doesn't provide signidice we refund deposit to player */
        case state::req_signidice_part_1:
            // transfer deposit to player
            transfer(session_itr->player, session_itr->deposit, "refund [session expired]");
            break;

        /* in other cases we assume that player lost */
        default:
            transfer(get_casino(ses_id), session_itr->deposit, "player lost");
            player_win -= session_itr->deposit;
        }

        notify_close_session(ses_id);

        emit_event<events::game_failed>(ses_id, event_type::game_failed, { player_win });

        sessions.erase(session_itr);

        on_finish(ses_id);
    }

    CONTRACT_ACTION(init)
    void init(name platform, name events, uint32_t session_ttl) {
        require_auth(get_self());
        eosio::check(eosio::is_account(platform), "platform account doesn't exists");
        eosio::check(eosio::is_account(events), "events account doesn't exists");

        global.platform = platform;
        global.events = events;
        global.session_ttl = session_ttl;

        on_init();
    }

private:
    session_table sessions;
    global_row global;

private:
    template <typename Event>
    void emit_event(uint64_t ses_id, event_type type, const Event& event) {
        const auto session_itr = sessions.require_find(ses_id, "session with this ses_id not found");

        const auto data_bytes = eosio::pack<Event>(event);

        eosio::action(
            {get_self(),"active"_n},
            get_events(),
            "send"_n,
            std::make_tuple(
                get_self(),
                session_itr->casino_id,
                get_self_id(),
                ses_id,
                static_cast<uint32_t>(type),
                data_bytes
            )
        ).send();
    }

    uint64_t get_ses_id(const std::string& str) const {
        return std::stoul(str);
    }

    uint64_t get_self_id() const {
        return platform::read::get_game(get_platform(), get_self()).id;
    }

    name get_platform() const {
        return global.platform;
    }

    name get_events() const {
        return global.events;
    }

    name get_casino(uint64_t ses_id) const {
        const auto session_itr = sessions.require_find(ses_id, "session with this ses_id not found");
        return platform::read::get_casino(get_platform(), session_itr->casino_id).contract;
    }

    bool is_expired(uint64_t ses_id) const {
        const auto session_itr = sessions.require_find(ses_id, "session with this ses_id not found");
        return eosio::current_time_point().sec_since_epoch() - session_itr->last_update.sec_since_epoch() > global.session_ttl;
    }

    checksum256 calc_seed(uint64_t ses_id) const {
        const auto session_itr = sessions.require_find(ses_id, "session with this ses_id not found");
        std::array<uint64_t, 4> values {
            get_self_id(),
            session_itr->casino_id,
            session_itr->ses_seq,
            session_itr->player.value
        };
        return checksum256(values);
    }

    void transfer_from_casino(name casino, name to, asset amount) const {
        eosio::action(
            {get_self(),"active"_n},
            casino,
            "onloss"_n,
            std::make_tuple(get_self(), to, amount)
        ).send();
    }

    void transfer(name to, asset amount, const std::string& memo = "") const {
        eosio::action(
            {get_self(),"active"_n},
            "eosio.token"_n,
            "transfer"_n,
            std::make_tuple(get_self(), to, amount, memo)
        ).send();
    }

    void notify_update_session(uint64_t ses_id, asset max_win_delta) const {
        eosio::action(
            {get_self(),"active"_n},
            get_casino(ses_id),
            "sesupdate"_n,
            std::make_tuple(
                get_self(),
                max_win_delta
            )
        ).send();
    }

    void notify_close_session(uint64_t ses_id) const {
        const auto last_max_win = sessions.get(ses_id).last_max_win;
        eosio::action(
            {get_self(),"active"_n},
            get_casino(ses_id),
            "sesclose"_n,
            std::make_tuple(
                get_self(),
                last_max_win
            )
        ).send();
    }
};

const asset game::zero_asset = asset(0, game::core_symbol);


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

