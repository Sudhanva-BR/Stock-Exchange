#pragma once

#include <string>
#include <unordered_map>
#include <optional>

#include "miniexchange/Order.h"
#include "miniexchange/OrderBook.h"
#include "miniexchange/TradeHistory.h"
#include "miniexchange/MatchingEngine.h"

namespace miniexchange {

    // ExchangeCore is the top-level multi-symbol router.
    //
    // Architecture:
    //   For each symbol it owns a SymbolData bundle {OrderBook, TradeHistory,
    //   MatchingEngine} stored as a value inside an unordered_map node.
    //   unordered_map guarantees that existing node addresses (and therefore the
    //   references held by MatchingEngine into OrderBook/TradeHistory) are NOT
    //   invalidated when new symbols are inserted — the key correctness invariant.
    //
    // Construction order:
    //   MatchingEngine has no default constructor because it holds non-const
    //   references.  SymbolData's constructor initialises book_ and history_
    //   first (member declaration order) then wires engine_ to them — this is
    //   safe because all three live in the same SymbolData object whose address
    //   is stable inside the map node.
    class ExchangeCore {
    public:
        ExchangeCore() = default;

        // Non-copyable, non-movable: MatchingEngine holds references into
        // sibling members of the same SymbolData.  Moving the map would
        // invalidate those references (unordered_map move is node-stable for
        // values but not for the enclosing object address when holding internal
        // references between members).
        ExchangeCore(const ExchangeCore&) = delete;
        ExchangeCore& operator=(const ExchangeCore&) = delete;
        ExchangeCore(ExchangeCore&&) = delete;
        ExchangeCore& operator=(ExchangeCore&&) = delete;

        // --- Order routing ---

        // Routes order to the correct symbol's MatchingEngine,
        // creating a new {OrderBook, TradeHistory, MatchingEngine} triplet on
        // first use of that symbol.
        void submitOrder(Order order);

        // Cancels the order with the given ID in the named symbol's book.
        // Returns false if symbol not found or order not found.
        bool cancelOrder(const std::string& symbol, uint64_t orderId);

        // Cancel-and-replace the order in the named symbol's book.
        // Returns false if symbol not found or order not found.
        bool modifyOrder(const std::string& symbol, uint64_t orderId,
                         double newPrice, uint32_t newQuantity);

        // --- Read-only access to per-symbol state ---

        // Throws std::out_of_range if symbol has never had an order submitted.
        const OrderBook&    getOrderBook    (const std::string& symbol) const;
        const TradeHistory& getTradeHistory (const std::string& symbol) const;

        // Returns true if at least one order has been submitted for that symbol.
        bool hasSymbol(const std::string& symbol) const noexcept;

        std::vector<std::string> getSymbols() const;

        MatchingEngine& getMatchingEngine(const std::string& symbol);

    protected:
        // SymbolData bundles the three objects that must live together.
        // engine_ MUST be declared last so that book_ and history_ are fully
        // constructed before engine_'s constructor runs and takes references.
        struct SymbolData {
            explicit SymbolData(std::string symbol)
                : book(std::move(symbol)), history(), engine(book, history) {}

            // Non-copyable / non-movable (same reason as ExchangeCore).
            SymbolData(const SymbolData&) = delete;
            SymbolData& operator=(const SymbolData&) = delete;
            SymbolData(SymbolData&&) = delete;
            SymbolData& operator=(SymbolData&&) = delete;

            OrderBook      book;
            TradeHistory   history;
            MatchingEngine engine;  // references book and history above — must be last
        };

        // Returns the SymbolData for the given symbol, creating it on first call.
        // Protected so that the thread-safe subclass can override locking behaviour.
        SymbolData& getOrCreate(const std::string& symbol);

        std::unordered_map<std::string, SymbolData> symbols_;
    };

} // namespace miniexchange
