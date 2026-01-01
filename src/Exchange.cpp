#include "Exchange.hpp"

#include <iostream>

Exchange::Exchange() {
  int numWorkers = std::thread::hardware_concurrency();
  if (numWorkers == 0) numWorkers = 1;

  shards_.resize(numWorkers);
  for (int i = 0; i < numWorkers; ++i) {
    shards_[i] = std::make_unique<Shard>();
    shards_[i]->matchingStrategy = std::make_unique<StandardMatchingStrategy>();
  }

  for (int i = 0; i < numWorkers; ++i) {
    workers_.emplace_back(&Exchange::workerLoop, this, i);
  }
}

Exchange::~Exchange() {
  for (auto &shard : shards_) {
    shard->queue.push_block({Command::Stop, {}, 0, ""});
  }
}

size_t Exchange::getShardId(const std::string &symbol) const {
  return std::hash<std::string>{}(symbol) % shards_.size();
}

void Exchange::submitOrder(const Order &order, int shardHint) {
  size_t shardId = (shardHint >= 0 && shardHint < shards_.size())
                       ? shardHint
                       : getShardId(order.symbol);
  auto &shard = *shards_[shardId];
  shard.queue.push_block({Command::Add, order, 0, order.symbol});
}

void Exchange::cancelOrder(const std::string &symbol, OrderId orderId) {
  size_t shardId = getShardId(symbol);
  auto &shard = *shards_[shardId];
  shard.queue.push_block({Command::Cancel, {}, orderId, symbol});
}

void Exchange::setTradeCallback(TradeCallback cb) { onTrade_ = std::move(cb); }

void Exchange::workerLoop(int shardId) {
  auto &shard = *shards_[shardId];

  while (true) {
    Command cmd;
    if (shard.queue.pop(cmd)) {
      if (cmd.type == Command::Stop) {
        break;
      }

      if (shard.books.find(cmd.symbol) == shard.books.end()) {
        shard.books[cmd.symbol] = std::make_unique<OrderBook>();
      }
      auto &book = *shard.books[cmd.symbol];

      if (cmd.type == Command::Add) {
        shard.matchingStrategy->match(book, cmd.order, shard.tradeBuffer);

        if (!shard.tradeBuffer.empty() && onTrade_) {
          onTrade_(shard.tradeBuffer);
        }
      } else if (cmd.type == Command::Cancel) {
        book.cancelOrder(cmd.cancelId);
      }
    } else {
      for (auto &pair : shard.books) {
        pair.second->compact();
      }
      std::this_thread::yield();
    }
  }
}

void Exchange::printOrderBook(const std::string &symbol) const {
  size_t shardId = getShardId(symbol);
  const auto &shard = *shards_[shardId];

  if (shard.books.find(symbol) != shard.books.end()) {
    std::cout << "Symbol: " << symbol << std::endl;
    shard.books.at(symbol)->printBook();
  } else {
    std::cout << "OrderBook for " << symbol << " not found." << std::endl;
  }
}

void Exchange::printAllOrderBooks() const {
  for (const auto &shard : shards_) {
    for (const auto &pair : shard->books) {
      std::cout << "Symbol: " << pair.first << std::endl;
      pair.second->printBook();
      std::cout << std::endl;
    }
  }
}

const OrderBook *Exchange::getOrderBook(const std::string &symbol) const {
  size_t shardId = getShardId(symbol);
  const auto &shard = *shards_[shardId];

  auto it = shard.books.find(symbol);
  if (it != shard.books.end()) {
    return it->second.get();
  }
  return nullptr;
}
