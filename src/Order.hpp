#pragma once

#include <cstdint>
#include <cstring>
#include <string>

enum class OrderSide { Buy, Sell };

enum class OrderType { Limit, Market };

using OrderId = uint64_t;
using Price = int64_t;
using Quantity = uint32_t;

struct Order {
  OrderId id;
  Price price;
  uint64_t clientOrderId;
  int32_t symbolId;
  Quantity quantity;
  OrderSide side;
  OrderType type;
  bool active = true;

  Order() = default;

  Order(OrderId id, uint64_t clientOrderId, int32_t symbolId, OrderSide side,
        OrderType type, Price price, Quantity quantity)
      : id(id),
        price(price),
        clientOrderId(clientOrderId),
        symbolId(symbolId),
        quantity(quantity),
        side(side),
        type(type) {}
};
