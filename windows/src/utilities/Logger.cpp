#include "utilities/Logger.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#include <chrono>
#include <format>
#include <mutex>

namespace reflection {

namespace {
    std::mutex g_log_mutex;
    bool g_initialized = false;
} // namespace

void Logger::init() {
    std::lock_guard lock(g_log_mutex);
    g_initialized = true;
}

void Logger::log(std::string_view level, const std::string& message) {
    const auto now = std::chrono::system_clock::now();
    const auto time_t_now = std::chrono::system_clock::to_time_t(now);
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;

    std::tm local_time{};
    localtime_s(&local_time, &time_t_now);

    const auto formatted = std::format(
        "[{:04d}-{:02d}-{:02d} {:02d}:{:02d}:{:02d}.{:03d}] [{}] {}\n",
        local_time.tm_year + 1900, local_time.tm_mon + 1, local_time.tm_mday,
        local_time.tm_hour, local_time.tm_min, local_time.tm_sec,
        static_cast<int>(ms.count()),
        level, message
    );

    {
        std::lock_guard lock(g_log_mutex);

        // Output to debugger
        OutputDebugStringA(formatted.c_str());

        // Also write to stderr for console builds / CI
        fprintf(stderr, "%s", formatted.c_str());
    }
}

} // namespace reflection
