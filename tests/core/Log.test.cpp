#include "core/utils/Log.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include <cstdlib>

TEST(LogTest, EmitsWhenEnabled) {
    std::stringstream buffer;
    auto* old = std::cerr.rdbuf(buffer.rdbuf());

    HB_LOG_INFO("hello");
    HB_LOG_WARN("world");

    std::cerr.rdbuf(old);
    auto output = buffer.str();

#if HB_LOG_LEVEL >= 3
    EXPECT_NE(output.find("[info] hello"), std::string::npos);
    EXPECT_NE(output.find("[warn] world"), std::string::npos);
#elif HB_LOG_LEVEL >= 2
    EXPECT_EQ(output.find("[info] hello"), std::string::npos);
    EXPECT_NE(output.find("[warn] world"), std::string::npos);
#elif HB_LOG_LEVEL >= 1
    EXPECT_EQ(output.find("[info] hello"), std::string::npos);
    EXPECT_EQ(output.find("[warn] world"), std::string::npos);
#else
    EXPECT_TRUE(output.empty());
#endif
}

// --- file sink (2026-07-30) -------------------------------------------------
// A console has bounded scrollback, so a long browsing session — the kind worth
// logging — loses its own beginning. These cover the file tee.

namespace {
void set_env(const char* name, const char* value) {
#ifdef _WIN32
    _putenv_s(name, value ? value : "");
#else
    if (value) {
        setenv(name, value, 1);
    } else {
        unsetenv(name);
    }
#endif
}

std::string read_all(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    std::ostringstream ss;
    ss << file.rdbuf();
    return ss.str();
}
}  // namespace

TEST(LogTest, FileSinkCapturesLinesAndLeavesTheConsoleIntact) {
    const auto path = std::filesystem::temp_directory_path() / "hb_log_sink_test.log";
    std::filesystem::remove(path);
    set_env("HB_LOG_FILE", path.string().c_str());

    // The console must keep receiving everything: this is a tee, not a
    // redirect. Losing console output would trade one problem for another.
    std::stringstream console;
    auto* old = std::cerr.rdbuf(console.rdbuf());

    const std::string started = Hummingbird::Core::Log::start_file_logging();
    HB_LOG_ERROR("to both sinks");
    Hummingbird::Core::Log::stop_file_logging();

    std::cerr.rdbuf(old);
    set_env("HB_LOG_FILE", nullptr);

    EXPECT_FALSE(started.empty()) << "the log file should have opened";
    EXPECT_NE(console.str().find("[error] to both sinks"), std::string::npos) << "console output must survive";
    EXPECT_NE(read_all(path).find("[error] to both sinks"), std::string::npos) << "the file must hold the same line";

    // After stop_file_logging the file must stop growing, or a test binary would
    // keep writing into whatever file ran last.
    const auto size_after_stop = read_all(path).size();
    HB_LOG_ERROR("after close");
    EXPECT_EQ(read_all(path).size(), size_after_stop);

    std::filesystem::remove(path);
}

// A log line is built and emitted as one unit. The network thread pool logs from
// its own threads, and the old macro streamed straight at std::cerr with a chain
// of `<<`, which interleaves mid-line when two of them race.
TEST(LogTest, AMultiPartLineIsEmittedWhole) {
    const auto path = std::filesystem::temp_directory_path() / "hb_log_atomic_test.log";
    std::filesystem::remove(path);
    set_env("HB_LOG_FILE", path.string().c_str());

    std::stringstream console;
    auto* old = std::cerr.rdbuf(console.rdbuf());
    Hummingbird::Core::Log::start_file_logging();

    constexpr int kThreads = 8;
    constexpr int kLines = 40;
    std::vector<std::thread> threads;
    for (int t = 0; t < kThreads; ++t) {
        threads.emplace_back([t] {
            for (int i = 0; i < kLines; ++i) {
                HB_LOG_ERROR("thread=" << t << " part=" << i << " end");
            }
        });
    }
    for (auto& thread : threads) thread.join();

    Hummingbird::Core::Log::stop_file_logging();
    std::cerr.rdbuf(old);
    set_env("HB_LOG_FILE", nullptr);

    // Every line must be intact: prefix, all three fields, nothing spliced in.
    std::istringstream lines(read_all(path));
    std::string line;
    int counted = 0;
    while (std::getline(lines, line)) {
        // The file is opened in text mode, so std::endl became CRLF on Windows
        // and getline leaves the CR behind.
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        ++counted;
        EXPECT_EQ(line.rfind("[error] thread=", 0), 0u) << "spliced line: " << line;
        EXPECT_TRUE(line.size() > 4 && line.compare(line.size() - 4, 4, " end") == 0)
            << "truncated or merged line: " << line;
    }
    EXPECT_EQ(counted, kThreads * kLines);

    std::filesystem::remove(path);
}
