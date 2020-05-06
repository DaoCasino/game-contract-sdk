#include <multi_stub/multi_stub.hpp>

namespace multi_stub {

void multi_stub::on_new_game(uint64_t ses_id) {
    // always require `stub` action
    require_action(stub_game_action_type);
}

void multi_stub::on_action(uint64_t ses_id, uint16_t type, std::vector<game_sdk::param_t> params) {
    // always require random
    require_random();
}

void multi_stub::on_random(uint64_t ses_id, checksum256 rand) {
    // player always lost
    finish_game(zero_asset);
}

void multi_stub::on_finish(uint64_t ses_id) {
    // do nothing
}

} // namespace stub

GAME_CONTRACT(multi_stub::multi_stub)
