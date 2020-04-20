#include <proto_dice/proto_dice.hpp>

namespace proto_dice {

void proto_dice::check_params(uint64_t ses_id) {
    eosio::check(get_param_value(ses_id, min_bet_param_type) != std::nullopt,
                 "absent min bet param");
    eosio::check(get_param_value(ses_id, max_bet_param_type) != std::nullopt,
                 "absent max bet param");
    eosio::check(get_param_value(ses_id, max_payout_param_type) != std::nullopt,
                 "absent max payout param");
}

void proto_dice::check_bet(uint64_t ses_id) {
    const auto &session = get_session(ses_id);
    const auto min_bet =
        asset(*get_param_value(ses_id, min_bet_param_type), core_symbol);
    const auto max_bet =
        asset(*get_param_value(ses_id, max_bet_param_type), core_symbol);

    eosio::check(min_bet <= session.deposit, "deposit less than min bet");
    eosio::check(max_bet >= session.deposit, "deposit greater than max bet");
}

asset proto_dice::calc_max_win(uint64_t ses_id, game_sdk::param_t num) {
    const auto &session = get_session(ses_id);
    auto max_profit =
        asset(*get_param_value(ses_id, max_payout_param_type), core_symbol);

    auto all_range = 100;
    auto win_chance = all_range - num;

    if (win_chance > 98) // if player choose '1' then profit -> 0
        return session.deposit;

    return max_profit / win_chance + session.deposit;
}

void proto_dice::on_new_game(uint64_t ses_id) {
    check_params(ses_id);
    check_bet(ses_id);

    require_action(roll_action_type);
}

void proto_dice::on_action(uint64_t ses_id, uint16_t type,
                           std::vector<game_sdk::param_t> params) {
    eosio::check(type == roll_action_type,
                 "allowed only roll action with type 0");
    eosio::check(params.size() == 1, "params amount should be 1");
    eosio::print("player number: ", params[0], "\n");
    eosio::check(params[0] > 0, "number should be more than 0");
    eosio::check(params[0] < 100, "number should be less than 100");

    rolls.emplace(get_self(), [&](auto &row) {
        row.ses_id = ses_id;
        row.number = uint32_t(params[0]);
    });

    update_max_win(calc_max_win(ses_id, params[0]));

    require_random();
}

void proto_dice::on_random(uint64_t ses_id, checksum256 rand) {
    const auto &roll = rolls.get(ses_id);
    const auto &session = get_session(ses_id);
    const auto rand_number = service::cut_to<uint32_t>(rand) % 100;

    eosio::print("rand num: ", rand_number, "\n");

    if (roll.number >= rand_number) { // Loose
        finish_game(zero_asset);
        return;
    }

    finish_game(calc_max_win(ses_id, roll.number));
}

void proto_dice::on_finish(uint64_t ses_id) {
    const auto roll_itr = rolls.find(ses_id);
    if (roll_itr != rolls.end()) {
        rolls.erase(roll_itr);
    }
}

} // namespace proto_dice

GAME_CONTRACT(proto_dice::proto_dice)
