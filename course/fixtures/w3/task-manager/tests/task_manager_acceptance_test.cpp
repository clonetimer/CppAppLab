#include "task_manager.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <sstream>
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
                ("cppapplab-task-manager-test-" + std::to_string(stamp));
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
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
        throw std::runtime_error("Cannot create test data file.");
    }
    output.write(content.data(), static_cast<std::streamsize>(content.size()));
    output.close();
    if (!output) {
        throw std::runtime_error("Cannot write test data file.");
    }
}

}  // namespace

TEST(TaskAcceptanceTest, TaskStoresItsValueState) {
    const cppapplab::tasks::Task task(4U, "review ownership", true);

    EXPECT_EQ(task.id(), 4U);
    EXPECT_EQ(task.title(), "review ownership");
    EXPECT_TRUE(task.completed());
}

TEST(TaskAcceptanceTest, TaskRejectsZeroId) {
    EXPECT_THROW(cppapplab::tasks::Task(0U, "invalid"), std::invalid_argument);
}

TEST(TaskAcceptanceTest, TaskRejectsWhitespaceOnlyTitle) {
    EXPECT_THROW(cppapplab::tasks::Task(1U, " \t\r\n"), std::invalid_argument);
}

TEST(TaskAcceptanceTest, TaskRejectsOversizedTitle) {
    EXPECT_THROW(cppapplab::tasks::Task(1U, std::string(4097U, 'x')), std::invalid_argument);
}

TEST(TaskAcceptanceTest, TaskEqualityUsesStableIdentity) {
    const cppapplab::tasks::Task first(7U, "first title");
    const cppapplab::tasks::Task same_id(7U, "different title");
    const cppapplab::tasks::Task different_id(8U, "first title");

    EXPECT_EQ(first, same_id);
    EXPECT_FALSE(first == different_id);
}

TEST(TaskAcceptanceTest, TaskOrderingUsesId) {
    const cppapplab::tasks::Task lower(2U, "lower");
    const cppapplab::tasks::Task higher(9U, "higher");

    EXPECT_LT(lower, higher);
    EXPECT_FALSE(higher < lower);
}

TEST(TaskAcceptanceTest, StreamOutputUsesTheCliTaskFormat) {
    const cppapplab::tasks::Task task(3U, "finish RAII", true);
    std::ostringstream output;

    output << task;

    EXPECT_EQ(output.str(), "#3 [x] finish RAII");
}

TEST(TaskAcceptanceTest, ManagerAddsAndFindsOwnedTasks) {
    cppapplab::tasks::TaskManager manager;

    ASSERT_TRUE(manager.add(5U, "first"));
    const auto* found = manager.find(5U);

    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->title(), "first");
    EXPECT_EQ(manager.size(), 1U);
    EXPECT_FALSE(manager.empty());
}

TEST(TaskAcceptanceTest, ManagerRejectsInvalidIdsAndTitles) {
    cppapplab::tasks::TaskManager manager;

    EXPECT_EQ(manager.add(0U, "valid title").error, cppapplab::tasks::Error::InvalidId);
    EXPECT_EQ(manager.add(1U, "").error, cppapplab::tasks::Error::InvalidTitle);
    EXPECT_EQ(manager.add(2U, " \t\r\n").error, cppapplab::tasks::Error::InvalidTitle);
    EXPECT_TRUE(manager.empty());
}

TEST(TaskAcceptanceTest, ManagerRejectsDuplicateIds) {
    cppapplab::tasks::TaskManager manager;
    ASSERT_TRUE(manager.add(5U, "first"));

    const auto result = manager.add(5U, "duplicate");

    EXPECT_EQ(result.error, cppapplab::tasks::Error::DuplicateId);
    ASSERT_EQ(manager.size(), 1U);
    EXPECT_EQ(manager.find(5U)->title(), "first");
}

TEST(TaskAcceptanceTest, ManagerMarksAnExistingTaskDone) {
    cppapplab::tasks::TaskManager manager;
    ASSERT_TRUE(manager.add(2U, "finish task"));

    ASSERT_TRUE(manager.mark_done(2U));

    ASSERT_NE(manager.find(2U), nullptr);
    EXPECT_TRUE(manager.find(2U)->completed());
}

TEST(TaskAcceptanceTest, ManagerReportsMissingTaskWhenMarkingDone) {
    cppapplab::tasks::TaskManager manager;

    const auto result = manager.mark_done(99U);

    EXPECT_EQ(result.error, cppapplab::tasks::Error::NotFound);
}

TEST(TaskAcceptanceTest, ManagerRemovesAndReleasesAnOwnedTask) {
    cppapplab::tasks::TaskManager manager;
    ASSERT_TRUE(manager.add(2U, "remove task"));

    ASSERT_TRUE(manager.remove(2U));

    EXPECT_TRUE(manager.empty());
    EXPECT_EQ(manager.find(2U), nullptr);
}

