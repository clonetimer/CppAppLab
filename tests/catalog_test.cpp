#include "cppapplab/task.hpp"

#include <gtest/gtest.h>

TEST(TaskCatalogTest, ExposesFirstHelloWorldExercise) {
    const cppapplab::Task* task = cppapplab::find_task("w1.hello-world");

    ASSERT_NE(task, nullptr);
    EXPECT_EQ(task->week, 1U);
    EXPECT_EQ(task->order, 1U);
    EXPECT_EQ(task->acceptance_kind, cppapplab::AcceptanceKind::ExactStdout);
    EXPECT_EQ(task->source_files.size(), 1U);
    EXPECT_TRUE(task->prerequisites.empty());
}

TEST(TaskCatalogTest, ExposesMultiFileCalculatorExercise) {
    const cppapplab::Task* task = cppapplab::find_task("w1.calculator");

    ASSERT_NE(task, nullptr);
    EXPECT_EQ(task->week, 1U);
    EXPECT_EQ(task->order, 2U);
    EXPECT_EQ(task->acceptance_kind, cppapplab::AcceptanceKind::CMakeGoogleTest);
    EXPECT_EQ(task->source_files.size(), 3U);
    EXPECT_EQ(task->expected_test_count, 9U);
    ASSERT_EQ(task->prerequisites.size(), 1U);
    EXPECT_EQ(task->prerequisites.front(), "w1.hello-world");
}

TEST(TaskCatalogTest, ExposesPersistentTodoCliExercise) {
    const cppapplab::Task* task = cppapplab::find_task("w2.todo-cli");

    ASSERT_NE(task, nullptr);
    EXPECT_EQ(task->week, 2U);
    EXPECT_EQ(task->order, 1U);
    EXPECT_EQ(task->acceptance_kind, cppapplab::AcceptanceKind::CMakeGoogleTest);
    EXPECT_EQ(task->source_files.size(), 2U);
    EXPECT_EQ(task->expected_test_count, 19U);
    ASSERT_EQ(task->prerequisites.size(), 1U);
    EXPECT_EQ(task->prerequisites.front(), "w1.calculator");
}

TEST(TaskCatalogTest, ExposesTaskManagerRefactorExercise) {
    const cppapplab::Task* task = cppapplab::find_task("w3.task-manager");

    ASSERT_NE(task, nullptr);
    EXPECT_EQ(task->week, 3U);
    EXPECT_EQ(task->order, 1U);
    EXPECT_EQ(task->acceptance_kind, cppapplab::AcceptanceKind::CMakeGoogleTest);
    EXPECT_EQ(task->source_files.size(), 3U);
    EXPECT_EQ(task->expected_test_count, 31U);
    ASSERT_EQ(task->prerequisites.size(), 1U);
    EXPECT_EQ(task->prerequisites.front(), "w2.todo-cli");
}

TEST(TaskCatalogTest, ExposesTaskManagerQualityGateExercise) {
    const cppapplab::Task* task = cppapplab::find_task("w4.quality-gate");

    ASSERT_NE(task, nullptr);
    EXPECT_EQ(task->week, 4U);
    EXPECT_EQ(task->order, 1U);
    EXPECT_EQ(task->acceptance_kind, cppapplab::AcceptanceKind::CMakeGoogleTest);
    EXPECT_EQ(task->source_files.size(), 4U);
    EXPECT_EQ(task->expected_test_count, 41U);
    ASSERT_EQ(task->prerequisites.size(), 1U);
    EXPECT_EQ(task->prerequisites.front(), "w3.task-manager");
}

TEST(TaskCatalogTest, ExposesLinuxSystemProgrammingLab) {
    const cppapplab::Task* task = cppapplab::find_task("w5.system-lab");

    ASSERT_NE(task, nullptr);
    EXPECT_EQ(task->week, 5U);
    EXPECT_EQ(task->order, 1U);
    EXPECT_EQ(task->acceptance_kind, cppapplab::AcceptanceKind::CMakeGoogleTest);
    EXPECT_EQ(task->source_files.size(), 5U);
    EXPECT_EQ(task->expected_test_count, 31U);
    ASSERT_EQ(task->prerequisites.size(), 1U);
    EXPECT_EQ(task->prerequisites.front(), "w4.quality-gate");
}

