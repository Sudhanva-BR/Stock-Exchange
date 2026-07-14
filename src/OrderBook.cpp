#include "miniexchange/OrderBook.h"
#include <stdexcept>
#include <iterator>

namespace miniexchange {

OrderBook::OrderBook(std::string symbol)
    : symbol_(std::move(symbol))
{
}

void OrderBook::addOrder(Order order) {
    if (!order.getPrice().has_value()) {
        throw std::invalid_argument("OrderBook::addOrder: only limit orders (with a price) can rest on the book");
    }

    const uint64_t id = order.getId();
    const Side side = order.getSide();
    const double price = order.getPrice().value();

    if (side == Side::Buy) {
        auto& level = bids_[price];
        level.emplace_back(std::move(order));
        orderLocationIndex_[id] = OrderLocation{side, price, std::prev(level.end())};
    } else {
        auto& level = asks_[price];
        level.emplace_back(std::move(order));
        orderLocationIndex_[id] = OrderLocation{side, price, std::prev(level.end())};
    }
}

bool OrderBook::cancelOrder(uint64_t orderId) {
    auto indexIt = orderLocationIndex_.find(orderId);
    if (indexIt == orderLocationIndex_.end()) {
        return false;
    }

    const OrderLocation& location = indexIt->second;

    if (location.side == Side::Buy) {
        auto levelIt = bids_.find(location.price);
        levelIt->second.erase(location.iterator);
        if (levelIt->second.empty()) {
            bids_.erase(levelIt);
        }
    } else {
        auto levelIt = asks_.find(location.price);
        levelIt->second.erase(location.iterator);
        if (levelIt->second.empty()) {
            asks_.erase(levelIt);
        }
    }

    orderLocationIndex_.erase(indexIt);
    return true;
}

std::optional<double> OrderBook::getBestBid() const noexcept {
    if (bids_.empty()) {
        return std::nullopt;
    }
    return bids_.begin()->first;
}

std::optional<double> OrderBook::getBestAsk() const noexcept {
    if (asks_.empty()) {
        return std::nullopt;
    }
    return asks_.begin()->first;
}

const std::string& OrderBook::getSymbol() const noexcept {
    return symbol_;
}

    std::optional<OrderBook::TopOfBook> OrderBook::peekBestBid() const noexcept {
    if (bids_.empty()) {
        return std::nullopt;
    }
    const auto& [price, level] = *bids_.begin();
    const Order& front = level.front();
    return TopOfBook{front.getId(), price, front.getRemainingQuantity()};
}

    std::optional<OrderBook::TopOfBook> OrderBook::peekBestAsk() const noexcept {
    if (asks_.empty()) {
        return std::nullopt;
    }
    const auto& [price, level] = *asks_.begin();
    const Order& front = level.front();
    return TopOfBook{front.getId(), price, front.getRemainingQuantity()};
}

    void OrderBook::fillBestBid(uint32_t quantity) {
    auto levelIt = bids_.begin();
    if (levelIt == bids_.end()) {
        throw std::logic_error("fillBestBid called on empty bid side");
    }
    Order& front = levelIt->second.front();
    front.fill(quantity);

    if (front.getRemainingQuantity() == 0) {
        orderLocationIndex_.erase(front.getId());
        levelIt->second.pop_front();
        if (levelIt->second.empty()) {
            bids_.erase(levelIt);
        }
    }
}

    void OrderBook::fillBestAsk(uint32_t quantity) {
    auto levelIt = asks_.begin();
    if (levelIt == asks_.end()) {
        throw std::logic_error("fillBestAsk called on empty ask side");
    }
    Order& front = levelIt->second.front();
    front.fill(quantity);

    if (front.getRemainingQuantity() == 0) {
        orderLocationIndex_.erase(front.getId());
        levelIt->second.pop_front();
        if (levelIt->second.empty()) {
            asks_.erase(levelIt);
        }
    }
}
    std::optional<Order> OrderBook::cancelAndReturnOrder(uint64_t orderId) {
    auto indexIt = orderLocationIndex_.find(orderId);
    if (indexIt == orderLocationIndex_.end()) {
        return std::nullopt;
    }

    const OrderLocation location = indexIt->second; // copy before we invalidate it below

    Order removedOrder = std::move(*location.iterator);

    // bids_ and asks_ have different comparator types so they cannot be aliased
    // by the same reference via a ternary — use explicit if/else branches instead.
    if (location.side == Side::Buy) {
        auto levelIt = bids_.find(location.price);
        levelIt->second.erase(location.iterator);
        if (levelIt->second.empty()) {
            bids_.erase(levelIt);
        }
    } else {
        auto levelIt = asks_.find(location.price);
        levelIt->second.erase(location.iterator);
        if (levelIt->second.empty()) {
            asks_.erase(levelIt);
        }
    }

    orderLocationIndex_.erase(indexIt);
    return removedOrder;
}

} // namespace miniexchange