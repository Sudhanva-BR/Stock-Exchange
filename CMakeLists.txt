cmake_minimum_required(VERSION 3.20)
project(MiniExchange VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---------------------------------------------------------------------------
# Core library — all reusable exchange components
# ---------------------------------------------------------------------------

add_library(miniexchange_core
    src/Order.cpp
    src/OrderBook.cpp
    src/Trade.cpp
    src/TradeHistory.cpp          # Milestone 6: expanded query methods
    src/MatchingEngine.cpp
    src/ExchangeCore.cpp          # Milestone 7: multi-symbol router
    src/ThreadSafeExchangeCore.cpp # Milestone 8: thread-safe subclass
    src/CsvOrderReader.cpp        # Milestone 9: CSV replay
)

target_include_directories(miniexchange_core
    PUBLIC ${CMAKE_SOURCE_DIR}/include
)

# ---------------------------------------------------------------------------
# Main executable (smoke test / demo)
# ---------------------------------------------------------------------------

add_executable(MiniExchange main.cpp)
target_link_libraries(MiniExchange PRIVATE miniexchange_core)

# ---------------------------------------------------------------------------
# Unit tests (Milestone 10) — GoogleTest via FetchContent
# ---------------------------------------------------------------------------

enable_testing()
add_subdirectory(tests)

# ---------------------------------------------------------------------------
# Benchmarks (Milestone 11)
# ---------------------------------------------------------------------------

add_subdirectory(benchmarks)