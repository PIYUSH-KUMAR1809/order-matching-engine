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
  std::vector<Trade> capturedTrades;
  std::mutex tradeMutex;
  std::condition_variable tradeCv;
  Exchange engine;

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

  std::vector<Trade> waitForTrades(
      size_t count,
      std::chrono::milliseconds timeout = std::chrono::milliseconds(200)) {
    engine.flush();
    std::unique_lock<std::mutex> lock(tradeMutex);
    tradeCv.wait_for(lock, timeout,
                     [&] { return capturedTrades.size() >= count; });
    return capturedTrades;
  }

  void waitForProcessing() {
    engine.flush();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }
};

namespace {
int countActiveOrdersAt(OrderBook* book, Price price, OrderSide side) {
  int32_t curr = book->getOrderHead(price, side);
  int count = 0;
  while (curr != -1) {
    if (book->getNode(curr).active) {
      count++;
    }
    curr = book->getNode(curr).next;
  }
  return count;
}

OrderNode* getFirstActive(OrderBook* book, Price price, OrderSide side) {
  int32_t curr = book->getOrderHead(price, side);
  while (curr != -1) {
    OrderNode& node = book->getNode(curr);
    if (node.active) return &node;
    curr = node.next;
  }
  return nullptr;
}
}  // namespace

TEST_F(ExchangeLogicTest, AddOrder) {
  int32_t symId = engine.registerSymbol("TEST", -1);
  engine.submitOrder(
      Order(1, 0, symId, OrderSide::Sell, OrderType::Limit, 10000, 10));

  engine.stop();

  OrderBook* book = const_cast<OrderBook*>(engine.getOrderBook(symId));
  ASSERT_NE(book, nullptr);

  ASSERT_EQ(countActiveOrdersAt(book, 10000, OrderSide::Sell), 1);

  auto* node = getFirstActive(book, 10000, OrderSide::Sell);
  ASSERT_NE(node, nullptr);
  ASSERT_EQ(node->order.quantity, 10);
}

TEST_F(ExchangeLogicTest, MatchFull) {
  int32_t symId = engine.registerSymbol("TEST", -1);
  engine.submitOrder(
      Order(1, 0, symId, OrderSide::Sell, OrderType::Limit, 10000, 10));
  engine.submitOrder(
      Order(2, 0, symId, OrderSide::Buy, OrderType::Limit, 10000, 10));

  auto trades = waitForTrades(1);
  ASSERT_EQ(trades.size(), 1);
  ASSERT_EQ(trades[0].quantity, 10);

  engine.stop();

  OrderBook* book = const_cast<OrderBook*>(engine.getOrderBook(symId));
  ASSERT_NE(book, nullptr);

  ASSERT_EQ(countActiveOrdersAt(book, 10000, OrderSide::Sell), 0);
  ASSERT_EQ(countActiveOrdersAt(book, 10000, OrderSide::Buy), 0);
}

TEST_F(ExchangeLogicTest, MatchPartial) {
  int32_t symId = engine.registerSymbol("TEST", -1);
  engine.submitOrder(
      Order(1, 0, symId, OrderSide::Sell, OrderType::Limit, 10000, 20));
  engine.submitOrder(
      Order(2, 0, symId, OrderSide::Buy, OrderType::Limit, 10000, 10));

  auto trades = waitForTrades(1);
  ASSERT_EQ(trades.size(), 1);
  ASSERT_EQ(trades[0].quantity, 10);

  engine.stop();

  OrderBook* book = const_cast<OrderBook*>(engine.getOrderBook(symId));
  ASSERT_NE(book, nullptr);

  ASSERT_EQ(countActiveOrdersAt(book, 10000, OrderSide::Sell), 1);
  auto* node = getFirstActive(book, 10000, OrderSide::Sell);
  ASSERT_NE(node, nullptr);
  ASSERT_EQ(node->order.quantity, 10);
  ASSERT_EQ(node->order.id, 1);
}

