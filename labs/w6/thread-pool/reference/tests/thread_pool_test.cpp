#include "thread_pool.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <future>
#include <memory>
#include <stdexcept>
#include <thread>
#include <vector>

using namespace std::chrono_literals;
using cppapplab::concurrency::ThreadPool;

TEST(ThreadPoolLearnerTest, ExecutesTasksAndReturnsValues) {
    ThreadPool pool(2U, 4U);
    EXPECT_EQ(pool.submit([] { return 42; }).get(), 42);
}
TEST(ThreadPoolLearnerTest, ForwardsArgumentsAndMoveOnlyValues) {
    ThreadPool pool(1U, 4U);
    auto value = std::make_unique<int>(9);
    EXPECT_EQ(pool.submit([](std::unique_ptr<int> item) { return *item; },
                          std::move(value)).get(), 9);
    EXPECT_EQ(value, nullptr);
}
TEST(ThreadPoolLearnerTest, PropagatesExceptionsThroughFuture) {
    ThreadPool pool(1U, 4U);
    auto result = pool.submit([]() -> int { throw std::runtime_error("expected"); });
    EXPECT_THROW(result.get(), std::runtime_error);
}
TEST(ThreadPoolLearnerTest, ContinuesProcessingAfterException) {
    ThreadPool pool(1U, 4U);
    auto failed = pool.submit([] { throw std::runtime_error("expected"); });
    auto succeeded = pool.submit([] { return 7; });
    EXPECT_THROW(failed.get(), std::runtime_error);
    EXPECT_EQ(succeeded.get(), 7);
}
TEST(ThreadPoolLearnerTest, ExecutesConcurrentTasksAcrossWorkers) {
    ThreadPool pool(3U, 4U);
    std::atomic<int> entered{0};
    std::promise<void> release;
    auto gate = release.get_future().share();
    std::vector<std::future<void>> tasks;
    for (int index = 0; index < 3; ++index) {
        tasks.push_back(pool.submit([&] {
            entered.fetch_add(1);
            gate.wait();
        }));
    }
    const auto deadline = std::chrono::steady_clock::now() + 1s;
    while (entered.load() != 3 && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::yield();
    }
    release.set_value();
    for (auto& task : tasks) {
        task.get();
    }
    EXPECT_EQ(entered.load(), 3);
}
TEST(ThreadPoolLearnerTest, ShutdownDrainsAcceptedTasks) {
    ThreadPool pool(2U, 16U);
    std::atomic<int> count{0};
    std::vector<std::future<void>> tasks;
    for (int index = 0; index < 12; ++index) {
        tasks.push_back(pool.submit([&] { count.fetch_add(1); }));
    }
    pool.shutdown();
    for (auto& task : tasks) {
        task.get();
    }
    EXPECT_EQ(count.load(), 12);
}
TEST(ThreadPoolLearnerTest, RejectsSubmissionAfterShutdown) {
    ThreadPool pool(1U, 2U);
    pool.shutdown();
    EXPECT_THROW(pool.submit([] { return 1; }), std::runtime_error);
}
TEST(ThreadPoolLearnerTest, ShutdownIsIdempotent) {
    ThreadPool pool(1U, 2U);
    std::thread first([&] { pool.shutdown(); });
    std::thread second([&] { pool.shutdown(); });
    first.join();
    second.join();
    EXPECT_TRUE(pool.is_shutdown());
}
TEST(ThreadPoolLearnerTest, DestructorJoinsWorkersAndDrainsWork) {
    std::atomic<int> count{0};
    {
        ThreadPool pool(2U, 8U);
        for (int index = 0; index < 8; ++index) {
            (void)pool.submit([&] { count.fetch_add(1); });
        }
    }
    EXPECT_EQ(count.load(), 8);
}
TEST(ThreadPoolLearnerTest, HandlesManyTasksWithoutLossOrDuplication) {
    ThreadPool pool(4U, 128U);
    std::vector<std::atomic<int>> counts(100);
    for (auto& count : counts) {
        count.store(0);
    }
    std::vector<std::future<void>> tasks;
    for (int index = 0; index < 100; ++index) {
        tasks.push_back(pool.submit([&, index] { counts[static_cast<std::size_t>(index)]++; }));
    }
    for (auto& task : tasks) {
        task.get();
    }
    for (const auto& count : counts) {
        EXPECT_EQ(count.load(), 1);
    }
}