TEST(TaskAcceptanceTest, ManagerReportsMissingTaskWhenRemoving) {
    cppapplab::tasks::TaskManager manager;

    const auto result = manager.remove(99U);

    EXPECT_EQ(result.error, cppapplab::tasks::Error::NotFound);
}

TEST(TaskAcceptanceTest, NextIdHandlesGapsAndExhaustedIdSpace) {
    cppapplab::tasks::TaskManager manager;
    ASSERT_TRUE(manager.add(5U, "five"));
    ASSERT_TRUE(manager.add(2U, "two"));
    EXPECT_EQ(manager.next_id(), 6U);

    cppapplab::tasks::TaskManager exhausted;
    ASSERT_TRUE(exhausted.add(std::numeric_limits<std::uint64_t>::max(), "last"));
    EXPECT_EQ(exhausted.next_id(), 0U);
}

TEST(TaskAcceptanceTest, ConstLookupReturnsAConstNonOwningPointer) {
    cppapplab::tasks::TaskManager manager;
    ASSERT_TRUE(manager.add(4U, "const lookup"));
    const cppapplab::tasks::TaskManager& const_manager = manager;

    const cppapplab::tasks::Task* found = const_manager.find(4U);

    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->title(), "const lookup");
}

TEST(TaskAcceptanceTest, CopyConstructorDeepCopiesOwnedTasks) {
    cppapplab::tasks::TaskManager original;
    ASSERT_TRUE(original.add(1U, "original"));

    cppapplab::tasks::TaskManager copied(original);

    ASSERT_NE(original.find(1U), nullptr);
    ASSERT_NE(copied.find(1U), nullptr);
    EXPECT_NE(original.find(1U), copied.find(1U));
    ASSERT_TRUE(copied.mark_done(1U));
    EXPECT_FALSE(original.find(1U)->completed());
    EXPECT_TRUE(copied.find(1U)->completed());
}

TEST(TaskAcceptanceTest, CopyAssignmentDeepCopiesAndReplacesTargetState) {
    cppapplab::tasks::TaskManager source;
    ASSERT_TRUE(source.add(1U, "source"));
    cppapplab::tasks::TaskManager target;
    ASSERT_TRUE(target.add(9U, "old target"));

    target = source;

    ASSERT_EQ(target.size(), 1U);
    EXPECT_EQ(target.find(1U)->title(), "source");
    EXPECT_EQ(target.find(9U), nullptr);
    EXPECT_NE(target.find(1U), source.find(1U));
    ASSERT_TRUE(target.mark_done(1U));
    EXPECT_FALSE(source.find(1U)->completed());
}

TEST(TaskAcceptanceTest, CopyAssignmentHandlesSelfAssignment) {
    cppapplab::tasks::TaskManager manager;
    ASSERT_TRUE(manager.add(1U, "keep"));
    cppapplab::tasks::TaskManager& same = manager;

    manager = same;

    ASSERT_EQ(manager.size(), 1U);
    EXPECT_EQ(manager.find(1U)->title(), "keep");
}

TEST(TaskAcceptanceTest, MoveConstructorTransfersOwnershipAndEmptiesSource) {
    cppapplab::tasks::TaskManager source;
    ASSERT_TRUE(source.add(1U, "move me"));

    cppapplab::tasks::TaskManager moved(std::move(source));

    EXPECT_TRUE(source.empty());
    ASSERT_EQ(moved.size(), 1U);
    EXPECT_EQ(moved.find(1U)->title(), "move me");
}

TEST(TaskAcceptanceTest, MoveAssignmentTransfersOwnershipAndEmptiesSource) {
    cppapplab::tasks::TaskManager source;
    ASSERT_TRUE(source.add(2U, "move source"));
    cppapplab::tasks::TaskManager target;
    ASSERT_TRUE(target.add(9U, "discard target"));

    target = std::move(source);

    EXPECT_TRUE(source.empty());
    ASSERT_EQ(target.size(), 1U);
    EXPECT_EQ(target.find(2U)->title(), "move source");
    EXPECT_EQ(target.find(9U), nullptr);
}

TEST(TaskAcceptanceTest, MoveAssignmentHandlesAliasedSelfMove) {
    cppapplab::tasks::TaskManager manager;
    ASSERT_TRUE(manager.add(3U, "keep after self move"));
    cppapplab::tasks::TaskManager& same = manager;

    manager = std::move(same);

    ASSERT_EQ(manager.size(), 1U);
    EXPECT_EQ(manager.find(3U)->title(), "keep after self move");
}

TEST(TaskAcceptanceTest, SnapshotReturnsValueCopiesOrderedById) {
    cppapplab::tasks::TaskManager manager;
    ASSERT_TRUE(manager.add(8U, "eight"));
    ASSERT_TRUE(manager.add(2U, "two"));
    ASSERT_TRUE(manager.add(5U, "five"));

    const auto items = manager.snapshot();

    ASSERT_EQ(items.size(), 3U);
    EXPECT_EQ(items[0].id(), 2U);
    EXPECT_EQ(items[1].id(), 5U);
    EXPECT_EQ(items[2].id(), 8U);
}