TEST_F(ExchangeLogicTest, NoMatch) {
  int32_t symId = engine.registerSymbol("TEST", -1);
  engine.submitOrder(
      Order(1, 0, symId, OrderSide::Sell, OrderType::Limit, 10100, 10));
  engine.submitOrder(
      Order(2, 0, symId, OrderSide::Buy, OrderType::Limit, 10000, 10));

  auto loopTrades = waitForTrades(1, std::chrono::milliseconds(50));
  ASSERT_TRUE(loopTrades.empty());

  engine.stop();

  OrderBook* book = const_cast<OrderBook*>(engine.getOrderBook(symId));
  ASSERT_NE(book, nullptr);

  ASSERT_EQ(countActiveOrdersAt(book, 10100, OrderSide::Sell), 1);
  ASSERT_EQ(countActiveOrdersAt(book, 10000, OrderSide::Buy), 1);
}

TEST_F(ExchangeLogicTest, CancelOrder) {
  int32_t symId = engine.registerSymbol("TEST", -1);
  engine.submitOrder(
      Order(1, 0, symId, OrderSide::Sell, OrderType::Limit, 10000, 10));

  waitForProcessing();

  engine.cancelOrder(symId, 1);

  engine.stop();

  OrderBook* book = const_cast<OrderBook*>(engine.getOrderBook(symId));
  ASSERT_NE(book, nullptr);

  ASSERT_EQ(countActiveOrdersAt(book, 10000, OrderSide::Sell), 0);
}

TEST_F(ExchangeLogicTest, MarketOrderFullFill) {
  int32_t symId = engine.registerSymbol("TEST", -1);
  engine.submitOrder(
      Order(1, 0, symId, OrderSide::Sell, OrderType::Limit, 10000, 10));

  waitForProcessing();

  engine.submitOrder(
      Order(2, 0, symId, OrderSide::Buy, OrderType::Market, 0, 10));

  auto trades = waitForTrades(1);

  ASSERT_EQ(trades.size(), 1);
  ASSERT_EQ(trades[0].quantity, 10);
  ASSERT_EQ(trades[0].price, 10000);
}

TEST(ExchangeTest, MultiAssetIsolation) {
  std::vector<Trade> captured;
  std::mutex mtx;
  std::condition_variable cv;
  Exchange engine;

  engine.setTradeCallback([&](const auto& trades) {
    std::lock_guard<std::mutex> lock(mtx);
    captured.insert(captured.end(), trades.begin(), trades.end());
    cv.notify_one();
  });

  int32_t aapl = engine.registerSymbol("AAPL", -1);
  int32_t goog = engine.registerSymbol("GOOG", -1);

  engine.submitOrder(
      Order(1, 0, aapl, OrderSide::Sell, OrderType::Limit, 15000, 100));

  engine.submitOrder(
      Order(2, 0, goog, OrderSide::Buy, OrderType::Limit, 15000, 100));

  engine.submitOrder(
      Order(3, 0, aapl, OrderSide::Buy, OrderType::Limit, 15000, 50));

  engine.flush();
  std::unique_lock<std::mutex> lock(mtx);
  cv.wait_for(lock, std::chrono::milliseconds(500),
              [&] { return !captured.empty(); });

  ASSERT_EQ(captured.size(), 1);
  ASSERT_EQ(captured[0].quantity, 50);
  ASSERT_EQ(captured[0].makerOrderId, 1);
  ASSERT_EQ(captured[0].takerOrderId, 3);
  ASSERT_EQ(captured[0].symbolId, aapl);
}

TEST(ExchangeTest, SmartSharding) {
  Exchange engine(2);

  int32_t symA = engine.registerSymbol("SYM_A", 0);
  int32_t symB = engine.registerSymbol("SYM_B", 1);

  engine.submitOrder(
      Order(1, 0, symA, OrderSide::Buy, OrderType::Limit, 100, 10));
  engine.submitOrder(
      Order(2, 0, symB, OrderSide::Buy, OrderType::Limit, 100, 10));

  engine.stop();

  const OrderBook* bookA = engine.getOrderBook(symA);
  const OrderBook* bookB = engine.getOrderBook(symB);

  ASSERT_NE(bookA, nullptr);
  ASSERT_NE(bookB, nullptr);
}
