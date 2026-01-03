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
  }

  for (int i = 0; i < numWorkers; ++i) {
    workers_.emplace_back(&Exchange::workerLoop, this, i);
  }
}

Exchange::~Exchange() { stop(); }

void Exchange::stop() {
  flush();
  for (auto &shard : shards_) {
    Command stopCmd;
    stopCmd.type = Command::Stop;
    shard->queue.push_block(stopCmd);
  }
  workers_.clear();
}

int32_t Exchange::registerSymbol(const std::string &symbol, int shardId) {
  if (symbolNameToId_.find(symbol) != symbolNameToId_.end()) {
    return symbolNameToId_[symbol];
  }

  int32_t symbolId = static_cast<int32_t>(symbolIdToName_.size());
  symbolIdToName_.push_back(symbol);
  symbolNameToId_[symbol] = symbolId;

  if (shardId < 0 || shardId >= static_cast<int>(shards_.size())) {
    shardId = symbolId % shards_.size();
  }
  symbolIdToShardId_.push_back(shardId);

  auto &shard = *shards_[shardId];
  if (shard.books.size() <= static_cast<size_t>(symbolId)) {
    shard.books.resize(symbolId + 1);
  }
  shard.books[symbolId] = std::make_unique<OrderBook>();

  return symbolId;
}

std::string Exchange::getSymbolName(int32_t symbolId) const {
  if (symbolId >= 0 &&
      symbolId < static_cast<int32_t>(symbolIdToName_.size())) {
    return symbolIdToName_[symbolId];
  }
  return "UNKNOWN";
}

namespace {
static constexpr size_t PRODUCER_BATCH_SIZE = 256;
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
  int32_t symbolId = order.symbolId;
  size_t shardId;

  if (shardHint >= 0 && shardHint < static_cast<int>(shards_.size())) {
    shardId = shardHint;
  } else if (symbolId >= 0 &&
             symbolId < static_cast<int32_t>(symbolIdToShardId_.size())) {
    shardId = symbolIdToShardId_[symbolId];
  } else {
    return;
  }

  if (localBatches.size() != shards_.size()) {
    localBatches.resize(shards_.size());
  }

  auto &batch = localBatches[shardId];
  Command &cmd = batch.commands[batch.count++];
  cmd.type = Command::Add;
  cmd.add.order = order;

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

void Exchange::submitOrders(const std::vector<Order> &orders, int shardHint) {
  if (localBatches.size() != shards_.size()) {
    localBatches.resize(shards_.size());
  }

  for (const auto &order : orders) {
    int32_t symbolId = order.symbolId;
    size_t shardId;

    if (shardHint >= 0 && shardHint < static_cast<int>(shards_.size())) {
      shardId = shardHint;
    } else if (symbolId >= 0 &&
               symbolId < static_cast<int32_t>(symbolIdToShardId_.size())) {
      shardId = symbolIdToShardId_[symbolId];
    } else {
      continue;
    }

    auto &batch = localBatches[shardId];
    Command &cmd = batch.commands[batch.count++];
    cmd.type = Command::Add;
    cmd.add.order = order;

    if (batch.count == PRODUCER_BATCH_SIZE) {
      while (!shards_[shardId]->queue.push_batch(batch.commands.data(),
                                                 batch.count)) {
        std::this_thread::yield();
      }
      batch.count = 0;
    }
  }
}

void Exchange::cancelOrder(int32_t symbolId, OrderId orderId) {
  size_t shardId;
  if (symbolId >= 0 &&
      symbolId < static_cast<int32_t>(symbolIdToShardId_.size())) {
    shardId = symbolIdToShardId_[symbolId];
  } else {
    return;
  }

  if (localBatches.size() != shards_.size()) {
    localBatches.resize(shards_.size());
  }

  auto &batch = localBatches[shardId];
  Command &cmd = batch.commands[batch.count++];

  cmd.type = Command::Cancel;
  cmd.cancel.orderId = orderId;
  cmd.cancel.symbolId = symbolId;

  if (batch.count == PRODUCER_BATCH_SIZE) {
    while (!shards_[shardId]->queue.push_batch(batch.commands.data(),
                                               batch.count)) {
      std::this_thread::yield();
    }
    batch.count = 0;
  }
}

void Exchange::reset() {
  for (auto &shard : shards_) {
    Command cmd;
    cmd.type = Command::Reset;
    shard->queue.push_block(cmd);
  }
  std::this_thread::sleep_for(std::chrono::milliseconds(20));
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

  const size_t BATCH_SIZE = 256;
  std::array<Command, BATCH_SIZE> cmdBuffer;

  while (true) {
    size_t count = shard.queue.pop_batch(cmdBuffer.data(), BATCH_SIZE);

    if (count == 0) {
      std::this_thread::yield();
      continue;
    }

    for (size_t i = 0; i < count; ++i) {
      auto &cmd = cmdBuffer[i];

      if (cmd.type == Command::Stop) {
        if (!shard.tradeBuffer.empty() && onTrade_) {
          onTrade_(shard.tradeBuffer);
        }
        return;
      }

      if (cmd.type == Command::Type::Add) {
        int32_t symId = cmd.add.order.symbolId;
        if (symId >= shard.books.size() || !shard.books[symId]) {
          continue;
        }
        OrderBook *book = shard.books[symId].get();
        shard.matchingStrategy.match(*book, cmd.add.order, shard.tradeBuffer);
      } else if (cmd.type == Command::Type::Cancel) {
        int32_t symId = cmd.cancel.symbolId;
        if (symId >= shard.books.size() || !shard.books[symId]) {
          continue;
        }
        OrderBook *book = shard.books[symId].get();
        book->cancelOrder(cmd.cancel.orderId, symId);
      } else if (cmd.type == Command::Type::Reset) {
        for (auto &b : shard.books) {
          if (b) b->reset();
        }
      }
    }

    if (!shard.tradeBuffer.empty()) {
      if (onTrade_) {
        onTrade_(shard.tradeBuffer);
      }
      shard.tradeBuffer.clear();
    }
  }
}

void Exchange::printOrderBook(int32_t symbolId) const {
  if (symbolId < 0 ||
      symbolId >= static_cast<int32_t>(symbolIdToShardId_.size()))
    return;

  size_t shardId = symbolIdToShardId_[symbolId];
  const auto &shard = *shards_[shardId];

  if (static_cast<size_t>(symbolId) < shard.books.size() &&
      shard.books[symbolId]) {
    std::cout << "Symbol ID: " << symbolId << " (" << getSymbolName(symbolId)
              << ")\n";
    shard.books[symbolId]->printBook();
  } else {
    std::cout << "OrderBook for Symbol ID " << symbolId << " not found.\n";
  }
}

void Exchange::printAllOrderBooks() const {
  for (size_t sid = 0; sid < symbolIdToName_.size(); ++sid) {
    printOrderBook(static_cast<int32_t>(sid));
  }
}

const OrderBook *Exchange::getOrderBook(int32_t symbolId) const {
  if (symbolId < 0 ||
      symbolId >= static_cast<int32_t>(symbolIdToShardId_.size()))
    return nullptr;

  size_t shardId = symbolIdToShardId_[symbolId];
  const auto &shard = *shards_[shardId];

  if (static_cast<size_t>(symbolId) < shard.books.size()) {
    return shard.books[symbolId].get();
  }
  return nullptr;
}
