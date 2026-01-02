#pragma once

#include <atomic>
#include <bit>
#include <cassert>
#include <cstddef>
#include <new>

#ifdef __cpp_lib_hardware_interference_size
using std::hardware_destructive_interference_size;
#else
constexpr size_t hardware_destructive_interference_size = 64;
#endif

template <typename T>
class RingBuffer {
 public:
  explicit RingBuffer(size_t capacity) {
    if (capacity < 2) capacity = 2;
    capacity_ = std::bit_ceil(capacity);
    mask_ = capacity_ - 1;

    // Use aligned allocation to ensure the buffer starts on a cache line
    // boundary preventing false sharing with preceding data.
    buffer_ = new (std::align_val_t(hardware_destructive_interference_size))
        T[capacity_];
  }

  ~RingBuffer() {
    ::operator delete[](
        buffer_, std::align_val_t(hardware_destructive_interference_size));
  }

  RingBuffer(const RingBuffer&) = delete;
  RingBuffer& operator=(const RingBuffer&) = delete;

  bool push(const T& item) {
    while (lock_.test_and_set(std::memory_order_acquire)) {
#if defined(__x86_64__) || defined(_M_X64)
      _mm_pause();
#elif defined(__aarch64__)
      asm volatile("yield");
#endif
    }

    const size_t head = head_.load(std::memory_order_relaxed);
    const size_t next_head = head + 1;

    if (head - tail_cache_ >= capacity_) {
      tail_cache_ = tail_.load(std::memory_order_acquire);

      if (head - tail_cache_ >= capacity_) {
        lock_.clear(std::memory_order_release);
        return false;
      }
    }

    buffer_[head & mask_] = item;
    head_.store(next_head, std::memory_order_release);
    lock_.clear(std::memory_order_release);
    return true;
  }

  bool pop(T& item) {
    const size_t tail = tail_.load(std::memory_order_relaxed);

    if (tail == head_cache_) {
      head_cache_ = head_.load(std::memory_order_acquire);
      if (tail == head_cache_) {
        return false;
      }
    }

    item = buffer_[tail & mask_];
    tail_.store(tail + 1, std::memory_order_release);
    return true;
  }

  size_t pop_batch(T* output, size_t max_count) {
    const size_t tail = tail_.load(std::memory_order_relaxed);

    // Check available using cache
    if (tail == head_cache_) {
      head_cache_ = head_.load(std::memory_order_acquire);
      if (tail == head_cache_) return 0;
    }

    size_t head = head_cache_;

    size_t available = head - tail;  // Monotonic difference
    if (available > max_count) available = max_count;

    // Read loop
    for (size_t i = 0; i < available; ++i) {
      output[i] = buffer_[(tail + i) & mask_];
    }

    tail_.store(tail + available, std::memory_order_release);
    return available;
  }

  void push_block(const T& item) {
    while (!push(item)) {
#if defined(__x86_64__) || defined(_M_X64)
      _mm_pause();
#elif defined(__aarch64__)
      asm volatile("yield");
#endif
    }
  }

 private:
  size_t capacity_;
  size_t mask_;
  T* buffer_;

  alignas(hardware_destructive_interference_size) std::atomic<size_t> head_{0};
  alignas(hardware_destructive_interference_size) size_t tail_cache_{0};

  alignas(hardware_destructive_interference_size) std::atomic<size_t> tail_{0};
  alignas(hardware_destructive_interference_size) size_t head_cache_{0};

  std::atomic_flag lock_ = ATOMIC_FLAG_INIT;
};
