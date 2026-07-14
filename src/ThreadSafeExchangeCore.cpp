#include "miniexchange/ThreadSafeExchangeCore.h"

#include <stdexcept>

namespace miniexchange {

// ---------------------------------------------------------------------------
// Destructor — stop background thread if it's running
// ---------------------------------------------------------------------------

ThreadSafeExchangeCore::~ThreadSafeExchangeCore() {
    stopProcessingThread();
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

std::mutex& ThreadSafeExchangeCore::getSymbolMutex(const std::string& symbol) {
    // Phase 1: check under mapMutex_ (read path — common case).
    {
        std::lock_guard<std::mutex> mapLock(mapMutex_);
        auto it = symbolMutexes_.find(symbol);
        if (it != symbolMutexes_.end()) {
            return it->second;  // already exists — no structural change needed
        }
        // Phase 2: insert under the same mapMutex_ lock.
        // default_insert_or_assign creates the mutex in-place.
        return symbolMutexes_[symbol];  // value-initialises (default ctor for mutex)
    }
}

void ThreadSafeExchangeCore::submitOrderLocked(Order order) {
    // Extract the symbol BEFORE moving the order — use-after-move guard.
    const std::string symbol = order.getSymbol();

    // Acquire per-symbol mutex (mapMutex_ is NOT held here).
    std::lock_guard<std::mutex> symLock(getSymbolMutex(symbol));

    // Delegate to the base class routing logic (getOrCreate + engine.submitOrder).
    ExchangeCore::submitOrder(std::move(order));
}

// ---------------------------------------------------------------------------
// Async producer path
// ---------------------------------------------------------------------------

void ThreadSafeExchangeCore::enqueueOrder(Order order) {
    orderQueue_.push(std::move(order));
}

std::size_t ThreadSafeExchangeCore::processAll() {
    std::size_t count = 0;
    Order order = Order::createMarketOrder(0, "_dummy", Side::Buy, 1);  // placeholder
    while (orderQueue_.tryPop(order)) {
        submitOrderLocked(std::move(order));
        ++count;
    }
    return count;
}

// ---------------------------------------------------------------------------
// Background thread
// ---------------------------------------------------------------------------

void ThreadSafeExchangeCore::startProcessingThread() {
    bool expected = false;
    if (!threadRunning_.compare_exchange_strong(expected, true)) {
        return;  // already running
    }
    processingThread_ = std::thread([this] {
        Order order = Order::createMarketOrder(0, "_dummy", Side::Buy, 1);
        while (orderQueue_.waitAndPop(order)) {
            submitOrderLocked(std::move(order));
        }
        threadRunning_.store(false, std::memory_order_relaxed);
    });
}

void ThreadSafeExchangeCore::stopProcessingThread() {
    orderQueue_.shutdown();
    if (processingThread_.joinable()) {
        processingThread_.join();
    }
}

// ---------------------------------------------------------------------------
// Thread-safe synchronous API
// ---------------------------------------------------------------------------

void ThreadSafeExchangeCore::submitOrder(Order order) {
    submitOrderLocked(std::move(order));
}

bool ThreadSafeExchangeCore::cancelOrder(const std::string& symbol, uint64_t orderId) {
    std::lock_guard<std::mutex> symLock(getSymbolMutex(symbol));
    return ExchangeCore::cancelOrder(symbol, orderId);
}

bool ThreadSafeExchangeCore::modifyOrder(const std::string& symbol, uint64_t orderId,
                                          double newPrice, uint32_t newQuantity) {
    std::lock_guard<std::mutex> symLock(getSymbolMutex(symbol));
    return ExchangeCore::modifyOrder(symbol, orderId, newPrice, newQuantity);
}

const OrderBook& ThreadSafeExchangeCore::getOrderBook(const std::string& symbol) const {
    // const_cast is safe: we only need the shared lock, not mutation.
    std::lock_guard<std::mutex> symLock(
        const_cast<ThreadSafeExchangeCore*>(this)->getSymbolMutex(symbol));
    return ExchangeCore::getOrderBook(symbol);
}

const TradeHistory& ThreadSafeExchangeCore::getTradeHistory(const std::string& symbol) const {
    std::lock_guard<std::mutex> symLock(
        const_cast<ThreadSafeExchangeCore*>(this)->getSymbolMutex(symbol));
    return ExchangeCore::getTradeHistory(symbol);
}

} // namespace miniexchange
