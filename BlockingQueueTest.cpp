//
// Created by Josh Wilson on 7/25/17.
//


#include <iostream>
#include <thread>
#include <chrono>
#include <string>

#include "gtest/gtest.h"
#include "BlockingQueue.h"

void delayms(int ms = 1000) {
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

class BlockingQueueTest : public ::testing::Test {
protected:
    // The blocking queue to be set up for each test
    BlockingQueue<std::string> bq;
};

TEST_F(BlockingQueueTest, SingleLocker) {
    EXPECT_FALSE(bq.owns_lock());
    {
        std::unique_lock<decltype(bq)> lock(bq);
        EXPECT_TRUE(bq.owns_lock());
    }
    EXPECT_FALSE(bq.owns_lock());
}

TEST_F(BlockingQueueTest, MultipleLockers) {
    EXPECT_FALSE(bq.owns_lock());
    {
        std::unique_lock<decltype(bq)> lock(bq);
        EXPECT_TRUE(bq.owns_lock());
        {
            std::thread t1([this]() {
                EXPECT_FALSE(bq.owns_lock());
                EXPECT_FALSE(bq.try_lock());
            });
            t1.join();
        }
        EXPECT_TRUE(bq.owns_lock());
    }
    EXPECT_FALSE(bq.owns_lock());
    std::thread t2([this]() {
        std::unique_lock<decltype(bq)> lock(bq);
        EXPECT_TRUE(bq.owns_lock());
    });
    t2.join();
    ASSERT_TRUE(bq.try_lock());
    bq.unlock();
}

TEST_F(BlockingQueueTest, Getters) {
    EXPECT_TRUE(bq.empty());

    bq.push("1");
    bq.push("2");
    bq.push("3");

    EXPECT_FALSE(bq.empty());

    EXPECT_EQ(bq.pop(), "1");

    bq.clear();

    EXPECT_TRUE(bq.empty());
}

TEST_F(BlockingQueueTest, Contention) {
    std::thread t1([this]() {
        bq.push("a");
        bq.push("b");
        bq.push("c");
        bq.push("d");
        bq.push("e");
    });
    std::thread t2([this]() {
        bq.push("f");
        bq.push("g");
        bq.push("h");
        bq.push("i");
        bq.push("j");
    });

    auto results = std::string();
    for (int i = 0; i < 10; ++i) {
        results.append(bq.pop());
    }
    EXPECT_TRUE(results[0] == 'a' || results[0] == 'f');
    EXPECT_THROW(bq.try_pop(), std::out_of_range);

    std::sort(results.begin(), results.end());
    EXPECT_EQ(results, "abcdefghij");
    t1.join();
    t2.join();
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
