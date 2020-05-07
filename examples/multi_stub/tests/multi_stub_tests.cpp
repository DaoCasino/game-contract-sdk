#include <game_tester/game_tester.hpp>

#include "contracts.hpp"

namespace testing {

class stub_tester : public game_tester {
  public:
    static const name game_name;
    static constexpr uint16_t stub_game_action_type{0u};

  public:
    stub_tester() {
        create_account(game_name);

        game_params_type game_params = {};
        deploy_game<multi_stub_game>(game_name, game_params);
    }
};

const name stub_tester::game_name = N(mstubgame);

BOOST_AUTO_TEST_SUITE(mstub_tests)

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
}
FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(full_session_one_action_success_test, stub_tester) try {
    const auto player_name = N(player);

    create_player(player_name);
    link_game(player_name, game_name);

    transfer(N(eosio), player_name, STRSYM("10.0000"));
    transfer(N(eosio), casino_name, STRSYM("1000.0000"));

    auto player_bet = STRSYM("5.0000");
    auto ses_id = new_game_session(game_name, player_name, casino_id, player_bet);

    BOOST_REQUIRE_EQUAL(get_balance(game_name), player_bet);

    game_action(game_name, ses_id, 0, {1});

    auto session = get_game_session(game_name, ses_id);
    BOOST_REQUIRE_EQUAL(session["state"].as<uint32_t>(),
                        3); // req_signidice_part_1 state

    signidice(game_name, ses_id);

    auto casino_balance_after = get_balance(casino_name);
    auto player_balance_after = get_balance(player_name);

    session = get_game_session(game_name, ses_id);
    BOOST_REQUIRE_EQUAL(session.is_null(), true);
}
FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(full_session_actions_success_test, stub_tester) try {
    const auto player_name = N(player);

    create_player(player_name);
    link_game(player_name, game_name);

    transfer(N(eosio), player_name, STRSYM("10.0000"));
    transfer(N(eosio), casino_name, STRSYM("1000.0000"));

    auto player_bet = STRSYM("5.0000");
    auto ses_id = new_game_session(game_name, player_name, casino_id, player_bet);

    game_action(game_name, ses_id, 0, {3});
    signidice(game_name, ses_id);
    if (const auto msg = get_events(events_id::game_message); msg != std::nullopt)
    {
        const auto values = msg.value()[0].as<std::vector<uint64_t>>();
    }

    game_action(game_name, ses_id, 1, {2});
    signidice(game_name, ses_id);

    game_action(game_name, ses_id, 2, {1});
    signidice(game_name, ses_id);

    const auto session = get_game_session(game_name, ses_id);
    BOOST_REQUIRE_EQUAL(session.is_null(), true);
}
FC_LOG_AND_RETHROW()

BOOST_AUTO_TEST_SUITE_END()

} // namespace testing
