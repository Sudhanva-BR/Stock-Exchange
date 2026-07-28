#pragma once

#include <memory>
#include <thread>
#include <mutex>
#include <unordered_set>
#include <atomic>
#include <string>
#include <variant>
#include <functional>

#include "miniexchange/ExchangeCore.h"
#include "miniexchange/ThreadSafeQueue.h"
#include "miniexchange/Trade.h"

// Forward declaration for Crow WebSocket
namespace crow {
    namespace websocket {
        struct connection;
    }
}

namespace miniexchange {

    // Broadcast event types pushed to the queue
    struct TradeEvent {
        std::string symbol;
        uint64_t tradeId;
        double price;
        uint32_t quantity;
        uint64_t buyOrderId;
        uint64_t sellOrderId;
        uint64_t timestamp_ms;  // Unix timestamp in milliseconds
    };

    struct OrderBookUpdateEvent {
        std::string symbol;
    };

    using BroadcastEvent = std::variant<TradeEvent, OrderBookUpdateEvent>;

    // ExchangeService wraps ExchangeCore with web server capabilities
    // - Owns ExchangeCore instance
    // - Manages thread-safe broadcast queue for WebSocket updates
    // - Registers trade callbacks on MatchingEngines
    // - Runs broadcaster thread to push events to WebSocket clients
    class ExchangeService {
    public:
        ExchangeService();
        ~ExchangeService();

        // Non-copyable, non-movable
        ExchangeService(const ExchangeService&) = delete;
        ExchangeService& operator=(const ExchangeService&) = delete;
        ExchangeService(ExchangeService&&) = delete;
        ExchangeService& operator=(ExchangeService&&) = delete;

        // --- Order operations (thread-safe) ---

        uint64_t submitOrder(const std::string& symbol, const std::string& side,
                            const std::string& type, std::optional<double> price,
                            uint32_t quantity);

        bool cancelOrder(const std::string& symbol, uint64_t orderId);

        bool modifyOrder(const std::string& symbol, uint64_t orderId,
                        double newPrice, uint32_t newQuantity);

        // --- Read operations (thread-safe) ---

        // Get JSON snapshot of order book (top N price levels per side)
        std::string getOrderBookSnapshot(const std::string& symbol, int topN = 10);

        // Get recent trades as JSON
        std::string getRecentTrades(const std::string& symbol, int limit = 50);

        // Get list of active symbols
        std::string getSymbols();

        // Get raw per-order node data for the debug panel (memory pool / node struct visualization)
        // Returns a JSON object with: activeNodes (array of {slot,orderId,side,price,qty,prevSlot,nextSlot}),
        // poolStats (usedSlots, freeSlots, totalSlots, nextFreeIdx),
        // occupiedSlots (array of int indices), priceLevels (array of price-level linked lists)
        std::string getOrderBookDebug(const std::string& symbol);

        // --- WebSocket management ---

        // Add/remove WebSocket connections for broadcasting
        void addWebSocketConnection(crow::websocket::connection* conn);
        void removeWebSocketConnection(crow::websocket::connection* conn);

        // Start/stop the broadcaster thread
        void start();
        void stop();

    private:
        // Register trade callback for a newly created symbol
        void registerTradeCallback(const std::string& symbol);

        // Broadcaster thread function
        void broadcasterThreadFunc();

        // Convert trade to JSON string
        std::string tradeToJson(const Trade& trade);

        // Core exchange instance
        std::unique_ptr<ExchangeCore> exchangeCore_;

        // Thread-safe queue for broadcast events
        ThreadSafeQueue<BroadcastEvent> broadcastQueue_;

        // WebSocket connections (managed by Crow, we just store pointers)
        std::unordered_set<crow::websocket::connection*> wsConnections_;
        std::mutex wsMutex_;

        // Next order ID (atomic for thread-safety)
        std::atomic<uint64_t> nextOrderId_{1};

        // Broadcaster thread
        std::thread broadcasterThread_;
        std::atomic<bool> running_{false};

        // Mutex for ExchangeCore access (per-symbol locking exists in ThreadSafeExchangeCore,
        // but we use this for symbol creation/symbol list access)
        std::mutex coreMutex_;
    };

} // namespace miniexchange
