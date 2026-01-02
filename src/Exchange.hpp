#pragma once

#include <array>
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
    enum Type : uint8_t { Add, Cancel, Stop } type = Add;
    Order order = {};
    OrderId cancelId = 0;
    std::array<char, 8> symbol = {};
  };

  void submitOrder(const Order &order, int shardHint = -1,
                   std::chrono::nanoseconds *wait_duration = nullptr);
  void cancelOrder(const std::string &symbol, OrderId orderId);
  void stop();
  void flush();

  void registerSymbol(const std::string &symbol, int shardId);

  void setTradeCallback(TradeCallback cb);

  void printOrderBook(const std::string &symbol) const;
  void printAllOrderBooks() const;
  const OrderBook *getOrderBook(const std::string &symbol) const;

  static void pinThread(int coreId);

 private:
  struct StringHash {
    using is_transparent = void;
    size_t operator()(const char *txt) const {
      return std::hash<std::string_view>{}(txt);
    }
    size_t operator()(std::string_view txt) const {
      return std::hash<std::string_view>{}(txt);
    }
    size_t operator()(const std::string &txt) const {
      return std::hash<std::string>{}(txt);
    }
  };

  struct Shard {
    RingBuffer<Command> queue{65536};
    std::unordered_map<std::string, std::unique_ptr<OrderBook>, StringHash,
                       std::equal_to<>>
        books;
    std::unique_ptr<MatchingStrategy> matchingStrategy;
    std::vector<Trade> tradeBuffer;
  };

  void workerLoop(int shardId);
  size_t getShardId(std::string_view symbol) const;

  std::vector<std::unique_ptr<Shard>> shards_;
  std::vector<std::jthread> workers_;
  TradeCallback onTrade_;
  std::unordered_map<std::string, size_t, StringHash, std::equal_to<>>
      symbolShardMap_;
};
