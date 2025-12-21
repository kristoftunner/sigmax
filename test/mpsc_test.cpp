#include <exception>
#include <gtest/gtest.h>

#include "mpsc_queue.hpp"

namespace sigmax {
class MpscQueueTest : public ::testing::Test {
protected:
    void SetUp() override {
        Logger::Init();
    }
};
/// \brief Test the MpscQueue class
/// \test Basic push and pop ops
TEST_F(MpscQueueTest, PushAndPop) {
    MpscQueue<int, 10> queue;
    queue.PushBack(1);
    queue.PushBack(2);
    queue.PushBack(3);
    EXPECT_EQ(queue.Size(), 3);
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
    EXPECT_EQ(value.error(), QueueError::QUEUE_IS_EMPTY);
}


TEST_F(MpscQueueTest, FillAndFlush) {
    MpscQueue<int, 10> queue;
    for (int i = 0; i < 10; i++) {
        queue.PushBack(i);
    }
    EXPECT_EQ(queue.Size(), 10);
    auto values = queue.Flush();
    EXPECT_EQ(values.size(), 10);
    for (int i = 0; i < 10; i++) {
        EXPECT_EQ(values[i], i);
    }

    for (int i = 0; i < 10; i++) {
        queue.PushBack(i);
    }
    EXPECT_EQ(queue.Size(), 10);
    for (int i = 0; i < 10; i++) {
        auto value = queue.Pop();
        ASSERT_TRUE(value.has_value());
        EXPECT_EQ(value.value(), i);
    }
    auto value = queue.Pop();
    EXPECT_FALSE(value.has_value());
    EXPECT_EQ(value.error(), QueueError::QUEUE_IS_EMPTY);
}

TEST_F(MpscQueueTest, OverFlowWithFlush) {
    // overflow 1
    MpscQueue<int, 10> queue;
    for (int i = 0; i < 11; i++) {
        queue.PushBack(i);
    }
    EXPECT_EQ(queue.Size(), 10);
    auto values = queue.Flush();
    EXPECT_EQ(values.size(), 10);
    for (int i = 0; i < 10; i++) {
        EXPECT_EQ(values[i], i + 1);
    }

    // overflow twice
    for (int i = 0; i < 22; i++) {
        queue.PushBack(i);
    }
    EXPECT_EQ(queue.Size(), 10);
    values = queue.Flush();
    EXPECT_EQ(values.size(), 10);
    for (int i = 0; i < 10; i++) {
        EXPECT_EQ(values[i], i + 12);
    }
}

TEST_F(MpscQueueTest, OverFlowWithPop) {
    // overflow 1
    MpscQueue<int, 10> queue;
    for (int i = 0; i < 11; i++) {
        queue.PushBack(i);
    }
    EXPECT_EQ(queue.Size(), 10);
    for (int i = 0; i < 10; i++) {
        auto value = queue.Pop();
        ASSERT_TRUE(value.has_value());
        EXPECT_EQ(value.value(), i + 1);
    }

    // overflow 2
    for (int i = 0; i < 22; i++) {
        queue.PushBack(i);
    }
    EXPECT_EQ(queue.Size(), 10);
    for (int i = 0; i < 10; i++) {
        auto value = queue.Pop();
        ASSERT_TRUE(value.has_value());
        EXPECT_EQ(value.value(), i + 12);
    }
}
}// namespace sigmax
