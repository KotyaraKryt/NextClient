set(TARGET_NAME "console_buffer-tests")

find_package(GTest CONFIG REQUIRED)

include(GoogleTest)

add_executable(${TARGET_NAME}
        ConsoleBufferTests.cpp
        SelectionTests.cpp
        CommandHistoryTests.cpp
        CompletionTests.cpp
)

target_link_libraries(${TARGET_NAME} PRIVATE
        GTest::gtest
        GTest::gtest_main
        console_buffer::console_buffer
)

gtest_discover_tests(${TARGET_NAME})
