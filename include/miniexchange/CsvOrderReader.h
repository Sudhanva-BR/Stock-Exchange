#pragma once

#include <filesystem>
#include <vector>
#include <string>

#include "miniexchange/Order.h"
#include "miniexchange/ExchangeCore.h"

namespace miniexchange {

    // Reads a CSV file and either returns parsed orders or replays them directly
    // into an ExchangeCore.
    //
    // Expected CSV format (header row required):
    //   orderId,symbol,side,type,price,quantity
    //
    //   orderId  : uint64_t — caller-supplied ID (must be unique per design)
    //   symbol   : string  — e.g. "AAPL"
    //   side     : "buy" or "sell" (case-insensitive)
    //   type     : "limit" or "market" (case-insensitive)
    //   price    : double  — required for limit orders; empty/omitted for market orders
    //   quantity : uint32_t — must be positive
    //
    // Blank lines are skipped.
    // On any parse error, std::runtime_error is thrown with the offending line number.
    class CsvOrderReader {
    public:
        explicit CsvOrderReader(std::filesystem::path csvPath);

        // Parse all rows and return the resulting Order objects.
        // Does NOT submit them to any engine.
        std::vector<Order> parseOrders() const;

        // Parse and submit every order into the given ExchangeCore in CSV row order.
        void replayInto(ExchangeCore& core) const;

    private:
        std::filesystem::path path_;
    };

} // namespace miniexchange
