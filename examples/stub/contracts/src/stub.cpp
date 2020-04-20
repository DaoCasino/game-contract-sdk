#include <stub/stub.hpp>

namespace stub {

void stub::on_new_game(uint64_t ses_id) {
    // always require `stub` action
    require_action(stub_game_action_type);
}

void stub::on_action(uint64_t ses_id, uint16_t type, std::vector<game_sdk::param_t> params) {
    // always require random
    require_random();
}

void stub::on_random(uint64_t ses_id, checksum256 rand) {
    // player always lost
    finish_game(zero_asset);
}

void stub::on_finish(uint64_t ses_id) {
    // do nothing
}

} // namespace stub

GAME_CONTRACT(stub::stub)
