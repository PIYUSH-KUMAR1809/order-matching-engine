#pragma once

#include <algorithm>
#include <vector>

#include "Bitset.hpp"
#include "Order.hpp"

class FlatOrderBook {
 public:
  static constexpr int MAX_PRICE = 100000;

  struct PriceLevel {
    std::vector<Order> orders;
    size_t activeCount = 0;
    size_t headIndex = 0;
  };

  FlatOrderBook() : bidMask(MAX_PRICE), askMask(MAX_PRICE) {}

  void addOrder(const Order& order) {
    if (order.price < 0 || order.price >= MAX_PRICE) return;

    if (order.side == OrderSide::Buy) {
      bids[order.price].orders.push_back(order);
      bids[order.price].activeCount++;
      bidMask.set(order.price);
      if (order.price > bestBid) bestBid = order.price;
    } else {
      asks[order.price].orders.push_back(order);
      asks[order.price].activeCount++;
      askMask.set(order.price);
      if (bestAsk == -1 || order.price < bestAsk) bestAsk = order.price;
    }
  }

  void cancelOrder(Price price, OrderSide side, OrderId orderId) {
    if (price < 0 || price >= MAX_PRICE) return;

    auto& level = (side == OrderSide::Buy) ? bids[price] : asks[price];
    for (auto& order : level.orders) {
      if (order.id == orderId) {
        order.active = false;
        level.activeCount--;
        return;
      }
    }
  }

  template <typename TradeCallback>
  void matchOrder(Order& incoming, TradeCallback&& onTrade) {
    if (incoming.side == OrderSide::Buy) {
      if (bestAsk == -1) return;

      Price p = bestAsk;
      while (p < MAX_PRICE) {
        if (!askMask.test(p)) {
          p = askMask.findFirstSet(p);
          if (p >= MAX_PRICE) break;
        }
        if (p > incoming.price && incoming.type == OrderType::Limit) break;

        auto& level = asks[p];
        if (level.activeCount > 0) {
          for (size_t i = level.headIndex; i < level.orders.size(); ++i) {
            auto& bookOrder = level.orders[i];
            if (!bookOrder.active) {
              if (i == level.headIndex) level.headIndex++;
              continue;
            }

            Quantity qty = std::min(incoming.quantity, bookOrder.quantity);

            onTrade(bookOrder, qty);

            bookOrder.quantity -= qty;
            incoming.quantity -= qty;

            if (bookOrder.quantity == 0) {
              bookOrder.active = false;
              level.activeCount--;
              if (i == level.headIndex) level.headIndex++;
            }
            if (incoming.quantity == 0) return;
          }
        }
        p++;
      }
    } else {
      if (bestBid == 0) return;

      Price p = bestBid;
      while (p > 0) {
        if (!bidMask.test(p)) {
          p = bidMask.findFirstSetDown(p);
          if (p == 0) {
            if (!bidMask.test(0)) break;
          }
          if (p >= MAX_PRICE) break;
        }

        if (p < incoming.price && incoming.type == OrderType::Limit) break;

        auto& level = bids[p];
        if (level.activeCount > 0) {
          for (size_t i = level.headIndex; i < level.orders.size(); ++i) {
            auto& bookOrder = level.orders[i];
            if (!bookOrder.active) {
              if (i == level.headIndex) level.headIndex++;
              continue;
            }

            Quantity qty = std::min(incoming.quantity, bookOrder.quantity);

            onTrade(bookOrder, qty);

            bookOrder.quantity -= qty;
            incoming.quantity -= qty;

            if (bookOrder.quantity == 0) {
              bookOrder.active = false;
              level.activeCount--;
              if (i == level.headIndex) level.headIndex++;
            }
            if (incoming.quantity == 0) return;
          }
        }

        if (p == 0) break;
        p--;
      }
    }
  }

  void compact(Price price, OrderSide side) {
    auto& level = (side == OrderSide::Buy) ? bids[price] : asks[price];
    auto it = std::remove_if(level.orders.begin(), level.orders.end(),
                             [](const Order& o) { return !o.active; });
    level.orders.erase(it, level.orders.end());
    level.headIndex = 0;
  }

 private:
  std::array<PriceLevel, MAX_PRICE> bids;
  std::array<PriceLevel, MAX_PRICE> asks;
  PriceBitset bidMask;
  PriceBitset askMask;

  Price bestBid = 0;
  Price bestAsk = -1;
};
