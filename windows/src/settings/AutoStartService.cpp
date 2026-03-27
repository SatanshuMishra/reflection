// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Satanshu Mishra

#include "settings/AutoStartService.h"
#include "utilities/Constants.h"
#include "utilities/Logger.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

namespace reflection {

bool AutoStartService::is_enabled() {
    HKEY key;
    if (RegOpenKeyEx(HKEY_CURRENT_USER, constants::kRunRegistryPath.data(),
                     0, KEY_READ, &key) != ERROR_SUCCESS) {
        return false;
    }

    const bool exists = (RegQueryValueEx(
        key, constants::kRunRegistryValueName.data(),
        nullptr, nullptr, nullptr, nullptr
    ) == ERROR_SUCCESS);

    RegCloseKey(key);
    return exists;
}

bool AutoStartService::enable() {
    // Get our own executable path
    wchar_t exe_path[MAX_PATH]{};
    DWORD len = GetModuleFileNameW(nullptr, exe_path, MAX_PATH);
    if (len == 0 || len == MAX_PATH) {
        Logger::error("GetModuleFileName failed or truncated: {}", GetLastError());
        return false;
    }

    HKEY key;
    if (RegOpenKeyEx(HKEY_CURRENT_USER, constants::kRunRegistryPath.data(),
                     0, KEY_WRITE, &key) != ERROR_SUCCESS) {
        Logger::error("Failed to open Run registry key");
        return false;
    }

    const std::wstring path(exe_path);
    const bool success = (RegSetValueEx(
        key, constants::kRunRegistryValueName.data(),
        0, REG_SZ,
        reinterpret_cast<const BYTE*>(path.c_str()),
        static_cast<DWORD>((path.size() + 1) * sizeof(wchar_t))
    ) == ERROR_SUCCESS);

    RegCloseKey(key);

    if (success) {
        Logger::info("Auto-start enabled");
    } else {
        Logger::error("Failed to enable auto-start");
    }

    return success;
}

bool AutoStartService::disable() {
    HKEY key;
    if (RegOpenKeyEx(HKEY_CURRENT_USER, constants::kRunRegistryPath.data(),
                     0, KEY_WRITE, &key) != ERROR_SUCCESS) {
        return false;
    }

    const bool success = (RegDeleteValue(
        key, constants::kRunRegistryValueName.data()
    ) == ERROR_SUCCESS);

    RegCloseKey(key);

    if (success) {
        Logger::info("Auto-start disabled");
    }

    return success;
}

} // namespace reflection
