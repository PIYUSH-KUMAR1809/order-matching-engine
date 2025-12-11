#include "Exchange.hpp"

#include <iostream>
#include <mutex>

Exchange::Exchange() {
  matchingStrategy = std::make_unique<StandardMatchingStrategy>();
}

Exchange::~Exchange() = default;

std::vector<Trade> Exchange::submitOrder(const Order &order) {
  OrderBook *book = nullptr;

  {
    std::shared_lock readLock(exchangeMutex_);
    auto it = orderBooks.find(order.symbol);
    if (it != orderBooks.end()) {
      book = it->second.get();
    }
  }

  if (!book) {
    std::unique_lock writeLock(exchangeMutex_);

    if (orderBooks.find(order.symbol) == orderBooks.end()) {
      orderBooks[order.symbol] = std::make_unique<OrderBook>();
    }
    book = orderBooks[order.symbol].get();
  }

  book->lock();
  std::vector<Trade> trades = matchingStrategy->match(*book, order);
  book->unlock();

  return trades;
}

void Exchange::cancelOrder(const std::string &symbol, OrderId orderId) {
  OrderBook *book = nullptr;

  {
    std::shared_lock lock(exchangeMutex_);
    if (orderBooks.find(symbol) != orderBooks.end()) {
      book = orderBooks.at(symbol).get();
    }
  }

  if (book) {
    book->lock();
    book->cancelOrder(orderId);
    book->unlock();
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
