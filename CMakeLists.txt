cmake_minimum_required(VERSION 3.5)

project(game_contract_sdk)

include(ExternalProject)
find_package(eosio.cdt)

set(EOSIO_CDT_VERSION_MIN "1.6.3")
set(EOSIO_CDT_VERSION_SOFT_MAX "1.6.3")

execute_process(COMMAND git describe --tags --always --dirty
    OUTPUT_VARIABLE GIT_TAG_RAW
    ERROR_QUIET
)
string(STRIP ${GIT_TAG_RAW} VERSION_FULL)


if(CMAKE_BUILD_TYPE STREQUAL "Debug")
   set(TEST_BUILD_TYPE "Debug")
   set(CMAKE_BUILD_TYPE "Release")
else()
   set(TEST_BUILD_TYPE ${CMAKE_BUILD_TYPE})
endif()


message(STATUS "Building example contracts ${VERSION_FULL}")

ExternalProject_Add(
    example_contracts_project
    SOURCE_DIR ${CMAKE_SOURCE_DIR}/examples
    BINARY_DIR ${CMAKE_BINARY_DIR}/examples
    CMAKE_ARGS
        -DCMAKE_TOOLCHAIN_FILE=${EOSIO_CDT_ROOT}/lib/cmake/eosio.cdt/EosioWasmToolchain.cmake
    PATCH_COMMAND ""
    TEST_COMMAND ""
    INSTALL_COMMAND ""
    BUILD_ALWAYS 1
)

