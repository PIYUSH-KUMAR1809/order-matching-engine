#pragma once

#include <vector>

#include "OrderBook.hpp"

class MatchingStrategy {
 public:
  virtual ~MatchingStrategy() = default;
  MatchingStrategy() = default;
  MatchingStrategy(const MatchingStrategy&) = default;
  MatchingStrategy& operator=(const MatchingStrategy&) = default;
  MatchingStrategy(MatchingStrategy&&) = default;
  MatchingStrategy& operator=(MatchingStrategy&&) = default;
  virtual void match(OrderBook& book, const Order& order,
                     std::vector<Trade>& out_trades) = 0;
};

class StandardMatchingStrategy : public MatchingStrategy {
 public:
  void match(OrderBook& book, const Order& order,
             std::vector<Trade>& out_trades) override;
};
