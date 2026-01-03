#pragma once

#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "MatchingStrategy.hpp"
#include "OrderBook.hpp"
#include "RingBuffer.hpp"

class Exchange {
 public:
  using TradeCallback = std::function<void(const std::vector<Trade> &)>;

  Exchange(int numWorkers = 0);
  ~Exchange();

  Exchange(const Exchange &) = delete;
  Exchange &operator=(const Exchange &) = delete;
  Exchange(Exchange &&) = delete;
  Exchange &operator=(Exchange &&) = delete;

  struct Command {
    enum Type : uint8_t { Add, Cancel, Stop, Reset } type;
    union {
      struct {
        Order order;
      } add;
      struct {
        OrderId orderId;
        int32_t symbolId;
      } cancel;
    };
    Command() : type(Add) { std::memset(&add, 0, sizeof(add)); }
  };

  void submitOrder(const Order &order, int shardHint = -1,
                   std::chrono::nanoseconds *wait_duration = nullptr);
  void submitOrders(const std::vector<Order> &orders, int shardHint = -1);
  void cancelOrder(int32_t symbolId, OrderId orderId);
  void stop();
  void flush();
  void drain();
  void reset();

  int32_t registerSymbol(const std::string &symbol, int shardId);
  std::string getSymbolName(int32_t symbolId) const;

  void setTradeCallback(TradeCallback cb);

  void printOrderBook(int32_t symbolId) const;
  void printAllOrderBooks() const;
  const OrderBook *getOrderBook(int32_t symbolId) const;

  static void pinThread(int coreId);

 private:
  struct Shard {
    RingBuffer<Command> queue{65536};

    std::vector<std::unique_ptr<OrderBook>> books;
    StandardMatchingStrategy matchingStrategy;
    std::vector<Trade> tradeBuffer;
  };

  void workerLoop(int shardId);

  std::vector<std::unique_ptr<Shard>> shards_;
  std::vector<std::jthread> workers_;
  TradeCallback onTrade_;

  std::unordered_map<std::string, int32_t> symbolNameToId_;
  std::vector<std::string> symbolIdToName_;
  std::vector<int> symbolIdToShardId_;
};
