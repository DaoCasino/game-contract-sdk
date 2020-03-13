cmake_minimum_required(VERSION 3.5)

project(example_contracts)

set(EOSIO_WASM_OLD_BEHAVIOR "Off")
find_package(eosio.cdt)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/../sdk/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../libs/platform-contracts/contracts/platform/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../libs/platform-contracts/contracts/events/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../libs/platform-contracts/contracts/casino/include
)

add_subdirectory(simple)
