#include <algorithm>
#include <atomic>
#include <chrono>
#include <iostream>
#include <memory>
#include <numeric>
#include <random>
#include <thread>
#include <vector>

#include "Exchange.hpp"

namespace {
static std::unique_ptr<std::atomic<int64_t>[]> submissionTimes;
static std::vector<long long> latencies;
static std::atomic<size_t> latencyIndex{0};
static bool measureLatency = false;

void pinThreadWithOffset(int threadId) {
  int offset = static_cast<int>(std::thread::hardware_concurrency()) / 2;
  Exchange::pinThread(threadId + offset);
}

void benchmarkWorker(Exchange &engine, const std::vector<Order> &orders,
                     int threadId, int iterations,
                     std::chrono::nanoseconds *totalWait = nullptr) {
  pinThreadWithOffset(threadId);

  std::chrono::nanoseconds localWait{0};

  std::vector<Order> batch;
  batch.reserve(256);

  for (int i = 0; i < iterations; ++i) {
    for (const auto &order : orders) {
      if (measureLatency) {
        submissionTimes[order.id].store(
            std::chrono::steady_clock::now().time_since_epoch().count(),
            std::memory_order_relaxed);
      }

      batch.push_back(order);
      if (batch.size() == 256) {
        engine.submitOrders(batch, threadId);
        batch.clear();
      }
    }
  }

  if (!batch.empty()) {
    engine.submitOrders(batch, threadId);
  }

  engine.flush();

  if (totalWait) {
    *totalWait = localWait;
  }
}

void runVerification() {
  std::cout << "\n=== Running Deterministic Verification Mode ===\n";

  int numThreads = 2;
  Exchange engine(numThreads);

  std::atomic<long long> totalTrades{0};
  std::atomic<Quantity> totalVolume{0};

  engine.setTradeCallback([&](const std::vector<Trade> &trades) {
    totalTrades.fetch_add(static_cast<long long>(trades.size()),
                          std::memory_order_relaxed);
    for (const auto &t : trades) {
      totalVolume.fetch_add(t.quantity, std::memory_order_relaxed);
    }
  });

  const int ORDER_COUNT = 100000;
  const Price PRICE = 100;
  const std::string SYMBOL = "VERIFY";

  int32_t symbolId = engine.registerSymbol(SYMBOL, 0);

  std::vector<Order> buyOrders;
  std::vector<Order> sellOrders;
  buyOrders.reserve(ORDER_COUNT);
  sellOrders.reserve(ORDER_COUNT);

  for (int i = 0; i < ORDER_COUNT; ++i) {
    buyOrders.emplace_back((OrderId)(i + 1), 0, symbolId, OrderSide::Buy,
                           OrderType::Limit, PRICE, 1);
    sellOrders.emplace_back((OrderId)(i + 1 + ORDER_COUNT), 0, symbolId,
                            OrderSide::Sell, OrderType::Limit, PRICE, 1);
  }

  std::cout << "Submitting " << ORDER_COUNT << " BUY orders...\n";
  benchmarkWorker(engine, buyOrders, 0, 1);

  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  std::cout << "Submitting " << ORDER_COUNT << " SELL orders...\n";
  benchmarkWorker(engine, sellOrders, 0, 1);
  std::cout << "Waiting for matching...\n";
  std::this_thread::sleep_for(std::chrono::seconds(2));

  long long trades = totalTrades.load();
  long long vol = totalVolume.load();

  std::cout << "Verification Results:\n";
  std::cout << "  Expected Trades: " << ORDER_COUNT << "\n";
  std::cout << "  Actual Trades:   " << trades << "\n";
  std::cout << "  Expected Volume: " << ORDER_COUNT << "\n";
  std::cout << "  Actual Volume:   " << vol << "\n";

  if (trades == ORDER_COUNT && vol == ORDER_COUNT) {
    std::cout << "[PASS] Verification Successful!\n";
  } else {
    std::cout << "[FAIL] Verification Failed!\n";
    exit(1);
  }
}
}  // namespace

