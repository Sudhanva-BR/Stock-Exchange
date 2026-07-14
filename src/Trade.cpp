//
// Created by sudha on 14-07-2026.
//

#include "miniexchange/Trade.h"

namespace miniexchange {

    Trade::Trade(uint64_t tradeId, std::string symbol, double price, uint32_t quantity,
                 uint64_t buyOrderId, uint64_t sellOrderId, TimePoint timestamp)
        : tradeId_(tradeId),
          symbol_(std::move(symbol)),
          price_(price),
          quantity_(quantity),
          buyOrderId_(buyOrderId),
          sellOrderId_(sellOrderId),
          timestamp_(timestamp)
    {
    }

    uint64_t Trade::getTradeId() const noexcept { return tradeId_; }
    const std::string& Trade::getSymbol() const noexcept { return symbol_; }
    double Trade::getPrice() const noexcept { return price_; }
    uint32_t Trade::getQuantity() const noexcept { return quantity_; }
    uint64_t Trade::getBuyOrderId() const noexcept { return buyOrderId_; }
    uint64_t Trade::getSellOrderId() const noexcept { return sellOrderId_; }
    Trade::TimePoint Trade::getTimestamp() const noexcept { return timestamp_; }

} // namespace miniexchange