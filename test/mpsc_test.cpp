#include <chrono>
#include <future>
#include <thread>

#include <gtest/gtest.h>

#include "mpsc_queue.hpp"

namespace sigmax {
class MpscQueueTest : public ::testing::Test
{
protected:
    void SetUp() override { Logger::Init(); }
};
/// \brief Test the MpscQueue class
/// \test Basic push and pop ops
TEST_F(MpscQueueTest, PushAndPop)
{
    MpscQueue<int, 16> queue;
    EXPECT_EQ(queue.PushBack(1), QueueState::SUCCESS);
    EXPECT_EQ(queue.PushBack(2), QueueState::SUCCESS);
    EXPECT_EQ(queue.PushBack(3), QueueState::SUCCESS);
    auto value = queue.Pop();
    EXPECT_TRUE(value.has_value());
    EXPECT_EQ(value.value(), 1);
    value = queue.Pop();
    EXPECT_TRUE(value.has_value());
    EXPECT_EQ(value.value(), 2);
    value = queue.Pop();
    EXPECT_TRUE(value.has_value());
    EXPECT_EQ(value.value(), 3);
    value = queue.Pop();
    EXPECT_FALSE(value.has_value());
    EXPECT_EQ(value.error(), QueueState::QUEUE_IS_EMPTY);
}


TEST_F(MpscQueueTest, FillAndPopEmpty)
{
    MpscQueue<int, 8> queue;
    for (int i = 0; i < 8; i++) { EXPECT_EQ(queue.PushBack(i), QueueState::SUCCESS); }

    for (int i = 0; i < 8; i++) {
        auto value = queue.Pop();
        ASSERT_TRUE(value.has_value());
        EXPECT_EQ(value.value(), i);
    }
    auto value = queue.Pop();
    EXPECT_FALSE(value.has_value());
    EXPECT_EQ(value.error(), QueueState::QUEUE_IS_EMPTY);
}

TEST_F(MpscQueueTest, OverflowTwice)
{
    // overflow 1
    MpscQueue<int, 16> queue;
    for (int i{ 0 }; i < 2; i++) {
        for (int j{ 0 }; j < 16; j++) { queue.PushBack(j); }
        for (int k{ 0 }; k < 2; k++) {
            auto ret = queue.PushBack(10 + k);
            EXPECT_EQ(ret, QueueState::QUEUE_IS_FULL);
        }

        for (int j = 0; j < 16; j++) {
            auto value = queue.Pop();
            ASSERT_TRUE(value.has_value());
            EXPECT_EQ(value.value(), j);
        }

        auto value = queue.Pop();
        EXPECT_FALSE(value.has_value());
        EXPECT_EQ(value.error(), QueueState::QUEUE_IS_EMPTY);

        value = queue.Pop();
        EXPECT_FALSE(value.has_value());
        EXPECT_EQ(value.error(), QueueState::QUEUE_IS_EMPTY);
    }
}


/// \brief multiple consumer and single producer concurrency tests
/// \detail 2 producer and one consumer
TEST_F(MpscQueueTest, MultipleConsumerTest)
{

    // this not a performance test if the reader can consume data fast enough from the queue
    // rather to test the concurrency of the queue
    MpscQueue<int, 512> queue;
    auto writer1 = [&]() {
        int i{ 0 };
        while (i < 50) {

            auto ret = queue.PushBack(1);
            EXPECT_EQ(ret, QueueState::SUCCESS);
            i++;
            if (i % 30) { std::this_thread::sleep_for(std::chrono::milliseconds(5)); }
            if(i == 50) {
                asm volatile("nop");
            }
        }
    };
    auto writer2 = [&]() {
        int i{ 0 };
        while (i < 60) {

            auto ret =queue.PushBack(2);
            EXPECT_EQ(ret, QueueState::SUCCESS);
            i++;
            if (i % 20) { std::this_thread::sleep_for(std::chrono::milliseconds(5)); }
            if(i == 60) {
                asm volatile("nop");
            }
        }
    };
    auto reader = [&]() -> int {
        int popCount{ 0 };
        int sum{ 0 };
        while (popCount < 110) {
            auto ret = queue.Pop();
            if(ret.has_value()) {
                popCount++;
                sum += ret.value();
            }
        }

        return popCount;
    };

    auto fut = std::async(std::launch::async, reader);
    std::thread w1Thread(writer1);
    std::thread w2Thread(writer2);
    std::thread readerThread(reader);

    w1Thread.join();
    w2Thread.join();
    const int res = fut.get();

    EXPECT_EQ(res, 500);
}


}// namespace sigmax
