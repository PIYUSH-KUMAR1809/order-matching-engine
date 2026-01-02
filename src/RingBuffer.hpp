#pragma once

#include <atomic>
#include <bit>
#include <cassert>
#include <chrono>
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

    buffer_ = new (std::align_val_t(hardware_destructive_interference_size))
        T[capacity_];
  }

  ~RingBuffer() {
    ::operator delete[](
        buffer_, std::align_val_t(hardware_destructive_interference_size));
  }

  RingBuffer(const RingBuffer&) = delete;
  RingBuffer& operator=(const RingBuffer&) = delete;
  RingBuffer(RingBuffer&&) = delete;
  RingBuffer& operator=(RingBuffer&&) = delete;

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

    if (tail == head_cache_) {
      head_cache_ = head_.load(std::memory_order_acquire);
      if (tail == head_cache_) return 0;
    }

    size_t head = head_cache_;

    size_t available = head - tail;
    if (available > max_count) available = max_count;

    for (size_t i = 0; i < available; ++i) {
      output[i] = buffer_[(tail + i) & mask_];
    }

    tail_.store(tail + available, std::memory_order_release);
    return available;
  }

  bool push_batch(const T* batch, size_t count) {
    if (count == 0) return true;
    if (count > capacity_) return false;

    while (lock_.test_and_set(std::memory_order_acquire)) {
#if defined(__x86_64__) || defined(_M_X64)
      _mm_pause();
#elif defined(__aarch64__)
      asm volatile("yield");
#endif
    }

    const size_t head = head_.load(std::memory_order_relaxed);

    if (head - tail_cache_ + count > capacity_) {
      tail_cache_ = tail_.load(std::memory_order_acquire);
      if (head - tail_cache_ + count > capacity_) {
        lock_.clear(std::memory_order_release);
        return false;
      }
    }

    for (size_t i = 0; i < count; ++i) {
      buffer_[(head + i) & mask_] = batch[i];
    }

    head_.store(head + count, std::memory_order_release);
    lock_.clear(std::memory_order_release);
    return true;
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

  void push_block_measure(const T& item,
                          std::chrono::nanoseconds& wait_duration) {
    if (push(item)) return;

    auto start = std::chrono::steady_clock::now();
    while (!push(item)) {
#if defined(__x86_64__) || defined(_M_X64)
      _mm_pause();
#elif defined(__aarch64__)
      asm volatile("yield");
#endif
    }
    auto end = std::chrono::steady_clock::now();
    wait_duration += (end - start);
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
