#pragma once

#include <variant>
#include <vector>

#include <casino/casino.hpp>
#include <events/events.hpp>
#include <platform/platform.hpp>

#include <eosio/asset.hpp>
#include <eosio/crypto.hpp>
#include <eosio/eosio.hpp>
#include <eosio/serialize.hpp>

#include <game-contract-sdk/dispatcher.hpp>
#include <game-contract-sdk/rsa.hpp>
#include <game-contract-sdk/service.hpp>

// abi generator hack
#ifndef NOABI
#define CONTRACT_ACTION(act_name) [[eosio::action(#act_name)]]
#else
#define CONTRACT_ACTION(act_name)
#endif

#define IS_DEBUG true

namespace game_sdk {

using bytes = std::vector<char>;
using casino::game_params_type;
using eosio::asset;
using eosio::checksum256;
using eosio::name;
using eosio::require_auth;
using eosio::symbol;
using eosio::time_point;
using param_t = uint64_t;

class game : public eosio::contract {
  public:
    using eosio::contract::contract;
    static constexpr name player_game_permission = "game"_n;
    static constexpr name platform_signidice_permission = "signidice"_n;
    static constexpr name casino_signidice_permission = "signidice"_n;
    static constexpr symbol core_symbol = symbol("BET", 4);
    static const asset zero_asset; // asset does't have constexpr contructor :(

    /* game states */
    // clang-format off
    enum class state : uint8_t {
        req_deposit = 0,        // <- init|req_signidice_part_2, -> req_start|req_action
        req_start,              // <- req_deposit, -> req_action|failed
        req_action,             // <- req_start|req_deposit, -> req_signidice_part_1|failed
        req_signidice_part_1,   // <- req_action, -> req_signidice_part_2|falied
        req_signidice_part_2,   // <- req_signidice_part_1, -> finished|req_deposit|req_action|failed
        finished,               // <- req_signidice_part_2
    };
    // clang-format on

    /* event data structures, type field doesn't serialize */
    struct events {
        struct game_started {
            static constexpr uint32_t type{0u};

            EOSLIB_SERIALIZE(game_started, )
        };

        struct action_request {
            static constexpr uint32_t type{1u};

            uint8_t action_type;
            bool need_deposit;
            EOSLIB_SERIALIZE(action_request, (action_type)(need_deposit))
        };

        struct signidice_part_1_request {
            static constexpr uint32_t type{2u};

            checksum256 digest;
            EOSLIB_SERIALIZE(signidice_part_1_request, (digest))
        };

        struct signidice_part_2_request {
            static constexpr uint32_t type{3u};

            checksum256 digest;
            EOSLIB_SERIALIZE(signidice_part_2_request, (digest))
        };

        struct game_finished {
            static constexpr uint32_t type{4u};

            asset player_win_amount;
            bytes msg;
            EOSLIB_SERIALIZE(game_finished, (player_win_amount)(msg))
        };

        struct game_failed {
            static constexpr uint32_t type{5u};

            asset player_win_amount;
            bytes msg;
            EOSLIB_SERIALIZE(game_failed, (player_win_amount)(msg))
        };

        struct game_message {
            static constexpr uint32_t type{6u};

            bytes msg;
            EOSLIB_SERIALIZE(game_message, (msg))
        };
    };

    /* global state variables */
    struct [[eosio::table("global"), eosio::contract("game")]] global_row {
        uint64_t session_seq{0u};
        name platform;
        name events;
        uint32_t session_ttl;
    };
    using global_singleton = eosio::singleton<"global"_n, global_row>;

#ifdef IS_DEBUG
    /* debug table */
    struct [[eosio::table("debug"), eosio::contract("game")]] debug_table {
        std::queue<checksum256> pseudo_queue;
    };

    using debug_singleton = eosio::singleton<"debug"_n, debug_table>;
#endif

