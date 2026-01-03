#pragma once

#include <algorithm>
#include <vector>

#include "Order.hpp"
#include "OrderBook.hpp"

class MatchingStrategy {
 public:
  MatchingStrategy() = default;
  virtual ~MatchingStrategy() = default;

  MatchingStrategy(const MatchingStrategy&) = default;
  MatchingStrategy& operator=(const MatchingStrategy&) = default;
  MatchingStrategy(MatchingStrategy&&) = default;
  MatchingStrategy& operator=(MatchingStrategy&&) = default;

  virtual void match(OrderBook& book, Order& incoming,
                     std::vector<Trade>& trades) = 0;
};

class StandardMatchingStrategy : public MatchingStrategy {
 public:
  void match(OrderBook& book, Order& incoming,
             std::vector<Trade>& trades) override {
    if (incoming.type == OrderType::Market) {
      if (incoming.side == OrderSide::Buy)
        incoming.price = OrderBook::MAX_PRICE;
      else
        incoming.price = 0;
    }

    if (incoming.side == OrderSide::Buy) {
      if (book.bestAsk == -1) {
        if (incoming.quantity > 0 && incoming.type != OrderType::Market)
          book.addOrder(incoming);
        return;
      }

      Price p = book.getBestAsk();

      while (p < OrderBook::MAX_PRICE) {
        if (!book.askMask.test(p)) {
          p = (Price)book.askMask.findFirstSet(p);
          if (p >= OrderBook::MAX_PRICE) break;
        }

        if (p > incoming.price && incoming.type == OrderType::Limit) break;

        auto& level = book.asks[p];
        if (level.activeCount > 0) {
          size_t size = level.orders.size();
          for (size_t i = level.headIndex; i < size; ++i) {
            Order& bookOrder = level.orders[i];
            if (!bookOrder.active) {
              if (i == level.headIndex) level.headIndex++;
              continue;
            }

            Quantity qty = std::min(incoming.quantity, bookOrder.quantity);

            trades.emplace_back(bookOrder.id, incoming.id, incoming.symbolId,
                                bookOrder.price, qty);

            bookOrder.quantity -= qty;
            incoming.quantity -= qty;

            if (bookOrder.quantity == 0) {
              bookOrder.active = false;
              level.activeCount--;
              if (i == level.headIndex) level.headIndex++;

              if (level.activeCount == 0 && book.getBestAsk() == p) {
                book.askMask.clear(p);
                level.orders.clear();
                level.headIndex = 0;
                break;
              }
            }
            if (incoming.quantity == 0) break;
          }
        }

        if (incoming.quantity == 0) break;
        p++;
        if (p > book.bestAsk)
          book.bestAsk = (p < OrderBook::MAX_PRICE) ? p : -1;
      }
      if (book.askMask.findFirstSet(book.bestAsk) >= OrderBook::MAX_PRICE) {
        book.bestAsk = -1;
      }

    } else {
      if (book.bestBid == 0 && !book.bidMask.test(0)) {
        if (incoming.quantity > 0 && incoming.type != OrderType::Market)
          book.addOrder(incoming);
        return;
      }

      Price p = book.getBestBid();
      while (p >= 0) {
        if (!book.bidMask.test(p)) {
          if (p == 0) break;
          size_t next = book.bidMask.findFirstSetDown(p - 1);
          if (next >= OrderBook::MAX_PRICE) {
            if (book.bidMask.test(0)) {
              p = 0;
            } else
              break;
          } else {
            p = (Price)next;
          }
          if (!book.bidMask.test(p)) break;
        }

        if (p < incoming.price && incoming.type == OrderType::Limit) break;

        auto& level = book.bids[p];
        if (level.activeCount > 0) {
          size_t size = level.orders.size();
          for (size_t i = level.headIndex; i < size; ++i) {
            Order& bookOrder = level.orders[i];
            if (!bookOrder.active) {
              if (i == level.headIndex) level.headIndex++;
              continue;
            }

            Quantity qty = std::min(incoming.quantity, bookOrder.quantity);
            trades.emplace_back(bookOrder.id, incoming.id, incoming.symbolId,
                                bookOrder.price, qty);

            bookOrder.quantity -= qty;
            incoming.quantity -= qty;

            if (bookOrder.quantity == 0) {
              bookOrder.active = false;
              level.activeCount--;
              if (i == level.headIndex) level.headIndex++;

              if (level.activeCount == 0) {
                book.bidMask.clear(p);
                level.orders.clear();
                level.headIndex = 0;
                break;
              }
            }
            if (incoming.quantity == 0) break;
          }
        }

        if (incoming.quantity == 0) break;
        if (p == 0) break;
        p--;
        book.bestBid = p;
      }
      if (book.bestBid > 0 && !book.bidMask.test(book.bestBid)) {
        size_t next = book.bidMask.findFirstSetDown(book.bestBid);
        book.bestBid = (next >= OrderBook::MAX_PRICE) ? 0 : (Price)next;
        if (!book.bidMask.test(book.bestBid)) book.bestBid = 0;
      }
    }

    if (incoming.quantity > 0 && incoming.type != OrderType::Market) {
      book.addOrder(incoming);
    }
  }
};
