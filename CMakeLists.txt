cmake_minimum_required(VERSION 3.16)
project(THZ-JS2CPP LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

add_compile_options(-Wall -Wextra)

add_executable(thzc
    main.cpp
    src/lexer.cpp
    src/parser.cpp
    src/irbuilder.cpp
    src/codegen.cpp
)

target_include_directories(thzc PRIVATE include)

enable_testing()

add_test(
    NAME transcompile_hello
    COMMAND ${CMAKE_COMMAND} -E env $<TARGET_FILE:thzc> ${CMAKE_SOURCE_DIR}/tests/cases/hello.js
)