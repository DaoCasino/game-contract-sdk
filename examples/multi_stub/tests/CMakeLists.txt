cmake_minimum_required(VERSION 3.5)

find_package(eosio)

add_subdirectory(${GAME_SDK_PATH}/tester ${CMAKE_BINARY_DIR}/tester)

configure_file(contracts.hpp.in ${CMAKE_BINARY_DIR}/contracts.hpp)

add_game_test(multi_stub_unit_test multi_stub_tests.cpp )
target_include_directories(multi_stub_unit_test PUBLIC ${CMAKE_BINARY_DIR})