    /* session struct */
    // clang-format off
    struct [[eosio::table("session"), eosio::contract("game")]] session_row {
        uint64_t ses_id;
        uint64_t casino_id;
        uint64_t ses_seq;
        name player;
        uint8_t state;
        game_params_type params; // <- game params, copied from casino contract aboid of params changing while active session
        asset deposit;           // <- current player deposit amount, increments when new deposit received
        checksum256 digest;      // <- signidice result, set seed value on new_game
        time_point last_update;  // <-- last action time
        asset last_max_win;      // <- last max win value, updates after on_action

        uint64_t primary_key() const { return ses_id; }
    };
    // clang-format on

    using session_table = eosio::multi_index<"session"_n, session_row>;

  public:
    game(name receiver, name code, eosio::datastream<const char*> ds)
        : contract(receiver, code, ds), sessions(_self, _self.value) {
        // load global singleton to memory
        global = global_singleton(_self, _self.value).get_or_default();

#ifdef IS_DEBUG
        global_debug = debug_singleton(_self, _self.value).get_or_default();
#endif
    }

    virtual ~game() {
        // store singleton after all operations
        global_singleton(_self, _self.value).set(global, _self);
#ifdef IS_DEBUG
        debug_singleton(_self, _self.value).set(global_debug, _self);
#endif
    }

  protected:
    // =============================================================
    // Optional
    // =============================================================

    /* onlinal contract initialization callback */
    virtual void on_init() {}
    virtual void on_finish(uint64_t ses_id) {}

    // =============================================================
    // Must be overridden
    // =============================================================

    /* game session life-cycle callbacks */
    virtual void on_new_game(uint64_t ses_id) = 0;
    virtual void on_action(uint64_t ses_id, uint16_t type, std::vector<param_t> params) = 0;
    virtual void on_random(uint64_t ses_id, checksum256 rand) = 0;

    // =============================================================
    // Getters
    // =============================================================
    const global_row& get_global() const { return global; }

#ifdef IS_DEBUG
    const debug_table& get_debug() const { return global_debug; }
#endif

    const session_row& get_session(uint64_t ses_id) const {
        return sessions.get(ses_id, "session with this ses_id not found");
    }

    std::optional<param_t> get_param_value(uint64_t ses_id, uint16_t param_type) const {
        const auto& session = sessions.get(ses_id);

        const auto itr = std::find_if(
            session.params.begin(), session.params.end(), [&](const auto& item) { return item.first == param_type; });

        return itr == session.params.end() ? std::nullopt : std::optional<param_t>{itr->second};
    }

  protected:
    /* game session state changers */
    void require_action(uint8_t action_type, bool need_deposit = false) {
        const auto& session = get_session(current_session);

        check_only_states(session,
                          {state::req_start, state::req_signidice_part_2},
                          "state should be 'req_start' or 'req_signidice_part_2'");

        sessions.modify(session, get_self(), [&](auto& obj) {
            if (!need_deposit) {
                obj.state = static_cast<uint8_t>(state::req_action);
            } else {
                obj.state = static_cast<uint8_t>(state::req_deposit);
            }
        });

        emit_event(session, events::action_request{action_type, need_deposit});
    }

    void require_random() {
        const auto& session = get_session(current_session);

        check_only_states(session, {state::req_action}, "state should be 'req_action'");

        sessions.modify(
            session, get_self(), [&](auto& obj) { obj.state = static_cast<uint8_t>(state::req_signidice_part_1); });

        emit_event(session, events::signidice_part_1_request{session.digest});
    }

    void send_game_message(bytes&& msg) { emit_event(get_session(current_session), events::game_message{msg}); }

    void finish_game(asset player_payout) {
        const auto& session = get_session(current_session);
        finish_game(player_payout, std::nullopt);
    }

