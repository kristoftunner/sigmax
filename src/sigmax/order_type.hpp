#pragma once
#include <string>

namespace sigmax {

enum class OrderSide : std::uint8_t { BUY, SELL };

enum class OrderState : std::uint8_t { NEW, PARTIAL, FILLED, CANCELLED };

using Timestamp = std::int64_t;
using OrderId = std::int64_t;// using an int because of the undefined overflow behavour of uints
struct Order
{
    OrderId orderId;
    std::string instrumentId;// later probably this should be an enum
    OrderSide side;
    OrderState state;
    std::int64_t quantity;
    std::int64_t price;
    Timestamp ts;
    // here will come the optional metadata
};

}// namespace sigmax
