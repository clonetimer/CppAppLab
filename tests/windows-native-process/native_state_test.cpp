#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <winioctl.h>

#include "cppapplab/process.hpp"
#include "cppapplab/progress.hpp"
#include "cppapplab/task.hpp"
#include "cppapplab/workspace.hpp"
#include "cppapplab_windows_state_config.hpp"
#include "windows_state_io.hpp"

#include <gtest/gtest.h>

#include <array>
#include <chrono>
#include <cstddef>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <future>
#include <iterator>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

namespace fs = std::filesystem;
namespace io = cppapplab::windows_state_io;
using namespace std::chrono_literals;

class Scratch {
   public:
    Scratch() {
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        root_ = fs::temp_directory_path() / ("cppapplab native state " + std::to_string(stamp));
        if (!fs::create_directory(root_)) {
            throw std::runtime_error("Cannot create test directory.");
        }
    }
    ~Scratch() {
        std::error_code ignored;
        fs::remove_all(root_, ignored);
    }
    Scratch(const Scratch&) = delete;
    Scratch& operator=(const Scratch&) = delete;
    [[nodiscard]] const fs::path& root() const noexcept { return root_; }

   private:
    fs::path root_;
};

class ScopedState {
   public:
    explicit ScopedState(const fs::path& path)
        : previous_(io::environment_path(L"CPPAPPLAB_STATE_DIR")) {
        if (!SetEnvironmentVariableW(L"CPPAPPLAB_STATE_DIR", path.c_str())) {
            throw std::runtime_error("Cannot set isolated state path.");
        }
    }
    ~ScopedState() {
        (void)SetEnvironmentVariableW(L"CPPAPPLAB_STATE_DIR",
                                     previous_ ? previous_->c_str() : nullptr);
    }
    ScopedState(const ScopedState&) = delete;
    ScopedState& operator=(const ScopedState&) = delete;

   private:
    std::optional<fs::path> previous_;
};

