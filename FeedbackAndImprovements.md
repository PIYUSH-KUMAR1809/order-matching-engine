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
## Round 2: Community Feedback (December 2025)

### 1. Optimize Order Struct & Cache Locality
- **Feedback**: "Cache locality - your order class stores a lot of information... Instead of a double you should be using fixed precision, an int32_t is fine. Storing the symbol as part of the order is wasteful. I would go so far to make the linked list implementation intrusive..."
- **Context**: `Order` struct currently has `std::string symbol` and `double price`. It is stored in a `std::list`.
- **Improvement**:
    - **Fixed Precision**: Change `Price` to `int32_t` (fixed point, e.g., cents).
    - **Remove Symbol**: The OrderBook already knows the symbol; individual orders don't need to store it if they are just nodes in the book.
    - **Intrusive List**: Embed the `next` pointer inside the `Order` struct itself to avoid `std::list` node allocation overhead and improve locality.

### 2. Contiguous Memory for Price Levels
- **Feedback**: "std::map over time in a busy book will cause fragmentation... a simple vector of [px, qty, order*] ... might be faster."
- **Context**: Currently using `std::map` for price levels.
- **Improvement**:
    - **Flat Structure**: Use `std::vector` or `std::array<Level, N>` (e.g., N=1000) where `Level` contains `{price, total_qty, head_order_ptr}`.
    - **Optimization**: Store best bid/ask at the *back* of the vector to allow O(1) push/pop as prices move.
    - **Benefit**: Fits in L1 cache (16KB for ~1000 entries), zero allocator fragmentation.

### 3. Event Loop Architecture (epoll)
- **Feedback**: "The thread per connection is the wrong abstraction... a simple single threaded ::poll/::epoll event loop will be faster... 0 lock contention."
- **Context**: Currently using `TcpServer` with one thread per client (`std::jthread`).
- **Improvement**:
    - **Single Threaded**: Use `epoll` (Linux) or `kqueue` (Mac) to handle many connections on a single thread.
    - **Serialization**: Read orders from network -> Serializing Queue -> Matching Engine.
    - **Benefit**: Deterministic ordering (crucial for fairness/WAL) and cleaner architecture without locks.

### 4. Recovery & Persistence (WAL)
- **Feedback**: "imagine if the node running this matching engine died, you need to store the state as a WAL..."
- **Context**: No persistence currently. Memory-only.
- **Improvement**:
    - **WAL (Write-Ahead Log)**: Append every incoming order/cancel to a file before processing.
    - **Snapshots**: Periodically dump the state of the OrderBooks.
    - **Replay**: On startup, load snapshot + replay WAL to restore state.

### 5. Profiling & Performance Analysis
- **Feedback**: "Next would be to profile your code and see what functions are executed the most and consume the most time."
- **Context**: Performance is critical, but optimizations without data are premature (knuth).
- **Action**:
    - **Tools**: Use `perf` (Linux) or `Instruments` (Mac) to profile the engine under load (using the benchmark tool).
    - **Goal**: Identify hotspots (e.g., `std::map` lookups, mutex contention) to guide the optimizations in points 1 & 2.

### 6. Mutex Choice (std::mutex vs std::shared_mutex)
- **Feedback**: "You might want to try a regular std::mutex and see if that's faster - std::shared_mutex ... has much higher fixed cost... Hard to know for sure based on synthetic benchmarks..."
- **Context**: Currently using `std::shared_mutex` for Read-Write locking on the OrderBook.
- **Hypothesis**: `std::shared_mutex` might be slower due to overhead if the read/write ratio isn't extremely high towards reads.
    - **Action**: Benchmark `std::mutex` vs `std::shared_mutex` under various read/write loads to see if the simpler lock performs better.

### 7. Benchmark Validity & Methodology
- **Feedback**: "I think you should rethink the logic of your benchmarks... [it] includes the order generation... pollutes the instruction cache."
- **Context**: `benchmark.cpp` currently generates random orders inside the measurement loop.
- **Action**:
    - **Pre-generation**: Generate all test orders *before* starting the timer. Store them in a vector.
    - **Clean Measure**: The benchmark loop should only measure `engine.submitOrder()`.