    void finish_game(asset player_payout, std::optional<bytes>&& msg) {
        const auto& session = get_session(current_session);

        check_only_states(session,
                          {state::req_action, state::req_signidice_part_2},
                          "state should be 'req_signidice_part_2' or 'req_action'");

        // player_payout is total payout, here we calculate player profit
        asset player_win = player_payout - session.deposit;
        eosio::check(player_win <= session.last_max_win, "player win should be less than 'last_max_win'");

        const auto casino_name = get_casino(session);

        /* payout more than deposit */
        if (player_win.amount > 0) {
            // request player win from casino
            transfer_from_casino(casino_name, session.player, player_win);
            // transfer back deposit
            transfer(session.player, session.deposit, "player win[game]");
        } else {
            /* player payout more than 0 */
            if (player_payout.amount > 0)
                transfer(session.player, player_payout, "player win[game]");

            /* all remain funds transfers to casino */
            auto to_casino = session.deposit - player_payout;
            if (to_casino.amount > 0)
                transfer(casino_name, to_casino, "casino win");
        }

        sessions.modify(session, get_self(), [&](auto& obj) {
            obj.last_update = eosio::current_time_point();
            obj.state = static_cast<uint8_t>(state::finished);
        });

        notify_close_session(session);

        if (msg.has_value())
            emit_event(session, events::game_finished{player_win, msg.value()});
        else
            emit_event(session, events::game_finished{player_win});

        sessions.erase(session);

        on_finish(current_session);
    }

#ifdef IS_DEBUG
    void push_next_random(checksum256 && next_random) {
        global_debug.pseudo_queue.push(next_random);
    }
#endif

    // new_max_win - total payout including deposit
    void update_max_win(asset new_max_win) {
        const auto& session = get_session(current_session);

        check_only_states(session, {state::req_action}, "state should be 'req_action'");

        // max casino loss
        const auto max_casino_lost = new_max_win - session.deposit;

        // casino require max_win delta
        const auto max_win_delta = max_casino_lost - session.last_max_win;

        notify_update_session(session, max_win_delta);

        sessions.modify(session, get_self(), [&](auto& obj) { obj.last_max_win = max_casino_lost; });
    }

  public:
    /* deposit handler */
    void on_transfer(name from, name to, asset quantity, std::string memo) {
        if (to != get_self()) {
            return;
        }

        const auto ses_id = get_ses_id(memo);

        eosio::check(quantity.symbol == core_symbol, "invalid token symbol");

        // if session doesn't exists create new session
        if (sessions.find(ses_id) == sessions.end()) {
            check_active_game();

            sessions.emplace(get_self(), [&](auto& row) {
                row.ses_id = ses_id;
                row.ses_seq = global.session_seq++;
                row.player = from;
                row.deposit = quantity;
                row.last_update = eosio::current_time_point();
                row.last_max_win = zero_asset;
                row.state = static_cast<uint8_t>(state::req_start);
            });
        } else { /* else add extra deposit */
            const auto& session = get_session(ses_id);

            check_not_expired(session);
            check_only_states(session, {state::req_deposit}, "state should be 'req_deposit'");
            eosio::check(session.player == from, "only player can deposit");

            sessions.modify(session, get_self(), [&](auto& row) {
                row.deposit += quantity;
                row.last_update = eosio::current_time_point();
                row.state = static_cast<uint8_t>(state::req_action);
            });
        }
    }

    /* contract actions */
    CONTRACT_ACTION(newgame)
    void new_game(uint64_t ses_id, uint64_t casino_id) {
        set_current_session(ses_id);
        const auto& session = get_session(ses_id);

        /* whitelist checks */
        check_active_game();
        check_active_casino(session);
        check_active_game_in_casino(session);

        /* auth & state checks */
        check_from_player(session);
        check_not_expired(session);
        check_only_states(session, {state::req_start}, "state should be 'req_start'");

        const auto game_params = fetch_game_params(session);
        const auto init_digest = calc_seed(session);

        sessions.modify(session, get_self(), [&](auto& obj) {
            obj.last_update = eosio::current_time_point();
            obj.casino_id = casino_id;
            obj.digest = init_digest;
            obj.params = game_params;
        });

        notify_new_session(session);

        emit_event(session, events::game_started{});

        on_new_game(ses_id);
    }

