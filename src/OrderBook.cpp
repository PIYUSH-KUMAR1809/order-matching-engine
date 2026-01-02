#include "OrderBook.hpp"

#include <iostream>

void OrderBook::addOrder(const Order &order) {
  if (orderPool.size() == orderPool.capacity()) {
    size_t newCap = std::max(INITIAL_POOL_SIZE, orderPool.capacity() * 2);
    orderPool.reserve(newCap);
  }

  orderPool.push_back({order, -1, true});
  int32_t newIdx = (int32_t)orderPool.size() - 1;

  if (order.id >= orderIndex.size()) {
    if (orderIndex.capacity() <= order.id) {
      orderIndex.reserve(
          std::max((size_t)order.id + 100000, orderIndex.capacity() * 2));
    }
    orderIndex.resize(order.id + 1, -1);
  }
  orderIndex[order.id] = newIdx;

  if (order.side == OrderSide::Buy) {
    if (order.price >= MAX_PRICE) return;

    int32_t tail = bidTails[order.price];
    if (tail != -1) {
      orderPool[tail].next = newIdx;
    } else {
      bidHeads[order.price] = newIdx;
    }
    bidTails[order.price] = newIdx;
    bidMask.set(order.price);

    if (bestBid == 0 || order.price > bestBid) {
      bestBid = order.price;
    }
  } else {
    if (order.price >= MAX_PRICE) return;

    int32_t tail = askTails[order.price];
    if (tail != -1) {
      orderPool[tail].next = newIdx;
    } else {
      askHeads[order.price] = newIdx;
    }
    askTails[order.price] = newIdx;
    askMask.set(order.price);

    if (order.price < bestAsk) {
      bestAsk = order.price;
    }
  }
}

void OrderBook::cancelOrder(OrderId orderId) {
  if (orderId >= orderIndex.size()) return;
  int32_t idx = orderIndex[orderId];
  if (idx != -1) {
    orderPool[idx].active = false;
  }
}

void OrderBook::printBook() const {
  std::cout << "--- Order Book ---" << std::endl;
  std::cout << "Best Bid: " << bestBid << std::endl;
  std::cout << "Best Ask: " << bestAsk << std::endl;
  std::cout << "Pool Size: " << orderPool.size() << std::endl;
}

void OrderBook::compact() {}
