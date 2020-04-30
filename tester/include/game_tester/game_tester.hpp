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

class game_tester : public TESTER {
  public:
    constexpr static uint32_t game_session_ttl = 60 * 10;
    constexpr static uint64_t casino_id = 0u;

  public:
    game_tester() {
        produce_blocks(2);
        RAND_set_rand_method(random_mock::RAND_stdlib());

        create_accounts({N(eosio.token), platform_name, events_name, casino_name});

        produce_blocks(100);
        deploy_contract<contracts::system::token>(N(eosio.token));

        symbol core_symbol = symbol{CORE_SYM};
        create_currency(N(eosio.token), config::system_account_name, asset(100000000000000, core_symbol));
        issue(config::system_account_name, asset(1672708210000, core_symbol));

        deploy_contract<contracts::platform::platfrm>(platform_name);
        deploy_contract<contracts::platform::events>(events_name);
        deploy_contract<contracts::platform::casino>(casino_name);

        produce_blocks(2);

        auto pl_key = new_rsa_keys();
        rsa_keys.insert(std::make_pair(platform_name, std::move(pl_key)));
        base_tester::push_action(platform_name,
                                 N(setrsakey),
                                 platform_name,
                                 mvo()("rsa_pubkey", rsa_pem_pubkey(rsa_keys.at(platform_name))));

        base_tester::push_action(events_name, N(setplatform), events_name, mvo()("platform_name", platform_name));
        base_tester::push_action(casino_name, N(setplatform), casino_name, mvo()("platform_name", platform_name));

        base_tester::push_action(
            platform_name, N(addcas), platform_name, mvo()("contract", casino_name)("meta", bytes()));

        rsa_keys.insert(std::make_pair(casino_name, new_rsa_keys()));
        base_tester::push_action(platform_name,
                                 N(setrsacas),
                                 platform_name,
                                 mvo()("id", casino_id)("rsa_pubkey", rsa_pem_pubkey(rsa_keys.at(casino_name))));

        // create permissions for signidice
        set_authority(platform_name, N(signidice), {get_public_key(platform_name, "signidice")}, N(active));
        set_authority(casino_name, N(signidice), {get_public_key(casino_name, "signidice")}, N(active));
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

    action_result issue(const name& to, const asset& amount) {
        return push_action(N(eosio.token),
                           N(issue),
                           config::system_account_name,
                           mutable_variant_object()("to", to)("quantity", amount)("memo", ""));
    }

    action_result transfer(const name& from, const name& to, const asset& amount, const std::string& memo = "") {
        return push_action(N(eosio.token),
                           N(transfer),
                           from,
                           mutable_variant_object()("from", from)("to", to)("quantity", amount)("memo", memo));
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

        return base_tester::push_action(std::move(act), actor);
    }

    std::optional<std::vector<fc::variant>> get_events(const events_id event_id)
    {
        if (auto it = _events.find(event_id); it != _events.end()) {
            return { it->second };
        } else {
            return std::nullopt;
        }
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

        base_tester::push_action(
            game_name,
            N(init),
            game_name,
            mvo()("platform", platform_name)("events", events_name)("session_ttl", game_session_ttl));

        base_tester::push_action(platform_name,
                                 N(addgame),
                                 platform_name,
                                 mvo()("contract", game_name)("params_cnt", params.size())("meta", bytes()));

        base_tester::push_action(casino_name, N(addgame), casino_name, mvo()("game_id", game_id)("params", params));

        // allow platform to make signidice action in this game
        link_authority(platform_name, game_name, N(signidice), N(sgdicefirst));

        // allow casino to make signidice action in this game
        link_authority(casino_name, game_name, N(signidice), N(sgdicesecond));

        return game_id;
    }

    void create_player(name player_name) {
        create_account(player_name);
        // create gaming permission (allow platform perform game action for
        // player account)
        set_authority(player_name, N(game), {{platform_name, N(active)}}, N(active));
    }

    void link_game(name player_name, name game_name) {
        // allow player to game to 'game_name' game
        link_authority(player_name, game_name, N(game));
    }

    uint64_t get_next_game_id() {
        return 0;
        // TODO fix, invalid ABI:
        // https://github.com/DaoCasino/platform-contracts/blob/528efa6b06c0f85325d95ab0a6172ab9a71d4268/contracts/platform/include/platform/platform.hpp#L19
        // vector<char> data = get_row_by_account(platform_name, platform_name,
        // N(global), N(global) ); return data.empty() ? 0 :
        // abi_ser[platform_name].binary_to_variant("global_row", data,
        // abi_serializer_max_time)["games_seq"].as<uint64_t>();
    }

    uint64_t new_game_session(name game_name, name player, uint64_t casino_id, asset deposit) {
        static uint64_t ses_id{0u};

        BOOST_REQUIRE_EQUAL(
            push_action(N(eosio.token),
                        N(transfer),
                        player,
                        mvo()("from", player)("to", game_name)("quantity", deposit)("memo", std::to_string(ses_id))),
            success());

        BOOST_REQUIRE_EQUAL(push_action(game_name,
                                        N(newgame),
                                        {player, N(game)},
                                        {platform_name, N(active)},
                                        mvo()("req_id", ses_id)("casino_id", casino_id)),
                            success());

        return ses_id++;
    }

    void game_action(name game_name,
                     uint64_t ses_id,
                     uint16_t action_type,
                     std::vector<param_t> params,
                     asset deposit = STRSYM("0")) {
        auto player = get_game_session(game_name, ses_id)["player"].as<name>();

        if (deposit.get_amount() > 0) {
            transfer(player, game_name, deposit, std::to_string(ses_id));
        }

        BOOST_REQUIRE_EQUAL(push_action(game_name,
                                        N(gameaction),
                                        {player, N(game)},
                                        {platform_name, N(active)},
                                        mvo()("req_id", ses_id)("type", action_type)("params", params)),
                            success());
    }

    void signidice(name game_name, uint64_t ses_id) {
        auto digest = get_game_session(game_name, ses_id)["digest"].as<sha256>();

        auto sign_1 = rsa_sign(rsa_keys.at(platform_name), digest);
        BOOST_REQUIRE_EQUAL(
            push_action(
                game_name, N(sgdicefirst), {platform_name, N(signidice)}, mvo()("req_id", ses_id)("sign", sign_1)),
            success());

        BOOST_REQUIRE_EQUAL(get_game_session(game_name, ses_id)["digest"].as<sha256>(), sha256::hash(sign_1));

        digest = get_game_session(game_name, ses_id)["digest"].as<sha256>();

        auto sign_2 = rsa_sign(rsa_keys.at(casino_name), digest);
        BOOST_REQUIRE_EQUAL(
            push_action(
                game_name, N(sgdicesecond), {casino_name, N(signidice)}, mvo()("req_id", ses_id)("sign", sign_2)),
            success());
    }

    void close_session(name game_name, uint64_t ses_id) {
        BOOST_REQUIRE_EQUAL(push_action(game_name, N(close), {platform_name, N(active)}, mvo()("req_id", ses_id)),
                            success());
    }

    asset get_balance(name account) const { return get_currency_balance(N(eosio.token), symbol(CORE_SYM), account); }

    fc::variant get_game_session(name game_name, uint64_t ses_id) {
        vector<char> data = get_row_by_account(game_name, game_name, N(session), ses_id);
        return data.empty() ? fc::variant()
                            : abi_ser[game_name].binary_to_variant("session_row", data, abi_serializer_max_time);
    }

  private:
    void handle_action_data(
            bytes&& action_data,
            const int event_type,
            fc::path event_abi
    ) {
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

        const abi_def event_abi_def = fc::json::from_file(event_abi).as<abi_def>();
        abi_serializer abi_ser;
        abi_ser.set_abi(event_abi_def, abi_serializer_max_time);

        fc::variant event_struct;
        if (!action_data.empty()) {
            event_struct = abi_ser.binary_to_variant(
                "event_data",
                action_data,
                abi_serializer_max_time
            );
        }

        if (auto it = _events.find(static_cast<events_id>(event_type)); it != _events.end()) {
            it->second.emplace_back(std::move(event_struct));
        } else {
            _events[static_cast<events_id>(event_type)] = {event_struct, };
        }
    }

    void handle_transaction_ptr(const transaction_trace_ptr& transaction_trace) {
        _events.clear();

        const fc::path cpath = fc::canonical(contracts::events_struct::folder());
        const abi_def events_abi = fc::json::from_file(contracts::platform::events::abi_path()).as<abi_def>();

        abi_serializer abi_ser;
        abi_ser.set_abi(events_abi, abi_serializer_max_time);

        std::for_each(
            transaction_trace->action_traces.begin(),
            transaction_trace->action_traces.end(),
            [&](const auto& action_trace) {
                if (action_trace.receiver != "events" || action_trace.name != "send")
                    return;

                const fc::variant send_action = abi_ser.binary_to_variant(
                    "send",
                    action_trace.act.data,
                    abi_serializer_max_time
                );

                handle_action_data(
                    send_action["data"].as<bytes>(),
                    send_action["event_type"].as<int>(),
                    cpath
                );
            }
        );
    }

  public:
    std::map<account_name, abi_serializer> abi_ser;
    std::map<account_name, RSA_ptr> rsa_keys;

  private:
    std::unordered_map<events_id, std::vector<fc::variant>> _events;
};

} // namespace testing

void translate_fc_exception(const fc::exception& e) {
    std::cerr << "\033[33m" << e.to_detail_string() << "\033[0m" << std::endl;
    BOOST_TEST_FAIL("Caught Unexpected Exception");
}

boost::unit_test::test_suite* init_unit_test_suite(int argc, char* argv[]) {
#ifdef IS_DEBUG
    bool is_verbose = true;
#else
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
#endif

    fc::logger::get(DEFAULT_LOGGER).set_log_level(is_verbose ? fc::log_level::debug : fc::log_level::off);

    // Register fc::exception translator
    boost::unit_test::unit_test_monitor.template register_exception_translator<fc::exception>(&translate_fc_exception);

    const auto seed = time(NULL);
    std::srand(seed);

    std::cout << "Random number generator seeded to " << seed << std::endl;

    return nullptr;
}
