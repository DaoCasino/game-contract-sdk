#pragma once

#include <Runtime/Runtime.h>

#include <boost/test/included/unit_test.hpp>
#include <boost/test/unit_test.hpp>

#include <eosio/chain/abi_serializer.hpp>
#include <eosio/chain/exceptions.hpp>
#include <eosio/chain/resource_limits.hpp>
#include <eosio/testing/tester.hpp>

#include <game_tester/contracts.hpp>
#include <game_tester/test_symbol.hpp>

#include <openssl/bio.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <openssl/x509.h>

#include <fc/log/logger.hpp>
#include <fc/variant_object.hpp>

#include <fstream>
#include <stdlib.h>

#include "random_mock.hpp"

#define BOOST_TEST_STATIC_LINK

#define REQUIRE_EQUAL_ITERABLES(left, right)                                                                           \
    {                                                                                                                  \
        auto l_itr = left.begin();                                                                                     \
        auto r_itr = right.begin();                                                                                    \
        for (; l_itr != left.end() && r_itr != right.end(); ++l_itr, ++r_itr) {                                        \
            BOOST_REQUIRE_EQUAL(*l_itr == *r_itr, true);                                                               \
        }                                                                                                              \
        BOOST_REQUIRE_EQUAL(l_itr == left.end(), true);                                                                \
        BOOST_REQUIRE_EQUAL(r_itr == right.end(), true);                                                               \
    }

#ifndef TESTER
#ifdef NON_VALIDATING_TEST
#define TESTER tester
#else
#define TESTER validating_tester
#endif
#endif

// just sugar
#define STRSYM(str) core_sym::from_string(str)
#define ASSET(str) sym::from_string(str)

using namespace eosio::chain;
using namespace eosio::testing;
using namespace fc;

using mvo = fc::mutable_variant_object;
using bytes = std::vector<char>;
using game_params_type = std::vector<std::pair<uint16_t, uint64_t>>;
using param_t = uint64_t;

using RSA_ptr = std::unique_ptr<RSA, decltype(&::RSA_free)>;
using BIO_ptr = std::unique_ptr<BIO, decltype(&::BIO_free)>;
using EVP_PKEY_ptr = std::unique_ptr<EVP_PKEY, decltype(&::EVP_PKEY_free)>;

namespace testing {

enum struct events_id {
    game_started = 0,
    action_request = 1,
    signidice_part_1_request = 2,
    signidice_part_2_request = 3,
    game_finished = 4,
    game_failed = 5,
    game_message = 6
};

// just random account to sign actions that doesn't require auth(e.g signidice)
static const eosio::chain::name service_name = N(service);

class game_tester : public TESTER {
  public:
    constexpr static uint32_t game_session_ttl = 60 * 10;
    constexpr static uint64_t casino_id = 0u;

  public:
    game_tester() {
        produce_blocks(2);
        RAND_set_rand_method(random_mock::RAND_stdlib());
        create_accounts({platform_name, events_name, casino_name, service_name});

        produce_blocks(100);

        deploy_contract<contracts::platform::platfrm>(platform_name);
        deploy_contract<contracts::platform::events>(events_name);
        deploy_contract<contracts::platform::casino>(casino_name);

        produce_blocks(2);

        auto pl_key = new_rsa_keys();
        rsa_keys.insert(std::make_pair(platform_name, std::move(pl_key)));
        push_action(platform_name,
                    N(setrsakey),
                    platform_name,
                    mvo()("rsa_pubkey", rsa_pem_pubkey(rsa_keys.at(platform_name))));

        push_action(events_name, N(setplatform), events_name, mvo()("platform_name", platform_name));
        push_action(casino_name, N(setplatform), casino_name, mvo()("platform_name", platform_name));

        push_action(platform_name, N(addcas), platform_name, mvo()("contract", casino_name)("meta", bytes()));

        rsa_keys.insert(std::make_pair(casino_name, new_rsa_keys()));
        push_action(platform_name,
                    N(setrsacas),
                    platform_name,
                    mvo()("id", casino_id)("rsa_pubkey", rsa_pem_pubkey(rsa_keys.at(casino_name))));

        const std::string core_token = CORE_SYM_NAME;
        const name eosio_token_name = N(eosio.token);
        allow_token(core_token, CORE_SYM_PRECISION, eosio_token_name);

        set_authority(platform_name, N(gameaction), {get_public_key(platform_name, "gameaction")}, N(active));

        const auto platform_abi_def = fc::json::from_file(contracts::platform::events::abi_path()).as<abi_def>();
        _platform_abi_ser = abi_serializer(platform_abi_def, abi_serializer_max_time);
    }

