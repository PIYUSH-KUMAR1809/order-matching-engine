#include "OrderBook.hpp"

#include <iostream>

void OrderBook::addOrderInternal(const Order &order) {
  if (order.id >= orderIndex.size()) {
    orderIndex.resize(order.id + 1, {0, OrderSide::Buy, nullptr});
  }

  if (orderIndex[order.id].orderPtr != nullptr) {
    return;  // Already exists
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

void OrderBook::removeOrderInternal(OrderId orderId) {
  if (orderId >= orderIndex.size()) return;

  auto &location = orderIndex[orderId];
  if (location.orderPtr == nullptr) return;

  location.orderPtr->active = false;

  // Clear the location
  location = {0, OrderSide::Buy, nullptr};
}

void OrderBook::removeIndexInternal(OrderId orderId) {
  if (orderId >= orderIndex.size()) return;
  orderIndex[orderId] = {0, OrderSide::Buy, nullptr};
}

std::vector<Trade> OrderBook::addOrder(const Order &order) {
  addOrderInternal(order);
  return {};
}

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
