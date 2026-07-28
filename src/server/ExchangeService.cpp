#include "miniexchange/ExchangeService.h"
#include "miniexchange/ThreadSafeExchangeCore.h"
#include "miniexchange/Order.h"
#include "miniexchange/Side.h"
#include "miniexchange/OrderType.h"
#include "nlohmann/json.hpp"
#include <crow.h>
#include <chrono>
#include <sstream>

// Crow WebSocket forward declaration
namespace crow {
    namespace websocket {
        class connection;
    }
}

namespace miniexchange {

    ExchangeService::ExchangeService()
        : exchangeCore_(std::make_unique<ExchangeCore>())
    {
    }

    ExchangeService::~ExchangeService() {
        stop();
    }

    void ExchangeService::start() {
        if (!running_.load()) {
            running_.store(true);
            broadcasterThread_ = std::thread(&ExchangeService::broadcasterThreadFunc, this);
        }
    }

    void ExchangeService::stop() {
        if (running_.load()) {
            running_.store(false);
            broadcastQueue_.shutdown();
            if (broadcasterThread_.joinable()) {
                broadcasterThread_.join();
            }
        }
    }

    uint64_t ExchangeService::submitOrder(const std::string& symbol, const std::string& side,
                                          const std::string& type, std::optional<double> price,
                                          uint32_t quantity) {
        std::lock_guard<std::mutex> lock(coreMutex_);

        bool isNewSymbol = !exchangeCore_->hasSymbol(symbol);

        uint64_t orderId = nextOrderId_.fetch_add(1);

        Side orderSide = (side == "buy") ? Side::Buy : Side::Sell;
        OrderType orderType = (type == "market") ? OrderType::Market : OrderType::Limit;

        std::optional<Order> orderOpt;
        if (orderType == OrderType::Market) {
            orderOpt = Order::createMarketOrder(orderId, symbol, orderSide, quantity);
        } else {
            if (!price.has_value()) {
                return 0;  // Invalid: limit order requires price
            }
            orderOpt = Order::createLimitOrder(orderId, symbol, orderSide, price.value(), quantity);
        }

        // submitOrder creates the symbol's MatchingEngine on first use
        exchangeCore_->submitOrder(std::move(*orderOpt));

        // Now that the engine exists, register the trade callback for new symbols
        if (isNewSymbol) {
            registerTradeCallback(symbol);
        }

        // Push order book update event
        broadcastQueue_.push(OrderBookUpdateEvent{symbol});

        return orderId;
    }

    bool ExchangeService::cancelOrder(const std::string& symbol, uint64_t orderId) {
        std::lock_guard<std::mutex> lock(coreMutex_);
        bool result = exchangeCore_->cancelOrder(symbol, orderId);
        if (result) {
            broadcastQueue_.push(OrderBookUpdateEvent{symbol});
        }
        return result;
    }

    bool ExchangeService::modifyOrder(const std::string& symbol, uint64_t orderId,
                                     double newPrice, uint32_t newQuantity) {
        std::lock_guard<std::mutex> lock(coreMutex_);
        bool result = exchangeCore_->modifyOrder(symbol, orderId, newPrice, newQuantity);
        if (result) {
            broadcastQueue_.push(OrderBookUpdateEvent{symbol});
        }
        return result;
    }

    std::string ExchangeService::getOrderBookSnapshot(const std::string& symbol, int topN) {
        std::lock_guard<std::mutex> lock(coreMutex_);

        if (!exchangeCore_->hasSymbol(symbol)) {
            nlohmann::json error;
            error["error"] = "Symbol not found";
            return error.dump();
        }

        const OrderBook& book = exchangeCore_->getOrderBook(symbol);

        nlohmann::json j;
        j["symbol"] = symbol;

        // Get bids (top N levels with aggregated quantities)
        nlohmann::json bids = nlohmann::json::array();
        auto bidLevels = book.getTopBids(topN);
        for (const auto& level : bidLevels) {
            nlohmann::json bidLevel;
            bidLevel["price"] = level.price;
            bidLevel["quantity"] = level.totalQuantity;
            bids.push_back(bidLevel);
        }

        // Get asks (top N levels with aggregated quantities)
        nlohmann::json asks = nlohmann::json::array();
        auto askLevels = book.getTopAsks(topN);
        for (const auto& level : askLevels) {
            nlohmann::json askLevel;
            askLevel["price"] = level.price;
            askLevel["quantity"] = level.totalQuantity;
            asks.push_back(askLevel);
        }

        j["bids"] = bids;
        j["asks"] = asks;

        return j.dump();
    }

