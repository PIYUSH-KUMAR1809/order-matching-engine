# High-Performance Order Matching Engine

A high-frequency trading (HFT) grade Limit Order Book (LOB) and Matching Engine written in C++20. Designed for extreme throughput, low latency, and memory safety.

> **Performance Benchmark**: **~2,200,000 orders/second** on a 10-core machine.

## 🚀 Key Features

*   **Ultra-High Throughput**: Capable of processing over 2.2 million distinct order operations per second.
*   **Sharded Concurrency Architecture**: Uses a **Lock-Free-ish** design where the order book is sharded by symbol. Each shard is pinned to a dedicated worker thread, eliminating mutex contention during matching.
*   **Memory Safe & Efficient**:
    *   **Lazy Deletion w/ Compaction**: Uses a high-performance lazy deletion strategy with automated memory compaction to prevent leaks.
    *   **Contiguous Memory**: Order pointers are indexed in a flat `std::vector` for cache-friendly O(1) lookups (vs `std::unordered_map` pointers).
*   **Precision Safety**: All prices are fixed-point `int64_t` (micros/nanos) to eliminate floating-point precision errors.
*   **Production-Ready Networking**: Includes a custom, asynchronous TCP server for order entry and market data broadcasting.

## 🏗 Architecture

The system avoids the traditional "Global Lock" bottleneck by adopting a **Shard-per-Core** architecture:

1.  **Exchange**: Acts as the router. Hashes incoming symbols (e.g., "AAPL") to a specific Shard ID.
2.  **Shards**: Each shard owns a unique subset of symbols and a dedicated `std::jthread`. It possesses an input command queue protected by a spin-lock (or mutex), but the **entire matching process is lock-free** and single-threaded within the shard.
3.  **OrderBook**: A `std::map<Price, std::deque<Order>>` structure optimized for price-time priority matching.
4.  **Compactor**: Background routines analyze memory usage and "compact" the order queues during idle cycles to maintain cache locality.

## 🛠 Build & Run

### Prerequisites
*   C++20 Compiler (GCC 10+ / Clang 12+)
*   CMake 3.14+

### Compiling
```bash
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

### Running Benchmarks
To verify the 2M+ ops/s performance on your machine:
```bash
./build/src/benchmark
```

### Running the Server
Start the engine networking layer:
```bash
./build/src/OrderMatchingEngine
```
The server listens on port **8080**.

## 📊 Client Usage (TCP)

Connect using `netcat` or the provided Python client.

**Submit Order:**
```
BUY AAPL 100 15000
> ORDER_ACCEPTED_ASYNC 1
```
*(Format: SIDE SYMBOL QTY PRICE_INT)*

**Subscribe to Market Data:**
```
SUBSCRIBE AAPL
> SUBSCRIBED AAPL
> TRADE AAPL 15000 50
```

## 🧪 Testing

The project includes a comprehensive GoogleTest suite covering:
*   Partial & Full Fills
*   Self-Trade Logic
*   Queue Priority & Fairness
*   Multi-Asset Isolation

```bash
./build/tests/unit_tests
```
