# Architecture — Mini Stock Exchange Simulator

## 1. Overview

The simulator is structured as a set of layered components, each with a single
well-defined responsibility. Data flows from the outside world through `ExchangeCore`
(or its thread-safe variant), which routes each order to the correct symbol's
`MatchingEngine`, which applies matching logic against the `OrderBook` and records
results in `TradeHistory`.

```
                     ┌─────────────────────────────────────┐
   Producer threads  │     ThreadSafeExchangeCore          │
   ─────────────────►│  enqueueOrder()   submitOrder()     │
                     │  ThreadSafeQueue<Order>              │
                     │  per-symbol mutex map                │
                     └────────────────┬────────────────────┘
                                      │ routes by symbol
                     ┌────────────────▼────────────────────┐
                     │          ExchangeCore               │
                     │  unordered_map<symbol, SymbolData>  │
                     │  getOrCreate(symbol)                │
                     └────────────────┬────────────────────┘
                                      │ 1 per symbol
                     ┌────────────────▼────────────────────┐
                     │           SymbolData                │
                     │  ┌─────────────────────────────┐   │
                     │  │  OrderBook                  │   │
                     │  │  MatchingEngine ──refs──►  │◄──┤
                     │  │  TradeHistory ◄──writes─    │   │
                     │  └─────────────────────────────┘   │
                     └─────────────────────────────────────┘
```

---

## 2. Layer-by-layer description

### 2.1 Value objects — `Order` and `Trade`

Both are **immutable after construction** (except for `Order`'s two controlled
mutations: `fill()` and `cancel()`).

- **`Order`** is created only via named factory functions
  (`createLimitOrder`, `createMarketOrder`) to enforce invariants at construction time
  (positive price, positive quantity).
- Price is `std::optional<double>` — `nullopt` signals a market order and eliminates
  sentinel magic-number conventions.
- **`Trade`** is fully immutable. Its price is always the *resting* order's price,
  reflecting how real exchanges work (price improvement goes to the aggressor).

### 2.2 `OrderBook` — per-symbol data structure

Responsibility: **store** limit orders; **never match**.

```
bids_: map<double, list<Order>, greater<double>>   ← highest bid first
asks_: map<double, list<Order>>                    ← lowest ask first
orderLocationIndex_: unordered_map<orderId, OrderLocation>
```

**Why `std::list`?** `std::list` is the only standard container whose iterators remain
valid after arbitrary insertions and erasures elsewhere in the same list. `OrderLocation`
caches the iterator, giving **O(1) cancel** at any position in the queue — critical for
realistic exchange simulation where cancel-heavy order flows are common.

**Why `std::map` for price levels?** Sorted order is required (best bid = largest price,
best ask = smallest price). Insertions and deletions are O(log levels) — acceptable
because the number of distinct price levels is typically much smaller than the number of
orders.

### 2.3 `MatchingEngine` — price-time priority matching

Responsibility: **match** incoming orders against resting orders.

Algorithm:
```
submitOrder(incoming):
    if incoming.side == Buy:
        matchAgainstAsks(incoming)
    else:
        matchAgainstBids(incoming)

    if incoming is a LimitOrder AND remaining > 0:
        book.addOrder(incoming)   ← rests on the book
    # Market orders with leftover quantity are discarded
```

Each `matchAgainstXxx` method:
1. Peeks at the best opposing order (`peekBestBid` / `peekBestAsk`).
2. Checks price crossing (market orders skip this check via `std::optional` absence).
3. Fills `min(incoming.remaining, resting.remaining)`.
4. Records a `Trade` at the **resting** order's price.
5. Repeats until no more matching orders or incoming is fully filled.

`modifyOrder` is implemented as **cancel-and-replace with the same ID**. The order loses
time priority (moves to the back of the queue), matching real exchange behaviour.

### 2.4 `TradeHistory` — query-capable trade log

Stores trades in a `std::vector<Trade>` (ordered by arrival). Provides:

| Method | Complexity |
|---|---|
| `recordTrade(trade)` | O(1) amortised |
| `getTrades()` | O(1) |
| `getTradesBySymbol(sym)` | O(n) |
| `getTradesInRange(from, to)` | O(n) |
| `getVolumeStats()` | O(n), single pass |

