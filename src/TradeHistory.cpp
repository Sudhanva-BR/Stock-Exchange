#include "miniexchange/TradeHistory.h"

#include <algorithm>
#include <numeric>

namespace miniexchange {

    void TradeHistory::recordTrade(Trade trade) {
        trades_.push_back(std::move(trade));
    }

    const std::vector<Trade>& TradeHistory::getTrades() const noexcept {
        return trades_;
    }

    // --- Milestone 6: Query implementations ---

    std::vector<Trade> TradeHistory::getTradesBySymbol(const std::string& symbol) const {
        std::vector<Trade> result;
        std::copy_if(trades_.begin(), trades_.end(), std::back_inserter(result),
                     [&symbol](const Trade& t) { return t.getSymbol() == symbol; });
        return result;
    }

    std::vector<Trade> TradeHistory::getTradesInRange(Trade::TimePoint from,
                                                       Trade::TimePoint to) const {
        std::vector<Trade> result;
        std::copy_if(trades_.begin(), trades_.end(), std::back_inserter(result),
                     [&from, &to](const Trade& t) {
                         return t.getTimestamp() >= from && t.getTimestamp() <= to;
                     });
        return result;
    }

    TradeHistory::VolumeStats TradeHistory::getVolumeStats() const noexcept {
        if (trades_.empty()) {
            return {};  // {totalVolume=0, vwap=0.0}
        }

        // Accumulate total volume and price*qty sum in a single pass.
        uint64_t totalVol  = 0;
        double   priceQsum = 0.0;
        for (const Trade& t : trades_) {
            totalVol  += t.getQuantity();
            priceQsum += static_cast<double>(t.getQuantity()) * t.getPrice();
        }

        return VolumeStats{
            totalVol,
            totalVol > 0 ? priceQsum / static_cast<double>(totalVol) : 0.0
        };
    }

} // namespace miniexchange