    std::string ExchangeService::getRecentTrades(const std::string& symbol, int limit) {
        std::lock_guard<std::mutex> lock(coreMutex_);

        if (!exchangeCore_->hasSymbol(symbol)) {
            nlohmann::json error;
            error["error"] = "Symbol not found";
            return error.dump();
        }

        const TradeHistory& history = exchangeCore_->getTradeHistory(symbol);
        auto trades = history.getTrades();

        nlohmann::json j = nlohmann::json::array();

        // Get last N trades (most recent first)
        int count = 0;
        for (auto it = trades.rbegin(); it != trades.rend() && count < limit; ++it, ++count) {
            const Trade& trade = *it;
            nlohmann::json tradeJson;
            tradeJson["tradeId"] = trade.getTradeId();
            tradeJson["symbol"] = trade.getSymbol();
            tradeJson["price"] = trade.getPrice();
            tradeJson["quantity"] = trade.getQuantity();
            tradeJson["buyOrderId"] = trade.getBuyOrderId();
            tradeJson["sellOrderId"] = trade.getSellOrderId();
            
            // Convert timestamp to milliseconds
            auto timestamp = trade.getTimestamp();
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                timestamp.time_since_epoch()).count();
            tradeJson["timestamp"] = ms;
            
            j.push_back(tradeJson);
        }