    template <typename Contract> void deploy_contract(account_name account) {
        set_code(account, Contract::wasm());
        set_abi(account, Contract::abi().data());

        abi_def abi;
        abi_serializer abi_s;
        const auto& accnt = control->db().get<account_object, by_name>(account);
        abi_serializer::to_abi(accnt.abi, abi);
        abi_s.set_abi(abi, abi_serializer_max_time);
        abi_ser.insert({account, abi_s});
    }

    action_result create_currency(const name& contract, const name& manager, const asset& maxsupply) {
        auto act = mutable_variant_object()("issuer", manager)("maximum_supply", maxsupply);

        return push_action(contract, N(create), contract, act);
    }

    action_result issue(const name& to, const asset& amount, const name& contract) {
        return push_action(contract,
                           N(issue),
                           config::system_account_name,
                           mutable_variant_object()("to", to)("quantity", amount)("memo", ""));
    }

    action_result transfer(const name& from, const name& to, const asset& amount, const std::string& memo = "") {
        // clang-format off
        return push_action(
            get_token_contract(amount.get_symbol()),
            N(transfer),
            from,
            mutable_variant_object()
               ("from", from)
               ("to", to)
               ("quantity", amount)
               ("memo", memo)
        );
        // clang-format on
    }

    std::optional<std::vector<fc::variant>> get_events(const events_id event_id) {
        if (auto it = _events.find(event_id); it != _events.end()) {
            return {it->second};
        } else {
            return std::nullopt;
        }
    }

    action_result push_action(const action_name& contract,
                              const action_name& name,
                              const action_name& actor,
                              const variant_object& data) {
        string action_type_name = abi_ser[contract].get_action_type(name);

        action act;
        act.account = contract;
        act.name = name;
        act.data = abi_ser[contract].variant_to_binary(action_type_name, data, abi_serializer_max_time);

        return push_action(std::move(act), actor);
    }

    const std::unordered_map<events_id, std::vector<fc::variant>>& get_events_map() const { return _events; }

    action_result push_action(action&& act, uint64_t authorizer) {
        signed_transaction trx;
        if (authorizer) {
            act.authorization = vector<permission_level>{{account_name(authorizer), config::active_name}};
        }
        trx.actions.emplace_back(std::move(act));
        set_transaction_headers(trx);
        if (authorizer) {
            trx.sign(get_private_key(account_name(authorizer), "active"), control->get_chain_id());
        }

        try {
            handle_transaction_ptr(push_transaction(trx));
        } catch (const fc::exception& ex) {
            edump((ex.to_detail_string()));
            return error(ex.top_message()); // top_message() is assumed by many tests; otherwise they fail
                                            // return error(ex.to_detail_string());
        }
        produce_block();
        BOOST_REQUIRE_EQUAL(true, chain_has_transaction(trx.id()));
        return success();
    }

    action_result push_action(const action_name& contract,
                              const action_name& name,
                              const permission_level& auth,
                              const permission_level& key,
                              const variant_object& data) {
        using namespace eosio;
        using namespace eosio::chain;

        string action_type_name = abi_ser[contract].get_action_type(name);

        action act;
        act.account = contract;
        act.name = name;
        act.data = abi_ser[contract].variant_to_binary(action_type_name, data, abi_serializer_max_time);

        act.authorization.push_back(auth);

        signed_transaction trx;
        trx.actions.emplace_back(std::move(act));
        set_transaction_headers(trx);
        trx.sign(get_private_key(key.actor, key.permission.to_string()), control->get_chain_id());

        try {
            handle_transaction_ptr(push_transaction(trx));
        } catch (const fc::exception& ex) {
            edump((ex.to_detail_string()));
            return error(ex.top_message()); // top_message() is assumed by many
                                            // tests; otherwise they fail
        }
        produce_block();
        BOOST_REQUIRE_EQUAL(true, chain_has_transaction(trx.id()));
        return success();
    }

