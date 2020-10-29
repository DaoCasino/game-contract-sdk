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
#include <game-contract-sdk/service.hpp>

// abi generator hack
#ifndef NOABI
#define CONTRACT_ACTION(act_name) [[eosio::action(#act_name)]]
#else
#define CONTRACT_ACTION(act_name)
#endif

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
    static constexpr name platform_game_permission = "gameaction"_n;
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

    /* session struct */
    // clang-format off
    struct [[eosio::table("session"), eosio::contract("game")]] session_row {
        uint64_t ses_id;
        uint64_t casino_id;
        uint64_t ses_seq;
        name player;
        uint8_t state;
        game_params_type params; // <- game params, copied from casino contract; avoid params changing during active session
        asset deposit;           // <- player's total deposit = real tokens + bonus, increases on
                                 //    transfer, newgamebon and depositbon actions
        asset bonus_deposit;     // <- player's bonus deposit
        checksum256 digest;      // <- signidice result, set seed value on new_game
        time_point last_update;  // <-- last action time
        asset last_max_win;      // <- last max win value, updated after on_action

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

    /* contract initialization callback */
    virtual void on_init() {}
    /* game session finalization callback */
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

    const session_row& get_session(uint64_t ses_id) const {
        return sessions.get(ses_id, "session with this ses_id not found");
    }

    std::optional<param_t> get_param_value(uint64_t ses_id, uint16_t param_type) const {
        const auto& session = sessions.get(ses_id);

        const auto itr = std::find_if(
            session.params.begin(), session.params.end(), [&](const auto& item) { return item.first == param_type; });

        return itr == session.params.end() ? std::nullopt : std::optional<param_t>{itr->second};
    }

    // =============================================================
    // PRNG
    // =============================================================
    service::PRNG::Ptr get_prng(checksum256&& seed) const {

#ifdef IS_DEBUG
        if (!get_debug().pseudo_prng.empty()) {
            const auto pseudo_prng = get_debug().pseudo_prng;
            global_debug.pseudo_prng.clear();
            return std::make_shared<service::PseudoPRNG>(pseudo_prng);
        }
#endif

        return std::make_shared<service::ShaMixWithRejection>(seed);
    }

  protected:
    /* game session state changers */
    void require_action(uint8_t action_type, bool need_deposit = false) {
        const auto& session = get_session(current_session);

        check_only_states(session,
                          {state::req_start, state::req_action, state::req_signidice_part_2},
                          "state should be 'req_start', 'req_action' or 'req_signidice_part_2'");

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

        check_only_states(session, {state::req_action, state::req_signidice_part_2}, "state should be 'req_action' or 'req_signidice_part_2'");

        sessions.modify(
            session, get_self(), [&](auto& obj) { obj.state = static_cast<uint8_t>(state::req_signidice_part_1); });

        emit_event(session, events::signidice_part_1_request{session.digest});
    }

    void send_game_message(bytes&& msg) { emit_event(get_session(current_session), events::game_message{msg}); }

    void send_game_message(std::vector<param_t>&& msg) { send_game_message(eosio::pack(msg)); }

    void finish_game(asset player_payout) {
        const auto& session = get_session(current_session);
        finish_game(player_payout, std::nullopt);
    }

    void finish_game(asset player_payout, std::optional<std::vector<param_t>>&& msg) {
        const auto& session = get_session(current_session);

        check_only_states(session,
                          {state::req_action, state::req_signidice_part_2},
                          "state should be 'req_signidice_part_2' or 'req_action'");

        // player_payout is total payout, here we calculate player profit
        const auto player_win = player_payout - session.deposit;
        eosio::check(player_win <= session.last_max_win, "player win should be less than 'last_max_win'");

        const auto casino_name = get_casino(session.casino_id);
        const auto bonus_payout = player_payout * session.bonus_deposit.amount / session.deposit.amount;
        const auto real_payout = player_payout - bonus_payout;

        /* payout more than deposit */
        if (player_win > zero_asset) {
            // request player win from casino
            const auto bonus_win = player_win * session.bonus_deposit.amount / session.deposit.amount;
            const auto real_win = player_win - bonus_win;

            // transfer win
            transfer_from_casino(casino_name, session.player, real_win);
            transfer_bonus_from_casino(casino_name, session.player, session.bonus_deposit + bonus_win);

            // transfer back real deposit
            transfer(session.player, session.deposit - session.bonus_deposit, "player win[game]");
        } else {
            transfer(session.player, real_payout, "player win[game]");
            transfer_bonus_from_casino(casino_name, session.player, bonus_payout);

            /* only transfer real win, bonuses are 'burned' */
            const auto real_deposit = session.deposit - session.bonus_deposit;
            const auto casino_real_win = real_deposit - real_payout;
            transfer(casino_name, casino_real_win, "casino win");
        }

        notify_new_payout(session, real_payout);

        sessions.modify(session, get_self(), [&](auto& obj) {
            obj.last_update = eosio::current_time_point();
            obj.state = static_cast<uint8_t>(state::finished);
        });

        notify_close_session(session);

        if (msg.has_value())
            emit_event(session, events::game_finished{player_win, eosio::pack(msg.value())});
        else
            emit_event(session, events::game_finished{player_win});

        sessions.erase(session);

        on_finish(current_session);
    }

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

        eosio::check(quantity.symbol == core_symbol, "invalid token symbol");

        const auto ses_id = get_ses_id(memo);

        if (sessions.find(ses_id) == sessions.end()) {
            check_active_game();
            create_session(ses_id, from, quantity);
        } else {
            handle_extra_deposit(get_session(ses_id), from, quantity);
        }
        notify_new_deposit(get_session(ses_id), quantity);
    }

    /* contract actions */
    CONTRACT_ACTION(newgame)
    void new_game(uint64_t ses_id, uint64_t casino_id) {
        new_game_internal(ses_id, casino_id);
    }

    CONTRACT_ACTION(newgameaffl)
    void new_game_affl(uint64_t ses_id, uint64_t casino_id, const std::string& affiliate_id) {
        new_game_internal(ses_id, casino_id);
    }

    CONTRACT_ACTION(newgamebon)
    void new_game_bonus(uint64_t ses_id, uint64_t casino_id, name from, asset quantity) {
        new_game_bonus_internal(ses_id, casino_id, from, quantity);
    }

    CONTRACT_ACTION(newgamebonaf)
    void new_game_bonus_affl(uint64_t ses_id, uint64_t casino_id, name from, asset quantity, const std::string& affiliate_id) {
        new_game_bonus_internal(ses_id, casino_id, from, quantity);
    }

    CONTRACT_ACTION(depositbon)
    void deposit_bonus(uint64_t ses_id, name from, asset quantity) {
        require_auth(from);
        const auto& session = get_session(ses_id);
        handle_extra_deposit(session, from, quantity);
        // deposit was already updated so we only need to update bonus deposit
        sessions.modify(session, get_self(), [&](auto& row) {
            row.bonus_deposit += quantity;
        });

        notify_new_bonus_deposit(session, quantity);
    }

    CONTRACT_ACTION(gameaction)
    void game_action(uint64_t ses_id, uint16_t type, std::vector<param_t> params) {
        set_current_session(ses_id);
        const auto& session = get_session(ses_id);

        check_from_platform_game();
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

    /*
     NOTE: that action dosn't check authorization by require_auth, but checks RSA sign
    */
    CONTRACT_ACTION(sgdicefirst)
    void signidice_part_1(uint64_t ses_id, const std::string& sign) {
        set_current_session(ses_id);
        const auto& session = get_session(ses_id);

        check_not_expired(session);
        check_only_states(session, {state::req_signidice_part_1}, "state should be 'req_signidice_part_1'");

        /* obtain platform's rsa key for signidice */
        const auto& platform_rsa_key = platform::read::get_rsa_pubkey(get_platform());

        /* check first part sign and calculate new digest */
        const auto new_digest = service::signidice(session.digest, sign, platform_rsa_key);

        sessions.modify(session, get_self(), [&](auto& obj) {
            obj.digest = new_digest;
            obj.last_update = eosio::current_time_point();
            obj.state = static_cast<uint8_t>(state::req_signidice_part_2);
        });

        /* emit event for second part of signidice with new digest */
        emit_event(session, events::signidice_part_2_request{new_digest});
    }

    /*
     NOTE: that action dosn't check authorization by require_auth, but checks RSA sign
    */
    CONTRACT_ACTION(sgdicesecond)
    void signidice_part_2(uint64_t ses_id, const std::string& sign) {
        set_current_session(ses_id);
        const auto& session = get_session(ses_id);

        check_not_expired(session);
        check_only_states(session, {state::req_signidice_part_2}, "state should be 'req_signidice_part_2'");

        /* obtain casino's rsa key for signidice */
        const auto& cas_rsa_pubkey = platform::read::get_casino(get_platform(), session.casino_id).rsa_pubkey;

        /* check second part sign and calculate resulting digest */
        const auto new_digest = service::signidice(session.digest, sign, cas_rsa_pubkey);

        sessions.modify(session, get_self(), [&](auto& obj) {
            obj.digest = new_digest;
            obj.last_update = eosio::current_time_point();
        });

        /* testing stuff for mock signidice result, uses only for testing */
#ifdef IS_DEBUG
        if (auto& pseudo_queue = global_debug.pseudo_queue; !pseudo_queue.empty()) {
            const checksum256 new_digest = pseudo_queue.back();
            pseudo_queue.pop_back();
            on_random(ses_id, new_digest);
            return;
        }
#endif

        /* return signidice result to game code */
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
            transfer_from_casino(get_casino(session.casino_id), session.player, session.last_max_win);
            notify_new_payout(session, session.last_max_win + session.deposit);
            player_win = session.last_max_win;
            break;

        /* if player doesn't start game just refund deposit (here we haven't info about casino) */
        case state::req_start:
        /* if platform doesn't provide signidice we refund deposit to player */
        case state::req_signidice_part_1:
            // transfer deposit to player
            transfer(session.player, session.deposit, "refund [session expired]");
            break;

        /* in other cases we assume that player lost */
        default:
            transfer(get_casino(session.casino_id), session.deposit, "player lost");
            player_win -= session.deposit;
        }

        // if session isn't started we have no info about casino and no need to perform any action
        if (static_cast<state>(session.state) == state::req_start) {
            sessions.erase(session);
            return;
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
    const session_row& create_session(uint64_t ses_id, name player, asset deposit) {
        return *sessions.emplace(get_self(), [&](auto& row) {
            row.ses_id = ses_id;
            row.ses_seq = global.session_seq++;
            row.player = player;
            row.deposit = deposit;
            row.bonus_deposit = zero_asset;
            row.last_update = eosio::current_time_point();
            row.last_max_win = zero_asset;
            row.state = static_cast<uint8_t>(state::req_start);
        });
    }

    const session_row& get_or_create_session(uint64_t ses_id, name player, asset deposit) {
        if (sessions.find(ses_id) == sessions.end()) {
            return create_session(ses_id, player, deposit);
        }
        return get_session(ses_id);
    }

    void handle_extra_deposit(const session_row& session, name from, asset quantity) {
        check_not_expired(session);
        check_only_states(session, {state::req_deposit, state::req_action}, "state should be 'req_deposit' or 'req_action'");
        eosio::check(session.player == from, "only player can deposit");

        sessions.modify(session, get_self(), [&](auto& row) {
            row.deposit += quantity;
            row.last_update = eosio::current_time_point();
            row.state = static_cast<uint8_t>(state::req_action);
        });
    }

    void new_game_internal(uint64_t ses_id, uint64_t casino_id) {
        set_current_session(ses_id);
        const auto& session = get_session(ses_id);

        /* whitelist checks */
        check_active_game();
        check_active_casino(casino_id);
        check_active_game_in_casino(casino_id);

        /* auth & state checks */
        check_from_platform_game();
        check_not_expired(session);
        check_only_states(session, {state::req_start}, "state should be 'req_start'");

        const auto game_params = fetch_game_params(casino_id);
        const auto init_digest = calc_seed(casino_id, session.ses_seq, session.player);

        // always be careful with ref after modify
        // ref will still life but refer to object without new updates
        sessions.modify(session, get_self(), [&](auto& obj) {
            obj.last_update = eosio::current_time_point();
            obj.casino_id = casino_id;
            obj.digest = init_digest;
            obj.params = game_params;
        });

        // update session ref (invalidates after modify)
        const auto& updated_session = get_session(ses_id);

        notify_new_session(updated_session);
        notify_new_session_legacy(updated_session);

        emit_event(updated_session, events::game_started{});

        on_new_game(ses_id);
    }

    void new_game_bonus_internal(uint64_t ses_id, uint64_t casino_id, name from, asset quantity) {
        // session might have been created if we transfered first
        const auto& session = get_or_create_session(ses_id, from, zero_asset);
        sessions.modify(session, get_self(), [&](auto& row) {
            row.deposit += quantity;
            row.bonus_deposit = quantity;
        });
        notify_new_bonus_deposit(session, quantity);

        // auth check & remaining logic
        new_game_internal(ses_id, casino_id);
    }

  private:
    session_table sessions;
    global_row global;
    uint64_t current_session; // id of session for which was called action

  private:
    template <typename Event> void emit_event(const session_row& ses, const Event& event) {
        const auto data_bytes = eosio::pack<Event>(event);

        eosio::action({get_self(), "active"_n},
                      get_events(),
                      "send"_n,
                      std::make_tuple(get_self(), ses.casino_id, get_self_id(), ses.ses_id, event.type, data_bytes))
            .send();
    }

    uint64_t get_ses_id(const std::string& str) const { return std::stoull(str); }

    uint64_t get_self_id() const { return platform::read::get_game(get_platform(), get_self()).id; }

    name get_platform() const { return global.platform; }

    name get_events() const { return global.events; }

    name get_casino(const uint64_t casino_id) const {
        return platform::read::get_casino(get_platform(), casino_id).contract;
    }

    game_params_type fetch_game_params(const uint64_t casino_id) const {
        const auto casino = get_casino(casino_id);
        casino::game_table casino_games(casino, casino.value);
        return casino_games.get(get_self_id()).params;
    }

    bool is_expired(const session_row& ses) const {
        return eosio::current_time_point().sec_since_epoch() - ses.last_update.sec_since_epoch() > global.session_ttl;
    }

    checksum256 calc_seed(uint64_t casino_id, uint64_t ses_seq, name player) const {
        std::array<uint64_t, 4> values {
            get_self_id(),
            casino_id,
            ses_seq,
            player.value
        };
        return checksum256(values);
    }

    void transfer_from_casino(name casino, name to, asset amount) const {
        if (amount == zero_asset) {
            return;
        }
        eosio::action({get_self(), "active"_n}, casino, "onloss"_n, std::make_tuple(get_self(), to, amount)).send();
    }

    void transfer(name to, asset amount, const std::string& memo = "") const {
        if (amount == zero_asset) {
            return;
        }
        eosio::action(
            {get_self(), "active"_n}, "eosio.token"_n, "transfer"_n, std::make_tuple(get_self(), to, amount, memo))
            .send();
    }

    void transfer_bonus_from_casino(name casino, name to, asset amount) const {
        if (amount == zero_asset) {
            return;
        }
        eosio::action({get_self(), "active"_n}, casino, "sesaddbon"_n, std::make_tuple(get_self(), to, amount)).send();
    }

    void notify_new_session(const session_row& ses) const {
        eosio::action(
            {get_self(),"active"_n},
            get_casino(ses.casino_id),
            "newsessionpl"_n,
            std::make_tuple(
                get_self(),
                ses.player
            )
        ).send();
    }

    void notify_new_session_legacy(const session_row& ses) const {
        eosio::action(
            {get_self(),"active"_n},
            get_casino(ses.casino_id),
            "newsession"_n,
            std::make_tuple(
                get_self()
            )
        ).send();
    }

    void notify_update_session(const session_row& ses, asset max_win_delta) const {
        eosio::action(
            {get_self(),"active"_n},
            get_casino(ses.casino_id),
            "sesupdate"_n,
            std::make_tuple(
                get_self(),
                max_win_delta
            )
        ).send();
    }

    void notify_close_session(const session_row& ses) const {
        eosio::action(
            {get_self(),"active"_n},
            get_casino(ses.casino_id),
            "sesclose"_n,
            std::make_tuple(
                get_self(),
                ses.last_max_win
            )
        ).send();
    }

    void notify_new_deposit(const session_row& ses, asset quantity) const {
        eosio::action(
            {get_self(),"active"_n},
            get_casino(ses.casino_id),
            "sesnewdepo"_n,
            std::make_tuple(
                get_self(),
                ses.player,
                quantity
            )
        ).send();
    }

    void notify_new_bonus_deposit(const session_row& ses, asset quantity) const {
        eosio::action(
            {get_self(),"active"_n},
            get_casino(ses.casino_id),
            "seslockbon"_n,
            std::make_tuple(
                get_self(),
                ses.player,
                quantity
            )
        ).send();
    }

    void notify_new_payout(const session_row& ses, asset quantity) const {
        eosio::action(
            {get_self(),"active"_n},
            get_casino(ses.casino_id),
            "sespayout"_n,
            std::make_tuple(
                get_self(),
                ses.player,
                quantity
            )
        ).send();
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

    void check_active_game_in_casino(const uint64_t casino_id) const {
        const auto casino_addr = get_casino(casino_id);
        casino::game_table casino_games(casino_addr, casino_addr.value);
        const auto& game = casino_games.require_find(get_self_id(), "game isn't listed in casino");
        eosio::check(!game->paused, "game isn't active in casino");
    }

    void check_active_casino(const uint64_t casino_id) const {
        eosio::check(platform::read::is_active_casino(get_platform(), casino_id), "casino is't active in platform");
    }

    void check_from_platform_game() const { require_auth({get_platform(), platform_game_permission}); }

#ifdef IS_DEBUG
  public:
    /* debug table */
    struct [[eosio::table("debug"), eosio::contract("game")]] debug_table {
        std::vector<checksum256> pseudo_queue;
        std::vector<uint64_t> pseudo_prng;
    };

    using debug_singleton = eosio::singleton<"debug"_n, debug_table>;

    CONTRACT_ACTION(pushprng)
    void push_to_prng(uint64_t next_random) { global_debug.pseudo_prng.push_back(next_random); }

    CONTRACT_ACTION(pushnrandom)
    void push_next_random(checksum256 next_random) { global_debug.pseudo_queue.push_back(next_random); }

  protected:
    const debug_table& get_debug() const { return global_debug; }
  private:
    mutable debug_table global_debug;
#endif
};

const asset game::zero_asset = asset(0, game::core_symbol);

} // namespace game_sdk