TEST(TaskCatalogTest, ExposesThreadPoolAndFuturesExercise) {
    const cppapplab::Task* task = cppapplab::find_task("w6.thread-pool");
    ASSERT_NE(task, nullptr);
    EXPECT_EQ(task->week, 6U);
    EXPECT_EQ(task->order, 1U);
    EXPECT_EQ(task->acceptance_kind, cppapplab::AcceptanceKind::CMakeGoogleTest);
    EXPECT_EQ(task->source_files.size(), 4U);
    EXPECT_EQ(task->expected_test_count, 34U);
    ASSERT_EQ(task->prerequisites.size(), 1U);
    EXPECT_EQ(task->prerequisites.front(), "w5.system-lab");
}

TEST(TaskCatalogTest, ExposesTcpEchoServerExercise) {
    const cppapplab::Task* task = cppapplab::find_task("w7.tcp-echo");
    ASSERT_NE(task, nullptr);
    EXPECT_EQ(task->week, 7U);
    EXPECT_EQ(task->order, 1U);
    EXPECT_EQ(task->acceptance_kind, cppapplab::AcceptanceKind::CMakeGoogleTest);
    EXPECT_EQ(task->source_files.size(), 4U);
    EXPECT_EQ(task->expected_test_count, 32U);
    ASSERT_EQ(task->prerequisites.size(), 1U);
    EXPECT_EQ(task->prerequisites.front(), "w6.thread-pool");
}

TEST(TaskCatalogTest, ExposesMultiClientEpollChatExercise) {
    const cppapplab::Task* task = cppapplab::find_task("w7.tcp-chat");
    ASSERT_NE(task, nullptr);
    EXPECT_EQ(task->week, 7U);
    EXPECT_EQ(task->order, 2U);
    EXPECT_EQ(task->acceptance_kind, cppapplab::AcceptanceKind::CMakeGoogleTest);
    EXPECT_EQ(task->source_files.size(), 4U);
    EXPECT_EQ(task->expected_test_count, 37U);
    ASSERT_EQ(task->prerequisites.size(), 1U);
    EXPECT_EQ(task->prerequisites.front(), "w7.tcp-echo");
}

TEST(TaskCatalogTest, ExposesHttpProtocolFoundationsExercise) {
    const cppapplab::Task* task = cppapplab::find_task("w8.http-protocol");
    ASSERT_NE(task, nullptr);
    EXPECT_EQ(task->week, 8U);
    EXPECT_EQ(task->order, 1U);
    EXPECT_EQ(task->acceptance_kind, cppapplab::AcceptanceKind::CMakeGoogleTest);
    EXPECT_EQ(task->source_files.size(), 4U);
    EXPECT_EQ(task->expected_test_count, 33U);
    ASSERT_EQ(task->prerequisites.size(), 1U);
    EXPECT_EQ(task->prerequisites.front(), "w7.tcp-chat");
}

TEST(TaskCatalogTest, ExposesIncrementalHttpRequestParserExercise) {
    const cppapplab::Task* task = cppapplab::find_task("w8.http-parser");
    ASSERT_NE(task, nullptr);
    EXPECT_EQ(task->week, 8U);
    EXPECT_EQ(task->order, 2U);
    EXPECT_EQ(task->acceptance_kind, cppapplab::AcceptanceKind::CMakeGoogleTest);
    EXPECT_EQ(task->source_files.size(), 5U);
    EXPECT_EQ(task->expected_test_count, 39U);
    ASSERT_EQ(task->prerequisites.size(), 1U);
    EXPECT_EQ(task->prerequisites.front(), "w8.http-protocol");
}

TEST(TaskCatalogTest, ExposesHttpStaticFilesExercise) {
    const cppapplab::Task* task = cppapplab::find_task("w8.static-files");
    ASSERT_NE(task, nullptr);
    EXPECT_EQ(task->week, 8U);
    EXPECT_EQ(task->order, 3U);
    EXPECT_EQ(task->acceptance_kind, cppapplab::AcceptanceKind::CMakeGoogleTest);
    EXPECT_EQ(task->source_files.size(), 6U);
    EXPECT_EQ(task->expected_test_count, 31U);
    ASSERT_EQ(task->prerequisites.size(), 1U);
    EXPECT_EQ(task->prerequisites.front(), "w8.http-parser");
}

