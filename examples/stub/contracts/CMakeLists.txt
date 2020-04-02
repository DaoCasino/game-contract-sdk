cmake_minimum_required(VERSION 3.5)

project(stub_contract)

add_subdirectory(${GAME_SDK_PATH}/sdk ${CMAKE_BINARY_DIR}/sdk)

add_game_contract(stub src/stub.cpp)
target_include_directories(stub PUBLIC include/)
