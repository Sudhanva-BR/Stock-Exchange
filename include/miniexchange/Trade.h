// //
// // Created by sudha on 14-07-2026.
// //
//
// #ifndef MINIEXCHANGE_TRADE_H
// #define MINIEXCHANGE_TRADE_H
//
// #endif //MINIEXCHANGE_TRADE_H

#pragma once

#include <cstdint>
#include <string>
#include <chrono>

namespace miniexchange {

    class Trade {
    public:
        using TimePoint = std::chrono::system_clock::time_point;

        Trade(uint64_t tradeId, std::string symbol, double price, uint32_t quantity,
              uint64_t buyOrderId, uint64_t sellOrderId, TimePoint timestamp);

        uint64_t getTradeId() const noexcept;
        const std::string& getSymbol() const noexcept;
        double getPrice() const noexcept;
        uint32_t getQuantity() const noexcept;
        uint64_t getBuyOrderId() const noexcept;
        uint64_t getSellOrderId() const noexcept;
        TimePoint getTimestamp() const noexcept;

    private:
        uint64_t tradeId_;
        std::string symbol_;
        double price_;
        uint32_t quantity_;
        uint64_t buyOrderId_;
        uint64_t sellOrderId_;
        TimePoint timestamp_;
    };

} // namespace miniexchange