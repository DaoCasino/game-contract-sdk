cmake_minimum_required(VERSION 3.5)

project(simple_example)

include(ExternalProject)

set(EOSIO_WASM_OLD_BEHAVIOR "Off")
find_package(eosio.cdt)

set(GAME_SDK_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../../) # Path to game SDK project root

message(STATUS "Building simple example contract")
ExternalProject_Add(
    simple_contract
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/contracts
    BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR}/contracts
    CMAKE_ARGS
        -DCMAKE_TOOLCHAIN_FILE=${EOSIO_CDT_ROOT}/lib/cmake/eosio.cdt/EosioWasmToolchain.cmake
        -DGAME_SDK_PATH=${GAME_SDK_PATH}
    PATCH_COMMAND ""
    TEST_COMMAND ""
    INSTALL_COMMAND ""
    BUILD_ALWAYS 1
)

string(REPLACE ";" "|" TEST_FRAMEWORK_PATH "${CMAKE_FRAMEWORK_PATH}")
string(REPLACE ";" "|" TEST_MODULE_PATH "${CMAKE_MODULE_PATH}")

message(STATUS "Building simple example unit tests")
ExternalProject_Add(
    simple_unit_tests
    LIST_SEPARATOR | # Use the alternate list separator
    CMAKE_ARGS
        -DBOOST_ROOT=${BOOST_ROOT}
        -DBoost_NO_SYSTEM_PATHS=${Boost_NO_SYSTEM_PATHS}
        -DCMAKE_BUILD_TYPE=${TEST_BUILD_TYPE}
        -DCMAKE_FRAMEWORK_PATH=${TEST_FRAMEWORK_PATH}
        -DCMAKE_MODULE_PATH=${TEST_MODULE_PATH}
        -DEOSIO_ROOT=${EOSIO_ROOT}
        -DLLVM_DIR=${LLVM_DIR}
        -Deosio_DIR=${CMAKE_MODULE_PATH}
        -DGAME_SDK_PATH=${GAME_SDK_PATH}
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/tests
    BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR}/tests
    BUILD_ALWAYS 1
    TEST_COMMAND   ""
    INSTALL_COMMAND ""
)
add_dependencies(simple_unit_tests simple_contract)

