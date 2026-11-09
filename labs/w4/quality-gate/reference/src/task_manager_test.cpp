#include "task_manager.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>

namespace {

class TemporaryDirectory {
public:
    TemporaryDirectory() {
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        path_ = std::filesystem::temp_directory_path() /
                ("cppapplab-w4-reference-test-" + std::to_string(stamp));
        std::filesystem::create_directories(path_);
    }

    ~TemporaryDirectory() {
        std::error_code ignored;
        std::filesystem::remove_all(path_, ignored);
    }

    [[nodiscard]] const std::filesystem::path& path() const noexcept {
        return path_;
    }

private:
    std::filesystem::path path_;
};

void write_file(const std::filesystem::path& path, const std::string& content) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
        throw std::runtime_error("Cannot create reference test data file.");
    }
    output.write(content.data(), static_cast<std::streamsize>(content.size()));
    output.close();
    if (!output) {
        throw std::runtime_error("Cannot write reference test data file.");
    }
}

}  // namespace

TEST(TaskManagerLearnerTest, TaskConstructionRejectsInvalidState) {
    EXPECT_THROW(cppapplab::tasks::Task(0U, "invalid"), std::invalid_argument);
    EXPECT_THROW(cppapplab::tasks::Task(1U, " \t\r\n"), std::invalid_argument);
}

TEST(TaskManagerLearnerTest, TaskEqualityAndOrderingUseIds) {
    const cppapplab::tasks::Task first(2U, "first");
    const cppapplab::tasks::Task same_id(2U, "renamed");
    const cppapplab::tasks::Task later(8U, "later");

    EXPECT_TRUE(first == same_id);
    EXPECT_FALSE(first == later);
    EXPECT_TRUE(first < later);
    EXPECT_FALSE(later < first);
}

TEST(TaskManagerLearnerTest, ManagerAddsAndFindsTasks) {
    cppapplab::tasks::TaskManager manager;

    ASSERT_TRUE(manager.add(4U, "inspect ownership"));
    const auto* found = manager.find(4U);

    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->title(), "inspect ownership");
    EXPECT_EQ(manager.size(), 1U);
}

TEST(TaskManagerLearnerTest, ManagerRejectsInvalidAndDuplicateIds) {
    cppapplab::tasks::TaskManager manager;

    EXPECT_EQ(manager.add(0U, "invalid").error, cppapplab::tasks::Error::InvalidId);
    EXPECT_EQ(manager.add(1U, "").error, cppapplab::tasks::Error::InvalidTitle);
    ASSERT_TRUE(manager.add(2U, "first"));
    EXPECT_EQ(manager.add(2U, "duplicate").error, cppapplab::tasks::Error::DuplicateId);
    EXPECT_EQ(manager.size(), 1U);
}

TEST(TaskManagerLearnerTest, ManagerMarksAndRemovesTasks) {
    cppapplab::tasks::TaskManager manager;
    ASSERT_TRUE(manager.add(3U, "complete this"));

    ASSERT_TRUE(manager.mark_done(3U));
    ASSERT_NE(manager.find(3U), nullptr);
    EXPECT_TRUE(manager.find(3U)->completed());

    ASSERT_TRUE(manager.remove(3U));
    EXPECT_EQ(manager.find(3U), nullptr);
    EXPECT_TRUE(manager.empty());
}

TEST(TaskManagerLearnerTest, NextIdHandlesGapsAndOverflow) {
    cppapplab::tasks::TaskManager manager;
    ASSERT_TRUE(manager.add(2U, "two"));
    ASSERT_TRUE(manager.add(9U, "nine"));
    EXPECT_EQ(manager.next_id(), 10U);

    cppapplab::tasks::TaskManager exhausted;
    ASSERT_TRUE(exhausted.add(std::numeric_limits<std::uint64_t>::max(), "last"));
    EXPECT_EQ(exhausted.next_id(), 0U);
}