    action_result push_action(const action_name& contract,
                              const action_name& name,
                              const permission_level& auth,
                              const variant_object& data) {
        return push_action(contract, name, auth, auth, data);
    }

    RSA_ptr new_rsa_keys() {
        static bool init = true;
        if (init) {
            ERR_load_crypto_strings();
            init = false;
        }

        auto rsa = RSA_ptr(RSA_generate_key(2048, 65537, NULL, NULL), ::RSA_free);
        return rsa;
    }

    std::string rsa_sign(const RSA_ptr& rsa, const sha256& digest) {
        bytes signature;
        signature.resize(RSA_size(rsa.get()));
        uint32_t len;
        RSA_sign(NID_sha256, (uint8_t*)digest.data(), 32, (unsigned char*)signature.data(), &len, rsa.get());
        return fc::base64_encode(signature.data(), signature.size());
    }

    std::string rsa_pem_pubkey(const RSA_ptr& rsa) {
        auto pkey = EVP_PKEY_ptr(EVP_PKEY_new(), ::EVP_PKEY_free);
        EVP_PKEY_set1_RSA(pkey.get(), rsa.get());

        auto mem = BIO_ptr(BIO_new(BIO_s_mem()), ::BIO_free);
        PEM_write_bio_PUBKEY(mem.get(), pkey.get());

        BUF_MEM* bio_buf;
        BIO_get_mem_ptr(mem.get(), &bio_buf);

        std::stringstream ss({bio_buf->data, bio_buf->length});
        std::string pem;

        while (ss) {
            std::string tmp;
            std::getline(ss, tmp);
            if (tmp[0] != '-') {
                pem += tmp;
            }
        }

        return pem;
    }

    template <typename Contract> uint64_t deploy_game(name game_name, game_params_type params) {
        auto game_id = get_next_game_id();

        deploy_contract<Contract>(game_name);

        push_action(game_name,
                    N(init),
                    game_name,
                    mvo()("platform", platform_name)("events", events_name)("session_ttl", game_session_ttl));

        push_action(platform_name,
                    N(addgame),
                    platform_name,
                    mvo()("contract", game_name)("params_cnt", params.size())("meta", bytes()));

        push_action(casino_name, N(addgame), casino_name, mvo()("game_id", game_id)("params", params));

        // allow platform to make newgame action in this game
        link_authority(platform_name, game_name, N(gameaction), N(newgame));
        link_authority(platform_name, game_name, N(gameaction), N(newgameaffl));
        link_authority(platform_name, game_name, N(gameaction), N(newgamebon));
        link_authority(platform_name, game_name, N(gameaction), N(depositbon));
        link_authority(platform_name, game_name, N(gameaction), N(gameaction));
        link_authority(platform_name, game_name, N(gameaction), N(close));

        return game_id;
    }

    void create_player(name player_name) {
        create_account(player_name);
        // create gaming permission (allow platform perform game action for
        // player account)
        set_authority(player_name, N(game), {{platform_name, N(active)}}, N(active));
    }

    void link_game(name player_name, name game_name) {
        // allow player to play 'game_name' game
        link_authority(player_name, game_name, N(game));
    }

    uint64_t get_next_game_id() {
        vector<char> data = get_row_by_account(platform_name, platform_name, N(global), N(global) );
        return data.empty()
            ? 0
            : abi_ser[platform_name]
                .binary_to_variant("global_row", data, abi_serializer_max_time)["games_seq"]
                .as<uint64_t>();
    }

