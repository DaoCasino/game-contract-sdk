
#add_contract(simple simple ${CMAKE_CURRENT_SOURCE_DIR}/src/simple.cpp)

add_executable(simple ${CMAKE_CURRENT_SOURCE_DIR}/src/simple.cpp)
target_compile_options( simple PUBLIC -contract game -DNOABI)

#target_compile_options(simple PUBLIC )

target_include_directories(simple
   PUBLIC
   ${CMAKE_CURRENT_SOURCE_DIR}/include
)
