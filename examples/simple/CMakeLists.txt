add_subdirectory(sdk)

add_game_contract(simple ${CMAKE_CURRENT_SOURCE_DIR}/src/simple.cpp)

#add_custom_command(TARGET simple POST_BUILD COMMAND cmake -E "messsage (${CMAKE_CXX_FLAGS})")

target_include_directories(simple
   PUBLIC
   ${CMAKE_CURRENT_SOURCE_DIR}/include
)


target_link_libraries(simple game-contract-sdk)
