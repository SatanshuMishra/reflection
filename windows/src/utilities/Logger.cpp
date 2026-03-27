// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Satanshu Mishra

#include "utilities/Logger.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#include <chrono>
#include <cstdio>
#include <format>
#include <mutex>
#include <share.h>

namespace reflection {

namespace {
    std::mutex g_log_mutex;
    FILE* g_log_file = nullptr;
    bool g_console_attached = false;
} // namespace

void Logger::init() {
    std::lock_guard lock(g_log_mutex);

    // Allocate a console window for live log output during development.
    // Only enabled in Debug builds -- Release builds log to file + OutputDebugString only.
#ifdef _DEBUG
    if (AllocConsole()) {
        FILE* dummy = nullptr;
        freopen_s(&dummy, "CONOUT$", "w", stdout);
        freopen_s(&dummy, "CONOUT$", "w", stderr);
        g_console_attached = true;

        SetConsoleTitleW(L"Reflection -- Log Output");
    }
#endif

    // Also open a log file next to the executable for post-mortem analysis
    wchar_t exe_path[MAX_PATH]{};
    DWORD path_len = GetModuleFileNameW(nullptr, exe_path, MAX_PATH);
    if (path_len > 0 && path_len < MAX_PATH) {
        // Replace .exe with .log
        std::wstring log_path(exe_path);
        const auto dot_pos = log_path.rfind(L'.');
        if (dot_pos != std::wstring::npos) {
            log_path = log_path.substr(0, dot_pos);
        }
        log_path += L".log";

        // Open with _SH_DENYNO so other processes can read the log file
        // while the app is running (e.g., for debugging)
        g_log_file = _wfsopen(log_path.c_str(), L"w", _SH_DENYNO);
    }
}

void Logger::shutdown() {
    std::lock_guard lock(g_log_mutex);

    if (g_log_file) {
        fclose(g_log_file);
        g_log_file = nullptr;
    }

    if (g_console_attached) {
        FreeConsole();
        g_console_attached = false;
    }
}

void Logger::log(std::string_view level, const std::string& message) {
    const auto now = std::chrono::system_clock::now();
    const auto time_t_now = std::chrono::system_clock::to_time_t(now);
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;

    std::tm utc_time{};
    gmtime_s(&utc_time, &time_t_now);

    const auto formatted = std::format(
        "[{:04d}-{:02d}-{:02d}T{:02d}:{:02d}:{:02d}.{:03d}Z] [{}] {}\n",
        utc_time.tm_year + 1900, utc_time.tm_mon + 1, utc_time.tm_mday,
        utc_time.tm_hour, utc_time.tm_min, utc_time.tm_sec,
        static_cast<int>(ms.count()),
        level, message
    );

    {
        std::lock_guard lock(g_log_mutex);

        // Output to debugger (Visual Studio Output window)
        OutputDebugStringA(formatted.c_str());

        // Output to console (allocated on startup)
        if (g_console_attached) {
            fprintf(stderr, "%s", formatted.c_str());
            fflush(stderr);
        }

        // Output to log file
        if (g_log_file) {
            fprintf(g_log_file, "%s", formatted.c_str());
            fflush(g_log_file);
        }
    }
}

} // namespace reflection
