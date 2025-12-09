# Project Feedback and Improvements

## Round 1: Initial Feedback (December 2025)

### Social Media Feedback

#### 1. Vector with Tombstones vs. Linked List
- **Feedback**: "Have you considered using a vector with tombstones to mark the empty spots instead of a linked list?"
- **Context**: Currently, `OrderBook` uses `std::map<Price, std::list<Order>>`.
- **Discussion**:
    - **Current (`std::list`)**: O(1) insertion/deletion given an iterator. Pointers remain valid.
    - **Proposed (`std::vector` + tombstones)**: Better cache locality. Deletion involves marking as "dead" (tombstone). Requires periodic cleanup (compaction) to remove dead orders. Complexity of matching might increase if skipping many tombstones.

#### 2. TcpServer::start() should return bool
- **Feedback**: "Your TcpServer::start() function should probably return a bool"
- **Context**: `void TcpServer::start()` currently prints errors to `stderr` but doesn't return status.
- **Improvement**: Change return type to `bool` (or throw exception) to allow caller to handle failure (e.g., port in use).

#### 3. Two-step Initialization
- **Feedback**: "Why the two step initialization"
- **Context**: `TcpServer` constructor initializes members, but `start()` creates/binds socket.
- **Discussion**: RAII principles suggest fully initializing resource in constructor. However, `start()` might block or fail, which some prefer to handle outside the constructor.

#### 4. Use std::jthread
- **Feedback**: "Consider using std::jthread"
- **Context**: `TcpServer` uses `std::thread` and manually joins in `stop()`/destructor.
- **Improvement**: `std::jthread` (C++20) automatically joins on destruction, simplifying `TcpServer` destructor and `stop()` logic.

#### 5. Concurrency Architecture (Lock-free / Sharding)
- **Feedback**: "Don't communicate by sharing, share by communicating." "Mutex is dang slow... pin a book to a thread."
- **Context**: Current implementation uses `std::shared_mutex` to protect the `OrderBook`.
- **Discussion**:
    - **Current**: Thread-safe via mutex. Simple but suffers from contention.
    - **Proposed**:
        - **Pinning**: Assign specific symbols (OrderBooks) to specific threads.
        - **MPSC Queues**: Use lock-free Multi-Producer Single-Consumer queues to route orders to the correct thread.
        - **Benefits**: Eliminates mutex contention, improves cache locality.

#### 6. Memory Allocations & Data Structures
- **Feedback**: "Allocations, in particular. You don't want allocations." Suggests ditching `std::list` and `std::map` default allocators.
- **Context**: `std::map<Price, std::list<Order>>` performs many small allocations.
- **Discussion**:
    - **Allocators**: Use `std::pmr::polymorphic_allocator` or `boost::pool_allocator`.
    - **Containers**:
        - `boost::unordered_flat_map` for O(1) order lookups (better cache locality than `std::unordered_map`).
        - B-Trees (Boost) or flat maps instead of node-based `std::map`.
    - **Key Redesign**: Instead of `std::list` for time priority, use `std::map<std::tuple<Price, Time>, Order>`. This simplifies the structure but might increase tree depth.

#### 7. Latency vs. Fairness/Throughput
- **Feedback**: "Exchange doesn't have to be low-latency... it's got to have good throughput... and guarantee a level playing field."
- **Context**: General architectural philosophy.
- **Takeaway**: Prioritize consistent ordering and throughput (processing capacity) over pure microsecond latency. Ensuring deterministic execution order (fairness) is paramount.

#### 8. Naming Conventions
- **Feedback**: "Rename OrderType to OrderSide, and OrderKind to possibly PriceType." "Add a client order ID."
- **Context**: Current naming is `OrderType` (Buy/Sell) and `OrderKind` (Limit).
- **Improvement**:
    - `OrderType` -> `OrderSide` (Buy/Sell).
    - `OrderKind` -> `OrderType` or `TimeInForce` (Limit, Market, FOK, GTC, etc.).
    - Add `ClientOrderID` to map execution reports back to client's internal ID.