    uint64_t new_game_session(name game_name,
                              name player,
                              uint64_t casino_id,
                              asset deposit,
                              const action_result& result = success()) {
        static uint64_t ses_id{0u};

        BOOST_REQUIRE_EQUAL(
            push_action(get_token_contract(deposit.get_symbol()),
                        N(transfer),
                        player,
                        mvo()("from", player)("to", game_name)("quantity", deposit)("memo", std::to_string(ses_id))),
            success());

        // format-clang off
        BOOST_REQUIRE_EQUAL(
            push_action(
                game_name,
                N(newgame),
                {platform_name, N(gameaction)},
                mvo()("req_id", ses_id)("casino_id", casino_id)
            ), result);
        // format-clang on

        return ses_id++;
    }

    uint64_t new_game_session(name game_name,
                              name player,
                              uint64_t casino_id,
                              asset real,
                              asset bonus,
                              const action_result& result = success()) {
        BOOST_TEST((real.get_amount() > 0 || bonus.get_amount() > 0), "real or bonus should be greater than 0");

        static uint64_t ses_id{0u};

        if (real.get_amount() > 0) {
            BOOST_REQUIRE_EQUAL(
                push_action(get_token_contract(real.get_symbol()),
                            N(transfer),
                            player,
                            mvo()("from", player)("to", game_name)("quantity", real)("memo", std::to_string(ses_id))),
                success());
        }

        if (bonus.get_amount() > 0) {
            BOOST_REQUIRE_EQUAL(
                push_action(
                    game_name,
                    N(newgamebon),
                    {platform_name, N(gameaction)},
                    mvo()("req_id", ses_id)("casino_id", casino_id)("from", player)("quantity", bonus)("affiliate_id", "")
                ), result);

        } else {
            BOOST_REQUIRE_EQUAL(
                push_action(
                    game_name,
                    N(newgame),
                    {platform_name, N(gameaction)},
                    mvo()("req_id", ses_id)("casino_id", casino_id)
                ), result);
        }
        // format-clang on

        return ses_id++;
    }

    void game_action(name game_name,
                     uint64_t ses_id,
                     uint16_t action_type,
                     std::vector<param_t> params,
                     asset deposit = STRSYM("0"),
                     const action_result& result = success()) {
        auto player = get_game_session(game_name, ses_id)["player"].as<name>();

        if (deposit.get_amount() > 0) {
            transfer(player, game_name, deposit, std::to_string(ses_id));
        }

        // format-clang off
        BOOST_REQUIRE_EQUAL(
            push_action(
                game_name,
                N(gameaction),
                {platform_name, N(gameaction)},
                mvo()("req_id", ses_id)("type", action_type)("params", params)
            ), result);
        // format-clang on
    }

    void game_action(name game_name,
                     uint64_t ses_id,
                     uint16_t action_type,
                     std::vector<param_t> params,
                     asset real,
                     asset bonus,
                     const action_result& result = success()) {
        auto player = get_game_session(game_name, ses_id)["player"].as<name>();

        if (real.get_amount() > 0) {
            transfer(player, game_name, real, std::to_string(ses_id));
        }

        if (bonus.get_amount() > 0) {
            BOOST_REQUIRE_EQUAL(
                push_action(game_name, N(depositbon), {platform_name, N(gameaction)}, mvo()
                    ("req_id", ses_id)
                    ("from", player)
                    ("quantity", bonus)
                ),
                success()
            );
        }
        // format-clang off
        BOOST_REQUIRE_EQUAL(
            push_action(
                game_name,
                N(gameaction),
                {platform_name, N(gameaction)},
                mvo()("req_id", ses_id)("type", action_type)("params", params)
            ), result);
        // format-clang on
    }

#ifdef IS_DEBUG
    void push_next_random(name game_name, const sha256& next_random) {
        BOOST_REQUIRE_EQUAL(
            push_action(game_name, N(pushnrandom), {platform_name, N(active)}, mvo()("next_random", next_random)),
            success());
    }

