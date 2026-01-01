# High-Performance Order Matching Engine

A high-frequency trading (HFT) grade Limit Order Book (LOB) and Matching Engine written in C++20. Designed for extreme throughput, low latency, and memory safety using modern lock-free techniques.

> **Performance Benchmark**: **~27,700,000 orders/second** on a 10-core machine (Apple M1 class).

## 🚀 Key Features

*   **Ultra-High Throughput**: Capable of processing over **27 million** distinct order operations per second.
*   **Lock-Free Architecture**: Uses a custom **SPSC (Single-Producer Single-Consumer) Ring Buffer** with shadow indices to eliminate mutex contention and atomic cache thrashing.
*   **Zero-Allocation Design**:
    *   **Memory Pool**: Pre-allocated object pool (15M+ slots) eliminates `malloc/free` calls during the trading loop.
    *   **Intrusive Linked Lists**: Uses index-based chaining (`int32_t next`) instead of standard container pointers.
*   **Cache Optimizations**:
    *   **Flat OrderBook**: Replaced `std::map` with `std::vector` for **O(1)** price level access.
    *   **Bitset Scanning**: Uses CPU intrinsics (`__builtin_ctzll`) to skip empty price levels instantly.
    *   **POD Enforce**: Order objects are Plain Old Data (POD) with fixed-size `char` arrays, enabling fast `memcpy` operations.
*   **Precision Safety**: All prices are fixed-point `int64_t` to eliminate floating-point precision errors.

## 🏗 Architecture

The system avoids the traditional "Global Lock" bottleneck by adopting a **Shard-per-Core** architecture combined with **Lock-Free Queues**:

1.  **Exchange**: Distributes orders to shards based on symbol.
2.  **Ring Buffer**: A cache-line aligned, lock-free SPSC queue acts as the highway between the producer (Net/Benchmark) and the consumer (Matcher).
3.  **OrderBook (Flat w/ Bitset)**: 
    *   `bids[price]` -> Head Index of the Memory Pool.
    *   `bidMask` -> Bitmap of active levels for O(1) iteration.
4.  **Memory Pool**: A monolithic `std::vector<OrderNode>` stores all orders contiguously, improving CPU cache locality.

## 🛠 Build & Run

### Prerequisites
*   C++20 Compiler (GCC 10+ / Clang 12+)
*   CMake 3.14+

### Compiling
```bash
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS="-march=native"
make -j$(nproc)
```

### Running Benchmarks
To verify the 27M+ ops/s performance on your machine:
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

The project includes a comprehensive GoogleTest suite.

```bash
./build/tests/unit_tests
```