    CONTRACT_ACTION(gameaction)
    void game_action(uint64_t ses_id, uint16_t type, std::vector<param_t> params) {
        set_current_session(ses_id);
        const auto& session = get_session(ses_id);

        check_from_player(session);
        check_not_expired(session);

        // allow `req_deposit` in case of zero deposit from player
        check_only_states(
            session, {state::req_action, state::req_deposit}, "state should be 'req_deposit' or 'req_action'");

        sessions.modify(session, get_self(), [&](auto& obj) {
            obj.last_update = eosio::current_time_point();
            obj.state = static_cast<uint8_t>(state::req_action);
        });

        on_action(ses_id, type, params);
    }

    CONTRACT_ACTION(sgdicefirst)
    void signidice_part_1(uint64_t ses_id, const std::string& sign) {
        set_current_session(ses_id);
        const auto& session = get_session(ses_id);

        check_from_platform_signidice();
        check_not_expired(session);
        check_only_states(session, {state::req_signidice_part_1}, "state should be 'req_signidice_part_1'");

        const auto& platform_rsa_key = platform::read::get_rsa_pubkey(get_platform());
        eosio::check(daobet::rsa_verify(session.digest, sign, platform_rsa_key), "invalid signature");

        const auto new_digest = eosio::sha256(sign.data(), sign.size());

        sessions.modify(session, get_self(), [&](auto& obj) {
            obj.digest = new_digest;
            obj.last_update = eosio::current_time_point();
            obj.state = static_cast<uint8_t>(state::req_signidice_part_2);
        });

        emit_event(session, events::signidice_part_1_request{new_digest});
    }

    CONTRACT_ACTION(sgdicesecond)
    void signidice_part_2(uint64_t ses_id, const std::string& sign) {
        set_current_session(ses_id);
        const auto& session = get_session(ses_id);

        check_from_casino_signidice(session);
        check_not_expired(session);
        check_only_states(session, {state::req_signidice_part_2}, "state should be 'req_signidice_part_2'");

        const auto& cas_rsa_pubkey = platform::read::get_casino(get_platform(), session.casino_id).rsa_pubkey;
        eosio::check(daobet::rsa_verify(session.digest, sign, cas_rsa_pubkey), "invalid signature");
        const auto new_digest = eosio::sha256(sign.data(), sign.size());

        sessions.modify(session, get_self(), [&](auto& obj) {
            obj.digest = new_digest;
            obj.last_update = eosio::current_time_point();
        });

#ifdef IS_DEBUG
        if (auto& pseudo_queue = global_debug.pseudo_queue; !pseudo_queue.empty()) {
            const checksum256 new_digest = pseudo_queue.front();
            pseudo_queue.pop();
            on_random(ses_id, new_digest);
            return;
        }
#endif
        on_random(ses_id, new_digest);
    }