    void push_to_prng(name game_name, uint64_t next_random) {
        BOOST_REQUIRE_EQUAL(
            push_action(game_name, N(pushprng), {platform_name, N(active)}, mvo()("next_random", next_random)),
            success());
    }
#endif

    void signidice(name game_name, uint64_t ses_id) {
        auto digest = get_game_session(game_name, ses_id)["digest"].as<sha256>();

        auto sign_1 = rsa_sign(rsa_keys.at(platform_name), digest);
        // clang-format off
        BOOST_REQUIRE_EQUAL(
            push_action(
                game_name,
                N(sgdicefirst),
                {service_name, N(active)}, //{platform_name, N(signidice)},
                mvo()
                    ("req_id", ses_id)
                    ("sign", sign_1)
            ), success());
        // clang-format on

        BOOST_REQUIRE_EQUAL(get_game_session(game_name, ses_id)["digest"].as<sha256>(), sha256::hash(sign_1));

        digest = get_game_session(game_name, ses_id)["digest"].as<sha256>();

        auto sign_2 = rsa_sign(rsa_keys.at(casino_name), digest);
        // clang-format off
        BOOST_REQUIRE_EQUAL(
            push_action(
                game_name,
                N(sgdicesecond),
                {service_name, N(active)}, //{platform_name, N(signidice)},
                mvo()
                    ("req_id", ses_id)
                    ("sign", sign_2)
            ), success());
        // clang-format on
    }

    void close_session(name game_name, uint64_t ses_id) {
        // clang-format off
        BOOST_REQUIRE_EQUAL(
            push_action(
                game_name,
                N(close),
                {platform_name, N(active)},
                mvo()
                    ("req_id", ses_id)
            ), success());
        // clang-format on
    }

    asset get_balance(name account, symbol sym = symbol{CORE_SYM}) const {
        const auto contract = get_token_contract(sym);
        return get_currency_balance(contract, sym, account); 
    }

    asset get_bonus_balance(name account) {
        vector<char> data = get_row_by_account(casino_name, casino_name, N(bonusbalance), account);
        return data.empty() ? asset(0, symbol{CORE_SYM}) :
            abi_ser[casino_name].binary_to_variant("bonus_balance_row", data, abi_serializer_max_time)["balance"].as<asset>();
    }

    fc::variant get_player_stats(name account) {
        vector<char> data = get_row_by_account(casino_name, casino_name, N(playerstats), account);
        return data.empty() ? fc::variant() : abi_ser[casino_name].binary_to_variant("player_stats_row", data, abi_serializer_max_time);
    }

    fc::variant get_game_session(name game_name, uint64_t ses_id) {
        vector<char> data = get_row_by_account(game_name, game_name, N(session), ses_id);
        return data.empty() ? fc::variant()
                            : abi_ser[game_name].binary_to_variant("session_row", data, abi_serializer_max_time);
    }

    void allow_token(const std::string& token_name, uint8_t precision, name contract) {
        create_account(contract);
        deploy_contract<contracts::system::token>(contract);

        symbol symb = symbol{string_to_symbol_c(precision, token_name.c_str())};
        create_currency( contract, config::system_account_name, asset(100000000000000, symb) );
        issue( config::system_account_name, asset(1672708210000, symb), contract );

        push_action(platform_name, N(addtoken), platform_name, mvo()
            ("token_name", token_name)
            ("contract", contract)
        );
        push_action(casino_name, N(addtoken), casino_name, mvo()
            ("token_name", token_name)
        );
    }

    name get_token_contract(symbol symbol) const {
        vector<char> data = get_row_by_account(platform_name, platform_name, N(token), symbol.to_symbol_code().value);
        const name eosio_token_name = N(eosio.token);
        return data.empty() ? eosio_token_name
            : abi_ser.at(platform_name).binary_to_variant("token_row", data, abi_serializer_max_time)["contract"].as<name>();
    }

    game_params_type extract_game_params(const fc::variant& data) {
        game_params_type params = {};
        for (auto& pair: data.as<vector<fc::variant>>()) {
            params.push_back({pair["first"].as<uint16_t>(), pair["second"].as<uint64_t>()});
        }
        return params;
    }

