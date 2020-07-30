cmake_minimum_required(VERSION 3.5)

project(game_contract_sdk)

set(EOSIO_CDT_VERSION_MIN "1.6.3")
set(EOSIO_CDT_VERSION_SOFT_MAX "1.6.3")
set(EOSIO_WASM_OLD_BEHAVIOR "Off")

find_package(eosio.cdt)

add_subdirectory(libs)

add_library(game-contract-sdk INTERFACE)
target_include_directories(game-contract-sdk INTERFACE
    include/
)

target_link_libraries(game-contract-sdk
    INTERFACE intx::intx
    INTERFACE platform-contracts
)

include(../cmake/utils.cmake)
