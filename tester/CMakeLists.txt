cmake_minimum_required(VERSION 3.5)

set(DAOBET_CONTRACTS_VERSION "v1.0.7")
set(PLATFORM_CONTRACTS_VERSION "v1.1.2")

find_package(eosio)

include(../cmake/utils.cmake)
load_daobet_contracts(${DAOBET_CONTRACTS_VERSION})
load_platform_contracts(${PLATFORM_CONTRACTS_VERSION})

enable_testing()

configure_file(include/game_tester/contracts.hpp.in
    ${CMAKE_CURRENT_BINARY_DIR}/include/game_tester/contracts.hpp
)

add_library(game-tester INTERFACE)

target_include_directories(game-tester INTERFACE
    include/
    ${CMAKE_CURRENT_BINARY_DIR}/include/
)