  private:
    const abi_def& get_events_abi(const events_id event_type) {
        if (_lazy_abi_events.find(event_type) != _lazy_abi_events.end()) {
            return _lazy_abi_events[event_type];
        }

        fc::path event_abi = fc::canonical(events_struct::folder());

        switch (event_type) {
        case events_id::game_started:
            event_abi /= "game_started.abi";
            break;
        case events_id::action_request:
            event_abi /= "action_request.abi";
            break;
        case events_id::signidice_part_1_request:
            event_abi /= "signidice_part_1_request.abi";
            break;
        case events_id::signidice_part_2_request:
            event_abi /= "signidice_part_2_request.abi";
            break;
        case events_id::game_finished:
            event_abi /= "game_finished.abi";
            break;
        case events_id::game_failed:
            event_abi /= "game_failed.abi";
            break;
        case events_id::game_message:
            event_abi /= "game_message.abi";
            break;
        default:
            BOOST_TEST_FAIL("Can't interpret event type");
        }

        _lazy_abi_events[event_type] = fc::json::from_file(event_abi).as<abi_def>();

        return _lazy_abi_events[event_type];
    }

    void handle_action_data(bytes&& action_data, const events_id event_type) {
        fc::variant event_struct;
        if (!action_data.empty()) {
            abi_serializer abi_ser(get_events_abi(event_type), abi_serializer_max_time);

            event_struct = abi_ser.binary_to_variant("event_data", action_data, abi_serializer_max_time);
        }

        if (auto it = _events.find(event_type); it != _events.end()) {
            it->second.emplace_back(std::move(event_struct));
        } else {
            _events[event_type] = {
                event_struct,
            };
        }
    }

    void handle_transaction_ptr(const transaction_trace_ptr& transaction_trace) {
        _events.clear();

        std::for_each(transaction_trace->action_traces.begin(),
                      transaction_trace->action_traces.end(),
                      [&](const auto& action_trace) {
                          if (action_trace.receiver != "events" || action_trace.act.name != "send")
                              return;

                          const fc::variant send_action = _platform_abi_ser.binary_to_variant(
                              "send", action_trace.act.data, abi_serializer_max_time);

                          handle_action_data(send_action["data"].as<bytes>(),
                                             static_cast<events_id>(send_action["event_type"].as<int>()));
                      });
    }

  public:
    std::map<account_name, abi_serializer> abi_ser;
    std::map<account_name, RSA_ptr> rsa_keys;

  private:
    std::unordered_map<events_id, std::vector<fc::variant>> _events;
    std::unordered_map<events_id, abi_def> _lazy_abi_events;
    abi_serializer _platform_abi_ser;
};

} // namespace testing

void translate_fc_exception(const fc::exception& e) {
    std::cerr << "\033[33m" << e.to_detail_string() << "\033[0m" << std::endl;
    BOOST_TEST_FAIL("Caught Unexpected Exception");
}

boost::unit_test::test_suite* init_unit_test_suite(int argc, char* argv[]) {
    bool is_verbose = false;
    // Turn off blockchain logging if no --verbose parameter is not added
    // To have verbose enabled, call "tests/chain_test -- --verbose"
    std::string verbose_arg = "--verbose";
    for (int i = 0; i < argc; i++) {
        if (verbose_arg == argv[i]) {
            is_verbose = true;
            break;
        }
    }

    fc::logger::get(DEFAULT_LOGGER).set_log_level(is_verbose ? fc::log_level::debug : fc::log_level::off);

    // Register fc::exception translator
    boost::unit_test::unit_test_monitor.template register_exception_translator<fc::exception>(&translate_fc_exception);

    const auto seed = time(NULL);
    std::srand(seed);

    std::cout << "Random number generator seeded to " << seed << std::endl;

    return nullptr;
}

namespace std {
    std::ostream& operator<<(std::ostream& os, const game_params_type& params) {
        for (auto pair: params) {
            os << pair.first << ":" << pair.second << " ";
        }
        return os;
    }
} 