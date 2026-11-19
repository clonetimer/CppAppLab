#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0602
#endif

#include "cppapplab/process.hpp"

#include <windows.h>

#include <algorithm>
#include <climits>
#include <cstddef>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

namespace cppapplab {
namespace {

class UniqueHandle {
   public:
    explicit UniqueHandle(HANDLE handle = nullptr) noexcept : handle_(handle) {}
    ~UniqueHandle() {
        reset();
    }

    UniqueHandle(const UniqueHandle&) = delete;
    UniqueHandle& operator=(const UniqueHandle&) = delete;

    UniqueHandle(UniqueHandle&& other) noexcept : handle_(other.release()) {}
    UniqueHandle& operator=(UniqueHandle&& other) noexcept {
        if (this != &other) {
            reset(other.release());
        }
        return *this;
    }

    [[nodiscard]] HANDLE get() const noexcept {
        return handle_;
    }

    [[nodiscard]] HANDLE release() noexcept {
        HANDLE result = handle_;
        handle_ = nullptr;
        return result;
    }

    void reset(HANDLE handle = nullptr) noexcept {
        if (handle_ != nullptr && handle_ != INVALID_HANDLE_VALUE) {
            (void)::CloseHandle(handle_);
        }
        handle_ = handle;
    }

   private:
    HANDLE handle_{nullptr};
};

class AttributeList {
   public:
    AttributeList() {
        SIZE_T required_size = 0;
        (void)::InitializeProcThreadAttributeList(nullptr, 1, 0, &required_size);
        if (::GetLastError() != ERROR_INSUFFICIENT_BUFFER || required_size == 0) {
            throw_system_error("InitializeProcThreadAttributeList(size)");
        }

        storage_.resize(required_size);
        list_ = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(storage_.data());
        if (!::InitializeProcThreadAttributeList(list_, 1, 0, &required_size)) {
            list_ = nullptr;
            throw_system_error("InitializeProcThreadAttributeList");
        }
    }

    ~AttributeList() {
        if (list_ != nullptr) {
            ::DeleteProcThreadAttributeList(list_);
        }
    }

    AttributeList(const AttributeList&) = delete;
    AttributeList& operator=(const AttributeList&) = delete;

    [[nodiscard]] LPPROC_THREAD_ATTRIBUTE_LIST get() const noexcept {
        return list_;
    }

   private:
    static void throw_system_error(const char* operation) {
        const DWORD error = ::GetLastError();
        throw std::system_error(static_cast<int>(error), std::system_category(), operation);
    }

    std::vector<unsigned char> storage_;
    LPPROC_THREAD_ATTRIBUTE_LIST list_{nullptr};
};

[[noreturn]] void throw_system_error(const char* operation, DWORD error = ::GetLastError()) {
    throw std::system_error(static_cast<int>(error), std::system_category(), operation);
}

void append_limited(ProcessResult& result, const char* data, std::size_t size,
                    std::size_t limit) {
    const std::size_t available =
        result.output.size() < limit ? limit - result.output.size() : 0U;
    const std::size_t keep = std::min(size, available);

    if (keep > 0U) {
        result.output.append(data, keep);
    }
    if (keep < size) {
        result.output_truncated = true;
    }
}

std::wstring utf8_to_wide(const std::string& value) {
    if (value.empty()) {
        return {};
    }
    if (value.size() > static_cast<std::size_t>(INT_MAX)) {
        throw std::invalid_argument("process argument exceeds the Windows string limit");
    }

    const int input_size = static_cast<int>(value.size());
    const int required = ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
                                                input_size, nullptr, 0);
    if (required == 0) {
        throw_system_error("MultiByteToWideChar");
    }

    std::wstring result(static_cast<std::size_t>(required), L'\0');
    if (::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), input_size,
                              result.data(), required) != required) {
        throw_system_error("MultiByteToWideChar");
    }
    return result;
}

