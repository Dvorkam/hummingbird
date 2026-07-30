#include "core/utils/Log.h"

#include <chrono>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>

namespace Hummingbird::Core::Log {

namespace {

// One lock guards both sinks. Emission is line-at-a-time, so a log line from
// the network pool can never land inside one from the main thread.
std::mutex& sink_mutex() {
    static std::mutex mutex;
    return mutex;
}

std::ofstream& file_sink() {
    static std::ofstream file;
    return file;
}

std::string env_or_empty(const char* name) {
#ifdef _MSC_VER
    // getenv is deprecated under MSVC's secure-CRT warnings; _dupenv_s is the
    // sanctioned form and hands back an allocation we own.
    char* value = nullptr;
    size_t size = 0;
    if (_dupenv_s(&value, &size, name) != 0 || !value) {
        return {};
    }
    std::string result(value);
    free(value);
    return result;
#else
    const char* value = std::getenv(name);
    return value ? std::string(value) : std::string{};
#endif
}

// log-YYMMDD-HHMMSS.log. Seconds are included deliberately: two runs in the
// same minute are entirely normal while chasing a bug, and the second one
// silently truncating the first is exactly the data loss this file exists to
// prevent.
std::string timestamped_name() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t as_time = std::chrono::system_clock::to_time_t(now);
    std::tm parts{};
#ifdef _WIN32
    localtime_s(&parts, &as_time);
#else
    localtime_r(&as_time, &parts);
#endif
    char buffer[64];
    std::strftime(buffer, sizeof(buffer), "log-%y%m%d-%H%M%S.log", &parts);
    return std::string(buffer);
}

}  // namespace

void emit(const char* level, const std::string& message) {
    std::lock_guard<std::mutex> lock(sink_mutex());
    std::cerr << level << message << std::endl;
    auto& file = file_sink();
    if (file.is_open()) {
        // Flushed per line rather than buffered: a log whose tail is missing is
        // least useful in the one case it is most needed, which is a crash.
        file << level << message << std::endl;
    }
}

std::string start_file_logging() {
    std::lock_guard<std::mutex> lock(sink_mutex());
    auto& file = file_sink();
    if (file.is_open()) {
        file.close();
    }

    std::filesystem::path path;
    if (const std::string explicit_path = env_or_empty("HB_LOG_FILE"); !explicit_path.empty()) {
        path = explicit_path;
    } else {
        const std::string dir = env_or_empty("HB_LOG_DIR");
        path = std::filesystem::path(dir.empty() ? "logs" : dir) / timestamped_name();
    }

    std::error_code ec;
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path(), ec);
        // A failure here is not fatal — the open below will fail too, and the
        // caller falls back to console-only.
    }

    file.open(path, std::ios::out | std::ios::trunc);
    if (!file.is_open()) {
        std::cerr << "[warn] [log] could not open log file " << path.string() << "; console only" << std::endl;
        return {};
    }
    return std::filesystem::absolute(path, ec).string();
}

void stop_file_logging() {
    std::lock_guard<std::mutex> lock(sink_mutex());
    auto& file = file_sink();
    if (file.is_open()) {
        file.close();
    }
}

}  // namespace Hummingbird::Core::Log
