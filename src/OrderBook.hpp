#pragma once

#include <deque>
#include <map>
#include <vector>

#include "Order.hpp"

struct Trade {
  std::string symbol;
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

  // Internal function to add order to the order book
  void addOrderInternal(const Order &order);

  // Internal function to soft delete the order
  void removeOrderInternal(OrderId orderId);

  // Removing the entry from vector by setting pointer to null
  void removeIndexInternal(OrderId orderId);

  // Called during downtime to remove the soft deleted orders
  void compact();

 private:
  struct OrderLocation {
    Price price;
    OrderSide side;
    Order *orderPtr;
  };

  std::map<Price, std::deque<Order>, std::greater<Price>> bids;
  std::map<Price, std::deque<Order>, std::less<Price>> asks;

  std::vector<OrderLocation> orderIndex;
};
