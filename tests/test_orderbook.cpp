#include <gtest/gtest.h>

#include <thread>
#include <vector>

#include "Exchange.hpp"
#include "OrderBook.hpp"

class OrderBookTest : public ::testing::Test {
 protected:
  OrderBook book;
};

TEST_F(OrderBookTest, AddOrder) {
  book.addOrder(
      Order(1, 0, "TEST", OrderSide::Sell, OrderType::Limit, 100.0, 10));
  auto &asks = book.getAsks();
  ASSERT_EQ(asks.size(), 1);
  ASSERT_EQ(asks.begin()->second.front().quantity, 10);
}

TEST_F(OrderBookTest, MatchFull) {
  book.addOrder(
      Order(1, 0, "TEST", OrderSide::Sell, OrderType::Limit, 100.0, 10));
  book.addOrder(
      Order(2, 0, "TEST", OrderSide::Buy, OrderType::Limit, 100.0, 10));

  ASSERT_TRUE(book.getAsks().empty());
  ASSERT_TRUE(book.getBids().empty());
}

TEST_F(OrderBookTest, MatchPartial) {
  book.addOrder(
      Order(1, 0, "TEST", OrderSide::Sell, OrderType::Limit, 100.0, 20));
  book.addOrder(
      Order(2, 0, "TEST", OrderSide::Buy, OrderType::Limit, 100.0, 10));

  auto &asks = book.getAsks();
  ASSERT_EQ(asks.size(), 1);
  ASSERT_EQ(asks.begin()->second.front().quantity,
            10);  // 20 - 10 = 10 remaining
  ASSERT_TRUE(book.getBids().empty());
}

TEST_F(OrderBookTest, NoMatch) {
  book.addOrder(
      Order(1, 0, "TEST", OrderSide::Sell, OrderType::Limit, 101.0, 10));
  book.addOrder(
      Order(2, 0, "TEST", OrderSide::Buy, OrderType::Limit, 100.0, 10));

  ASSERT_EQ(book.getAsks().size(), 1);
  ASSERT_EQ(book.getBids().size(), 1);
}

TEST_F(OrderBookTest, CancelOrder) {
  book.addOrder(
      Order(1, 0, "TEST", OrderSide::Sell, OrderType::Limit, 100.0, 10));
  ASSERT_EQ(book.getAsks().size(), 1);

  book.cancelOrder(1);
  ASSERT_TRUE(book.getAsks().empty());
}

TEST_F(OrderBookTest, MarketOrderFullFill) {
  book.addOrder(
      Order(1, 0, "TEST", OrderSide::Sell, OrderType::Limit, 100.0, 10));

  auto trades = book.addOrder(
      Order(2, 0, "TEST", OrderSide::Buy, OrderType::Market, 0.0, 10));

  ASSERT_EQ(trades.size(), 1);
  ASSERT_EQ(trades[0].quantity, 10);
  ASSERT_EQ(trades[0].price, 100.0);

  ASSERT_TRUE(book.getAsks().empty());
  ASSERT_TRUE(book.getBids().empty());
}

TEST_F(OrderBookTest, MarketOrderPartialFill) {
  book.addOrder(
      Order(1, 0, "TEST", OrderSide::Sell, OrderType::Limit, 100.0, 10));

  // Buy 20. 10 should match, 10 should be cancelled (IOC).
  auto trades = book.addOrder(
      Order(2, 0, "TEST", OrderSide::Buy, OrderType::Market, 0.0, 20));

  ASSERT_EQ(trades.size(), 1);
  ASSERT_EQ(trades[0].quantity, 10);

  ASSERT_TRUE(book.getAsks().empty());
  ASSERT_TRUE(book.getBids().empty());  // Remainder cancelled
}

TEST_F(OrderBookTest, MarketOrderNoMatch) {
  // Empty book
  auto trades = book.addOrder(
      Order(1, 0, "TEST", OrderSide::Buy, OrderType::Market, 0.0, 10));

  ASSERT_TRUE(trades.empty());
  ASSERT_TRUE(book.getBids().empty());  // IOC cancelled
}

TEST(ExchangeTest, MultiAssetIsolation) {
  Exchange engine;

  // Add order for AAPL
  engine.submitOrder(
      Order(1, 0, "AAPL", OrderSide::Sell, OrderType::Limit, 150.0, 100));

  // Add order for GOOG (same price, different symbol)
  engine.submitOrder(
      Order(2, 0, "GOOG", OrderSide::Buy, OrderType::Limit, 150.0, 100));

  // Should NOT match because symbols are different
  // We can't easily inspect internal state without getters, but we can verify
  // by submitting a matching order for AAPL and ensuring it matches.

  auto trades = engine.submitOrder(
      Order(3, 0, "AAPL", OrderSide::Buy, OrderType::Limit, 150.0, 50));

  ASSERT_EQ(trades.size(), 1);
  ASSERT_EQ(trades[0].quantity, 50);
  ASSERT_EQ(trades[0].makerOrderId, 1);
  ASSERT_EQ(trades[0].takerOrderId, 3);
}

TEST(ExchangeTest, ConcurrencyTest) {
  Exchange engine;
  std::vector<std::thread> threads;

  auto tradeFunc = [&](std::string symbol, int startId) {
    for (int i = 0; i < 100; ++i) {
      engine.submitOrder(Order(startId + i * 2, 0, symbol, OrderSide::Sell,
                               OrderType::Limit, 100.0, 10));
      engine.submitOrder(Order(startId + i * 2 + 1, 0, symbol, OrderSide::Buy,
                               OrderType::Limit, 100.0, 10));
    }
  };

  // Launch 4 threads trading different symbols
  threads.emplace_back(tradeFunc, "SYM1", 1000);
  threads.emplace_back(tradeFunc, "SYM2", 2000);
  threads.emplace_back(tradeFunc, "SYM1", 3000);  // Contention on SYM1
  threads.emplace_back(tradeFunc, "SYM3", 4000);

  for (auto &t : threads) {
    t.join();
  }

  // Verification is hard without inspecting state, but we ensure no crashes
  // and basic consistency could be checked if we had getters.
  // For now, survival is the test.
}
