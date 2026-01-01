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
  OrderSide side;
  OrderType type;
  Price price;
  Quantity quantity;
  char symbol[8];
  uint64_t clientOrderId;

  bool active = true;

  Order() = default;

  Order(OrderId id, uint64_t clientOrderId, const std::string &sym,
        OrderSide side, OrderType type, Price price, Quantity quantity)
      : id(id),
        clientOrderId(clientOrderId),
        side(side),
        type(type),
        price(price),
        quantity(quantity)
  {
    std::memset(symbol, 0, 8);
    std::strncpy(symbol, sym.c_str(), 7);
  }

  std::string getSymbol() const { return std::string(symbol); }
};
