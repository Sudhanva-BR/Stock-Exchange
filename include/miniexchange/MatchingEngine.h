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

namespace miniexchange {

    class MatchingEngine {
    public:
        MatchingEngine(OrderBook& book, TradeHistory& history);

        void submitOrder(Order incomingOrder);
        bool cancelOrder(uint64_t orderId);
        bool modifyOrder(uint64_t orderId, double newPrice, uint32_t newQuantity);

    private:
        void matchAgainstAsks(Order& incomingBuyOrder);
        void matchAgainstBids(Order& incomingSellOrder);

        OrderBook& book_;
        TradeHistory& history_;
        uint64_t nextTradeId_;
    };

} // namespace miniexchange