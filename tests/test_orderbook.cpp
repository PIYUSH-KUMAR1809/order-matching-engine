#include <gtest/gtest.h>

#include <thread>
#include <vector>

#include "Exchange.hpp"
#include "OrderBook.hpp"

class ExchangeLogicTest : public ::testing::Test {
 protected:
  Exchange engine;
};

TEST_F(ExchangeLogicTest, AddOrder) {
  engine.submitOrder(
      Order(1, 0, "TEST", OrderSide::Sell, OrderType::Limit, 100.0, 10));

  const OrderBook* book = engine.getOrderBook("TEST");
  ASSERT_NE(book, nullptr);
  auto& asks = book->getAsks();
  ASSERT_EQ(asks.size(), 1);
  ASSERT_EQ(asks.begin()->second.front().quantity, 10);
}

TEST_F(ExchangeLogicTest, MatchFull) {
  engine.submitOrder(
      Order(1, 0, "TEST", OrderSide::Sell, OrderType::Limit, 100.0, 10));
  engine.submitOrder(
      Order(2, 0, "TEST", OrderSide::Buy, OrderType::Limit, 100.0, 10));

  auto countActive = [](const auto& bookSide) {
    int count = 0;
    for (const auto& [price, orders] : bookSide) {
      for (const auto& order : orders) {
        if (order.active) count++;
      }
    }
    return count;
  };

  const OrderBook* book = engine.getOrderBook("TEST");
  ASSERT_EQ(countActive(book->getAsks()), 0);
  ASSERT_EQ(countActive(book->getBids()), 0);
}

TEST_F(ExchangeLogicTest, MatchPartial) {
  engine.submitOrder(
      Order(1, 0, "TEST", OrderSide::Sell, OrderType::Limit, 100.0, 20));
  engine.submitOrder(
      Order(2, 0, "TEST", OrderSide::Buy, OrderType::Limit, 100.0, 10));

  const OrderBook* book = engine.getOrderBook("TEST");
  auto& asks = book->getAsks();

  bool found = false;
  for (const auto& order : asks.at(100.0)) {
    if (order.id == 1 && order.active) {
      ASSERT_EQ(order.quantity, 10);
      found = true;
    }
  }
  ASSERT_TRUE(found);

  auto countActive = [](const auto& bookSide) {
    int count = 0;
    for (const auto& [price, orders] : bookSide) {
      for (const auto& order : orders) {
        if (order.active) count++;
      }
    }
    return count;
  };
  ASSERT_EQ(countActive(book->getBids()), 0);
}

TEST_F(ExchangeLogicTest, NoMatch) {
  engine.submitOrder(
      Order(1, 0, "TEST", OrderSide::Sell, OrderType::Limit, 101.0, 10));
  engine.submitOrder(
      Order(2, 0, "TEST", OrderSide::Buy, OrderType::Limit, 100.0, 10));

  const OrderBook* book = engine.getOrderBook("TEST");
  auto countActive = [](const auto& bookSide) {
    int count = 0;
    for (const auto& [price, orders] : bookSide) {
      for (const auto& order : orders) {
        if (order.active) count++;
      }
    }
    return count;
  };
  ASSERT_EQ(countActive(book->getAsks()), 1);
  ASSERT_EQ(countActive(book->getBids()), 1);
}

TEST_F(ExchangeLogicTest, CancelOrder) {
  engine.submitOrder(
      Order(1, 0, "TEST", OrderSide::Sell, OrderType::Limit, 100.0, 10));
  const OrderBook* book = engine.getOrderBook("TEST");

  engine.cancelOrder("TEST", 1);

  auto countActive = [](const auto& bookSide) {
    int count = 0;
    for (const auto& [price, orders] : bookSide) {
      for (const auto& order : orders) {
        if (order.active) count++;
      }
    }
    return count;
  };
  ASSERT_EQ(countActive(book->getAsks()), 0);
}

TEST_F(ExchangeLogicTest, MarketOrderFullFill) {
  engine.submitOrder(
      Order(1, 0, "TEST", OrderSide::Sell, OrderType::Limit, 100.0, 10));

  auto trades = engine.submitOrder(
      Order(2, 0, "TEST", OrderSide::Buy, OrderType::Market, 0.0, 10));

  ASSERT_EQ(trades.size(), 1);
  ASSERT_EQ(trades[0].quantity, 10);
  ASSERT_EQ(trades[0].price, 100.0);

  const OrderBook* book = engine.getOrderBook("TEST");
  auto countActive = [](const auto& bookSide) {
    int count = 0;
    for (const auto& [price, orders] : bookSide) {
      for (const auto& order : orders) {
        if (order.active) count++;
      }
    }
    return count;
  };
  ASSERT_EQ(countActive(book->getAsks()), 0);
  ASSERT_EQ(countActive(book->getBids()), 0);
}

TEST_F(ExchangeLogicTest, MarketOrderPartialFill) {
  engine.submitOrder(
      Order(1, 0, "TEST", OrderSide::Sell, OrderType::Limit, 100.0, 10));

  auto trades = engine.submitOrder(
      Order(2, 0, "TEST", OrderSide::Buy, OrderType::Market, 0.0, 20));

  ASSERT_EQ(trades.size(), 1);
  ASSERT_EQ(trades[0].quantity, 10);

  const OrderBook* book = engine.getOrderBook("TEST");
  auto countActive = [](const auto& bookSide) {
    int count = 0;
    for (const auto& [price, orders] : bookSide) {
      for (const auto& order : orders) {
        if (order.active) count++;
      }
    }
    return count;
  };
  ASSERT_EQ(countActive(book->getAsks()), 0);
  ASSERT_EQ(countActive(book->getBids()), 0);
}

TEST_F(ExchangeLogicTest, MarketOrderNoMatch) {
  auto trades = engine.submitOrder(
      Order(1, 0, "TEST", OrderSide::Buy, OrderType::Market, 0.0, 10));

  ASSERT_TRUE(trades.empty());

  const OrderBook* book = engine.getOrderBook("TEST");
  ASSERT_NE(book, nullptr);
  ASSERT_TRUE(book->getBids().empty());
}

TEST(ExchangeTest, MultiAssetIsolation) {
  Exchange engine;

  engine.submitOrder(
      Order(1, 0, "AAPL", OrderSide::Sell, OrderType::Limit, 150.0, 100));

  engine.submitOrder(
      Order(2, 0, "GOOG", OrderSide::Buy, OrderType::Limit, 150.0, 100));

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

  threads.emplace_back(tradeFunc, "SYM1", 1000);
  threads.emplace_back(tradeFunc, "SYM2", 2000);
  threads.emplace_back(tradeFunc, "SYM1", 3000);
  threads.emplace_back(tradeFunc, "SYM3", 4000);

  for (auto& t : threads) {
    t.join();
  }
}
