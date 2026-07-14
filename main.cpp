#include <iostream>
#include "miniexchange/OrderBook.h"
#include "miniexchange/MatchingEngine.h"
#include "miniexchange/TradeHistory.h"
#include "miniexchange/Order.h"

int main() {
    using namespace miniexchange;

    OrderBook book("AAPL");
    TradeHistory history;
    MatchingEngine engine(book, history);

    // Rest two sell orders on the book
    engine.submitOrder(Order::createLimitOrder(1, "AAPL", Side::Sell, 150.75, 30)); // Order E
    engine.submitOrder(Order::createLimitOrder(2, "AAPL", Side::Sell, 151.00, 50)); // Order C

    std::cout << "Best ask before match: " << book.getBestAsk().value_or(-1) << "\n"; // expect 150.75

    // Incoming buy crosses both levels: fully fills order 1, partially fills order 2
    engine.submitOrder(Order::createLimitOrder(3, "AAPL", Side::Buy, 151.00, 60));

    std::cout << "Best ask after match: " << book.getBestAsk().value_or(-1) << "\n"; // expect 151.00 (order 2 still has 20 left)

    // Market sell order should hit remaining resting liquidity if any exists on bid side
    engine.submitOrder(Order::createLimitOrder(4, "AAPL", Side::Buy, 149.00, 100)); // rests, doesn't cross
    engine.submitOrder(Order::createMarketOrder(5, "AAPL", Side::Sell, 40));         // should match against order 4

    std::cout << "\n--- Trade History ---\n";
    for (const auto& trade : history.getTrades()) {
        std::cout << "Trade " << trade.getTradeId()
                   << " | price: " << trade.getPrice()
                   << " | qty: " << trade.getQuantity()
                   << " | buyOrder: " << trade.getBuyOrderId()
                   << " | sellOrder: " << trade.getSellOrderId()
                   << "\n";
    }

    std::cout << "\n--- Cancel/Modify test ---\n";
    engine.submitOrder(Order::createLimitOrder(6, "AAPL", Side::Buy, 148.00, 20));
    std::cout << "Cancel order 6: " << std::boolalpha << engine.cancelOrder(6) << "\n"; // expect true
    std::cout << "Cancel order 6 again: " << engine.cancelOrder(6) << "\n"; // expect false

    engine.submitOrder(Order::createLimitOrder(7, "AAPL", Side::Buy, 148.00, 20));
    bool modified = engine.modifyOrder(7, 149.50, 25);
    std::cout << "Modify order 7: " << modified << "\n"; // expect true
    std::cout << "Best bid after modify: " << book.getBestBid().value_or(-1) << "\n"; // expect 149.5

    return 0;
}