### 8. Allocation-Free Matching Path
- **Feedback**: "To get good latency... you need to avoid dynamic memory allocation at all costs... StandardMatchingStrategy [uses] a std::vector that you are push()ing to."
- **Context**: `MatchingStrategy::match` likely returns a `std::vector<Trade>`, causing allocation on every match.
- **Action**:
    - **Optimization**: Pass a pre-allocated buffer (or a callback/listener) to `match()` instead of returning a vector.
    - **Goal**: Zero allocations on the hot path (order submission -> matching -> trade report).

### 9. Metrics Granularity (Latency vs Throughput)
- **Feedback**: "I would keep two sets of benchmarking metrics, one for orders that don't match, and another for orders that do." "In a real world exchange latency and jitter would matter more than just throughput."
- **Context**: Currently measuring aggregate throughput.
- **Action**:
    - **Split Metrics**: Measure latency (pctiles) separately for:
        1.  **Passive Orders** (Add to book, no match).
        2.  **Aggressive Orders** (Match / Trade).
    - **Focus**: Prioritize minimizing 99th% latency and jitter over raw throughput numbers.

### 10. Sharded vs. Global Multi-threading
- **Feedback**: "Isn't multi-threaded better than single threaded... [choice] ... performs better and is highly scalable like in real life"
- **Context**: User prefers multi-threading for scalability.
- **Clarification**:
    - **Global Lock Model (Current)**: Threads fight for locks on `OrderBooks`. Scalability limits < # cores.
    - **Single-Threaded Pure**: One core, no locks. Fastest latency, limited by 1 core speed.
    - **Sharded Architecture (Best/Real-Life)**: N Single-Threaded Engines (one per core). Each engine owns a subset of symbols (e.g., Core 1: AAPL, Core 2: GOOG).
    - **Action**: Implement "Shard-per-Core" architecture. Keep multi-threading (1 thread per core), but *remove locks* by ensuring each thread has exclusive access to its shards. This is the "high scalability" solution.

---

## Round 3: HFT Optimization & Results (January 2026)

This section details the architectural evolution executed to maximize throughput on single-node hardware.

### Performance Summary

| Optimization Stage | Throughput (Orders/Sec) | Speedup | Key Bottleneck Removed |
| :--- | :--- | :--- | :--- |
| **Baseline (v1)** | ~1,000,000 | 1x | `std::mutex`, `std::condition_variable` |
| **SPSC Ring Buffer** | ~9,000,000 | 9x | Thread Context Switching (Locks) |
| **Memory Pool** | ~17,500,000 | 17.5x | `malloc` / `free` Contention |
| **POD Zero-Copy** | ~27,700,000 | 27.7x | `std::string` Copies & Memory Bandwidth |

### Detailed Improvements

#### 1. Lock-Free Inter-Thread Communication (SPSC Ring Buffer)
*   **The Problem:** The initial implementation used `std::deque` protected by `std::mutex`. Every `submitOrder` call required acquiring a lock, potentially putting the producer thread to sleep if the consumer was busy. This caused expensive OS context switches (~5-10µs).
*   **The Solution:** Implemented a custom **Single-Producer Single-Consumer (SPSC) Ring Buffer**.
    *   **Lock-Free:** Uses `std::atomic` head and tail indices with `acquire`/`release` memory semantics.
    *   **Shadow Indices:** The producer and consumer keep local copies of the indices to avoid querying the shared atomic variable (which causes cache line bouncing) unless the buffer appears full/empty.
    *   **False Sharing Prevention:** The structure is aligned to cache lines (`alignas(64)`) to ensure the head and tail live on separate cache lines.

#### 2. Flat OrderBook (O(1) Access)
*   **The Problem:** The original `OrderBook` used `std::map<Price, std::deque<Order>>`.
    *   `std::map` is a Red-Black Tree. Lookups are `O(log N)`.
    *   It creates a new heap allocation for every node.
    *   Pointer chasing kills CPU cache locality.
*   **The Solution:** Switched to **Flat Arrays** (`std::vector`).
    *   `bids[price]` allows `O(1)` instant access to any price level.
    *   Since prices in our benchmark (and many real exchanges) are integers within a known range, we can use the price directly as an array index.