// A real NTFS junction does not need Windows symlink privilege. If the runner
// cannot create it, the security test fails rather than silently skipping.
class Junction {
   public:
    Junction(const fs::path& link, fs::path target) : link_(link) {
        target.make_preferred();
        const std::wstring substitute = L"\\??\\" + target.native();
        const auto print = target.native();
        struct MountPoint {
            DWORD tag;
            WORD data_length;
            WORD reserved;
            WORD substitute_offset;
            WORD substitute_length;
            WORD print_offset;
            WORD print_length;
            wchar_t paths[1];
        };
        const auto bytes = (substitute.size() + print.size() + 2U) * sizeof(wchar_t);
        std::vector<unsigned char> buffer(offsetof(MountPoint, paths) + bytes, 0U);
        auto* data = reinterpret_cast<MountPoint*>(buffer.data());
        data->tag = IO_REPARSE_TAG_MOUNT_POINT;
        data->data_length = static_cast<WORD>(buffer.size() - 8U);
        data->substitute_length = static_cast<WORD>(substitute.size() * sizeof(wchar_t));
        data->print_offset = static_cast<WORD>(data->substitute_length + sizeof(wchar_t));
        data->print_length = static_cast<WORD>(print.size() * sizeof(wchar_t));
        std::memcpy(buffer.data() + offsetof(MountPoint, paths),
                    substitute.data(), data->substitute_length);
        std::memcpy(buffer.data() + offsetof(MountPoint, paths) + data->print_offset,
                    print.data(), data->print_length);
        if (!CreateDirectoryW(link_.c_str(), nullptr)) {
            throw std::runtime_error("Cannot create junction directory.");
        }
        const HANDLE handle = CreateFileW(link_.c_str(), GENERIC_WRITE, 0U, nullptr,
                                           OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS |
                                               FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
        if (handle == INVALID_HANDLE_VALUE) {
            (void)RemoveDirectoryW(link_.c_str());
            throw std::runtime_error("Cannot open junction directory.");
        }
        DWORD returned = 0;
        const BOOL ok = DeviceIoControl(handle, FSCTL_SET_REPARSE_POINT, buffer.data(),
                                        static_cast<DWORD>(buffer.size()), nullptr, 0U,
                                        &returned, nullptr);
        const DWORD error = GetLastError();
        (void)CloseHandle(handle);
        if (!ok) {
            (void)RemoveDirectoryW(link_.c_str());
            throw std::runtime_error("Cannot create real junction: " + std::to_string(error));
        }
    }
    ~Junction() { (void)RemoveDirectoryW(link_.c_str()); }
    Junction(const Junction&) = delete;
    Junction& operator=(const Junction&) = delete;

   private:
    fs::path link_;
};

void edit_source(const fs::path& path, const std::string& suffix) {
    std::ofstream out(path, std::ios::binary | std::ios::app);
    out << suffix;
    out.close();
    if (!out) {
        throw std::runtime_error("Cannot edit test source.");
    }
}

cppapplab::Evidence save_attempt(const cppapplab::Workspace& workspace, bool passed) {
    cppapplab::Evidence evidence;
    evidence.task_id = workspace.task->id;
    evidence.fingerprint = cppapplab::source_fingerprint(*workspace.task, workspace.root);
    evidence.passed = passed;
    evidence.tests_passed = passed ? 1U : 0U;
    evidence.tests_total = 1U;
    evidence.timestamp = "2026-11-27T00:00:00Z";
    cppapplab::write_evidence(workspace.root, evidence, "native storage fixture only\n");
    cppapplab::write_progress(workspace.root, evidence);
    return evidence;
}

TEST(NativeStateIO, ReplacesRecordsAndPreservesBinaryBytesInWidePaths) {
    Scratch scratch;
    const auto directory = scratch.root() / L"\u5b66\u4e60 state";
    io::ensure_directory(directory);
    const auto path = directory / "record";
    EXPECT_FALSE(io::regular_exists(path));
    const std::string binary("a\r\n\0b", 5U);
    io::atomic_write(path, binary);
    EXPECT_TRUE(io::regular_exists(path));
    EXPECT_EQ(io::read(path, 5U), binary);
    EXPECT_THROW(static_cast<void>(io::read(path, 4U)), std::runtime_error);
    io::atomic_write(path, "replacement\n");
    EXPECT_EQ(io::read(path, 32U), "replacement\n");
    EXPECT_EQ(std::distance(fs::directory_iterator(directory), fs::directory_iterator{}), 1);
}

TEST(NativeStateIO, RejectsActualJunctionAncestorsWithoutTouchingTarget) {
    Scratch scratch;
    const auto target = scratch.root() / "target";
    io::ensure_directory(target);
    io::atomic_write(target / "record", "unchanged");
    const auto link = scratch.root() / "junction";
    Junction junction(link, target);
    EXPECT_THROW(io::ensure_directory(link / "new"), std::runtime_error);
    EXPECT_THROW(static_cast<void>(io::regular_exists(link / "record")), std::runtime_error);
    EXPECT_THROW(static_cast<void>(io::read(link / "record", 32U)), std::runtime_error);
    EXPECT_THROW(io::atomic_write(link / "record", "modified"), std::runtime_error);
    EXPECT_THROW(io::append(link / "record", "modified", 64U), std::runtime_error);
    EXPECT_EQ(io::read(target / "record", 32U), "unchanged");
    EXPECT_FALSE(fs::exists(target / "new"));
}

TEST(NativeStateIO, RejectsHardLinksAndAlternateStreams) {
    Scratch scratch;
    const auto target = scratch.root() / "target";
    io::atomic_write(target, "unchanged");
    const auto alias = scratch.root() / "alias";
    ASSERT_TRUE(CreateHardLinkW(alias.c_str(), target.c_str(), nullptr)) << GetLastError();
    EXPECT_THROW(io::atomic_write(alias, "modified"), std::runtime_error);
    EXPECT_THROW(io::append(alias, "modified", 64U), std::runtime_error);
    EXPECT_THROW(static_cast<void>(io::read(alias, 64U)), std::runtime_error);
    EXPECT_THROW(io::atomic_write(fs::path(target.native() + L":stream"), "bad"), std::runtime_error);
    EXPECT_TRUE(DeleteFileW(alias.c_str()));
    EXPECT_EQ(io::read(target, 32U), "unchanged");
}

TEST(NativeStateIO, BoundsHistoryAndDoesNotPartiallyAppendOnLimit) {
    Scratch scratch;
    const auto path = scratch.root() / "history";
    io::append(path, "first\n", 12U);
    io::append(path, "other\n", 12U);
    EXPECT_THROW(io::append(path, "x", 12U), std::runtime_error);
    EXPECT_EQ(io::read(path, 12U), "first\nother\n");
}

TEST(NativeStateIO, SerializesIndependentNativeProcessAppends) {
    Scratch scratch;
    const auto path = scratch.root() / L"\u8bb0\u5f55 history";
    constexpr unsigned workers = 4U;
    std::array<std::future<cppapplab::ProcessResult>, workers> futures;
    for (unsigned worker = 0; worker < workers; ++worker) {
        futures[worker] = std::async(std::launch::async, [&, worker] {
            return cppapplab::run_process(
                {CPPAPPLAB_NATIVE_STATE_CHILD, path.u8string(), std::to_string(worker)},
                scratch.root(), 30s, 4096U);
        });
    }
    for (auto& future : futures) {
        const auto result = future.get();
        ASSERT_FALSE(result.timed_out) << result.output;
        ASSERT_EQ(result.exit_code, 0) << result.output;
    }
    std::istringstream input(io::read(path, 1024U * 1024U));
    std::set<std::string> observed;
    std::string line;
    unsigned count = 0;
    while (std::getline(input, line)) {
        ++count;
        EXPECT_TRUE(observed.insert(line).second);
    }
    EXPECT_EQ(count, workers * 20U);
    for (unsigned worker = 0; worker < workers; ++worker) {
        for (unsigned i = 0; i < 20U; ++i) {
            EXPECT_EQ(observed.count(std::to_string(worker) + ":" + std::to_string(i)), 1U);
        }
    }
}

TEST(NativeProgress, RealProgressHistoryFingerprintAndPrerequisiteState) {
    Scratch scratch;
    ScopedState state(scratch.root() / L"\u5b66\u4e60 state");
    const auto* hello = cppapplab::find_task("w1.hello-world");
    const auto* calculator = cppapplab::find_task("w1.calculator");
    ASSERT_NE(hello, nullptr);
    ASSERT_NE(calculator, nullptr);
    EXPECT_EQ(cppapplab::task_catalog().size(), 25U);
    EXPECT_FALSE(cppapplab::read_progress(hello->id).has_value());
    EXPECT_FALSE(cppapplab::prerequisites_satisfied(*calculator));
    const auto workspace = cppapplab::create_workspace(*hello, scratch.root() / L"\u5b66\u5458 workspace");
    cppapplab::register_workspace(workspace.root);
    const auto started = cppapplab::read_progress(hello->id);
    ASSERT_TRUE(started.has_value());
    EXPECT_EQ(started->workspace_root, workspace.root);
    EXPECT_FALSE(started->checked);
    EXPECT_FALSE(cppapplab::has_current_pass(hello->id));

    const auto pass = save_attempt(workspace, true);
    EXPECT_TRUE(cppapplab::has_current_pass(hello->id));
    EXPECT_TRUE(cppapplab::prerequisites_satisfied(*calculator));
    const auto persisted = cppapplab::read_progress(hello->id);
    ASSERT_TRUE(persisted.has_value());
    EXPECT_EQ(persisted->workspace_root, workspace.root);
    EXPECT_EQ(persisted->evidence.fingerprint, pass.fingerprint);

    edit_source(workspace.root / "src/main.cpp", "// different source\n");
    EXPECT_FALSE(cppapplab::has_current_pass(hello->id));
    EXPECT_FALSE(cppapplab::prerequisites_satisfied(*calculator));
    EXPECT_THROW(cppapplab::write_progress(workspace.root, pass), std::runtime_error);
    (void)save_attempt(workspace, false);
    const auto history = cppapplab::read_progress_history(hello->id);
    ASSERT_EQ(history.size(), 2U);
    EXPECT_TRUE(history[0].evidence.passed);
    EXPECT_FALSE(history[1].evidence.passed);
    EXPECT_EQ(history[0].workspace_root, workspace.root);
    EXPECT_EQ(history[1].workspace_root, workspace.root);
    EXPECT_FALSE(cppapplab::has_current_pass(hello->id));
    EXPECT_TRUE(cppapplab::read_evidence(workspace.root).has_value());
}

TEST(NativeProgress, RejectsReparseStateAndEvidenceDirectories) {
    Scratch scratch;
    ScopedState state(scratch.root() / "state");
    const auto* hello = cppapplab::find_task("w1.hello-world");
    ASSERT_NE(hello, nullptr);
    const auto workspace = cppapplab::create_workspace(*hello, scratch.root() / "workspace");
    const auto target = scratch.root() / "outside";
    io::ensure_directory(target);
    io::ensure_directory(scratch.root() / "state");
    {
        Junction progress(scratch.root() / "state" / "progress", target);
        EXPECT_THROW(cppapplab::register_workspace(workspace.root), std::runtime_error);
        EXPECT_THROW(static_cast<void>(cppapplab::read_progress(hello->id)), std::runtime_error);
        EXPECT_FALSE(cppapplab::has_current_pass(hello->id));
    }
    EXPECT_TRUE(fs::is_empty(target));
    {
        Junction evidence(workspace.root / ".cppapplab", target);
        cppapplab::Evidence value;
        value.task_id = hello->id;
        value.fingerprint = cppapplab::source_fingerprint(*hello, workspace.root);
        EXPECT_THROW(cppapplab::write_evidence(workspace.root, value, "unsafe"), std::runtime_error);
        EXPECT_THROW(static_cast<void>(cppapplab::read_evidence(workspace.root)), std::runtime_error);
    }
    EXPECT_TRUE(fs::is_empty(target));
    const auto original_source = workspace.root / "src";
    fs::remove_all(original_source);
    io::atomic_write(target / "main.cpp", "int main() {}");
    {
        Junction source(original_source, target);
        EXPECT_THROW(static_cast<void>(cppapplab::source_fingerprint(*hello, workspace.root)),
                     std::runtime_error);
    }
}

}  // namespace
