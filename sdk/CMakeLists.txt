cmake_minimum_required(VERSION 3.5)

project(game_contract_sdk)

set(EOSIO_WASM_OLD_BEHAVIOR "Off")
find_package(eosio.cdt)

add_subdirectory(libs)

add_library(game-contract-sdk INTERFACE)
target_include_directories(game-contract-sdk INTERFACE
    include/
)

target_link_libraries(game-contract-sdk INTERFACE platform-contracts)

macro(add_game_contract TARGET)
    add_executable(${TARGET} ${ARGN})
    target_compile_options(${TARGET} PUBLIC -contract=game -DNOABI)
#    get_target_property(BINOUTPUT ${TARGET} BINARY_DIR)
#    get_property(SDK_LOCATION TARGET game-contract-sdk PROPERTY LOCATION)
#    configure_file(${SDK_LOCATION}/game.abi ${BINOUTPUT}/${TARGET}.abi)
endmacro()
