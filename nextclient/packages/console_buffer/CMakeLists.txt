cmake_minimum_required(VERSION 3.21)
project(console_buffer)

set(CMAKE_CXX_STANDARD 23)

add_library(${PROJECT_NAME} STATIC
    src/console_buffer.cpp
    src/selection.cpp
)
add_library(console_buffer::console_buffer ALIAS ${PROJECT_NAME})

target_include_directories(${PROJECT_NAME} PUBLIC
    include
)

if (NEXTCLIENT_BUILD_TESTS)
    add_subdirectory(tests)
endif ()
