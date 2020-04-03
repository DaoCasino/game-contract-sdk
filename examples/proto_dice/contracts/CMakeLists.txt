cmake_minimum_required(VERSION 3.5)

project(proto_dice_contract)

add_subdirectory(${GAME_SDK_PATH}/sdk ${CMAKE_BINARY_DIR}/sdk)

add_game_contract(proto_dice src/proto_dice.cpp)
target_include_directories(proto_dice PUBLIC include/)
