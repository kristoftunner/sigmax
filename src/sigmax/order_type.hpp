#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace sigmax {

enum class OrderSide : std::uint8_t { BUY, SELL };
enum class OrderState : std::uint8_t { NEW, PARTIAL, FILLED, CANCELLED };
enum class Symbol : std::uint16_t { BNBBTC };

using int64 = std::int64_t;
using uint32 = std::uint32_t;

using Timestamp = int64;
using OrderId = int64;// using an int because of the undefined overflow behavour of uints

constexpr const int64 kFixedPointShift{ 100000000 };

Symbol StrToSymbol(const std::string_view &symbol_str);

// TODO: use better instrument id
// lets use a string for now for the instrument ID and use an enum or an integer later with a proper
// perf analysis
struct BidsAsks
{
    int64_t price;
    int64_t quantity;
};

struct BookDepthUpdate
{
    Timestamp event_ts;
    Symbol symbol;// later probably this should be an enum
    int64_t first_update_id;
    int64_t final_update_id;
    std::vector<BidsAsks> bids;
    std::vector<BidsAsks> asks;
};

struct BookInitData
{
    Timestamp event_ts;
    Symbol symbol;// later probably this should be an enum
    uint32_t update_id;
    std::vector<BidsAsks> bids;
    std::vector<BidsAsks> asks;
};

}// namespace sigmax
