#include <chrono>
#include <iostream>
#include <random>
#include <thread>
#include <vector>

#include "Exchange.hpp"

void benchmarkWorker(Exchange &engine, const std::vector<Order> &orders) {
  for (const auto &order : orders) {
    engine.submitOrder(order);
  }
}

int main() {
  int numThreads = std::thread::hardware_concurrency();
  int ordersPerThread = 100000;
  int totalOrders = numThreads * ordersPerThread;

  std::cout << "Preparing benchmark with " << numThreads << " threads..."
            << std::endl;
  std::cout << "Pre-generating " << totalOrders << " orders..." << std::endl;

  std::vector<std::vector<Order>> threadOrders(numThreads);

  for (int i = 0; i < numThreads; ++i) {
    threadOrders[i].reserve(ordersPerThread);
    std::mt19937 gen(i);
    std::uniform_real_distribution<> priceDist(100.0, 200.0);
    std::uniform_int_distribution<> qtyDist(1, 100);
    std::uniform_int_distribution<> typeDist(0, 1);

    for (int j = 0; j < ordersPerThread; ++j) {
      OrderSide side = (typeDist(gen) == 0) ? OrderSide::Buy : OrderSide::Sell;
      double price = priceDist(gen);
      double qty = qtyDist(gen);
      OrderId id = (long)i * ordersPerThread + j + 1;
      std::string symbol = "SYM-" + std::to_string(i % 10);

      threadOrders[i].emplace_back(id, 0, symbol, side, OrderType::Limit, price,
                                   qty);
    }
  }

  std::cout << "Starting benchmark..." << std::endl;

  auto start = std::chrono::high_resolution_clock::now();

  {
    Exchange engine;
    std::vector<std::jthread> threads;
    threads.reserve(numThreads);

    for (int i = 0; i < numThreads; ++i) {
      threads.emplace_back(benchmarkWorker, std::ref(engine),
                           std::cref(threadOrders[i]));
    }
  }

  auto end = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double> diff = end - start;

  std::cout << "Benchmark completed in " << diff.count() << " seconds."
            << std::endl;
  std::cout << "Throughput: " << (totalOrders / diff.count())
            << " orders/second" << std::endl;

  return 0;
}
