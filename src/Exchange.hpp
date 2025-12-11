#pragma once

#include <memory>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "MatchingStrategy.hpp"

class Exchange {
 public:
  Exchange();
  ~Exchange();
  std::vector<Trade> submitOrder(const Order &order);
  void cancelOrder(const std::string &symbol, OrderId orderId);
  void printOrderBook(const std::string &symbol) const;
  void printAllOrderBooks() const;
  const OrderBook *getOrderBook(const std::string &symbol) const;

 private:
  std::unordered_map<std::string, std::unique_ptr<OrderBook>> orderBooks;
  std::unique_ptr<MatchingStrategy> matchingStrategy;
  mutable std::shared_mutex exchangeMutex_;
};
