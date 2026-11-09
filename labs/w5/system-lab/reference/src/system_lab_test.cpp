#include "bounded_queue.hpp"
#include "process_runner.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <string>
#include <thread>

using namespace std::chrono_literals;
using cppapplab::system_lab::BoundedQueue;
using cppapplab::system_lab::run_process;

TEST(SystemLabLearnerTest, QueuePreservesFifoOrder) {
    BoundedQueue<int> queue(2U);
    ASSERT_TRUE(queue.push(1));
    ASSERT_TRUE(queue.push(2));
    EXPECT_EQ(queue.pop(), 1);
    EXPECT_EQ(queue.pop(), 2);
}

TEST(SystemLabLearnerTest, QueueBlocksProducerUntilCapacityIsAvailable) {
    BoundedQueue<int> queue(1U);
    ASSERT_TRUE(queue.push(1));
    std::atomic<bool> completed{false};
    std::thread producer([&] {
        completed.store(queue.push(2));
    });
    std::this_thread::sleep_for(20ms);
    EXPECT_FALSE(completed.load());
    EXPECT_EQ(queue.pop(), 1);
    producer.join();
    EXPECT_TRUE(completed.load());
    EXPECT_EQ(queue.pop(), 2);
}

TEST(SystemLabLearnerTest, QueueCloseWakesWaitingConsumer) {
    BoundedQueue<int> queue(1U);
    std::atomic<bool> returned{false};
    std::thread consumer([&] {
        returned.store(!queue.pop().has_value());
    });
    std::this_thread::sleep_for(20ms);
    queue.close();
    consumer.join();
    EXPECT_TRUE(returned.load());
}

TEST(SystemLabLearnerTest, QueueCloseRejectsFurtherPushes) {
    BoundedQueue<int> queue(1U);
    queue.close();
    EXPECT_FALSE(queue.push(1));
}

TEST(SystemLabLearnerTest, QueueDrainsBeforeReturningEmpty) {
    BoundedQueue<int> queue(2U);
    ASSERT_TRUE(queue.push(3));
    ASSERT_TRUE(queue.push(4));
    queue.close();
    EXPECT_EQ(queue.pop(), 3);
    EXPECT_EQ(queue.pop(), 4);
    EXPECT_FALSE(queue.pop().has_value());
}

TEST(SystemLabLearnerTest, MultipleProducersDeliverEveryItemExactlyOnce) {
    BoundedQueue<int> queue(5U);
    std::atomic<int> total{0};
    std::thread consumer([&] {
        while (queue.pop().has_value()) total.fetch_add(1);
    });
    std::thread first([&] { for (int i=0;i<100;++i) (void)queue.push(i); });
    std::thread second([&] { for (int i=0;i<100;++i) (void)queue.push(i); });
    first.join();
    second.join();
    queue.close();
    consumer.join();
    EXPECT_EQ(total.load(), 200);
}

TEST(SystemLabLearnerTest, ProcessCapturesOutputAndExitCode) {
    const auto result = run_process({"/bin/echo", "learner"});
    ASSERT_TRUE(result.exited);
    EXPECT_EQ(result.exit_code, 0);
    EXPECT_EQ(result.output, "learner\n");
}

TEST(SystemLabLearnerTest, ProcessDoesNotInterpretArgumentsThroughShell) {
    const auto result = run_process({"/usr/bin/printf", "%s", "; touch /tmp/not-created"});
    EXPECT_EQ(result.output, "; touch /tmp/not-created");
}

TEST(SystemLabLearnerTest, ProcessTimeoutTerminatesChild) {
    const auto result = run_process({"/bin/sleep", "2"}, 30ms);
    EXPECT_TRUE(result.signaled);
}

TEST(SystemLabLearnerTest, ProcessOutputIsBounded) {
    const auto result = run_process({"/usr/bin/seq", "1", "10000"}, 2s, 64U);
    EXPECT_LE(result.output.size(), 64U);
    EXPECT_TRUE(result.exited);
}
