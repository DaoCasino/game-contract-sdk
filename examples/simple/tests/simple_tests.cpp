#include <game_tester/game_tester.hpp>

#include "contracts.hpp"

namespace testing {

class simple_tester: public game_tester {
public:
    static const name game_name;
    static constexpr uint32_t default_min_bet = 1;
    static constexpr uint32_t default_max_bet = 10;
    static constexpr uint32_t default_max_payout = 20;

public:
    simple_tester() {
        create_account(game_name);

        game_params_type game_params = {
            {0, default_min_bet * 10000},
            {1, default_max_bet * 10000},
            {2, default_max_payout * 10000}
        };

        deploy_game<simple_game>(game_name, game_params);
    }
};

const name simple_tester::game_name = N(simplegame);


BOOST_AUTO_TEST_SUITE(simple_tests)

BOOST_FIXTURE_TEST_CASE(new_session_test, simple_tester) try {
    auto player_name = N(algys);

    create_player(player_name);
    link_game(player_name, game_name);

    transfer(N(eosio), player_name, STRSYM("10.0000"));

    auto ses_id = new_game_session(game_name, player_name, casino_id, STRSYM("5.0000"));

    auto session = get_game_session(game_name, ses_id);

    BOOST_REQUIRE_EQUAL(session["req_id"].as<uint64_t>(), ses_id);
    BOOST_REQUIRE_EQUAL(session["casino_id"].as<uint64_t>(), casino_id);
    BOOST_REQUIRE_EQUAL(session["ses_seq"].as<uint64_t>(), 0);
    BOOST_REQUIRE_EQUAL(session["player"].as<name>(), player_name);
    BOOST_REQUIRE_EQUAL(session["state"].as<uint32_t>(), 2); // req_action state
    BOOST_REQUIRE_EQUAL(session["deposit"].as<asset>(), STRSYM("5.0000"));
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(max_win_min_test, simple_tester) try {
    auto player_name = N(algys);

    create_player(player_name);
    link_game(player_name, game_name);

    transfer(N(eosio), player_name, STRSYM("10.0000"));

    auto ses_id = new_game_session(game_name, player_name, casino_id, STRSYM("5.0000"));

    game_action(game_name, ses_id, 0, { 1 });

    auto session = get_game_session(game_name, ses_id);
    BOOST_REQUIRE_EQUAL(session["last_max_win"].as<asset>(), STRSYM("0.0000"));
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(max_win_max_test, simple_tester) try {
    auto player_name = N(algys);

    create_player(player_name);
    link_game(player_name, game_name);

    transfer(N(eosio), player_name, STRSYM("10.0000"));

    auto ses_id = new_game_session(game_name, player_name, casino_id, STRSYM("5.0000"));

    game_action(game_name, ses_id, 0, { 99 });

    auto session = get_game_session(game_name, ses_id);
    BOOST_REQUIRE_EQUAL(session["last_max_win"].as<asset>(), STRSYM("20.0000"));
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(max_win_normal_test, simple_tester) try {
    auto player_name = N(algys);

    create_player(player_name);
    link_game(player_name, game_name);

    transfer(N(eosio), player_name, STRSYM("10.0000"));

    auto ses_id = new_game_session(game_name, player_name, casino_id, STRSYM("5.0000"));

    game_action(game_name, ses_id, 0, { 50 });

    auto session = get_game_session(game_name, ses_id);

    asset expected_max_win = asset(default_max_payout * 10000 / 50, symbol(CORE_SYM)); // 50% win chance -> max_win = max_payout / 50
    BOOST_REQUIRE_EQUAL(session["last_max_win"].as<asset>(), expected_max_win);
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(signidice_test, simple_tester) try {
    auto player_name = N(algys);

    create_player(player_name);
    link_game(player_name, game_name);

    transfer(N(eosio), player_name, STRSYM("10.0000"));
    transfer(N(eosio), casino_name, STRSYM("1000.0000"));

    auto ses_id = new_game_session(game_name, player_name, casino_id, STRSYM("5.0000"));
    game_action(game_name, ses_id, 0, { 90 });

    auto session = get_game_session(game_name, ses_id);
    BOOST_REQUIRE_EQUAL(session["state"].as<uint32_t>(), 3); // req_signidice_part_1 state

    signidice(game_name, ses_id);

    std::cout << "casino balance: " << get_balance(casino_name) << "\n";
    std::cout << "player balance: " << get_balance(player_name) << "\n";
    std::cout << "game balance: " << get_balance(game_name) << "\n";

//    session = get_game_session(game_name, ses_id);
//    BOOST_REQUIRE_EQUAL(session["state"].as<uint32_t>(), 4); // req_signidice_part_2 state
} FC_LOG_AND_RETHROW()

BOOST_AUTO_TEST_SUITE_END()

} // namespace testing

