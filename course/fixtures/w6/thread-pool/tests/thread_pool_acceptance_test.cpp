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

TEST(ThreadPoolAcceptanceTest, RejectsZeroWorkers) {
    EXPECT_THROW(ThreadPool(0U, 2U), std::invalid_argument);
}
TEST(ThreadPoolAcceptanceTest, RejectsZeroQueueCapacity) {
    EXPECT_THROW(ThreadPool(1U, 0U), std::invalid_argument);
}
TEST(ThreadPoolAcceptanceTest, ReportsWorkerCount) {
    ThreadPool pool(3U, 8U);
    EXPECT_EQ(pool.worker_count(), 3U);
}
TEST(ThreadPoolAcceptanceTest, ExecutesSubmittedTask) {
    ThreadPool pool(1U, 4U);
    EXPECT_EQ(pool.submit([] { return 42; }).get(), 42);
}
TEST(ThreadPoolAcceptanceTest, SupportsVoidTasks) {
    ThreadPool pool(1U, 4U);
    std::atomic<int> value{0};
    pool.submit([&] { value.store(7); }).get();
    EXPECT_EQ(value.load(), 7);
}
TEST(ThreadPoolAcceptanceTest, ForwardsArguments) {
    ThreadPool pool(1U, 4U);
    EXPECT_EQ(pool.submit([](int a, int b) { return a + b; }, 19, 23).get(), 42);
}
TEST(ThreadPoolAcceptanceTest, SupportsMoveOnlyArguments) {
    ThreadPool pool(1U, 4U);
    auto value = std::make_unique<int>(17);
    EXPECT_EQ(pool.submit([](std::unique_ptr<int> p) { return *p; }, std::move(value)).get(), 17);
    EXPECT_EQ(value, nullptr);
}
TEST(ThreadPoolAcceptanceTest, SupportsMultipleReturnTypes) {
    ThreadPool pool(2U, 8U);
    auto integer = pool.submit([] { return 5; });
    auto text = pool.submit([] { return std::string("worker"); });
    EXPECT_EQ(integer.get(), 5);
    EXPECT_EQ(text.get(), "worker");
}
TEST(ThreadPoolAcceptanceTest, PropagatesTaskExceptionThroughFuture) {
    ThreadPool pool(1U, 4U);
    auto result = pool.submit([]() -> int { throw std::runtime_error("task failure"); });
    EXPECT_THROW(result.get(), std::runtime_error);
}
TEST(ThreadPoolAcceptanceTest, ContinuesAfterTaskException) {
    ThreadPool pool(1U, 4U);
    auto bad = pool.submit([] { throw std::runtime_error("expected"); });
    auto good = pool.submit([] { return 9; });
    EXPECT_THROW(bad.get(), std::runtime_error);
    EXPECT_EQ(good.get(), 9);
}
TEST(ThreadPoolAcceptanceTest, ExecutesManyTasksExactlyOnce) {
    ThreadPool pool(4U, 128U);
    std::atomic<int> count{0};
    std::vector<std::future<void>> results;
    for (int i = 0; i < 100; ++i)
        results.push_back(pool.submit([&] { ++count; }));
    for (auto& result : results)
        result.get();
    EXPECT_EQ(count.load(), 100);
}
TEST(ThreadPoolAcceptanceTest, UsesMoreThanOneWorker) {
    ThreadPool pool(3U, 8U);
    std::atomic<int> workers{0};
    std::atomic<bool> release{false};
    std::vector<std::future<void>> tasks;
    for (int i = 0; i < 3; ++i)
        tasks.push_back(pool.submit([&] {
            workers.fetch_add(1);
            while (!release.load())
                std::this_thread::yield();
        }));
    const auto deadline = std::chrono::steady_clock::now() + 1s;
    while (workers.load() < 3 && std::chrono::steady_clock::now() < deadline)
        std::this_thread::yield();
    release.store(true);
    for (auto& task : tasks)
        task.get();
    EXPECT_EQ(workers.load(), 3);
}
TEST(ThreadPoolAcceptanceTest, ShutdownIsIdempotent) {
    ThreadPool pool(1U, 4U);
    pool.shutdown();
    pool.shutdown();
    EXPECT_TRUE(pool.is_shutdown());
}
TEST(ThreadPoolAcceptanceTest, ShutdownDrainsAcceptedTasks) {
    ThreadPool pool(2U, 16U);
    std::atomic<int> count{0};
    std::vector<std::future<void>> tasks;
    for (int i = 0; i < 12; ++i)
        tasks.push_back(pool.submit([&] { ++count; }));
    pool.shutdown();
    for (auto& task : tasks)
        task.get();
    EXPECT_EQ(count.load(), 12);
}
TEST(ThreadPoolAcceptanceTest, RejectsSubmissionAfterShutdown) {
    ThreadPool pool(1U, 2U);
    pool.shutdown();
    EXPECT_THROW(pool.submit([] { return 1; }), std::runtime_error);
}
TEST(ThreadPoolAcceptanceTest, DestructorDrainsQueuedTasks) {
    std::atomic<int> count{0};
    {
        ThreadPool pool(2U, 16U);
        for (int i = 0; i < 10; ++i)
            (void)pool.submit([&] { ++count; });
    }
    EXPECT_EQ(count.load(), 10);
}
TEST(ThreadPoolAcceptanceTest, FutureCanBeWaitedBeforeGet) {
    ThreadPool pool(1U, 4U);
    auto result = pool.submit([] { return 12; });
    result.wait();
    EXPECT_EQ(result.get(), 12);
}
TEST(ThreadPoolAcceptanceTest, ReportsPendingQueueSize) {
    ThreadPool pool(1U, 2U);
    std::promise<void> gate;
    auto ready = gate.get_future().share();
    auto first = pool.submit([ready] { ready.wait(); });
    auto second = pool.submit([] { return 2; });
    EXPECT_LE(pool.pending(), 1U);
    gate.set_value();
    first.get();
    EXPECT_EQ(second.get(), 2);
}
TEST(ThreadPoolAcceptanceTest, HandlesRapidConstructionAndDestruction) {
    for (int i = 0; i < 20; ++i) {
        ThreadPool pool(2U, 4U);
        EXPECT_EQ(pool.submit([] { return 1; }).get(), 1);
    }
}
TEST(ThreadPoolAcceptanceTest, MoveOnlyCallableIsSupported) {
    ThreadPool pool(1U, 4U);
    auto state = std::make_unique<int>(31);
    auto result = pool.submit([owned = std::move(state)] { return *owned; });
    EXPECT_EQ(result.get(), 31);
}
TEST(ThreadPoolAcceptanceTest, ConcurrentShutdownCallersAllWaitForJoin) {
    ThreadPool pool(2U, 4U);
    std::promise<void> gate;
    auto ready = gate.get_future().share();
    auto task = pool.submit([ready] { ready.wait(); });
    std::atomic<int> returned{0};
    std::thread first([&] {
        pool.shutdown();
        ++returned;
    });
    std::thread second([&] {
        pool.shutdown();
        ++returned;
    });
    std::this_thread::sleep_for(20ms);
    gate.set_value();
    task.get();
    first.join();
    second.join();
    EXPECT_EQ(returned.load(), 2);
    EXPECT_TRUE(pool.is_shutdown());
}
TEST(ThreadPoolAcceptanceTest, BoundedSubmissionBlocksUntilCapacityIsAvailable) {
    ThreadPool pool(1U, 1U);
    std::promise<void> release_worker;
    auto gate = release_worker.get_future().share();
    std::promise<void> worker_entered;
    auto entered = worker_entered.get_future();

    auto first = pool.submit([&] {
        worker_entered.set_value();
        gate.wait();
        return 1;
    });
    ASSERT_EQ(entered.wait_for(1s), std::future_status::ready);
    auto second = pool.submit([] { return 2; });

    std::promise<std::future<int>> third_submission;
    auto third_ready = third_submission.get_future();
    std::thread submitter([&] {
        third_submission.set_value(pool.submit([] { return 3; }));
    });

    const auto blocked_status = third_ready.wait_for(20ms);
    release_worker.set_value();
    auto third = third_ready.get();
    submitter.join();

    EXPECT_EQ(blocked_status, std::future_status::timeout);
    EXPECT_EQ(first.get(), 1);
    EXPECT_EQ(second.get(), 2);
    EXPECT_EQ(third.get(), 3);
}

TEST(ThreadPoolAcceptanceTest, SubmitRacingShutdownIsRejectedOrCompleted) {
    ThreadPool pool(2U, 4U);
    std::atomic<int> completed{0};
    std::atomic<int> rejected{0};
    std::thread submitter([&] {
        for (int i = 0; i < 50; ++i) {
            try {
                auto future = pool.submit([&] { ++completed; });
                future.get();
            } catch (const std::runtime_error&) {
                ++rejected;
            }
        }
    });
    std::this_thread::sleep_for(5ms);
    pool.shutdown();
    submitter.join();
    EXPECT_EQ(completed.load() + rejected.load(), 50);
    EXPECT_TRUE(pool.is_shutdown());
}
