#include <game_tester/game_tester.hpp>

#include "contracts.hpp"

namespace testing {

class odd_or_even_tester : public game_tester {
public:
    static const name game_name;
    static const name player_name;
    static const asset zero_asset;
    static const asset starting_balance;
    static const sha256 odd_number;
    static const sha256 even_number;
public:
    odd_or_even_tester() {
        create_account(game_name);
        deploy_game<odd_or_even_game>(game_name, {});

        create_player(player_name);
        link_game(player_name, game_name);

        transfer(N(eosio), casino_name, starting_balance);
        transfer(N(eosio), player_name, starting_balance);
        // bonuses
        BOOST_REQUIRE_EQUAL(
            push_action(casino_name, N(sendbon), {casino_name, N(active)}, mvo()
                ("to", player_name)
                ("amount", starting_balance)
            ),
            success()
        );
    }

    void bet(uint64_t ses_id, asset real = zero_asset, asset bonus = zero_asset) {
        game_action(game_name, ses_id, 0, {}, real, bonus);
    }

    void take(uint64_t ses_id) {
        game_action(game_name, ses_id, 1, {});
    }

    #ifdef IS_DEBUG
    void roll(uint64_t ses_id, const sha256& number, asset real = zero_asset, asset bonus = zero_asset) {
        push_next_random(game_name, number);
        bet(ses_id, real, bonus);
        signidice(game_name, ses_id);
    }
    #endif