int main(int argc, char *argv[]) {
  bool verifyMode = false;
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "--latency" || arg == "-l") {
      measureLatency = true;
    }
    if (arg == "--verify" || arg == "-v") {
      verifyMode = true;
    }
  }

  if (verifyMode) {
    runVerification();
    return 0;
  }

  int totalCores = static_cast<int>(std::thread::hardware_concurrency());
  int numThreads = totalCores / 2;
  if (numThreads < 1) numThreads = 1;

  long long ordersPerThread = 10000000;
  long long poolSize = 200000;
  long long iterations = ordersPerThread / poolSize;

  if (measureLatency) {
    poolSize = ordersPerThread;
    iterations = 1;
  }

  long long totalOrders = static_cast<long long>(numThreads) * ordersPerThread;

  std::cout << "Running Warmup Phase with " << numThreads << " threads...\n";
  {
    Exchange warmupEngine(numThreads);
    std::vector<std::jthread> threads;
    threads.reserve(numThreads);
    for (int i = 0; i < numThreads; ++i) {
      threads.emplace_back([&warmupEngine, id = i]() {
        int32_t warmupSymId = warmupEngine.registerSymbol("WARMUP", 0);
        for (int j = 0; j < 100000; ++j) {
          warmupEngine.submitOrder({(OrderId)j, 0, warmupSymId, OrderSide::Buy,
                                    OrderType::Limit, 100, 1},
                                   id);
        }
      });
    }
  }
  std::cout << "Warmup complete.\n";

  std::cout << "Preparing benchmark with " << numThreads << " threads...\n";
  std::cout << "Pool Size: " << poolSize << " orders (x" << iterations
            << " iterations)\n";

  if (measureLatency) {
    std::cout << "Latency measurement ENABLED (expect lower throughput).\n";
  } else {
    std::cout << "Latency measurement DISABLED (max throughput).\n";
  }

  std::cout << "Pre-generating orders...\n";

  if (measureLatency) {
    submissionTimes = std::make_unique<std::atomic<int64_t>[]>(
        static_cast<size_t>(totalOrders + 1));
    latencies.resize(static_cast<size_t>(totalOrders));
  }

  std::vector<std::vector<Order>> threadOrders(numThreads);

  for (int i = 0; i < numThreads; ++i) {
    threadOrders[i].reserve(static_cast<size_t>(poolSize));
    std::mt19937 gen(i);
    std::uniform_int_distribution<long long> priceDist(10000, 20000);
    std::uniform_int_distribution<> qtyDist(1, 100);
    std::uniform_int_distribution<> typeDist(0, 1);

    for (long long j = 0; j < poolSize; ++j) {
      OrderSide side = (typeDist(gen) == 0) ? OrderSide::Buy : OrderSide::Sell;
      Price price = static_cast<Price>(priceDist(gen));

      Quantity qty = static_cast<Quantity>(qtyDist(gen));
      OrderId id = static_cast<OrderId>((i * ordersPerThread) + j + 1);
      int32_t symbolId = i % 10;

      threadOrders[i].emplace_back(id, 0, symbolId, side, OrderType::Limit,
                                   price, qty);
    }
  }

  std::cout << "Starting benchmark (10 runs)....\n";

  std::vector<long long> throughputs;
  std::vector<double> durations;
  throughputs.reserve(10);
  durations.reserve(10);

  for (int run = 0; run < 10; ++run) {
    auto start = std::chrono::steady_clock::now();
    std::vector<std::chrono::nanoseconds> threadWaits(numThreads);

    {
      Exchange engine(numThreads);

      for (int s = 0; s < 10; ++s) {
        engine.registerSymbol("SYM-" + std::to_string(s), -1);
      }

      if (measureLatency) {
        latencyIndex = 0;
        engine.setTradeCallback([](const std::vector<Trade> &trades) {
          long long now =
              std::chrono::steady_clock::now().time_since_epoch().count();
          for (const auto &trade : trades) {
            long long submitTime = submissionTimes[trade.takerOrderId].load(
                std::memory_order_relaxed);
            if (submitTime > 0) {
              size_t idx = latencyIndex.fetch_add(1, std::memory_order_relaxed);
              if (idx < latencies.size()) {
                latencies[idx] = now - submitTime;
              }
            }
          }
        });
      }

      std::vector<std::jthread> threads;
      threads.reserve(numThreads);

      for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back(benchmarkWorker, std::ref(engine),
                             std::cref(threadOrders[i]), i, iterations,
                             &threadWaits[i]);
      }
    }

    auto end = std::chrono::steady_clock::now();
    std::chrono::duration<double> diff = end - start;
    long long tput =
        static_cast<long long>(static_cast<double>(totalOrders) / diff.count());

    std::chrono::nanoseconds totalWaitSum{0};
    for (const auto &w : threadWaits) totalWaitSum += w;
    double avgWaitNs = static_cast<double>(totalWaitSum.count()) /
                       static_cast<double>(totalOrders);

    durations.push_back(diff.count());
    throughputs.push_back(tput);

    std::cout << "Run " << (run + 1) << ": " << diff.count()
              << " seconds. Throughput: " << tput << " orders/second\n";
    std::cout << "  Backpressure (Wait): Total=" << totalWaitSum.count()
              << "ns Avg=" << avgWaitNs << "ns/order\n";

    if (measureLatency) {
      size_t numLatencies =
          std::min((size_t)latencyIndex.load(), latencies.size());
      if (numLatencies > 0) {
        std::sort(
            latencies.begin(),
            latencies.begin() + static_cast<std::ptrdiff_t>(numLatencies));
        long long p50 = latencies[static_cast<size_t>(
            static_cast<double>(numLatencies) * 0.50)];
        long long p99 = latencies[static_cast<size_t>(
            static_cast<double>(numLatencies) * 0.99)];
        long long maxLat = latencies[numLatencies - 1];
        long long avgLat =
            std::reduce(
                latencies.begin(),
                latencies.begin() + static_cast<std::ptrdiff_t>(numLatencies),
                0LL) /
            static_cast<long long>(numLatencies);
        std::cout << "  Latency (ns): Avg=" << avgLat << " P50=" << p50
                  << " P99=" << p99 << " Max=" << maxLat << "\n";
      } else {
        std::cout << "  No trades recorded (latencies).\n";
      }
    }
  }

  long long minTput = throughputs[0];
  long long maxTput = throughputs[0];
  long long sumTput = 0;

  for (long long t : throughputs) {
    if (t < minTput) minTput = t;
    if (t > maxTput) maxTput = t;
    sumTput += t;
  }
  long long avgTput = sumTput / static_cast<long long>(throughputs.size());

  std::cout << "\n--- Benchmark Summary (10 Runs) ---\n";
  std::cout << "Average Throughput: " << avgTput << " orders/second\n";
  std::cout << "Minimum Throughput: " << minTput << " orders/second\n";
  std::cout << "Maximum Throughput: " << maxTput << " orders/second\n";

  return 0;
}
