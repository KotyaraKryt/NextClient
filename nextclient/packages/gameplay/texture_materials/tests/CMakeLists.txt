set(TARGET_NAME "texture-materials-tests")

find_package(GTest CONFIG REQUIRED)

include(GoogleTest)

add_executable(${TARGET_NAME}
        TextureMaterialsTests.cpp
)

target_link_libraries(${TARGET_NAME} PRIVATE
        GTest::gtest
        GTest::gtest_main
        texture_materials::texture_materials
)

gtest_discover_tests(${TARGET_NAME})
