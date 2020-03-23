cmake_minimum_required(VERSION 3.5)

project(simple_contract)

add_subdirectory(${GAME_SDK_PATH}/sdk ${CMAKE_BINARY_DIR}/sdk)

add_game_contract(simple src/simple.cpp)
target_include_directories(simple PUBLIC include/)
