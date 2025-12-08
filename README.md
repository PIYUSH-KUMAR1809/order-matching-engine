# High-Performance Order Matching Engine

A C++20 Limit Order Book (LOB) and Matching Engine designed for low-latency trading simulations. This project implements a thread-safe, multi-asset matching system accessible via a custom TCP server.

## Features

*   **Core Matching Logic**: Implements Price-Time priority (FIFO) matching for Limit and Market orders.
*   **Concurrency**: Thread-safe architecture using `std::shared_mutex` for high-throughput order processing.
*   **Networking**: Custom TCP server implementation using POSIX sockets (no external networking dependencies).
*   **Performance**: Optimized data structures (`std::map` for ordered price levels, `std::unordered_map` for O(1) order lookups).
*   **Visualization**: Real-time python-based dashboard for effective order book state visualization.

## Architecture

The system is built with a modular architecture:

*   **`OrderBook`**: The core data structure managing Bids and Asks. Handles the matching logic.
*   **`MatchingEngine`**: The controller layer that manages multiple OrderBooks (one per symbol) and ensures thread safety.
*   **`TcpServer`**: Handles client connections, request parsing, and response serialization.

### Data Structures

| Component | Structure | Reason |
| :--- | :--- | :--- |
| **Price Levels** | `std::map<Price, std::list<Order>>` | Keeps limits sorted for matching; `std::list` allows O(1) deletion from anywhere in the queue. |
| **Order Lookup** | `std::unordered_map<OrderId, Location>` |  Enables O(1) cancellation by mapping IDs directly to list iterators. |

## Quick Start

### Prerequisites
*   C++ Compiler with C++20 support (GCC/Clang)
*   CMake 3.14+
*   Python 3 (for dashboard/testing)

### Build
```bash
mkdir -p build && cd build
cmake ..
make
```

### Run Server
```bash
./src/OrderMatchingEngine
```
The server will start on port `8080`.

### Run Tests
```bash
./tests/unit_tests
```

## Dashboard
A Python Streamlit dashboard is included to visualize the order book in real-time.
```bash
# In a separate terminal
pip install streamlit pandas
streamlit run dashboard.py
```