    void check_player_win(asset real = zero_asset, asset bonus = zero_asset) {
        BOOST_REQUIRE_EQUAL(get_balance(player_name) - starting_balance, real);
        BOOST_REQUIRE_EQUAL(get_balance(casino_name) - starting_balance, -real);
        BOOST_REQUIRE_EQUAL(get_bonus_balance(player_name) - starting_balance, bonus);
    }
};

const name odd_or_even_tester::game_name = N(game);
const name odd_or_even_tester::player_name = N(player);
const asset odd_or_even_tester::zero_asset = STRSYM("0.0000");
const asset odd_or_even_tester::starting_balance = STRSYM("100.0000");
const sha256 odd_or_even_tester::odd_number = sha256("0000000000000000000000000000000000000000000000000000000000000001");
const sha256 odd_or_even_tester::even_number = sha256("0000000000000000000000000000000000000000000000000000000000000002");

BOOST_AUTO_TEST_SUITE(odd_or_even_tests)

#ifdef IS_DEBUG

// ============ ZERO ROUNDS

BOOST_FIXTURE_TEST_CASE(take_no_bet_real, odd_or_even_tester)
{
    const auto ses_id = new_game_session(game_name, player_name, casino_id, STRSYM("10.0000"));
    take(ses_id);
    check_player_win();
}

BOOST_FIXTURE_TEST_CASE(take_no_bet_bonus, odd_or_even_tester)
{
    const auto ses_id = new_game_session(game_name, player_name, casino_id, zero_asset, STRSYM("10.0000"));
    take(ses_id);
    check_player_win();
}

BOOST_FIXTURE_TEST_CASE(take_no_bet_mixed, odd_or_even_tester)
{
    const auto ses_id = new_game_session(game_name, player_name, casino_id, STRSYM("10.0000"), STRSYM("10.0000"));
    take(ses_id);
    check_player_win();
}

// ================= SINGLE ROUND

BOOST_FIXTURE_TEST_CASE(bet_and_win_one_round_real, odd_or_even_tester)
{
    const auto ses_id = new_game_session(game_name, player_name, casino_id, STRSYM("10.0000"));
    roll(ses_id, odd_number);
    take(ses_id);
    check_player_win(STRSYM("5.0000"));
}

BOOST_FIXTURE_TEST_CASE(bet_and_win_one_round_bonus, odd_or_even_tester)
{
    const auto ses_id = new_game_session(game_name, player_name, casino_id, zero_asset, STRSYM("10.0000"));
    roll(ses_id, odd_number);
    take(ses_id);
    check_player_win(zero_asset, STRSYM("5.0000"));
}

BOOST_FIXTURE_TEST_CASE(bet_and_win_one_round_mixed, odd_or_even_tester)
{
    const auto ses_id = new_game_session(game_name, player_name, casino_id, STRSYM("8.0000"), STRSYM("2.0000"));
    roll(ses_id, odd_number);
    take(ses_id);
    check_player_win(STRSYM("4.0000"), STRSYM("1.0000"));
}

BOOST_FIXTURE_TEST_CASE(bet_and_win_one_round_mixed_larger_bonus, odd_or_even_tester)
{
    const auto ses_id = new_game_session(game_name, player_name, casino_id, STRSYM("2.0000"), STRSYM("8.0000"));
    roll(ses_id, odd_number);
    take(ses_id);
    check_player_win(STRSYM("1.0000"), STRSYM("4.0000"));
}

BOOST_FIXTURE_TEST_CASE(bet_and_win_one_round_mixed_fractional, odd_or_even_tester)
{
    const auto ses_id = new_game_session(game_name, player_name, casino_id, STRSYM("9.0000"), STRSYM("1.0000"));
    roll(ses_id, odd_number);
    take(ses_id);
    check_player_win(STRSYM("4.5000"), STRSYM("0.5000"));
}

BOOST_FIXTURE_TEST_CASE(bet_and_win_one_round_mixed_larger_bonus_fractional, odd_or_even_tester)
{
    const auto ses_id = new_game_session(game_name, player_name, casino_id, STRSYM("1.0000"), STRSYM("9.0000"));
    roll(ses_id, odd_number);
    take(ses_id);
    check_player_win(STRSYM("0.5000"), STRSYM("4.5000"));
}


BOOST_FIXTURE_TEST_CASE(bet_and_lose_one_round_real, odd_or_even_tester)
{
    const auto ses_id = new_game_session(game_name, player_name, casino_id, STRSYM("10.0000"));
    roll(ses_id, even_number);
    check_player_win(-STRSYM("5.0000"));
}

BOOST_FIXTURE_TEST_CASE(bet_and_lose_one_round_bonus, odd_or_even_tester)
{
    const auto ses_id = new_game_session(game_name, player_name, casino_id, zero_asset, STRSYM("10.0000"));
    roll(ses_id, even_number);
    check_player_win(zero_asset, -STRSYM("5.0000"));
}

BOOST_FIXTURE_TEST_CASE(bet_and_lose_one_round_mixed, odd_or_even_tester)
{
    const auto ses_id = new_game_session(game_name, player_name, casino_id, STRSYM("5.0000"), STRSYM("5.0000"));
    roll(ses_id, even_number);
    check_player_win(-STRSYM("2.5000"), -STRSYM("2.5000"));
}

// ============= TWO ROUNDS

BOOST_FIXTURE_TEST_CASE(bet_and_win_two_rounds_real, odd_or_even_tester)
{
    const asset ante = STRSYM("10.0000");
    const auto ses_id = new_game_session(game_name, player_name, casino_id, ante);
    roll(ses_id, odd_number);
    roll(ses_id, odd_number, ante);
    take(ses_id);
    check_player_win(STRSYM("10.0000"));
}

BOOST_FIXTURE_TEST_CASE(bet_and_win_two_rounds_bonus, odd_or_even_tester)
{
    const asset ante = STRSYM("10.0000");
    const auto ses_id = new_game_session(game_name, player_name, casino_id, zero_asset, ante);
    roll(ses_id, odd_number);
    roll(ses_id, odd_number, zero_asset, ante);
    take(ses_id);
    check_player_win(zero_asset, STRSYM("10.0000"));
}

BOOST_FIXTURE_TEST_CASE(bet_and_win_two_rounds_mixed, odd_or_even_tester)
{
    const auto ses_id = new_game_session(game_name, player_name, casino_id, STRSYM("5.0000"), STRSYM("5.0000"));
    roll(ses_id, odd_number);
    roll(ses_id, odd_number, STRSYM("15.0000"), STRSYM("5.0000"));
    take(ses_id);
    check_player_win(STRSYM("10.0000"), STRSYM("5.0000"));
}

BOOST_FIXTURE_TEST_CASE(bet_and_lose_two_rounds_mixed, odd_or_even_tester)
{
    const auto ses_id = new_game_session(game_name, player_name, casino_id, STRSYM("5.0000"), STRSYM("5.0000"));
    roll(ses_id, odd_number);
    roll(ses_id, even_number, STRSYM("15.0000"), STRSYM("5.0000"));
    check_player_win(-STRSYM("10.0000"), -STRSYM("5.0000"));
}

// player stats

BOOST_FIXTURE_TEST_CASE(bet_twice_stats, odd_or_even_tester)
{
    auto ses_id = new_game_session(game_name, player_name, casino_id, STRSYM("5.0000"), STRSYM("5.0000"));
    roll(ses_id, odd_number);
    roll(ses_id, odd_number, STRSYM("15.0000"), STRSYM("5.0000"));
    take(ses_id);
    check_player_win(STRSYM("10.0000"), STRSYM("5.0000"));

    auto stats = get_player_stats(player_name);
    BOOST_REQUIRE_EQUAL(stats["sessions_created"].as<int>(), 1);
    BOOST_REQUIRE_EQUAL(stats["volume_real"].as<asset>(), STRSYM("20.0000"));
    BOOST_REQUIRE_EQUAL(stats["volume_bonus"].as<asset>(), STRSYM("10.0000"));
    BOOST_REQUIRE_EQUAL(stats["profit_real"].as<asset>(), STRSYM("10.0000"));
    BOOST_REQUIRE_EQUAL(stats["profit_bonus"].as<asset>(), STRSYM("5.0000"));

    ses_id = new_game_session(game_name, player_name, casino_id, STRSYM("5.0000"), STRSYM("5.0000"));
    roll(ses_id, odd_number);
    roll(ses_id, even_number, STRSYM("15.0000"), STRSYM("5.0000"));

    stats = get_player_stats(player_name);
    BOOST_REQUIRE_EQUAL(stats["sessions_created"].as<int>(), 2);
    BOOST_REQUIRE_EQUAL(stats["volume_real"].as<asset>(), STRSYM("40.0000"));
    BOOST_REQUIRE_EQUAL(stats["volume_bonus"].as<asset>(), STRSYM("20.0000"));
    BOOST_REQUIRE_EQUAL(stats["profit_real"].as<asset>(), STRSYM("0.0000"));
    BOOST_REQUIRE_EQUAL(stats["profit_bonus"].as<asset>(), STRSYM("0.0000"));
}

#endif

BOOST_AUTO_TEST_SUITE_END()

}