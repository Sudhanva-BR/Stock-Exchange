#include "miniexchange/Order.h"
#include <stdexcept>
#include "miniexchange/Side.h"

namespace miniexchange {

Order::Order(uint64_t id, std::string symbol, Side side, OrderType type,
             std::optional<double> price, uint32_t quantity)
    : id_(id),
      symbol_(std::move(symbol)),
      side_(side),
      type_(type),
      price_(price),
      quantity_(quantity),
      remainingQuantity_(quantity),
      status_(OrderStatus::New),
      timestamp_(Clock::now())
{
}

Order Order::createLimitOrder(uint64_t id, std::string symbol, Side side,
                               double price, uint32_t quantity) {
    if (price <= 0.0) {
        throw std::invalid_argument("Limit order price must be positive");
    }
    if (quantity == 0) {
        throw std::invalid_argument("Order quantity must be positive");
    }
    return Order(id, std::move(symbol), side, OrderType::Limit, price, quantity);
}

Order Order::createMarketOrder(uint64_t id, std::string symbol, Side side,
                                uint32_t quantity) {
    if (quantity == 0) {
        throw std::invalid_argument("Order quantity must be positive");
    }
    return Order(id, std::move(symbol), side, OrderType::Market, std::nullopt, quantity);
}

uint64_t Order::getId() const noexcept { return id_; }
const std::string& Order::getSymbol() const noexcept { return symbol_; }
Side Order::getSide() const noexcept { return side_; }
OrderType Order::getType() const noexcept { return type_; }
std::optional<double> Order::getPrice() const noexcept { return price_; }
uint32_t Order::getQuantity() const noexcept { return quantity_; }
uint32_t Order::getRemainingQuantity() const noexcept { return remainingQuantity_; }
OrderStatus Order::getStatus() const noexcept { return status_; }
Order::TimePoint Order::getTimestamp() const noexcept { return timestamp_; }

void Order::fill(uint32_t fillQuantity) {
    if (fillQuantity > remainingQuantity_) {
        throw std::invalid_argument("Fill quantity exceeds remaining quantity");
    }
    remainingQuantity_ -= fillQuantity;
    status_ = (remainingQuantity_ == 0) ? OrderStatus::Filled
                                         : OrderStatus::PartiallyFilled;
}

void Order::cancel() noexcept {
    status_ = OrderStatus::Cancelled;
}

} // namespace miniexchange