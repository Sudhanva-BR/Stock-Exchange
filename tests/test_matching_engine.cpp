#include <gtest/gtest.h>
#include "miniexchange/MatchingEngine.h"
#include "miniexchange/OrderBook.h"
#include "miniexchange/TradeHistory.h"
#include "miniexchange/Order.h"
#include "miniexchange/Trade.h"

using namespace miniexchange;

// ---------------------------------------------------------------------------
// Test fixture — fresh book/history/engine per test case
// ---------------------------------------------------------------------------

class MatchingEngineTest : public ::testing::Test {
protected:
    void SetUp() override {
        book    = std::make_unique<OrderBook>("TEST");
        history = std::make_unique<TradeHistory>();
        engine  = std::make_unique<MatchingEngine>(*book, *history);
    }

    std::unique_ptr<OrderBook>      book;
    std::unique_ptr<TradeHistory>   history;
    std::unique_ptr<MatchingEngine> engine;

    // Helpers
    void sell(uint64_t id, double price, uint32_t qty) {
        engine->submitOrder(Order::createLimitOrder(id, "TEST", Side::Sell, price, qty));
    }
    void buy(uint64_t id, double price, uint32_t qty) {
        engine->submitOrder(Order::createLimitOrder(id, "TEST", Side::Buy, price, qty));
    }
    void buyMarket(uint64_t id, uint32_t qty) {
        engine->submitOrder(Order::createMarketOrder(id, "TEST", Side::Buy, qty));
    }
    void sellMarket(uint64_t id, uint32_t qty) {
        engine->submitOrder(Order::createMarketOrder(id, "TEST", Side::Sell, qty));
    }
    const std::vector<Trade>& trades() { return history->getTrades(); }
};

// ---------------------------------------------------------------------------
// No match: incoming order rests on the book
// ---------------------------------------------------------------------------

TEST_F(MatchingEngineTest, BuyBelowAskRests) {
    sell(1, 101.0, 10);
    buy(2, 99.0, 10);  // doesn't cross
    EXPECT_EQ(trades().size(), 0u);
    EXPECT_DOUBLE_EQ(book->getBestBid().value(), 99.0);
    EXPECT_DOUBLE_EQ(book->getBestAsk().value(), 101.0);
}

TEST_F(MatchingEngineTest, SellAboveBidRests) {
    buy(1, 99.0, 10);
    sell(2, 101.0, 10);  // doesn't cross
    EXPECT_EQ(trades().size(), 0u);
}

// ---------------------------------------------------------------------------
// Exact match: full fill on both sides
// ---------------------------------------------------------------------------

TEST_F(MatchingEngineTest, ExactFullFill) {
    sell(1, 100.0, 10);
    buy(2, 100.0, 10);  // crosses exactly

    ASSERT_EQ(trades().size(), 1u);
    EXPECT_DOUBLE_EQ(trades()[0].getPrice(), 100.0);
    EXPECT_EQ(trades()[0].getQuantity(), 10u);
    EXPECT_EQ(trades()[0].getSellOrderId(), 1u);
    EXPECT_EQ(trades()[0].getBuyOrderId(), 2u);

    // Both sides should now be empty.
    EXPECT_FALSE(book->getBestBid().has_value());
    EXPECT_FALSE(book->getBestAsk().has_value());
}

// ---------------------------------------------------------------------------
// Partial fill: incoming is smaller than resting
// ---------------------------------------------------------------------------

TEST_F(MatchingEngineTest, IncomingBuyPartiallyFillsResting) {
    sell(1, 100.0, 20);
    buy(2, 100.0, 8);  // consumes 8 of 20

    ASSERT_EQ(trades().size(), 1u);
    EXPECT_EQ(trades()[0].getQuantity(), 8u);

    // Resting sell should still have 12 left.
    auto top = book->peekBestAsk();
    ASSERT_TRUE(top.has_value());
    EXPECT_EQ(top->remainingQuantity, 12u);
}

TEST_F(MatchingEngineTest, IncomingBuyLargerThanResting) {
    sell(1, 100.0, 5);
    buy(2, 100.0, 15);  // fills all 5, rests 10 as bid

    ASSERT_EQ(trades().size(), 1u);
    EXPECT_EQ(trades()[0].getQuantity(), 5u);

    EXPECT_FALSE(book->getBestAsk().has_value());
    auto top = book->peekBestBid();
    ASSERT_TRUE(top.has_value());
    EXPECT_EQ(top->remainingQuantity, 10u);
}

// ---------------------------------------------------------------------------
// Multi-level sweep
// ---------------------------------------------------------------------------

