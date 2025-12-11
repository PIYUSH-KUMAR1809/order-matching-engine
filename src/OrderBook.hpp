#pragma once

#include <deque>
#include <functional>
#include <map>
#include <shared_mutex>
#include <unordered_map>
#include <vector>

#include "Order.hpp"

struct Trade {
  Price price;
  Quantity quantity;
  OrderId makerOrderId;
  OrderId takerOrderId;
};

class OrderBook {
 public:
  std::vector<Trade> addOrder(const Order &order);
  void cancelOrder(OrderId orderId);

  void lock() const { mutex_.lock(); }
  void unlock() const { mutex_.unlock(); }

  void printBook() const;

  const std::map<Price, std::deque<Order>, std::greater<Price>> &getBids()
      const {
    return bids;
  }
  const std::map<Price, std::deque<Order>, std::less<Price>> &getAsks() const {
    return asks;
  }

  std::map<Price, std::deque<Order>, std::greater<Price>> &getBidsInternal() {
    return bids;
  }
  std::map<Price, std::deque<Order>, std::less<Price>> &getAsksInternal() {
    return asks;
  }

  void addOrderInternal(const Order &order);
  void removeOrderInternal(OrderId orderId);
  void removeIndexInternal(OrderId orderId);

 private:
  struct OrderLocation {
    Price price;
    OrderSide side;
    Order *orderPtr;
  };

  std::map<Price, std::deque<Order>, std::greater<Price>> bids;
  std::map<Price, std::deque<Order>, std::less<Price>> asks;

  std::unordered_map<OrderId, OrderLocation> orderIndex;

  mutable std::shared_mutex mutex_;
};
