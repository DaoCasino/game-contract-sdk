#include <multi_stub/multi_stub.hpp>

namespace multi_stub {
using boost::hana::detail::accumulate;

void multi_stub::on_new_game(uint64_t ses_id) {
    rolls.emplace(get_self(), [&](auto& row) { row.ses_id = ses_id; });

    // always require `stub` action
    require_action(0);
}

void multi_stub::on_action(uint64_t ses_id, uint16_t type, std::vector<game_sdk::param_t> params) {
    const auto it = rolls.find(ses_id);
    eosio::check(it != rolls.end(), "Can't find session.");

    if (!it->event_numbers.empty() && type != it->event_numbers.size())
        return eosio::check(false, "Wrong action type.");

    if (params.size() != 1)
        return eosio::check(false, "Wrong params size.");

    if (params[0] == 0)
        return eosio::check(false, "First param can't be zero.");

    rolls.modify(it, get_self(), [&](auto& row) {
        if (row.event_numbers.size() == 0) {
            row.event_numbers.reserve(params[0]);
        }
        row.event_numbers.push_back(params[0]);
    });

    return require_random();
}

void multi_stub::on_random(uint64_t ses_id, checksum256 rand) {
    const auto it = rolls.find(ses_id);
    eosio::check(it != rolls.end(), "Can't find session.");

    send_game_message(std::vector<game_sdk::param_t>{service::cut_to<game_sdk::param_t>(rand)});

    const uint8_t current_action = it->event_numbers.size();
    eosio::print("current", current_action);
    if (current_action < it->event_numbers[0]) {
        require_action(current_action + 1);
    } else {
        finish_game(zero_asset);
    }
}

void multi_stub::on_finish(uint64_t ses_id) {
    if (const auto roll_itr = rolls.find(ses_id); roll_itr != rolls.end()) {
        rolls.erase(roll_itr);
    }
}

} // namespace multi_stub

GAME_CONTRACT(multi_stub::multi_stub)
