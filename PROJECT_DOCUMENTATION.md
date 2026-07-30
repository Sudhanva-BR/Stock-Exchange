# Mini Exchange — Complete Project Documentation

> A limit order book matching engine built in C++20, paired with a real-time React trading terminal.
> This document explains **every part** of the project — what it does, how it works, and why each decision was made — in simple, plain English.

---

## Table of Contents

1. [What Is This Project?](#1-what-is-this-project)
2. [What Is a Stock Exchange?](#2-what-is-a-stock-exchange)
3. [What Is a Limit Order Book?](#3-what-is-a-limit-order-book)
4. [How Does Order Matching Work?](#4-how-does-order-matching-work)
5. [Project Directory Layout](#5-project-directory-layout)
6. [Architecture Overview](#6-architecture-overview)
7. [The Order — The Basic Unit of Everything](#7-the-order--the-basic-unit-of-everything)
8. [The Order Book — Where Orders Live](#8-the-order-book--where-orders-live)
9. [The Matching Engine — The Brain](#9-the-matching-engine--the-brain)
10. [The Trade — Proof That a Deal Happened](#10-the-trade--proof-that-a-deal-happened)
11. [Trade History — The Record Keeper](#11-trade-history--the-record-keeper)
12. [Exchange Core — The Multi-Symbol Router](#12-exchange-core--the-multi-symbol-router)
13. [Thread Safety — Making It Work with Multiple Users](#13-thread-safety--making-it-work-with-multiple-users)
14. [The Web Server — Connecting to the Outside World](#14-the-web-server--connecting-to-the-outside-world)
15. [The Frontend — The Trading Terminal](#15-the-frontend--the-trading-terminal)
16. [CSV Order Reader — Replaying Orders from a File](#16-csv-order-reader--replaying-orders-from-a-file)
17. [Testing — Proving Everything Works](#17-testing--proving-everything-works)
18. [Benchmarking — Measuring Speed](#18-benchmarking--measuring-speed)
19. [Build System — How to Compile It All](#19-build-system--how-to-compile-it-all)
20. [How to Run the Project](#20-how-to-run-the-project)
21. [End-to-End Data Flow — Following One Order Through the Entire System](#21-end-to-end-data-flow--following-one-order-through-the-entire-system)
22. [Performance Characteristics](#22-performance-characteristics)
23. [Key Design Decisions and Why](#23-key-design-decisions-and-why)
24. [Glossary](#24-glossary)

---

## 1. What Is This Project?

This project is a **stock exchange simulator**. It does the same thing that the New York Stock Exchange (NYSE) or NASDAQ does at their core: it takes buy and sell orders from traders, figures out when a buyer's price meets a seller's price, and executes a trade between them.

The system has two main parts:

1. **The C++ Engine** (the backend) — This is the brain. It receives orders, stores them, matches buyers with sellers, and records every trade. It is written in C++20 for speed.

2. **The React Frontend** (the trading terminal) — This is the face. It shows traders a live dashboard with price charts, order book depth, trade history, and lets them submit new orders. It communicates with the C++ engine over a network connection.

Think of it like a restaurant:
- The **kitchen** (C++ engine) is where the real work happens — cooking food (matching orders).
- The **dining room** (React frontend) is where customers sit, read the menu (see prices), and place orders.
- The **waiter** (web server) carries orders from the dining room to the kitchen and brings back the finished plates (trade confirmations).

---

## 2. What Is a Stock Exchange?

A stock exchange is a place where people buy and sell shares of companies. When you hear "Apple stock went up 3% today," that movement happened because of millions of buy and sell orders being matched on an exchange.

Here is how it works in simple terms:

1. **Alice** wants to buy 100 shares of Apple at $150 each. She sends a buy order to the exchange.
2. **Bob** wants to sell 100 shares of Apple at $150 each. He sends a sell order to the exchange.
3. The exchange sees that Alice's buy price ($150) meets Bob's sell price ($150). **Match!**
4. The exchange executes the trade: Bob's 100 shares go to Alice, and Alice's $15,000 goes to Bob.
5. Both Alice and Bob receive a confirmation message (called an **execution report**).

The exchange does not buy or sell anything itself. It is just the middleman that brings buyers and sellers together.

---

## 3. What Is a Limit Order Book?

The **limit order book** (often just called "the book") is the central data structure of any exchange. It is a list of all the buy orders and all the sell orders that are currently waiting to be matched.

The book has two sides:

```
                    LIMIT ORDER BOOK FOR AAPL
    ─────────────────────────────────────────────────────
    BUY SIDE (Bids)              │  SELL SIDE (Asks)
    Buyers waiting to buy        │  Sellers waiting to sell
    ─────────────────────────────│───────────────────────
    $149.00 — 200 shares         │  $150.75 — 30 shares
    $148.50 — 150 shares         │  $151.00 — 50 shares
    $148.00 — 300 shares         │  $152.00 — 100 shares
    ─────────────────────────────────────────────────────
            ▲ Best Bid               ▲ Best Ask
           $149.00                  $150.75

              Spread = $150.75 - $149.00 = $1.75
```

### Key Terms

- **Bid**: A buy order. "I bid $149 for 200 shares" means "I want to buy 200 shares and I am willing to pay up to $149 each."
- **Ask**: A sell order. "I ask $150.75 for 30 shares" means "I want to sell 30 shares and I won't accept less than $150.75 each."
- **Best Bid**: The highest price any buyer is willing to pay. This is always the most attractive buy order.
- **Best Ask**: The lowest price any seller is willing to accept. This is always the most attractive sell order.
- **Spread**: The gap between the best bid and the best ask. When the spread closes to zero (a buyer's price meets a seller's price), a trade happens.

### Two Types of Orders

**Limit Order**: "Buy 100 shares at $150 or less." This order specifies a maximum price the buyer will pay (or a minimum price the seller will accept). If the order can't be matched right away, it sits on the book and waits.

**Market Order**: "Buy 100 shares at whatever the current price is." This order says "match me immediately at the best available price — I don't care about the exact price." Market orders never sit on the book. If there is nothing to match against, the market order is simply discarded.

---

## 4. How Does Order Matching Work?

The matching engine uses a rule called **price-time priority**. This is a two-part rule:

### Rule 1: Price Priority (Better Price Goes First)

Among all buyers, the one offering the **highest** price gets matched first. Among all sellers, the one asking the **lowest** price gets matched first.

Why? Because the buyer offering $151 is more attractive to sellers than the buyer offering $149. And the seller asking $99 is more attractive to buyers than the seller asking $101.

### Rule 2: Time Priority (First Come, First Served)

Among all orders at the **same** price, the order that arrived **earliest** gets matched first. This is the FIFO (First In, First Out) rule.

Why? Fairness. If two sellers both want $100, the one who submitted their order first should be served first.

### A Concrete Matching Example

Let's walk through a complete matching scenario step by step:

**Starting state of the book:**

```
    SELL SIDE (Asks):
      $101.00:  Order A (100 shares, arrived 10:00:01)  ← oldest, goes first
                Order B (50 shares,  arrived 10:00:02)

    BUY SIDE (Bids):
      $99.00:   Order C (200 shares)

    Best ask: $101.00    Best bid: $99.00    Spread: $2.00
    No match possible yet — asks are above bids.
```

**Incoming order: Buy 120 shares at $101.00**

**Step 1:** Check if prices cross. Best ask is $101.00. Incoming buy is $101.00. Yes, $101.00 ≥ $101.00. Match begins.

**Step 2:** The oldest order at $101.00 is Order A (100 shares). Fill the smaller of 120 and 100 = **100 shares**. Order A is fully consumed and removed from the book. Incoming buy still needs 120 - 100 = **20 more shares**.

**Step 3:** Next order at $101.00 is Order B (50 shares). Prices still cross. Fill the smaller of 20 and 50 = **20 shares**. Order B is partially filled — it stays on the book with 30 shares remaining. Incoming buy is now fully filled (0 shares remaining).

**Output — two execution reports:**
```
    Trade 1:  100 shares @ $101.00  (between incoming buy and Order A)
    Trade 2:   20 shares @ $101.00  (between incoming buy and Order B)
```

**Final state of the book:**
```
    SELL SIDE (Asks):
      $101.00:  Order B (30 shares remaining)

    BUY SIDE (Bids):
      $99.00:   Order C (200 shares, unchanged)
```

---

## 5. Project Directory Layout

Here is every file and folder in the project, with an explanation of what each one does:

```
Mini Exchange/
│
├── CMakeLists.txt                    Build configuration (tells the compiler what to compile)
├── main.cpp                          Simple demo program that tests the engine manually
├── start.bat                         Windows script that launches both server and frontend
│
├── include/miniexchange/             C++ header files (declarations / blueprints)
│   ├── Order.h                       Defines what an Order looks like
│   ├── OrderBook.h                   Defines the Order Book data structure
│   ├── MatchingEngine.h              Defines the matching logic interface
│   ├── ExchangeCore.h                Defines the multi-symbol router
│   ├── ThreadSafeExchangeCore.h      Defines the thread-safe version of the router
│   ├── Trade.h                       Defines what a Trade looks like
│   ├── TradeHistory.h                Defines the trade log with query support
│   ├── CsvOrderReader.h             Defines the CSV file reader
│   ├── ExchangeService.h            Defines the web API service layer
│   ├── ThreadSafeQueue.h            Defines a thread-safe producer-consumer queue
│   ├── Side.h                        Enum: Buy or Sell
│   ├── OrderType.h                   Enum: Limit or Market
│   └── OrderStatus.h                 Enum: New, PartiallyFilled, Filled, Cancelled
│
├── src/                              C++ source files (implementations)
│   ├── Order.cpp                     Order creation, validation, filling, cancellation
│   ├── OrderBook.cpp                 Adding, cancelling, filling orders in the book
│   ├── MatchingEngine.cpp            The core matching loop
│   ├── ExchangeCore.cpp              Multi-symbol routing logic
│   ├── ThreadSafeExchangeCore.cpp    Thread-safe wrapper with per-symbol locking
│   ├── Trade.cpp                     Trade data accessors
│   ├── TradeHistory.cpp              Trade recording and queries (VWAP, filtering)
│   ├── CsvOrderReader.cpp            CSV parsing and replay logic
│   └── server/
│       ├── main_server.cpp           Web server entry point (REST + WebSocket routes)
│       └── ExchangeService.cpp       Business logic between web API and engine core
│
├── frontend/                         React + Vite web application
│   ├── src/
│   │   ├── App.jsx                   Main application component (state orchestrator)
│   │   ├── index.css                 Dark-mode terminal design system
│   │   └── components/
│   │       ├── WatchlistStrip.jsx    Multi-symbol ticker with live price flashes
│   │       ├── CandleChart.jsx       SVG candlestick chart with VWAP overlay
│   │       ├── OrderBookDepth.jsx    Bid/Ask depth table with flash animations
│   │       ├── OBIPanel.jsx          Order Book Imbalance gauge and trend chart
│   │       ├── LatencyDashboard.jsx  Real-time p50/p99 latency and throughput metrics
│   │       ├── OrderEntryForm.jsx    Order submission form
│   │       ├── TradeTape.jsx         Real-time trade execution stream
│   │       ├── MemoryPoolPanel.jsx   Memory pool visualization
│   │       ├── NodeDataPanel.jsx     Raw order node data viewer
│   │       ├── SymbolSelector.jsx    Symbol dropdown selector
│   │       └── AlertToasts.jsx       Notification toast messages
│   ├── package.json                  Node.js dependencies
│   └── vite.config.js                Vite development server configuration
│
├── tests/                            Automated test suites
│   ├── test_order_book.cpp           Tests for the OrderBook data structure
│   ├── test_matching_engine.cpp      Tests for the matching logic
│   ├── test_cancel_modify.cpp        Tests for cancel, modify, and edge cases
│   └── CMakeLists.txt                Test build configuration
│
├── benchmarks/                       Performance measurement
│   ├── bench_matching_engine.cpp     Custom latency and throughput benchmark
│   └── CMakeLists.txt                Benchmark build configuration
│
├── config/
│   └── sample_orders.csv             Sample order data for testing
│
└── docs/
    └── architecture.md               Technical architecture notes
```

---

## 6. Architecture Overview

The system is built in layers. Each layer has one job and one job only. This makes the code easier to understand, test, and change.

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                         FRONTEND (React + Vite)                            │
│                     http://localhost:5173                                   │
│                                                                             │
│   ┌──────────────┐ ┌──────────────┐ ┌──────────────┐ ┌──────────────┐     │
│   │  Watchlist    │ │  Candle      │ │  Order Book  │ │  OBI         │     │
│   │  Strip       │ │  Chart       │ │  Depth       │ │  Panel       │     │
│   └──────────────┘ └──────────────┘ └──────────────┘ └──────────────┘     │
│   ┌──────────────┐ ┌──────────────┐ ┌──────────────┐ ┌──────────────┐     │
│   │  Order       │ │  Trade       │ │  Latency     │ │  Memory      │     │
│   │  Entry Form  │ │  Tape        │ │  Dashboard   │ │  Pool Panel  │     │
│   └──────────────┘ └──────────────┘ └──────────────┘ └──────────────┘     │
└──────────────────────────────┬──────────────────────────────────────────────┘
                               │
                    REST API (HTTP) + WebSocket (real-time)
                               │
┌──────────────────────────────▼──────────────────────────────────────────────┐
│                    WEB SERVER (Crow C++ Framework)                          │
│                     http://localhost:8080                                   │
│                                                                             │
│   REST Endpoints:                     WebSocket:                            │
│   POST /api/orders        (submit)    /ws (real-time trade + book updates)  │
│   DELETE /api/orders/:s/:id (cancel)                                        │
│   PUT /api/orders/:s/:id  (modify)                                          │
│   GET /api/orderbook/:s   (snapshot)                                        │
│   GET /api/trades/:s      (history)                                         │
│   GET /api/symbols        (list)                                            │
│   GET /api/debug/orderbook/:s (debug)                                       │
└──────────────────────────────┬──────────────────────────────────────────────┘
                               │
                     ExchangeService (thread-safe wrapper)
                               │
┌──────────────────────────────▼──────────────────────────────────────────────┐
│                        EXCHANGE CORE                                        │
│                                                                             │
│   Routes each order to the correct symbol's engine:                         │
│                                                                             │
│   ┌───────────────────────────────────────────────────────────────┐         │
│   │  Symbol: "AAPL"                                               │         │
│   │  ┌─────────────┐  ┌────────────────┐  ┌──────────────┐      │         │
│   │  │  OrderBook   │  │ MatchingEngine │  │ TradeHistory │      │         │
│   │  │  (bids/asks) │◄─│ (matching      │──│ (trade log)  │      │         │
│   │  │              │  │  logic)        │  │              │      │         │
│   │  └─────────────┘  └────────────────┘  └──────────────┘      │         │
│   └───────────────────────────────────────────────────────────────┘         │
│   ┌───────────────────────────────────────────────────────────────┐         │
│   │  Symbol: "GOOGL"                                              │         │
│   │  ┌─────────────┐  ┌────────────────┐  ┌──────────────┐      │         │
│   │  │  OrderBook   │  │ MatchingEngine │  │ TradeHistory │      │         │
│   │  └─────────────┘  └────────────────┘  └──────────────┘      │         │
│   └───────────────────────────────────────────────────────────────┘         │
│   ... (one set per symbol)                                                  │
└─────────────────────────────────────────────────────────────────────────────┘
```

**The key insight:** Each stock symbol (AAPL, GOOGL, MSFT, etc.) gets its own completely independent set of three objects — an `OrderBook`, a `MatchingEngine`, and a `TradeHistory`. Apple orders never touch Google's order book, and vice versa.

---

## 7. The Order — The Basic Unit of Everything

An **Order** is the fundamental piece of data in the system. Every order has these properties:

| Property | Type | Description |
|---|---|---|
| `id` | `uint64_t` | A unique number identifying this order (1, 2, 3, ...) |
| `symbol` | `string` | Which stock this order is for ("AAPL", "GOOGL", etc.) |
| `side` | `Side` enum | `Buy` or `Sell` |
| `type` | `OrderType` enum | `Limit` (has a price) or `Market` (no price, match immediately) |
| `price` | `optional<double>` | The price per share (only for limit orders; empty for market orders) |
| `quantity` | `uint32_t` | How many shares the trader originally wanted |
| `remainingQuantity` | `uint32_t` | How many shares are still unfilled |
| `status` | `OrderStatus` enum | `New`, `PartiallyFilled`, `Filled`, or `Cancelled` |
| `timestamp` | `TimePoint` | Exactly when this order was created |

### How Orders Are Created

Orders are created using **factory functions** rather than calling the constructor directly. This is a design pattern that lets us enforce rules at creation time:

```cpp
// Create a limit order: "Buy 100 shares of AAPL at $150.75"
Order order = Order::createLimitOrder(
    1,          // order ID
    "AAPL",     // symbol
    Side::Buy,  // side
    150.75,     // price per share
    100         // quantity (number of shares)
);

// Create a market order: "Sell 50 shares of AAPL at whatever price is available"
Order order = Order::createMarketOrder(
    2,           // order ID
    "AAPL",      // symbol
    Side::Sell,  // side
    50           // quantity
);
```

The factory functions check for invalid inputs and throw errors:
- Price must be positive (no one sells at $0 or -$5)
- Quantity must be at least 1 (you can't buy 0 shares)

### What Can Happen to an Order After Creation

Once an order is created, only **two things** can change it:

1. **`fill(quantity)`** — The matching engine matched some shares. The order's remaining quantity goes down, and its status changes to `PartiallyFilled` or `Filled`.
2. **`cancel()`** — The trader decided to withdraw the order. Its status changes to `Cancelled`.

Everything else about the order (its ID, symbol, side, original quantity, and price) is **permanent** and never changes. This immutability-by-design prevents bugs where one part of the system accidentally modifies data that another part depends on.

### Why Price Is `optional<double>`

Market orders have no price — the trader says "give me the best available price." Using `std::optional<double>` (which can be either a number or "empty") lets us represent this cleanly:

```cpp
if (order.getPrice().has_value()) {
    // This is a limit order. Price is order.getPrice().value()
} else {
    // This is a market order. No price specified.
}
```

The alternative — using a magic number like `price = -1` to mean "no price" — is error-prone. What if someone forgets to check for -1? `optional` makes the "no value" case explicit and impossible to ignore.

---

## 8. The Order Book — Where Orders Live

The `OrderBook` is the core data structure that stores all resting orders for a single stock symbol. It answers questions like:
- "What is the best bid right now?"
- "What is the best ask right now?"
- "Remove order #42 from wherever it is in the book."

### Internal Data Structures

The order book uses three data structures working together:

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                          OrderBook for "AAPL"                              │
│                                                                             │
│  BIDS (sorted highest-price first):                                         │
│  ┌─────────────────────────────────────────────────────────────────┐        │
│  │  std::map<double, std::list<Order>, std::greater<double>>       │        │
│  │                                                                   │        │
│  │  $149.00 ──► [ Order(id=4, 100 shares) → Order(id=7, 50 shares)]│        │
│  │  $148.50 ──► [ Order(id=9, 200 shares) ]                        │        │
│  │  $148.00 ──► [ Order(id=2, 300 shares) ]                        │        │
│  └─────────────────────────────────────────────────────────────────┘        │
│                                                                             │
│  ASKS (sorted lowest-price first):                                          │
│  ┌─────────────────────────────────────────────────────────────────┐        │
│  │  std::map<double, std::list<Order>>                              │        │
│  │                                                                   │        │
│  │  $150.75 ──► [ Order(id=1, 30 shares) ]                         │        │
│  │  $151.00 ──► [ Order(id=3, 50 shares) → Order(id=6, 20 shares)]│        │
│  │  $152.00 ──► [ Order(id=5, 100 shares)]                        │        │
│  └─────────────────────────────────────────────────────────────────┘        │
│                                                                             │
│  ORDER LOCATION INDEX (for fast cancellation):                              │
│  ┌─────────────────────────────────────────────────────────────────┐        │
│  │  std::unordered_map<orderId, OrderLocation>                      │        │
│  │                                                                   │        │
│  │  orderId=1 ──► { side=Sell, price=150.75, iterator→(Order #1) } │        │
│  │  orderId=2 ──► { side=Buy,  price=148.00, iterator→(Order #2) } │        │
│  │  orderId=3 ──► { side=Sell, price=151.00, iterator→(Order #3) } │        │
│  │  ...                                                              │        │
│  └─────────────────────────────────────────────────────────────────┘        │
└─────────────────────────────────────────────────────────────────────────────┘
```

Let's understand each one:

### Data Structure 1: `std::map` for Price Levels

A `std::map` is like a sorted dictionary. The **key** is the price, and the **value** is a list of all orders at that price.

- For **bids** (buy orders), we use `std::greater<double>` which sorts prices from **highest to lowest**. This means `bids_.begin()` (the first entry) always gives us the best bid — the highest price any buyer is willing to pay.

- For **asks** (sell orders), we use the default sort which goes from **lowest to highest**. This means `asks_.begin()` always gives us the best ask — the lowest price any seller is willing to accept.

Why `std::map`? Because it automatically keeps prices sorted. When a new price level appears (say, someone submits a bid at $149.25 for the first time), the map inserts it in the correct sorted position. When the last order at a price is removed, we delete that price level from the map. Finding the best price is always just reading the first element.

### Data Structure 2: `std::list` for Orders at the Same Price

Within each price level, orders are stored in a `std::list` (a doubly-linked list). The first order in the list arrived earliest (time priority), and the last order arrived most recently.

Why `std::list` and not `std::vector`? Because of **iterator stability**. When we cancel an order in the middle of the list, `std::list` removes just that one element without moving anything else. The "pointers" (iterators) to all the other orders remain valid. With `std::vector`, removing an element from the middle would shift all subsequent elements, invalidating their positions.

This is critical for the third data structure:

### Data Structure 3: `unordered_map` for the Order Location Index

When a trader wants to cancel order #42, we need to find it instantly — we can't scan through every price level and every queue to find it. That would be far too slow.

The `orderLocationIndex_` is a hash map that stores, for each order ID, exactly where that order lives:

```cpp
struct OrderLocation {
    Side side;                       // Which side (Buy or Sell)?
    double price;                    // Which price level?
    std::list<Order>::iterator iter;  // Pointer directly to the order in the list
};
```

This gives us **O(1) cancellation** — constant time, no matter how many orders are in the book:

1. Look up the order ID in the hash map → get its location.
2. Use the stored iterator to erase the order from the `std::list` in O(1) time.
3. If that was the last order at that price level, remove the price level from the `std::map`.
4. Remove the entry from the hash map.

Done. No searching required.

### OrderBook Operations — How They Work

**Adding an order:**
```
addOrder(order):
    1. Read the order's price and side
    2. If side is Buy:
         - Find (or create) the price level in bids_
         - Append the order to the end of that level's list (newest = lowest time priority)
         - Store its location in orderLocationIndex_
    3. If side is Sell:
         - Same thing, but in asks_
```

**Cancelling an order:**
```
cancelOrder(orderId):
    1. Look up orderId in orderLocationIndex_
    2. If not found, return false (order doesn't exist)
    3. Use the stored iterator to erase the order from its list
    4. If the list at that price level is now empty, remove the entire price level
    5. Remove the entry from orderLocationIndex_
    6. Return true
```

**Filling the best order (called by the matching engine):**
```
fillBestAsk(quantity):
    1. Get the first (best) price level from asks_
    2. Get the first (oldest) order from that level's list
    3. Call order.fill(quantity) — reduces its remaining quantity
    4. If remaining quantity is now 0:
         - Remove the order from the list
         - Remove its entry from orderLocationIndex_
         - If the list is now empty, remove the price level
```

---

## 9. The Matching Engine — The Brain

The `MatchingEngine` is where the magic happens. It takes an incoming order and tries to match it against existing orders on the opposite side of the book.

### The Matching Algorithm

Here is the complete algorithm in simple English:

```
submitOrder(incomingOrder):

    IF incoming order is a BUY:
        Try to match it against existing SELL orders (asks)
    ELSE:
        Try to match it against existing BUY orders (bids)

    AFTER matching is done:
        IF the incoming order is a LIMIT order AND still has shares remaining:
            Add it to the book (it becomes a resting order)
        ELSE IF it's a MARKET order with shares remaining:
            Discard it (market orders never rest on the book)
```

The matching loop for a buy order looks like this:

```
matchAgainstAsks(incomingBuyOrder):

    WHILE incoming buy still has shares remaining:

        1. Peek at the best ask (lowest-priced sell order)
        2. If no sell orders exist, stop — nothing to match against

        3. Price crossing check:
           - If incoming is a MARKET order: always crosses (skip price check)
           - If incoming is a LIMIT order: check if buy price >= ask price
             - If buy price < ask price: stop — prices don't cross

        4. Calculate fill quantity = MIN(incoming remaining, resting remaining)

        5. Fill both sides:
           - incoming.fill(fillQty)      — reduce incoming's remaining
           - book.fillBestAsk(fillQty)   — reduce resting's remaining (or remove it)

        6. Record the trade:
           - Create a Trade object with the fill details
           - Price = the RESTING order's price (not the incoming order's price)
           - Store it in TradeHistory

        7. Go back to step 1 (there might be more matches at the next price level)
```

### Why the Trade Price Is the Resting Order's Price

When a buyer willing to pay $105 matches against a seller asking $100, the trade happens at $100 (the resting order's price), not $105. This is how real exchanges work — the **price improvement** goes to the aggressor (the incoming order). The buyer gets a better deal than they expected.

### Actual Code for the Matching Loop

```cpp
void MatchingEngine::matchAgainstAsks(Order& incomingBuyOrder) {
    while (incomingBuyOrder.getRemainingQuantity() > 0) {
        // Step 1: Look at the best sell order
        auto topAsk = book_.peekBestAsk();
        if (!topAsk.has_value()) {
            break;  // No sell orders left
        }

        // Step 3: Check if prices cross
        const bool isMarketOrder = !incomingBuyOrder.getPrice().has_value();
        if (!isMarketOrder && incomingBuyOrder.getPrice().value() < topAsk->price) {
            break;  // Buy price is below ask price — no match
        }

        // Step 4: How many shares can we fill?
        const uint32_t fillQty = std::min(
            incomingBuyOrder.getRemainingQuantity(),
            topAsk->remainingQuantity
        );

        // Step 5: Execute the fill on both sides
        incomingBuyOrder.fill(fillQty);
        book_.fillBestAsk(fillQty);

        // Step 6: Record the trade
        Trade trade(
            nextTradeId_++,
            incomingBuyOrder.getSymbol(),
            topAsk->price,           // Price is the RESTING order's price
            fillQty,
            incomingBuyOrder.getId(), // Buy order ID
            topAsk->orderId,         // Sell order ID
            Order::Clock::now()      // Timestamp
        );
        history_.recordTrade(trade);
    }
}
```

### Cancel and Modify

**Cancel** simply removes an order from the book:

```cpp
bool MatchingEngine::cancelOrder(uint64_t orderId) {
    return book_.cancelOrder(orderId);
}
```

**Modify** (cancel-and-replace) is more interesting. It removes the old order, creates a new one with the updated price and quantity, and re-submits it through the matching engine. This means:

1. The order **loses its time priority** — it moves to the back of the queue, just like a brand-new order.
2. The modified order **might trigger a match** if the new price crosses the spread.

```cpp
bool MatchingEngine::modifyOrder(uint64_t orderId, double newPrice, uint32_t newQuantity) {
    // Step 1: Remove the old order and get its details
    std::optional<Order> existing = book_.cancelAndReturnOrder(orderId);
    if (!existing.has_value()) {
        return false;  // Order not found
    }

    // Step 2: Create a replacement order with the same ID but new price/quantity
    Order replacement = Order::createLimitOrder(
        orderId,
        existing->getSymbol(),
        existing->getSide(),
        newPrice,
        newQuantity
    );

    // Step 3: Submit the replacement through the full matching logic
    submitOrder(std::move(replacement));
    return true;
}
```

---

## 10. The Trade — Proof That a Deal Happened

A `Trade` is an immutable record that a match occurred. Once created, it can never be changed. It stores:

| Field | Type | Description |
|---|---|---|
| `tradeId` | `uint64_t` | Unique trade number (auto-incrementing: 1, 2, 3, ...) |
| `symbol` | `string` | Which stock was traded |
| `price` | `double` | The price at which the trade executed |
| `quantity` | `uint32_t` | How many shares changed hands |
| `buyOrderId` | `uint64_t` | The ID of the buy order involved |
| `sellOrderId` | `uint64_t` | The ID of the sell order involved |
| `timestamp` | `TimePoint` | When the trade happened |

---

## 11. Trade History — The Record Keeper

`TradeHistory` stores every trade that has ever happened for a particular stock symbol. It supports several queries:

| Method | What It Does | Speed |
|---|---|---|
| `recordTrade(trade)` | Adds a new trade to the log | O(1) — instant |
| `getTrades()` | Returns all trades | O(1) — returns a reference |
| `getTradesBySymbol(sym)` | Filters trades by symbol name | O(n) — scans all trades |
| `getTradesInRange(from, to)` | Filters trades by time window | O(n) — scans all trades |
| `getVolumeStats()` | Computes total volume and VWAP | O(n) — single pass |

### What Is VWAP?

**VWAP** (Volume-Weighted Average Price) answers the question: "What was the average price, weighted by how many shares traded at each price?"

Simple example:
- Trade 1: 10 shares at $100
- Trade 2: 10 shares at $110

Simple average = ($100 + $110) / 2 = $105
VWAP = (10 × $100 + 10 × $110) / (10 + 10) = $2,100 / 20 = **$105**

But if the quantities were different:
- Trade 1: 90 shares at $100
- Trade 2: 10 shares at $110

Simple average = still $105
VWAP = (90 × $100 + 10 × $110) / (90 + 10) = $10,100 / 100 = **$101**

VWAP is lower because most of the volume traded at the lower price. It gives a more accurate picture of "where the real action was."

The formula:

```
VWAP = Σ(price_i × quantity_i) / Σ(quantity_i)
```

---

## 12. Exchange Core — The Multi-Symbol Router

A real exchange handles many different stocks — AAPL, GOOGL, MSFT, TSLA, and thousands more. `ExchangeCore` is the component that routes each order to the correct symbol's matching engine.

### How It Works

When an order for "AAPL" arrives:

1. `ExchangeCore` looks up "AAPL" in its internal map.
2. If this is the first time anyone has traded AAPL, it creates a new set of {OrderBook, MatchingEngine, TradeHistory} for AAPL.
3. It passes the order to AAPL's MatchingEngine.

```cpp
void ExchangeCore::submitOrder(Order order) {
    const std::string symbol = order.getSymbol();
    getOrCreate(symbol).engine.submitOrder(std::move(order));
}
```

### The SymbolData Bundle

Each symbol's three objects are bundled together in a struct:

```cpp
struct SymbolData {
    OrderBook      book;     // Where orders live
    TradeHistory   history;  // Where trades are recorded
    MatchingEngine engine;   // The matching logic (holds references to book and history)
};
```

**Critical detail:** `MatchingEngine` is declared **last** in the struct. This matters because C++ constructs struct members in **declaration order**. Since `MatchingEngine` needs references to `book` and `history`, those two must be fully constructed before `engine`'s constructor runs.

### Why `std::unordered_map`?

The symbols are stored in an `std::unordered_map<string, SymbolData>`. This was chosen over `std::vector` for one critical reason: **reference stability**.

`MatchingEngine` holds references (internal pointers) to the `OrderBook` and `TradeHistory` that live in the same `SymbolData`. If we used a `std::vector`, adding a new symbol could trigger a reallocation that moves all existing `SymbolData` objects to new memory addresses. This would break all the `MatchingEngine` references — they'd be pointing to old, now-invalid memory.

`std::unordered_map` stores each value in its own independently allocated node. Inserting new keys never moves existing values. The references stay valid forever.

---

## 13. Thread Safety — Making It Work with Multiple Users

When multiple users submit orders at the same time, we have a problem: two threads might try to modify the same order book simultaneously. This can corrupt data.

The project solves this with two components:

### ThreadSafeQueue — The Waiting Line

Think of this like a queue at a bank. Multiple people (threads) can join the queue at the same time, but only one teller (consumer) processes them one at a time.

```cpp
// Producer thread 1 (Web server handling client A):
queue.push(order1);

// Producer thread 2 (Web server handling client B):
queue.push(order2);

// Consumer thread (the matching engine):
Order order;
while (queue.waitAndPop(order)) {
    processOrder(order);  // Only one thread does this at a time
}
```

The queue uses a **mutex** (a lock) to ensure that only one thread can add or remove items at any given moment, and a **condition variable** to let the consumer thread sleep efficiently until there's work to do.

### ThreadSafeExchangeCore — Per-Symbol Locking

The thread-safe version uses a clever two-level locking strategy:

**Level 1 — Map Mutex:** A single lock that protects the symbol map (the container that holds all SymbolData). This lock is only needed briefly when:
- Looking up whether a symbol exists
- Creating a new symbol for the first time

**Level 2 — Per-Symbol Mutex:** Each symbol gets its own independent lock. When someone submits an order for AAPL, only the AAPL lock is taken. GOOGL and MSFT remain completely unblocked.

```
Thread 1 submitting AAPL order:  locks AAPL mutex → processes → unlocks
Thread 2 submitting GOOGL order: locks GOOGL mutex → processes → unlocks
                                  (these two run completely in parallel!)

Thread 3 submitting AAPL order:  tries to lock AAPL mutex → WAITS for Thread 1
                                  (same symbol, must wait for safety)
```

This design means that operations on **different symbols never block each other**. Only operations on the **same symbol** are serialized (executed one at a time).

**Deadlock prevention:** The map mutex is always released **before** the per-symbol mutex is acquired. The two locks are never held at the same time, which makes deadlocks impossible.

---

## 14. The Web Server — Connecting to the Outside World

The C++ engine on its own is just a library. To let external clients (like the React frontend) interact with it, we need a web server. The project uses **Crow**, a lightweight C++ web framework.

### ExchangeService — The Middleman

`ExchangeService` sits between the web server and the exchange core. It:

1. Translates web requests (JSON) into Order objects
2. Passes them to the exchange core
3. Sends back results as JSON
4. Broadcasts real-time updates to connected WebSocket clients

### REST API Endpoints

| Method | URL | What It Does |
|---|---|---|
| `POST` | `/api/orders` | Submit a new buy or sell order |
| `DELETE` | `/api/orders/:symbol/:orderId` | Cancel an existing order |
| `PUT` | `/api/orders/:symbol/:orderId` | Modify an order's price and quantity |
| `GET` | `/api/orderbook/:symbol` | Get current bid/ask depth |
| `GET` | `/api/trades/:symbol?limit=N` | Get recent trades |
| `GET` | `/api/symbols` | List all active symbols |
| `GET` | `/api/debug/orderbook/:symbol` | Get raw internal order book state |

### Example: Submitting an Order

**Request:**
```json
POST /api/orders
{
    "symbol": "AAPL",
    "side": "buy",
    "type": "limit",
    "price": 150.00,
    "quantity": 100
}
```

**Response:**
```json
{
    "orderId": 42,
    "status": "submitted"
}
```

### WebSocket — Real-Time Updates

HTTP (REST) is request-response: the client asks, the server answers. But for a trading terminal, we need the server to **push** updates to the client the instant something happens — without the client having to ask.

**WebSocket** is a protocol that keeps a persistent connection open. The server can send messages to the client at any time.

When a trade happens or the order book changes, the `ExchangeService` broadcasts a message to all connected WebSocket clients:

```json
// Trade event (pushed to all clients)
{
    "type": "trade",
    "symbol": "AAPL",
    "tradeId": 7,
    "price": 150.00,
    "quantity": 100,
    "buyOrderId": 42,
    "sellOrderId": 38,
    "timestamp": 1753660800000
}

// Order book update event (tells clients to refresh their view)
{
    "type": "orderbook_update",
    "symbol": "AAPL"
}
```

### The Broadcast Queue

Trade events and order book updates are pushed into a `ThreadSafeQueue<BroadcastEvent>`. A dedicated background thread (the **broadcaster**) drains this queue and sends each event to all connected WebSocket clients.

```
Matching Engine ──produces──► BroadcastQueue ──consumed by──► Broadcaster Thread
                                                                    │
                                                                    ├──► WebSocket Client 1
                                                                    ├──► WebSocket Client 2
                                                                    └──► WebSocket Client 3
```

This decouples the matching engine from network I/O. The engine records a trade and moves on immediately — it never waits for WebSocket messages to be delivered.

---

## 15. The Frontend — The Trading Terminal

The frontend is a React application built with Vite. It displays a professional-grade trading terminal interface with these components:

### WatchlistStrip
A horizontal ticker strip at the top showing all active symbols. Each symbol displays its current price with green/red flash animations when the price changes — just like the tickers you see on financial news channels.

### CandleChart
A real-time candlestick chart built entirely with SVG (no charting library). Shows OHLC (Open, High, Low, Close) candles on 1-second intervals with an optional VWAP line overlay. Each candle shows:
- **Green candle**: Close price was higher than open (price went up)
- **Red candle**: Close price was lower than open (price went down)
- **Wicks**: The thin lines showing the highest and lowest prices during that period

### OrderBookDepth
A side-by-side view of the bid and ask depth. Shows the top price levels with their aggregate quantities. Flash animations highlight when quantities change.

### OBIPanel (Order Book Imbalance)
Measures the balance between buy and sell pressure. The formula:

```
OBI = (Total Bid Volume - Total Ask Volume) / (Total Bid Volume + Total Ask Volume)
```

An OBI of +1.0 means all volume is on the buy side (strong buying pressure). An OBI of -1.0 means all volume is on the sell side (strong selling pressure). An OBI of 0.0 means equal volume on both sides.

### LatencyDashboard
Real-time telemetry showing:
- **p50 latency**: The median round-trip time for REST requests
- **p99 latency**: The 99th percentile — only 1% of requests are slower than this
- **OPS**: Orders per second being processed
- **TPS**: Trades per second being executed

### OrderEntryForm
A form where users can submit new orders by selecting symbol, side (buy/sell), type (limit/market), price, and quantity.

### TradeTape
A scrolling feed of recent trade executions, showing price, quantity, and timestamp.

### MemoryPoolPanel & NodeDataPanel (Debug Visualizers)
These two components visualize the internal data structures typical of a C++ HFT system:
- **MemoryPoolPanel**: Simulates a pre-allocated memory pool (a technique used in HFT to avoid dynamic memory allocation overhead). It visually represents "memory slots" as active (used) or inactive (free) blocks, showing the capacity utilization of the system.
- **NodeDataPanel**: Displays the raw, simulated doubly-linked list nodes underlying the order book. Each active order/price level is shown as a memory node containing pointers (`slot`, `prevSlot`, `nextSlot`), bridging the gap between high-level trading data and low-level C++ memory layout. Both panels fetch data from the backend's `/api/debug/orderbook/:symbol` debug endpoint.

### Market Simulator
A built-in feature that generates random buy and sell orders at realistic prices to simulate a live market. Click "Sim ON" to start the simulation and watch the order book, charts, and trades update in real time.

---

## 16. CSV Order Reader — Replaying Orders from a File

For testing and demo purposes, the `CsvOrderReader` can read orders from a CSV file and replay them into the exchange.

### CSV Format

```csv
orderId,symbol,side,type,price,quantity
1,AAPL,sell,limit,150.75,30
2,AAPL,sell,limit,151.00,50
3,AAPL,buy,limit,151.00,60
4,AAPL,buy,limit,149.00,100
5,AAPL,sell,market,,40
```

Each row is one order. The columns are:
- `orderId`: Unique number
- `symbol`: Stock ticker
- `side`: "buy" or "sell" (case-insensitive)
- `type`: "limit" or "market" (case-insensitive)
- `price`: The limit price (left empty for market orders)
- `quantity`: Number of shares

The reader skips blank lines, ignores the header row, and throws descriptive errors with line numbers if any row is malformed.

### Usage

```cpp
CsvOrderReader reader("config/sample_orders.csv");

// Option 1: Parse and get the orders as a list
std::vector<Order> orders = reader.parseOrders();

// Option 2: Parse and replay directly into the exchange
ExchangeCore core;
reader.replayInto(core);  // Submits each order in CSV row order
```

---

## 17. Testing — Proving Everything Works

The project includes 42 automated tests using Google Test. The tests are organized into three suites:

### Test Suite 1: OrderBook Tests (`test_order_book.cpp`)

These tests verify that the order book data structure works correctly on its own, without the matching engine:

| Test | What It Checks |
|---|---|
| `EmptyBestBidReturnsNullopt` | An empty book has no best bid |
| `EmptyBestAskReturnsNullopt` | An empty book has no best ask |
| `CancelUnknownOrderReturnsFalse` | Cancelling a nonexistent order returns false |
| `AddBidAndQueryBestBid` | Adding a buy order makes it the best bid |
| `BidsSortedByPriceDescending` | Best bid is always the highest price |
| `AsksSortedByPriceAscending` | Best ask is always the lowest price |
| `SamePriceBidsObeyTimeOrder` | FIFO: oldest order at same price is first |
| `CancelRemovesOrderAndReturnsTrue` | Cancel removes the order and returns true |
| `DoubleCancelReturnsFalse` | Cancelling the same order twice returns false |
| `CancelMiddleOrderDoesNotAffectNeighbours` | Cancelling order B in [A, B, C] leaves A and C intact |
| `CancelAllOrdersAtLevelClearsLevel` | Removing all orders at a price removes the price level |
| `FillBestAskPartial` | Partially filling leaves the correct remaining quantity |
| `FillBestAskFullRemovesOrder` | Fully filling removes the order from the book |
| `AddMarketOrderThrows` | Market orders cannot rest on the book |

### Test Suite 2: MatchingEngine Tests (`test_matching_engine.cpp`)

These tests verify the matching logic:

| Test | What It Checks |
|---|---|
| `BuyBelowAskRests` | A buy below the best ask doesn't match |
| `SellAboveBidRests` | A sell above the best bid doesn't match |
| `ExactFullFill` | Equal buy and sell at same price produces one trade |
| `IncomingBuyPartiallyFillsResting` | Smaller buy fills partially, resting order keeps remainder |
| `IncomingBuyLargerThanResting` | Larger buy fills resting completely, buy rests with remainder |
| `BuySweepesTwoPriceLevels` | A large buy crosses multiple price levels in order |
| `TradePriceIsRestingPrice` | Trade price is the resting order's price, not the incoming |
| `MarketBuyAgainstEmptyBookNoTrade` | Market order against empty book produces no trade |
| `MarketBuyMatchesRestingAsk` | Market buy matches against resting sell orders |
| `MarketOrderDoesNotRestAfterPartialFill` | Unfilled market order is discarded, not rested |
| `TimeOrderWithinPriceLevel` | Earlier order at same price gets matched first |
| `TradeIdsAreUnique` | Each trade gets a unique, auto-incrementing ID |
| `VolumeStatsAfterTrades` | VWAP calculation is correct after multiple trades |

### Test Suite 3: Cancel/Modify Tests (`test_cancel_modify.cpp`)

These tests verify cancellation and modification:

| Test | What It Checks |
|---|---|
| `CancelKnownOrderReturnsTrue` | Cancelling a known order succeeds |
| `CancelRemovesFromBook` | The cancelled order is gone from the book |
| `DoubleCancelReturnsFalseSecondTime` | Can't cancel the same order twice |
| `CancelOneSideDoesNotAffectOther` | Cancelling a bid doesn't affect asks |
| `ModifyUpdatesPrice` | Modifying changes the price correctly |
| `ModifyUpdatesQuantity` | Modifying changes the quantity correctly |
| `ModifyRetainsSameOrderId` | Modified order keeps its original ID |
| `ModifyCanTriggerMatch` | Modifying price to cross the spread triggers a trade |
| `ModifyLosesTimePriority` | Modified order goes to the back of the queue |
| `OverfillThrowsInvalidArgument` | Filling more than remaining quantity throws an error |
| `ZeroQuantityOrderThrows` | Creating an order with quantity 0 throws |
| `NegativePriceLimitOrderThrows` | Creating a limit order with negative price throws |

### Running the Tests

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

---

## 18. Benchmarking — Measuring Speed

The project includes a custom benchmark that measures two things:

1. **Throughput**: How many orders per second can the engine process?
2. **Latency**: How long does it take to process a single order?

### How the Benchmark Works

```
Phase 1 — Setup:
    For each iteration, populate the book with 100 resting sell orders

Phase 2 — Warm-up (500 iterations):
    Submit aggressive buy orders to warm up the CPU cache.
    Timing data is discarded.

Phase 3 — Measurement (5,000 iterations):
    For each iteration:
        1. Populate 100 sell orders
        2. Start the clock
        3. Submit one aggressive buy order that sweeps the entire book
        4. Stop the clock
        5. Record the elapsed time in nanoseconds

Phase 4 — Results:
    Sort all 5,000 latency measurements
    Report: Min, Avg, p50, p99, Max, Throughput
```

### What the Numbers Mean

```
Throughput:       ~1,200,000 orders/sec
p50 latency:      ~400 ns
p99 latency:      ~1,200 ns
```

- **Throughput of 1.2M orders/sec** means the engine can process over a million orders every second.
- **p50 of 400 ns** means the typical (median) order takes 400 nanoseconds — that's 0.0000004 seconds, or about 100× faster than a single blink of an eye.
- **p99 of 1,200 ns** means that 99% of all orders complete in under 1.2 microseconds. Only 1 in 100 orders takes longer, due to rare events like cache misses or OS interrupts.

### Why We Measure Percentiles Instead of Averages

Averages can be misleading. If 99 orders take 100 ns each and 1 order takes 100,000 ns, the average is 1,099 ns — but that doesn't describe anyone's actual experience. Most orders were fast (100 ns), and one was slow (100,000 ns).

Percentiles give a clearer picture:
- **p50 (median)**: What a typical order experiences
- **p99**: The worst-case for 99% of orders
- **p99.9**: The worst-case for 99.9% of orders

The gap between p50 and p99 is called the **latency tail**. A well-designed system has a tight tail (small gap).

---

## 19. Build System — How to Compile It All

The project uses **CMake** as its build system. CMake is a tool that generates the actual build files (Makefiles, Visual Studio projects, etc.) from a platform-independent description.

### What CMake Does

The `CMakeLists.txt` at the project root defines:

1. **Core Library (`miniexchange_core`)**: All the engine source files compiled into a reusable library:
   - Order.cpp, OrderBook.cpp, MatchingEngine.cpp, Trade.cpp, TradeHistory.cpp
   - ExchangeCore.cpp, ThreadSafeExchangeCore.cpp, CsvOrderReader.cpp

2. **Demo Executable (`MiniExchange`)**: A simple `main.cpp` that exercises the engine locally.

3. **Server Executable (`MiniExchangeServer`)**: The full web server with REST API and WebSocket support.

4. **Tests**: Three test executables using Google Test.

5. **Benchmark**: The performance measurement executable.

### External Dependencies (Auto-Downloaded)

CMake automatically downloads these dependencies using `FetchContent`:

| Dependency | Purpose |
|---|---|
| **nlohmann/json** | JSON parsing and serialization for the REST API |
| **Crow** | Lightweight C++ web framework for HTTP + WebSocket |
| **Asio** | Network I/O library required by Crow |
| **Google Test** | Testing framework for automated tests |

No manual installation of these libraries is required — CMake downloads and builds them automatically.

---

## 20. How to Run the Project

### Method 1: Using the start.bat Script (Windows, Easiest)

Just double-click `start.bat` or run it in a terminal. It will:
1. Kill any old server processes
2. Check that the server binary exists
3. Copy required DLLs if needed
4. Launch the C++ server on port 8080
5. Wait for the server to be ready (health check)
6. Install frontend npm dependencies if needed
7. Launch the React frontend on port 5173
8. Open your browser to http://localhost:5173

### Method 2: Manual Steps

**Step 1 — Build the C++ Server:**
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

**Step 2 — Start the C++ Server:**
```bash
# Windows (PowerShell)
.\cmake-build-debug\MiniExchangeServer.exe 8080

# Linux / macOS
./build/MiniExchangeServer 8080
```

**Step 3 — Start the React Frontend (in a separate terminal):**
```bash
cd frontend
npm install      # First time only
npm run dev
```

**Step 4 — Open your browser:**
Navigate to **http://localhost:5173**

**Step 5 — Start the market simulator:**
Click "Sim ON" in the top navigation bar to generate live market activity.

---

## 21. End-to-End Data Flow — Following One Order Through the Entire System

Let's trace exactly what happens when you click "Submit" on a buy order for 100 shares of AAPL at $150.00:

```
    ┌─────────────────────────────────────────────────────────────────┐
    │  STEP 1: User clicks "Submit Buy" in the React frontend        │
    │                                                                 │
    │  The OrderEntryForm component creates a JSON object:           │
    │  { symbol: "AAPL", side: "buy", type: "limit",                │
    │    price: 150.00, quantity: 100 }                               │
    │                                                                 │
    │  and sends it via HTTP POST to http://localhost:8080/api/orders │
    └─────────────────────────────────┬───────────────────────────────┘
                                      │
                                      ▼
    ┌─────────────────────────────────────────────────────────────────┐
    │  STEP 2: Crow web server receives the HTTP request              │
    │                                                                 │
    │  The POST /api/orders route handler:                            │
    │  - Parses the JSON body                                         │
    │  - Extracts symbol, side, type, price, quantity                │
    │  - Calls exchangeService->submitOrder(...)                      │
    └─────────────────────────────────┬───────────────────────────────┘
                                      │
                                      ▼
    ┌─────────────────────────────────────────────────────────────────┐
    │  STEP 3: ExchangeService processes the submission               │
    │                                                                 │
    │  - Acquires the coreMutex_ lock                                │
    │  - Generates a unique order ID (atomic counter: 42)            │
    │  - Creates an Order object:                                     │
    │    Order::createLimitOrder(42, "AAPL", Side::Buy, 150.00, 100) │
    │  - Calls exchangeCore_->submitOrder(order)                      │
    │  - If this is a new symbol, registers a trade callback          │
    │  - Pushes an OrderBookUpdateEvent to the broadcast queue        │
    │  - Returns the order ID (42)                                    │
    └─────────────────────────────────┬───────────────────────────────┘
                                      │
                                      ▼
    ┌─────────────────────────────────────────────────────────────────┐
    │  STEP 4: ExchangeCore routes to the correct symbol              │
    │                                                                 │
    │  - Calls getOrCreate("AAPL")                                   │
    │  - If AAPL doesn't exist yet:                                   │
    │      Creates SymbolData { OrderBook("AAPL"),                   │
    │                           TradeHistory(),                       │
    │                           MatchingEngine(book, history) }       │
    │  - Calls engine.submitOrder(order)                              │
    └─────────────────────────────────┬───────────────────────────────┘
                                      │
                                      ▼
    ┌─────────────────────────────────────────────────────────────────┐
    │  STEP 5: MatchingEngine runs the matching algorithm             │
    │                                                                 │
    │  Since the order is a BUY, call matchAgainstAsks():             │
    │                                                                 │
    │  Iteration 1:                                                   │
    │    - peekBestAsk() → $150.00, 80 shares, order #38             │
    │    - Price check: $150.00 (buy) >= $150.00 (ask) ✓ crosses!    │
    │    - fillQty = min(100, 80) = 80                                │
    │    - incomingBuy.fill(80) → remaining = 20                     │
    │    - book.fillBestAsk(80) → order #38 fully consumed, removed  │
    │    - Record Trade(tradeId=7, AAPL, $150.00, 80, buy=42, sell=38)│
    │                                                                 │
    │  Iteration 2:                                                   │
    │    - peekBestAsk() → $150.50, 200 shares, order #39            │
    │    - Price check: $150.00 (buy) >= $150.50 (ask)? NO. ✗         │
    │    - Break out of matching loop                                  │
    │                                                                 │
    │  After matching: incoming buy has 20 shares remaining            │
    │  It's a limit order, so add it to the book:                     │
    │    book.addOrder(order) → rests at $150.00 on the bid side     │
    └─────────────────────────────────┬───────────────────────────────┘
                                      │
                                      ▼
    ┌─────────────────────────────────────────────────────────────────┐
    │  STEP 6: Trade callback fires                                   │
    │                                                                 │
    │  The trade callback (registered by ExchangeService) creates     │
    │  a TradeEvent and pushes it to the broadcast queue.             │
    └─────────────────────────────────┬───────────────────────────────┘
                                      │
                                      ▼
    ┌─────────────────────────────────────────────────────────────────┐
    │  STEP 7: Broadcaster thread sends WebSocket updates             │
    │                                                                 │
    │  The broadcaster thread drains the queue and sends:             │
    │  1. Trade event → all WebSocket clients                         │
    │  2. OrderBookUpdate event → all WebSocket clients               │
    └─────────────────────────────────┬───────────────────────────────┘
                                      │
                                      ▼
    ┌─────────────────────────────────────────────────────────────────┐
    │  STEP 8: Frontend receives updates and re-renders               │
    │                                                                 │
    │  The React App.jsx receives WebSocket messages and:             │
    │  - Updates the candlestick chart with the new trade price       │
    │  - Refreshes the order book depth view                          │
    │  - Adds the trade to the trade tape                             │
    │  - Recalculates OBI (order book imbalance)                      │
    │  - Updates the watchlist strip price and flash animation        │
    │  - Updates latency metrics                                       │
    └─────────────────────────────────────────────────────────────────┘
```

---

## 22. Performance Characteristics

### Time Complexity of Operations

| Operation | Time Complexity | Explanation |
|---|---|---|
| Submit limit order (no match) | O(log P) | Inserting into the sorted `std::map`. P = number of distinct price levels, usually tens to hundreds. |
| Cancel order | O(1) | Hash map lookup → direct list removal via cached iterator |
| Match against one price level | O(1) amortized | Read the front of the list, fill, possibly remove |
| Sweep across k price levels | O(k × log P) | Matching k levels, with possible map erasure at each |
| Modify order | O(log P) | Cancel O(1) + re-insert O(log P) |
| VWAP calculation | O(n) | Single pass over all trades |

### Benchmark Results (Typical)

```
Throughput:       ~1,200,000 orders/sec
p50 latency:      ~400 ns        (0.4 microseconds)
p99 latency:      ~1,200 ns      (1.2 microseconds)
```

To put these numbers in perspective:
- Light travels about 1 foot (30 cm) in 1 nanosecond
- A single CPU instruction takes about 0.3 nanoseconds
- A cache miss (reading from RAM) takes about 67 nanoseconds
- A system call (asking the OS for something) takes about 200-1,000 nanoseconds
- A network round-trip on the same machine takes about 50,000 nanoseconds
- A human blink takes about 100,000,000 nanoseconds

The engine processes an order in about 400 nanoseconds — faster than the time it takes for light to travel 400 feet.

---

## 23. Key Design Decisions and Why

### Decision 1: `std::list` over `std::vector` for orders at a price level

**Choice:** `std::list<Order>` (doubly-linked list)
**Alternative considered:** `std::vector<Order>` (contiguous array)

**Why:** When we cancel an order in the middle of a queue, `std::list` removes it in O(1) and doesn't move anything else. `std::vector` would need to shift all subsequent elements, taking O(n) time and invalidating all stored iterators. Since the `orderLocationIndex_` stores iterators, they must remain valid across insertions and deletions. Only `std::list` guarantees this.

### Decision 2: `std::map` over `std::unordered_map` for price levels

**Choice:** `std::map<double, list<Order>>` (sorted tree)
**Alternative considered:** `std::unordered_map` (hash table)

**Why:** We always need the best price (lowest ask or highest bid) which means we need sorted order. `std::map` keeps keys sorted automatically, giving us O(1) access to the best price via `begin()`. An `unordered_map` would require scanning all entries to find the minimum or maximum, which would be O(n).

### Decision 3: `std::unordered_map` for the symbol router

**Choice:** `std::unordered_map<string, SymbolData>`
**Alternative considered:** `std::vector<SymbolData>` or `std::map<string, SymbolData>`

**Why:** `MatchingEngine` holds references (pointers) to `OrderBook` and `TradeHistory` that live inside the same `SymbolData`. The container must guarantee that existing values are never moved when new entries are added. `unordered_map` stores values in individually allocated nodes — adding a new key never moves existing values. `vector` would move everything on reallocation, breaking all internal references.

### Decision 4: Named factory functions instead of public constructors for Order

**Choice:** `Order::createLimitOrder()` and `Order::createMarketOrder()`
**Alternative considered:** A single public constructor

**Why:** Factory functions enforce invariants at creation time. A limit order must have a positive price. A market order must not have a price. Using two separate factory functions makes these constraints explicit and impossible to bypass. The private constructor handles the actual initialization.

### Decision 5: `optional<double>` for price instead of a sentinel value

**Choice:** `std::optional<double>` — can be a price or "no price"
**Alternative considered:** Using `price = -1.0` or `price = 0.0` to mean "no price"

**Why:** Sentinel values are error-prone. If someone forgets to check for -1 before using the price in a calculation, they get garbage results with no warning. `std::optional` forces the programmer to explicitly check `.has_value()` before accessing the value, making bugs much harder to introduce.

### Decision 6: Per-symbol mutexes instead of a single global lock

**Choice:** One mutex per stock symbol
**Alternative considered:** One mutex for the entire exchange

**Why:** With a single global lock, an order for AAPL would block an order for GOOGL. This destroys parallelism. With per-symbol locks, operations on different symbols run completely in parallel. Only operations on the same symbol are serialized, which is the minimum required for correctness.

### Decision 7: Trade price is the resting order's price

**Choice:** Trade executes at the resting order's price
**Alternative considered:** Trade executes at the incoming order's price, or at the midpoint

**Why:** This is how real exchanges work. The resting order has been waiting in the book, publicly visible. The incoming order is the aggressor. The aggressor gets "price improvement" — if they were willing to pay $105 but the ask is $100, they get the better price of $100. This incentivizes traders to post visible orders (providing liquidity), which is healthy for the market.

---

## 24. Glossary

| Term | Definition |
|---|---|
| **Ask** | A sell order. "I am asking this price for my shares." |
| **Best Ask** | The lowest price any seller is currently willing to accept |
| **Best Bid** | The highest price any buyer is currently willing to pay |
| **Bid** | A buy order. "I am bidding this price for shares." |
| **Candlestick** | A chart element showing open, high, low, and close prices for a time period |
| **Crosses** | When a buy price meets or exceeds a sell price, enabling a trade |
| **FIFO** | First In, First Out — orders at the same price are matched in arrival order |
| **Fill** | When an order is matched (partially or fully) with a counter-party |
| **Limit Order** | An order with a specified maximum buy price or minimum sell price |
| **Liquidity** | The availability of orders on the book — more orders means more liquidity |
| **Market Order** | An order that executes immediately at the best available price |
| **Matching Engine** | The algorithm that pairs buy and sell orders |
| **Mutex** | A lock that ensures only one thread accesses a shared resource at a time |
| **OBI** | Order Book Imbalance — measures the ratio of buy vs sell volume |
| **OHLC** | Open, High, Low, Close — the four prices that define a candlestick |
| **Order Book** | The data structure holding all resting buy and sell orders |
| **Partial Fill** | When only some of an order's quantity is matched |
| **Percentile (p50, p99)** | The value below which a given percentage of data falls |
| **Price-Time Priority** | Best price first, then earliest time first — the matching rule |
| **REST API** | A web API using HTTP methods (GET, POST, PUT, DELETE) |
| **Resting Order** | An order sitting on the book, waiting to be matched |
| **Spread** | The gap between the best bid and best ask |
| **Sweep** | When an incoming order matches across multiple price levels |
| **Throughput** | Number of operations per second |
| **VWAP** | Volume-Weighted Average Price — average price weighted by trade volume |
| **WebSocket** | A protocol for real-time, bidirectional communication |

---

*This document covers the entire Mini Exchange project as of July 2026. For the latest code, refer to the source files directly.*
