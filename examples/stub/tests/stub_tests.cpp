#include <game_tester/game_tester.hpp>

#include "contracts.hpp"

namespace testing {

class stub_tester: public game_tester {
public:
    static const name game_name;
    static constexpr uint16_t stub_game_action_type { 0u };

public:
    stub_tester() {
        create_account(game_name);

        game_params_type game_params = {};
        deploy_game<stub_game>(game_name, game_params);
    }
};

const name stub_tester::game_name = N(stubgame);


BOOST_AUTO_TEST_SUITE(stub_tests)

BOOST_FIXTURE_TEST_CASE(new_session_test, stub_tester) try {
    auto player_name = N(player);

    create_player(player_name);
    link_game(player_name, game_name);

    transfer(N(eosio), player_name, STRSYM("10.0000"));

    auto player_bet = STRSYM("5.0000");
    auto ses_id = new_game_session(game_name, player_name, casino_id, player_bet);

    auto session = get_game_session(game_name, ses_id);

    BOOST_REQUIRE_EQUAL(session["req_id"].as<uint64_t>(), ses_id);
    BOOST_REQUIRE_EQUAL(session["casino_id"].as<uint64_t>(), casino_id);
    BOOST_REQUIRE_EQUAL(session["ses_seq"].as<uint64_t>(), 0);
    BOOST_REQUIRE_EQUAL(session["player"].as<name>(), player_name);
    BOOST_REQUIRE_EQUAL(session["state"].as<uint32_t>(), 2); // req_action state
    BOOST_REQUIRE_EQUAL(session["deposit"].as<asset>(), player_bet);

} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(full_session_success_test, stub_tester) try {
    auto player_name = N(player);

    create_player(player_name);
    link_game(player_name, game_name);

    transfer(N(eosio), player_name, STRSYM("10.0000"));
    transfer(N(eosio), casino_name, STRSYM("1000.0000"));

    auto casino_balance_before = get_balance(casino_name);
    auto player_balance_before = get_balance(player_name);

    auto player_bet = STRSYM("5.0000");
    auto ses_id = new_game_session(game_name, player_name, casino_id, player_bet);

    BOOST_REQUIRE_EQUAL(get_balance(game_name), player_bet);

    game_action(game_name, ses_id, 0, { 0 });

    auto session = get_game_session(game_name, ses_id);
    BOOST_REQUIRE_EQUAL(session["state"].as<uint32_t>(), 3); // req_signidice_part_1 state

    signidice(game_name, ses_id);

    auto casino_balance_after = get_balance(casino_name);
    auto player_balance_after = get_balance(player_name);

    session = get_game_session(game_name, ses_id);
    BOOST_REQUIRE_EQUAL(session.is_null(), true);
    BOOST_REQUIRE_EQUAL(player_balance_before - player_bet, player_balance_after);
    BOOST_REQUIRE_EQUAL(casino_balance_before + player_bet, casino_balance_after);

} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(session_exiration_test, stub_tester) try {
    auto player_name = N(player);

    create_player(player_name);
    link_game(player_name, game_name);

    transfer(N(eosio), player_name, STRSYM("10.0000"));
    transfer(N(eosio), casino_name, STRSYM("1000.0000"));

    auto player_balance_before = get_balance(player_name);
    auto casino_balance_before = get_balance(casino_name);

    auto player_bet = STRSYM("5.0000");
    auto ses_id = new_game_session(game_name, player_name, casino_id, player_bet);

    BOOST_REQUIRE_EQUAL(get_balance(game_name), player_bet);

    BOOST_REQUIRE_EQUAL(push_action(game_name, N(close), { platform_name, N(active) }, mvo()
        ("req_id", ses_id)
    ), wasm_assert_msg("session isn't expired, only expired session can be closed"));

    produce_block(fc::seconds(game_session_ttl + 1));

    close_session(game_name, ses_id);

    auto session = get_game_session(game_name, ses_id);
    BOOST_REQUIRE_EQUAL(session.is_null(), true);

    auto player_balance_after = get_balance(player_name);
    auto casino_balance_after = get_balance(casino_name);

    BOOST_REQUIRE_EQUAL(player_balance_before, player_balance_after + player_bet);
    BOOST_REQUIRE_EQUAL(casino_balance_before, casino_balance_after - player_bet);

} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(new_session_bad_auth_test, stub_tester) try {
    auto player_name = N(player);

    create_player(player_name);
    link_game(player_name, game_name);

    transfer(N(eosio), player_name, STRSYM("10.0000"));
    transfer(N(eosio), casino_name, STRSYM("1000.0000"));

    auto ses_id = 0u;
    auto player_bet = STRSYM("5.0000");
    transfer(player_name, game_name, player_bet, std::to_string(ses_id));

    BOOST_TEST_REQUIRE(push_action(game_name, N(newgame), { player_name, N(game) }, { casino_name, N(active) }, mvo()
        ("req_id", ses_id)
        ("casino_id", casino_id)
    ).find("but does not have signatures for it") != std::string::npos);

    BOOST_REQUIRE_EQUAL(push_action(game_name, N(newgame), { casino_name, N(active) }, { casino_name, N(active) }, mvo()
        ("req_id", ses_id)
        ("casino_id", casino_id)
    ), "missing authority of player/game");

} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(game_action_bad_auth_test, stub_tester) try {
    auto player_name = N(player);

    create_player(player_name);
    link_game(player_name, game_name);

    transfer(N(eosio), player_name, STRSYM("10.0000"));
    transfer(N(eosio), casino_name, STRSYM("1000.0000"));

    auto player_bet = STRSYM("5.0000");
    auto ses_id = new_game_session(game_name, player_name, casino_id, player_bet);

    BOOST_TEST_REQUIRE(push_action(game_name, N(gameaction), { player_name, N(game) }, { casino_name, N(active) }, mvo()
        ("req_id", ses_id)
        ("type", 0)
        ("params", std::vector<uint32_t> { 30 })
    ).find("but does not have signatures for it") != std::string::npos);

    BOOST_REQUIRE_EQUAL(push_action(game_name, N(gameaction), { casino_name, N(active) }, { casino_name, N(active) }, mvo()
        ("req_id", ses_id)
        ("type", 0)
        ("params", std::vector<uint32_t> { 30 })
    ), "missing authority of player/game");

} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(deposit_bad_sender_test, stub_tester) try {
    auto player_name = N(player);

    create_player(player_name);
    link_game(player_name, game_name);

    transfer(N(eosio), player_name, STRSYM("10.0000"));
    transfer(N(eosio), casino_name, STRSYM("1000.0000"));

    auto player_bet = STRSYM("5.0000");
    auto ses_id = new_game_session(game_name, player_name, casino_id, player_bet);

    BOOST_REQUIRE_EQUAL(transfer(casino_name, game_name, player_bet, std::to_string(ses_id)),
        wasm_assert_msg("only player can deposit")
    );

} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(deposit_bad_state_test, stub_tester) try {
    auto player_name = N(player);

    create_player(player_name);
    link_game(player_name, game_name);

    transfer(N(eosio), player_name, STRSYM("10.0000"));
    transfer(N(eosio), casino_name, STRSYM("1000.0000"));

    auto player_bet = STRSYM("5.0000");
    auto ses_id = new_game_session(game_name, player_name, casino_id, player_bet);

    game_action(game_name, ses_id, 0, { 30 });

    BOOST_REQUIRE_EQUAL(transfer(player_name, game_name, player_bet, std::to_string(ses_id)),
        wasm_assert_msg("state should be 'req_deposit'")
    );

    auto digest = get_game_session(game_name, ses_id)["digest"].as<sha256>();
    auto sign_1 = rsa_sign(rsa_keys.at(platform_name), digest);
    BOOST_REQUIRE_EQUAL(push_action(game_name, N(sgdicefirst), { platform_name, N(signidice) }, mvo()
        ("req_id", ses_id)
        ("sign", sign_1)
    ), success());

    BOOST_REQUIRE_EQUAL(transfer(player_name, game_name, player_bet, std::to_string(ses_id)),
        wasm_assert_msg("state should be 'req_deposit'")
    );
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(game_action_bad_state_test, stub_tester) try {
    auto player_name = N(player);

    create_player(player_name);
    link_game(player_name, game_name);

    transfer(N(eosio), player_name, STRSYM("10.0000"));
    transfer(N(eosio), casino_name, STRSYM("1000.0000"));

    auto player_bet = STRSYM("5.0000");
    auto ses_id = new_game_session(game_name, player_name, casino_id, player_bet);

    game_action(game_name, ses_id, 0, { 30 });

    BOOST_REQUIRE_EQUAL(push_action(game_name, N(gameaction), { player_name, N(game) }, { platform_name, N(active) }, mvo()
        ("req_id", ses_id)
        ("type", 0)
        ("params", std::vector<uint32_t> { 30 })
    ), wasm_assert_msg("state should be 'req_deposit' or 'req_action'"));

    auto digest = get_game_session(game_name, ses_id)["digest"].as<sha256>();
    auto sign_1 = rsa_sign(rsa_keys.at(platform_name), digest);
    BOOST_REQUIRE_EQUAL(push_action(game_name, N(sgdicefirst), { platform_name, N(signidice) }, mvo()
        ("req_id", ses_id)
        ("sign", sign_1)
    ), success());

    BOOST_REQUIRE_EQUAL(push_action(game_name, N(gameaction), { player_name, N(game) }, { platform_name, N(active) }, mvo()
        ("req_id", ses_id)
        ("type", 0)
        ("params", std::vector<uint32_t> { 30 })
    ), wasm_assert_msg("state should be 'req_deposit' or 'req_action'"));

} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(signidice_1_bad_state_test, stub_tester) try {
    auto player_name = N(player);

    create_player(player_name);
    link_game(player_name, game_name);

    transfer(N(eosio), player_name, STRSYM("10.0000"));
    transfer(N(eosio), casino_name, STRSYM("1000.0000"));

    auto player_bet = STRSYM("5.0000");
    auto ses_id = new_game_session(game_name, player_name, casino_id, player_bet);

    auto digest = get_game_session(game_name, ses_id)["digest"].as<sha256>();
    auto sign_1 = rsa_sign(rsa_keys.at(platform_name), digest);
    BOOST_REQUIRE_EQUAL(push_action(game_name, N(sgdicefirst), { platform_name, N(signidice) }, mvo()
        ("req_id", ses_id)
        ("sign", sign_1)
    ), wasm_assert_msg("state should be 'req_signidice_part_1'"));

    game_action(game_name, ses_id, 0, { 30 });

    BOOST_REQUIRE_EQUAL(push_action(game_name, N(sgdicefirst), { platform_name, N(signidice) }, mvo()
        ("req_id", ses_id)
        ("sign", sign_1)
    ), success());

    BOOST_REQUIRE_EQUAL(push_action(game_name, N(sgdicefirst), { platform_name, N(signidice) }, mvo()
        ("req_id", ses_id)
        ("sign", sign_1)
    ), wasm_assert_msg("state should be 'req_signidice_part_1'"));

} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(signidice_2_bad_state_test, stub_tester) try {
    auto player_name = N(player);

    create_player(player_name);
    link_game(player_name, game_name);

    transfer(N(eosio), player_name, STRSYM("10.0000"));
    transfer(N(eosio), casino_name, STRSYM("1000.0000"));

    auto player_bet = STRSYM("5.0000");
    auto ses_id = new_game_session(game_name, player_name, casino_id, player_bet);

    auto digest = get_game_session(game_name, ses_id)["digest"].as<sha256>();
    auto sign = rsa_sign(rsa_keys.at(platform_name), digest);
    BOOST_REQUIRE_EQUAL(push_action(game_name, N(sgdicesecond), { casino_name, N(signidice) }, mvo()
        ("req_id", ses_id)
        ("sign", sign)
    ), wasm_assert_msg("state should be 'req_signidice_part_2'"));

    game_action(game_name, ses_id, 0, { 30 });

    BOOST_REQUIRE_EQUAL(push_action(game_name, N(sgdicesecond), { casino_name, N(signidice) }, mvo()
        ("req_id", ses_id)
        ("sign", sign)
    ), wasm_assert_msg("state should be 'req_signidice_part_2'"));

} FC_LOG_AND_RETHROW()
BOOST_AUTO_TEST_SUITE_END()

} // namespace testing