std::wstring quote_windows_argument(const std::wstring& argument) {
    std::wstring result;
    result.push_back(L'"');

    std::size_t backslashes = 0U;
    for (const wchar_t character : argument) {
        if (character == L'\\') {
            ++backslashes;
            continue;
        }

        if (character == L'"') {
            result.append(backslashes * 2U + 1U, L'\\');
            result.push_back(L'"');
        } else {
            result.append(backslashes, L'\\');
            result.push_back(character);
        }
        backslashes = 0U;
    }

    result.append(backslashes * 2U, L'\\');
    result.push_back(L'"');
    return result;
}

std::wstring build_command_line(const std::vector<std::string>& arguments) {
    std::wstring command_line;
    for (const std::string& argument : arguments) {
        if (!command_line.empty()) {
            command_line.push_back(L' ');
        }
        command_line += quote_windows_argument(utf8_to_wide(argument));
    }

    // CreateProcessW accepts at most 32,767 UTF-16 code units including NUL.
    if (command_line.size() >= 32767U) {
        throw std::invalid_argument("process command line exceeds the Windows limit");
    }
    return command_line;
}

UniqueHandle create_standard_input(const std::string& standard_input) {
    SECURITY_ATTRIBUTES security{};
    security.nLength = sizeof(security);
    security.bInheritHandle = TRUE;

    if (standard_input.empty()) {
        HANDLE handle = ::CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                      &security, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (handle == INVALID_HANDLE_VALUE) {
            throw_system_error("CreateFileW(NUL)");
        }
        return UniqueHandle(handle);
    }

    wchar_t temporary_path[MAX_PATH + 1]{};
    const DWORD path_length = ::GetTempPathW(MAX_PATH, temporary_path);
    if (path_length == 0 || path_length >= MAX_PATH) {
        throw_system_error("GetTempPathW");
    }

    wchar_t temporary_file[MAX_PATH + 1]{};
    if (::GetTempFileNameW(temporary_path, L"cpl", 0, temporary_file) == 0) {
        throw_system_error("GetTempFileNameW");
    }

    HANDLE handle = ::CreateFileW(
        temporary_file, GENERIC_READ | GENERIC_WRITE | DELETE,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, &security, OPEN_EXISTING,
        FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, nullptr);
    if (handle == INVALID_HANDLE_VALUE) {
        const DWORD error = ::GetLastError();
        (void)::DeleteFileW(temporary_file);
        throw_system_error("CreateFileW(temporary stdin)", error);
    }

    UniqueHandle result(handle);
    std::size_t written = 0U;
    while (written < standard_input.size()) {
        const DWORD remaining = static_cast<DWORD>(
            std::min(standard_input.size() - written, static_cast<std::size_t>(MAXDWORD)));
        DWORD count = 0;
        if (!::WriteFile(result.get(), standard_input.data() + written, remaining, &count, nullptr)) {
            throw_system_error("WriteFile(temporary stdin)");
        }
        if (count == 0U) {
            throw std::runtime_error("WriteFile(temporary stdin) made no progress");
        }
        written += static_cast<std::size_t>(count);
    }

    if (!::FlushFileBuffers(result.get())) {
        throw_system_error("FlushFileBuffers(temporary stdin)");
    }

    LARGE_INTEGER beginning{};
    if (!::SetFilePointerEx(result.get(), beginning, nullptr, FILE_BEGIN)) {
        throw_system_error("SetFilePointerEx(temporary stdin)");
    }
    return result;
}

UniqueHandle create_kill_on_close_job() {
    UniqueHandle job(::CreateJobObjectW(nullptr, nullptr));
    if (job.get() == nullptr) {
        throw_system_error("CreateJobObjectW");
    }

    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if (!::SetInformationJobObject(job.get(), JobObjectExtendedLimitInformation, &limits,
                                   sizeof(limits))) {
        throw_system_error("SetInformationJobObject");
    }
    return job;
}

