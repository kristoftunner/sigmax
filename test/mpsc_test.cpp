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
TEST_F(MpscQueueTest, Push_SingleThread)
{
    static constexpr int kQueueSize = 16;
    MpscQueue<int, kQueueSize> queue;
    for (int i = 0; i < kQueueSize; i++) { EXPECT_EQ(queue.PushBack(i), QueueState::SUCCESS); }
}

/// \test Basic push and pop ops
TEST_F(MpscQueueTest, PushAndPop_SingleThread)
{
    static constexpr int kQueueSize = 16;
    MpscQueue<int, kQueueSize> queue;
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


TEST_F(MpscQueueTest, FillAndPopEmpty_SingleThread)
{
    static constexpr int kQueueSize = 8;
    MpscQueue<int, kQueueSize> queue;
    for (int i = 0; i < kQueueSize; i++) { EXPECT_EQ(queue.PushBack(i), QueueState::SUCCESS); }

    for (int i = 0; i < kQueueSize; i++) {
        auto value = queue.Pop();
        ASSERT_TRUE(value.has_value());
        EXPECT_EQ(value.value(), i);
    }
    auto value = queue.Pop();
    EXPECT_FALSE(value.has_value());
    EXPECT_EQ(value.error(), QueueState::QUEUE_IS_EMPTY);
}

TEST_F(MpscQueueTest, OverflowTwice_SingleThread)
{
    // overflow 1
    static constexpr int kQueueSize = 16;
    MpscQueue<int, kQueueSize> queue;
    for (int i{ 0 }; i < 2; i++) {
        for (int j{ 0 }; j < kQueueSize; j++) { queue.PushBack(j); }
        for (int k{ 0 }; k < 2; k++) {
            auto ret = queue.PushBack(10 + k);
            EXPECT_EQ(ret, QueueState::QUEUE_IS_FULL);
        }

        for (int j{ 0 }; j < kQueueSize; j++) {
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
TEST_F(MpscQueueTest, MultipleConsumerTest1)
{

    // this not a performance test if the reader can consume data fast enough from the queue
    // rather to test the concurrency of the queue
    static constexpr int kQueueSize = 512;
    MpscQueue<int, kQueueSize> queue;
    std::promise<void> go, w1Ready, w2Ready, rReady;
    std::shared_future<void> ready(go.get_future().share());

    auto writer1 = [&queue, &ready, &w1Ready]() -> void {
        w1Ready.set_value();
        ready.wait();
        int i{ 0 };
        while (i < kQueueSize / 2) {

            auto ret = queue.PushBack(1);
            EXPECT_EQ(ret, QueueState::SUCCESS);
            i++;
        }
    };
    auto writer2 = [&queue, &ready, &w2Ready]() -> void {
        w2Ready.set_value();
        ready.wait();
        int i{ 0 };
        while (i < kQueueSize / 2) {

            auto ret = queue.PushBack(2);
            EXPECT_EQ(ret, QueueState::SUCCESS);
            i++;
        }
    };
    auto reader = [&queue, &ready, &rReady]() -> std::pair<int, int> {
        rReady.set_value();
        ready.wait();
        int successfulPopCount{ 0 };
        int sum{ 0 };
        while (successfulPopCount < kQueueSize) {
            auto ret = queue.Pop();
            if (ret.has_value()) {
                successfulPopCount++;
                sum += ret.value();
            }
        }

        return { successfulPopCount, sum };
    };

    std::future<void> w1Done;
    std::future<void> w2Done;
    std::future<std::pair<int, int>> rDone;

    w1Done = std::async(std::launch::async, writer1);
    w2Done = std::async(std::launch::async, writer2);
    rDone = std::async(std::launch::async, reader);

    w1Ready.get_future().wait();
    w2Ready.get_future().wait();
    go.set_value();
    const auto [popCount, sum] = rDone.get();

    EXPECT_EQ(popCount, kQueueSize);
    EXPECT_EQ(sum, kQueueSize / 2 * (1 + 2));
}


/// \brief multiple producer and single consumer concurrency tests
/// \detail 3 producer and one consumer, overwriting the queue
TEST_F(MpscQueueTest, MultipleConsumerTest2)
{
    static constexpr int kQueueSize = 512;
    auto writer = [](MpscQueue<int, kQueueSize> &queue, const int valuesToWrite, std::promise<void> &ready, std::shared_future<void> &go) {
        ready.set_value();
        go.wait();
        for (int i{ 0 }; i < valuesToWrite; i++) { queue.PushBack(1); }
    };
    auto reader = [](MpscQueue<int, kQueueSize> &queue,
                      const int valuesToRead,
                      std::promise<void> &ready,
                      std::shared_future<void> &go) -> std::pair<int, int> {
        ready.set_value();
        go.wait();
        int successfulPopCount{ 0 };
        int sum{ 0 };
        while (successfulPopCount < valuesToRead) {
            auto ret = queue.Pop();
            if (ret.has_value()) {
                successfulPopCount++;
                sum += ret.value();
            }
        }

        return { successfulPopCount, sum };
    };

    std::future<void> w1Done;
    std::future<void> w2Done;
    std::future<void> w3Done;
    std::future<std::pair<int, int>> rDone;

    std::promise<void> go, w1Ready, w2Ready, w3Ready, rReady;
    std::shared_future<void> ready(go.get_future().share());
    MpscQueue<int, kQueueSize> queue;
    w1Done = std::async(std::launch::async, writer, std::ref(queue), kQueueSize, std::ref(w1Ready), std::ref(ready));
    w2Done = std::async(std::launch::async, writer, std::ref(queue), kQueueSize, std::ref(w2Ready), std::ref(ready));
    w3Done = std::async(std::launch::async, writer, std::ref(queue), kQueueSize, std::ref(w3Ready), std::ref(ready));
    rDone = std::async(std::launch::async, reader, std::ref(queue), kQueueSize, std::ref(rReady), std::ref(ready));

    w1Ready.get_future().wait();
    w2Ready.get_future().wait();
    w3Ready.get_future().wait();
    go.set_value();

    const auto [popCount, sum] = rDone.get();

    EXPECT_EQ(popCount, kQueueSize);
    EXPECT_EQ(sum, kQueueSize);
    EXPECT_EQ(queue.GetPopCount(), kQueueSize);
}
}// namespace sigmax
