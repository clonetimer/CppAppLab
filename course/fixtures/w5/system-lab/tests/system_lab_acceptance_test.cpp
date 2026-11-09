#include "bounded_queue.hpp"
#include "process_runner.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <csignal>
#include <stdexcept>
#include <string>
#include <thread>
#include <unistd.h>
#include <vector>

using namespace std::chrono_literals;
using cppapplab::system_lab::BoundedQueue;
using cppapplab::system_lab::run_process;

TEST(SystemLabAcceptanceTest, QueueRejectsZeroCapacity) {
    BoundedQueue<int> queue(0U);
    EXPECT_EQ(queue.capacity(), 0U);
    EXPECT_FALSE(queue.push(1));
    EXPECT_FALSE(queue.pop().has_value());
}

TEST(SystemLabAcceptanceTest, QueuePushPopPreservesFifoOrder) {
    BoundedQueue<int> queue(3U);
    ASSERT_TRUE(queue.push(1));
    ASSERT_TRUE(queue.push(2));
    ASSERT_TRUE(queue.push(3));
    EXPECT_EQ(queue.pop(), 1);
    EXPECT_EQ(queue.pop(), 2);
    EXPECT_EQ(queue.pop(), 3);
}

TEST(SystemLabAcceptanceTest, QueueReportsCapacityAndSize) {
    BoundedQueue<int> queue(2U);
    EXPECT_EQ(queue.capacity(), 2U);
    EXPECT_EQ(queue.size(), 0U);
    ASSERT_TRUE(queue.push(8));
    EXPECT_EQ(queue.size(), 1U);
}

TEST(SystemLabAcceptanceTest, QueueCloseAllowsDrainingExistingItems) {
    BoundedQueue<int> queue(2U);
    ASSERT_TRUE(queue.push(4));
    ASSERT_TRUE(queue.push(5));
    queue.close();
    EXPECT_EQ(queue.pop(), 4);
    EXPECT_EQ(queue.pop(), 5);
    EXPECT_FALSE(queue.pop().has_value());
}

TEST(SystemLabAcceptanceTest, QueueRejectsPushAfterClose) {
    BoundedQueue<int> queue(1U);
    queue.close();
    EXPECT_FALSE(queue.push(7));
}

TEST(SystemLabAcceptanceTest, QueueCloseIsIdempotent) {
    BoundedQueue<int> queue(1U);
    queue.close();
    queue.close();
    EXPECT_TRUE(queue.closed());
}

TEST(SystemLabAcceptanceTest, QueueWakesBlockedConsumerOnClose) {
    BoundedQueue<int> queue(1U);
    std::atomic<bool> finished{false};
    std::thread consumer([&] {
        EXPECT_FALSE(queue.pop().has_value());
        finished.store(true);
    });
    std::this_thread::sleep_for(20ms);
    queue.close();
    consumer.join();
    EXPECT_TRUE(finished.load());
}

TEST(SystemLabAcceptanceTest, QueueWakesBlockedProducerOnClose) {
    BoundedQueue<int> queue(1U);
    ASSERT_TRUE(queue.push(1));
    std::atomic<bool> accepted{true};
    std::thread producer([&] { accepted.store(queue.push(2)); });
    std::this_thread::sleep_for(20ms);
    queue.close();
    producer.join();
    EXPECT_FALSE(accepted.load());
}

TEST(SystemLabAcceptanceTest, QueueSupportsMultipleProducersAndConsumers) {
    BoundedQueue<int> queue(8U);
    constexpr int producer_count = 3;
    constexpr int values_per_producer = 100;
    std::atomic<int> consumed{0};
    std::vector<std::thread> consumers;
    for (int i = 0; i < 2; ++i) {
        consumers.emplace_back([&] {
            while (queue.pop().has_value()) {
                consumed.fetch_add(1);
            }
        });
    }
    std::vector<std::thread> producers;
    for (int i = 0; i < producer_count; ++i) {
        producers.emplace_back([&, i] {
            for (int j = 0; j < values_per_producer; ++j) {
                (void)queue.push(i * values_per_producer + j);
            }
        });
    }
    for (auto& producer : producers)
        producer.join();
    queue.close();
    for (auto& consumer : consumers)
        consumer.join();
    EXPECT_EQ(consumed.load(), producer_count * values_per_producer);
}

TEST(SystemLabAcceptanceTest, ProcessCapturesStandardOutput) {
    const auto result = run_process({"/bin/echo", "hello"});
    ASSERT_TRUE(result.exited);
    EXPECT_EQ(result.exit_code, 0);
    EXPECT_EQ(result.output, "hello\n");
}

TEST(SystemLabAcceptanceTest, ProcessCapturesStandardError) {
    const auto result = run_process({"/bin/sh", "-c", "echo problem >&2"});
    ASSERT_TRUE(result.exited);
    EXPECT_NE(result.output.find("problem"), std::string::npos);
}

TEST(SystemLabAcceptanceTest, ProcessPreservesExitStatus) {
    const auto result = run_process({"/bin/sh", "-c", "exit 7"});
    ASSERT_TRUE(result.exited);
    EXPECT_EQ(result.exit_code, 7);
}

TEST(SystemLabAcceptanceTest, ProcessUsesArgumentVectorWithoutShellExpansion) {
    const auto result = run_process({"/usr/bin/printf", "%s", "$HOME"});
    EXPECT_EQ(result.output, "$HOME");
}

TEST(SystemLabAcceptanceTest, ProcessRejectsEmptyArgumentList) {
    EXPECT_THROW(run_process({}), std::invalid_argument);
}

TEST(SystemLabAcceptanceTest, ProcessRejectsEmptyProgramName) {
    EXPECT_THROW(run_process({""}), std::invalid_argument);
}

TEST(SystemLabAcceptanceTest, ProcessRejectsNonPositiveTimeout) {
    EXPECT_THROW(run_process({"/bin/true"}, 0ms), std::invalid_argument);
}

TEST(SystemLabAcceptanceTest, ProcessRejectsZeroOutputLimit) {
    EXPECT_THROW(run_process({"/bin/true"}, 1s, 0U), std::invalid_argument);
}

TEST(SystemLabAcceptanceTest, ProcessBoundsCapturedOutput) {
    const auto result = run_process({"/usr/bin/seq", "1", "10000"}, 2s, 128U);
    EXPECT_LE(result.output.size(), 128U);
    EXPECT_TRUE(result.exited);
}

TEST(SystemLabAcceptanceTest, ProcessKillsTimedOutChild) {
    const auto result = run_process({"/bin/sleep", "2"}, 30ms);
    EXPECT_TRUE(result.signaled);
    EXPECT_EQ(result.signal_number, SIGKILL);
}

TEST(SystemLabAcceptanceTest, WriteAllWritesCompleteBuffer) {
    int descriptors[2]{};
    ASSERT_EQ(::pipe(descriptors), 0);
    const std::string expected = "complete message";
    ASSERT_TRUE(cppapplab::system_lab::write_all(descriptors[1], expected.data(), expected.size()));
    ::close(descriptors[1]);
    char buffer[64]{};
    const ssize_t count = ::read(descriptors[0], buffer, sizeof(buffer));
    ::close(descriptors[0]);
    ASSERT_EQ(count, static_cast<ssize_t>(expected.size()));
    EXPECT_EQ(std::string(buffer, static_cast<std::size_t>(count)), expected);
}
