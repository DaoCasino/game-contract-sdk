#include <game_tester/game_tester.hpp>

#include "contracts.hpp"

namespace testing {

BOOST_AUTO_TEST_SUITE(simple_tests)

BOOST_FIXTURE_TEST_CASE(new_session, game_tester) try {
    auto player_name = N(algys);
    create_player(player_name);

    auto game_name = N(simplegame);
    create_account(game_name);

    game_params_type game_params = {{0,1}, {1,2}};
    deploy_game<simple_game>(game_name, game_params);

    link_game(player_name, game_name);

    transfer(N(eosio), player_name, STRSYM("10.0000"));

    auto ses_id = new_game_session(game_name, player_name, casino_id, STRSYM("5.0000"));

    auto session = get_game_session(game_name, ses_id);

    BOOST_REQUIRE_EQUAL(session["req_id"].as<uint64_t>(), ses_id);
    BOOST_REQUIRE_EQUAL(session["casino_id"].as<uint64_t>(), casino_id);
    BOOST_REQUIRE_EQUAL(session["ses_seq"].as<uint64_t>(), 0);
    BOOST_REQUIRE_EQUAL(session["player"].as<name>(), player_name);
    BOOST_REQUIRE_EQUAL(session["deposit"].as<asset>(), STRSYM("5.0000"));

} FC_LOG_AND_RETHROW()

BOOST_AUTO_TEST_SUITE_END()

} // namespace testing

