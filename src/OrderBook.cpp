#include "OrderBook.hpp"

#include <iostream>

void OrderBook::addOrderInternal(const Order &order) {
  if (order.id >= orderIndex.size()) {
    orderIndex.resize(order.id + 1, {0, OrderSide::Buy, nullptr});
  }

  if (orderIndex[order.id].orderPtr != nullptr) {
    return;
  }

  if (order.side == OrderSide::Buy) {
    bids[order.price].push_back(order);
    Order *ptr = &bids[order.price].back();
    orderIndex[order.id] = {order.price, order.side, ptr};
  } else {
    asks[order.price].push_back(order);
    Order *ptr = &asks[order.price].back();
    orderIndex[order.id] = {order.price, order.side, ptr};
  }
}


// When user wants to cancel an order, we set the order to inactive.
// This is lazy deletion, we don't remove the order from the order book
// immediately. We just set the order to inactive.
void OrderBook::removeOrderInternal(OrderId orderId) {
  if (orderId >= orderIndex.size()) return;

  auto &location = orderIndex[orderId];
  if (location.orderPtr == nullptr) return;

  location.orderPtr->active = false;

  location = {0, OrderSide::Buy, nullptr};
}

// Called by matching engine after successfully matching an order.
// Since order is completely removed from the order book, we just remove the
// index entry.
void OrderBook::removeIndexInternal(OrderId orderId) {
  if (orderId >= orderIndex.size()) return;
  orderIndex[orderId] = {0, OrderSide::Buy, nullptr};
}

// Currently simply calling addOrderInternal, this function can later be used to
// add validations and logging
std::vector<Trade> OrderBook::addOrder(const Order &order) {
  addOrderInternal(order);
  return {};
}

// Currently simply calling removeOrderInternal, this function can later be used
// to add validations and logging
void OrderBook::cancelOrder(OrderId orderId) { removeOrderInternal(orderId); }

void OrderBook::printBook() const {
  std::cout << "--- Order Book ---" << std::endl;
  std::cout << "ASKS:" << std::endl;
  for (auto it = asks.rbegin(); it != asks.rend(); ++it) {
    for (const auto &order : it->second) {
      if (order.active) {
        std::cout << order.price << " x " << order.quantity
                  << " (ID: " << order.id << ")" << std::endl;
      }
    }
  }
  std::cout << "BIDS:" << std::endl;
  for (const auto &pair : bids) {
    for (const auto &order : pair.second) {
      if (order.active) {
        std::cout << pair.first << " x " << order.quantity
                  << " (ID: " << order.id << ")" << std::endl;
      }
    }
  }
  std::cout << "------------------" << std::endl;
}

void OrderBook::compact() {
  for (auto &pair : bids) {
    auto &queue = pair.second;
    std::vector<Order> validOrders;
    validOrders.reserve(queue.size());

    bool needsCompaction = false;
    for (const auto &order : queue) {
      if (order.active) {
        validOrders.push_back(order);
      } else {
        needsCompaction = true;
      }
    }

    if (needsCompaction) {
      queue.assign(validOrders.begin(), validOrders.end());
      for (auto &order : queue) {
        orderIndex[order.id].orderPtr = &order;
      }
    }
  }

  for (auto &pair : asks) {
    auto &queue = pair.second;
    std::vector<Order> validOrders;
    validOrders.reserve(queue.size());

    bool needsCompaction = false;
    for (const auto &order : queue) {
      if (order.active) {
        validOrders.push_back(order);
      } else {
        needsCompaction = true;
      }
    }

    if (needsCompaction) {
      queue.assign(validOrders.begin(), validOrders.end());
      for (auto &order : queue) {
        orderIndex[order.id].orderPtr = &order;
      }
    }
  }
}
