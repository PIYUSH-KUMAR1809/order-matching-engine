#include "MatchingStrategy.hpp"

#include <algorithm>
#include <cstring>

void StandardMatchingStrategy::match(OrderBook& book, const Order& order,
                                     std::vector<Trade>& trades) {
  trades.clear();
  Order incoming = order;

  if (incoming.side == OrderSide::Buy) {
    Price currentPrice = book.getBestAsk();
    if (currentPrice >= OrderBook::MAX_PRICE) currentPrice = book.getNextAsk(0);

    while (incoming.quantity > 0) {
      currentPrice = book.getNextAsk(currentPrice);
      if (currentPrice >= OrderBook::MAX_PRICE) break;
      if (incoming.type == OrderType::Limit && currentPrice > incoming.price)
        break;

      int32_t headIdx = book.getOrderHead(currentPrice, OrderSide::Sell);
      int32_t currIdx = headIdx;

      while (currIdx != -1) {
        OrderNode& askNode = book.getNode(currIdx);

        if (!askNode.active) {
          currIdx = askNode.next;
          continue;
        }

        Quantity quantity = std::min(incoming.quantity, askNode.order.quantity);

        Trade t;
        std::memcpy(t.symbol, incoming.symbol, 8);
        t.price = askNode.order.price;
        t.quantity = quantity;
        t.makerOrderId = askNode.order.id;
        t.takerOrderId = incoming.id;
        trades.push_back(t);

        askNode.order.quantity -= quantity;
        incoming.quantity -= quantity;

        if (askNode.order.quantity == 0) {
          askNode.active = false;
        }

        if (incoming.quantity == 0) break;

        currIdx = askNode.next;
      }

      if (incoming.quantity > 0) {
        book.resetLevel(currentPrice, OrderSide::Sell);
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

      int32_t headIdx = book.getOrderHead(currentPrice, OrderSide::Buy);
      int32_t currIdx = headIdx;

      while (currIdx != -1) {
        OrderNode& bidNode = book.getNode(currIdx);

        if (!bidNode.active) {
          currIdx = bidNode.next;
          continue;
        }

        Quantity quantity = std::min(incoming.quantity, bidNode.order.quantity);

        Trade t;
        std::memcpy(t.symbol, incoming.symbol, 8);
        t.price = bidNode.order.price;
        t.quantity = quantity;
        t.makerOrderId = bidNode.order.id;
        t.takerOrderId = incoming.id;
        trades.push_back(t);

        bidNode.order.quantity -= quantity;
        incoming.quantity -= quantity;

        if (bidNode.order.quantity == 0) {
          bidNode.active = false;
        }

        if (incoming.quantity == 0) break;

        currIdx = bidNode.next;
      }

      if (incoming.quantity > 0) {
        book.resetLevel(currentPrice, OrderSide::Buy);
      }

      if (currentPrice == 0) break;
      currentPrice--;
    }

    if (incoming.quantity > 0 && incoming.type == OrderType::Limit) {
      book.addOrder(incoming);
    }
  }
}
