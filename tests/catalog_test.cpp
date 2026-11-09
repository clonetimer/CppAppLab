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

TEST(TaskCatalogTest, UnknownTaskDoesNotResolve) {
    EXPECT_EQ(cppapplab::find_task("w99.unknown"), nullptr);
}
