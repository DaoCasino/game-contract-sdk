#include <win_raise_lose/win_raise_lose.hpp>

namespace win_raise_lose {

void win_raise_lose::on_new_game(uint64_t ses_id) {
    require_action(action::roll);
}

void win_raise_lose::on_action(uint64_t ses_id, uint16_t type, std::vector<game_sdk::param_t> params) {
    eosio::check(type == action::roll, "allowed only action with type 0");
    eosio::check(params.size() == 1, "params amount should be 1");
    eosio::check(params[0] == decision::win || params[0] == decision::lose
        || params[0] == decision::deposit, "allowed only decision with type 0, 1, 2");

    const auto roll_it = rolls.find(ses_id);
    const auto& session = get_session(ses_id);

    if (roll_it == rolls.end()) {
        rolls.emplace(get_self(), [&](auto& row) {
            row.ses_id = ses_id;
            row.deposit = session.deposit;
        });
    } else {
        rolls.modify(roll_it, get_self(), [&](auto& row) {
            row.deposit = session.deposit;
        });
    }

    const auto& roll = rolls.get(ses_id);
    update_max_win(roll.deposit * win_coef);

    switch (params[0])
    {
    case decision::win:
        finish_game(win_coef * roll.deposit);
        break;
    case decision::lose:
        finish_game(asset(0, get_session_symbol(ses_id)));
        break;
    case decision::deposit:
        require_action(action::roll, true);
        break;
    default:
        break;
    }
}

void win_raise_lose::on_random(uint64_t ses_id, checksum256 rand) {}

void win_raise_lose::on_finish(uint64_t ses_id) {
    const auto roll_itr = rolls.find(ses_id);
    if (roll_itr != rolls.end()) {
        rolls.erase(roll_itr);
    }
}

} // namespace win_raise_lose

GAME_CONTRACT(win_raise_lose::win_raise_lose)
