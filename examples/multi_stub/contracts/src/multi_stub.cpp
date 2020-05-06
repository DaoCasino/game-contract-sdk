#include <multi_stub/multi_stub.hpp>

namespace multi_stub {
using boost::hana::detail::accumulate;

void multi_stub::on_new_game(uint64_t ses_id) {

    rolls.emplace(get_self(), [&](auto& row) {
        row.ses_id = ses_id;
        row.counter.resize(6);
    });

    // always require `stub` action
    require_action(first_action);
}

void multi_stub::on_action(uint64_t ses_id, uint16_t type, std::vector<game_sdk::param_t> params) {
    if (type != first_action && type != second_action && type != third_action)
        return eosio::check(false, "Wrong action type.");

    if (params.size() != 1)
        return eosio::check(false, "Wrong params size.");

    const auto it = rolls.find(ses_id);
    eosio::check(it != rolls.end(), "Session error");
    rolls.modify(it, get_self(), [&](auto& row) { row.counter.push_back(params[0]); });

    return require_random();
}

void multi_stub::on_random(uint64_t ses_id, checksum256 rand) {
    uint8_t stored;

    const auto it = rolls.find(ses_id);
    eosio::check(it != rolls.end(), "Session error");
    rolls.modify(it, get_self(), [&](auto& row) {
        row.counter.push_back(service::cut_to<game_sdk::param_t>(rand));
        stored = row.counter.size();
    });

    switch (stored) {
    case 2:
        return require_action(second_action);
    case 4:
        return require_action(third_action);
    case 6:
        return finish_game(zero_asset);
    default:
        return eosio::check(false, "Unreachable.");
    }
}

void multi_stub::on_finish(uint64_t ses_id) {
    if (const auto roll_itr = rolls.find(ses_id); roll_itr != rolls.end()) {
        rolls.erase(roll_itr);
    }
}

} // namespace multi_stub

GAME_CONTRACT(multi_stub::multi_stub)
