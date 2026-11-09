#include "thread_pool.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <future>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

using namespace std::chrono_literals;
using cppapplab::concurrency::ThreadPool;

TEST(ThreadPoolLearnerTest, ExecutesTasksAndReturnsValues) {
    ThreadPool pool(2U, 8U);
    EXPECT_EQ(pool.submit([] { return 42; }).get(), 42);
}
TEST(ThreadPoolLearnerTest, ForwardsArgumentsAndMoveOnlyValues) {
    ThreadPool pool(1U, 4U);
    auto value = std::make_unique<int>(9);
    EXPECT_EQ(pool.submit([](std::unique_ptr<int> p, int n) { return *p + n; }, std::move(value), 3)
                  .get(),
              12);
    EXPECT_EQ(value, nullptr);
}
TEST(ThreadPoolLearnerTest, PropagatesExceptionsThroughFuture) {
    ThreadPool pool(1U, 4U);
    auto future = pool.submit([]() -> int { throw std::runtime_error("expected"); });
    EXPECT_THROW(future.get(), std::runtime_error);
}
TEST(ThreadPoolLearnerTest, ContinuesProcessingAfterException) {
    ThreadPool pool(1U, 4U);
    auto bad = pool.submit([] { throw std::runtime_error("expected"); });
    auto good = pool.submit([] { return 7; });
    EXPECT_THROW(bad.get(), std::runtime_error);
    EXPECT_EQ(good.get(), 7);
}
TEST(ThreadPoolLearnerTest, ExecutesConcurrentTasksAcrossWorkers) {
    ThreadPool pool(3U, 4U);
    std::atomic<int> active{0};
    std::atomic<int> peak{0};
    std::atomic<bool> release{false};
    std::vector<std::future<void>> futures;
    for (int i = 0; i < 3; ++i)
        futures.push_back(pool.submit([&] {
            const int now = ++active;
            int old = peak.load();
            while (old < now && !peak.compare_exchange_weak(old, now)) {
            }
            while (!release.load())
                std::this_thread::yield();
            --active;
        }));
    const auto deadline = std::chrono::steady_clock::now() + 1s;
    while (peak.load() < 3 && std::chrono::steady_clock::now() < deadline)
        std::this_thread::yield();
    release.store(true);
    for (auto& future : futures)
        future.get();
    EXPECT_EQ(peak.load(), 3);
}
TEST(ThreadPoolLearnerTest, ShutdownDrainsAcceptedTasks) {
    ThreadPool pool(2U, 16U);
    std::atomic<int> count{0};
    std::vector<std::future<void>> futures;
    for (int i = 0; i < 10; ++i)
        futures.push_back(pool.submit([&] { ++count; }));
    pool.shutdown();
    for (auto& future : futures)
        future.get();
    EXPECT_EQ(count.load(), 10);
}
TEST(ThreadPoolLearnerTest, RejectsSubmissionAfterShutdown) {
    ThreadPool pool(1U, 2U);
    pool.shutdown();
    EXPECT_THROW(pool.submit([] { return 1; }), std::runtime_error);
}
TEST(ThreadPoolLearnerTest, ShutdownIsIdempotent) {
    ThreadPool pool(1U, 2U);
    std::thread a([&] { pool.shutdown(); });
    std::thread b([&] { pool.shutdown(); });
    a.join();
    b.join();
    EXPECT_TRUE(pool.is_shutdown());
}
TEST(ThreadPoolLearnerTest, DestructorJoinsWorkersAndDrainsWork) {
    std::atomic<int> count{0};
    {
        ThreadPool pool(2U, 16U);
        for (int i = 0; i < 12; ++i)
            (void)pool.submit([&] { ++count; });
    }
    EXPECT_EQ(count.load(), 12);
}
TEST(ThreadPoolLearnerTest, HandlesManyTasksWithoutLossOrDuplication) {
    ThreadPool pool(4U, 128U);
    std::vector<std::atomic<int>> seen(100);
    std::vector<std::future<void>> futures;
    for (int i = 0; i < 100; ++i)
        futures.push_back(pool.submit([&, i] { ++seen[static_cast<std::size_t>(i)]; }));
    for (auto& future : futures)
        future.get();
    for (const auto& count : seen)
        EXPECT_EQ(count.load(), 1);
}
