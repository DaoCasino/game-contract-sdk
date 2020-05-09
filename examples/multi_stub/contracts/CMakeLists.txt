cmake_minimum_required(VERSION 3.5)

project(multi_stub_contract)

add_subdirectory(${GAME_SDK_PATH}/sdk ${CMAKE_BINARY_DIR}/sdk)

add_game_contract(multi_stub src/multi_stub.cpp)
target_include_directories(multi_stub PUBLIC include/)
