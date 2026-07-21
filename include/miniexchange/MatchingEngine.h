//
// Created by sudha on 14-07-2026.
//

// #ifndef MINIEXCHANGE_MATCHINGENGINE_H
// #define MINIEXCHANGE_MATCHINGENGINE_H
//
// #endif //MINIEXCHANGE_MATCHINGENGINE_H

#pragma once

#include "miniexchange/Order.h"
#include "miniexchange/OrderBook.h"
#include "miniexchange/TradeHistory.h"
#include <functional>

namespace miniexchange {

    class MatchingEngine {
    public:
        MatchingEngine(OrderBook& book, TradeHistory& history);

        void submitOrder(Order incomingOrder);
        bool cancelOrder(uint64_t orderId);
        bool modifyOrder(uint64_t orderId, double newPrice, uint32_t newQuantity);

        using TradeCallback = std::function<void(const Trade&)>;
        void setOnTradeCallback(TradeCallback callback);

    private:
        void matchAgainstAsks(Order& incomingBuyOrder);
        void matchAgainstBids(Order& incomingSellOrder);

        OrderBook& book_;
        TradeHistory& history_;
        uint64_t nextTradeId_;
        TradeCallback tradeCallback_;
    };

} // namespace miniexchange