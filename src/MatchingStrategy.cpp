#include "MatchingStrategy.hpp"

#include <algorithm>

std::vector<Trade> StandardMatchingStrategy::match(OrderBook& book,
                                                   const Order& order) {
  std::vector<Trade> trades;
  Order incoming = order;

  if (incoming.side == OrderSide::Buy) {
    auto& asks = book.getAsksInternal();
    auto askIt = asks.begin();

    while (incoming.quantity > 0 && askIt != asks.end()) {
      auto& askQueue = askIt->second;

      while (!askQueue.empty() && incoming.quantity > 0) {
        Order& ask = askQueue.front();

        if (!ask.active) {
          book.removeIndexInternal(ask.id);
          askQueue.pop_front();
          continue;
        }

        if (incoming.type == OrderType::Limit && incoming.price < ask.price) {
          goto end_match_buy;
        }

        Quantity quantity = std::min(incoming.quantity, ask.quantity);
        trades.push_back(
            {incoming.symbol, ask.price, quantity, ask.id, incoming.id});

        ask.quantity -= quantity;
        incoming.quantity -= quantity;

        if (ask.quantity == 0) {
          book.removeIndexInternal(ask.id);
          askQueue.pop_front();
        }
      }

      if (askQueue.empty()) {
        askIt = asks.erase(askIt);
      } else {
        ++askIt;
      }
    }
  end_match_buy:;

    if (incoming.quantity > 0 && incoming.type == OrderType::Limit) {
      book.addOrderInternal(incoming);
    }

  } else {
    auto& bids = book.getBidsInternal();
    auto bidIt = bids.begin();

    while (incoming.quantity > 0 && bidIt != bids.end()) {
      auto& bidQueue = bidIt->second;

      while (!bidQueue.empty() && incoming.quantity > 0) {
        Order& bid = bidQueue.front();

        if (!bid.active) {
          book.removeIndexInternal(bid.id);
          bidQueue.pop_front();
          continue;
        }

        if (incoming.type == OrderType::Limit && incoming.price > bid.price) {
          goto end_match_sell;
        }

        Quantity quantity = std::min(incoming.quantity, bid.quantity);
        trades.push_back(
            {incoming.symbol, bid.price, quantity, bid.id, incoming.id});

        bid.quantity -= quantity;
        incoming.quantity -= quantity;

        if (bid.quantity == 0) {
          book.removeIndexInternal(bid.id);
          bidQueue.pop_front();
        }
      }

      if (bidQueue.empty()) {
        bidIt = bids.erase(bidIt);
      } else {
        ++bidIt;
      }
    }
  end_match_sell:;

    if (incoming.quantity > 0 && incoming.type == OrderType::Limit) {
      book.addOrderInternal(incoming);
    }
  }

  return trades;
}
