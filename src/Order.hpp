#pragma once

#include <chrono>
#include <cstdint>
#include <string>

enum class OrderType { Buy, Sell };

enum class OrderKind { Limit, Market };

using OrderId = uint64_t;
using Price = double;
using Quantity = uint32_t;

struct Order {
  OrderId id;
  OrderType type;
  OrderKind kind;
  Price price;
  Quantity quantity;
  std::string symbol;
  std::chrono::system_clock::time_point timestamp;

  Order(OrderId id, const std::string &symbol, OrderType type, OrderKind kind,
        Price price, Quantity quantity)
      : id(id),
        symbol(symbol),
        type(type),
        kind(kind),
        price(price),
        quantity(quantity),
        timestamp(std::chrono::system_clock::now()) {}
};
