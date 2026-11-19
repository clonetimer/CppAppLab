#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "cppapplab/process.hpp"

#include <windows.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <vector>

namespace cppapplab {
namespace {

constexpr std::size_t kMaxStdinBytes = 256U * 1024U;
constexpr DWORD kPipePollMilliseconds = 5U;
constexpr DWORD kTerminateExitCode = 1U;

class UniqueHandle {
   public:
    UniqueHandle() = default;
    explicit UniqueHandle(HANDLE value) : value_(value) {}
    ~UniqueHandle() { reset(); }

    UniqueHandle(const UniqueHandle&) = delete;
    UniqueHandle& operator=(const UniqueHandle&) = delete;

    UniqueHandle(UniqueHandle&& other) noexcept : value_(other.release()) {}
    UniqueHandle& operator=(UniqueHandle&& other) noexcept {
        if (this != &other) {
            reset(other.release());
        }
        return *this;
    }

    [[nodiscard]] HANDLE get() const noexcept { return value_; }
    [[nodiscard]] explicit operator bool() const noexcept {
        return value_ != nullptr && value_ != INVALID_HANDLE_VALUE;
    }

    [[nodiscard]] HANDLE release() noexcept {
        HANDLE value = value_;
        value_ = nullptr;
        return value;
    }

    void reset(HANDLE value = nullptr) noexcept {
        if (*this) {
            (void)CloseHandle(value_);
        }
        value_ = value;
    }

   private:
    HANDLE value_{nullptr};
};

[[noreturn]] void throw_windows_error(const char* operation, DWORD error = GetLastError()) {
    throw std::system_error(static_cast<int>(error), std::system_category(), operation);
}

std::wstring utf8_to_wide(std::string_view value) {
    if (value.empty()) {
        return {};
    }
    if (value.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        throw std::invalid_argument("Process argument exceeds the Windows conversion limit.");
    }

    const int input_size = static_cast<int>(value.size());
    const int required = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
                                             input_size, nullptr, 0);
    if (required == 0) {
        throw_windows_error("MultiByteToWideChar");
    }

    std::wstring result(static_cast<std::size_t>(required), L'\0');
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), input_size,
                            result.data(), required) != required) {
        throw_windows_error("MultiByteToWideChar");
    }
    return result;
}

std::wstring quote_windows_argument(const std::wstring& argument) {
    if (!argument.empty() &&
        argument.find_first_of(L" \t\n\v\"") == std::wstring::npos) {
        return argument;
    }

    std::wstring quoted;
    quoted.push_back(L'"');
    std::size_t backslashes = 0U;

    for (const wchar_t character : argument) {
        if (character == L'\\') {
            ++backslashes;
            continue;
        }

        if (character == L'"') {
            quoted.append(backslashes * 2U + 1U, L'\\');
            quoted.push_back(L'"');
            backslashes = 0U;
            continue;
        }

        quoted.append(backslashes, L'\\');
        backslashes = 0U;
        quoted.push_back(character);
    }

    quoted.append(backslashes * 2U, L'\\');
    quoted.push_back(L'"');
    return quoted;
}

