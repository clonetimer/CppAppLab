#include "task_manager.hpp"

#include <algorithm>
#include <charconv>
#include <fstream>
#include <limits>
#include <system_error>
#include <unordered_map>
#include <utility>

namespace cppapplab::tasks {
namespace {

constexpr std::size_t kMaxFileBytes = 1024U * 1024U;
constexpr std::size_t kMaxTitleBytes = 4096U;
constexpr std::string_view kFileHeader = "CPPAPPLAB_TODO_V1";

Result make_result(Error error, std::string message) {
    return {error, std::move(message)};
}

bool valid_title(std::string_view title) {
    if (title.empty() || title.size() > kMaxTitleBytes) {
        return false;
    }
    return std::any_of(title.begin(), title.end(), [](unsigned char character) {
        return character != ' ' && character != '\t' &&
               character != '\r' && character != '\n';
    });
}

std::string escape_title(std::string_view title) {
    std::string escaped;
    escaped.reserve(title.size());
    for (const char character : title) {
        switch (character) {
            case '\\':
                escaped += "\\\\";
                break;
            case '\t':
                escaped += "\\t";
                break;
            case '\n':
                escaped += "\\n";
                break;
            case '\r':
                escaped += "\\r";
                break;
            default:
                escaped.push_back(character);
                break;
        }
    }
    return escaped;
}

bool unescape_title(std::string_view encoded, std::string& title) {
    title.clear();
    title.reserve(encoded.size());
    for (std::size_t index = 0; index < encoded.size(); ++index) {
        if (encoded[index] != '\\') {
            title.push_back(encoded[index]);
            continue;
        }
        if (++index >= encoded.size()) {
            return false;
        }
        switch (encoded[index]) {
            case '\\':
                title.push_back('\\');
                break;
            case 't':
                title.push_back('\t');
                break;
            case 'n':
                title.push_back('\n');
                break;
            case 'r':
                title.push_back('\r');
                break;
            default:
                return false;
        }
    }
    return valid_title(title);
}

bool parse_id(std::string_view text, std::uint64_t& id) {
    if (text.empty()) {
        return false;
    }
    const char* begin = text.data();
    const char* end = begin + text.size();
    const auto parsed = std::from_chars(begin, end, id);
    return parsed.ec == std::errc{} && parsed.ptr == end && id != 0U;
}

Result atomic_write(const std::filesystem::path& path, std::string_view content) {
    if (std::filesystem::is_symlink(path)) {
        return make_result(Error::IoError, "refusing to replace a symlinked data file");
    }

    const auto parent = path.parent_path();
    if (!parent.empty()) {
        std::error_code error;
        std::filesystem::create_directories(parent, error);
        if (error) {
            return make_result(Error::IoError, "cannot create data directory: " + error.message());
        }
    }

    auto temporary = path;
    temporary += ".tmp";
    if (std::filesystem::is_symlink(temporary)) {
        return make_result(Error::IoError, "refusing to write through a symlinked temporary file");
    }

    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        if (!output) {
            return make_result(Error::IoError, "cannot create temporary data file");
        }
        output.write(content.data(), static_cast<std::streamsize>(content.size()));
        output.flush();
        if (!output) {
            output.close();
            std::error_code ignored;
            std::filesystem::remove(temporary, ignored);
            return make_result(Error::IoError, "failed while writing temporary data file");
        }
    }

    std::error_code error;
    std::filesystem::rename(temporary, path, error);
    if (error) {
        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
        return make_result(Error::IoError, "cannot publish data file: " + error.message());
    }
    return {};
}

}  // namespace

TaskManager::TaskManager(const TaskManager& other) {
    tasks_.reserve(other.tasks_.size());
    for (const auto& task : other.tasks_) {
        tasks_.push_back(std::make_unique<Task>(*task));
    }
}

TaskManager& TaskManager::operator=(const TaskManager& other) {
    if (this != &other) {
        TaskManager copy(other);
        swap(copy);
    }
    return *this;
}

TaskManager::TaskManager(TaskManager&& other) noexcept
    : tasks_(std::move(other.tasks_)) {
    other.tasks_.clear();
}

TaskManager& TaskManager::operator=(TaskManager&& other) noexcept {
    if (this != &other) {
        tasks_ = std::move(other.tasks_);
        other.tasks_.clear();
    }
    return *this;
}

Result TaskManager::add(std::uint64_t id, std::string_view title) {
    if (id == 0U) {
        return make_result(Error::InvalidId, "task ID must be positive");
    }
    if (!valid_title(title)) {
        return make_result(Error::InvalidTitle, "title cannot be empty or whitespace-only");
    }
    if (find(id) != nullptr) {
        return make_result(Error::DuplicateId, "task ID already exists");
    }

    tasks_.push_back(std::make_unique<Task>(id, std::string(title)));
    return {};
}

Result TaskManager::mark_done(std::uint64_t id) {
    if (id == 0U) {
        return make_result(Error::InvalidId, "task ID must be positive");
    }
    Task* task = find(id);
    if (task == nullptr) {
        return make_result(Error::NotFound, "task not found");
    }
    task->mark_done();
    return {};
}

Result TaskManager::remove(std::uint64_t id) {
    if (id == 0U) {
        return make_result(Error::InvalidId, "task ID must be positive");
    }
    const auto found = std::find_if(
        tasks_.begin(),
        tasks_.end(),
        [id](const std::unique_ptr<Task>& task) {
            return task->id() == id;
        }
    );
    if (found == tasks_.end()) {
        return make_result(Error::NotFound, "task not found");
    }

    tasks_.erase(found);
    return {};
}

