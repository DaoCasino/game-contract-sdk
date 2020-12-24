cmake_minimum_required(VERSION 3.5)

find_package(eosio)

add_subdirectory(${GAME_SDK_PATH}/tester ${CMAKE_BINARY_DIR}/tester)

configure_file(contracts.hpp.in ${CMAKE_BINARY_DIR}/contracts.hpp)

option(IS_DEBUG "Is Debug" OFF)
add_game_test(win_raise_lose_unit_test win_raise_lose_tests.cpp )
target_include_directories(win_raise_lose_unit_test PUBLIC ${CMAKE_BINARY_DIR})
