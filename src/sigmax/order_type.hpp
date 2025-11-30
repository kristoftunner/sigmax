#include <string>

namespace sigmax
{

enum class OrderSide : std::uint8_t { BUY, SELL };

enum class OrderState : std::uint8_t {
  NEW, PARTIAL, FILLED, CANCELLED 
};

using Timestamp = std::int64_t;
struct Order
{
    std::uint64_t orderId;
    std::string instrumentId;
    OrderSide side;
    OrderState state;
    std::int64_t quantity;
    std::int64_t price;   
    Timestamp ts;
    // here will come the optional metadata
};

} //namespace sigmax

