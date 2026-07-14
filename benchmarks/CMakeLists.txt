cmake_minimum_required(VERSION 3.20)

add_executable(BenchMatchingEngine bench_matching_engine.cpp)

target_link_libraries(BenchMatchingEngine
    PRIVATE miniexchange_core
)

target_include_directories(BenchMatchingEngine
    PRIVATE ${CMAKE_SOURCE_DIR}/include
)
