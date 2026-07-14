//
// Created by sudha on 14-07-2026.
//
#pragma once
// #ifndef MINI_EXCHANGE_ORDER_H
// #define MINI_EXCHANGE_ORDER_H
//
// #endif //MINI_EXCHANGE_ORDER_H

#pragma once

#include <cstdint>
#include <string>
#include <chrono>
#include <optional>

#include "miniexchange/Side.h"
#include "miniexchange/OrderType.h"
#include "miniexchange/OrderStatus.h"

namespace miniexchange {

    class Order {
    public:
        using Clock = std::chrono::system_clock;
        using TimePoint = Clock::time_point;

        // Named constructors (factory functions)
        static Order createLimitOrder(uint64_t id, std::string symbol, Side side,
                                       double price, uint32_t quantity);

        static Order createMarketOrder(uint64_t id, std::string symbol, Side side,
                                        uint32_t quantity);

        // Read-only accessors
        uint64_t getId() const noexcept;
        const std::string& getSymbol() const noexcept;
        Side getSide() const noexcept;
        OrderType getType() const noexcept;
        std::optional<double> getPrice() const noexcept;
        uint32_t getQuantity() const noexcept;
        uint32_t getRemainingQuantity() const noexcept;
        OrderStatus getStatus() const noexcept;
        TimePoint getTimestamp() const noexcept;

        // The only two operations allowed to mutate an order
        void fill(uint32_t fillQuantity);
        void cancel() noexcept;

    private:
        Order(uint64_t id, std::string symbol, Side side, OrderType type,
              std::optional<double> price, uint32_t quantity);

        uint64_t id_;
        std::string symbol_;
        Side side_;
        OrderType type_;
        std::optional<double> price_;
        uint32_t quantity_;
        uint32_t remainingQuantity_;
        OrderStatus status_;
        TimePoint timestamp_;
    };

} // namespace miniexchange