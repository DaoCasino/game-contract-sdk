#include <eosio/datastream.hpp>
#include <eosio/eosio.hpp>
#include <eosio/serialize.hpp>

namespace game_sdk {

// fwd declaration for 'execute_action' func
class game;

template <typename T, typename... Args>
bool execute_action(eosio::name self, eosio::name code, void (game::*func)(Args...)) {
    using namespace eosio;
    size_t size = action_data_size();

    // using malloc/free here potentially is not exception-safe, although WASM
    // doesn't support exceptions
    constexpr size_t max_stack_buffer_size = 512;
    void* buffer = nullptr;
    if (size > 0) {
        buffer = max_stack_buffer_size < size ? malloc(size) : alloca(size);
        read_action_data(buffer, size);
    }

    std::tuple<std::decay_t<Args>...> args;
    datastream<const char*> ds((char*)buffer, size);
    ds >> args;

    T inst(self, code, ds);

    auto f2 = [&](auto... a) { ((&inst)->*func)(a...); };

    boost::mp11::tuple_apply(f2, args);
    if (max_stack_buffer_size < size) {
        free(buffer);
    }
    return true;
}

template <typename T, typename... Args>
bool execute_action(eosio::name self, eosio::name code, void (T::*func)(Args...)) {
    return execute_action<T>(self, code, (void (game::*)(Args...))func);
}

} // namespace game_sdk

#ifdef IS_DEBUG
#define EXTRA_CHECK(TYPE)                                                                                      \
    case "pushnrandom"_n.value:                                                                                \
        game_sdk::execute_action<TYPE>(eosio::name(receiver), eosio::name(code), &TYPE::push_next_random);     \
        break;                                                                                                 \
    case "pushprng"_n.value:                                                                                   \
        game_sdk::execute_action<TYPE>(eosio::name(receiver), eosio::name(code), &TYPE::push_to_prng);         \
        break;
#else
#define EXTRA_CHECK(TYPE)
#endif


 #define GAME_DISPATCH_INTERNAL( r, OP, elem ) \
    case eosio::name( BOOST_PP_STRINGIZE(elem) ).value: \
       game_sdk::execute_action<OP>( eosio::name(receiver), eosio::name(code), &OP::elem ); \
       break;

 #define GAME_DISPATCH_HELPER( TYPE,  MEMBERS ) \
    BOOST_PP_SEQ_FOR_EACH( EOSIO_DISPATCH_INTERNAL, TYPE, MEMBERS )

#define GAME_CONTRACT_CUSTOM_ACTIONS(TYPE, MEMBERS)                                                                    \
    extern "C" {                                                                                                       \
    void apply(uint64_t receiver, uint64_t code, uint64_t action) {                                                    \
        if (code == "eosio.token"_n.value && action == "transfer"_n.value) {                                           \
            game_sdk::execute_action<TYPE>(eosio::name(receiver), eosio::name(code), &TYPE::on_transfer);              \
        } else if (code == receiver) {                                                                                 \
            switch (action) {                                                                                          \
            case "init"_n.value:                                                                                       \
                game_sdk::execute_action<TYPE>(eosio::name(receiver), eosio::name(code), &TYPE::init);                 \
                break;                                                                                                 \
            case "newgame"_n.value:                                                                                    \
                game_sdk::execute_action<TYPE>(eosio::name(receiver), eosio::name(code), &TYPE::new_game);             \
                break;                                                                                                 \
            case "gameaction"_n.value:                                                                                 \
                game_sdk::execute_action<TYPE>(eosio::name(receiver), eosio::name(code), &TYPE::game_action);          \
                break;                                                                                                 \
            case "sgdicefirst"_n.value:                                                                                \
                game_sdk::execute_action<TYPE>(eosio::name(receiver), eosio::name(code), &TYPE::signidice_part_1);     \
                break;                                                                                                 \
            case "sgdicesecond"_n.value:                                                                               \
                game_sdk::execute_action<TYPE>(eosio::name(receiver), eosio::name(code), &TYPE::signidice_part_2);     \
                break;                                                                                                 \
            case "close"_n.value:                                                                                      \
                game_sdk::execute_action<TYPE>(eosio::name(receiver), eosio::name(code), &TYPE::close);                \
                break;                                                                                                 \
            GAME_DISPATCH_HELPER(TYPE, MEMBERS)                                                                        \
            EXTRA_CHECK(TYPE)                                                                                          \
            default:                                                                                                   \
                eosio::eosio_exit(1);                                                                                  \
            }                                                                                                          \
        }                                                                                                              \
        eosio::eosio_exit(0);                                                                                          \
    }                                                                                                                  \
    }

#define GAME_CONTRACT(TYPE)                                                                                            \
    GAME_CONTRACT_CUSTOM_ACTIONS(TYPE, )
