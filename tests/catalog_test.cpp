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

TEST(TaskCatalogTest, UnknownTaskDoesNotResolve) {
    EXPECT_EQ(cppapplab::find_task("w99.unknown"), nullptr);
}