std::wstring build_command_line(const std::vector<std::string>& arguments) {
    std::wstring command_line;
    for (std::size_t index = 0U; index < arguments.size(); ++index) {
        if (arguments[index].find('\0') != std::string::npos) {
            throw std::invalid_argument("Process arguments cannot contain NUL bytes.");
        }
        if (index != 0U) {
            command_line.push_back(L' ');
        }
        command_line += quote_windows_argument(utf8_to_wide(arguments[index]));
    }

    // CreateProcessW accepts at most 32,767 UTF-16 code units including the terminator.
    if (command_line.size() >= 32767U) {
        throw std::invalid_argument("Process command line exceeds the Windows 32,767-character limit.");
    }
    return command_line;
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

void set_non_inheritable(HANDLE handle) {
    if (!SetHandleInformation(handle, HANDLE_FLAG_INHERIT, 0U)) {
        throw_windows_error("SetHandleInformation");
    }
}

class AttributeList {
   public:
    AttributeList() {
        SIZE_T required_bytes = 0U;
        (void)InitializeProcThreadAttributeList(nullptr, 1U, 0U, &required_bytes);
        const DWORD size_error = GetLastError();
        if (size_error != ERROR_INSUFFICIENT_BUFFER || required_bytes == 0U) {
            throw_windows_error("InitializeProcThreadAttributeList(size)", size_error);
        }

        storage_.resize(required_bytes);
        list_ = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(storage_.data());
        if (!InitializeProcThreadAttributeList(list_, 1U, 0U, &required_bytes)) {
            list_ = nullptr;
            throw_windows_error("InitializeProcThreadAttributeList");
        }
    }

    ~AttributeList() {
        if (list_ != nullptr) {
            DeleteProcThreadAttributeList(list_);
        }
    }

    AttributeList(const AttributeList&) = delete;
    AttributeList& operator=(const AttributeList&) = delete;

    [[nodiscard]] LPPROC_THREAD_ATTRIBUTE_LIST get() const noexcept { return list_; }

   private:
    std::vector<unsigned char> storage_;
    LPPROC_THREAD_ATTRIBUTE_LIST list_{nullptr};
};

void write_child_input(HANDLE handle, const std::string& input, DWORD& write_error) noexcept {
    std::size_t offset = 0U;
    while (offset < input.size()) {
        const std::size_t remaining = input.size() - offset;
        const DWORD requested = static_cast<DWORD>(
            std::min(remaining, static_cast<std::size_t>(std::numeric_limits<DWORD>::max())));
        DWORD written = 0U;
        if (!WriteFile(handle, input.data() + offset, requested, &written, nullptr)) {
            const DWORD error = GetLastError();
            if (error != ERROR_BROKEN_PIPE && error != ERROR_NO_DATA) {
                write_error = error;
            }
            break;
        }
        if (written == 0U) {
            write_error = ERROR_WRITE_FAULT;
            break;
        }
        offset += static_cast<std::size_t>(written);
    }
    (void)CloseHandle(handle);
}

void terminate_and_reap(HANDLE job, HANDLE process, std::thread& input_writer) noexcept {
    if (job != nullptr && job != INVALID_HANDLE_VALUE) {
        (void)TerminateJobObject(job, kTerminateExitCode);
    } else if (process != nullptr && process != INVALID_HANDLE_VALUE) {
        (void)TerminateProcess(process, kTerminateExitCode);
    }
    if (process != nullptr && process != INVALID_HANDLE_VALUE) {
        (void)WaitForSingleObject(process, INFINITE);
    }
    if (input_writer.joinable()) {
        input_writer.join();
    }
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
    if (standard_input.size() > kMaxStdinBytes) {
        throw std::invalid_argument("run_process stdin exceeds the 256 KiB limit");
    }

    std::wstring command_line = build_command_line(arguments);
    const std::wstring working_directory_wide =
        working_directory.empty() ? std::wstring{} : working_directory.native();

    SECURITY_ATTRIBUTES security{};
    security.nLength = sizeof(security);
    security.bInheritHandle = TRUE;

    HANDLE output_read_raw = nullptr;
    HANDLE output_write_raw = nullptr;
    if (!CreatePipe(&output_read_raw, &output_write_raw, &security, 0U)) {
        throw_windows_error("CreatePipe(stdout)");
    }
    UniqueHandle output_read(output_read_raw);
    UniqueHandle output_write(output_write_raw);
    set_non_inheritable(output_read.get());

    HANDLE input_read_raw = nullptr;
    HANDLE input_write_raw = nullptr;
    if (!CreatePipe(&input_read_raw, &input_write_raw, &security, 0U)) {
        throw_windows_error("CreatePipe(stdin)");
    }
    UniqueHandle input_read(input_read_raw);
    UniqueHandle input_write(input_write_raw);
    set_non_inheritable(input_write.get());

    UniqueHandle job(CreateJobObjectW(nullptr, nullptr));
    if (!job) {
        throw_windows_error("CreateJobObjectW");
    }

    JOBOBJECT_EXTENDED_LIMIT_INFORMATION job_limits{};
    job_limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if (!SetInformationJobObject(job.get(), JobObjectExtendedLimitInformation,
                                 &job_limits, sizeof(job_limits))) {
        throw_windows_error("SetInformationJobObject");
    }

    AttributeList attributes;
    HANDLE inherited_handles[]{input_read.get(), output_write.get()};
    if (!UpdateProcThreadAttribute(attributes.get(), 0U, PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
                                   inherited_handles, sizeof(inherited_handles), nullptr, nullptr)) {
        throw_windows_error("UpdateProcThreadAttribute(handle list)");
    }

    STARTUPINFOEXW startup{};
    startup.StartupInfo.cb = sizeof(startup);
    startup.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
    startup.StartupInfo.hStdInput = input_read.get();
    startup.StartupInfo.hStdOutput = output_write.get();
    startup.StartupInfo.hStdError = output_write.get();
    startup.lpAttributeList = attributes.get();

    PROCESS_INFORMATION process_info{};
    const DWORD creation_flags =
        CREATE_SUSPENDED | CREATE_NO_WINDOW | EXTENDED_STARTUPINFO_PRESENT;
    const wchar_t* current_directory =
        working_directory_wide.empty() ? nullptr : working_directory_wide.c_str();

    if (!CreateProcessW(nullptr, command_line.data(), nullptr, nullptr, TRUE, creation_flags,
                        nullptr, current_directory, &startup.StartupInfo, &process_info)) {
        const DWORD error = GetLastError();
        ProcessResult failed;
        failed.exit_code = (error == ERROR_DIRECTORY) ? 126 : 127;
        char diagnostic[96]{};
        const int count = std::snprintf(diagnostic, sizeof(diagnostic),
                                        "CreateProcessW failed with Windows error %lu\n",
                                        static_cast<unsigned long>(error));
        if (count > 0) {
            append_limited(failed, diagnostic,
                           static_cast<std::size_t>(std::min<int>(count,
                               static_cast<int>(sizeof(diagnostic) - 1U))),
                           max_output_bytes);
        }
        return failed;
    }

    UniqueHandle process(process_info.hProcess);
    UniqueHandle primary_thread(process_info.hThread);

    // The child inherited only these two handles. Parent copies must close before reading EOF.
    input_read.reset();
    output_write.reset();

    if (!AssignProcessToJobObject(job.get(), process.get())) {
        const DWORD error = GetLastError();
        (void)TerminateProcess(process.get(), kTerminateExitCode);
        (void)WaitForSingleObject(process.get(), INFINITE);
        throw_windows_error("AssignProcessToJobObject", error);
    }

    if (ResumeThread(primary_thread.get()) == static_cast<DWORD>(-1)) {
        const DWORD error = GetLastError();
        (void)TerminateJobObject(job.get(), kTerminateExitCode);
        (void)WaitForSingleObject(process.get(), INFINITE);
        throw_windows_error("ResumeThread", error);
    }
    primary_thread.reset();

    ProcessResult result;
    DWORD input_write_error = ERROR_SUCCESS;
    std::thread input_writer;
    if (standard_input.empty()) {
        input_write.reset();  // Closing the parent writer delivers EOF to the child.
    } else {
        const HANDLE writer_handle = input_write.release();
        try {
            input_writer = std::thread([writer_handle, &standard_input, &input_write_error]() {
                write_child_input(writer_handle, standard_input, input_write_error);
            });
        } catch (...) {
            (void)CloseHandle(writer_handle);
            (void)TerminateJobObject(job.get(), kTerminateExitCode);
            (void)WaitForSingleObject(process.get(), INFINITE);
            throw;
        }
    }

    bool process_exited = false;
    bool output_closed = false;
    bool timeout_terminated = false;
    DWORD child_exit_code = STILL_ACTIVE;
    const auto deadline = std::chrono::steady_clock::now() + timeout;

    try {
        while (!process_exited || !output_closed) {
            if (!timeout_terminated && std::chrono::steady_clock::now() >= deadline) {
                result.timed_out = true;
                if (!TerminateJobObject(job.get(), kTerminateExitCode)) {
                    throw_windows_error("TerminateJobObject");
                }
                timeout_terminated = true;
            }

            if (!output_closed) {
                DWORD available = 0U;
                if (PeekNamedPipe(output_read.get(), nullptr, 0U, nullptr, &available, nullptr)) {
                    if (available > 0U) {
                        char buffer[4096];
                        const DWORD requested =
                            std::min<DWORD>(available, static_cast<DWORD>(sizeof(buffer)));
                        DWORD bytes_read = 0U;
                        if (ReadFile(output_read.get(), buffer, requested, &bytes_read, nullptr)) {
                            if (bytes_read > 0U) {
                                append_limited(result, buffer, static_cast<std::size_t>(bytes_read),
                                               max_output_bytes);
                            }
                        } else {
                            const DWORD error = GetLastError();
                            if (error == ERROR_BROKEN_PIPE || error == ERROR_PIPE_NOT_CONNECTED) {
                                output_closed = true;
                            } else {
                                throw_windows_error("ReadFile(stdout)", error);
                            }
                        }
                    }
                } else {
                    const DWORD error = GetLastError();
                    if (error == ERROR_BROKEN_PIPE || error == ERROR_PIPE_NOT_CONNECTED) {
                        output_closed = true;
                    } else {
                        throw_windows_error("PeekNamedPipe(stdout)", error);
                    }
                }
            }

            if (!process_exited) {
                const DWORD wait_result = WaitForSingleObject(process.get(), 0U);
                if (wait_result == WAIT_OBJECT_0) {
                    process_exited = true;
                    if (!GetExitCodeProcess(process.get(), &child_exit_code)) {
                        throw_windows_error("GetExitCodeProcess");
                    }
                } else if (wait_result == WAIT_FAILED) {
                    throw_windows_error("WaitForSingleObject(process)");
                }
            }

            if (!process_exited || !output_closed) {
                Sleep(kPipePollMilliseconds);
            }
        }
    } catch (...) {
        terminate_and_reap(job.get(), process.get(), input_writer);
        throw;
    }

    if (input_writer.joinable()) {
        input_writer.join();
    }
    if (input_write_error != ERROR_SUCCESS) {
        throw_windows_error("WriteFile(stdin)", input_write_error);
    }

    result.exit_code = static_cast<int>(child_exit_code);
    return result;
}

}  // namespace cppapplab
