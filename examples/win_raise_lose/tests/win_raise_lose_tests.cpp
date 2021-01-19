#include <game_tester/game_tester.hpp>
#include <game_tester/strategy.hpp>

#include "contracts.hpp"
#include <iostream>

namespace testing {

class win_raise_lose_tester : public game_tester {
  public:
    static const name game_name;
    static const symbol kek_symbol;

  public:
    win_raise_lose_tester() {
        create_account(game_name);

        game_params_type game_params = {};
        game_params_type game_params_kek = {{0, 1}};

        const auto game_id = deploy_game<win_raise_lose_game>(game_name, game_params);
        const auto& token = "KEK";
        allow_token(token, 5, N(token.kek));

        BOOST_REQUIRE_EQUAL(
            push_action(
                casino_name, 
                N(setgameparam2), 
                {casino_name, N(active)}, 
                mvo()("game_id", game_id)("token", token)("params", game_params_kek)
            ),
            success()
        );
    }
};

const name win_raise_lose_tester::game_name = N(winraiselose);
const symbol win_raise_lose_tester::kek_symbol = symbol{string_to_symbol_c(5, "KEK")};


BOOST_AUTO_TEST_SUITE(win_raise_lose_tests)

BOOST_FIXTURE_TEST_CASE(new_session_test, win_raise_lose_tester) try {
    auto player_name = N(player);

    create_player(player_name);
    link_game(player_name, game_name);

    transfer(N(eosio), player_name, ASSET("10.00000 KEK"));

    auto ses_id = new_game_session(game_name, player_name, casino_id, ASSET("5.00000 KEK"));

    auto session = get_game_session(game_name, ses_id);

    BOOST_REQUIRE_EQUAL(session["req_id"].as<uint64_t>(), ses_id);
    BOOST_REQUIRE_EQUAL(session["casino_id"].as<uint64_t>(), casino_id);
    BOOST_REQUIRE_EQUAL(session["ses_seq"].as<uint64_t>(), 0);
    BOOST_REQUIRE_EQUAL(session["player"].as<name>(), player_name);
    BOOST_REQUIRE_EQUAL(session["state"].as<uint32_t>(), 2); // req_action state
    BOOST_REQUIRE_EQUAL(session["deposit"].as<asset>(), ASSET("5.00000 KEK"));
}
FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(full_session_success_test, win_raise_lose_tester) try {
    auto player_name = N(player);

    create_player(player_name);
    link_game(player_name, game_name);

    transfer(N(eosio), player_name, ASSET("10.00000 KEK"));
    transfer(N(eosio), casino_name, ASSET("1000.00000 KEK"));

    auto casino_balance_before = get_balance(casino_name, kek_symbol);
    auto player_balance_before = get_balance(player_name, kek_symbol);

    auto ses_id = new_game_session(game_name, player_name, casino_id, ASSET("5.00000 KEK"));

    BOOST_REQUIRE_EQUAL(get_balance(game_name, kek_symbol), ASSET("5.00000 KEK"));

    game_action(game_name, ses_id, 0, {2});

    game_action(game_name, ses_id, 0, {2});
    transfer(player_name, game_name, ASSET("5.00000 KEK"));
    game_action(game_name, ses_id, 0, {0});

    BOOST_REQUIRE(get_events(events_id::game_finished) != std::nullopt);

    auto casino_balance_after = get_balance(casino_name, kek_symbol);
    auto player_balance_after = get_balance(player_name, kek_symbol);

    const auto& session = get_game_session(game_name, ses_id);
    BOOST_REQUIRE_EQUAL(session.is_null(), true);
    BOOST_REQUIRE_EQUAL(casino_balance_before + player_balance_before, casino_balance_after + player_balance_after);
}
FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(session_exiration_test, win_raise_lose_tester) try {
    auto player_name = N(player);

    create_player(player_name);
    link_game(player_name, game_name);

    transfer(N(eosio), player_name, ASSET("10.00000 KEK"));
    transfer(N(eosio), casino_name, ASSET("1000.00000 KEK"));

    auto player_balance_before = get_balance(player_name, kek_symbol);
    auto casino_balance_before = get_balance(casino_name, kek_symbol);

    auto player_bet = ASSET("5.00000 KEK");
    auto ses_id = new_game_session(game_name, player_name, casino_id, player_bet);

    BOOST_REQUIRE_EQUAL(get_balance(game_name, kek_symbol), player_bet);

    BOOST_REQUIRE_EQUAL(push_action(game_name, N(close), {platform_name, N(active)}, mvo()("req_id", ses_id)),
                        wasm_assert_msg("session isn't expired, only expired session can be closed"));

    produce_block(fc::seconds(game_session_ttl + 1));

    close_session(game_name, ses_id);

    auto session = get_game_session(game_name, ses_id);
    BOOST_REQUIRE_EQUAL(session.is_null(), true);

    auto player_balance_after = get_balance(player_name, kek_symbol);
    auto casino_balance_after = get_balance(casino_name, kek_symbol);

    // deposit returns if player hasn't acted
    BOOST_REQUIRE_EQUAL(player_balance_before, player_balance_after);
    BOOST_REQUIRE_EQUAL(casino_balance_before, casino_balance_after);
}
FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(incorrect_deposit_test, win_raise_lose_tester) try {
    auto player_name = N(player);

    create_player(player_name);
    link_game(player_name, game_name);

    transfer(N(eosio), player_name, STRSYM("10.0000"));
    transfer(N(eosio), casino_name, STRSYM("1000.0000"));


    transfer(N(eosio), player_name, ASSET("10.00000 KEK"));
    transfer(N(eosio), casino_name, ASSET("1000.00000 KEK"));

    auto ses_id = new_game_session(game_name, player_name, casino_id, ASSET("5.00000 KEK"));
    game_action(game_name, ses_id, 0, {2});
    BOOST_REQUIRE_EQUAL(
        transfer(player_name, game_name, ASSET("5.0000 LUL")),
        wasm_assert_msg("unable to find key")
    );

    // incorrect token
    BOOST_REQUIRE_EQUAL(
        transfer(player_name, game_name, STRSYM("5.0000"), std::to_string(ses_id)),
        wasm_assert_msg("invalid token symbol")
    );

    // incorrect precision
    BOOST_REQUIRE_EQUAL(
        transfer(player_name, game_name, ASSET("5.0000 KEK"), std::to_string(ses_id)),
        wasm_assert_msg("symbol precision mismatch")
    );
}
FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(incorrect_deposits_test, win_raise_lose_tester) try {
    name player_name = N(player);

    create_player(player_name);
    link_game(player_name, game_name);

    transfer(N(eosio), player_name, ASSET("100.00000 KEK"));
    transfer(N(eosio), casino_name, ASSET("1000.00000 KEK"));
    transfer(N(eosio), player_name, STRSYM("100.0000"));
    transfer(N(eosio), casino_name, STRSYM("1000.0000"));
    const auto bonus_bal = STRSYM("10.0000");

    BOOST_REQUIRE_EQUAL(
        push_action(casino_name, N(sendbon), {casino_name, N(active)}, mvo()("to", player_name)("amount", bonus_bal)),
        success()
    );

    // try to deposit with incorrect precision
    BOOST_REQUIRE_EQUAL(
        transfer(player_name, game_name, ASSET("5.0000 KEK"), "10"),
        wasm_assert_msg("symbol precision mismatch")
    );

    // try to deposit different real and bonus assets
    new_game_session(game_name, player_name, casino_id, ASSET("5.00000 KEK"), STRSYM("1.0000"), 
        wasm_assert_msg("deposit bonus incorrect token"));
    
    // similar as above but extra bonus deposit
    const auto ses_id = new_game_session(game_name, player_name, casino_id, ASSET("5.00000 KEK"));
    BOOST_REQUIRE_EQUAL(
        push_action(
            game_name,
            N(depositbon),
            {platform_name, N(gameaction)},
            mvo()("req_id", ses_id)("from", player_name)("quantity", STRSYM("1.0000"))
        ), 
        wasm_assert_msg("deposit bonus incorrect symbol")
    );

    // try to deposit with incorrect precision
    BOOST_REQUIRE_EQUAL(
        transfer(player_name, game_name, ASSET("5.00000 KEK"), "12"),
        success()
    );
    BOOST_REQUIRE_EQUAL(
        transfer(player_name, game_name, STRSYM("5.0000"), "12"),
        wasm_assert_msg("invalid token symbol")
    );
}
FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(unallowed_token_test, win_raise_lose_tester) try {
    name player_name = N(player);

    create_player(player_name);
    link_game(player_name, game_name);

    const name contract = N(token.lul);
    const auto token_name = "LUL";

    create_account(contract);
    deploy_contract<contracts::system::token>(contract);

    symbol symb = symbol{string_to_symbol_c(4, token_name)};
    create_currency( contract, config::system_account_name, asset(100000000000000, symb) );
    issue( config::system_account_name, asset(1672708210000, symb), contract );

    BOOST_REQUIRE_EQUAL(
        wasm_assert_msg("unable to find key"),
        transfer(player_name, game_name, ASSET("5.0000 LUL"))
    );

    push_action(platform_name, N(addtoken), platform_name, mvo()
        ("token_name", token_name)
        ("contract", contract)
    );

    BOOST_REQUIRE_EQUAL(
        transfer(N(eosio), player_name, ASSET("5.0000 LUL")),
        success()
    );

    BOOST_REQUIRE_EQUAL(
        transfer(player_name, game_name, ASSET("5.0000 LUL"), "10"),
        wasm_assert_msg("token is not supported")
    );
}
FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(game_params_test, win_raise_lose_tester) try {
    auto player_name = N(player);

    create_player(player_name);
    link_game(player_name, game_name);

    transfer(N(eosio), player_name, ASSET("10.00000 KEK"));
    auto ses_id = new_game_session(game_name, player_name, casino_id, ASSET("5.00000 KEK"));
    const auto& session = get_game_session(game_name, ses_id);
    const auto& params = extract_game_params(session["params"]);
    const game_params_type expected_params = {{0, 1}};
    BOOST_REQUIRE_EQUAL(params, expected_params);
    
    transfer(N(eosio), player_name, ASSET("10.0000 BET"));
    ses_id = new_game_session(game_name, player_name, casino_id, ASSET("5.0000 BET"));
    const auto& session_bet = get_game_session(game_name, ses_id);
    const auto& params_bet = extract_game_params(session_bet["params"]);
    const game_params_type expected_params_bet = {};
    BOOST_REQUIRE_EQUAL(params_bet, expected_params_bet);

    const auto token_lul = "LUL";
    allow_token(token_lul, 3, N(token.lul));
    const game_params_type expected_params_lul = {{0, 1}, {1, 1}};

    transfer(N(eosio), player_name, ASSET("10.000 LUL"));
    new_game_session(
        game_name, player_name, casino_id, 
        ASSET("5.000 LUL"), wasm_assert_msg("token is not allowed for this game")
    );

    BOOST_REQUIRE_EQUAL(
        success(),
        push_action(casino_name, N(setgameparam2), casino_name, mvo()
            ("game_id", 0)
            ("token", token_lul)
            ("params", expected_params_lul)
        )
    );

    ses_id = new_game_session(game_name, player_name, casino_id, ASSET("5.000 LUL"));
    const auto& session_lul = get_game_session(game_name, ses_id);
    const auto& params_lul = extract_game_params(session_lul["params"]);
    BOOST_REQUIRE_EQUAL(params_lul, expected_params_lul);
}
FC_LOG_AND_RETHROW()

BOOST_AUTO_TEST_SUITE_END()

} // namespace testing
