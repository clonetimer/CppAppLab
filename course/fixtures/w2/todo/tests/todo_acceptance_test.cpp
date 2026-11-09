#include "todo.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>
#include <system_error>

namespace {

class TemporaryDirectory {
   public:
    TemporaryDirectory() {
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        path_ = std::filesystem::temp_directory_path() /
                ("cppapplab-todo-test-" + std::to_string(stamp));
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

TEST(TodoAcceptanceTest, NextIdStartsAtOneForAnEmptyList) {
    EXPECT_EQ(cppapplab::todo::next_id(cppapplab::todo::List{}), 1U);
}

TEST(TodoAcceptanceTest, NextIdUsesTheLargestExistingId) {
    const cppapplab::todo::List items{
        {2U, "second", false}, {9U, "ninth", true}, {4U, "fourth", false}};

    EXPECT_EQ(cppapplab::todo::next_id(items), 10U);
}

TEST(TodoAcceptanceTest, NextIdReportsExhaustedIdSpace) {
    const cppapplab::todo::List items{{std::numeric_limits<std::uint64_t>::max(), "last", false}};

    EXPECT_EQ(cppapplab::todo::next_id(items), 0U);
}

TEST(TodoAcceptanceTest, AddCreatesPendingItemAndPreservesTitle) {
    cppapplab::todo::List items;

    const auto result = cppapplab::todo::add(items, 7U, "Review STL containers");

    ASSERT_TRUE(result);
    ASSERT_EQ(items.size(), 1U);
    EXPECT_EQ(items.front().id, 7U);
    EXPECT_EQ(items.front().title, "Review STL containers");
    EXPECT_FALSE(items.front().completed);
}

TEST(TodoAcceptanceTest, AddRejectsZeroIdAndInvalidTitles) {
    cppapplab::todo::List items;

    EXPECT_EQ(cppapplab::todo::add(items, 0U, "valid title").error,
              cppapplab::todo::Error::InvalidId);
    EXPECT_EQ(cppapplab::todo::add(items, 1U, "").error, cppapplab::todo::Error::InvalidTitle);
    EXPECT_EQ(cppapplab::todo::add(items, 2U, " \t\r\n").error,
              cppapplab::todo::Error::InvalidTitle);
    EXPECT_TRUE(items.empty());
}

TEST(TodoAcceptanceTest, AddRejectsDuplicateIds) {
    cppapplab::todo::List items{{1U, "existing", false}};

    const auto result = cppapplab::todo::add(items, 1U, "duplicate");

    EXPECT_EQ(result.error, cppapplab::todo::Error::DuplicateId);
    ASSERT_EQ(items.size(), 1U);
    EXPECT_EQ(items.front().title, "existing");
}

TEST(TodoAcceptanceTest, MarkDoneUpdatesAnExistingItem) {
    cppapplab::todo::List items{{3U, "finish task", false}};

    const auto result = cppapplab::todo::mark_done(items, 3U);

    ASSERT_TRUE(result);
    ASSERT_EQ(items.size(), 1U);
    EXPECT_TRUE(items.front().completed);
}

TEST(TodoAcceptanceTest, MarkDoneReportsMissingItem) {
    cppapplab::todo::List items{{3U, "existing", false}};

    const auto result = cppapplab::todo::mark_done(items, 99U);

    EXPECT_EQ(result.error, cppapplab::todo::Error::NotFound);
    EXPECT_FALSE(items.front().completed);
}

TEST(TodoAcceptanceTest, RemoveErasesAnExistingItem) {
    cppapplab::todo::List items{{1U, "keep", false}, {2U, "remove", true}, {3U, "keep too", false}};

    const auto result = cppapplab::todo::remove(items, 2U);

    ASSERT_TRUE(result);
    ASSERT_EQ(items.size(), 2U);
    EXPECT_EQ(items[0].id, 1U);
    EXPECT_EQ(items[1].id, 3U);
}

TEST(TodoAcceptanceTest, RemoveReportsMissingItem) {
    cppapplab::todo::List items{{1U, "keep", false}};

    const auto result = cppapplab::todo::remove(items, 99U);

    EXPECT_EQ(result.error, cppapplab::todo::Error::NotFound);
    ASSERT_EQ(items.size(), 1U);
}

TEST(TodoAcceptanceTest, SaveAndLoadRoundTripEscapedText) {
    TemporaryDirectory temporary;
    const auto path = temporary.path() / "nested" / "todos.tsv";
    const std::string title = "review\tparser\nand \"quotes\" \\ path\r";
    cppapplab::todo::List original{{1U, title, false}, {4U, "completed item", true}};
    cppapplab::todo::List loaded;

    ASSERT_TRUE(cppapplab::todo::save(original, path));
    ASSERT_TRUE(cppapplab::todo::load(loaded, path));

    ASSERT_EQ(loaded.size(), 2U);
    EXPECT_EQ(loaded[0].id, 1U);
    EXPECT_EQ(loaded[0].title, title);
    EXPECT_FALSE(loaded[0].completed);
    EXPECT_EQ(loaded[1].id, 4U);
    EXPECT_EQ(loaded[1].title, "completed item");
    EXPECT_TRUE(loaded[1].completed);
}

TEST(TodoAcceptanceTest, LoadOfMissingFileCreatesAnEmptyLogicalList) {
    TemporaryDirectory temporary;
    cppapplab::todo::List items{{8U, "old item", true}};

    const auto result = cppapplab::todo::load(items, temporary.path() / "not-created.tsv");

    ASSERT_TRUE(result);
    EXPECT_TRUE(items.empty());
}

TEST(TodoAcceptanceTest, MalformedFileDoesNotReplaceExistingItems) {
    TemporaryDirectory temporary;
    const auto path = temporary.path() / "malformed.tsv";
    write_file(path,
               "CPPAPPLAB_TODO_V1\n"
               "1\t0\tvalid record parsed before failure\n"
               "this record is malformed\n");
    cppapplab::todo::List items{{5U, "keep this", true}};

    const auto result = cppapplab::todo::load(items, path);

    EXPECT_EQ(result.error, cppapplab::todo::Error::InvalidFormat);
    ASSERT_EQ(items.size(), 1U);
    EXPECT_EQ(items.front().id, 5U);
    EXPECT_EQ(items.front().title, "keep this");
    EXPECT_TRUE(items.front().completed);
}

TEST(TodoAcceptanceTest, LoadRejectsUnsupportedEscapeSequence) {
    TemporaryDirectory temporary;
    const auto path = temporary.path() / "bad-escape.tsv";
    write_file(path,
               "CPPAPPLAB_TODO_V1\n"
               "1\t0\tinvalid\\qescape\n");
    cppapplab::todo::List items;

    const auto result = cppapplab::todo::load(items, path);

    EXPECT_EQ(result.error, cppapplab::todo::Error::InvalidFormat);
    EXPECT_TRUE(items.empty());
}

TEST(TodoAcceptanceTest, LoadRejectsInvalidCompletionFlag) {
    TemporaryDirectory temporary;
    const auto path = temporary.path() / "bad-flag.tsv";
    write_file(path,
               "CPPAPPLAB_TODO_V1\n"
               "1\t2\tinvalid completion flag\n");
    cppapplab::todo::List items;

    const auto result = cppapplab::todo::load(items, path);

    EXPECT_EQ(result.error, cppapplab::todo::Error::InvalidFormat);
    EXPECT_TRUE(items.empty());
}

TEST(TodoAcceptanceTest, LoadRejectsDuplicateIds) {
    TemporaryDirectory temporary;
    const auto path = temporary.path() / "duplicate.tsv";
    write_file(path,
               "CPPAPPLAB_TODO_V1\n"
               "1\t0\tfirst\n"
               "1\t1\tsecond\n");
    cppapplab::todo::List items;

    const auto result = cppapplab::todo::load(items, path);

    EXPECT_EQ(result.error, cppapplab::todo::Error::InvalidFormat);
    EXPECT_TRUE(items.empty());
}

TEST(TodoAcceptanceTest, LoadRejectsSymlinkedDataFile) {
    TemporaryDirectory temporary;
    const auto target = temporary.path() / "target.tsv";
    const auto link = temporary.path() / "linked.tsv";
    write_file(target, "CPPAPPLAB_TODO_V1\n");
    std::filesystem::create_symlink(target, link);
    cppapplab::todo::List items;

    const auto result = cppapplab::todo::load(items, link);

    EXPECT_EQ(result.error, cppapplab::todo::Error::IoError);
}

TEST(TodoAcceptanceTest, SaveRefusesSymlinkedDestination) {
    TemporaryDirectory temporary;
    const auto target = temporary.path() / "target.tsv";
    const auto link = temporary.path() / "linked.tsv";
    write_file(target, "do not overwrite\n");
    std::filesystem::create_symlink(target, link);
    const cppapplab::todo::List items{{1U, "new data", false}};

    const auto result = cppapplab::todo::save(items, link);

    EXPECT_EQ(result.error, cppapplab::todo::Error::IoError);
    std::ifstream input(target, std::ios::binary);
    std::string content;
    std::getline(input, content);
    EXPECT_EQ(content, "do not overwrite");
}
