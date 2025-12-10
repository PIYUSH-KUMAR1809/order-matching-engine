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
    std::unique_lock lock(exchangeMutex_);
    orderSymbolIndex[order.id] = order.symbol;
    if (orderBooks.find(order.symbol) == orderBooks.end()) {
      orderBooks[order.symbol] = std::make_unique<OrderBook>();
    }
    book = orderBooks[order.symbol].get();

    // Use the strategy to match/add
    // Note: Locking is tricky. StandardMatchingStrategy expects to be able to
    // mutate the book. Since we are in Exchange, we derived proper access. For
    // Phase 2, we assume single-threaded access per order processing OR the
    // book lock. If we removed book lock, we need to hold lock here?
    // Exchange::exchangeMutex_ protects the map of books.
    // We need a lock on the specific book.
    // OrderBook::addOrderInternal doesn't lock anymore.
    // So we need a lock on 'book'.
    // TODO: Add per-book mutex in Exchange or OrderBook?
    // User feedback: "shard-per-thread".
    // For now, let's just make it work.

    return matchingStrategy->match(*book, order);
  }
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
