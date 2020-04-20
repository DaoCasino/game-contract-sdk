#include <eosio/datastream.hpp>
#include <eosio/eosio.hpp>
#include <eosio/serialize.hpp>

namespace game_sdk {

// fwd declaration for 'execute_action' func
class game;

template <typename T, typename... Args>
bool execute_action(eosio::name self, eosio::name code,
                    void (game::*func)(Args...)) {
    using namespace eosio;
    size_t size = action_data_size();

    // using malloc/free here potentially is not exception-safe, although WASM
    // doesn't support exceptions
    constexpr size_t max_stack_buffer_size = 512;
    void *buffer = nullptr;
    if (size > 0) {
        buffer = max_stack_buffer_size < size ? malloc(size) : alloca(size);
        read_action_data(buffer, size);
    }

    std::tuple<std::decay_t<Args>...> args;
    datastream<const char *> ds((char *)buffer, size);
    ds >> args;

    T inst(self, code, ds);

    auto f2 = [&](auto... a) { ((&inst)->*func)(a...); };

    boost::mp11::tuple_apply(f2, args);
    if (max_stack_buffer_size < size) {
        free(buffer);
    }
    return true;
}

} // namespace game_sdk

#define GAME_CONTRACT(TYPE)                                                    \
    extern "C" {                                                               \
    void apply(uint64_t receiver, uint64_t code, uint64_t action) {            \
        if (code == "eosio.token"_n.value && action == "transfer"_n.value) {   \
            game_sdk::execute_action<TYPE>(                                    \
                eosio::name(receiver), eosio::name(code), &TYPE::on_transfer); \
        } else if (code == receiver) {                                         \
            switch (action) {                                                  \
            case "init"_n.value:                                               \
                game_sdk::execute_action<TYPE>(                                \
                    eosio::name(receiver), eosio::name(code), &TYPE::init);    \
                break;                                                         \
            case "newgame"_n.value:                                            \
                game_sdk::execute_action<TYPE>(eosio::name(receiver),          \
                                               eosio::name(code),              \
                                               &TYPE::new_game);               \
                break;                                                         \
            case "gameaction"_n.value:                                         \
                game_sdk::execute_action<TYPE>(eosio::name(receiver),          \
                                               eosio::name(code),              \
                                               &TYPE::game_action);            \
                break;                                                         \
            case "sgdicefirst"_n.value:                                        \
                game_sdk::execute_action<TYPE>(eosio::name(receiver),          \
                                               eosio::name(code),              \
                                               &TYPE::signidice_part_1);       \
                break;                                                         \
            case "sgdicesecond"_n.value:                                       \
                game_sdk::execute_action<TYPE>(eosio::name(receiver),          \
                                               eosio::name(code),              \
                                               &TYPE::signidice_part_2);       \
                break;                                                         \
            case "close"_n.value:                                              \
                game_sdk::execute_action<TYPE>(                                \
                    eosio::name(receiver), eosio::name(code), &TYPE::close);   \
                break;                                                         \
            default:                                                           \
                eosio::eosio_exit(1);                                          \
            }                                                                  \
        }                                                                      \
        eosio::eosio_exit(0);                                                  \
    }                                                                          \
    }
