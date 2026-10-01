cmake_minimum_required(VERSION 3.21)
project(imgui)

find_package(OpenGL REQUIRED)

add_library(${PROJECT_NAME} STATIC
    src/imgui.cpp
    src/imgui_demo.cpp
    src/imgui_draw.cpp
    src/imgui_tables.cpp
    src/imgui_widgets.cpp
    src/imgui_impl_opengl2.cpp
)
add_library(imgui::imgui ALIAS ${PROJECT_NAME})

# Dear ImGui's own sources and backends include each other as "imgui.h"
target_include_directories(${PROJECT_NAME} PUBLIC
    include
    include/imgui
)

target_link_libraries(${PROJECT_NAME} PUBLIC
    OpenGL::GL
)
