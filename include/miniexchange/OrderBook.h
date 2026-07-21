// //
// // Created by sudhanva on 14-07-2026.
// //
//
// #ifndef MINIEXCHANGE_ORDERBOOK_H
// #define MINIEXCHANGE_ORDERBOOK_H
//
// #endif //MINIEXCHANGE_ORDERBOOK_H

#pragma once

#include <string>
#include <map>
#include <list>
#include <unordered_map>
#include <optional>
#include <cstdint>

#include "miniexchange/Order.h"
#include "miniexchange/Side.h"

namespace miniexchange {

    class OrderBook {
    public:
        explicit OrderBook(std::string symbol);

        void addOrder(Order order);
        bool cancelOrder(uint64_t orderId);

        std::optional<double> getBestBid() const noexcept;
        std::optional<double> getBestAsk() const noexcept;

        struct TopOfBook {
            uint64_t orderId;
            double price;
            uint32_t remainingQuantity;
        };

        struct LevelSnapshot {
            double price;
            uint32_t totalQuantity;
        };

        std::optional<TopOfBook> peekBestBid() const noexcept;
        std::optional<TopOfBook> peekBestAsk() const noexcept;

        std::vector<LevelSnapshot> getTopBids(int topN) const;
        std::vector<LevelSnapshot> getTopAsks(int topN) const;

        void fillBestBid(uint32_t quantity);
        void fillBestAsk(uint32_t quantity);

        const std::string& getSymbol() const noexcept;
        std::optional<Order> cancelAndReturnOrder(uint64_t orderId);

    private:
        struct OrderLocation {
            Side side;
            double price;
            std::list<Order>::iterator iterator;
        };

        std::string symbol_;
        std::map<double, std::list<Order>, std::greater<double>> bids_;
        std::map<double, std::list<Order>> asks_;
        std::unordered_map<uint64_t, OrderLocation> orderLocationIndex_;
    };

} // namespace miniexchange