#include <gtest/gtest.h>
#include "miniexchange/OrderBook.h"
#include "miniexchange/Order.h"

using namespace miniexchange;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static Order makeBuy(uint64_t id, double price, uint32_t qty) {
    return Order::createLimitOrder(id, "TEST", Side::Buy, price, qty);
}

static Order makeSell(uint64_t id, double price, uint32_t qty) {
    return Order::createLimitOrder(id, "TEST", Side::Sell, price, qty);
}

// ---------------------------------------------------------------------------
// Empty book behaviour
// ---------------------------------------------------------------------------

TEST(OrderBook, EmptyBestBidReturnsNullopt) {
    OrderBook book("TEST");
    EXPECT_FALSE(book.getBestBid().has_value());
}

TEST(OrderBook, EmptyBestAskReturnsNullopt) {
    OrderBook book("TEST");
    EXPECT_FALSE(book.getBestAsk().has_value());
}

TEST(OrderBook, EmptyPeekBestBidReturnsNullopt) {
    OrderBook book("TEST");
    EXPECT_FALSE(book.peekBestBid().has_value());
}

TEST(OrderBook, EmptyPeekBestAskReturnsNullopt) {
    OrderBook book("TEST");
    EXPECT_FALSE(book.peekBestAsk().has_value());
}

TEST(OrderBook, CancelUnknownOrderReturnsFalse) {
    OrderBook book("TEST");
    EXPECT_FALSE(book.cancelOrder(99999));
}

TEST(OrderBook, CancelAndReturnUnknownOrderReturnsNullopt) {
    OrderBook book("TEST");
    EXPECT_FALSE(book.cancelAndReturnOrder(99999).has_value());
}

// ---------------------------------------------------------------------------
// Single order insertion
// ---------------------------------------------------------------------------

TEST(OrderBook, AddBidAndQueryBestBid) {
    OrderBook book("TEST");
    book.addOrder(makeBuy(1, 100.0, 10));
    ASSERT_TRUE(book.getBestBid().has_value());
    EXPECT_DOUBLE_EQ(book.getBestBid().value(), 100.0);
}

TEST(OrderBook, AddAskAndQueryBestAsk) {
    OrderBook book("TEST");
    book.addOrder(makeSell(1, 101.0, 10));
    ASSERT_TRUE(book.getBestAsk().has_value());
    EXPECT_DOUBLE_EQ(book.getBestAsk().value(), 101.0);
}

TEST(OrderBook, PeekBestBidReturnsCorrectFields) {
    OrderBook book("TEST");
    book.addOrder(makeBuy(42, 100.0, 15));
    auto top = book.peekBestBid();
    ASSERT_TRUE(top.has_value());
    EXPECT_EQ(top->orderId, 42u);
    EXPECT_DOUBLE_EQ(top->price, 100.0);
    EXPECT_EQ(top->remainingQuantity, 15u);
}

// ---------------------------------------------------------------------------
// Price-time priority
// ---------------------------------------------------------------------------

TEST(OrderBook, BidsSortedByPriceDescending) {
    OrderBook book("TEST");
    book.addOrder(makeBuy(1, 99.0, 10));
    book.addOrder(makeBuy(2, 101.0, 10));
    book.addOrder(makeBuy(3, 100.0, 10));
    EXPECT_DOUBLE_EQ(book.getBestBid().value(), 101.0);
}

TEST(OrderBook, AsksSortedByPriceAscending) {
    OrderBook book("TEST");
    book.addOrder(makeSell(1, 103.0, 10));
    book.addOrder(makeSell(2, 101.0, 10));
    book.addOrder(makeSell(3, 102.0, 10));
    EXPECT_DOUBLE_EQ(book.getBestAsk().value(), 101.0);
}

TEST(OrderBook, SamePriceBidsObeyTimeOrder) {
    // FIFO within a price level: first inserted should be at the front.
    OrderBook book("TEST");
    book.addOrder(makeBuy(10, 100.0, 5));
    book.addOrder(makeBuy(11, 100.0, 8));

    auto top = book.peekBestBid();
    ASSERT_TRUE(top.has_value());
    EXPECT_EQ(top->orderId, 10u);  // order 10 arrived first
}

