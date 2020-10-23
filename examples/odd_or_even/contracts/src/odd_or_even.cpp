#include <odd_or_even/odd_or_even.hpp>

namespace odd_or_even {

void odd_or_even::on_new_game(uint64_t ses_id) {
    // always require `odd_or_even` action
    state.emplace(get_self(), [&](auto& row) {
        row.ses_id = ses_id;
        row.round = 1;
        row.prev_round_deposit = zero_asset;
    });
    require_action(0);
}

void odd_or_even::on_action(uint64_t ses_id, uint16_t type, std::vector<game_sdk::param_t> params) {
    eosio::check(type == action::bet || type == action::take, "invalid action");
    update_max_win(get_session(ses_id).deposit * 3 / 2);
    const auto state_itr = state.require_find(ses_id, "invalid ses_id");
    if (type == action::take) {
        if (state_itr->round == 1) {
            return finish_game(get_session(ses_id).deposit);
        }
        eosio::check(get_session(ses_id).deposit == state_itr->prev_round_deposit, "cannot deposit and take");
        return finish_game(get_session(ses_id).deposit * 3 / 2);
    }
    eosio::check(state_itr->round <= MAX_NUM_ROUNDS, "invalid round number");
    eosio::check(get_session(ses_id).deposit > state_itr->prev_round_deposit, "should deposit before bet");
    require_random();
}

void odd_or_even::on_random(uint64_t ses_id, checksum256 rand) {
    const auto state_itr = state.require_find(ses_id, "invalid ses_id");
    const auto round = state_itr->round;

    state.modify(state_itr, get_self(), [&](auto& row) {
        row.prev_round_deposit = get_session(ses_id).deposit;
        row.round++;
    });

    const auto odd = service::cut_to<game_sdk::param_t>(rand) % 2;
    if (odd) {
        eosio::print("player wins\n");
        // player wins
        if (round == MAX_NUM_ROUNDS) {
            return finish_game(get_session(ses_id).deposit * 3 / 2);
        }
        return require_action(0);
    }
    eosio::print("player loses\n");
    // player loses
    finish_game(get_session(ses_id).deposit / 2);
}

void odd_or_even::on_finish(uint64_t ses_id) {
    const auto state_itr = state.find(ses_id);
    if (state_itr != state.end()) {
        state.erase(state_itr);
    }
}

} // namespace odd_or_even

GAME_CONTRACT(odd_or_even::odd_or_even)
