#include "cppapplab/progress.hpp"

#include "cppapplab/workspace.hpp"

#include <cerrno>
#include <chrono>
#include <ctime>
#include <cstdint>
#include <cstdlib>
#include <fcntl.h>
#include <fstream>
#include <limits>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>

namespace cppapplab {
namespace {

constexpr std::uintmax_t kMaxProgressBytes = 16U * 1024U;
constexpr std::uintmax_t kMaxHistoryBytes = 1024U * 1024U;

bool valid_task_id(std::string_view task_id) {
    if (task_id.empty()) {
        return false;
    }
    for (const char ch : task_id) {
        const bool valid = (ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') || ch == '.' ||
                           ch == '-' || ch == '_';
        if (!valid) {
            return false;
        }
    }
    return true;
}

void ensure_real_directory(const std::filesystem::path& directory) {
    std::filesystem::create_directories(directory);
    const auto status = std::filesystem::symlink_status(directory);
    if (std::filesystem::is_symlink(status) || !std::filesystem::is_directory(status)) {
        throw std::runtime_error("Progress path must be a real directory: " + directory.string());
    }
}

std::string read_regular_file(const std::filesystem::path& path, std::uintmax_t maximum_bytes) {
    std::error_code error;
    const auto status = std::filesystem::symlink_status(path, error);
    if (error) {
        throw std::runtime_error("Cannot inspect progress file: " + path.string());
    }
    if (!std::filesystem::is_regular_file(status) || std::filesystem::is_symlink(status)) {
        throw std::runtime_error("Progress path must be a regular non-symlink file.");
    }

    const auto size = std::filesystem::file_size(path);
    if (size > maximum_bytes) {
        throw std::runtime_error("Progress file exceeds the configured size limit.");
    }

    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("Cannot open progress file: " + path.string());
    }

    std::string content{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    if (!input.eof() && input.fail()) {
        throw std::runtime_error("Failed while reading progress file.");
    }
    return content;
}

std::map<std::string, std::string> parse_fields(const std::string& content) {
    std::map<std::string, std::string> fields;
    std::istringstream input(content);
    std::string line;

    while (std::getline(input, line)) {
        const std::size_t separator = line.find('=');
        if (separator == std::string::npos || separator == 0) {
            continue;
        }
        fields[line.substr(0, separator)] = line.substr(separator + 1);
    }
    return fields;
}

const std::string& required_field(const std::map<std::string, std::string>& fields,
                                  const std::string& name) {
    const auto found = fields.find(name);
    if (found == fields.end()) {
        throw std::runtime_error("Progress record is missing field: " + name);
    }
    return found->second;
}

unsigned parse_unsigned(const std::map<std::string, std::string>& fields, const std::string& name) {
    const std::string& value = required_field(fields, name);
    std::size_t consumed = 0;
    const unsigned long parsed = std::stoul(value, &consumed);
    if (consumed != value.size() || parsed > std::numeric_limits<unsigned>::max()) {
        throw std::runtime_error("Progress record contains an invalid number: " + name);
    }
    return static_cast<unsigned>(parsed);
}

std::string encode_field(std::string_view value) {
    constexpr char hex[] = "0123456789ABCDEF";
    std::string encoded;
    for (const unsigned char byte : value) {
        const bool unreserved = (byte >= 'a' && byte <= 'z') || (byte >= 'A' && byte <= 'Z') ||
                                (byte >= '0' && byte <= '9') || byte == '/' || byte == '.' ||
                                byte == '_' || byte == '-';
        if (unreserved) {
            encoded.push_back(static_cast<char>(byte));
        } else {
            encoded.push_back('%');
            encoded.push_back(hex[(byte >> 4U) & 0x0FU]);
            encoded.push_back(hex[byte & 0x0FU]);
        }
    }
    return encoded;
}

unsigned char hex_value(char value) {
    if (value >= '0' && value <= '9') {
        return static_cast<unsigned char>(value - '0');
    }
    if (value >= 'A' && value <= 'F') {
        return static_cast<unsigned char>(value - 'A' + 10);
    }
    if (value >= 'a' && value <= 'f') {
        return static_cast<unsigned char>(value - 'a' + 10);
    }
    throw std::runtime_error("Progress history contains an invalid escape sequence.");
}

std::string decode_field(std::string_view value) {
    std::string decoded;
    for (std::size_t index = 0; index < value.size(); ++index) {
        if (value[index] != '%') {
            decoded.push_back(value[index]);
            continue;
        }
        if (index + 2 >= value.size()) {
            throw std::runtime_error("Progress history contains an incomplete escape sequence.");
        }
        const unsigned char byte = static_cast<unsigned char>((hex_value(value[index + 1]) << 4U) |
                                                              hex_value(value[index + 2]));
        decoded.push_back(static_cast<char>(byte));
        index += 2;
    }
    return decoded;
}

void write_all(int descriptor, std::string_view content) {
    std::size_t written = 0;
    while (written < content.size()) {
        const ssize_t count =
            ::write(descriptor, content.data() + written, content.size() - written);
        if (count < 0) {
            if (errno == EINTR) {
                continue;
            }
            throw std::runtime_error("Failed to write progress data.");
        }
        written += static_cast<std::size_t>(count);
    }
}

void atomic_write(const std::filesystem::path& path, std::string_view content) {
    if (std::filesystem::is_symlink(path)) {
        throw std::runtime_error("Refusing to replace a symlinked progress record.");
    }

    auto temporary = path;
    temporary += ".tmp." + std::to_string(static_cast<long long>(::getpid()));
    if (std::filesystem::is_symlink(temporary)) {
        throw std::runtime_error("Refusing to write through a symlinked temporary file.");
    }
    std::error_code ignored;
    std::filesystem::remove(temporary, ignored);

    const int descriptor = ::open(
        temporary.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW, S_IRUSR | S_IWUSR);
    if (descriptor < 0) {
        throw std::runtime_error("Cannot create temporary progress file.");
    }

    try {
        write_all(descriptor, content);
        if (::fsync(descriptor) != 0) {
            throw std::runtime_error("Cannot flush temporary progress file.");
        }
    } catch (...) {
        ::close(descriptor);
        std::filesystem::remove(temporary, ignored);
        throw;
    }
    ::close(descriptor);

    std::error_code error;
    std::filesystem::rename(temporary, path, error);
    if (error) {
        std::filesystem::remove(temporary, ignored);
        throw std::runtime_error("Cannot publish progress record: " + error.message());
    }
}

std::vector<std::string> split_tabs(const std::string& line) {
    std::vector<std::string> fields;
    std::size_t start = 0;
    while (true) {
        const std::size_t position = line.find('\t', start);
        if (position == std::string::npos) {
            fields.push_back(line.substr(start));
            break;
        }
        fields.push_back(line.substr(start, position - start));
        start = position + 1;
    }
    return fields;
}

ProgressRecord parse_history_record(const std::vector<std::string>& fields,
                                    std::string_view task_id) {
    if (fields.size() != 6U) {
        throw std::runtime_error("Progress history row has an invalid column count.");
    }

    ProgressRecord record;
    record.evidence.task_id = std::string(task_id);
    record.evidence.timestamp = decode_field(fields[0]);
    if (fields[1] != "PASS" && fields[1] != "FAIL") {
        throw std::runtime_error("Progress history contains an invalid result.");
    }
    record.evidence.passed = fields[1] == "PASS";
    record.evidence.tests_passed = static_cast<unsigned>(std::stoul(fields[2]));
    record.evidence.tests_total = static_cast<unsigned>(std::stoul(fields[3]));
    record.evidence.fingerprint = decode_field(fields[4]);
    record.workspace_root = decode_field(fields[5]);
    return record;
}

std::string serialize_progress(const ProgressRecord& record) {
    std::ostringstream output;
    output << "format=1\n"
           << "task_id=" << record.evidence.task_id << '\n'
           << "workspace_root="
           << encode_field(
                  std::filesystem::absolute(record.workspace_root).lexically_normal().string())
           << '\n'
           << "fingerprint=" << encode_field(record.evidence.fingerprint) << '\n'
           << "result="
           << (!record.checked ? "STARTED" : (record.evidence.passed ? "PASS" : "FAIL")) << '\n'
           << "tests_passed=" << record.evidence.tests_passed << '\n'
           << "tests_total=" << record.evidence.tests_total << '\n'
           << "timestamp=" << encode_field(record.evidence.timestamp) << '\n';
    return output.str();
}

std::string serialize_history(const ProgressRecord& record) {
    std::ostringstream output;
    output << encode_field(record.evidence.timestamp) << '\t'
           << (record.evidence.passed ? "PASS" : "FAIL") << '\t' << record.evidence.tests_passed
           << '\t' << record.evidence.tests_total << '\t'
           << encode_field(record.evidence.fingerprint) << '\t'
           << encode_field(
                  std::filesystem::absolute(record.workspace_root).lexically_normal().string())
           << '\n';
    return output.str();
}

void append_history(const std::filesystem::path& path, std::string_view line) {
    if (std::filesystem::is_symlink(path)) {
        throw std::runtime_error("Refusing to append to a symlinked progress history.");
    }

    const int descriptor = ::open(
        path.c_str(), O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC | O_NOFOLLOW, S_IRUSR | S_IWUSR);
    if (descriptor < 0) {
        throw std::runtime_error("Cannot open progress history file.");
    }

    if (::flock(descriptor, LOCK_EX) != 0) {
        ::close(descriptor);
        throw std::runtime_error("Cannot lock progress history file.");
    }

    struct stat file_status {};
    if (::fstat(descriptor, &file_status) != 0 || file_status.st_size < 0 ||
        static_cast<std::uintmax_t>(file_status.st_size) + line.size() > kMaxHistoryBytes) {
        ::flock(descriptor, LOCK_UN);
        ::close(descriptor);
        throw std::runtime_error("Progress history reached its 1 MiB limit.");
    }

    try {
        write_all(descriptor, line);
        if (::fsync(descriptor) != 0) {
            throw std::runtime_error("Cannot flush progress history.");
        }
    } catch (...) {
        ::flock(descriptor, LOCK_UN);
        ::close(descriptor);
        throw;
    }

    ::flock(descriptor, LOCK_UN);
    ::close(descriptor);
}

}  // namespace

std::filesystem::path progress_directory() {
    if (const char* override_path = std::getenv("CPPAPPLAB_STATE_DIR");
        override_path != nullptr && override_path[0] != '\0') {
        return std::filesystem::absolute(override_path).lexically_normal() / "progress";
    }

    const char* home = std::getenv("HOME");
    if (home == nullptr || home[0] == '\0') {
        throw std::runtime_error(
            "HOME is not set; set CPPAPPLAB_STATE_DIR to choose a progress directory.");
    }

    return std::filesystem::path(home) / ".local" / "share" / "cppapplab" / "progress";
}

std::optional<ProgressRecord> read_progress(std::string_view task_id) {
    if (!valid_task_id(task_id)) {
        throw std::invalid_argument("Invalid task ID for progress lookup.");
    }

    const auto directory = progress_directory();
    std::error_code error;
    const auto directory_status = std::filesystem::symlink_status(directory, error);
    if (error == std::errc::no_such_file_or_directory) {
        return std::nullopt;
    }
    if (error) {
        throw std::runtime_error("Cannot inspect progress directory.");
    }
    if (std::filesystem::is_symlink(directory_status) ||
        !std::filesystem::is_directory(directory_status)) {
        throw std::runtime_error("Progress directory must be a real directory.");
    }

    const auto path = directory / (std::string(task_id) + ".progress");
    if (!std::filesystem::exists(path) && !std::filesystem::is_symlink(path)) {
        return std::nullopt;
    }

    const auto fields = parse_fields(read_regular_file(path, kMaxProgressBytes));
    if (required_field(fields, "format") != "1") {
        throw std::runtime_error("Unsupported progress record format.");
    }

    const std::string result = required_field(fields, "result");
    if (result != "PASS" && result != "FAIL" && result != "STARTED") {
        throw std::runtime_error("Progress result must be PASS, FAIL, or STARTED.");
    }

    ProgressRecord record;
    record.checked = result != "STARTED";
    record.evidence.task_id = required_field(fields, "task_id");
    record.evidence.fingerprint = decode_field(required_field(fields, "fingerprint"));
    record.evidence.passed = result == "PASS";
    record.evidence.tests_passed = parse_unsigned(fields, "tests_passed");
    record.evidence.tests_total = parse_unsigned(fields, "tests_total");
    record.evidence.timestamp = decode_field(required_field(fields, "timestamp"));
    record.workspace_root = decode_field(required_field(fields, "workspace_root"));

    if (record.evidence.task_id != std::string(task_id) || record.workspace_root.empty()) {
        throw std::runtime_error("Progress record task ID or workspace path is invalid.");
    }
    return record;
}

std::vector<ProgressRecord> read_progress_history(std::string_view task_id) {
    if (!valid_task_id(task_id)) {
        throw std::invalid_argument("Invalid task ID for progress history lookup.");
    }

    const auto path = progress_directory() / "history" / (std::string(task_id) + ".history");
    if (!std::filesystem::exists(path) && !std::filesystem::is_symlink(path)) {
        return {};
    }

    const std::string content = read_regular_file(path, kMaxHistoryBytes);
    std::vector<ProgressRecord> records;
    std::istringstream input(content);
    std::string line;
    while (std::getline(input, line)) {
        if (!line.empty()) {
            records.push_back(parse_history_record(split_tabs(line), task_id));
        }
    }
    return records;
}

void register_workspace(const std::filesystem::path& workspace_root) {
    const Workspace workspace = open_workspace(workspace_root);
    const std::string fingerprint = source_fingerprint(*workspace.task, workspace.root);

    const auto now = std::chrono::system_clock::now();
    const std::time_t timestamp = std::chrono::system_clock::to_time_t(now);
    std::tm utc{};
    char formatted[32]{};
    std::string timestamp_text = "unknown";
    if (::gmtime_r(&timestamp, &utc) != nullptr &&
        std::strftime(formatted, sizeof(formatted), "%Y-%m-%dT%H:%M:%SZ", &utc) != 0) {
        timestamp_text = formatted;
    }

    Evidence evidence;
    evidence.task_id = workspace.task->id;
    evidence.fingerprint = fingerprint;
    evidence.passed = false;
    evidence.tests_passed = 0U;
    evidence.tests_total = 0U;
    evidence.timestamp = timestamp_text;

    const ProgressRecord record{evidence, workspace.root, false};
    const auto directory = progress_directory();
    ensure_real_directory(directory);
    ensure_real_directory(directory / "history");
    atomic_write(directory / (evidence.task_id + ".progress"), serialize_progress(record));
}

void write_progress(const std::filesystem::path& workspace_root, const Evidence& evidence) {
    if (!valid_task_id(evidence.task_id)) {
        throw std::invalid_argument("Invalid task ID for progress write.");
    }

    const Workspace workspace = open_workspace(workspace_root);
    if (workspace.task->id != evidence.task_id) {
        throw std::runtime_error("Progress task ID does not match the workspace marker.");
    }

    const std::string current_fingerprint = source_fingerprint(*workspace.task, workspace.root);
    if (current_fingerprint != evidence.fingerprint) {
        throw std::runtime_error(
            "Learner source changed after the check; progress was not updated. Run check again.");
    }

    const auto local_evidence = read_evidence(workspace.root);
    if (!local_evidence.has_value() || local_evidence->task_id != evidence.task_id ||
        local_evidence->fingerprint != evidence.fingerprint ||
        local_evidence->passed != evidence.passed ||
        local_evidence->tests_passed != evidence.tests_passed ||
        local_evidence->tests_total != evidence.tests_total) {
        throw std::runtime_error(
            "Local evidence does not match the result being saved to course progress.");
    }

    const ProgressRecord record{evidence, workspace.root, true};
    const auto directory = progress_directory();
    ensure_real_directory(directory);
    ensure_real_directory(directory / "history");

    atomic_write(directory / (evidence.task_id + ".progress"), serialize_progress(record));
    append_history(directory / "history" / (evidence.task_id + ".history"),
                   serialize_history(record));
}

bool has_current_pass(std::string_view task_id) {
    try {
        std::map<std::string, ProgressRecord> latest_by_workspace;
        for (const ProgressRecord& attempt : read_progress_history(task_id)) {
            latest_by_workspace[attempt.workspace_root.string()] = attempt;
        }

        const auto latest = read_progress(task_id);
        if (latest.has_value() && latest->checked) {
            latest_by_workspace[latest->workspace_root.string()] = *latest;
        }

        for (const auto& item : latest_by_workspace) {
            const ProgressRecord& record = item.second;
            if (!record.checked || !record.evidence.passed) {
                continue;
            }

            try {
                const Workspace workspace = open_workspace(record.workspace_root);
                if (workspace.task->id != task_id) {
                    continue;
                }

                const auto local_evidence = read_evidence(workspace.root);
                if (!local_evidence.has_value() || !local_evidence->passed ||
                    local_evidence->fingerprint != record.evidence.fingerprint) {
                    continue;
                }

                if (source_fingerprint(*workspace.task, workspace.root) ==
                    record.evidence.fingerprint) {
                    return true;
                }
            } catch (...) {
                continue;
            }
        }
        return false;
    } catch (...) {
        return false;
    }
}

bool prerequisites_satisfied(const Task& task, std::string* missing_prerequisite) {
    for (const std::string& prerequisite : task.prerequisites) {
        if (!has_current_pass(prerequisite)) {
            if (missing_prerequisite != nullptr) {
                *missing_prerequisite = prerequisite;
            }
            return false;
        }
    }
    return true;
}

}  // namespace cppapplab