TEST(TaskManagerLearnerTest, CopyOperationsAreDeep) {
    cppapplab::tasks::TaskManager original;
    ASSERT_TRUE(original.add(1U, "original"));

    cppapplab::tasks::TaskManager copied(original);
    cppapplab::tasks::TaskManager assigned;
    ASSERT_TRUE(assigned.add(9U, "replace"));
    assigned = original;

    ASSERT_NE(original.find(1U), nullptr);
    ASSERT_NE(copied.find(1U), nullptr);
    ASSERT_NE(assigned.find(1U), nullptr);
    EXPECT_NE(original.find(1U), copied.find(1U));
    EXPECT_NE(original.find(1U), assigned.find(1U));

    ASSERT_TRUE(copied.mark_done(1U));
    ASSERT_TRUE(assigned.remove(1U));
    EXPECT_FALSE(original.find(1U)->completed());
    EXPECT_NE(original.find(1U), nullptr);
}

TEST(TaskManagerLearnerTest, MoveOperationsTransferOwnership) {
    cppapplab::tasks::TaskManager source;
    ASSERT_TRUE(source.add(5U, "move me"));

    cppapplab::tasks::TaskManager moved(std::move(source));

    EXPECT_TRUE(source.empty());
    ASSERT_NE(moved.find(5U), nullptr);
    EXPECT_EQ(moved.find(5U)->title(), "move me");

    cppapplab::tasks::TaskManager destination;
    ASSERT_TRUE(destination.add(99U, "old"));
    destination = std::move(moved);

    EXPECT_TRUE(moved.empty());
    EXPECT_EQ(destination.find(99U), nullptr);
    EXPECT_EQ(destination.find(5U)->title(), "move me");
}

TEST(TaskManagerLearnerTest, SnapshotReturnsOrderedCopies) {
    cppapplab::tasks::TaskManager manager;
    ASSERT_TRUE(manager.add(7U, "seven"));
    ASSERT_TRUE(manager.add(2U, "two"));
    ASSERT_TRUE(manager.add(5U, "five"));

    auto snapshot = manager.snapshot();

    ASSERT_EQ(snapshot.size(), 3U);
    EXPECT_EQ(snapshot[0].id(), 2U);
    EXPECT_EQ(snapshot[1].id(), 5U);
    EXPECT_EQ(snapshot[2].id(), 7U);
    snapshot[0].mark_done();
    EXPECT_FALSE(manager.find(2U)->completed());
}

TEST(TaskManagerLearnerTest, PersistencePreservesW2FormatAndRejectsMalformedData) {
    TemporaryDirectory temporary;
    const auto path = temporary.path() / "nested" / "todos.tsv";
    const std::string title = "tab\tnewline\nslash\\";
    cppapplab::tasks::TaskManager original;
    ASSERT_TRUE(original.add(1U, title));
    ASSERT_TRUE(original.add(3U, "completed"));
    ASSERT_TRUE(original.mark_done(3U));
    ASSERT_TRUE(original.save(path));

    cppapplab::tasks::TaskManager loaded;
    ASSERT_TRUE(loaded.load(path));
    ASSERT_EQ(loaded.size(), 2U);
    EXPECT_EQ(loaded.find(1U)->title(), title);
    EXPECT_TRUE(loaded.find(3U)->completed());

    const auto malformed = temporary.path() / "malformed.tsv";
    write_file(
        malformed,
        "CPPAPPLAB_TODO_V1\n"
        "7\t0\tvalid first row\n"
        "not-a-valid-record\n"
    );
    const auto result = loaded.load(malformed);

    EXPECT_EQ(result.error, cppapplab::tasks::Error::InvalidFormat);
    ASSERT_EQ(loaded.size(), 2U);
    EXPECT_EQ(loaded.find(1U)->title(), title);
    EXPECT_TRUE(loaded.find(3U)->completed());
}
