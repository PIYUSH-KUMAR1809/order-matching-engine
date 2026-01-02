#pragma once

#include <algorithm>
#include <cstring>
#include <vector>

#include "OrderBook.hpp"

class MatchingStrategy {
 public:
  virtual ~MatchingStrategy() = default;
  MatchingStrategy() = default;
  MatchingStrategy(const MatchingStrategy&) = default;
  MatchingStrategy& operator=(const MatchingStrategy&) = default;
  MatchingStrategy(MatchingStrategy&&) = default;
  MatchingStrategy& operator=(MatchingStrategy&&) = default;
  virtual void match(OrderBook& book, const Order& order,
                     std::vector<Trade>& out_trades) = 0;
};

class NullMatchingStrategy : public MatchingStrategy {
 public:
  inline void match(OrderBook& book, const Order& order,
                    std::vector<Trade>& trades) override {
    return;
  }
};

class StandardMatchingStrategy : public MatchingStrategy {
 public:
  inline void match(OrderBook& book, const Order& order,
                    std::vector<Trade>& trades) override {
    Order incoming = order;

    if (incoming.side == OrderSide::Buy) {
      Price currentPrice = book.getBestAsk();
      if (currentPrice >= OrderBook::MAX_PRICE)
        currentPrice = book.getNextAsk(0);

      while (incoming.quantity > 0) {
        currentPrice = book.getNextAsk(currentPrice);
        if (currentPrice >= OrderBook::MAX_PRICE) break;
        if (incoming.type == OrderType::Limit && currentPrice > incoming.price)
          break;

        while (true) {
          int32_t currIdx = book.getOrderHead(currentPrice, OrderSide::Sell);
          if (currIdx == -1) break;

          OrderNode& askNode = book.getNode(currIdx);
          if (askNode.next != -1) {
            __builtin_prefetch(&book.getNode(askNode.next), 0, 1);
          }

          if (!askNode.active) {
            book.setOrderHead(currentPrice, OrderSide::Sell, askNode.next);
            book.freeNode(currIdx);
            continue;
          }

          Quantity quantity =
              std::min(incoming.quantity, askNode.order.quantity);

          Trade t{};
          t.symbolId = incoming.symbolId;
          t.price = askNode.order.price;
          t.quantity = quantity;
          t.makerOrderId = askNode.order.id;
          t.takerOrderId = incoming.id;
          trades.push_back(t);

          askNode.order.quantity -= quantity;
          incoming.quantity -= quantity;

          if (askNode.order.quantity == 0) {
            askNode.active = false;
            book.setOrderHead(currentPrice, OrderSide::Sell, askNode.next);
            book.freeNode(currIdx);
          }

          if (incoming.quantity == 0) break;
        }

        currentPrice++;
      }

      if (incoming.quantity > 0 && incoming.type == OrderType::Limit) {
        book.addOrder(incoming);
      }

    } else {
      Price currentPrice = book.getBestBid();

      while (incoming.quantity > 0) {
        currentPrice = book.getNextBid(currentPrice);
        if (currentPrice == 0 && book.getOrderHead(0, OrderSide::Buy) == -1)
          break;

        if (incoming.type == OrderType::Limit && currentPrice < incoming.price)
          break;

        while (true) {
          int32_t currIdx = book.getOrderHead(currentPrice, OrderSide::Buy);
          if (currIdx == -1) break;

          OrderNode& bidNode = book.getNode(currIdx);
          if (bidNode.next != -1) {
            __builtin_prefetch(&book.getNode(bidNode.next), 0, 1);
          }

          if (!bidNode.active) {
            book.setOrderHead(currentPrice, OrderSide::Buy, bidNode.next);
            book.freeNode(currIdx);
            continue;
          }

          Quantity quantity =
              std::min(incoming.quantity, bidNode.order.quantity);

          Trade t{};
          t.symbolId = incoming.symbolId;
          t.price = bidNode.order.price;
          t.quantity = quantity;
          t.makerOrderId = bidNode.order.id;
          t.takerOrderId = incoming.id;
          trades.push_back(t);

          bidNode.order.quantity -= quantity;
          incoming.quantity -= quantity;

          if (bidNode.order.quantity == 0) {
            bidNode.active = false;
            book.setOrderHead(currentPrice, OrderSide::Buy, bidNode.next);
            book.freeNode(currIdx);
          }

          if (incoming.quantity == 0) break;
        }

        if (currentPrice == 0) break;
        currentPrice--;
      }

      if (incoming.quantity > 0 && incoming.type == OrderType::Limit) {
        book.addOrder(incoming);
      }
    }
  }
};