TEST(TaskAcceptanceTest, SaveAndLoadPreserveEscapedTitlesAndCompletion) {
    TemporaryDirectory temporary;
    const auto path = temporary.path() / "nested" / "todos.tsv";
    const std::string title = "review\tcopy\nand move \\ ownership\r";
    cppapplab::tasks::TaskManager original;
    ASSERT_TRUE(original.add(3U, title));
    ASSERT_TRUE(original.add(7U, "completed"));
    ASSERT_TRUE(original.mark_done(7U));

    ASSERT_TRUE(original.save(path));

    cppapplab::tasks::TaskManager loaded;
    ASSERT_TRUE(loaded.load(path));
    const auto items = loaded.snapshot();

    ASSERT_EQ(items.size(), 2U);
    EXPECT_EQ(items[0].id(), 3U);
    EXPECT_EQ(items[0].title(), title);
    EXPECT_FALSE(items[0].completed());
    EXPECT_EQ(items[1].id(), 7U);
    EXPECT_TRUE(items[1].completed());
}

TEST(TaskAcceptanceTest, LoadsExistingW2TodoFileWithoutMigration) {
    TemporaryDirectory temporary;
    const auto path = temporary.path() / "w2-todos.tsv";
    write_file(path,
               "CPPAPPLAB_TODO_V1\n"
               "4\t0\tcreated by W2\n"
               "9\t1\tlegacy completed task\n");
    cppapplab::tasks::TaskManager manager;

    ASSERT_TRUE(manager.load(path));

    const auto items = manager.snapshot();
    ASSERT_EQ(items.size(), 2U);
    EXPECT_EQ(items[0].id(), 4U);
    EXPECT_EQ(items[0].title(), "created by W2");
    EXPECT_FALSE(items[0].completed());
    EXPECT_EQ(items[1].id(), 9U);
    EXPECT_TRUE(items[1].completed());
}

TEST(TaskAcceptanceTest, MissingFileLoadsAsAnEmptyManager) {
    TemporaryDirectory temporary;
    cppapplab::tasks::TaskManager manager;
    ASSERT_TRUE(manager.add(8U, "old item"));

    ASSERT_TRUE(manager.load(temporary.path() / "not-created.tsv"));

    EXPECT_TRUE(manager.empty());
}

TEST(TaskAcceptanceTest, MalformedLoadPreservesTheExistingManager) {
    TemporaryDirectory temporary;
    const auto path = temporary.path() / "malformed.tsv";
    write_file(path,
               "CPPAPPLAB_TODO_V1\n"
               "1\t0\tvalid row before failure\n"
               "this row is malformed\n");
    cppapplab::tasks::TaskManager manager;
    ASSERT_TRUE(manager.add(9U, "keep current state"));

    const auto result = manager.load(path);

    EXPECT_EQ(result.error, cppapplab::tasks::Error::InvalidFormat);
    ASSERT_EQ(manager.size(), 1U);
    EXPECT_EQ(manager.find(9U)->title(), "keep current state");
}

TEST(TaskAcceptanceTest, DuplicateIdsDuringLoadPreserveTheExistingManager) {
    TemporaryDirectory temporary;
    const auto path = temporary.path() / "duplicate.tsv";
    write_file(path,
               "CPPAPPLAB_TODO_V1\n"
               "1\t0\tfirst\n"
               "1\t1\tduplicate\n");
    cppapplab::tasks::TaskManager manager;
    ASSERT_TRUE(manager.add(9U, "keep current state"));

    const auto result = manager.load(path);

    EXPECT_EQ(result.error, cppapplab::tasks::Error::InvalidFormat);
    ASSERT_EQ(manager.size(), 1U);
    EXPECT_EQ(manager.find(9U)->title(), "keep current state");
}

TEST(TaskAcceptanceTest, LoadRejectsSymlinkedDataFile) {
    TemporaryDirectory temporary;
    const auto target = temporary.path() / "target.tsv";
    const auto link = temporary.path() / "linked.tsv";
    write_file(target, "CPPAPPLAB_TODO_V1\n");
    std::filesystem::create_symlink(target, link);
    cppapplab::tasks::TaskManager manager;

    const auto result = manager.load(link);

    EXPECT_EQ(result.error, cppapplab::tasks::Error::IoError);
}

TEST(TaskAcceptanceTest, SaveRefusesSymlinkedDestination) {
    TemporaryDirectory temporary;
    const auto target = temporary.path() / "target.tsv";
    const auto link = temporary.path() / "linked.tsv";
    write_file(target, "do not overwrite\n");
    std::filesystem::create_symlink(target, link);
    cppapplab::tasks::TaskManager manager;
    ASSERT_TRUE(manager.add(1U, "new data"));

    const auto result = manager.save(link);

    EXPECT_EQ(result.error, cppapplab::tasks::Error::IoError);
    std::ifstream input(target, std::ios::binary);
    std::string content;
    std::getline(input, content);
    EXPECT_EQ(content, "do not overwrite");
}
