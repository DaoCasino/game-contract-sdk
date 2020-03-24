cmake_minimum_required(VERSION 3.5)

project(dice_contract)

add_subdirectory(${GAME_SDK_PATH}/sdk ${CMAKE_BINARY_DIR}/sdk)

add_game_contract(dice src/dice.cpp)
target_include_directories(dice PUBLIC include/)