#### 3. Bitset Scan for Sparse Levels
*   **The Problem:** Iterating through a flat array is fast, but if the book is sparse (e.g., bids at 100, 95, 90), the engine wastes time checking empty slots 99, 98, 97...
*   **The Solution:** Implemented a `PriceBitset` using **CPU Intrinsics**.
    *   We maintain a bitmask where `1` = active price level.
    *   Using `__builtin_ctzll` (Count Trailing Zeros), the CPU can find the next active price index in a 64-bit word in a single cycle.
    *   This allows the Matching Engine to "teleport" to the next matchable price instantly.

#### 4. Zero-Allocation Memory Pool
*   **The Problem:** `std::deque<Order>` or `std::vector<Order>` (with resize) allocates memory on the heap. `malloc` and `free` are slow and require locks inside the memory allocator, causing contention when multiple threads are running.
*   **The Solution:** **Object Pool (Intrusive Linked List)**.
    *   We pre-allocate a massive `std::vector<OrderNode>` (15M slots) at startup.
    *   Instead of `std::list`, we use integer indices (`next`) to chain orders.
    *   **Result:** The trading loop performs **Zero** heap allocations. It just reads/writes integers and structs in pre-warmed memory.

#### 5. POD (Plain Old Data) & Zero-Copy
*   **The Problem:** The `Order` struct contained `std::string symbol`.
    *   `std::string` often allocates on the heap.
    *   Copying an Order involved copying the string, which is slow.
*   **The Solution:** Replaced `std::string` with `char symbol[8]`.
    *   The `Order` struct is now a **POD** (Plain Old Data).
    *   We can copy it using `std::memcpy` or simple register moves.
    *   This massively reduced memory bandwidth usage, unlocking the jump from 17M to 27M.

---

## Future Roadmap (To break 100M)

1.  **Batching:** Process orders in groups of 16 or 32 to amortize the cost of atomic updates in the Ring Buffer.
2.  **Kernel Bypass:** Use Solarflare/DPDK to read packets directly from the NIC, skipping the OS network stack.

---

## Round 3.5: Breaking the 100M Barrier (The 230M Leap)

Between Round 3 and Round 4, throughput exploded from **~27M** to **~234M** orders/second. This 10x improvement wasn't magic—it was methodology.

### 1. Benchmark Hygiene (Pre-generation)
- **The Bottleneck**: The previous benchmark (`v1`) generated random numbers (`rand()`, distributions) *inside* the hot measure loop.
- **The Reality**: We were benchmarking the system's ability to generate random numbers, not the Matching Engine's speed.
- **The Fix**: Pre-generate 50M orders into a `std::vector<std::vector<Order>>` before starting the clock. The benchmark loop now only measures the raw `submitOrder` latency.

### 2. Thread Topology Optimization
- **The Bottleneck**: Running `numThreads = std::thread::hardware_concurrency()` (e.g., 10 threads on 10 cores) caused OS scheduler contention and "fighting" between producers and consumers if hyperthreading wasn't perfect.
- **The Fix**: `numThreads = totalCores / 2`.
    - By running exactly 1 producer per 2 logical threads (or ensuring producers + consumers <= Physical Cores), we allow the CPU's pipeline to stay full without context switching.

### 3. Hash Bypass (Producer Affinity)
- **The Bottleneck**: Even with a lock-free queue, calculating `std::hash(symbol)` for every order adds latency and CPU cycles.
- **The Fix**: Implemented **Shard Hinting**.
    - The benchmark (Acting as a Smart Gateway) knows which thread it is running on.
    - It passes `threadId` to `submitOrder(order, threadId)`.
    - The `Exchange` uses this hint to skip hashing and write directly to `shards_[threadId]`.
    - This simulates "sticky sessions" or deterministic routing often found in HFT gateways.

---

## Round 4: Latency & Verification (January 1st Week 2026)

### 1. End-To-End Latency Measurement
- **Feedback**: "Benchmark measures submission throughput only... consider adding end to end latency measurement... and order stream handling under backpressure."
- **Context**: The previous benchmark was "fire-and-forget", effectively flooding the engine to measure peak ingestion rate (~230M/s) but ignoring processing latency.
- **Improvement**:
    - **Instrumented Benchmark**: Added `--latency` flag to `src/benchmark`.
    - **Methodology**:
        1.  Record `submissionTime` (high-resolution clock) atomically for every order.
        2.  Pass this timestamp ID through the engine.
        3.  In the `onTrade` callback, capture `executionTime`.
        4.  Compute `Latency = executionTime - submissionTime`.
    - **Results**:
        - **Throughput (Instrumented)**: ~38M orders/sec (Overhead of millions of atomic/clock calls).
        - **Latency (P50)**: ~4ms.
        - **Latency (P99)**: ~400ms.
    - **Observation**: The high P99 latency confirms the engine's behavior under backpressure (queue buildup) when flooded with 50M orders.

