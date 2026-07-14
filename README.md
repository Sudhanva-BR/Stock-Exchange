# Mini Stock Exchange Simulator

A high-performance, multi-symbol order matching engine written in **C++20**. This project
demonstrates production-grade exchange architecture: price-time priority matching,
O(1) order cancellation, thread-safe concurrent processing, and a CSV order replay
system — all with comprehensive unit tests and a custom latency benchmark.

---

## Features

| Milestone | Feature |
|---|---|
| M6 | TradeHistory with VWAP, symbol filter, time-range queries |
| M7 | `ExchangeCore` — multi-symbol router with stable reference semantics |
| M8 | `ThreadSafeExchangeCore` — per-symbol mutexes + async `ThreadSafeQueue<T>` |
| M9 | `CsvOrderReader` — CSV replay via `std::filesystem` |
| M10 | GoogleTest unit tests (42 test cases across 3 suites) |
| M11 | Custom `std::chrono` benchmark — throughput + min/max/p50/p99 latency |

---

## Architecture

See [docs/architecture.md](docs/architecture.md) for the full design document.

### Key classes

```
ExchangeCore
├── unordered_map<symbol, SymbolData>
│   └── SymbolData { OrderBook, TradeHistory, MatchingEngine }
│
ThreadSafeExchangeCore  (extends ExchangeCore)
├── ThreadSafeQueue<Order>          ← async producer path
├── unordered_map<symbol, mutex>    ← per-symbol locking
└── optional background thread
```

### Matching algorithm

Price-time priority (FIFO within a price level):

1. Incoming order is matched against the opposite side.
2. While remaining quantity > 0 and the spread is crossed:
   - Fill `min(incoming.remaining, resting.remaining)`.
   - Record a `Trade` at the **resting** order's price.
3. If a limit order has unfilled quantity remaining → rests on the book.
4. Market orders with leftover quantity are **discarded** (never rest).

### O(1) cancel

`OrderBook` maintains an `orderLocationIndex_: unordered_map<orderId, OrderLocation>`
where `OrderLocation` caches the `std::list<Order>::iterator`. Because `std::list`
does not invalidate iterators on insert/erase elsewhere, this gives O(1) cancel at any
position in the queue.

---

## Prerequisites

| Tool | Minimum version |
|---|---|
| CMake | 3.20 |
| C++ compiler | GCC 11 / Clang 13 / MSVC 19.29 (VS 2019 16.11) |
| Internet access | Required first time (GoogleTest downloaded via FetchContent) |

---

## Build & Run

### Command-line (any platform)

```bash
# Configure
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release

# Build everything
cmake --build build --config Release

# Run smoke-test demo
./build/MiniExchange          # Linux/macOS
build\Release\MiniExchange.exe  # Windows

# Run unit tests
ctest --test-dir build --output-on-failure --config Release

# Run benchmark
./build/benchmarks/BenchMatchingEngine
```

### CLion

1. Open the `MiniExchange/` folder — CLion detects `CMakeLists.txt` automatically.
2. **Build → Build Project** (or `Ctrl+F9`).
3. Select the `MiniExchange` run configuration and press **Run**.
4. To run tests: select `All CTest` and press **Run**.
5. To run the benchmark: select `BenchMatchingEngine` and press **Run**.

### CSV replay

```bash
# Uses config/sample_orders.csv bundled with the project.
# The main.cpp demo includes a CSV replay section.
./build/MiniExchange
```

---

## Project structure

```
MiniExchange/
├── CMakeLists.txt
├── main.cpp                          # Smoke-test / demo entry point
├── config/
│   └── sample_orders.csv             # Sample order feed
├── include/miniexchange/
│   ├── Side.h, OrderType.h, OrderStatus.h
│   ├── Order.h                       # Immutable value object
│   ├── OrderBook.h                   # Per-symbol order storage (std::list + index)
│   ├── Trade.h                       # Immutable trade record
│   ├── TradeHistory.h                # Trade log + query methods (M6)
│   ├── MatchingEngine.h              # Price-time priority matching
│   ├── ExchangeCore.h                # Multi-symbol router (M7)
│   ├── ThreadSafeQueue.h             # Header-only MPSC queue (M8)
│   ├── ThreadSafeExchangeCore.h      # Thread-safe subclass (M8)
│   └── CsvOrderReader.h              # CSV replay reader (M9)
├── src/                              # Implementations (.cpp)
├── tests/
│   ├── CMakeLists.txt                # FetchContent GoogleTest
│   ├── test_order_book.cpp
│   ├── test_matching_engine.cpp
│   └── test_cancel_modify.cpp
├── benchmarks/
│   ├── CMakeLists.txt
│   └── bench_matching_engine.cpp
└── docs/
    └── architecture.md
```

---

## Running tests

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

Expected output: **42 tests passed, 0 failed**.

---

## License

MIT — see `LICENSE` if present, otherwise consider this free for educational use.
