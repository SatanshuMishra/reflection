#pragma once

#include <format>
#include <string>
#include <string_view>

namespace reflection {

/// Simple logging utility.
/// Outputs to OutputDebugString (visible in debugger) and optional log file.
/// Thread-safe — each call formats and writes atomically.
class Logger {
public:
    /// Initialize the logger. Call once at startup.
    static void init();

    /// Log at INFO level.
    template <typename... Args>
    static void info(std::format_string<Args...> fmt, Args&&... args) {
        log("INFO", std::format(fmt, std::forward<Args>(args)...));
    }

    /// Log at WARNING level.
    template <typename... Args>
    static void warn(std::format_string<Args...> fmt, Args&&... args) {
        log("WARN", std::format(fmt, std::forward<Args>(args)...));
    }

    /// Log at ERROR level.
    template <typename... Args>
    static void error(std::format_string<Args...> fmt, Args&&... args) {
        log("ERROR", std::format(fmt, std::forward<Args>(args)...));
    }

    /// Log at DEBUG level.
    template <typename... Args>
    static void debug(std::format_string<Args...> fmt, Args&&... args) {
        log("DEBUG", std::format(fmt, std::forward<Args>(args)...));
    }

private:
    static void log(std::string_view level, const std::string& message);
};

} // namespace reflection
