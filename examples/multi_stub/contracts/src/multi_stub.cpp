#include <multi_stub/multi_stub.hpp>

namespace multi_stub {

void multi_stub::on_new_game(session_id_t ses_id) {
    rolls.emplace(get_self(), [&](auto& row) { row.ses_id = ses_id; });
    require_action(0); // Require event with count of events
}

void multi_stub::on_action(session_id_t ses_id, uint16_t type, std::vector<game_sdk::param_t> params) {
    const auto it = rolls.find(ses_id);
    eosio::check(it != rolls.end(), "Can't find session.");

    if (type != it->event_numbers.size())
        return eosio::check(false, "Wrong action type.");

    if (params.size() != 1)
        return eosio::check(false, "Wrong params size.");

    if (it->event_numbers.empty()) {
        if (params[0] == 0) {
            return eosio::check(false, "First param in zero action can't be zero.");
        } else if (params[0] > std::numeric_limits<uint16_t>::max()) {
            return eosio::check(false, "First param is too large");
        }
    }

    rolls.modify(it, get_self(), [&](auto& row) {
        if (row.event_numbers.empty())
            row.event_numbers.reserve(params[0]);
        row.event_numbers.push_back(params[0]);
    });

    return require_random();
}

void multi_stub::on_random(session_id_t ses_id, checksum256 rand) {
    const auto it = rolls.find(ses_id);
    eosio::check(it != rolls.end(), "Can't find session.");

    send_game_message(std::vector<game_sdk::param_t>{service::cut_to<game_sdk::param_t>(rand)});

    const auto current_action = it->event_numbers.size();
    if (current_action < it->event_numbers[0]) {
        eosio::print("Current action: ", current_action, "\n");
        require_action(current_action);
    } else {
        eosio::print("Finish game at action: ", current_action, "\n");
        finish_game(zero_asset, it->event_numbers);
    }
}

void multi_stub::on_finish(session_id_t ses_id) {
    if (const auto roll_itr = rolls.find(ses_id); roll_itr != rolls.end()) {
        rolls.erase(roll_itr);
    }
}

} // namespace multi_stub

GAME_CONTRACT(multi_stub::multi_stub)