TEST_F(MatchingEngineTest, BuySweepesTwoPriceLevels) {
    sell(1, 100.0, 10);
    sell(2, 101.0, 20);
    buy(3, 102.0, 25);  // takes all 10@100, then 15@101

    ASSERT_EQ(trades().size(), 2u);
    EXPECT_DOUBLE_EQ(trades()[0].getPrice(), 100.0);  // resting price
    EXPECT_EQ(trades()[0].getQuantity(), 10u);
    EXPECT_DOUBLE_EQ(trades()[1].getPrice(), 101.0);
    EXPECT_EQ(trades()[1].getQuantity(), 15u);

    // 5 left on 101 level.
    auto top = book->peekBestAsk();
    ASSERT_TRUE(top.has_value());
    EXPECT_DOUBLE_EQ(top->price, 101.0);
    EXPECT_EQ(top->remainingQuantity, 5u);
}

// ---------------------------------------------------------------------------
// Trade price equals the RESTING order's price
// ---------------------------------------------------------------------------

TEST_F(MatchingEngineTest, TradePriceIsRestingPrice) {
    sell(1, 100.0, 10);
    buy(2, 105.0, 10);  // incoming at 105, resting at 100

    ASSERT_EQ(trades().size(), 1u);
    EXPECT_DOUBLE_EQ(trades()[0].getPrice(), 100.0);  // NOT 105
}

// ---------------------------------------------------------------------------
// Market orders
// ---------------------------------------------------------------------------

TEST_F(MatchingEngineTest, MarketBuyAgainstEmptyBookNoTrade) {
    buyMarket(1, 10);
    EXPECT_EQ(trades().size(), 0u);
    EXPECT_FALSE(book->getBestBid().has_value());  // market orders don't rest
}

TEST_F(MatchingEngineTest, MarketBuyMatchesRestingAsk) {
    sell(1, 99.0, 15);
    buyMarket(2, 10);

    ASSERT_EQ(trades().size(), 1u);
    EXPECT_EQ(trades()[0].getQuantity(), 10u);
    EXPECT_DOUBLE_EQ(trades()[0].getPrice(), 99.0);
}

TEST_F(MatchingEngineTest, MarketSellMatchesRestingBid) {
    buy(1, 98.0, 20);
    sellMarket(2, 8);

    ASSERT_EQ(trades().size(), 1u);
    EXPECT_EQ(trades()[0].getQuantity(), 8u);
    EXPECT_DOUBLE_EQ(trades()[0].getPrice(), 98.0);
}

TEST_F(MatchingEngineTest, MarketOrderDoesNotRestAfterPartialFill) {
    sell(1, 100.0, 5);
    buyMarket(2, 20);  // 5 filled, 15 cannot match — market orders don't rest

    ASSERT_EQ(trades().size(), 1u);
    EXPECT_EQ(trades()[0].getQuantity(), 5u);
    EXPECT_FALSE(book->getBestBid().has_value());  // leftover NOT rested
}

// ---------------------------------------------------------------------------
// Price-time priority within a level
// ---------------------------------------------------------------------------

TEST_F(MatchingEngineTest, TimeOrderWithinPriceLevel) {
    sell(10, 100.0, 5);   // arrives first
    sell(11, 100.0, 5);   // arrives second
    buy(12, 100.0, 5);    // should match against order 10 (time priority)

    ASSERT_EQ(trades().size(), 1u);
    EXPECT_EQ(trades()[0].getSellOrderId(), 10u);
}

// ---------------------------------------------------------------------------
// Trade ID increments
// ---------------------------------------------------------------------------

TEST_F(MatchingEngineTest, TradeIdsAreUnique) {
    sell(1, 100.0, 5);
    sell(2, 100.0, 5);
    buy(3, 100.0, 10);  // produces 2 trades

    ASSERT_EQ(trades().size(), 2u);
    EXPECT_NE(trades()[0].getTradeId(), trades()[1].getTradeId());
}

// ---------------------------------------------------------------------------
// TradeHistory query methods (Milestone 6)
// ---------------------------------------------------------------------------

TEST_F(MatchingEngineTest, VolumeStatsEmptyHistory) {
    const auto stats = history->getVolumeStats();
    EXPECT_EQ(stats.totalVolume, 0u);
    EXPECT_DOUBLE_EQ(stats.vwap, 0.0);
}

TEST_F(MatchingEngineTest, VolumeStatsAfterTrades) {
    sell(1, 100.0, 10);
    buy(2, 100.0, 10);  // trade: 10@100

    sell(3, 110.0, 10);
    buy(4, 110.0, 10);  // trade: 10@110

    const auto stats = history->getVolumeStats();
    EXPECT_EQ(stats.totalVolume, 20u);
    // VWAP = (10*100 + 10*110) / 20 = 105
    EXPECT_DOUBLE_EQ(stats.vwap, 105.0);
}
