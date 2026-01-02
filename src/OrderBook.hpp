#pragma once

#include <cstdint>
#include <limits>
#include <vector>

#include "Order.hpp"

struct Trade {
  std::array<char, 8> symbol;
  Price price;
  Quantity quantity;
  OrderId makerOrderId;
  OrderId takerOrderId;
};

class PriceBitset {
  std::vector<uint64_t> data_;
  size_t size_;

 public:
  explicit PriceBitset(size_t size) : data_((size + 63) / 64, 0), size_(size) {}
  void set(size_t index) {
    if (index < size_) data_[index / 64] |= (1ULL << (index % 64));
  }
  void clear(size_t index) {
    if (index < size_) data_[index / 64] &= ~(1ULL << (index % 64));
  }
  size_t findFirstSet(size_t start) const {
    if (start >= size_) return size_;
    size_t idx = start / 64;
    size_t bit = start % 64;
    uint64_t word = data_[idx] & (~0ULL << bit);
    if (word != 0) return idx * 64 + __builtin_ctzll(word);
    for (idx++; idx < data_.size(); idx++)
      if (data_[idx] != 0) return idx * 64 + __builtin_ctzll(data_[idx]);
    return size_;
  }
  size_t findFirstSetDown(size_t start) const {
    if (start >= size_) start = size_ - 1;
    size_t idx = start / 64;
    size_t bit = start % 64;
    uint64_t mask = (bit == 63) ? ~0ULL : ((1ULL << (bit + 1)) - 1);
    uint64_t word = data_[idx] & mask;
    if (word != 0) return idx * 64 + (63 - __builtin_clzll(word));
    for (size_t i = idx; i-- > 0;)
      if (data_[i] != 0) return i * 64 + (63 - __builtin_clzll(data_[i]));
    return size_;
  }
};

struct OrderNode {
  Order order;
  int32_t next = -1;
  bool active = true;
};

class OrderBook {
 public:
  static constexpr size_t MAX_PRICE = 200000;
  static constexpr size_t INITIAL_POOL_SIZE = 15000000;

  OrderBook()
      : bidHeads(MAX_PRICE, -1),
        askHeads(MAX_PRICE, -1),
        bidTails(MAX_PRICE, -1),
        askTails(MAX_PRICE, -1),
        bidMask(MAX_PRICE),
        askMask(MAX_PRICE) {
    orderPool.reserve(INITIAL_POOL_SIZE);
    orderIndex.reserve(INITIAL_POOL_SIZE);
  }

  void addOrder(const Order& order);
  void cancelOrder(OrderId orderId);

  int32_t getOrderHead(Price p, OrderSide side) const {
    if (side == OrderSide::Buy) return bidHeads[p];
    return askHeads[p];
  }

  void setOrderHead(Price p, OrderSide side, int32_t newHead) {
    if (side == OrderSide::Buy) {
      bidHeads[p] = newHead;
      if (newHead == -1) {
        bidTails[p] = -1;
        bidMask.clear(p);
      }
    } else {
      askHeads[p] = newHead;
      if (newHead == -1) {
        askTails[p] = -1;
        askMask.clear(p);
      }
    }
  }

  OrderNode& getNode(int32_t idx) { return orderPool[idx]; }
  const OrderNode& getNode(int32_t idx) const { return orderPool[idx]; }

  Price getBestBid() const { return bestBid; }
  Price getBestAsk() const { return bestAsk; }

  Price getNextBid(Price start) const {
    size_t p = bidMask.findFirstSetDown(start);
    return (p == MAX_PRICE) ? 0 : static_cast<Price>(p);
  }

  Price getNextAsk(Price start) const {
    size_t p = askMask.findFirstSet(start);
    return (p == MAX_PRICE) ? MAX_PRICE : static_cast<Price>(p);
  }

  void resetLevel(Price p, OrderSide side) {
    if (side == OrderSide::Buy) {
      bidHeads[p] = -1;
      bidTails[p] = -1;
      bidMask.clear(p);
    } else {
      askHeads[p] = -1;
      askTails[p] = -1;
      askMask.clear(p);
    }
  }

  void printBook() const;
  void compact();

  void updateBestBid() {
    size_t p = bidMask.findFirstSetDown(bestBid);
    bestBid = (p == MAX_PRICE) ? 0 : static_cast<Price>(p);
  }
  void updateBestAsk() {
    size_t p = askMask.findFirstSet(bestAsk);
    bestAsk = (p == MAX_PRICE) ? MAX_PRICE : static_cast<Price>(p);
  }

 private:
  std::vector<int32_t> bidHeads;
  std::vector<int32_t> askHeads;
  std::vector<int32_t> bidTails;
  std::vector<int32_t> askTails;

  PriceBitset bidMask;
  PriceBitset askMask;

  Price bestBid = 0;
  Price bestAsk = std::numeric_limits<Price>::max();

  std::vector<OrderNode> orderPool;
  std::vector<int32_t> orderIndex;
};
