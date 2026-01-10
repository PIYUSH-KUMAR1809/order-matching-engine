#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

class PriceBitset {
  std::vector<uint64_t> data_;
  size_t size_;

 public:
  explicit PriceBitset(size_t size) : data_((size + 63) / 64, 0), size_(size) {}
  void set(size_t index) {
    if (index < size_) data_[index / 64] |= (1ULL << (index % 64));
  }
  void clear(size_t index) {
    if (index < size_) data_[index / 64] &= ~(1ULL << (index % 64));
  }
  void clearAll() { std::fill(data_.begin(), data_.end(), 0); }
  bool test(size_t index) const {
    if (index >= size_) return false;
    return (data_[index / 64] & (1ULL << (index % 64))) != 0;
  }

  [[nodiscard]] size_t findFirstSet(size_t start) const {
    if (start >= size_) return size_;
    size_t idx = start / 64;
    size_t bit = start % 64;
    uint64_t word = data_[idx] & (~0ULL << bit);
    if (word != 0) return idx * 64 + __builtin_ctzll(word);
    for (idx++; idx < data_.size(); idx++)
      if (data_[idx] != 0) return idx * 64 + __builtin_ctzll(data_[idx]);
    return size_;
  }
  [[nodiscard]] size_t findFirstSetDown(size_t start) const {
    if (start >= size_) start = size_ - 1;
    size_t idx = start / 64;
    size_t bit = start % 64;
    uint64_t mask = (bit == 63) ? ~0ULL : ((1ULL << (bit + 1)) - 1);
    uint64_t word = data_[idx] & mask;
    if (word != 0) return idx * 64 + (63 - __builtin_clzll(word));
    for (size_t i = idx; i-- > 0;)
      if (data_[i] != 0) return i * 64 + (63 - __builtin_clzll(data_[i]));
    return size_;
  }
};
