cmake_minimum_required(VERSION 3.21)
project(texture_materials)

set(CMAKE_CXX_STANDARD 23)

add_library(${PROJECT_NAME} STATIC
        src/texture_materials.cpp
)
add_library(texture_materials::texture_materials ALIAS ${PROJECT_NAME})

target_include_directories(${PROJECT_NAME} PUBLIC include)

if (NEXTCLIENT_BUILD_TESTS)
    add_subdirectory(tests)
endif ()