// ---------------------------------------------------------------------------
// Cancel
// ---------------------------------------------------------------------------

TEST(OrderBook, CancelRemovesOrderAndReturnsTrue) {
    OrderBook book("TEST");
    book.addOrder(makeBuy(1, 100.0, 10));
    EXPECT_TRUE(book.cancelOrder(1));
    EXPECT_FALSE(book.getBestBid().has_value());
}

TEST(OrderBook, DoubleCancelReturnsFalse) {
    OrderBook book("TEST");
    book.addOrder(makeBuy(1, 100.0, 10));
    EXPECT_TRUE(book.cancelOrder(1));
    EXPECT_FALSE(book.cancelOrder(1));
}

TEST(OrderBook, CancelAndReturnPreservesOrderData) {
    OrderBook book("TEST");
    book.addOrder(makeBuy(7, 105.0, 20));
    auto result = book.cancelAndReturnOrder(7);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->getId(), 7u);
    EXPECT_DOUBLE_EQ(result->getPrice().value(), 105.0);
    EXPECT_EQ(result->getRemainingQuantity(), 20u);
}

TEST(OrderBook, CancelMiddleOrderDoesNotAffectNeighbours) {
    OrderBook book("TEST");
    book.addOrder(makeBuy(1, 100.0, 5));
    book.addOrder(makeBuy(2, 100.0, 5));
    book.addOrder(makeBuy(3, 100.0, 5));
    EXPECT_TRUE(book.cancelOrder(2));
    // Orders 1 and 3 must still be present (std::list iterator stability).
    auto top = book.peekBestBid();
    ASSERT_TRUE(top.has_value());
    EXPECT_EQ(top->orderId, 1u);
}

TEST(OrderBook, CancelAllOrdersAtLevelClearsLevel) {
    OrderBook book("TEST");
    book.addOrder(makeBuy(1, 100.0, 5));
    book.addOrder(makeBuy(2, 100.0, 5));
    EXPECT_TRUE(book.cancelOrder(1));
    EXPECT_TRUE(book.cancelOrder(2));
    EXPECT_FALSE(book.getBestBid().has_value());
}

// ---------------------------------------------------------------------------
// Fill
// ---------------------------------------------------------------------------

TEST(OrderBook, FillBestAskPartial) {
    OrderBook book("TEST");
    book.addOrder(makeSell(1, 101.0, 20));
    book.fillBestAsk(10);
    auto top = book.peekBestAsk();
    ASSERT_TRUE(top.has_value());
    EXPECT_EQ(top->remainingQuantity, 10u);
}

TEST(OrderBook, FillBestAskFullRemovesOrder) {
    OrderBook book("TEST");
    book.addOrder(makeSell(1, 101.0, 10));
    book.fillBestAsk(10);
    EXPECT_FALSE(book.getBestAsk().has_value());
}

TEST(OrderBook, FillBestBidFullRemovesOrder) {
    OrderBook book("TEST");
    book.addOrder(makeBuy(1, 99.0, 10));
    book.fillBestBid(10);
    EXPECT_FALSE(book.getBestBid().has_value());
}

// ---------------------------------------------------------------------------
// Market order cannot rest on the book
// ---------------------------------------------------------------------------

TEST(OrderBook, AddMarketOrderThrows) {
    OrderBook book("TEST");
    EXPECT_THROW(
        book.addOrder(Order::createMarketOrder(1, "TEST", Side::Buy, 10)),
        std::invalid_argument
    );
}

// ---------------------------------------------------------------------------
// getSymbol
// ---------------------------------------------------------------------------

TEST(OrderBook, GetSymbolReturnsCorrectValue) {
    OrderBook book("XYZW");
    EXPECT_EQ(book.getSymbol(), "XYZW");
}
