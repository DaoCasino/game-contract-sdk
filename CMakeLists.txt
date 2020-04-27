cmake_minimum_required(VERSION 3.5)

project(game_contract_sdk)

execute_process(COMMAND git describe --tags --always --dirty
    OUTPUT_VARIABLE GIT_TAG_RAW
    ERROR_QUIET
)
string(STRIP ${GIT_TAG_RAW} VERSION_FULL)

message(STATUS "Building example contracts ${VERSION_FULL}")
option(IS_DEBUG "Is Debug" OFF)
add_subdirectory(examples)
