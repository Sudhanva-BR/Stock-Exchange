//
// Created by sudha on 14-07-2026.
//

// #ifndef MINIEXCHANGE_TRADEHISTORY_H
// #define MINIEXCHANGE_TRADEHISTORY_H
//
// #endif //MINIEXCHANGE_TRADEHISTORY_H

#pragma once

#include <vector>
#include <string>
#include <cstdint>
#include "miniexchange/Trade.h"

namespace miniexchange {

    class TradeHistory {
    public:
        // --- Existing API (unchanged) ---
        void recordTrade(Trade trade);
        const std::vector<Trade>& getTrades() const noexcept;

        // --- Milestone 6: Query extensions ---

        // Returns all trades whose symbol matches the given symbol string.
        std::vector<Trade> getTradesBySymbol(const std::string& symbol) const;

        // Returns all trades whose timestamp falls within [from, to] inclusive.
        std::vector<Trade> getTradesInRange(Trade::TimePoint from,
                                            Trade::TimePoint to) const;

        // Aggregated statistics for all trades (or a specific symbol when provided).
        struct VolumeStats {
            uint64_t totalVolume{0};  // sum of trade quantities
            double   vwap{0.0};       // volume-weighted average price; 0.0 if no trades
        };

        // Computes VWAP across all trades recorded in this history.
        // Use the per-symbol TradeHistory on ExchangeCore for symbol-scoped stats,
        // or call getTradesBySymbol() first if mixing symbols is ever needed.
        VolumeStats getVolumeStats() const noexcept;

    private:
        std::vector<Trade> trades_;
    };

} // namespace miniexchange