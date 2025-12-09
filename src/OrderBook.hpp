#pragma once

#include <functional>
#include <list>
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

  void printBook() const;

  // Getters for testing
  const std::map<Price, std::list<Order>, std::greater<Price>> &getBids()
      const {
    return bids;
  }
  const std::map<Price, std::list<Order>, std::less<Price>> &getAsks() const {
    return asks;
  }

 private:
  struct OrderLocation {
    Price price;
    OrderSide side;
    std::list<Order>::iterator iterator;
  };

  // Bids: Highest price first
  std::map<Price, std::list<Order>, std::greater<Price>> bids;
  // Asks: Lowest price first
  std::map<Price, std::list<Order>, std::less<Price>> asks;

  // O(1) lookup for cancellation
  std::unordered_map<OrderId, OrderLocation> orderIndex;

  // Internal matching logic
  std::vector<Trade> match();

  mutable std::shared_mutex mutex_;
};
