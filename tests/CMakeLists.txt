cmake_minimum_required(VERSION 3.20)

include(FetchContent)

FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

# Prevent GoogleTest from overriding our compiler/linker settings on Windows.
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# Helper macro to reduce boilerplate per test target.
macro(add_exchange_test TEST_NAME SOURCE_FILE)
    add_executable(${TEST_NAME} ${SOURCE_FILE})
    target_link_libraries(${TEST_NAME}
        PRIVATE
            miniexchange_core
            GTest::gtest_main
    )
    target_include_directories(${TEST_NAME}
        PRIVATE ${CMAKE_SOURCE_DIR}/include
    )
    include(GoogleTest)
    gtest_discover_tests(${TEST_NAME})
endmacro()

add_exchange_test(TestOrderBook       test_order_book.cpp)
add_exchange_test(TestMatchingEngine  test_matching_engine.cpp)
add_exchange_test(TestCancelModify    test_cancel_modify.cpp)
