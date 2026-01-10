#include "OrderBook.hpp"

#include <algorithm>
#include <cstddef>
#include <iostream>
#include <vector>

#include "Order.hpp"

OrderBook::OrderBook()
    : buffer(static_cast<size_t>(512 * 1024 * 1024)),
      pool(buffer.data(), buffer.size(), std::pmr::new_delete_resource()),
      bidMask(MAX_PRICE),
      askMask(MAX_PRICE) {
  idToLocation.resize(10000000);

  bids.reserve(MAX_PRICE);
  asks.reserve(MAX_PRICE);

  for (int i = 0; i < MAX_PRICE; ++i) {
    bids.emplace_back(&pool);
    asks.emplace_back(&pool);
  }
}

void OrderBook::addOrder(const Order& order) {
  if (order.price < 0 || order.price >= MAX_PRICE) return;

  if (order.id >= idToLocation.size()) {
    idToLocation.resize(order.id * 2);
  }

  bool isBid = (order.side == OrderSide::Buy);
  auto& levels = isBid ? bids : asks;
  auto& level = levels[order.price];

  idToLocation[order.id] = {order.price, (int32_t)level.orders.size()};

  level.orders.push_back(order);
  level.activeCount++;

  if (isBid) {
    bidMask.set(order.price);
    if (order.price > bestBid) bestBid = order.price;
  } else {
    askMask.set(order.price);
    if (bestAsk == -1 || order.price < bestAsk) bestAsk = order.price;
  }
}

void OrderBook::cancelOrder(OrderId orderId) {
  if (orderId >= idToLocation.size()) return;

  OrderLocation loc = idToLocation[orderId];
  if (loc.price == -1) return;

  bool found = false;
  if (loc.price < MAX_PRICE && loc.price >= 0) {
    if (loc.index < bids[loc.price].orders.size()) {
      if (bids[loc.price].orders[loc.index].id == orderId) {
        Order& o = bids[loc.price].orders[loc.index];
        if (o.active) {
          o.active = false;
          bids[loc.price].activeCount--;
          if (bids[loc.price].activeCount == 0) {
            bidMask.clear(loc.price);
            if (loc.price == bestBid) {
              size_t p = bidMask.findFirstSetDown(MAX_PRICE);
              bestBid = (p >= MAX_PRICE) ? 0 : (Price)p;
            }
          }
          found = true;
        }
      }
    }
    if (!found && loc.index < asks[loc.price].orders.size()) {
      if (asks[loc.price].orders[loc.index].id == orderId) {
        Order& o = asks[loc.price].orders[loc.index];
        if (o.active) {
          o.active = false;
          asks[loc.price].activeCount--;
          if (asks[loc.price].activeCount == 0) {
            askMask.clear(loc.price);
            if (loc.price == bestAsk) {
              size_t p = askMask.findFirstSet(0);
              bestAsk = (p >= MAX_PRICE) ? -1 : (Price)p;
            }
          }
          found = true;
        }
      }
    }
  }

  if (found) {
    idToLocation[orderId] = {-1, -1};
  }
}

void OrderBook::reset() {
  for (auto& level : bids) {
    level.orders.clear();
    level.activeCount = 0;
    level.headIndex = 0;
  }
  for (auto& level : asks) {
    level.orders.clear();
    level.activeCount = 0;
    level.headIndex = 0;
  }

  pool.release();

  bidMask.clearAll();
  askMask.clearAll();
  bestBid = 0;
  bestAsk = -1;
  std::fill(idToLocation.begin(), idToLocation.end(), OrderLocation{-1, -1});
}

void OrderBook::printBook() const {
  size_t count = 0;
  for (const auto& level : bids) count += level.activeCount;
  for (const auto& level : asks) count += level.activeCount;
  std::cout << "OrderBook Active Orders: " << count << "\n";
}
