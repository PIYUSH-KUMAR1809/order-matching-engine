#include "MatchingStrategy.hpp"

#include <algorithm>

std::vector<Trade> StandardMatchingStrategy::match(OrderBook& book,
                                                   const Order& order) {
  std::vector<Trade> trades;
  Order incoming = order;  // Make a copy to modify quantity

  if (incoming.side == OrderSide::Buy) {
    // Attempt to match with Asks
    auto& asks = book.getAsksInternal();  // We need mutable access
    while (incoming.quantity > 0 && !asks.empty()) {
      auto bestAskIt = asks.begin();
      auto& askList = bestAskIt->second;
      Order& ask = askList.front();

      // Price Check
      if (incoming.type == OrderType::Limit && incoming.price < ask.price) {
        break;  // Best ask is too expensive
      }

      Quantity quantity = std::min(incoming.quantity, ask.quantity);
      trades.push_back({ask.price, quantity, ask.id, incoming.id});

      ask.quantity -= quantity;
      incoming.quantity -= quantity;

      if (ask.quantity == 0) {
        book.removeOrderInternal(ask.id);  // Helper to sync maps
      }

      // If ask quantity was 0, removeOrderInternal handles list/map cleanup?
      // We need to be careful about iterator invalidation here if
      // removeOrderInternal invalidates bestAskIt. Standard way: check if list
      // empty, then erase from map.
    }

    // If remaining and Limit, add to book
    if (incoming.quantity > 0 && incoming.type == OrderType::Limit) {
      book.addOrderInternal(incoming);
    }
  } else {
    // Sell Order - Match with Bids
    auto& bids = book.getBidsInternal();
    while (incoming.quantity > 0 && !bids.empty()) {
      auto bestBidIt = bids.begin();
      // list is sorted by price descending
      auto& bidList = bestBidIt->second;
      Order& bid = bidList.front();

      if (incoming.type == OrderType::Limit && incoming.price > bid.price) {
        break;  // Best bid is too low
      }

      Quantity quantity = std::min(incoming.quantity, bid.quantity);
      trades.push_back({bid.price, quantity, bid.id, incoming.id});

      bid.quantity -= quantity;
      incoming.quantity -= quantity;

      if (bid.quantity == 0) {
        book.removeOrderInternal(bid.id);
      }
    }

    if (incoming.quantity > 0 && incoming.type == OrderType::Limit) {
      book.addOrderInternal(incoming);
    }
  }

  return trades;
}
