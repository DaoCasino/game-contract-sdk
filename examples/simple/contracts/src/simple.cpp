#include <simple/simple.hpp>

namespace simple {

void simple::check_params(uint64_t ses_id) {
    eosio::check(get_param_value(ses_id, min_bet_param_type) != std::nullopt, "absent min bet param");
    eosio::check(get_param_value(ses_id, max_bet_param_type) != std::nullopt, "absent max bet param");
    eosio::check(get_param_value(ses_id, max_payout_param_type) != std::nullopt, "absent max payout param");
}

void simple::check_bet(uint64_t ses_id) {
    const auto& session = get_session(ses_id);
    auto min_bet = asset(*get_param_value(ses_id, min_bet_param_type), core_symbol);
    auto max_bet = asset(*get_param_value(ses_id, max_bet_param_type), core_symbol);
    eosio::check(min_bet <= session.deposit, "deposit less than min bet");
    eosio::check(max_bet >= session.deposit, "deposit greater than max bet");
}

asset simple::calc_max_win(uint64_t ses_id, uint32_t num) {
    const auto& session = get_session(ses_id);
    auto max_profit = asset(*get_param_value(ses_id, max_payout_param_type), core_symbol);

    auto all_range = 10000;
    auto win_range = all_range - num * 100;
    auto win_chance = win_range * 100 / all_range; // 1..99

    if (win_chance > 98)
        return zero_asset;

    return max_profit / win_chance; // 0..max_payout
}

void simple::on_new_game(uint64_t ses_id) {
    check_params(ses_id);
    check_bet(ses_id);

    require_action(ses_id, roll_action_type);
}

void simple::on_action(uint64_t ses_id, uint16_t type, std::vector<uint32_t> params) {
    eosio::check(type == roll_action_type, "allowed only roll action with type 0");
    eosio::check(params.size() == 1, "params amount should be 1");
    eosio::check(params[0] > 0, "number should be more than 0");
    eosio::check(params[0] < 100, "number should be less than 100");

    rolls.emplace(get_self(), [&](auto& row){
        row.ses_id = ses_id;
        row.number = params[0];
    });

    update_max_win(ses_id, calc_max_win(ses_id, params[0]));

    require_random(ses_id);
}

void simple::on_random(uint64_t ses_id, checksum256 rand) {
    const auto& roll = rolls.get(ses_id);
    const auto& session = get_session(ses_id);
    auto rand_number = rand_range(rand, 0, 100);

    eosio::print("rand num: ", rand_number, "\n");

    if (roll.number >= rand_number) {
        finish_game(ses_id, zero_asset);
        return;
    }

    finish_game(ses_id, calc_max_win(ses_id, roll.number) + session.deposit);
}

void simple::on_finish(uint64_t ses_id) {
    rolls.erase(rolls.get(ses_id));
}

uint32_t simple::rand_range(const checksum256& rand, uint32_t lower, uint32_t upper) {
    uint32_t val = 0u;
    for (auto byte: rand.extract_as_byte_array()) {
        val += byte;
    }
    return val % (upper - lower) + lower;
}

} // namespace simple

GAME_CONTRACT(simple::simple)
