#include <gtest/gtest.h>

#include "binance_api.hpp"
#include "log.hpp"

namespace sigmax {
class BinanceApiTests : public ::testing::Test
{
protected:
    void SetUp() override { Logger::Init(); }
};

/// \brief End-to-end test for parsing
TEST_F(BinanceApiTests, TestDiffDepthMessageParsing)
{
    constexpr int64 kFirstUpdateId{ 20350592474 };
    constexpr int64 kLastUpdateId{ 20350592477 };
    boost::json::value json_message{ { "e", "depthUpdate" },
        { "E", 1786886403206 },
        { "s", "BNBUSDT" },
        { "U", kFirstUpdateId },
        { "u", kLastUpdateId },
        { "b", boost::json::array{ boost::json::array{ "608.12000000", "19.03700000" },
              boost::json::array{ "608.10000000", "24.89700000" } } },
        { "a", boost::json::array{ boost::json::array{ "608.19000000", "61.90000000" } } } };

    const std::optional<BookDepthUpdate> diff_depth{ ParseBookEvent(json_message) };

    ASSERT_TRUE(diff_depth.has_value()); /// fail on this hard
    const BookDepthUpdate &diff_depth_val{ diff_depth.value() };
    EXPECT_EQ(diff_depth_val.first_update_id, kFirstUpdateId);
    EXPECT_EQ(diff_depth_val.final_update_id, kLastUpdateId);
    EXPECT_EQ(diff_depth_val.symbol, Symbol::BNBUSDT);
    EXPECT_EQ(diff_depth_val.event_ts, 1786886403206);

    const std::vector<BidsAsks> expected_bids{ { 60812000000, 1903700000 }, { 60810000000, 2489700000 } };
    const std::vector<BidsAsks> expected_asks{ { 60819000000, 6190000000 } };

    ASSERT_EQ(diff_depth_val.bids.size(), expected_bids.size());
    for (std::size_t i{ 0 }; i < expected_bids.size(); ++i) {
        EXPECT_EQ(diff_depth_val.bids[i].price, expected_bids[i].price);
        EXPECT_EQ(diff_depth_val.bids[i].quantity, expected_bids[i].quantity);
    }

    ASSERT_EQ(diff_depth_val.asks.size(), expected_asks.size());
    for (std::size_t i{ 0 }; i < expected_asks.size(); ++i) {
        EXPECT_EQ(diff_depth_val.asks[i].price, expected_asks[i].price);
        EXPECT_EQ(diff_depth_val.asks[i].quantity, expected_asks[i].quantity);
    }
}
}// namespace sigmax
