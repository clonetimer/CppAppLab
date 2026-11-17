#include "cppapplab/learning.hpp"
#include "cppapplab/process.hpp"
#include "cppapplab/task.hpp"
#include "cppapplab/workspace.hpp"

#include <gtest/gtest.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <csignal>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {

class TemporaryDirectory {
   public:
    TemporaryDirectory() {
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        path_ = std::filesystem::temp_directory_path() /
                ("cppapplab-application-runtime-test-" + std::to_string(stamp));
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

class ScopedEnvironment {
   public:
    ScopedEnvironment(std::string name, std::string value) : name_(std::move(name)) {
        if (const char* previous = std::getenv(name_.c_str())) {
            previous_ = previous;
            had_previous_ = true;
        }
        if (::setenv(name_.c_str(), value.c_str(), 1) != 0) {
            throw std::runtime_error("Cannot set test environment variable: " + name_);
        }
    }

    ~ScopedEnvironment() {
        if (had_previous_) {
            (void)::setenv(name_.c_str(), previous_.c_str(), 1);
        } else {
            (void)::unsetenv(name_.c_str());
        }
    }

    ScopedEnvironment(const ScopedEnvironment&) = delete;
    ScopedEnvironment& operator=(const ScopedEnvironment&) = delete;

   private:
    std::string name_;
    std::string previous_;
    bool had_previous_{false};
};

class StdoutCapture {
   public:
    StdoutCapture()
        : previous_(std::cout.rdbuf(stream_.rdbuf())),
          previous_error_(std::cerr.rdbuf(stream_.rdbuf())) {}
    ~StdoutCapture() {
        std::cout.rdbuf(previous_);
        std::cerr.rdbuf(previous_error_);
    }
    [[nodiscard]] std::string str() const { return stream_.str(); }

   private:
    std::ostringstream stream_;
    std::streambuf* previous_;
    std::streambuf* previous_error_;
};

int invoke(const std::vector<std::string>& arguments) {
    std::vector<char*> argv;
    argv.reserve(arguments.size());
    for (const std::string& argument : arguments) {
        argv.push_back(const_cast<char*>(argument.c_str()));
    }
    return cppapplab::run_cli(static_cast<int>(argv.size()), argv.data());
}

class ScopedInputCapture {
   public:
    explicit ScopedInputCapture(const std::string& input) : stream_(input), previous_(std::cin.rdbuf(stream_.rdbuf())) {
        std::cin.clear();
    }
    ~ScopedInputCapture() {
        std::cin.rdbuf(previous_);
        std::cin.clear();
    }
    ScopedInputCapture(const ScopedInputCapture&) = delete;
    ScopedInputCapture& operator=(const ScopedInputCapture&) = delete;

   private:
    std::istringstream stream_;
    std::streambuf* previous_;
};

std::string invoke_capture(const std::vector<std::string>& arguments, int& result) {
    StdoutCapture capture;
    result = invoke(arguments);
    return capture.str();
}

void copy_reference(const std::filesystem::path& workspace,
                    const std::filesystem::path& reference,
                    const std::vector<std::string>& files) {
    for (const std::string& relative : files) {
        std::filesystem::copy_file(reference / relative, workspace / relative,
                                   std::filesystem::copy_options::overwrite_existing);
    }
}

std::uint16_t reserve_test_loopback_port() {
    const int descriptor = ::socket(AF_INET, SOCK_STREAM, 0);
    if (descriptor < 0) {
        throw std::runtime_error("Cannot create a test TCP socket.");
    }
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = 0U;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (::bind(descriptor, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) < 0) {
        (void)::close(descriptor);
        throw std::runtime_error("Cannot bind a test TCP socket.");
    }
    socklen_t length = sizeof(address);
    if (::getsockname(descriptor, reinterpret_cast<sockaddr*>(&address), &length) < 0) {
        (void)::close(descriptor);
        throw std::runtime_error("Cannot inspect the test TCP port.");
    }
    const std::uint16_t port = ntohs(address.sin_port);
    (void)::close(descriptor);
    return port;
}

}  // namespace

TEST(ApplicationRuntimeWorkflowTest, CalculatorBuildAndRunUsesFixedProfileWithoutWritingEvidence) {
    const cppapplab::Task* task = cppapplab::find_task("w1.calculator");
    ASSERT_NE(task, nullptr);
    TemporaryDirectory temporary;
    ScopedEnvironment state("CPPAPPLAB_STATE_DIR", (temporary.path() / "state").string());

    const auto workspace = cppapplab::create_workspace(*task, temporary.path() / "calculator");
    const auto reference = cppapplab::data_root() / "labs/w1/calculator/reference";
    copy_reference(workspace.root, reference, {"src/calculator.cpp"});

    int result = -1;
    std::string output = invoke_capture(
        {"cppapplab", "app-run", workspace.root.string(), "calculate", "add", "20", "22"},
        result);
    EXPECT_EQ(result, 2);
    EXPECT_NE(output.find("build is missing or stale"), std::string::npos);

    output = invoke_capture({"cppapplab", "app-build", workspace.root.string()}, result);
    ASSERT_EQ(result, 0) << output;
    EXPECT_NE(output.find("Application build result: PASS"), std::string::npos);

    output = invoke_capture(
        {"cppapplab", "app-run", workspace.root.string(), "calculate", "add", "20", "22"},
        result);
    EXPECT_EQ(result, 0) << output;
    EXPECT_NE(output.find("42"), std::string::npos);
    EXPECT_NE(output.find("Application result: PASS"), std::string::npos);
    EXPECT_FALSE(std::filesystem::exists(workspace.root / ".cppapplab" / "evidence.txt"));

    {
        std::ofstream changed(workspace.root / "src/calculator.cpp", std::ios::app);
        ASSERT_TRUE(changed.good());
        changed << "\n";
    }
    output = invoke_capture(
        {"cppapplab", "app-run", workspace.root.string(), "calculate", "add", "20", "22"},
        result);
    EXPECT_EQ(result, 2);
    EXPECT_NE(output.find("stale"), std::string::npos);
}

TEST(ApplicationRuntimeWorkflowTest, TodoApplicationStateIsIsolatedToItsWorkspace) {
    const cppapplab::Task* task = cppapplab::find_task("w2.todo-cli");
    ASSERT_NE(task, nullptr);
    TemporaryDirectory temporary;
    ScopedEnvironment state("CPPAPPLAB_STATE_DIR", (temporary.path() / "state").string());
    const auto global_todo_file = temporary.path() / "global-todos.tsv";
    ScopedEnvironment global_todo("CPPAPPLAB_TODO_FILE", global_todo_file.string());

    const auto workspace = cppapplab::create_workspace(*task, temporary.path() / "todo");
    const auto reference = cppapplab::data_root() / "labs/w2/todo/reference";
    copy_reference(workspace.root, reference, {"src/todo.cpp"});

    int result = -1;
    std::string output = invoke_capture({"cppapplab", "app-build", workspace.root.string()}, result);
    ASSERT_EQ(result, 0) << output;

    output = invoke_capture(
        {"cppapplab", "app-run", workspace.root.string(), "add", "write browser tests"},
        result);
    EXPECT_EQ(result, 0) << output;
    output = invoke_capture({"cppapplab", "app-run", workspace.root.string(), "list"}, result);
    EXPECT_EQ(result, 0) << output;
    EXPECT_NE(output.find("write browser tests"), std::string::npos);

    EXPECT_TRUE(std::filesystem::exists(
        workspace.root / ".cppapplab" / "application" / "runtime" / "todos.tsv"));
    EXPECT_FALSE(std::filesystem::exists(global_todo_file));
}

TEST(ApplicationRuntimeWorkflowTest, TaskManagerApplicationStateIsIsolatedToItsWorkspace) {
    const cppapplab::Task* task = cppapplab::find_task("w3.task-manager");
    ASSERT_NE(task, nullptr);
    TemporaryDirectory temporary;
    ScopedEnvironment state("CPPAPPLAB_STATE_DIR", (temporary.path() / "state").string());
    const auto global_todo_file = temporary.path() / "global-todos.tsv";
    ScopedEnvironment global_todo("CPPAPPLAB_TODO_FILE", global_todo_file.string());

    const auto workspace = cppapplab::create_workspace(*task, temporary.path() / "task-manager");
    const auto reference = cppapplab::data_root() / "labs/w3/task-manager/reference";
    copy_reference(workspace.root, reference, {"src/task.cpp", "src/task_manager.cpp"});

    int result = -1;
    std::string output = invoke_capture({"cppapplab", "app-build", workspace.root.string()}, result);
    ASSERT_EQ(result, 0) << output;

    output = invoke_capture(
        {"cppapplab", "app-run", workspace.root.string(), "add", "workspace scoped task"}, result);
    EXPECT_EQ(result, 0) << output;
    EXPECT_NE(output.find("Added #1: workspace scoped task"), std::string::npos);

    output = invoke_capture({"cppapplab", "app-run", workspace.root.string(), "list"}, result);
    EXPECT_EQ(result, 0) << output;
    EXPECT_NE(output.find("#1 [ ] workspace scoped task"), std::string::npos);

    output = invoke_capture(
        {"cppapplab", "app-run", workspace.root.string(), "done", "1"}, result);
    EXPECT_EQ(result, 0) << output;
    EXPECT_NE(output.find("Completed #1"), std::string::npos);

    output = invoke_capture({"cppapplab", "app-run", workspace.root.string(), "list"}, result);
    EXPECT_EQ(result, 0) << output;
    EXPECT_NE(output.find("#1 [x] workspace scoped task"), std::string::npos);

    output = invoke_capture(
        {"cppapplab", "app-run", workspace.root.string(), "remove", "1"}, result);
    EXPECT_EQ(result, 0) << output;
    EXPECT_NE(output.find("Removed #1"), std::string::npos);

    output = invoke_capture({"cppapplab", "app-run", workspace.root.string(), "list"}, result);
    EXPECT_EQ(result, 0) << output;
    EXPECT_NE(output.find("No tasks."), std::string::npos);

    EXPECT_TRUE(std::filesystem::is_regular_file(
        workspace.root / ".cppapplab" / "application" / "runtime" / "todos.tsv"));
    EXPECT_FALSE(std::filesystem::exists(global_todo_file));
    EXPECT_FALSE(std::filesystem::exists(workspace.root / ".cppapplab" / "evidence.txt"));
}

TEST(ApplicationRuntimeWorkflowTest, SystemLabOnlyRunsItsFixedEchoDemonstration) {
    const cppapplab::Task* task = cppapplab::find_task("w5.system-lab");
    ASSERT_NE(task, nullptr);
    TemporaryDirectory temporary;
    ScopedEnvironment state("CPPAPPLAB_STATE_DIR", (temporary.path() / "state").string());

    const auto workspace = cppapplab::create_workspace(*task, temporary.path() / "system-lab");
    int result = -1;
    std::string output = invoke_capture({"cppapplab", "app-build", workspace.root.string()}, result);
    ASSERT_EQ(result, 0) << output;

    output = invoke_capture({"cppapplab", "app-run", workspace.root.string(), "echo"}, result);
    EXPECT_NE(result, 0) << output;
    EXPECT_NE(output.find("Application result: FAIL"), std::string::npos);

    output = invoke_capture(
        {"cppapplab", "app-run", workspace.root.string(), "execute", "/bin/sh", "-c", "id"},
        result);
    EXPECT_EQ(result, 2);
    EXPECT_NE(output.find("System Lab supports only the fixed echo demonstration"), std::string::npos);
}

TEST(ApplicationRuntimeWorkflowTest, ThreadPoolDemoRunsThroughItsFixedApplicationProfile) {
    const cppapplab::Task* task = cppapplab::find_task("w6.thread-pool");
    ASSERT_NE(task, nullptr);
    TemporaryDirectory temporary;
    ScopedEnvironment state("CPPAPPLAB_STATE_DIR", temporary.path() / "state");

    const auto workspace = cppapplab::create_workspace(*task, temporary.path() / "thread-pool");
    const auto reference = cppapplab::data_root() / "labs/w6/thread-pool/reference";
    copy_reference(workspace.root, reference, {"src/thread_pool.cpp"});

    int result = -1;
    std::string output = invoke_capture({"cppapplab", "app-build", workspace.root.string()}, result);
    ASSERT_EQ(result, 0) << output;
    output = invoke_capture({"cppapplab", "app-run", workspace.root.string(), "demo"}, result);
    // The learner's starter implementation is intentionally incomplete. The
    // browser must run that learner source and surface its TODO diagnostic,
    // rather than silently substituting the reference implementation.
    EXPECT_EQ(result, 1) << output;
    EXPECT_NE(output.find("TODO: implement ThreadPool::submit"), std::string::npos);
    EXPECT_NE(output.find("Application result: FAIL"), std::string::npos);
}

TEST(ApplicationRuntimeWorkflowTest, TcpEchoProfileBuildsAndValidatesItsFixedClientAction) {
    const cppapplab::Task* task = cppapplab::find_task("w7.tcp-echo");
    ASSERT_NE(task, nullptr);
    TemporaryDirectory temporary;
    ScopedEnvironment state("CPPAPPLAB_STATE_DIR", (temporary.path() / "state").string());
    const auto workspace = cppapplab::create_workspace(*task, temporary.path() / "tcp-echo");

    int result = -1;
    std::string output = invoke_capture({"cppapplab", "app-build", workspace.root.string()}, result);
    ASSERT_EQ(result, 0) << output;
    EXPECT_NE(output.find("src/tcp_echo.cpp"), std::string::npos);

    output = invoke_capture(
        {"cppapplab", "app-run", workspace.root.string(), "client", "0", "hello"}, result);
    EXPECT_EQ(result, 2);
    EXPECT_NE(output.find("port must be in the range 1..65535"), std::string::npos);
    EXPECT_FALSE(std::filesystem::exists(workspace.root / ".cppapplab" / "evidence.txt"));
}

TEST(ApplicationRuntimeWorkflowTest, TcpEchoServiceAndRegisteredLearnerClientCompleteARealRoundTrip) {
    const cppapplab::Task* task = cppapplab::find_task("w7.tcp-echo");
    ASSERT_NE(task, nullptr);
    TemporaryDirectory temporary;
    ScopedEnvironment state("CPPAPPLAB_STATE_DIR", (temporary.path() / "state").string());
    const auto workspace = cppapplab::create_workspace(*task, temporary.path() / "tcp-echo-roundtrip");

    int result = -1;
    std::string output = invoke_capture({"cppapplab", "app-build", workspace.root.string()}, result);
    ASSERT_EQ(result, 0) << output;

    const int reservation = ::socket(AF_INET, SOCK_STREAM, 0);
    ASSERT_GE(reservation, 0);
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = 0;
    ASSERT_EQ(::bind(reservation, reinterpret_cast<sockaddr*>(&address), sizeof(address)), 0);
    socklen_t address_size = sizeof(address);
    ASSERT_EQ(::getsockname(reservation, reinterpret_cast<sockaddr*>(&address), &address_size), 0);
    const unsigned port = ntohs(address.sin_port);
    ASSERT_GT(port, 0U);
    (void)::close(reservation);

    const auto executable = workspace.root / ".cppapplab" / "application" / "learner_app";
    const pid_t server = ::fork();
    ASSERT_GE(server, 0);
    if (server == 0) {
        const std::string port_text = std::to_string(port);
        ::execl(executable.c_str(), executable.c_str(), "server", port_text.c_str(),
                static_cast<char*>(nullptr));
        _exit(127);
    }

    bool round_trip_passed = false;
    bool server_reaped = false;
    int server_status = 0;
    for (int attempt = 0; attempt < 30; ++attempt) {
        output = invoke_capture(
            {"cppapplab", "app-run", workspace.root.string(), "client",
             std::to_string(port), "hello from browser"}, result);
        if (result == 0) {
            round_trip_passed = true;
            break;
        }
        const pid_t waited = ::waitpid(server, &server_status, WNOHANG);
        if (waited == server) {
            server_reaped = true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    if (!server_reaped) {
        for (int attempt = 0; attempt < 30; ++attempt) {
            const pid_t waited = ::waitpid(server, &server_status, WNOHANG);
            if (waited == server) {
                server_reaped = true;
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }
    if (!server_reaped) {
        (void)::kill(server, SIGKILL);
        (void)::waitpid(server, &server_status, 0);
        server_reaped = true;
    }

    EXPECT_TRUE(round_trip_passed) << output;
    EXPECT_TRUE(server_reaped);
    if (round_trip_passed) {
        EXPECT_NE(output.find("hello from browser"), std::string::npos);
        EXPECT_NE(output.find("Application result: PASS"), std::string::npos);
    }
    EXPECT_FALSE(std::filesystem::exists(workspace.root / ".cppapplab" / "evidence.txt"));
}
TEST(ApplicationRuntimeWorkflowTest, TcpEchoLearnerServerAndRegisteredClientCompleteARealRoundTrip) {
    const cppapplab::Task* task = cppapplab::find_task("w7.tcp-echo");
    ASSERT_NE(task, nullptr);
    TemporaryDirectory temporary;
    ScopedEnvironment state("CPPAPPLAB_STATE_DIR", (temporary.path() / "state").string());
    const auto workspace = cppapplab::create_workspace(*task, temporary.path() / "tcp-echo-round-trip");

    int result = -1;
    std::string output = invoke_capture({"cppapplab", "app-build", workspace.root.string()}, result);
    ASSERT_EQ(result, 0) << output;

    const auto executable =
        workspace.root / ".cppapplab" / "application" / "learner_app";
    const std::uint16_t port = reserve_test_loopback_port();
    cppapplab::ProcessResult server_result;
    std::thread server([&] {
        server_result = cppapplab::run_process(
            {executable.string(), "server", std::to_string(port)},
            workspace.root, std::chrono::seconds(5), 4096U);
    });

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    bool echoed = false;
    while (std::chrono::steady_clock::now() < deadline) {
        output = invoke_capture(
            {"cppapplab", "app-run", workspace.root.string(), "client",
             std::to_string(port), "real browser workbench round trip"},
            result);
        if (result == 0) {
            echoed = true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    server.join();

    EXPECT_TRUE(echoed) << output;
    EXPECT_NE(output.find("real browser workbench round trip"), std::string::npos);
    EXPECT_NE(output.find("Application result: PASS"), std::string::npos);
    EXPECT_EQ(server_result.exit_code, 0) << server_result.output;
    EXPECT_FALSE(server_result.timed_out);
    EXPECT_NE(server_result.output.find("listening on 127.0.0.1:"), std::string::npos);
    EXPECT_FALSE(std::filesystem::exists(workspace.root / ".cppapplab" / "evidence.txt"));
}

TEST(ApplicationRuntimeWorkflowTest, TcpChatProfileBuildsTheDeclaredLearnerServer) {
    const cppapplab::Task* task = cppapplab::find_task("w7.tcp-chat");
    ASSERT_NE(task, nullptr);
    TemporaryDirectory temporary;
    ScopedEnvironment state("CPPAPPLAB_STATE_DIR", (temporary.path() / "state").string());
    const auto workspace = cppapplab::create_workspace(*task, temporary.path() / "tcp-chat");

    int result = -1;
    const std::string output = invoke_capture(
        {"cppapplab", "app-build", workspace.root.string()}, result);
    EXPECT_EQ(result, 0) << output;
    EXPECT_NE(output.find("src/tcp_chat.cpp"), std::string::npos);
    EXPECT_FALSE(std::filesystem::exists(workspace.root / ".cppapplab" / "evidence.txt"));
}

TEST(ApplicationRuntimeWorkflowTest, HttpServerProfileUsesPlatformOwnedRouterAndThreadPoolSupport) {
    const cppapplab::Task* task = cppapplab::find_task("w8.http-server");
    ASSERT_NE(task, nullptr);
    TemporaryDirectory temporary;
    ScopedEnvironment state("CPPAPPLAB_STATE_DIR", (temporary.path() / "state").string());
    const auto workspace = cppapplab::create_workspace(*task, temporary.path() / "http-server");

    int result = -1;
    const std::string output = invoke_capture(
        {"cppapplab", "app-build", workspace.root.string()}, result);
    EXPECT_EQ(result, 0) << output;
    EXPECT_NE(output.find("[platform] labs/w8/router/reference/src/router.cpp"), std::string::npos);
    EXPECT_NE(output.find("[platform] labs/w6/thread-pool/reference/src/thread_pool.cpp"), std::string::npos);
    EXPECT_FALSE(std::filesystem::exists(workspace.root / ".cppapplab" / "evidence.txt"));
}
TEST(ApplicationRuntimeWorkflowTest, HttpProtocolProfileBuildsAndRunsRegisteredRequestLineAction) {
    const cppapplab::Task* task = cppapplab::find_task("w8.http-protocol");
    ASSERT_NE(task, nullptr);
    TemporaryDirectory temporary;
    ScopedEnvironment state("CPPAPPLAB_STATE_DIR", (temporary.path() / "state").string());

    const auto workspace = cppapplab::create_workspace(*task, temporary.path() / "http-protocol");
    int result = -1;
    std::string output = invoke_capture({"cppapplab", "app-build", workspace.root.string()}, result);
    ASSERT_EQ(result, 0) << output;
    output = invoke_capture(
        {"cppapplab", "app-run", workspace.root.string(), "parse-line", "GET / HTTP/1.1"},
        result);
    EXPECT_EQ(result, 1) << output;
    EXPECT_NE(output.find("400 Bad Request: invalid request line"), std::string::npos);
    EXPECT_FALSE(std::filesystem::exists(workspace.root / ".cppapplab" / "evidence.txt"));
}

TEST(ApplicationRuntimeWorkflowTest, StaticFilesProfileUsesWorkspaceLocalDocumentRoot) {
    const cppapplab::Task* task = cppapplab::find_task("w8.static-files");
    ASSERT_NE(task, nullptr);
    TemporaryDirectory temporary;
    ScopedEnvironment state("CPPAPPLAB_STATE_DIR", (temporary.path() / "state").string());

    const auto workspace = cppapplab::create_workspace(*task, temporary.path() / "static-files");
    int result = -1;
    std::string output = invoke_capture({"cppapplab", "app-build", workspace.root.string()}, result);
    ASSERT_EQ(result, 0) << output;
    output = invoke_capture(
        {"cppapplab", "app-run", workspace.root.string(), "handle", "GET", "/index.html"},
        result);
    EXPECT_NE(result, 0) << output;
    EXPECT_NE(output.find("TODO: implement StaticFileHandler::handle"), std::string::npos);
    EXPECT_TRUE(std::filesystem::is_regular_file(
        workspace.root / ".cppapplab" / "application" / "runtime" / "www" / "index.html"));
    EXPECT_FALSE(std::filesystem::exists(workspace.root / ".cppapplab" / "evidence.txt"));
}

TEST(ApplicationRuntimeWorkflowTest, HttpParserProfilePassesRequestThroughStandardInput) {
    const cppapplab::Task* task = cppapplab::find_task("w8.http-parser");
    ASSERT_NE(task, nullptr);
    TemporaryDirectory temporary;
    ScopedEnvironment state("CPPAPPLAB_STATE_DIR", (temporary.path() / "state").string());

    const auto workspace = cppapplab::create_workspace(*task, temporary.path() / "http-parser");
    int result = -1;
    std::string output = invoke_capture({"cppapplab", "app-build", workspace.root.string()}, result);
    ASSERT_EQ(result, 0) << output;
    const std::string request = "GET / HTTP/1.1\r\nHost: localhost\r\n\r\n";
    {
        ScopedInputCapture input(request);
        output = invoke_capture(
            {"cppapplab", "app-run-stdin", workspace.root.string(), "parse"}, result);
    }
    EXPECT_NE(result, 0) << output;
    EXPECT_NE(output.find("TODO: implement incremental HTTP request parsing"), std::string::npos);
}

TEST(ApplicationRuntimeWorkflowTest, RouterProfilePassesRequestThroughStandardInput) {
    const cppapplab::Task* task = cppapplab::find_task("w8.router");
    ASSERT_NE(task, nullptr);
    TemporaryDirectory temporary;
    ScopedEnvironment state("CPPAPPLAB_STATE_DIR", (temporary.path() / "state").string());

    const auto workspace = cppapplab::create_workspace(*task, temporary.path() / "router");
    int result = -1;
    std::string output = invoke_capture({"cppapplab", "app-build", workspace.root.string()}, result);
    ASSERT_EQ(result, 0) << output;
    const std::string request = "GET / HTTP/1.1\r\nHost: localhost\r\n\r\n";
    {
        ScopedInputCapture input(request);
        output = invoke_capture(
            {"cppapplab", "app-run-stdin", workspace.root.string(), "dispatch"}, result);
    }
    EXPECT_NE(result, 0) << output;
    EXPECT_NE(output.find("TODO: register the three dynamic endpoints"), std::string::npos);
    EXPECT_TRUE(std::filesystem::is_regular_file(
        workspace.root / ".cppapplab" / "application" / "runtime" / "www" / "index.html"));
}

TEST(ApplicationRuntimeWorkflowTest, StdinEntrypointRejectsUnregisteredActions) {
    const cppapplab::Task* task = cppapplab::find_task("w1.calculator");
    ASSERT_NE(task, nullptr);
    TemporaryDirectory temporary;
    ScopedEnvironment state("CPPAPPLAB_STATE_DIR", (temporary.path() / "state").string());
    const auto workspace = cppapplab::create_workspace(*task, temporary.path() / "calculator");

    int result = -1;
    const std::string output = invoke_capture(
        {"cppapplab", "app-run-stdin", workspace.root.string(), "parse"}, result);
    EXPECT_EQ(result, 2);
    EXPECT_NE(output.find("Only registered HTTP stdin actions"), std::string::npos);
}

TEST(ApplicationRuntimeWorkflowTest, StdinEntrypointRejectsOversizedHttpRequests) {
    const cppapplab::Task* task = cppapplab::find_task("w8.http-parser");
    ASSERT_NE(task, nullptr);
    TemporaryDirectory temporary;
    ScopedEnvironment state("CPPAPPLAB_STATE_DIR", (temporary.path() / "state").string());
    const auto workspace = cppapplab::create_workspace(*task, temporary.path() / "parser");

    std::string input_data(256U * 1024U + 1U, 'x');
    ScopedInputCapture input(input_data);
    int result = -1;
    const std::string output = invoke_capture(
        {"cppapplab", "app-run-stdin", workspace.root.string(), "parse"}, result);
    EXPECT_EQ(result, 2);
    EXPECT_NE(output.find("HTTP input exceeds the 256 KiB limit"), std::string::npos);
}

TEST(ApplicationRuntimeWorkflowTest, LibraryOnlyTaskHasNoApplicationExecutionProfile) {
    const cppapplab::Task* task = cppapplab::find_task("w8.http-connection-policy");
    ASSERT_NE(task, nullptr);
    TemporaryDirectory temporary;
    ScopedEnvironment state("CPPAPPLAB_STATE_DIR", temporary.path() / "state");

    const auto workspace = cppapplab::create_workspace(*task, temporary.path() / "connection-policy");
    int result = -1;
    const std::string output = invoke_capture(
        {"cppapplab", "app-build", workspace.root.string()}, result);
    EXPECT_EQ(result, 2);
    EXPECT_NE(output.find("no registered browser application profile"), std::string::npos);
}