    CONTRACT_ACTION(close)
    void close(uint64_t ses_id) {
        set_current_session(ses_id);
        const auto& session = get_session(ses_id);

        eosio::check(is_expired(session), "session isn't expired, only expired session can be closed");

        asset player_win = zero_asset;

        switch (static_cast<state>(session.state)) {
        /* if casino doesn't provide signidice we assume that casino lost */
        case state::req_signidice_part_2:
            // transfer deposit to player
            transfer(session.player, session.deposit, "player win [session expired]");
            // request last reported max_win amount funds for player
            transfer_from_casino(get_casino(session), session.player, session.last_max_win);
            player_win = session.last_max_win;
            break;

        /* if platform doesn't provide signidice we refund deposit to player */
        case state::req_signidice_part_1:
            // transfer deposit to player
            transfer(session.player, session.deposit, "refund [session expired]");
            break;

        /* in other cases we assume that player lost */
        default:
            transfer(get_casino(session), session.deposit, "player lost");
            player_win -= session.deposit;
        }

        notify_close_session(session);

        emit_event(session, events::game_failed{player_win});

        sessions.erase(session);

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
    uint64_t current_session; // id of session for which was called action

#ifdef IS_DEBUG
    debug_table global_debug;
#endif

  private:
    template <typename Event> void emit_event(const session_row& ses, const Event& event) {
        const auto data_bytes = eosio::pack<Event>(event);

        eosio::action({get_self(), "active"_n},
                      get_events(),
                      "send"_n,
                      std::make_tuple(get_self(), ses.casino_id, get_self_id(), ses.ses_id, event.type, data_bytes))
            .send();
    }

    uint64_t get_ses_id(const std::string& str) const { return std::stoul(str); }

    uint64_t get_self_id() const { return platform::read::get_game(get_platform(), get_self()).id; }

    name get_platform() const { return global.platform; }

    name get_events() const { return global.events; }

    name get_casino(const session_row& ses) const {
        return platform::read::get_casino(get_platform(), ses.casino_id).contract;
    }

    game_params_type fetch_game_params(const session_row& ses) const {
        const auto casino = get_casino(ses);
        casino::game_table casino_games(casino, casino.value);
        return casino_games.get(get_self_id()).params;
    }

    bool is_expired(const session_row& ses) const {
        return eosio::current_time_point().sec_since_epoch() - ses.last_update.sec_since_epoch() > global.session_ttl;
    }

    checksum256 calc_seed(const session_row& ses) const {
        std::array<uint64_t, 4> values{get_self_id(), ses.casino_id, ses.ses_seq, ses.player.value};
        return checksum256(values);
    }

    void transfer_from_casino(name casino, name to, asset amount) const {
        eosio::action({get_self(), "active"_n}, casino, "onloss"_n, std::make_tuple(get_self(), to, amount)).send();
    }

    void transfer(name to, asset amount, const std::string& memo = "") const {
        eosio::action(
            {get_self(), "active"_n}, "eosio.token"_n, "transfer"_n, std::make_tuple(get_self(), to, amount, memo))
            .send();
    }

    void notify_new_session(const session_row& ses) const {
        eosio::action({get_self(), "active"_n}, get_casino(ses), "newsession"_n, std::make_tuple(get_self())).send();
    }

    void notify_update_session(const session_row& ses, asset max_win_delta) const {
        eosio::action(
            {get_self(), "active"_n}, get_casino(ses), "sesupdate"_n, std::make_tuple(get_self(), max_win_delta))
            .send();
    }

    void notify_close_session(const session_row& ses) const {
        eosio::action(
            {get_self(), "active"_n}, get_casino(ses), "sesclose"_n, std::make_tuple(get_self(), ses.last_max_win))
            .send();
    }

    void set_current_session(uint64_t ses_id) { current_session = ses_id; }

  private:
    /* checkers */
    void check_only_states(const session_row& ses,
                           std::initializer_list<state> states,
                           const char* err = "invalid state") const {
        bool ok = false;
        for (auto&& st : states) {
            if (static_cast<state>(ses.state) == st) {
                ok = true;
                break;
            }
        }
        eosio::check(ok, err);
    }

    void check_not_expired(const session_row& ses) const { eosio::check(!is_expired(ses), "session expired"); }

    void check_active_game() const {
        eosio::check(platform::read::is_active_game(get_platform(), get_self_id()), "game is't active in platform");
    }

    void check_active_game_in_casino(const session_row& ses) const {
        const auto casino_addr = get_casino(ses);
        casino::game_table casino_games(casino_addr, casino_addr.value);
        const auto& game = casino_games.require_find(get_self_id(), "game isn't listed in casino");
        eosio::check(!game->paused, "game isn't active in casino");
    }

    void check_active_casino(const session_row& ses) const {
        eosio::check(platform::read::is_active_casino(get_platform(), ses.casino_id), "casino is't active in platform");
    }

    void check_from_player(const session_row& ses) const { require_auth({ses.player, player_game_permission}); }

    void check_from_casino_signidice(const session_row& ses) const {
        require_auth({get_casino(ses), casino_signidice_permission});
    }

    void check_from_platform_signidice() const { require_auth({get_platform(), platform_signidice_permission}); }
};

const asset game::zero_asset = asset(0, game::core_symbol);

} // namespace game_sdk
