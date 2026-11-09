#include "cppapplab/task.hpp"

#include <gtest/gtest.h>

TEST(TaskCatalogTest, ExposesFirstHelloWorldExercise) {
    const cppapplab::Task* task = cppapplab::find_task("w1.hello-world");

    ASSERT_NE(task, nullptr);
    EXPECT_EQ(task->week, 1U);
    EXPECT_EQ(task->order, 1U);
    EXPECT_EQ(task->acceptance_kind, cppapplab::AcceptanceKind::ExactStdout);
    EXPECT_EQ(task->source_files.size(), 1U);
}

TEST(TaskCatalogTest, ExposesMultiFileCalculatorExercise) {
    const cppapplab::Task* task = cppapplab::find_task("w1.calculator");

    ASSERT_NE(task, nullptr);
    EXPECT_EQ(task->week, 1U);
    EXPECT_EQ(task->order, 2U);
    EXPECT_EQ(task->acceptance_kind, cppapplab::AcceptanceKind::CMakeGoogleTest);
    EXPECT_EQ(task->source_files.size(), 3U);
    EXPECT_EQ(task->expected_unit_test_count, 6U);
}

TEST(TaskCatalogTest, UnknownTaskDoesNotResolve) {
    EXPECT_EQ(cppapplab::find_task("w99.unknown"), nullptr);
}
