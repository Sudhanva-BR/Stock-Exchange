// bench_matching_engine.cpp
//
// Custom benchmarking harness using std::chrono::high_resolution_clock.
// No external dependencies — deliberate per project spec.
//
// Measures:
//   - Throughput (orders/second) for a sustained batch of matching operations.
//   - Per-order latency: min, max, p50, p99 (nanoseconds).
//
// Methodology:
//   Phase 1 (Setup): Pre-populate the ask side with N resting sell orders
//                    at prices [BASE_PRICE, BASE_PRICE+1, ...].
//   Phase 2 (Warm-up): Submit WARMUP_N buy orders without recording timings.
//   Phase 3 (Measurement): Submit MEASURE_N buy orders, each time-stamped
//                    individually for latency capture.
//
// Design decisions:
//   - Each measured iteration rebuilds the book (N resting asks) to ensure
//     every match finds orders and the test is stable across iterations.
//   - Latency is measured per-order (wall-clock ns), not per-trade, so the
//     metric is "time to process submitOrder() call" end-to-end.
//   - Results are printed to stdout as a simple table — no file I/O.

#include <iostream>
#include <iomanip>
#include <vector>
#include <algorithm>
#include <numeric>
#include <chrono>
#include <cstdint>

#include "miniexchange/OrderBook.h"
#include "miniexchange/TradeHistory.h"
#include "miniexchange/MatchingEngine.h"
#include "miniexchange/Order.h"

using namespace miniexchange;
using Clock     = std::chrono::high_resolution_clock;
using TimePoint = Clock::time_point;
using Nanos     = std::chrono::nanoseconds;

// ---------------------------------------------------------------------------
// Benchmark parameters
// ---------------------------------------------------------------------------

static constexpr uint32_t BOOK_DEPTH    = 100;    // resting asks per iteration
static constexpr uint32_t WARMUP_N      = 500;    // iterations before measuring
static constexpr uint32_t MEASURE_N     = 5'000;  // measured iterations
static constexpr double   BASE_PRICE    = 100.0;
static constexpr uint32_t ORDER_QTY     = 10;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// Percentile (0-100) of a sorted vector.
static double percentile(const std::vector<int64_t>& sorted, double p) {
    if (sorted.empty()) return 0.0;
    const double idx = (p / 100.0) * static_cast<double>(sorted.size() - 1);
    const std::size_t lo = static_cast<std::size_t>(idx);
    const std::size_t hi = lo + 1 < sorted.size() ? lo + 1 : lo;
    const double frac    = idx - static_cast<double>(lo);
    return static_cast<double>(sorted[lo]) * (1.0 - frac)
         + static_cast<double>(sorted[hi]) * frac;
}

// Populate BOOK_DEPTH sell orders starting at BASE_PRICE.
static void populateBook(MatchingEngine& engine, uint64_t& nextId) {
    for (uint32_t i = 0; i < BOOK_DEPTH; ++i) {
        engine.submitOrder(Order::createLimitOrder(
            nextId++, "BENCH", Side::Sell,
            BASE_PRICE + static_cast<double>(i), ORDER_QTY));
    }
}

// Submit a single aggressive buy order that crosses the entire book.
// Using a very high price guarantees it always matches (price-crosses every level).
static void submitAggressiveBuy(MatchingEngine& engine, uint64_t& nextId) {
    engine.submitOrder(Order::createLimitOrder(
        nextId++, "BENCH", Side::Buy,
        BASE_PRICE + static_cast<double>(BOOK_DEPTH) + 1.0, ORDER_QTY));
}

// ---------------------------------------------------------------------------
// Single-iteration bench: rebuild book + measure one submitOrder call.
// Returns latency in nanoseconds.
// ---------------------------------------------------------------------------

static int64_t benchIteration(OrderBook& book, TradeHistory& history,
                               MatchingEngine& engine, uint64_t& nextId) {
    populateBook(engine, nextId);

    const TimePoint t0 = Clock::now();
    submitAggressiveBuy(engine, nextId);
    const TimePoint t1 = Clock::now();

    return std::chrono::duration_cast<Nanos>(t1 - t0).count();
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main() {
    std::cout << "=== Mini Exchange — Matching Engine Benchmark ===\n\n";

    OrderBook    book("BENCH");
    TradeHistory history;
    MatchingEngine engine(book, history);

    uint64_t nextId = 1;

    // ----- Warm-up phase -----
    std::cout << "Warming up (" << WARMUP_N << " iterations)...\n";
    for (uint32_t i = 0; i < WARMUP_N; ++i) {
        benchIteration(book, history, engine, nextId);
    }

    // ----- Measurement phase -----
    std::cout << "Measuring (" << MEASURE_N << " iterations)...\n";
    std::vector<int64_t> latencies;
    latencies.reserve(MEASURE_N);

    const TimePoint batchStart = Clock::now();
    for (uint32_t i = 0; i < MEASURE_N; ++i) {
        latencies.push_back(benchIteration(book, history, engine, nextId));
    }
    const TimePoint batchEnd = Clock::now();

    // ----- Throughput -----
    const double totalSecs = std::chrono::duration<double>(batchEnd - batchStart).count();
    // Each iteration submits (BOOK_DEPTH + 1) orders: BOOK_DEPTH sells + 1 buy.
    const uint64_t totalOrders = static_cast<uint64_t>(MEASURE_N) * (BOOK_DEPTH + 1);
    const double   throughput  = static_cast<double>(totalOrders) / totalSecs;

    // ----- Latency statistics (only on the aggressive buy, the "interesting" order) -----
    std::sort(latencies.begin(), latencies.end());
    const double minNs = static_cast<double>(latencies.front());
    const double maxNs = static_cast<double>(latencies.back());
    const double p50   = percentile(latencies, 50.0);
    const double p99   = percentile(latencies, 99.0);
    const double avg   = static_cast<double>(
        std::accumulate(latencies.begin(), latencies.end(), int64_t{0}))
        / static_cast<double>(latencies.size());

    // ----- Print results -----
    const int W = 22;
    std::cout << "\n--- Results ---\n";
    std::cout << std::left
              << std::setw(W) << "Book depth per iter:"
              << BOOK_DEPTH << " resting orders\n"
              << std::setw(W) << "Measured iterations:" << MEASURE_N << "\n"
              << std::setw(W) << "Total orders:"        << totalOrders << "\n"
              << std::setw(W) << "Total time:"
              << std::fixed << std::setprecision(3)
              << totalSecs * 1000.0 << " ms\n"
              << std::setw(W) << "Throughput:"
              << std::setprecision(0) << throughput << " orders/sec\n"
              << "\n--- Per-order submit() latency (ns) ---\n"
              << std::setw(W) << "Min:"   << std::setprecision(1) << minNs << " ns\n"
              << std::setw(W) << "Avg:"   << avg  << " ns\n"
              << std::setw(W) << "p50:"   << p50  << " ns\n"
              << std::setw(W) << "p99:"   << p99  << " ns\n"
              << std::setw(W) << "Max:"   << maxNs << " ns\n"
              << std::flush;

    return 0;
}
