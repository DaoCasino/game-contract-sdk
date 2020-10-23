cmake_minimum_required(VERSION 3.5)

project(guess_number_contract)

add_subdirectory(${GAME_SDK_PATH}/sdk ${CMAKE_BINARY_DIR}/sdk)

add_game_contract(guess_number src/guess_number.cpp)
target_include_directories(guess_number PUBLIC include/)
