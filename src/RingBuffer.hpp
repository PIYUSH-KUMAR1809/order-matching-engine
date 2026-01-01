#pragma once

#include <atomic>
#include <cassert>
#include <cstddef>
#include <new>

#ifdef __cpp_lib_hardware_interference_size
using std::hardware_destructive_interference_size;
#endif

template <typename T>
class RingBuffer {
 public:
  explicit RingBuffer(size_t capacity) {
    if (capacity < 2) capacity = 2;
    size_t cap = 1;
    while (cap < capacity) cap *= 2;
    capacity_ = cap;
    mask_ = capacity_ - 1;

    buffer_ = new T[capacity_];
  }

  ~RingBuffer() { delete[] buffer_; }

  RingBuffer(const RingBuffer&) = delete;
  RingBuffer& operator=(const RingBuffer&) = delete;

  bool push(const T& item) {
    const size_t head = head_.load(std::memory_order_relaxed);
    const size_t next_head = (head + 1) & mask_;

    if (next_head == tail_cache_) {
      tail_cache_ = tail_.load(std::memory_order_acquire);
      
      if (next_head == tail_cache_) {
        return false;
      }
    }

    buffer_[head] = item;
    head_.store(next_head, std::memory_order_release);
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

    item = buffer_[tail];
    tail_.store((tail + 1) & mask_, std::memory_order_release);
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

 private:
  size_t capacity_;
  size_t mask_;
  T* buffer_;

  alignas(hardware_destructive_interference_size) std::atomic<size_t> head_{0};
  alignas(hardware_destructive_interference_size) size_t tail_cache_{0};

  alignas(hardware_destructive_interference_size) std::atomic<size_t> tail_{0};
  alignas(hardware_destructive_interference_size) size_t head_cache_{0};
};