void append_launch_error(ProcessResult& result, DWORD error, std::size_t output_limit) {
    const std::string message = "CreateProcessW failed with Win32 error " +
                                std::to_string(static_cast<unsigned long>(error)) + "\n";
    append_limited(result, message.data(), message.size(), output_limit);
}

bool is_broken_pipe_error(DWORD error) {
    return error == ERROR_BROKEN_PIPE || error == ERROR_NO_DATA;
}

}  // namespace

ProcessResult run_process(const std::vector<std::string>& arguments,
                          const std::filesystem::path& working_directory,
                          std::chrono::milliseconds timeout, std::size_t max_output_bytes,
                          const std::string& standard_input) {
    if (arguments.empty() || arguments.front().empty()) {
        throw std::invalid_argument("run_process requires a non-empty executable");
    }
    if (timeout.count() <= 0) {
        throw std::invalid_argument("run_process timeout must be positive");
    }
    constexpr std::size_t kMaxStdinBytes = 256U * 1024U;
    if (standard_input.size() > kMaxStdinBytes) {
        throw std::invalid_argument("run_process stdin exceeds the 256 KiB limit");
    }

    std::wstring command_line = build_command_line(arguments);
    const std::wstring current_directory =
        working_directory.empty() ? std::wstring{} : working_directory.wstring();

    UniqueHandle child_input = create_standard_input(standard_input);

    SECURITY_ATTRIBUTES pipe_security{};
    pipe_security.nLength = sizeof(pipe_security);
    pipe_security.bInheritHandle = TRUE;

    HANDLE raw_read_pipe = nullptr;
    HANDLE raw_write_pipe = nullptr;
    if (!::CreatePipe(&raw_read_pipe, &raw_write_pipe, &pipe_security, 0)) {
        throw_system_error("CreatePipe");
    }
    UniqueHandle read_pipe(raw_read_pipe);
    UniqueHandle write_pipe(raw_write_pipe);

    if (!::SetHandleInformation(read_pipe.get(), HANDLE_FLAG_INHERIT, 0)) {
        throw_system_error("SetHandleInformation(stdout pipe)");
    }

    AttributeList attributes;
    HANDLE inherited_handles[] = {child_input.get(), write_pipe.get()};
    if (!::UpdateProcThreadAttribute(attributes.get(), 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
                                     inherited_handles, sizeof(inherited_handles), nullptr,
                                     nullptr)) {
        throw_system_error("UpdateProcThreadAttribute(handle list)");
    }

    STARTUPINFOEXW startup{};
    startup.StartupInfo.cb = sizeof(startup);
    startup.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
    startup.StartupInfo.hStdInput = child_input.get();
    startup.StartupInfo.hStdOutput = write_pipe.get();
    startup.StartupInfo.hStdError = write_pipe.get();
    startup.lpAttributeList = attributes.get();

    UniqueHandle job = create_kill_on_close_job();

    PROCESS_INFORMATION process_info{};
    const DWORD creation_flags =
        CREATE_SUSPENDED | CREATE_NO_WINDOW | EXTENDED_STARTUPINFO_PRESENT;
    const BOOL created = ::CreateProcessW(
        nullptr, command_line.data(), nullptr, nullptr, TRUE, creation_flags, nullptr,
        current_directory.empty() ? nullptr : current_directory.c_str(), &startup.StartupInfo,
        &process_info);

    if (!created) {
        const DWORD error = ::GetLastError();
        ProcessResult result;
        if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND) {
            std::error_code filesystem_error;
            const bool directory_is_valid =
                working_directory.empty() ||
                std::filesystem::is_directory(working_directory, filesystem_error);
            result.exit_code = directory_is_valid ? 127 : 126;
            append_launch_error(result, error, max_output_bytes);
            return result;
        }
        if (error == ERROR_DIRECTORY) {
            result.exit_code = 126;
            append_launch_error(result, error, max_output_bytes);
            return result;
        }
        throw_system_error("CreateProcessW", error);
    }

    UniqueHandle process(process_info.hProcess);
    UniqueHandle primary_thread(process_info.hThread);

    if (!::AssignProcessToJobObject(job.get(), process.get())) {
        const DWORD error = ::GetLastError();
        (void)::TerminateProcess(process.get(), 127);
        (void)::WaitForSingleObject(process.get(), INFINITE);
        throw_system_error("AssignProcessToJobObject", error);
    }

    if (::ResumeThread(primary_thread.get()) == static_cast<DWORD>(-1)) {
        const DWORD error = ::GetLastError();
        (void)::TerminateJobObject(job.get(), 127);
        (void)::WaitForSingleObject(process.get(), INFINITE);
        throw_system_error("ResumeThread", error);
    }

    primary_thread.reset();
    write_pipe.reset();
    child_input.reset();

    ProcessResult result;
    bool process_finished = false;
    bool pipe_closed = false;
    const auto deadline = std::chrono::steady_clock::now() + timeout;

    while (!process_finished || !pipe_closed) {
        if (!result.timed_out &&
            std::chrono::steady_clock::now() >= deadline &&
            (!process_finished || !pipe_closed)) {
            result.timed_out = true;
            if (!::TerminateJobObject(job.get(), 137)) {
                const DWORD error = ::GetLastError();
                // Closing a kill-on-close job is the final cleanup path if explicit
                // termination races with the last process leaving the job.
                job.reset();
                if (!process_finished) {
                    const DWORD wait_result = ::WaitForSingleObject(process.get(), 1000);
                    if (wait_result != WAIT_OBJECT_0) {
                        if (!::TerminateProcess(process.get(), 137)) {
                            throw_system_error("TerminateJobObject/TerminateProcess", error);
                        }
                        (void)::WaitForSingleObject(process.get(), INFINITE);
                    }
                    process_finished = true;
                }
            }
        }

        if (!pipe_closed) {
            DWORD available = 0;
            if (!::PeekNamedPipe(read_pipe.get(), nullptr, 0, nullptr, &available, nullptr)) {
                const DWORD error = ::GetLastError();
                if (is_broken_pipe_error(error)) {
                    pipe_closed = true;
                } else {
                    throw_system_error("PeekNamedPipe", error);
                }
            } else {
                while (available > 0U) {
                    char buffer[4096];
                    const DWORD requested =
                        std::min<DWORD>(available, static_cast<DWORD>(sizeof(buffer)));
                    DWORD count = 0;
                    if (!::ReadFile(read_pipe.get(), buffer, requested, &count, nullptr)) {
                        const DWORD error = ::GetLastError();
                        if (is_broken_pipe_error(error)) {
                            pipe_closed = true;
                            break;
                        }
                        throw_system_error("ReadFile(stdout pipe)", error);
                    }
                    if (count == 0U) {
                        pipe_closed = true;
                        break;
                    }

                    append_limited(result, buffer, static_cast<std::size_t>(count),
                                   max_output_bytes);

                    if (!::PeekNamedPipe(read_pipe.get(), nullptr, 0, nullptr, &available,
                                         nullptr)) {
                        const DWORD error = ::GetLastError();
                        if (is_broken_pipe_error(error)) {
                            pipe_closed = true;
                            break;
                        }
                        throw_system_error("PeekNamedPipe", error);
                    }
                }
            }
        }

        if (!process_finished) {
            const DWORD wait_result = ::WaitForSingleObject(process.get(), 10);
            if (wait_result == WAIT_OBJECT_0) {
                process_finished = true;
            } else if (wait_result == WAIT_FAILED) {
                throw_system_error("WaitForSingleObject(process)");
            }
        } else if (!pipe_closed) {
            ::Sleep(10);
        }
    }

    if (result.timed_out) {
        result.exit_code = 137;
    } else {
        DWORD exit_code = 0;
        if (!::GetExitCodeProcess(process.get(), &exit_code)) {
            throw_system_error("GetExitCodeProcess");
        }
        result.exit_code = static_cast<int>(exit_code);
    }

    return result;
}

}  // namespace cppapplab
