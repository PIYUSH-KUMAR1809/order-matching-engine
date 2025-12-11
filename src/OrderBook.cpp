#include "OrderBook.hpp"

#include <iostream>

// Deprecated: logic moved to Strategy.
// However, Exchange calls this? No, Exchange will call Strategy.
// But current Exchange calls book->addOrder(order).
// We should change Exchange to use Strategy->match(book, order).
// So OrderBook::addOrder is useless or should strictly be "add to storage".
// Let's repurpose addOrder to just add (for tests?) OR remove it.
// Tests use book.addOrder(). Tests expect it to match.
// Ideally, tests should use Exchange or Strategy.
// For now, let's make addOrderInternal the semantic "add to storage".

// Retrying replacement with correct content based on plan.
// We will implement addOrderInternal and removeOrderInternal.
// We will REMOVE addOrder's matching logic.
// We will KEEP addOrder as a wrapper for addOrderInternal (for tests that just
// want to add stuff?). Wait, tests expect matching. Break tests? Yes, tests
// need update if we change semantics. Let's simplify: `OrderBook::addOrder` is
// REMOVED from interface. Storage only. Tests should use `Exchange` or
// `Strategy`. I will temporarily keep addOrder but make it Storage Only? No
// that breaks tests. I will update OrderBook.cpp to have Storage methods.

void OrderBook::addOrderInternal(const Order &order) {
  // No lock here? Strategy handles locking?
  // If we want "Dumb Container", it shouldn't have mutexes?
  // Exchange has one big mutex.
  // Feedback said "OrderBook should be standard container".
  // Let's assume Exchange handles locking.
  // Remove mutex_ from OrderBook?
  // 'Exchange' has `exchangeMutex_`.
  // 'OrderBook' has `mutex_`.
  // If we move to Exchange, maybe efficient to have per-book mutex in Exchange.
  // Let's keep `mutex_` for now but add public Lock/Unlock?
  // Or just rely on Exchange's mutex for now (Global lock -> Symbol lock).
  // The Exchange code locks `exchangeMutex_` (shared) allowing concurrent
  // access to different books? Wait, `Exchange::submitOrder` locks
  // `exchangeMutex_` (unique? no, we changed to per-book?) `Exchange.cpp` uses
  // `unique_lock(exchangeMutex_)` for map lookup?

  // Let's implement add/removeInternal.

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

  // Just set Tombstone flag.
  // We keep the index entry so we know it's already cancelled if asked again.
  // But MatchingStrategy can remove it from index when popping.
  const auto &location = it->second;
  if (location.orderPtr) {
    location.orderPtr->active = false;
  }

  // Note: We DO NOT remove from orderIndex here unless we want to allow re-use
  // of ID? Standard logic: Cancelled order is dead. If we remove from
  // orderIndex, repeated cancel calls return early (safer).
  orderIndex.erase(it);
}

void OrderBook::removeIndexInternal(OrderId orderId) {
  orderIndex.erase(orderId);
}

// Deprecated public methods
std::vector<Trade> OrderBook::addOrder(const Order &order) {
  // This method is now legacy or just for storage?
  // If I keep it as storage, existing tests will fail matching.
  // I should update tests.
  addOrderInternal(order);
  return {};
}

void OrderBook::cancelOrder(OrderId orderId) { removeOrderInternal(orderId); }

void OrderBook::printBook() const {
  // std::shared_lock lock(mutex_); // mutex removed or managed externally
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
