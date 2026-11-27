#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "windows_state_io.hpp"

#include <windows.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cwctype>
#include <limits>
#include <iterator>
#include <stdexcept>
#include <system_error>
#include <utility>
#include <vector>

namespace cppapplab::windows_state_io {
namespace {

using Path = std::filesystem::path;
using Clock = std::chrono::steady_clock;
constexpr auto kLockTimeout = std::chrono::seconds(5);
constexpr DWORD kChunkBytes = 65536U;

class Handle {
   public:
    explicit Handle(HANDLE value = INVALID_HANDLE_VALUE) noexcept : value_(value) {}
    ~Handle() {
        if (*this) {
            (void)CloseHandle(value_);
        }
    }
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;
    Handle(Handle&& other) noexcept : value_(std::exchange(other.value_, INVALID_HANDLE_VALUE)) {}
    [[nodiscard]] explicit operator bool() const noexcept {
        return value_ != INVALID_HANDLE_VALUE && value_ != nullptr;
    }
    [[nodiscard]] HANDLE get() const noexcept { return value_; }
    void reset() noexcept {
        if (*this) {
            (void)CloseHandle(value_);
        }
        value_ = INVALID_HANDLE_VALUE;
    }

   private:
    HANDLE value_;
};

[[noreturn]] void fail(const char* operation, DWORD error = GetLastError()) {
    throw std::system_error(static_cast<int>(error), std::system_category(), operation);
}

[[nodiscard]] bool missing(DWORD error) noexcept {
    return error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND;
}

void validate_component(const std::wstring& name) {
    if (name.empty() || name == L"." || name == L".." ||
        name.find_first_of(L":<>\"|?*") != std::wstring::npos ||
        name.find(L'\0') != std::wstring::npos || name.back() == L'.' || name.back() == L' ') {
        throw std::runtime_error("Unsafe Windows state path component.");
    }
    // Win32 device aliases may have an extension. Reject their DOS spellings.
    std::wstring stem = name.substr(0, name.find(L'.'));
    for (auto& ch : stem) {
        ch = static_cast<wchar_t>(std::towupper(ch));
    }
    while (!stem.empty() && stem.back() == L' ') {
        stem.pop_back();
    }
    if (stem == L"CON" || stem == L"PRN" || stem == L"AUX" || stem == L"NUL" ||
        stem == L"CONIN$" || stem == L"CONOUT$" ||
        (stem.size() == 4U && (stem.compare(0, 3, L"COM") == 0 ||
                               stem.compare(0, 3, L"LPT") == 0) &&
         ((stem[3] >= L'0' && stem[3] <= L'9') || stem[3] == L'\u00b9' ||
          stem[3] == L'\u00b2' || stem[3] == L'\u00b3'))) {
        throw std::runtime_error("Windows device aliases are not state paths.");
    }
}

[[nodiscard]] Path checked_absolute(const Path& input) {
    if (input.empty() || input.native().find(L'\0') != std::wstring::npos ||
        (input.has_root_name() && !input.is_absolute())) {
        throw std::runtime_error("Invalid Windows state path.");
    }
    Path path = std::filesystem::absolute(input).lexically_normal();
    path.make_preferred();
    const auto drive = path.root_name().native();
    if (drive.size() != 2U || drive[1] != L':' ||
        !((drive[0] >= L'A' && drive[0] <= L'Z') ||
          (drive[0] >= L'a' && drive[0] <= L'z')) ||
        GetDriveTypeW(path.root_path().c_str()) != DRIVE_FIXED) {
        throw std::runtime_error("Windows state must be on a local fixed drive.");
    }
    for (const auto& component : path.relative_path()) {
        validate_component(component.native());
    }
    return path;
}

void validate_handle(HANDLE handle, bool directory) {
    if (GetFileType(handle) != FILE_TYPE_DISK) {
        throw std::runtime_error("State handle is not a disk file or directory.");
    }
    BY_HANDLE_FILE_INFORMATION info{};
    if (!GetFileInformationByHandle(handle, &info)) {
        fail("GetFileInformationByHandle");
    }
    const bool is_directory = (info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0U;
    if ((info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0U ||
        is_directory != directory || (!directory && info.nNumberOfLinks != 1U)) {
        throw std::runtime_error("Unsafe state path: reparse point, hard link or unexpected type.");
    }
}

struct DirectoryChain {
    std::vector<Handle> handles;
    bool exists{true};
};

// Pin each opened directory without FILE_SHARE_DELETE. Open reparse points
// themselves, never their targets. Reject any nonordinary ancestor before
// proceeding. This is local state protection, not a same-user execution sandbox.
[[nodiscard]] DirectoryChain pin_directory(
    const Path& path, bool create,
    DWORD final_access = FILE_READ_ATTRIBUTES | FILE_LIST_DIRECTORY) {
    DirectoryChain result;
    Path current = path.root_path();
    auto open_directory = [&](const Path& directory, bool allow_create, DWORD access) {
        Handle initial(CreateFileW(directory.c_str(), access,
                                   FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
                                   FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr));
        if (initial) {
            validate_handle(initial.get(), true);
            result.handles.push_back(std::move(initial));
            return true;
        }
        const DWORD error = GetLastError();
        if (!missing(error)) {
            fail("Open state directory", error);
        }
        if (!allow_create) {
            return false;
        }
        if (!CreateDirectoryW(directory.c_str(), nullptr)) {
            const DWORD create_error = GetLastError();
            if (create_error != ERROR_ALREADY_EXISTS) {
                fail("CreateDirectoryW", create_error);
            }
        }
        Handle opened(CreateFileW(directory.c_str(), access,
                                  FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
                                  FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr));
        if (!opened) {
            fail("Open created state directory");
        }
        validate_handle(opened.get(), true);
        result.handles.push_back(std::move(opened));
        return true;
    };
    if (!open_directory(current, false, FILE_READ_ATTRIBUTES | FILE_LIST_DIRECTORY)) {
        throw std::runtime_error("State volume root is unavailable.");
    }
    const auto components = path.relative_path();
    std::size_t index = 0U;
    for (const auto& component : components) {
        current /= component;
        ++index;
        const DWORD access = index == static_cast<std::size_t>(
                                         std::distance(components.begin(), components.end()))
                                 ? final_access
                                 : FILE_READ_ATTRIBUTES | FILE_LIST_DIRECTORY;
        if (!open_directory(current, create, access)) {
            result.exists = false;
            break;
        }
    }
    return result;
}

[[nodiscard]] Handle open_file(const Path& path, DWORD access, DWORD share, DWORD disposition) {
    return Handle(CreateFileW(path.c_str(), access, share, nullptr, disposition,
                              FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT, nullptr));
}

[[nodiscard]] bool existing_regular(const Path& path) {
    Handle file = open_file(path, FILE_READ_ATTRIBUTES,
                            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, OPEN_EXISTING);
    if (!file) {
        const DWORD error = GetLastError();
        if (missing(error)) {
            return false;
        }
        fail("Inspect state file", error);
    }
    validate_handle(file.get(), false);
    return true;
}

class ByteLock {
   public:
    ByteLock(HANDLE handle, bool exclusive) : handle_(handle) {
        const auto deadline = Clock::now() + kLockTimeout;
        const DWORD flags = LOCKFILE_FAIL_IMMEDIATELY | (exclusive ? LOCKFILE_EXCLUSIVE_LOCK : 0U);
        while (!LockFileEx(handle_, flags, 0U, MAXDWORD, MAXDWORD, &offset_)) {
            const DWORD error = GetLastError();
            if (error != ERROR_LOCK_VIOLATION || Clock::now() >= deadline) {
                fail("LockFileEx", error);
            }
            Sleep(10U);
        }
    }
    ~ByteLock() {
        (void)UnlockFileEx(handle_, 0U, MAXDWORD, MAXDWORD, &offset_);
    }
    ByteLock(const ByteLock&) = delete;
    ByteLock& operator=(const ByteLock&) = delete;

   private:
    HANDLE handle_;
    OVERLAPPED offset_{};
};

[[nodiscard]] std::uintmax_t file_size(HANDLE handle) {
    LARGE_INTEGER length{};
    if (!GetFileSizeEx(handle, &length)) {
        fail("GetFileSizeEx");
    }
    if (length.QuadPart < 0) {
        throw std::runtime_error("Invalid state file size.");
    }
    return static_cast<std::uintmax_t>(length.QuadPart);
}

void write_all(HANDLE handle, std::string_view data) {
    std::size_t offset = 0;
    while (offset < data.size()) {
        const auto count = static_cast<DWORD>(std::min<std::size_t>(data.size() - offset, kChunkBytes));
        DWORD written = 0;
        if (!WriteFile(handle, data.data() + offset, count, &written, nullptr)) {
            fail("WriteFile(state)");
        }
        if (written == 0U) {
            throw std::runtime_error("No bytes written to state file.");
        }
        offset += written;
    }
}

void flush(HANDLE handle) {
    if (!FlushFileBuffers(handle)) {
        fail("FlushFileBuffers(state)");
    }
}

void discard(HANDLE handle) noexcept {
    FILE_DISPOSITION_INFO info{};
    info.DeleteFile = TRUE;
    (void)SetFileInformationByHandle(handle, FileDispositionInfo, &info, sizeof(info));
}

}  // namespace

std::optional<Path> environment_path(const wchar_t* name) {
    SetLastError(ERROR_SUCCESS);
    const DWORD length = GetEnvironmentVariableW(name, nullptr, 0U);
    if (length == 0U) {
        const DWORD error = GetLastError();
        if (error == ERROR_ENVVAR_NOT_FOUND || error == ERROR_SUCCESS) {
            return std::nullopt;
        }
        fail("GetEnvironmentVariableW", error);
    }
    std::wstring value(length, L'\0');
    const DWORD copied = GetEnvironmentVariableW(name, value.data(), length);
    if (copied == 0U || copied >= length) {
        throw std::runtime_error("State environment variable changed during read.");
    }
    value.resize(copied);
    return Path(value);
}

void ensure_directory(const Path& directory) {
    const auto chain = pin_directory(checked_absolute(directory), true);
    if (!chain.exists) {
        throw std::runtime_error("Cannot create Windows state directory.");
    }
}

bool regular_exists(const Path& input) {
    const Path path = checked_absolute(input);
    const auto chain = pin_directory(path.parent_path(), false);
    return chain.exists && existing_regular(path);
}

std::string read(const Path& input, std::uintmax_t maximum_bytes) {
    const Path path = checked_absolute(input);
    const auto chain = pin_directory(path.parent_path(), false);
    if (!chain.exists) {
        throw std::runtime_error("State directory is missing.");
    }
    Handle file = open_file(path, GENERIC_READ,
                            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, OPEN_EXISTING);
    if (!file) {
        fail("Open state file for reading");
    }
    validate_handle(file.get(), false);
    ByteLock lock(file.get(), false);
    const auto size = file_size(file.get());
    if (size > maximum_bytes || size > static_cast<std::uintmax_t>(std::string{}.max_size())) {
        throw std::runtime_error("State file exceeds its size limit.");
    }
    std::string content(static_cast<std::size_t>(size), '\0');
    std::size_t offset = 0;
    while (offset < content.size()) {
        const auto count = static_cast<DWORD>(std::min<std::size_t>(content.size() - offset, kChunkBytes));
        DWORD received = 0;
        if (!ReadFile(file.get(), content.data() + offset, count, &received, nullptr)) {
            fail("ReadFile(state)");
        }
        if (received == 0U) {
            throw std::runtime_error("State file changed during read.");
        }
        offset += received;
    }
    return content;
}

void atomic_write(const Path& input, std::string_view content) {
    const Path path = checked_absolute(input);
    const auto chain = pin_directory(
        path.parent_path(), false,
        FILE_READ_ATTRIBUTES | FILE_LIST_DIRECTORY | FILE_ADD_FILE);
    if (!chain.exists || chain.handles.empty()) {
        throw std::runtime_error("State directory is missing.");
    }
    (void)existing_regular(path);  // Missing is allowed; unsafe existing targets are not.
    static std::atomic<unsigned long> sequence{0};
    Path temporary = path;
    temporary += L".tmp." + std::to_wstring(GetCurrentProcessId()) + L"." +
                 std::to_wstring(sequence.fetch_add(1U));
    Handle file = open_file(temporary, GENERIC_WRITE | DELETE, 0U, CREATE_NEW);
    if (!file) {
        fail("Create exclusive state temporary");
    }
    try {
        validate_handle(file.get(), false);
        write_all(file.get(), content);
        flush(file.get());
        (void)existing_regular(path);
        // MoveFileExW requires the source to be closed. The unique CREATE_NEW
        // temporary lives beside the destination, while the checked parent
        // directory handles remain pinned without FILE_SHARE_DELETE.
        file.reset();
        // This is a same-volume rename; do not set MOVEFILE_COPY_ALLOWED
        // (copy/delete would weaken the atomic replacement contract).
        if (!MoveFileExW(temporary.c_str(), path.c_str(),
                         MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
            fail("MoveFileExW(state atomic replace)");
        }
    } catch (...) {
        if (file) {
            discard(file.get());
        } else {
            (void)DeleteFileW(temporary.c_str());
        }
        throw;
    }
}

void append(const Path& input, std::string_view line, std::uintmax_t maximum_bytes) {
    const Path path = checked_absolute(input);
    const auto chain = pin_directory(
        path.parent_path(), false,
        FILE_READ_ATTRIBUTES | FILE_LIST_DIRECTORY | FILE_ADD_FILE);
    if (!chain.exists) {
        throw std::runtime_error("State history directory is missing.");
    }
    Handle file = open_file(path, GENERIC_READ | GENERIC_WRITE,
                            FILE_SHARE_READ | FILE_SHARE_WRITE, OPEN_ALWAYS);
    if (!file) {
        fail("Open state history");
    }
    validate_handle(file.get(), false);
    ByteLock lock(file.get(), true);
    const auto size = file_size(file.get());
    if (size > maximum_bytes || line.size() > maximum_bytes - size) {
        throw std::runtime_error("State history reached its size limit.");
    }
    LARGE_INTEGER zero{};
    if (!SetFilePointerEx(file.get(), zero, nullptr, FILE_END)) {
        fail("Seek state history");
    }
    write_all(file.get(), line);
    flush(file.get());
}


namespace {

class FindHandle {
   public:
    explicit FindHandle(HANDLE value) noexcept : value_(value) {}
    ~FindHandle() {
        if (value_ != INVALID_HANDLE_VALUE) {
            (void)FindClose(value_);
        }
    }
    FindHandle(const FindHandle&) = delete;
    FindHandle& operator=(const FindHandle&) = delete;
    [[nodiscard]] HANDLE get() const noexcept { return value_; }

   private:
    HANDLE value_;
};

void copy_regular_file(const Path& source, const Path& destination) {
    const auto source_chain = pin_directory(source.parent_path(), false);
    if (!source_chain.exists) {
        throw std::runtime_error("Workspace template parent directory is missing.");
    }
    Handle input = open_file(source, GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING);
    if (!input) {
        fail("Open workspace template file");
    }
    validate_handle(input.get(), false);

    const auto destination_chain = pin_directory(
        destination.parent_path(), false,
        FILE_READ_ATTRIBUTES | FILE_LIST_DIRECTORY | FILE_ADD_FILE);
    if (!destination_chain.exists) {
        throw std::runtime_error("Workspace destination parent directory is missing.");
    }
    Handle output = open_file(destination, GENERIC_WRITE | DELETE, 0U, CREATE_NEW);
    if (!output) {
        fail("Create exclusive learner workspace file");
    }
    try {
        validate_handle(output.get(), false);
        char buffer[kChunkBytes];
        while (true) {
            DWORD received = 0U;
            if (!ReadFile(input.get(), buffer, sizeof(buffer), &received, nullptr)) {
                fail("Read workspace template file");
            }
            if (received == 0U) {
                break;
            }
            write_all(output.get(), std::string_view(buffer, received));
        }
        flush(output.get());
    } catch (...) {
        discard(output.get());
        throw;
    }
}

void copy_tree_contents(const Path& source, const Path& destination) {
    const auto source_chain = pin_directory(source, false);
    if (!source_chain.exists) {
        throw std::runtime_error("Workspace template directory is missing.");
    }
    const auto destination_chain = pin_directory(
        destination, false, FILE_READ_ATTRIBUTES | FILE_LIST_DIRECTORY | FILE_ADD_FILE);
    if (!destination_chain.exists) {
        throw std::runtime_error("Workspace destination directory is missing.");
    }

    Path pattern = source;
    pattern /= L"*";
    WIN32_FIND_DATAW data{};
    const HANDLE raw_find = FindFirstFileW(pattern.c_str(), &data);
    if (raw_find == INVALID_HANDLE_VALUE) {
        const DWORD error = GetLastError();
        if (error == ERROR_FILE_NOT_FOUND) {
            return;
        }
        fail("Enumerate workspace template directory", error);
    }
    FindHandle find(raw_find);
    do {
        const std::wstring name(data.cFileName);
        if (name == L"." || name == L"..") {
            continue;
        }
        if ((data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0U ||
            (data.dwFileAttributes & FILE_ATTRIBUTE_DEVICE) != 0U) {
            throw std::runtime_error("Workspace template contains a reparse point or device entry.");
        }

        const Path source_child = source / name;
        const Path destination_child = destination / name;
        if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0U) {
            create_directory_exclusive(destination_child);
            copy_tree_contents(source_child, destination_child);
        } else {
            copy_regular_file(source_child, destination_child);
        }
    } while (FindNextFileW(find.get(), &data));

    const DWORD error = GetLastError();
    if (error != ERROR_NO_MORE_FILES) {
        fail("Continue enumerating workspace template directory", error);
    }
}

void remove_tree_impl(const Path& input) {
    const Path path = checked_absolute(input);
    if (path == path.root_path()) {
        throw std::runtime_error("Refusing to remove a volume root as a workspace.");
    }
    const auto parent_chain = pin_directory(
        path.parent_path(), false,
        FILE_READ_ATTRIBUTES | FILE_LIST_DIRECTORY | FILE_DELETE_CHILD);
    if (!parent_chain.exists) {
        return;
    }
    const DWORD attributes = GetFileAttributesW(path.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES) {
        const DWORD error = GetLastError();
        if (missing(error)) {
            return;
        }
        fail("Inspect workspace cleanup entry", error);
    }

    if ((attributes & FILE_ATTRIBUTE_DIRECTORY) != 0U &&
        (attributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0U) {
        auto directory_chain = pin_directory(path, false);
        if (!directory_chain.exists) {
            throw std::runtime_error("Workspace cleanup directory disappeared.");
        }
        Path pattern = path;
        pattern /= L"*";
        WIN32_FIND_DATAW data{};
        const HANDLE raw_find = FindFirstFileW(pattern.c_str(), &data);
        if (raw_find != INVALID_HANDLE_VALUE) {
            FindHandle find(raw_find);
            do {
                const std::wstring name(data.cFileName);
                if (name == L"." || name == L"..") {
                    continue;
                }
                remove_tree_impl(path / name);
            } while (FindNextFileW(find.get(), &data));
            const DWORD error = GetLastError();
            if (error != ERROR_NO_MORE_FILES) {
                fail("Continue enumerating workspace cleanup directory", error);
            }
        } else {
            const DWORD error = GetLastError();
            if (error != ERROR_FILE_NOT_FOUND) {
                fail("Enumerate workspace cleanup directory", error);
            }
        }
        // Windows will not remove a directory while our no-follow handle
        // to that directory remains open without FILE_SHARE_DELETE.
        directory_chain.handles.clear();
        if (!RemoveDirectoryW(path.c_str())) {
            fail("Remove learner workspace directory");
        }
        return;
    }

    if ((attributes & FILE_ATTRIBUTE_DIRECTORY) != 0U) {
        if (!RemoveDirectoryW(path.c_str())) {
            fail("Remove learner workspace reparse directory");
        }
    } else if (!DeleteFileW(path.c_str())) {
        fail("Remove learner workspace file");
    }
}

}  // namespace

void require_directory(const Path& input) {
    const Path path = checked_absolute(input);
    const auto chain = pin_directory(path, false);
    if (!chain.exists) {
        throw std::runtime_error("Required native directory does not exist.");
    }
}

void create_directory_exclusive(const Path& input) {
    const Path path = checked_absolute(input);
    const auto parent_chain = pin_directory(
        path.parent_path(), false,
        FILE_READ_ATTRIBUTES | FILE_LIST_DIRECTORY | FILE_ADD_FILE);
    if (!parent_chain.exists) {
        throw std::runtime_error("Cannot create directory under a missing parent.");
    }
    if (!CreateDirectoryW(path.c_str(), nullptr)) {
        const DWORD error = GetLastError();
        if (error == ERROR_ALREADY_EXISTS) {
            throw std::runtime_error("Workspace destination already exists.");
        }
        fail("CreateDirectoryW(exclusive workspace)");
    }

    Handle directory(CreateFileW(path.c_str(), FILE_READ_ATTRIBUTES | FILE_LIST_DIRECTORY,
                                  FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
                                  FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT,
                                  nullptr));
    if (!directory) {
        const DWORD error = GetLastError();
        (void)RemoveDirectoryW(path.c_str());
        fail("Open newly created workspace directory", error);
    }
    try {
        validate_handle(directory.get(), true);
    } catch (...) {
        directory.reset();
        (void)RemoveDirectoryW(path.c_str());
        throw;
    }
}

bool entry_exists(const Path& input) {
    const Path path = checked_absolute(input);
    const auto parent_chain = pin_directory(path.parent_path(), false);
    if (!parent_chain.exists) {
        return false;
    }
    const DWORD attributes = GetFileAttributesW(path.c_str());
    if (attributes != INVALID_FILE_ATTRIBUTES) {
        return true;
    }
    const DWORD error = GetLastError();
    if (missing(error)) {
        return false;
    }
    fail("Inspect native workspace destination", error);
}

[[nodiscard]] bool path_is_within(const Path& child, const Path& parent) {
    auto child_part = child.begin();
    for (auto parent_part = parent.begin(); parent_part != parent.end();
         ++parent_part, ++child_part) {
        if (child_part == child.end() ||
            CompareStringOrdinal(parent_part->c_str(), -1, child_part->c_str(), -1, TRUE) !=
                CSTR_EQUAL) {
            return false;
        }
    }
    return true;
}

void copy_tree(const Path& source, const Path& destination) {
    require_directory(source);
    require_directory(destination);
    const Path absolute_source = checked_absolute(source);
    const Path absolute_destination = checked_absolute(destination);
    if (path_is_within(absolute_destination, absolute_source) ||
        path_is_within(absolute_source, absolute_destination)) {
        throw std::runtime_error("Workspace template and destination must not contain one another.");
    }
    copy_tree_contents(absolute_source, absolute_destination);
}

void remove_tree(const Path& path) {
    remove_tree_impl(path);
}

}  // namespace cppapplab::windows_state_io
