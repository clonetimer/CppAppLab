#include "cppapplab/task.hpp"
#include "cppapplab/workspace.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

namespace {

class TemporaryDirectory {
   public:
    TemporaryDirectory() {
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        path_ =
            std::filesystem::temp_directory_path() / ("cppapplab-test-" + std::to_string(stamp));
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

}  // namespace

TEST(WorkspaceTest, CreatesLearnerWorkspaceWithoutReferenceFiles) {
    const cppapplab::Task* task = cppapplab::find_task("w1.hello-world");
    ASSERT_NE(task, nullptr);

    TemporaryDirectory temporary;
    const auto destination = temporary.path() / "learner";

    const auto workspace = cppapplab::create_workspace(*task, destination);

    EXPECT_EQ(workspace.task->id, "w1.hello-world");
    EXPECT_TRUE(std::filesystem::is_regular_file(destination / "src/main.cpp"));
    EXPECT_FALSE(std::filesystem::exists(destination / "reference/main.cpp"));
}

TEST(WorkspaceTest, RefusesToOverwriteExistingWorkspace) {
    const cppapplab::Task* task = cppapplab::find_task("w1.hello-world");
    ASSERT_NE(task, nullptr);

    TemporaryDirectory temporary;
    const auto destination = temporary.path() / "learner";
    std::filesystem::create_directories(destination);
    {
        std::ofstream marker(destination / "keep.txt");
        marker << "keep";
    }

    EXPECT_THROW(cppapplab::create_workspace(*task, destination), std::runtime_error);
    EXPECT_TRUE(std::filesystem::is_regular_file(destination / "keep.txt"));
}

TEST(WorkspaceTest, FingerprintChangesWhenLearnerSourceChanges) {
    const cppapplab::Task* task = cppapplab::find_task("w1.hello-world");
    ASSERT_NE(task, nullptr);

    TemporaryDirectory temporary;
    const auto workspace = cppapplab::create_workspace(*task, temporary.path() / "learner");
    const std::string before = cppapplab::source_fingerprint(*task, workspace.root);

    {
        std::ofstream source(workspace.root / "src/main.cpp", std::ios::app);
        source << "\n// learner edit\n";
    }

    const std::string after = cppapplab::source_fingerprint(*task, workspace.root);
    EXPECT_NE(before, after);
}

TEST(WorkspaceTest, RejectsSymlinkedSourceParent) {
    const cppapplab::Task* task = cppapplab::find_task("w1.hello-world");
    ASSERT_NE(task, nullptr);

    TemporaryDirectory temporary;
    const auto workspace = cppapplab::create_workspace(*task, temporary.path() / "learner");
    const auto external_directory = temporary.path() / "external";
    std::filesystem::create_directory(external_directory);

    std::filesystem::remove_all(workspace.root / "src");
    std::filesystem::create_directory_symlink(external_directory, workspace.root / "src");

    EXPECT_THROW(cppapplab::source_fingerprint(*task, workspace.root), std::runtime_error);
}
