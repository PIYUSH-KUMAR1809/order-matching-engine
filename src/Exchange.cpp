#include "Exchange.hpp"

#include <iostream>
#include <mutex>

std::vector<Trade> Exchange::submitOrder(const Order &order) {
  OrderBook *book = nullptr;

  {
    std::unique_lock lock(exchangeMutex_);
    orderSymbolIndex[order.id] = order.symbol;
    if (orderBooks.find(order.symbol) == orderBooks.end()) {
      orderBooks[order.symbol] = std::make_unique<OrderBook>();
    }
    book = orderBooks[order.symbol].get();
  }

  return book->addOrder(order);
}

void Exchange::cancelOrder(OrderId orderId) {
  OrderBook *book = nullptr;

  {
    std::shared_lock lock(exchangeMutex_);
    if (orderSymbolIndex.find(orderId) == orderSymbolIndex.end()) {
      return;
    }
    std::string symbol = orderSymbolIndex.at(orderId);
    if (orderBooks.find(symbol) != orderBooks.end()) {
      book = orderBooks[symbol].get();
    }
  }

  if (book) {
    book->cancelOrder(orderId);
    // Note: We don't remove from orderSymbolIndex to avoid race conditions
    // with concurrent lookups. In a real system, we'd need a more complex
    // cleanup strategy or a tombstone mechanism.
  }
}

void Exchange::printOrderBook(const std::string &symbol) const {
  OrderBook *book = nullptr;
  {
    std::shared_lock lock(exchangeMutex_);
    if (orderBooks.find(symbol) != orderBooks.end()) {
      book = orderBooks.at(symbol).get();
    }
  }

  if (book) {
    std::cout << "Symbol: " << symbol << std::endl;
    book->printBook();
  } else {
    std::cout << "OrderBook for " << symbol << " not found." << std::endl;
  }
}

const OrderBook *Exchange::getOrderBook(const std::string &symbol) const {
  std::shared_lock lock(exchangeMutex_);
  if (orderBooks.find(symbol) != orderBooks.end()) {
    return orderBooks.at(symbol).get();
  }
  return nullptr;
}

void Exchange::printAllOrderBooks() const {
  std::shared_lock lock(exchangeMutex_);
  for (const auto &pair : orderBooks) {
    std::cout << "Symbol: " << pair.first << std::endl;
    pair.second->printBook();
    std::cout << std::endl;
  }
}
