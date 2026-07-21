//
// Created by sudha on 14-07-2026.
//

#include "miniexchange/MatchingEngine.h"

namespace miniexchange {

MatchingEngine::MatchingEngine(OrderBook& book, TradeHistory& history)
    : book_(book), history_(history), nextTradeId_(1)
{
}

void MatchingEngine::submitOrder(Order incomingOrder) {
    if (incomingOrder.getSide() == Side::Buy) {
        matchAgainstAsks(incomingOrder);
    } else {
        matchAgainstBids(incomingOrder);
    }

    const bool isLimitOrder = incomingOrder.getPrice().has_value();
    if (isLimitOrder && incomingOrder.getRemainingQuantity() > 0) {
        book_.addOrder(std::move(incomingOrder));
    }
    // Market orders with leftover quantity are simply not rested (by design).
}

void MatchingEngine::setOnTradeCallback(TradeCallback callback) {
    tradeCallback_ = std::move(callback);
}

void MatchingEngine::matchAgainstAsks(Order& incomingBuyOrder) {
    while (incomingBuyOrder.getRemainingQuantity() > 0) {
        auto topAsk = book_.peekBestAsk();
        if (!topAsk.has_value()) {
            break;  // nothing left to match against
        }

        const bool isMarketOrder = !incomingBuyOrder.getPrice().has_value();
        if (!isMarketOrder && incomingBuyOrder.getPrice().value() < topAsk->price) {
            break;  // doesn't cross the spread
        }

        const uint32_t fillQty = std::min(incomingBuyOrder.getRemainingQuantity(),
                                           topAsk->remainingQuantity);

        incomingBuyOrder.fill(fillQty);
        book_.fillBestAsk(fillQty);

        Trade trade(
            nextTradeId_++, incomingBuyOrder.getSymbol(), topAsk->price, fillQty,
            incomingBuyOrder.getId(), topAsk->orderId, Order::Clock::now()
        );
        history_.recordTrade(trade);
        if (tradeCallback_) {
            tradeCallback_(trade);
        }
    }
}

void MatchingEngine::matchAgainstBids(Order& incomingSellOrder) {
    while (incomingSellOrder.getRemainingQuantity() > 0) {
        auto topBid = book_.peekBestBid();
        if (!topBid.has_value()) {
            break;
        }

        const bool isMarketOrder = !incomingSellOrder.getPrice().has_value();
        if (!isMarketOrder && incomingSellOrder.getPrice().value() > topBid->price) {
            break;
        }

        const uint32_t fillQty = std::min(incomingSellOrder.getRemainingQuantity(),
                                           topBid->remainingQuantity);

        incomingSellOrder.fill(fillQty);
        book_.fillBestBid(fillQty);

        Trade trade(
            nextTradeId_++, incomingSellOrder.getSymbol(), topBid->price, fillQty,
            topBid->orderId, incomingSellOrder.getId(), Order::Clock::now()
        );
        history_.recordTrade(trade);
        if (tradeCallback_) {
            tradeCallback_(trade);
        }
    }
}

    bool MatchingEngine::cancelOrder(uint64_t orderId) {
    return book_.cancelOrder(orderId);
}
    bool MatchingEngine::modifyOrder(uint64_t orderId, double newPrice, uint32_t newQuantity) {
    std::optional<Order> existing = book_.cancelAndReturnOrder(orderId);
    if (!existing.has_value()) {
        return false;
    }

    Order replacement = Order::createLimitOrder(
        orderId, existing->getSymbol(), existing->getSide(), newPrice, newQuantity
    );

    submitOrder(std::move(replacement));
    return true;
}



} // namespace miniexchange