#include "OrderBook.hpp"

#include <iostream>

void OrderBook::addOrderInternal(const Order &order) {
  if (orderIndex.find(order.id) != orderIndex.end()) {
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

void OrderBook::removeOrderInternal(OrderId orderId) {
  auto it = orderIndex.find(orderId);
  if (it == orderIndex.end()) {
    return;
  }

  const auto &location = it->second;
  if (location.orderPtr) {
    location.orderPtr->active = false;
  }

  orderIndex.erase(it);
}

void OrderBook::removeIndexInternal(OrderId orderId) {
  orderIndex.erase(orderId);
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
