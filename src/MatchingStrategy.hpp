#pragma once

#include <vector>

#include "OrderBook.hpp"

class MatchingStrategy {
 public:
  virtual ~MatchingStrategy() = default;
  virtual void match(OrderBook& book, const Order& order,
                     std::vector<Trade>& out_trades) = 0;
};

class StandardMatchingStrategy : public MatchingStrategy {
 public:
  void match(OrderBook& book, const Order& order,
             std::vector<Trade>& out_trades) override;
};