TEST(TaskCatalogTest, ExposesHttpTcpServerExercise) {
    const cppapplab::Task* task = cppapplab::find_task("w8.http-server");
    ASSERT_NE(task, nullptr);
    EXPECT_EQ(task->week, 8U);
    EXPECT_EQ(task->order, 5U);
    EXPECT_EQ(task->acceptance_kind, cppapplab::AcceptanceKind::CMakeGoogleTest);
    EXPECT_EQ(task->source_files.size(), 4U);
    EXPECT_EQ(task->expected_test_count, 34U);
    ASSERT_EQ(task->prerequisites.size(), 1U);
    EXPECT_EQ(task->prerequisites.front(), "w8.router");
}

TEST(TaskCatalogTest, ExposesHttpConnectionPolicyExercise) {
    const cppapplab::Task* task = cppapplab::find_task("w8.http-connection-policy");
    ASSERT_NE(task, nullptr);
    EXPECT_EQ(task->week, 8U);
    EXPECT_EQ(task->order, 6U);
    EXPECT_EQ(task->source_files.size(), 2U);
    EXPECT_EQ(task->expected_test_count, 14U);
    ASSERT_EQ(task->prerequisites.size(), 1U);
    EXPECT_EQ(task->prerequisites.front(), "w8.http-server");
}

TEST(TaskCatalogTest, ExposesHttpKeepAliveExercise) {
    const cppapplab::Task* task = cppapplab::find_task("w8.http-keep-alive");
    ASSERT_NE(task, nullptr);
    EXPECT_EQ(task->week, 8U);
    EXPECT_EQ(task->order, 7U);
    EXPECT_EQ(task->source_files.size(), 2U);
    EXPECT_EQ(task->expected_test_count, 8U);
    ASSERT_EQ(task->prerequisites.size(), 1U);
    EXPECT_EQ(task->prerequisites.front(), "w8.http-connection-policy");
}

TEST(TaskCatalogTest, ExposesHttpConnectionGuardsExercise) {
    const cppapplab::Task* task = cppapplab::find_task("w8.http-connection-guards");
    ASSERT_NE(task, nullptr);
    EXPECT_EQ(task->week, 8U);
    EXPECT_EQ(task->order, 8U);
    EXPECT_EQ(task->source_files.size(), 2U);
    EXPECT_EQ(task->expected_test_count, 9U);
    ASSERT_EQ(task->prerequisites.size(), 1U);
    EXPECT_EQ(task->prerequisites.front(), "w8.http-keep-alive");
}

TEST(TaskCatalogTest, ExposesHttpGracefulShutdownExercise) {
    const cppapplab::Task* task = cppapplab::find_task("w8.http-graceful-shutdown");
    ASSERT_NE(task, nullptr);
    EXPECT_EQ(task->week, 8U);
    EXPECT_EQ(task->order, 9U);
    EXPECT_EQ(task->source_files.size(), 2U);
    EXPECT_EQ(task->expected_test_count, 5U);
    ASSERT_EQ(task->prerequisites.size(), 1U);
    EXPECT_EQ(task->prerequisites.front(), "w8.http-connection-guards");
}

TEST(TaskCatalogTest, ExposesHttpObservabilityExercise) {
    const cppapplab::Task* task = cppapplab::find_task("w8.http-observability");
    ASSERT_NE(task, nullptr);
    EXPECT_EQ(task->week, 8U);
    EXPECT_EQ(task->order, 10U);
    EXPECT_EQ(task->source_files.size(), 2U);
    EXPECT_EQ(task->expected_test_count, 8U);
    ASSERT_EQ(task->prerequisites.size(), 1U);
    EXPECT_EQ(task->prerequisites.front(), "w8.http-graceful-shutdown");
}

TEST(TaskCatalogTest, ExposesHttpOperationalLoadProbeExercise) {
    const cppapplab::Task* task = cppapplab::find_task("w8.http-operational-load");
    ASSERT_NE(task, nullptr);
    EXPECT_EQ(task->week, 8U);
    EXPECT_EQ(task->order, 11U);
    EXPECT_EQ(task->source_files.size(), 2U);
    EXPECT_EQ(task->expected_test_count, 5U);
    ASSERT_EQ(task->prerequisites.size(), 1U);
    EXPECT_EQ(task->prerequisites.front(), "w8.http-observability");
}

TEST(TaskCatalogTest, UnknownTaskDoesNotResolve) {
    EXPECT_EQ(cppapplab::find_task("w99.unknown"), nullptr);
}
