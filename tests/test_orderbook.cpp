#include <gtest/gtest.h>

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>

#include "Exchange.hpp"
#include "OrderBook.hpp"

class ExchangeLogicTest : public ::testing::Test {
 protected:
  Exchange engine;
  std::vector<Trade> capturedTrades;
  std::mutex tradeMutex;
  std::condition_variable tradeCv;

  void SetUp() override {
    engine.setTradeCallback([this](const std::vector<Trade>& trades) {
      std::lock_guard<std::mutex> lock(tradeMutex);
      capturedTrades.insert(capturedTrades.end(), trades.begin(), trades.end());
      tradeCv.notify_all();
    });
  }

  void clearTrades() {
    std::lock_guard<std::mutex> lock(tradeMutex);
    capturedTrades.clear();
  }

  std::vector<Trade> waitForTrades(size_t count, int timeoutMs = 200) {
    std::unique_lock<std::mutex> lock(tradeMutex);
    tradeCv.wait_for(lock, std::chrono::milliseconds(timeoutMs),
                     [&] { return capturedTrades.size() >= count; });
    return capturedTrades;
  }

  void waitForProcessing() {
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }
};

TEST_F(ExchangeLogicTest, AddOrder) {
  engine.submitOrder(
      Order(1, 0, "TEST", OrderSide::Sell, OrderType::Limit, 10000, 10));

  waitForProcessing();

  const OrderBook* book = engine.getOrderBook("TEST");
  ASSERT_NE(book, nullptr);
  auto& asks = book->getAsks();
  ASSERT_EQ(asks.size(), 1);
  ASSERT_EQ(asks.begin()->second.front().quantity, 10);
}

TEST_F(ExchangeLogicTest, MatchFull) {
  engine.submitOrder(
      Order(1, 0, "TEST", OrderSide::Sell, OrderType::Limit, 10000, 10));
  engine.submitOrder(
      Order(2, 0, "TEST", OrderSide::Buy, OrderType::Limit, 10000, 10));

  auto trades = waitForTrades(1);
  ASSERT_EQ(trades.size(), 1);
  ASSERT_EQ(trades[0].quantity, 10);

  const OrderBook* book = engine.getOrderBook("TEST");
  ASSERT_NE(book, nullptr);
  ASSERT_TRUE(book->getAsks().empty());
  ASSERT_TRUE(book->getBids().empty());
}

TEST_F(ExchangeLogicTest, MatchPartial) {
  engine.submitOrder(
      Order(1, 0, "TEST", OrderSide::Sell, OrderType::Limit, 10000, 20));
  engine.submitOrder(
      Order(2, 0, "TEST", OrderSide::Buy, OrderType::Limit, 10000, 10));

  auto trades = waitForTrades(1);
  ASSERT_EQ(trades.size(), 1);
  ASSERT_EQ(trades[0].quantity, 10);

  const OrderBook* book = engine.getOrderBook("TEST");
  ASSERT_NE(book, nullptr);
  auto& asks = book->getAsks();
  bool found = false;
  if (!asks.empty()) {
    for (const auto& order : asks.at(10000)) {
      if (order.id == 1 && order.active) {
        ASSERT_EQ(order.quantity, 10);
        found = true;
      }
    }
  }
  ASSERT_TRUE(found);
}

TEST_F(ExchangeLogicTest, NoMatch) {
  engine.submitOrder(
      Order(1, 0, "TEST", OrderSide::Sell, OrderType::Limit, 10100, 10));
  engine.submitOrder(
      Order(2, 0, "TEST", OrderSide::Buy, OrderType::Limit, 10000, 10));

  waitForProcessing();

  auto loopTrades = waitForTrades(1, 50);
  ASSERT_TRUE(loopTrades.empty());

  const OrderBook* book = engine.getOrderBook("TEST");
  ASSERT_NE(book, nullptr);
  ASSERT_FALSE(book->getAsks().empty());
  ASSERT_FALSE(book->getBids().empty());
}

TEST_F(ExchangeLogicTest, CancelOrder) {
  engine.submitOrder(
      Order(1, 0, "TEST", OrderSide::Sell, OrderType::Limit, 10000, 10));

  waitForProcessing();

  engine.cancelOrder("TEST", 1);

  waitForProcessing();

  const OrderBook* book = engine.getOrderBook("TEST");
  ASSERT_NE(book, nullptr);

  int activeCount = 0;
  for (const auto& pair : book->getAsks()) {
    for (const auto& order : pair.second) {
      if (order.active) activeCount++;
    }
  }
  ASSERT_EQ(activeCount, 0);
}

TEST_F(ExchangeLogicTest, MarketOrderFullFill) {
  engine.submitOrder(
      Order(1, 0, "TEST", OrderSide::Sell, OrderType::Limit, 10000, 10));

  waitForProcessing();

  engine.submitOrder(
      Order(2, 0, "TEST", OrderSide::Buy, OrderType::Market, 0, 10));

  auto trades = waitForTrades(1);

  ASSERT_EQ(trades.size(), 1);
  ASSERT_EQ(trades[0].quantity, 10);
  ASSERT_EQ(trades[0].price, 10000);
}

TEST(ExchangeTest, MultiAssetIsolation) {
  Exchange engine;
  std::vector<Trade> captured;
  std::mutex mtx;
  std::condition_variable cv;

  engine.setTradeCallback([&](const auto& trades) {
    std::lock_guard<std::mutex> lock(mtx);
    captured.insert(captured.end(), trades.begin(), trades.end());
    cv.notify_one();
  });

  engine.submitOrder(
      Order(1, 0, "AAPL", OrderSide::Sell, OrderType::Limit, 15000, 100));

  engine.submitOrder(
      Order(2, 0, "GOOG", OrderSide::Buy, OrderType::Limit, 15000, 100));

  engine.submitOrder(
      Order(3, 0, "AAPL", OrderSide::Buy, OrderType::Limit, 15000, 50));

  std::unique_lock<std::mutex> lock(mtx);
  cv.wait_for(lock, std::chrono::milliseconds(500),
              [&] { return !captured.empty(); });

  ASSERT_EQ(captured.size(), 1);
  ASSERT_EQ(captured[0].quantity, 50);
  ASSERT_EQ(captured[0].makerOrderId, 1);
  ASSERT_EQ(captured[0].takerOrderId, 3);
  ASSERT_EQ(captured[0].symbol, "AAPL");
}
