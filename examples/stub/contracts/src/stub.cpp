#include <stub/stub.hpp>

namespace stub {

void stub::on_new_game(uint64_t ses_id) {
    // always require `stub` action
    require_action(ses_id, stub_game_action_type);
}

void stub::on_action(uint64_t ses_id, uint16_t type, std::vector<uint32_t> params) {
    // always require random
    require_random(ses_id);
}

void stub::on_random(uint64_t ses_id, checksum256 rand) {
    // player always lost
    finish_game(ses_id, zero_asset);
}

void stub::on_finish(uint64_t ses_id) {
    // do nothing
}

} // namespace stub

GAME_CONTRACT(stub::stub)
