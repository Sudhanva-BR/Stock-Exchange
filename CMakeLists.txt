cmake_minimum_required(VERSION 3.20)
project(MiniExchange VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

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
# Server dependencies (Crow, nlohmann_json, asio)
# ---------------------------------------------------------------------------

include(FetchContent)

# Nlohmann JSON
FetchContent_Declare(json
    URL https://github.com/nlohmann/json/releases/download/v3.11.2/json.tar.xz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(json)

# Asio (required by Crow)
FetchContent_Declare(asio
    URL https://github.com/chriskohlhoff/asio/archive/refs/tags/asio-1-28-0.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(asio)

# Crow
FetchContent_Declare(Crow
    URL https://github.com/CrowCpp/Crow/releases/download/v1.2.0/Crow-1.2.0.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
# Crow's CMake might need some options disabled
set(CROW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CROW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(Crow)

# ---------------------------------------------------------------------------
# Main executables (smoke test / demo / server)
# ---------------------------------------------------------------------------

add_executable(MiniExchange main.cpp)
target_link_libraries(MiniExchange PRIVATE miniexchange_core)

add_executable(MiniExchangeServer 
    src/server/main_server.cpp 
    src/server/ExchangeService.cpp
)
target_link_libraries(MiniExchangeServer PRIVATE miniexchange_core nlohmann_json::nlohmann_json)
if(WIN32)
    target_link_libraries(MiniExchangeServer PRIVATE ws2_32 mswsock)
else()
    target_link_libraries(MiniExchangeServer PRIVATE pthread)
endif()
target_include_directories(MiniExchangeServer PRIVATE 
    ${CMAKE_BINARY_DIR}/_deps/crow-src/include
    ${CMAKE_BINARY_DIR}/_deps/asio-src/asio/include
)

# ---------------------------------------------------------------------------
# Unit tests (Milestone 10) — GoogleTest via FetchContent
# ---------------------------------------------------------------------------

enable_testing()
add_subdirectory(tests)

# ---------------------------------------------------------------------------
# Benchmarks (Milestone 11)
# ---------------------------------------------------------------------------

add_subdirectory(benchmarks)