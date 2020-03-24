macro(add_game_contract TARGET)
    add_executable(${TARGET} ${ARGN})
    target_compile_options(${TARGET} PUBLIC -contract=game -DNOABI)
    target_link_libraries(${TARGET} game-contract-sdk)
    add_custom_command(TARGET ${TARGET} POST_BUILD
        COMMAND cp ${GAME_SDK_PATH}/sdk/files/game.abi ${CMAKE_CURRENT_BINARY_DIR}/${TARGET}.abi
    )
endmacro()

macro(load_daobet_contracts VERSION)
    message(STATUS "Downloading daobet.contracts version: ${VERSION}")
    set(DAOBET_CONTRACTS_URL "https://github.com/DaoCasino/DAOBET.contracts/releases/download/${VERSION}/contracts-${VERSION}.tar.gz")
    set(DAOBET_CONTRACTS_TAR_NAME "daobet.contracts-${VERSION}.tar.gz")
    set(DAOBET_CONTRACTS_TAR_PATH "${CMAKE_BINARY_DIR}/${DAOBET_CONTRACTS_TAR_NAME}")
    set(DAOBET_CONTRACTS_PATH "${CMAKE_BINARY_DIR}/daobet.contracts")

    file(DOWNLOAD ${DAOBET_CONTRACTS_URL} ${DAOBET_CONTRACTS_TAR_PATH}
         SHOW_PROGRESS
    )
    file(MAKE_DIRECTORY ${DAOBET_CONTRACTS_PATH})
    execute_process(COMMAND tar -xf ${DAOBET_CONTRACTS_TAR_PATH} --strip=1 -C ${DAOBET_CONTRACTS_PATH}
                WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    )
endmacro()

macro(load_platform_contracts VERSION)
    message(STATUS "Downloading DAOPlatform contracts version: ${VERSION}")
    set(PLATFORM_CONTRACTS_URL "https://github.com/DaoCasino/platform-contracts/releases/download/${VERSION}/contracts-${VERSION}.tar.gz")
    set(PLATFORM_CONTRACTS_TAR_NAME "platform-contracts-${VERSION}.tar.gz")
    set(PLATFORM_CONTRACTS_TAR_PATH "${CMAKE_BINARY_DIR}/${PLATFORM_CONTRACTS_TAR_NAME}")
    set(PLATFORM_CONTRACTS_PATH "${CMAKE_BINARY_DIR}/platform-contracts")

    file(DOWNLOAD ${PLATFORM_CONTRACTS_URL} ${PLATFORM_CONTRACTS_TAR_PATH}
         SHOW_PROGRESS
    )
    file(MAKE_DIRECTORY ${PLATFORM_CONTRACTS_PATH})
    execute_process(COMMAND tar -xf ${PLATFORM_CONTRACTS_TAR_PATH} --strip=1 -C ${PLATFORM_CONTRACTS_PATH}
                WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    )
endmacro()

macro(add_game_test TARGET)
    add_eosio_test(${TARGET} ${ARGN})
    target_compile_options(${TARGET} PUBLIC -Wno-deprecated-declarations)
    target_link_libraries(${TARGET} game-tester)
endmacro()

