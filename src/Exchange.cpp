#include "Exchange.hpp"

#include <iostream>

Exchange::Exchange(int numWorkers) {
  if (numWorkers <= 0) {
    numWorkers = std::thread::hardware_concurrency();
  }
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

Exchange::~Exchange() { stop(); }

void Exchange::stop() {
  for (auto &shard : shards_) {
    shard->queue.push_block({Command::Stop, {}, 0, ""});
  }
  workers_.clear();
}

size_t Exchange::getShardId(std::string_view symbol) const {
  return std::hash<std::string_view>{}(symbol) % shards_.size();
}

void Exchange::submitOrder(const Order &order, int shardHint) {
  size_t shardId = (shardHint >= 0 && shardHint < shards_.size())
                       ? shardHint
                       : getShardId(order.symbol);

  auto &shard = *shards_[shardId];
  Command cmd;
  cmd.type = Command::Add;
  cmd.order = order;
  cmd.cancelId = 0;
  std::memcpy(cmd.symbol, order.symbol, 8);
  shard.queue.push_block(cmd);
}

void Exchange::cancelOrder(const std::string &symbol, OrderId orderId) {
  size_t shardId = getShardId(symbol);
  auto &shard = *shards_[shardId];

  Command cmd;
  cmd.type = Command::Cancel;
  cmd.cancelId = orderId;
  std::memset(cmd.symbol, 0, 8);
  std::strncpy(cmd.symbol, symbol.c_str(), 7);

  shard.queue.push_block(cmd);
}

void Exchange::setTradeCallback(TradeCallback cb) { onTrade_ = std::move(cb); }

void Exchange::workerLoop(int shardId) {
  auto &shard = *shards_[shardId];

  OrderBook *lastBook = nullptr;
  char lastSymbol[8] = {0};

  const size_t BATCH_SIZE = 64;
  Command cmdBuffer[BATCH_SIZE];

  while (true) {
    size_t count = shard.queue.pop_batch(cmdBuffer, BATCH_SIZE);

    if (count == 0) {
      for (auto &pair : shard.books) {
        pair.second->compact();
      }
      std::this_thread::yield();
      continue;
    }

    for (size_t i = 0; i < count; ++i) {
      const Command &cmd = cmdBuffer[i];

      if (cmd.type == Command::Stop) {
        return;
      }

      OrderBook *book = nullptr;

      if (lastBook && std::memcmp(cmd.symbol, lastSymbol, 8) == 0) {
        book = lastBook;
      } else {
        auto it = shard.books.find(cmd.symbol);
        if (it == shard.books.end()) {
          it = shard.books.emplace(cmd.symbol, std::make_unique<OrderBook>())
                   .first;
        }
        book = it->second.get();
        lastBook = book;
        std::memcpy(lastSymbol, cmd.symbol, 8);
      }

      if (cmd.type == Command::Add) {
        shard.matchingStrategy->match(*book, cmd.order, shard.tradeBuffer);

        if (!shard.tradeBuffer.empty() && onTrade_) {
          onTrade_(shard.tradeBuffer);
        }
      } else if (cmd.type == Command::Cancel) {
        book->cancelOrder(cmd.cancelId);
      }
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
