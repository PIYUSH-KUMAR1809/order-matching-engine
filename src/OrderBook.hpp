#pragma once

#include <cstdint>
#include <memory_resource>
#include <vector>

#include "Bitset.hpp"
#include "Order.hpp"

struct Trade {
  int32_t symbolId;
  Price price;
  Quantity quantity;
  OrderId makerOrderId;
  OrderId takerOrderId;

  Trade(OrderId maker, OrderId taker, int32_t sym, Price p, Quantity q)
      : symbolId(sym),
        price(p),
        quantity(q),
        makerOrderId(maker),
        takerOrderId(taker) {}
};

struct PriceLevel {
  std::pmr::vector<Order> orders;
  int32_t activeCount = 0;
  int32_t headIndex = 0;

  explicit PriceLevel(
      std::pmr::memory_resource* mr = std::pmr::get_default_resource())
      : orders(mr) {}

  PriceLevel(PriceLevel&&) = default;
  PriceLevel& operator=(PriceLevel&&) = default;
  PriceLevel(const PriceLevel&) = delete;
  PriceLevel& operator=(const PriceLevel&) = delete;
};

class OrderBook {
 public:
  static constexpr int MAX_PRICE = 100000;

  struct OrderLocation {
    Price price = -1;
    int32_t index = -1;
  };

  OrderBook();

  void addOrder(const Order& order);
  void cancelOrder(OrderId orderId);

  std::vector<PriceLevel>& getBids() { return bids; }
  std::vector<PriceLevel>& getAsks() { return asks; }
  const std::vector<PriceLevel>& getBids() const { return bids; }
  const std::vector<PriceLevel>& getAsks() const { return asks; }

  PriceBitset& getBidMask() { return bidMask; }
  PriceBitset& getAskMask() { return askMask; }
  const PriceBitset& getBidMask() const { return bidMask; }
  const PriceBitset& getAskMask() const { return askMask; }

  const PriceLevel& getLevel(Price price, OrderSide side) const {
    return (side == OrderSide::Buy) ? bids[price] : asks[price];
  }
  PriceLevel& getLevelMutable(Price price, OrderSide side) {
    return (side == OrderSide::Buy) ? bids[price] : asks[price];
  }

  Price getBestBid() const { return bestBid; }
  Price getBestAsk() const { return bestAsk; }

  void reset();
  void printBook() const;

 private:
  std::vector<PriceLevel> bids;
  std::vector<PriceLevel> asks;

  PriceBitset bidMask;
  PriceBitset askMask;

  Price bestBid = 0;
  Price bestAsk = -1;

  std::vector<OrderLocation> idToLocation;

  std::vector<std::byte> buffer;
  std::pmr::monotonic_buffer_resource pool;

  friend class StandardMatchingStrategy;
};
