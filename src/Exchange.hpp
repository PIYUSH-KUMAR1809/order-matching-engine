#pragma once

#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "MatchingStrategy.hpp"
#include "RingBuffer.hpp"

class Exchange {
 public:
  using TradeCallback = std::function<void(const std::vector<Trade> &)>;

  Exchange();
  ~Exchange();

  void submitOrder(const Order &order, int shardHint = -1);
  void cancelOrder(const std::string &symbol, OrderId orderId);

  void setTradeCallback(TradeCallback cb);

  void printOrderBook(const std::string &symbol) const;
  void printAllOrderBooks() const;
  const OrderBook *getOrderBook(const std::string &symbol) const;

 private:
  struct Command {
    enum Type { Add, Cancel, Stop } type;
    Order order;
    OrderId cancelId;
    std::string symbol;
  };

  struct Shard {
    RingBuffer<Command> queue{65536};
    std::unordered_map<std::string, std::unique_ptr<OrderBook>> books;
    std::unique_ptr<MatchingStrategy> matchingStrategy;
    std::vector<Trade> tradeBuffer;
  };

  void workerLoop(int shardId);
  size_t getShardId(const std::string &symbol) const;

  std::vector<std::unique_ptr<Shard>> shards_;
  std::vector<std::jthread> workers_;
  TradeCallback onTrade_;
};
