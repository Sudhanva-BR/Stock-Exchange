#pragma once

#include <mutex>
#include <thread>
#include <unordered_map>
#include <string>
#include <functional>

#include "miniexchange/ExchangeCore.h"
#include "miniexchange/ThreadSafeQueue.h"
#include "miniexchange/Order.h"

namespace miniexchange {

    // ThreadSafeExchangeCore extends ExchangeCore with:
    //
    //  1. A ThreadSafeQueue<Order> for asynchronous order submission (producer path).
    //  2. Per-symbol std::mutex so that MatchingEngine instances for *different*
    //     symbols can run concurrently with zero contention, while operations on
    //     the *same* symbol are fully serialised.
    //  3. A background consumer thread that drains the queue and dispatches orders.
    //
    // Locking protocol (two-level):
    //  - mapMutex_  : guards structural changes to ExchangeCore::symbols_ and
    //                 symbolMutexes_ (i.e., inserting a new key).  Held only
    //                 during lookup + potential insertion, then released.
    //  - per-symbol mutex (symbolMutexes_[symbol]) : guards all operations on
    //                 that symbol's MatchingEngine / OrderBook / TradeHistory.
    //                 Acquired AFTER releasing mapMutex_ to avoid long critical
    //                 sections and potential deadlock.
    //
    // Thread safety guarantee:
    //  - enqueueOrder()       : safe to call from any thread.
    //  - processAll()         : drains the queue synchronously on the calling thread.
    //  - submitOrderDirect()  : thread-safe direct submit (bypass queue).
    //  - cancelOrder(),
    //    modifyOrder()        : thread-safe.
    //  - getOrderBook(),
    //    getTradeHistory()    : thread-safe for const read (take symbol mutex).
    //  - startProcessingThread() / stopProcessingThread() : must be called from
    //                         the same thread (typical owner-thread pattern).
    class ThreadSafeExchangeCore : public ExchangeCore {
    public:
        ThreadSafeExchangeCore() = default;
        ~ThreadSafeExchangeCore();  // joins background thread if running

        // Non-copyable / non-movable (inherits from ExchangeCore which is also).
        ThreadSafeExchangeCore(const ThreadSafeExchangeCore&) = delete;
        ThreadSafeExchangeCore& operator=(const ThreadSafeExchangeCore&) = delete;
        ThreadSafeExchangeCore(ThreadSafeExchangeCore&&) = delete;
        ThreadSafeExchangeCore& operator=(ThreadSafeExchangeCore&&) = delete;

        // --- Asynchronous producer path ---

        // Enqueue an order for processing (non-blocking, thread-safe).
        void enqueueOrder(Order order);

        // Drain the queue synchronously on the calling thread.
        // Returns the number of orders processed.
        std::size_t processAll();

        // --- Background thread management ---

        // Starts a background consumer thread that continuously drains the queue.
        // No-op if a thread is already running.
        void startProcessingThread();

        // Signals the background thread to stop after draining remaining items,
        // then joins it.  Safe to call if no thread was started.
        void stopProcessingThread();

        // --- Thread-safe synchronous API (overrides ExchangeCore) ---

        // Direct submit (bypasses the queue).  Acquires the symbol's mutex.
        void submitOrder(Order order);

        bool cancelOrder(const std::string& symbol, uint64_t orderId);
        bool modifyOrder(const std::string& symbol, uint64_t orderId,
                         double newPrice, uint32_t newQuantity);

        const OrderBook&    getOrderBook    (const std::string& symbol) const;
        const TradeHistory& getTradeHistory (const std::string& symbol) const;

    private:
        // Returns (or creates) the per-symbol mutex.
        // Must NOT be called while holding a symbol mutex (to avoid deadlock).
        std::mutex& getSymbolMutex(const std::string& symbol);

        // Internal submit under the symbol mutex — called by both submitOrder and
        // the queue consumer.
        void submitOrderLocked(Order order);

        ThreadSafeQueue<Order>                          orderQueue_;
        mutable std::mutex                              mapMutex_;   // guards symbols_ + symbolMutexes_ structure
        mutable std::unordered_map<std::string, std::mutex> symbolMutexes_;

        std::thread     processingThread_;
        std::atomic<bool> threadRunning_{false};
    };

} // namespace miniexchange
