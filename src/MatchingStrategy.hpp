#pragma once

#include <vector>

#include "OrderBook.hpp"

class MatchingStrategy {
 public:
  virtual ~MatchingStrategy() = default;
  virtual std::vector<Trade> match(OrderBook& book, const Order& order) = 0;
};

class StandardMatchingStrategy : public MatchingStrategy {
 public:
  std::vector<Trade> match(OrderBook& book, const Order& order) override;
};
