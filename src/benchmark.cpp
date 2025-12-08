
#include <chrono>
#include <iostream>
#include <random>
#include <thread>
#include <vector>

#include "MatchingEngine.hpp"

void benchmarkWorker(MatchingEngine &engine, int numOrders, int threadId) {
  std::mt19937 gen(threadId);
  std::uniform_real_distribution<> priceDist(100.0, 200.0);
  std::uniform_int_distribution<> qtyDist(1, 100);
  std::uniform_int_distribution<> typeDist(0, 1);  // 0: Buy, 1: Sell

  for (int i = 0; i < numOrders; ++i) {
    OrderType type = (typeDist(gen) == 0) ? OrderType::Buy : OrderType::Sell;
    double price = priceDist(gen);
    double qty = qtyDist(gen);

    // Use threadId * numOrders + i as ID to ensure uniqueness
    OrderId id = (long)threadId * numOrders + i + 1;

    engine.submitOrder(
        Order(id, "BTC-USD", type, OrderKind::Limit, price, qty));
  }
}

int main() {
  MatchingEngine engine;
  int numThreads = std::thread::hardware_concurrency();
  int ordersPerThread = 100000;
  int totalOrders = numThreads * ordersPerThread;

  std::cout << "Starting benchmark with " << numThreads << " threads..."
            << std::endl;
  std::cout << "Total orders: " << totalOrders << std::endl;

  auto start = std::chrono::high_resolution_clock::now();

  std::vector<std::thread> threads;
  for (int i = 0; i < numThreads; ++i) {
    threads.emplace_back(benchmarkWorker, std::ref(engine), ordersPerThread, i);
  }

  for (auto &t : threads) {
    t.join();
  }

  auto end = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double> diff = end - start;

  std::cout << "Benchmark completed in " << diff.count() << " seconds."
            << std::endl;
  std::cout << "Throughput: " << (totalOrders / diff.count())
            << " orders/second" << std::endl;

  return 0;
}
