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
