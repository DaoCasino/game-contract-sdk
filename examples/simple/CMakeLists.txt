cmake_minimum_required(VERSION 3.5)

project(example_contracts)

set(EOSIO_WASM_OLD_BEHAVIOR "Off")
find_package(eosio.cdt)

add_subdirectory(sdk)

add_game_contract(simple ${CMAKE_CURRENT_SOURCE_DIR}/src/simple.cpp)

target_include_directories(simple
   PUBLIC
   ${CMAKE_CURRENT_SOURCE_DIR}/include
)


target_link_libraries(simple game-contract-sdk)
