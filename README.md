# Mini Stock Exchange & HFT Terminal

A production-grade, high-performance multi-symbol order matching engine written in **C++20**, paired with an institutional-style **React + Vite HFT Terminal Dashboard**.

This project demonstrates low-latency exchange architecture: price-time priority matching, O(1) order cancellation, thread-safe concurrent processing, WebSocket/REST API integration via Crow C++, and custom SVG visualization components for real-time order book, trades, candlestick analytics, and latency telemetry.

---

## Key Features

### ⚡ C++20 Core Matching Engine
- **High Throughput & Low Latency**: Processes ~1.2M orders/sec with p99 latency < 1.2µs.
- **Price-Time Priority (FIFO)**: Strict matching algorithm for Limit and Market orders.
- **O(1) Order Cancellation**: Maintained via `std::list` iterator caching indexed in an `unordered_map`.
- **Thread-Safe Architecture**: Per-symbol fine-grained mutexes with lock-free MPSC queues (`ThreadSafeExchangeCore`).
- **REST & WebSocket API**: Built with Crow C++ framework for sub-millisecond data broadcasting.

### 📊 Institutional HFT Terminal Frontend
- **Watchlist Ticker Strip**: Multi-symbol ticker strip with live price tick flash animations and mini inline sparklines.
- **Real-Time Candlestick Chart**: Custom SVG 1-second OHLC candlestick chart with toggleable **VWAP (Volume-Weighted Average Price)** overlay.
- **Order Book Depth View**: Side-by-side bids & asks depth table with animated quantity change flash overlays.
- **Order Book Imbalance (OBI) Panel**: Dynamic gauge meter showing buy/sell liquidity pressure alongside a 60-second historical trend chart.
- **Performance & Latency Monitor**: Real-time telemetry tracking REST p50/p99 round-trip latencies, Orders/sec (OPS), and Trades/sec (TPS).
- **Market Simulator**: Embedded high-frequency order flow simulator to stress-test matching & UI rendering.

---

## System Architecture

```
                               ┌────────────────────────────────────────┐
                               │     React + Vite HFT Terminal UI      │
                               │        (http://localhost:5173)         │
                               └───────────────────┬────────────────────┘
                                                   │ WebSocket / REST API
                                                   ▼
┌────────────────────────────────────────────────────────────────────────────────────────┐
│ C++20 Crow Web Server (port 8080)                                                      │
│                                                                                        │
│   ┌────────────────────────────────────────────────────────────────────────────────┐   │
│   │ ThreadSafeExchangeCore                                                         │   │
│   │ ├── ThreadSafeQueue<Order>        (Lock-free MPSC async queue)                   │   │
│   │ └── SymbolRouter (unordered_map)                                                │   │
│   │     ├── SymbolData [AAPL] ──► { OrderBook, MatchingEngine, TradeHistory }      │   │
│   │     ├── SymbolData [GOOGL] ─► { OrderBook, MatchingEngine, TradeHistory }      │   │
│   │     └── SymbolData [MSFT] ──► { OrderBook, MatchingEngine, TradeHistory }      │   │
│   └────────────────────────────────────────────────────────────────────────────────┘   │
└────────────────────────────────────────────────────────────────────────────────────────┘
```

---

## How to Run

### 1. Start the C++ Exchange Server

Run the pre-compiled C++ server binary from the root directory:

**Windows (PowerShell):**
```powershell
.\cmake-build-debug\MiniExchangeServer.exe 8080
```

**Linux / macOS (or manual build):**
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
./build/MiniExchangeServer 8080
```

### 2. Start the React Frontend

In a separate terminal window:

```bash
cd frontend
npm install
npm run dev
```

### 3. Access the Terminal

Open your browser and navigate to **[http://localhost:5173](http://localhost:5173)**.
- Ensure the connection badge in the top-right indicates **Live**.
- Click **Sim ON** in the top navigation bar to launch the market simulator and stream live order book depth, candlestick ticks, OBI fluctuations, and latency metrics!

---

## Project Structure

```
Mini Exchange/
├── include/miniexchange/           # C++ Core Headers
│   ├── Order.h                     # Immutable order value object
│   ├── OrderBook.h                 # Price-time priority book with O(1) cancel index
│   ├── MatchingEngine.h            # Core matching logic
│   ├── ExchangeCore.h              # Multi-symbol engine router
│   ├── ThreadSafeExchangeCore.h    # Thread-safe concurrent subclass
│   ├── Trade.h & TradeHistory.h    # Execution logging & VWAP calculations
│   └── CsvOrderReader.h            # CSV order replay reader
├── src/                            # C++ Implementation source files
├── cmake-build-debug/              # Pre-compiled C++ binaries & runtime DLLs
│   └── MiniExchangeServer.exe      # Crow C++ Web & WebSocket Server executable
├── frontend/                       # React + Vite HFT Web Terminal
│   ├── src/
│   │   ├── components/
│   │   │   ├── WatchlistStrip.jsx  # Multi-symbol ticker with tick flashes & sparklines
│   │   │   ├── CandleChart.jsx     # Pure SVG real-time Candlestick + VWAP chart
│   │   │   ├── OrderBookDepth.jsx  # Level-2 depth view with flash animations
│   │   │   ├── OBIPanel.jsx        # Order Book Imbalance gauge & history graph
│   │   │   ├── LatencyDashboard.jsx# Real-time p50/p99 & OPS/TPS telemetry
│   │   │   ├── OrderEntryForm.jsx  # Order execution interface
│   │   │   └── TradeTape.jsx       # Real-time execution stream
│   │   ├── App.jsx                 # Central state orchestrator
│   │   └── index.css               # Dark-mode terminal design system
├── tests/                          # GoogleTest unit test suites (42 test cases)
├── benchmarks/                     # Custom benchmark suite measuring throughput & latency
└── start.bat                       # Windows automated launcher script
```

---

## Running Unit Tests & Benchmarks

```bash
# Build with Debug/Release flags
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build

# Run unit tests (42 passed)
ctest --test-dir build --output-on-failure

# Run C++ latency benchmark
./build/benchmarks/BenchMatchingEngine
```

---

## License

MIT License — free for educational and personal project use.