#### 9. Architectural Separation (Container vs. Logic)
- **Feedback**: "OrderBook class should focus on just being an efficient container... Matching logic itself will become increasingly sophisticated... move that out."
- **Context**: `OrderBook::match()` currently contains the matching logic.
- **Improvement**:
    - **Refactor**: Make `OrderBook` a dumb container.
    - **New Component**: Create a `MatchingStrategy` or `AuctionMechanism` that operates on the book.
    - **Renaming**: `MatchingEngine` -> `Exchange` (manages multiple books).
    - **Domain Objects**: Separate `OrderInstruction` (incoming request) from `RestingOrder` (internal representation).

### Planned Improvements (Round 1)

Here is the step-by-step technical plan to address the feedback.

#### Phase 1: Cleanups & Standards (Quick Wins)
*Focus: Naming conventions, modern C++ features, and basic error handling.*

1.  **Refine Naming & Types**
    *   **Action**: Rename `OrderType` enum to `OrderSide`.
        *   *Values*: `Buy`, `Sell`.
    *   **Action**: Rename `OrderKind` enum to `OrderType`.
        *   *Values*: `Limit`, `Market`, `FOK`, `IOC`, etc.
    *   **Action**: Add `ClientOrderID` to the `Order` struct.
        *   *Type*: `std::string` (or `uint64_t` for performance, but string is easier for now).
        *   *Purpose*: Allow clients to track their specific orders.

2.  **Modernize Threading**
    *   **Action**: In `TcpServer`, replace `std::vector<std::thread> clientThreads_` with `std::vector<std::jthread>`.
    *   **Benefit**: `jthread` automatically joins on destruction. You can remove the manual `join()` loops in `TcpServer::stop()` and `~TcpServer()`.

3.  **Improve Error Handling**
    *   **Action**: Change `void TcpServer::start()` to `bool TcpServer::start()`.
    *   **Detail**: Return `false` if `bind()` or `listen()` fails, instead of just printing to stderr. This allows `main.cpp` to exit gracefully.

#### Phase 2: Architectural Refactoring
*Focus: Separation of concerns. Decoupling the data (OrderBook) from the logic (Matching).*

4.  **Rename Top-Level Engine**
    *   **Action**: Rename the class `MatchingEngine` to `Exchange`.
    *   **Reasoning**: An "Exchange" contains multiple "OrderBooks" and manages them. This aligns better with domain terminology.

5.  **Extract Matching Logic**
    *   **Action**: Remove the `match()` function from the `OrderBook` class.
    *   **Action**: Create a new class/struct (e.g., `MatchingStrategy` or `ContinuousDoubleAuction`).
    *   **Logic**:
        *   Pass the `OrderBook` (or specific sides: Bids/Asks) into the matching strategy.
        *   The strategy iterates through the book and generates `Trade` objects.
    *   **Goal**: `OrderBook` becomes a dumb container with simple `add`, `remove`, `get` methods.

#### Phase 3: High-Performance Core (Deep Dive)
*Focus: Concurrency without mutexes and memory optimization.*

6.  **Concurrency: Share by Communicating**
    *   **Action**: Remove `std::shared_mutex` from `OrderBook`.
    *   **Action**: Implement a "Shard-per-Thread" model in `Exchange`.
        *   Create a fixed number of worker threads (e.g., equal to CPU cores).
        *   Assign each `Symbol` (e.g., "AAPL") to a specific thread index (`hash(Symbol) % thread_count`).
        *   Incoming orders are pushed to a thread-safe queue (initially `std::queue` with mutex, later lock-free MPSC) for that specific worker thread.
    *   **Result**: No more lock contention on the OrderBook itself. Each thread processes its assigned books linearly.

7.  **Data Structure Optimization**
    *   **Action**: Replace `std::map<Price, std::list<Order>>` with a more cache-friendly structure.
    *   **Step A**: Replace `std::list` with `std::vector`.
        *   *Challenge**: Deleting from the middle of a vector is O(N).
        *   *Solution*: Use "Tombstones". When cancelling, just mark the order as `active = false` inside the vector. Cleanup ("compact") the vector periodically or when it gets too large.
    *   **Step B (Optional)**: Replace `std::map` (Node-based) with a flat map structure (e.g., `std::vector<Level>` sorted by price).

---
## Round 2: [Date/Topic]
*(Reserved for future feedback)*
