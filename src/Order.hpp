#pragma once

#include <chrono>
#include <cstdint>
#include <string>

enum class OrderSide { Buy, Sell };

enum class OrderType { Limit, Market };

using OrderId = uint64_t;
using Price = double;
using Quantity = uint32_t;

struct Order {
  OrderId id;
  OrderSide side;
  OrderType type;
  Price price;
  Quantity quantity;
  std::string symbol;
  uint64_t clientOrderId;
  std::chrono::system_clock::time_point timestamp;
  bool active = true;  // For Tombstone deletion

  Order(OrderId id, uint64_t clientOrderId, const std::string &symbol,
        OrderSide side, OrderType type, Price price, Quantity quantity)
      : id(id),
        clientOrderId(clientOrderId),
        symbol(symbol),
        side(side),
        type(type),
        price(price),
        quantity(quantity),
        timestamp(std::chrono::system_clock::now()) {}
};
