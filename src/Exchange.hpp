#pragma once

#include <memory>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "OrderBook.hpp"

class Exchange {
 public:
  std::vector<Trade> submitOrder(const Order &order);
  void cancelOrder(OrderId orderId);
  void printOrderBook(const std::string &symbol) const;
  void printAllOrderBooks() const;
  const OrderBook *getOrderBook(const std::string &symbol) const;

 private:
  std::unordered_map<std::string, std::unique_ptr<OrderBook>> orderBooks;
  std::unordered_map<OrderId, std::string> orderSymbolIndex;
  mutable std::shared_mutex exchangeMutex_;
};
