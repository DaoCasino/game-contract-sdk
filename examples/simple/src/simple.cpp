#include <simple/simple.hpp>

namespace simple {

void simple::on_new_game(uint64_t ses_id) {
    //STUB
}

void simple::on_action(uint64_t ses_id, uint16_t type, std::vector<uint32_t> params) {
    //STUB
}

void simple::on_random(uint64_t ses_id, checksum256 rand) {
    //STUB
}

} // namespace simple

extern "C" {
void apply( uint64_t receiver, uint64_t code, uint64_t action ) {
    eosio::check(false, "dawdw");
}
}