### 2. Verified Determinism
- **Feedback**: "Execute results verification."
- **Context**: Ensuring that 10M buy orders + 10M sell orders result in exactly 10M trades.
- **Improvement**: (Planned) Deterministic verification mode to assert `Matches == min(Buys, Sells)` and `BookSize == |Buys - Sells|`.

---

## Round 5: Conceptual Clarity & QoL Improvements (January 2nd 2026)

### Conceptual Feedback
1.  **Conceptual Clarity vs. Optimization**
    - **Feedback**: "Priority is in conceptual clarity rather than performance optimization... you are just measuring submission throughput... bypassing real challenges such as load skew, order mix, worker contention."
    - **Context**: The user emphasizes that raw throughput numbers (100M/s) are less meaningful if the benchmark doesn't reflect real-world conditions (networking, tail latency, skew).
    - **Takeaway**: Shift focus from chasing numbers to ensuring the engine handles realistic scenarios (e.g., measuring latency under load, not just throughput).

2.  **Benchmark Validity**
    - **Feedback**: "Your submitOrder boils down to a level of indirection + queue push... measuring amortized enqueue performance... ignore the arguably more important tail latency."
    - **Action**: Pause and review the project's goals. Calculate amortized cycles/op/core.

### Technical & QoL Improvements
3.  **SPSC Buffer Alignment**
    - **Feedback**: "Your SPSC buffer only statically align the head and tail, but not the buffer itself... use std::align_val_t with new."
    - **Improvement**: modify `RingBuffer` allocation to ensure the data buffer starts on a cache line boundary.

4.  **Hardware Interference Size**
    - **Feedback**: "std::hardware_destructive_interference_size is not absolutely guaranteed... define a fallback say 64-byte."
    - **Improvement**: Add a macro check or constant for cache line size to support cross-platform builds (e.g., M1 Mac vs Linux x86).

5.  **Buffer Size & Power of 2**
    - **Feedback**: "Don't have to make your buffer size power of 2 just to wrap around... opaque round-up could be wasteful... risks spilling your buffer to lower level caches."
    - **Improvement**: Re-evaluate if power-of-2 is strictly necessary. If keeping it, use C++20 `<bit>` header (e.g., `std::bit_ceil`, `std::countl_zero`) instead of manual loops.

6.  **Benchmarking Clock**
    - **Feedback**: "std::high_resolution_clock... may alias system_clock and break monotonicity."
    - **Improvement**: Use `std::chrono::steady_clock` for duration measurements.

7.  **Concurrency Testing**
    - **Feedback**: "Consider writing some TSan'd unit tests... benchmark using a single core vs two or more cores."
    - **Improvement**: Add ThreadSanitizer (TSan) build target. Run benchmarks with specific core pinning/isolation if possible.

8.  **System Quiescence**
    - **Feedback**: "Benchmark setup doesn't quiesce the system... using std::thread::hardware_concurrency() ... cores will be serving other workloads."
    - **Improvement**: Reduce thread count to leave room for OS/background tasks. Explore pinning (affinity).

9.  **Tools**
    - **Feedback**: Use `clang-tidy` and `perf`.



### Efficiency Stats (Back-of-Envelope)
- **Throughput**: ~45 Million ops/sec
- **Hardware**: Macbook Pro M1 Pro (Performance Core freq ~3.2 GHz)
- **Cores Utilized**: 5 Cores
- **Total Cycles Consumed**: $3.2 \times 10^9 \text{ cycles/sec/core} \times 5 \text{ cores} \approx 16 \times 10^9 \text{ cycles/sec}$
- **Cycles per Op**: $\frac{16 \times 10^9}{45 \times 10^6} \approx 355 \text{ cycles/op}$
- **Conclusion**: The amortized cost of 355 cycles per order submission suggests reasonably low overhead, but there is headroom for optimization compared to sub-100 cycle HFT standards.