VWAP (Volume-Weighted Average Price) is computed as:
```
VWAP = Σ(price_i × qty_i) / Σ(qty_i)
```

### 2.5 `ExchangeCore` — multi-symbol router

Owns one `SymbolData` struct per symbol inside an `unordered_map`. The struct
bundles `{OrderBook, TradeHistory, MatchingEngine}` together because `MatchingEngine`
holds **references** into its siblings.

**Reference stability guarantee**: `std::unordered_map` guarantees that inserting a new
key does NOT move or invalidate existing values (node-based storage). This is the
architectural reason `unordered_map` was chosen over `std::vector<SymbolData>` (which
would invalidate references on reallocation).

**Construction order**: `MatchingEngine` has no default constructor. `SymbolData`'s
constructor initialises `book` and `history` first (by declaration order), then
`engine(book, history)` — so the references are always valid.

### 2.6 `ThreadSafeExchangeCore` — concurrency layer

Two-level locking protocol:

| Lock | Scope | Purpose |
|---|---|---|
| `mapMutex_` | ExchangeCore::symbols_ + symbolMutexes_ | Guards structural changes (new key insertion) |
| `symbolMutexes_[sym]` | One entry per symbol | Serialises all matching operations for that symbol |

**Key property**: Operations on *different* symbols hold *different* mutexes — full
parallelism across symbols. Operations on the *same* symbol are fully serialised.

`mapMutex_` is released *before* acquiring a symbol mutex (never held together) to
prevent deadlock.

The async path:
```
Producer thread  →  enqueueOrder()  →  ThreadSafeQueue<Order>
                                             │
Consumer thread  ←  waitAndPop()    ←────────┘
                 →  submitOrderLocked()  →  MatchingEngine
```

### 2.7 `ThreadSafeQueue<T>` — producer-consumer queue

Header-only template. Uses `std::mutex` + `std::condition_variable` for blocking pop,
and `std::atomic<bool>` for the shutdown flag.

`waitAndPop` checks both "queue has items" and "shutdown requested" in the predicate —
consumers wake up on either condition and exit cleanly when the queue is empty and
shutdown is signalled.

### 2.8 `CsvOrderReader` — order replay

Reads a CSV file using `std::filesystem` + `std::ifstream`. Produces `std::vector<Order>`
or replays directly into an `ExchangeCore`. Parse errors throw `std::runtime_error` with
the offending line number for easy debugging.

---

## 3. Data flow — CSV replay example

```
config/sample_orders.csv
        │
   CsvOrderReader::replayInto(core)
        │  parseOrders() → vector<Order>
        │
   ThreadSafeExchangeCore::submitOrder(order)
        │  acquires symbolMutexes_[symbol]
        │
   ExchangeCore::getOrCreate(symbol)
        │  emplace SymbolData if new
        │
   MatchingEngine::submitOrder(order)
        │  matchAgainstAsks / matchAgainstBids
        │  ├── OrderBook::peekBestAsk()
        │  ├── Order::fill() + OrderBook::fillBestAsk()
        │  └── TradeHistory::recordTrade(Trade)
        │
        └── OrderBook::addOrder(order)  [if resting]
```

---

## 4. Testing strategy

| Suite | Coverage |
|---|---|
| `test_order_book` | Empty-book edge cases, price-time priority, cancel stability (list iterator), fill |
| `test_matching_engine` | Full/partial fills, multi-level sweep, trade price correctness, market order edge cases, VWAP |
| `test_cancel_modify` | Cancel / double-cancel, modify price/qty/ID, time-priority loss after modify, Order invariant exceptions, TradeHistory queries |

---

## 5. Performance characteristics

| Operation | Complexity |
|---|---|
| Submit limit order (no match) | O(log P) where P = distinct price levels |
| Cancel order | O(1) via `orderLocationIndex_` |
| Match one level | O(1) amortised |
| Sweep k levels | O(k × log P) |
| VWAP calculation | O(n) trades |

Benchmark results (Release build, example figures):

```
Throughput:       ~1,200,000 orders/sec
p50 latency:      ~400 ns
p99 latency:      ~1,200 ns
```
*(Actual results vary by hardware.)*
