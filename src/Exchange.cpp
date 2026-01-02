#include "Exchange.hpp"

#include <iostream>

Exchange::Exchange(int numWorkers) {
  if (numWorkers <= 0) {
    numWorkers = static_cast<int>(std::thread::hardware_concurrency());
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
  flush();
  for (auto &shard : shards_) {
    shard->queue.push_block({Command::Stop, {}, 0, {}});
  }
  workers_.clear();
}

void Exchange::registerSymbol(const std::string &symbol, int shardId) {
  if (shardId >= 0 && shardId < static_cast<int>(shards_.size())) {
    symbolShardMap_[symbol] = static_cast<size_t>(shardId);
  }
}

size_t Exchange::getShardId(std::string_view symbol) const {
  auto it = symbolShardMap_.find(symbol);
  if (it != symbolShardMap_.end()) {
    return it->second;
  }
  return std::hash<std::string_view>{}(symbol) % shards_.size();
}

namespace {
static constexpr size_t PRODUCER_BATCH_SIZE = 64;
struct CommandBatch {
  std::array<Exchange::Command, PRODUCER_BATCH_SIZE> commands;
  size_t count = 0;
};
static thread_local std::vector<CommandBatch> localBatches;
}  // namespace

void Exchange::flush() {
  if (localBatches.empty()) return;

  for (size_t i = 0; i < localBatches.size(); ++i) {
    if (localBatches[i].count > 0 && i < shards_.size()) {
      while (!shards_[i]->queue.push_batch(localBatches[i].commands.data(),
                                           localBatches[i].count)) {
        std::this_thread::yield();
      }
      localBatches[i].count = 0;
    }
  }
}

void Exchange::submitOrder(const Order &order, int shardHint,
                           std::chrono::nanoseconds *wait_duration) {
  size_t shardId =
      (shardHint >= 0 && shardHint < static_cast<int>(shards_.size()))
          ? shardHint
          : getShardId(order.symbol);

  if (localBatches.size() != shards_.size()) {
    localBatches.resize(shards_.size());
  }

  auto &batch = localBatches[shardId];
  Command &cmd = batch.commands[batch.count++];
  cmd.type = Command::Add;
  cmd.order = order;
  cmd.cancelId = 0;
  std::copy_n(order.symbol, 8, cmd.symbol.begin());

  if (wait_duration) {
    *wait_duration = std::chrono::nanoseconds(0);
  }

  if (batch.count == PRODUCER_BATCH_SIZE) {
    if (wait_duration) {
      auto start = std::chrono::steady_clock::now();
      while (!shards_[shardId]->queue.push_batch(batch.commands.data(),
                                                 batch.count)) {
        std::this_thread::yield();
      }
      auto end = std::chrono::steady_clock::now();
      *wait_duration = (end - start);
    } else {
      while (!shards_[shardId]->queue.push_batch(batch.commands.data(),
                                                 batch.count)) {
        std::this_thread::yield();
      }
    }
    batch.count = 0;
  }
}

void Exchange::cancelOrder(const std::string &symbol, OrderId orderId) {
  size_t shardId = getShardId(symbol);

  if (localBatches.size() != shards_.size()) {
    localBatches.resize(shards_.size());
  }

  auto &batch = localBatches[shardId];
  Command &cmd = batch.commands[batch.count++];

  cmd.type = Command::Cancel;
  cmd.cancelId = orderId;
  cmd.symbol.fill(0);
  std::copy_n(symbol.c_str(), std::min<size_t>(symbol.size(), 8),
              cmd.symbol.begin());

  if (batch.count == PRODUCER_BATCH_SIZE) {
    while (!shards_[shardId]->queue.push_batch(batch.commands.data(),
                                               batch.count)) {
      std::this_thread::yield();
    }
    batch.count = 0;
  }
}

void Exchange::setTradeCallback(TradeCallback cb) { onTrade_ = std::move(cb); }

#ifdef __linux__
#include <pthread.h>
#include <sched.h>
#elif defined(__APPLE__)
#include <mach/mach.h>
#include <mach/thread_policy.h>
#endif

void Exchange::pinThread(int coreId) {
#ifdef __linux__
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(coreId, &cpuset);
  pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
#elif defined(__APPLE__)
  thread_affinity_policy_data_t policy = {static_cast<integer_t>(coreId + 1)};
  thread_policy_set(mach_thread_self(), THREAD_AFFINITY_POLICY,
                    reinterpret_cast<thread_policy_t>(&policy),
                    THREAD_AFFINITY_POLICY_COUNT);
#endif
}

void Exchange::workerLoop(int shardId) {
  pinThread(shardId);

  auto &shard = *shards_[shardId];

  OrderBook *lastBook = nullptr;
  std::array<char, 8> lastSymbol{};
  lastSymbol.fill(0);

  const size_t BATCH_SIZE = 64;
  std::array<Command, BATCH_SIZE> cmdBuffer;

  while (true) {
    size_t count = shard.queue.pop_batch(cmdBuffer.data(), BATCH_SIZE);

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

      if (lastBook && cmd.symbol == lastSymbol) {
        book = lastBook;
      } else {
        size_t len = 0;
        while (len < 8 && cmd.symbol[len] != '\0') len++;
        std::string_view symView(cmd.symbol.data(), len);

        auto it = shard.books.find(symView);
        if (it == shard.books.end()) {
          it = shard.books
                   .emplace(std::string(symView), std::make_unique<OrderBook>())
                   .first;
        }
        book = it->second.get();
        lastBook = book;
        lastSymbol = cmd.symbol;
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
    std::cout << "Symbol: " << symbol << "\n";
    shard.books.at(symbol)->printBook();
  } else {
    std::cout << "OrderBook for " << symbol << " not found.\n";
  }
}

void Exchange::printAllOrderBooks() const {
  for (const auto &shard : shards_) {
    for (const auto &pair : shard->books) {
      std::cout << "Symbol: " << pair.first << "\n";
      pair.second->printBook();
      std::cout << "\n";
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
