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

std::unique_ptr<std::atomic<int64_t>[]> submissionTimes;
std::vector<long long> latencies;
std::atomic<size_t> latencyIndex{0};
bool measureLatency = false;

void benchmarkWorker(Exchange &engine, const std::vector<Order> &orders,
                     int threadId) {
  for (const auto &order : orders) {
    if (measureLatency) {
      submissionTimes[order.id].store(
          std::chrono::steady_clock::now().time_since_epoch().count(),
          std::memory_order_relaxed);
    }
    engine.submitOrder(order, threadId);
  }
}

void runVerification() {
  std::cout << "\n=== Running Deterministic Verification Mode ===\n";

  int numThreads = 2;
  Exchange engine(numThreads);

  std::atomic<long long> totalTrades{0};
  std::atomic<Quantity> totalVolume{0};

  engine.setTradeCallback([&](const std::vector<Trade> &trades) {
    totalTrades.fetch_add(trades.size(), std::memory_order_relaxed);
    for (const auto &t : trades) {
      totalVolume.fetch_add(t.quantity, std::memory_order_relaxed);
    }
  });

  const int ORDER_COUNT = 100000;
  const Price PRICE = 100;
  const std::string SYMBOL = "VERIFY";

  std::vector<Order> buyOrders, sellOrders;
  buyOrders.reserve(ORDER_COUNT);
  sellOrders.reserve(ORDER_COUNT);

  for (int i = 0; i < ORDER_COUNT; ++i) {
    buyOrders.emplace_back((OrderId)(i + 1), 0, SYMBOL, OrderSide::Buy,
                           OrderType::Limit, PRICE, 1);
    sellOrders.emplace_back((OrderId)(i + 1 + ORDER_COUNT), 0, SYMBOL,
                            OrderSide::Sell, OrderType::Limit, PRICE, 1);
  }

  std::cout << "Submitting " << ORDER_COUNT << " BUY orders..." << std::endl;
  benchmarkWorker(engine, buyOrders, 0);  // Thread 0

  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  std::cout << "Submitting " << ORDER_COUNT << " SELL orders..." << std::endl;
  benchmarkWorker(engine, sellOrders, 0);
  std::cout << "Waiting for matching..." << std::endl;
  std::this_thread::sleep_for(std::chrono::seconds(2));

  long long trades = totalTrades.load();
  long long vol = totalVolume.load();

  std::cout << "Verification Results:" << std::endl;
  std::cout << "  Expected Trades: " << ORDER_COUNT << std::endl;
  std::cout << "  Actual Trades:   " << trades << std::endl;
  std::cout << "  Expected Volume: " << ORDER_COUNT << std::endl;
  std::cout << "  Actual Volume:   " << vol << std::endl;

  if (trades == ORDER_COUNT && vol == ORDER_COUNT) {
    std::cout << "[PASS] Verification Successful!" << std::endl;
  } else {
    std::cout << "[FAIL] Verification Failed!" << std::endl;
    exit(1);
  }
}

int main(int argc, char *argv[]) {
  // Check for latency flag
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

  int totalCores = std::thread::hardware_concurrency();
  int numThreads = totalCores / 2;
  if (numThreads < 1) numThreads = 1;

  long long ordersPerThread = 10000000;
  long long totalOrders = numThreads * ordersPerThread;

  std::cout << "Running Warmup Phase with " << numThreads << " threads..."
            << std::endl;
  {
    Exchange warmupEngine(numThreads);
    std::vector<std::jthread> threads;
    for (int i = 0; i < numThreads; ++i) {
      threads.emplace_back([&warmupEngine, id = i]() {
        for (int j = 0; j < 100000; ++j) {
          warmupEngine.submitOrder({(OrderId)j, 0, "WARMUP", OrderSide::Buy,
                                    OrderType::Limit, 100, 1},
                                   id);
        }
      });
    }
  }
  std::cout << "Warmup complete." << std::endl;

  std::cout << "Preparing benchmark with " << numThreads << " threads..."
            << std::endl;

  if (measureLatency) {
    std::cout << "Latency measurement ENABLED (expect lower throughput)."
              << std::endl;
  } else {
    std::cout << "Latency measurement DISABLED (max throughput)." << std::endl;
  }

  std::cout << "Pre-generating " << totalOrders << " orders..." << std::endl;

  if (measureLatency) {
    submissionTimes = std::make_unique<std::atomic<int64_t>[]>(totalOrders + 1);
    latencies.resize(totalOrders);
  }

  std::vector<std::vector<Order>> threadOrders(numThreads);

  for (long long i = 0; i < numThreads; ++i) {
    threadOrders[i].reserve(ordersPerThread);
    std::mt19937 gen(i);
    std::uniform_int_distribution<long long> priceDist(10000, 20000);
    std::uniform_int_distribution<> qtyDist(1, 100);
    std::uniform_int_distribution<> typeDist(0, 1);

    for (long long j = 0; j < ordersPerThread; ++j) {
      OrderSide side = (typeDist(gen) == 0) ? OrderSide::Buy : OrderSide::Sell;
      Price price = priceDist(gen);
      double qty = qtyDist(gen);
      OrderId id = (i * ordersPerThread) + j + 1;
      std::string symbol = "SYM-" + std::to_string(i % 10);

      threadOrders[i].emplace_back(id, 0, symbol, side, OrderType::Limit, price,
                                   qty);
    }
  }

  std::cout << "Starting benchmark (10 runs)..." << std::endl;

  std::vector<long long> throughputs;
  std::vector<double> durations;
  throughputs.reserve(10);
  durations.reserve(10);

  for (int run = 0; run < 10; ++run) {
    auto start = std::chrono::high_resolution_clock::now();

    {
      Exchange engine(numThreads);

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

      for (long long i = 0; i < numThreads; ++i) {
        threads.emplace_back(benchmarkWorker, std::ref(engine),
                             std::cref(threadOrders[i]), i);
      }
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> diff = end - start;
    long long tput = (long long)(totalOrders / diff.count());

    durations.push_back(diff.count());
    throughputs.push_back(tput);

    std::cout << "Run " << (run + 1) << ": " << diff.count()
              << " seconds. Throughput: " << tput << " orders/second"
              << std::endl;

    if (measureLatency) {
      size_t numLatencies =
          std::min((size_t)latencyIndex.load(), latencies.size());
      if (numLatencies > 0) {
        std::sort(latencies.begin(), latencies.begin() + numLatencies);
        long long p50 = latencies[numLatencies * 0.50];
        long long p99 = latencies[numLatencies * 0.99];
        long long maxLat = latencies[numLatencies - 1];
        long long avgLat = std::reduce(latencies.begin(),
                                       latencies.begin() + numLatencies, 0LL) /
                           numLatencies;
        std::cout << "  Latency (ns): Avg=" << avgLat << " P50=" << p50
                  << " P99=" << p99 << " Max=" << maxLat << std::endl;
      } else {
        std::cout << "  No trades recorded (latencies)." << std::endl;
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
  long long avgTput = sumTput / throughputs.size();

  std::cout << "\n--- Benchmark Summary (10 Runs) ---\n";
  std::cout << "Average Throughput: " << avgTput << " orders/second\n";
  std::cout << "Minimum Throughput: " << minTput << " orders/second\n";
  std::cout << "Maximum Throughput: " << maxTput << " orders/second\n";

  return 0;
}
