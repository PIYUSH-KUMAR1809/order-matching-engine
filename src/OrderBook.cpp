#include "OrderBook.hpp"

#include <iostream>
#include <mutex>
#include <shared_mutex>

std::vector<Trade> OrderBook::addOrder(const Order &order) {
  std::unique_lock lock(mutex_);
  if (orderIndex.find(order.id) != orderIndex.end()) {
    return {};  // Duplicate Order ID
  }

  // Market Order Logic (IOC)
  if (order.kind == OrderKind::Market) {
    std::vector<Trade> trades;
    Quantity remaining = order.quantity;

    if (order.type == OrderType::Buy) {
      while (remaining > 0 && !asks.empty()) {
        auto bestAskIt = asks.begin();
        auto &askList = bestAskIt->second;
        Order &ask = askList.front();

        Quantity quantity = std::min(remaining, ask.quantity);
        trades.push_back({ask.price, quantity, ask.id, order.id});

        ask.quantity -= quantity;
        remaining -= quantity;

        if (ask.quantity == 0) {
          orderIndex.erase(ask.id);
          askList.pop_front();
          if (askList.empty()) {
            asks.erase(bestAskIt);
          }
        }
      }
    } else {
      // Market Sell
      while (remaining > 0 && !bids.empty()) {
        auto bestBidIt = bids.begin();
        auto &bidList = bestBidIt->second;
        Order &bid = bidList.front();

        Quantity quantity = std::min(remaining, bid.quantity);
        trades.push_back({bid.price, quantity, bid.id, order.id});

        bid.quantity -= quantity;
        remaining -= quantity;

        if (bid.quantity == 0) {
          orderIndex.erase(bid.id);
          bidList.pop_front();
          if (bidList.empty()) {
            bids.erase(bestBidIt);
          }
        }
      }
    }
    return trades;
  }

  // Limit Order Logic
  if (order.type == OrderType::Buy) {
    bids[order.price].push_back(order);
    auto it = bids[order.price].end();
    --it;  // Iterator to the newly added order
    orderIndex[order.id] = {order.price, order.type, it};
  } else {
    asks[order.price].push_back(order);
    auto it = asks[order.price].end();
    --it;
    orderIndex[order.id] = {order.price, order.type, it};
  }

  return match();
}

void OrderBook::cancelOrder(OrderId orderId) {
  std::unique_lock lock(mutex_);
  auto it = orderIndex.find(orderId);
  if (it == orderIndex.end()) {
    return;  // Order not found
  }

  const auto &location = it->second;
  if (location.type == OrderType::Buy) {
    auto &list = bids[location.price];
    list.erase(location.iterator);
    if (list.empty()) {
      bids.erase(location.price);
    }
  } else {
    auto &list = asks[location.price];
    list.erase(location.iterator);
    if (list.empty()) {
      asks.erase(location.price);
    }
  }

  orderIndex.erase(it);
}

std::vector<Trade> OrderBook::match() {
  // Mutex is already locked by addOrder
  std::vector<Trade> trades;

  while (!bids.empty() && !asks.empty()) {
    auto bestBidIt = bids.begin();
    auto bestAskIt = asks.begin();

    // Check for overlap
    if (bestBidIt->first >= bestAskIt->first) {
      auto &bidList = bestBidIt->second;
      auto &askList = bestAskIt->second;

      Order &bid = bidList.front();
      Order &ask = askList.front();

      Quantity quantity = std::min(bid.quantity, ask.quantity);

      // Determine Maker/Taker and Trade Price
      // The order that was in the book EARLIER is the Maker.
      // The Maker determines the price.
      Price tradePrice;
      OrderId makerId, takerId;

      if (bid.timestamp < ask.timestamp) {
        // Bid is older (Maker)
        tradePrice = bid.price;
        makerId = bid.id;
        takerId = ask.id;
      } else {
        // Ask is older (Maker)
        tradePrice = ask.price;
        makerId = ask.id;
        takerId = bid.id;
      }

      trades.push_back({tradePrice, quantity, makerId, takerId});

      bid.quantity -= quantity;
      ask.quantity -= quantity;

      if (bid.quantity == 0) {
        orderIndex.erase(bid.id);
        bidList.pop_front();
        if (bidList.empty()) {
          bids.erase(bestBidIt);
        }
      }

      if (ask.quantity == 0) {
        orderIndex.erase(ask.id);
        askList.pop_front();
        if (askList.empty()) {
          asks.erase(bestAskIt);
        }
      }
    } else {
      break;  // No overlap
    }
  }
  return trades;
}

void OrderBook::printBook() const {
  std::shared_lock lock(mutex_);
  std::cout << "--- Order Book ---" << std::endl;
  std::cout << "ASKS:" << std::endl;
  for (auto it = asks.rbegin(); it != asks.rend(); ++it) {
    for (const auto &order : it->second) {
      std::cout << order.price << " x " << order.quantity
                << " (ID: " << order.id << ")" << std::endl;
    }
  }
  std::cout << "BIDS:" << std::endl;
  for (const auto &pair : bids) {
    for (const auto &order : pair.second) {
      std::cout << pair.first << " x " << order.quantity << " (ID: " << order.id
                << ")" << std::endl;
    }
  }
  std::cout << "------------------" << std::endl;
}
