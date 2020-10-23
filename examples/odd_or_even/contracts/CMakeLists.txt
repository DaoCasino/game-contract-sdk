cmake_minimum_required(VERSION 3.5)

project(odd_or_even_contract)

add_subdirectory(${GAME_SDK_PATH}/sdk ${CMAKE_BINARY_DIR}/sdk)

add_game_contract(odd_or_even src/odd_or_even.cpp)
target_include_directories(odd_or_even PUBLIC include/)
