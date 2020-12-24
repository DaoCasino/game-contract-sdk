cmake_minimum_required(VERSION 3.5)

project(win_raise_lose_contract)

add_subdirectory(${GAME_SDK_PATH}/sdk ${CMAKE_BINARY_DIR}/sdk)

add_game_contract(win_raise_lose src/win_raise_lose.cpp)
option(IS_DEBUG "Is debug" OFF)
target_include_directories(win_raise_lose PUBLIC include/)
