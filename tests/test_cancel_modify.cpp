#include <gtest/gtest.h>
#include "miniexchange/MatchingEngine.h"
#include "miniexchange/OrderBook.h"
#include "miniexchange/TradeHistory.h"
#include "miniexchange/Order.h"

using namespace miniexchange;

// ---------------------------------------------------------------------------
// Test fixture
// ---------------------------------------------------------------------------

class CancelModifyTest : public ::testing::Test {
protected:
    void SetUp() override {
        book    = std::make_unique<OrderBook>("TEST");
        history = std::make_unique<TradeHistory>();
        engine  = std::make_unique<MatchingEngine>(*book, *history);
    }

    std::unique_ptr<OrderBook>      book;
    std::unique_ptr<TradeHistory>   history;
    std::unique_ptr<MatchingEngine> engine;

    void limitBuy(uint64_t id, double price, uint32_t qty) {
        engine->submitOrder(Order::createLimitOrder(id, "TEST", Side::Buy, price, qty));
    }
    void limitSell(uint64_t id, double price, uint32_t qty) {
        engine->submitOrder(Order::createLimitOrder(id, "TEST", Side::Sell, price, qty));
    }
};

// ---------------------------------------------------------------------------
// Cancel
// ---------------------------------------------------------------------------

TEST_F(CancelModifyTest, CancelKnownOrderReturnsTrue) {
    limitBuy(1, 100.0, 10);
    EXPECT_TRUE(engine->cancelOrder(1));
}

TEST_F(CancelModifyTest, CancelRemovesFromBook) {
    limitBuy(1, 100.0, 10);
    engine->cancelOrder(1);
    EXPECT_FALSE(book->getBestBid().has_value());
}

TEST_F(CancelModifyTest, CancelUnknownOrderReturnsFalse) {
    EXPECT_FALSE(engine->cancelOrder(9999));
}

TEST_F(CancelModifyTest, DoubleCancelReturnsFalseSecondTime) {
    limitBuy(1, 100.0, 10);
    EXPECT_TRUE(engine->cancelOrder(1));
    EXPECT_FALSE(engine->cancelOrder(1));
}

TEST_F(CancelModifyTest, CancelOneSideDoesNotAffectOther) {
    limitBuy(1, 100.0, 10);
    limitSell(2, 101.0, 10);
    engine->cancelOrder(1);
    EXPECT_FALSE(book->getBestBid().has_value());
    EXPECT_TRUE(book->getBestAsk().has_value());
}

// ---------------------------------------------------------------------------
// Modify (cancel-and-replace)
// ---------------------------------------------------------------------------

TEST_F(CancelModifyTest, ModifyNonExistentReturnsFalse) {
    EXPECT_FALSE(engine->modifyOrder(999, 105.0, 20));
}

TEST_F(CancelModifyTest, ModifyUpdatesPrice) {
    limitBuy(1, 100.0, 10);
    EXPECT_TRUE(engine->modifyOrder(1, 105.0, 10));
    EXPECT_DOUBLE_EQ(book->getBestBid().value(), 105.0);
}

TEST_F(CancelModifyTest, ModifyUpdatesQuantity) {
    limitBuy(1, 100.0, 10);
    EXPECT_TRUE(engine->modifyOrder(1, 100.0, 25));
    auto top = book->peekBestBid();
    ASSERT_TRUE(top.has_value());
    EXPECT_EQ(top->remainingQuantity, 25u);
}

TEST_F(CancelModifyTest, ModifyRetainsSameOrderId) {
    limitBuy(1, 100.0, 10);
    engine->modifyOrder(1, 105.0, 10);
    auto top = book->peekBestBid();
    ASSERT_TRUE(top.has_value());
    EXPECT_EQ(top->orderId, 1u);  // same ID per spec (cancel-and-replace)
}

TEST_F(CancelModifyTest, ModifyCanTriggerMatch) {
    // Put a resting sell at 101.
    limitSell(1, 101.0, 10);
    // Rest a buy at 99 — no match.
    limitBuy(2, 99.0, 10);
    // Modify buy to 102 — should now cross.
    engine->modifyOrder(2, 102.0, 10);

    EXPECT_EQ(history->getTrades().size(), 1u);
    EXPECT_FALSE(book->getBestBid().has_value());
    EXPECT_FALSE(book->getBestAsk().has_value());
}

