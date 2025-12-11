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

  // Getters for testing
  const std::map<Price, std::deque<Order>, std::greater<Price>> &getBids()
      const {
    return bids;
  }
  const std::map<Price, std::deque<Order>, std::less<Price>> &getAsks() const {
    return asks;
  }

  // Internal access for MatchingStrategy
  // Internal access for MatchingStrategy
  std::map<Price, std::deque<Order>, std::greater<Price>> &getBidsInternal() {
    return bids;
  }
  std::map<Price, std::deque<Order>, std::less<Price>> &getAsksInternal() {
    return asks;
  }

  void addOrderInternal(const Order &order);
  void removeOrderInternal(OrderId orderId);
  void removeIndexInternal(OrderId orderId);  // Helper to clean index

 private:
  struct OrderLocation {
    Price price;
    OrderSide side;
    Order *orderPtr;  // Pointer stable in Deque (except erased)
  };

  // Bids: Highest price first
  std::map<Price, std::deque<Order>, std::greater<Price>> bids;
  // Asks: Lowest price first
  std::map<Price, std::deque<Order>, std::less<Price>> asks;

  // O(1) lookup for cancellation
  std::unordered_map<OrderId, OrderLocation> orderIndex;

  mutable std::shared_mutex mutex_;
};
