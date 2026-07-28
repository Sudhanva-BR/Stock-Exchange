#include "miniexchange/ExchangeCore.h"

#include <stdexcept>

namespace miniexchange {

// ---------------------------------------------------------------------------
// Private helper
// ---------------------------------------------------------------------------

ExchangeCore::SymbolData& ExchangeCore::getOrCreate(const std::string& symbol) {
    // emplace is a no-op if the key already exists; it only constructs SymbolData
    // (and therefore MatchingEngine) on first insertion.
    // std::piecewise_construct forwards arguments directly into the value
    // constructor so we never need a move or copy of SymbolData.
    auto [it, _] = symbols_.emplace(
        std::piecewise_construct,
        std::forward_as_tuple(symbol),          // key
        std::forward_as_tuple(symbol)           // SymbolData(std::string symbol)
    );
    return it->second;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void ExchangeCore::submitOrder(Order order) {
    const std::string symbol = order.getSymbol();  // copy before move
    getOrCreate(symbol).engine.submitOrder(std::move(order));
}

bool ExchangeCore::cancelOrder(const std::string& symbol, uint64_t orderId) {
    auto it = symbols_.find(symbol);
    if (it == symbols_.end()) {
        return false;
    }
    return it->second.engine.cancelOrder(orderId);
}

bool ExchangeCore::modifyOrder(const std::string& symbol, uint64_t orderId,
                                double newPrice, uint32_t newQuantity) {
    auto it = symbols_.find(symbol);
    if (it == symbols_.end()) {
        return false;
    }
    return it->second.engine.modifyOrder(orderId, newPrice, newQuantity);
}

const OrderBook& ExchangeCore::getOrderBook(const std::string& symbol) const {
    auto it = symbols_.find(symbol);
    if (it == symbols_.end()) {
        throw std::out_of_range("ExchangeCore::getOrderBook: unknown symbol: " + symbol);
    }
    return it->second.book;
}

void ExchangeCore::accessOrderBook(const std::string& symbol, const std::function<void(const OrderBook&)>& accessor) const {
    accessor(getOrderBook(symbol));
}

const TradeHistory& ExchangeCore::getTradeHistory(const std::string& symbol) const {
    auto it = symbols_.find(symbol);
    if (it == symbols_.end()) {
        throw std::out_of_range("ExchangeCore::getTradeHistory: unknown symbol: " + symbol);
    }
    return it->second.history;
}

bool ExchangeCore::hasSymbol(const std::string& symbol) const noexcept {
    return symbols_.find(symbol) != symbols_.end();
}

std::vector<std::string> ExchangeCore::getSymbols() const {
    std::vector<std::string> result;
    for (const auto& [sym, data] : symbols_) {
        result.push_back(sym);
    }
    return result;
}

MatchingEngine& ExchangeCore::getMatchingEngine(const std::string& symbol) {
    auto it = symbols_.find(symbol);
    if (it == symbols_.end()) {
        throw std::out_of_range("ExchangeCore::getMatchingEngine: unknown symbol: " + symbol);
    }
    return it->second.engine;
}

} // namespace miniexchange