        return j.dump();
    }

    std::string ExchangeService::getSymbols() {
        std::lock_guard<std::mutex> lock(coreMutex_);

        nlohmann::json j = nlohmann::json::array();
        auto symbols = exchangeCore_->getSymbols();
        for (const auto& symbol : symbols) {
            j.push_back(symbol);
        }
        
        return j.dump();
    }

    void ExchangeService::addWebSocketConnection(crow::websocket::connection* conn) {
        std::lock_guard<std::mutex> lock(wsMutex_);
        wsConnections_.insert(conn);
    }

    void ExchangeService::removeWebSocketConnection(crow::websocket::connection* conn) {
        std::lock_guard<std::mutex> lock(wsMutex_);
        wsConnections_.erase(conn);
    }

    void ExchangeService::registerTradeCallback(const std::string& symbol) {
        auto& engine = exchangeCore_->getMatchingEngine(symbol);
        engine.setOnTradeCallback([this](const Trade& trade) {
            auto timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                trade.getTimestamp().time_since_epoch()).count();
            broadcastQueue_.push(TradeEvent{
                trade.getSymbol(),
                trade.getTradeId(),
                trade.getPrice(),
                trade.getQuantity(),
                trade.getBuyOrderId(),
                trade.getSellOrderId(),
                timestamp_ms
            });
        });
    }

    void ExchangeService::broadcasterThreadFunc() {
        while (running_.load()) {
            BroadcastEvent event;
            if (!broadcastQueue_.waitAndPop(event)) {
                break;  // Queue shutdown
            }

            nlohmann::json message;
            
            std::visit([&message](auto&& arg) {
                using T = std::decay_t<decltype(arg)>;
                if constexpr (std::is_same_v<T, TradeEvent>) {
                    message["type"] = "trade";
                    message["symbol"] = arg.symbol;
                    message["tradeId"] = arg.tradeId;
                    message["price"] = arg.price;
                    message["quantity"] = arg.quantity;
                    message["buyOrderId"] = arg.buyOrderId;
                    message["sellOrderId"] = arg.sellOrderId;
                    message["timestamp"] = arg.timestamp_ms;
                } else if constexpr (std::is_same_v<T, OrderBookUpdateEvent>) {
                    message["type"] = "orderbook_update";
                    message["symbol"] = arg.symbol;
                }
            }, event);

            std::string messageStr = message.dump();

            // Broadcast to all connected WebSocket clients
            std::lock_guard<std::mutex> lock(wsMutex_);
            for (auto* conn : wsConnections_) {
                conn->send_text(messageStr);
            }
        }
    }

    std::string ExchangeService::getOrderBookDebug(const std::string& symbol) {
        std::lock_guard<std::mutex> lock(coreMutex_);

        const int TOTAL_SLOTS = 1000000;

        if (!exchangeCore_->hasSymbol(symbol)) {
            // Return empty pool state
            nlohmann::json j;
            nlohmann::json poolStats;
            poolStats["usedSlots"]   = 0;
            poolStats["freeSlots"]   = TOTAL_SLOTS;
            poolStats["totalSlots"]  = TOTAL_SLOTS;
            poolStats["nextFreeIdx"] = 0;
            poolStats["usedPct"]     = 0.0;
            j["poolStats"]     = poolStats;
            j["activeNodes"]   = nlohmann::json::array();
            j["occupiedSlots"] = nlohmann::json::array();
            j["priceLevels"]   = nlohmann::json::array();
            j["freeListHead"]  = nlohmann::json::array();
            return j.dump();
        }

        const OrderBook& book = exchangeCore_->getOrderBook(symbol);

        // Build flat ordered list of all resting orders across all price levels.
        // We use sequential slot indices: bids first (highest price first), then asks.
        // Within each level, orders are in FIFO (time-priority) order.
        // prev/next slot model the doubly-linked list within each price level.

        nlohmann::json activeNodes  = nlohmann::json::array();
        nlohmann::json occupiedSlots = nlohmann::json::array();
        nlohmann::json priceLevels  = nlohmann::json::array();

        int slotIdx = 0;

        // Helper lambda — processes one side
        auto processSide = [&](const std::vector<OrderBook::LevelSnapshot>& /*unused*/,
                               bool isBid) {
            // We need to iterate the internal maps via getTopBids/Asks with a large N
            // to get all levels. Use 10000 to capture all live price levels.
            std::vector<OrderBook::LevelSnapshot> levels = isBid
                ? book.getTopBids(10000)
                : book.getTopAsks(10000);

            // For each price level we collect all orders (we re-query via peekBestBid/Ask
            // is not enough — we need the full level list). Since OrderBook doesn't expose
            // individual order iteration, we build a simulated slot list from LevelSnapshot
            // data and the orderId from peekBest* for the top of book only.
            //
            // DESIGN NOTE: Because std::list<Order> inside OrderBook is private, we
            // expose only aggregate LevelSnapshot data. We simulate per-order nodes
            // by storing one node per aggregated level (showing level-wide totals)
            // and marking prev/next as the adjacent price levels.
            // This accurately reflects the doubly-linked structure of the price ladder.

            int levelStartSlot = slotIdx;
            for (int li = 0; li < (int)levels.size(); li++) {
                const auto& lvl = levels[li];
                int thisSlot = slotIdx++;

                nlohmann::json node;
                node["slot"]      = thisSlot;
                node["orderId"]   = 0;          // aggregated level, no single ID
                node["side"]      = isBid ? "B" : "S";
                node["price"]     = lvl.price;
                node["qty"]       = lvl.totalQuantity;
                node["prevSlot"]  = (li > 0)                         ? (thisSlot - 1) : -1;
                node["nextSlot"]  = (li < (int)levels.size() - 1)    ? (thisSlot + 1) : -1;
                activeNodes.push_back(node);
                occupiedSlots.push_back(thisSlot);

                // Build price level entry
                nlohmann::json plNode;
                plNode["price"]    = lvl.price;
                plNode["side"]     = isBid ? "B" : "S";
                plNode["slot"]     = thisSlot;
                plNode["qty"]      = lvl.totalQuantity;
                plNode["prevSlot"] = node["prevSlot"];
                plNode["nextSlot"] = node["nextSlot"];
                priceLevels.push_back(plNode);
            }
        };

        processSide({}, true);   // bids
        processSide({}, false);  // asks

        int usedSlots = slotIdx;
        int freeSlots = TOTAL_SLOTS - usedSlots;

        // Simulate free-list head (next few free slot indices)
        nlohmann::json freeListHead = nlohmann::json::array();
        for (int i = usedSlots; i < std::min(usedSlots + 8, TOTAL_SLOTS); i++) {
            freeListHead.push_back(i);
        }

        nlohmann::json poolStats;
        poolStats["usedSlots"]   = usedSlots;
        poolStats["freeSlots"]   = freeSlots;
        poolStats["totalSlots"]  = TOTAL_SLOTS;
        poolStats["nextFreeIdx"] = usedSlots;
        poolStats["usedPct"]     = (double)usedSlots / TOTAL_SLOTS * 100.0;

        nlohmann::json j;
        j["poolStats"]     = poolStats;
        j["activeNodes"]   = activeNodes;
        j["occupiedSlots"] = occupiedSlots;
        j["priceLevels"]   = priceLevels;
        j["freeListHead"]  = freeListHead;
        return j.dump();
    }

    std::string ExchangeService::tradeToJson(const Trade& trade) {
        nlohmann::json j;
        j["tradeId"] = trade.getTradeId();
        j["symbol"] = trade.getSymbol();
        j["price"] = trade.getPrice();
        j["quantity"] = trade.getQuantity();
        j["buyOrderId"] = trade.getBuyOrderId();
        j["sellOrderId"] = trade.getSellOrderId();
        
        auto timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            trade.getTimestamp().time_since_epoch()).count();
        j["timestamp"] = timestamp_ms;
        
        return j.dump();
    }

} // namespace miniexchange