Task* TaskManager::find(std::uint64_t id) noexcept {
    const auto found = std::find_if(
        tasks_.begin(),
        tasks_.end(),
        [id](const std::unique_ptr<Task>& task) {
            return task->id() == id;
        }
    );
    return found == tasks_.end() ? nullptr : found->get();
}

const Task* TaskManager::find(std::uint64_t id) const noexcept {
    const auto found = std::find_if(
        tasks_.begin(),
        tasks_.end(),
        [id](const std::unique_ptr<Task>& task) {
            return task->id() == id;
        }
    );
    return found == tasks_.end() ? nullptr : found->get();
}

std::uint64_t TaskManager::next_id() const noexcept {
    if (tasks_.empty()) {
        return 1U;
    }

    const auto largest = std::max_element(
        tasks_.begin(),
        tasks_.end(),
        [](const std::unique_ptr<Task>& left, const std::unique_ptr<Task>& right) {
            return left->id() < right->id();
        }
    );
    if ((*largest)->id() == std::numeric_limits<std::uint64_t>::max()) {
        return 0U;
    }
    return (*largest)->id() + 1U;
}

std::vector<Task> TaskManager::snapshot() const {
    std::vector<Task> result;
    result.reserve(tasks_.size());
    for (const auto& task : tasks_) {
        result.push_back(*task);
    }
    std::sort(result.begin(), result.end());
    return result;
}

std::size_t TaskManager::size() const noexcept {
    return tasks_.size();
}

bool TaskManager::empty() const noexcept {
    return tasks_.empty();
}

Result TaskManager::save(const std::filesystem::path& path) const {
    if (path.empty()) {
        return make_result(Error::IoError, "data file path is empty");
    }

    const std::vector<Task> items = snapshot();
    std::string content;
    content.reserve(64U + items.size() * 64U);
    content.append(kFileHeader);
    content.push_back('\n');
    for (const Task& task : items) {
        content += std::to_string(task.id());
        content.push_back('\t');
        content.push_back(task.completed() ? '1' : '0');
        content.push_back('\t');
        content += escape_title(task.title());
        content.push_back('\n');
    }

    if (content.size() > kMaxFileBytes) {
        return make_result(Error::InvalidFormat, "task data exceeds the 1 MiB file limit");
    }
    return atomic_write(path, content);
}

Result TaskManager::load(const std::filesystem::path& path) {
    if (path.empty()) {
        return make_result(Error::IoError, "data file path is empty");
    }

    std::error_code error;
    const auto status = std::filesystem::symlink_status(path, error);
    if (error == std::errc::no_such_file_or_directory) {
        tasks_.clear();
        return {};
    }
    if (error) {
        return make_result(Error::IoError, "cannot inspect task data file: " + error.message());
    }
    if (std::filesystem::is_symlink(status) || !std::filesystem::is_regular_file(status)) {
        return make_result(Error::IoError, "task data path must be a regular non-symlink file");
    }

    const auto size = std::filesystem::file_size(path, error);
    if (error) {
        return make_result(Error::IoError, "cannot determine task data file size");
    }
    if (size > kMaxFileBytes) {
        return make_result(Error::InvalidFormat, "task data exceeds the 1 MiB file limit");
    }

    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return make_result(Error::IoError, "cannot open task data file");
    }

    std::string header;
    if (!std::getline(input, header)) {
        return make_result(Error::InvalidFormat, "task data header is missing or unsupported");
    }
    if (!header.empty() && header.back() == '\r') {
        header.pop_back();
    }
    if (header != kFileHeader) {
        return make_result(Error::InvalidFormat, "task data header is missing or unsupported");
    }

    TaskManager candidate;
    std::unordered_map<std::uint64_t, bool> seen;
    std::string line;
    while (std::getline(input, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.empty()) {
            return make_result(Error::InvalidFormat, "task data contains an empty record");
        }

        const std::size_t first_separator = line.find('\t');
        const std::size_t second_separator = first_separator == std::string::npos
                                                 ? std::string::npos
                                                 : line.find('\t', first_separator + 1U);
        if (first_separator == std::string::npos || second_separator == std::string::npos) {
            return make_result(Error::InvalidFormat, "task record must contain three fields");
        }

        std::uint64_t id = 0U;
        if (!parse_id(std::string_view(line).substr(0, first_separator), id)) {
            return make_result(Error::InvalidFormat, "task record contains an invalid ID");
        }

        const std::string_view completed_text = std::string_view(line).substr(
            first_separator + 1U,
            second_separator - first_separator - 1U
        );
        if (completed_text != "0" && completed_text != "1") {
            return make_result(Error::InvalidFormat, "task record contains an invalid completion flag");
        }

        std::string title;
        if (!unescape_title(std::string_view(line).substr(second_separator + 1U), title)) {
            return make_result(Error::InvalidFormat, "task record contains an invalid title");
        }
        if (!seen.emplace(id, true).second) {
            return make_result(Error::InvalidFormat, "task data contains a duplicate ID");
        }

        const Result added = candidate.add(id, title);
        if (!added) {
            return make_result(Error::InvalidFormat, "task record violates the task invariants");
        }
        if (completed_text == "1") {
            (void)candidate.mark_done(id);
        }
    }

    if (!input.eof() && input.fail()) {
        return make_result(Error::IoError, "failed while reading task data file");
    }

    swap(candidate);
    return {};
}

void TaskManager::swap(TaskManager& other) noexcept {
    tasks_.swap(other.tasks_);
}

}  // namespace cppapplab::tasks
