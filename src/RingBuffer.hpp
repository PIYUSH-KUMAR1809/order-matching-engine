#pragma once

#include <algorithm>
#include <atomic>
#include <cstring>
#include <memory>
#include <thread>

#if defined(__x86_64__) || defined(_M_X64)
#include <emmintrin.h>
#endif

class SpinLock {
  std::atomic_flag flag = ATOMIC_FLAG_INIT;

 public:
  void lock() {
    while (flag.test_and_set(std::memory_order_acquire)) {
      while (flag.test(std::memory_order_relaxed)) {
#if defined(__x86_64__) || defined(_M_X64)
        _mm_pause();
#elif defined(__aarch64__)
        asm volatile("yield");
#else
        std::this_thread::yield();
#endif
      }
    }
  }

  void unlock() { flag.clear(std::memory_order_release); }
};

template <typename T>
class RingBuffer {
 public:
  explicit RingBuffer(size_t size)
      : size_(size), buffer_(std::make_unique<T[]>(size)), head_(0), tail_(0) {}

  RingBuffer(const RingBuffer&) = delete;
  RingBuffer& operator=(const RingBuffer&) = delete;

  bool push(const T& item) {
    lock_.lock();
    size_t next_tail = (tail_ + 1) % size_;
    if (next_tail == head_) {
      lock_.unlock();
      return false;
    }
    buffer_[tail_] = item;
    tail_ = next_tail;
    lock_.unlock();
    return true;
  }

  bool pop(T& item) {
    lock_.lock();
    if (head_ == tail_) {
      lock_.unlock();
      return false;
    }
    item = buffer_[head_];
    head_ = (head_ + 1) % size_;
    lock_.unlock();
    return true;
  }

  bool push_block(const T& item) {
    while (true) {
      lock_.lock();
      size_t next_tail = (tail_ + 1) % size_;
      if (next_tail != head_) {
        buffer_[tail_] = item;
        tail_ = next_tail;
        lock_.unlock();
        return true;
      }
      lock_.unlock();
      std::this_thread::yield();
    }
  }

  bool push_batch(const T* items, size_t count) {
    if (count == 0) return true;
    lock_.lock();
    size_t available = (size_ + head_ - tail_ - 1) % size_;
    if (count > available) {
      lock_.unlock();
      return false;
    }

    size_t first_chunk = std::min(count, size_ - tail_);
    std::memcpy(&buffer_[tail_], items, first_chunk * sizeof(T));
    if (first_chunk < count) {
      std::memcpy(&buffer_[0], items + first_chunk,
                  (count - first_chunk) * sizeof(T));
    }
    tail_ = (tail_ + count) % size_;

    lock_.unlock();
    return true;
  }

  size_t pop_batch(T* dest, size_t max_count) {
    lock_.lock();
    if (head_ == tail_) {
      lock_.unlock();
      return 0;
    }

    size_t available = (size_ + tail_ - head_) % size_;
    size_t count = std::min(max_count, available);

    size_t first_chunk = std::min(count, size_ - head_);
    std::memcpy(dest, &buffer_[head_], first_chunk * sizeof(T));
    if (first_chunk < count) {
      std::memcpy(dest + first_chunk, &buffer_[0],
                  (count - first_chunk) * sizeof(T));
    }
    head_ = (head_ + count) % size_;

    lock_.unlock();
    return count;
  }

 private:
  size_t size_;
  std::unique_ptr<T[]> buffer_;
  size_t head_;
  size_t tail_;

  alignas(64) SpinLock lock_;
};