TEST_F(CancelModifyTest, ModifyLosesTimePriority) {
    // Two buys at same price; order 1 first, then order 2.
    limitBuy(1, 100.0, 5);
    limitBuy(2, 100.0, 5);
    // Modify order 1 — it should move to the back of the queue.
    engine->modifyOrder(1, 100.0, 5);

    // Now sell into the book — should fill order 2 first (it now has time priority).
    limitSell(3, 100.0, 5);
    ASSERT_EQ(history->getTrades().size(), 1u);
    EXPECT_EQ(history->getTrades()[0].getBuyOrderId(), 2u);  // order 2 filled
}

// ---------------------------------------------------------------------------
// Order invariants
// ---------------------------------------------------------------------------

TEST_F(CancelModifyTest, OverfillThrowsInvalidArgument) {
    auto order = Order::createLimitOrder(1, "TEST", Side::Buy, 100.0, 10);
    EXPECT_THROW(order.fill(11), std::invalid_argument);
}

TEST_F(CancelModifyTest, ZeroQuantityOrderThrows) {
    EXPECT_THROW(
        Order::createLimitOrder(1, "TEST", Side::Buy, 100.0, 0),
        std::invalid_argument
    );
}

TEST_F(CancelModifyTest, ZeroQuantityMarketOrderThrows) {
    EXPECT_THROW(
        Order::createMarketOrder(1, "TEST", Side::Buy, 0),
        std::invalid_argument
    );
}

TEST_F(CancelModifyTest, NegativePriceLimitOrderThrows) {
    EXPECT_THROW(
        Order::createLimitOrder(1, "TEST", Side::Buy, -1.0, 10),
        std::invalid_argument
    );
}

TEST_F(CancelModifyTest, ZeroPriceLimitOrderThrows) {
    EXPECT_THROW(
        Order::createLimitOrder(1, "TEST", Side::Buy, 0.0, 10),
        std::invalid_argument
    );
}

// ---------------------------------------------------------------------------
// TradeHistory range query (Milestone 6)
// ---------------------------------------------------------------------------

TEST_F(CancelModifyTest, GetTradesBySymbolFiltersCorrectly) {
    // Use a second standalone history to test filtering.
    TradeHistory multiHistory;
    auto now = std::chrono::system_clock::now();
    multiHistory.recordTrade(Trade(1, "AAPL", 150.0, 10, 1, 2, now));
    multiHistory.recordTrade(Trade(2, "MSFT", 300.0, 5,  3, 4, now));
    multiHistory.recordTrade(Trade(3, "AAPL", 151.0, 8,  5, 6, now));

    auto aaplTrades = multiHistory.getTradesBySymbol("AAPL");
    EXPECT_EQ(aaplTrades.size(), 2u);

    auto msftTrades = multiHistory.getTradesBySymbol("MSFT");
    EXPECT_EQ(msftTrades.size(), 1u);

    auto tslaTrades = multiHistory.getTradesBySymbol("TSLA");
    EXPECT_EQ(tslaTrades.size(), 0u);
}

TEST_F(CancelModifyTest, GetTradesInRangeFiltersCorrectly) {
    using Clock = std::chrono::system_clock;
    TradeHistory th;
    auto t0 = Clock::now();
    auto t1 = t0 + std::chrono::seconds(1);
    auto t2 = t0 + std::chrono::seconds(2);
    auto t3 = t0 + std::chrono::seconds(3);

    th.recordTrade(Trade(1, "X", 10.0, 1, 1, 2, t0));
    th.recordTrade(Trade(2, "X", 10.0, 1, 3, 4, t1));
    th.recordTrade(Trade(3, "X", 10.0, 1, 5, 6, t2));
    th.recordTrade(Trade(4, "X", 10.0, 1, 7, 8, t3));

    auto result = th.getTradesInRange(t1, t2);
    EXPECT_EQ(result.size(), 2u);  // t1 and t2 inclusive
}
