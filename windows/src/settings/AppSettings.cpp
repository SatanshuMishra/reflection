#include "settings/AppSettings.h"
#include "utilities/Constants.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

namespace reflection {

AppSettings::AppSettings() = default;

std::wstring AppSettings::server_name() const {
    return read_string(constants::kRegKeyServerName.data(), L"Reflection");
}

bool AppSettings::start_on_login() const {
    return read_bool(constants::kRegKeyStartOnLogin.data(), false);
}

bool AppSettings::minimize_to_tray() const {
    return read_bool(constants::kRegKeyMinimizeToTray.data(), true);
}

void AppSettings::set_server_name(const std::wstring& name) {
    write_string(constants::kRegKeyServerName.data(), name);
}

void AppSettings::set_start_on_login(bool enabled) {
    write_bool(constants::kRegKeyStartOnLogin.data(), enabled);
}

void AppSettings::set_minimize_to_tray(bool enabled) {
    write_bool(constants::kRegKeyMinimizeToTray.data(), enabled);
}

std::string AppSettings::theme() const {
    std::wstring wtheme = read_string(constants::kRegKeyTheme.data(), L"system");
    // Convert wstring to string (ASCII-safe for theme values)
    std::string result;
    result.reserve(wtheme.size());
    for (wchar_t c : wtheme) {
        result += static_cast<char>(c);
    }
    return result;
}

void AppSettings::set_theme(const std::string& theme) {
    std::wstring wtheme(theme.begin(), theme.end());
    write_string(constants::kRegKeyTheme.data(), wtheme);
}

// -- Registry helpers --

std::wstring AppSettings::read_string(
    const wchar_t* name, const std::wstring& default_value
) const {
    HKEY key;
    if (RegOpenKeyEx(HKEY_CURRENT_USER, constants::kRegistryRoot.data(),
                     0, KEY_READ, &key) != ERROR_SUCCESS) {
        return default_value;
    }

    // First call: get required buffer size
    DWORD size = 0;
    DWORD type = 0;
    if (RegQueryValueEx(key, name, nullptr, &type, nullptr, &size)
            != ERROR_SUCCESS || type != REG_SZ || size == 0) {
        RegCloseKey(key);
        return default_value;
    }

    // Allocate buffer and read the value
    // size includes the null terminator in bytes
    const DWORD char_count = size / sizeof(wchar_t);
    std::wstring result(char_count, L'\0');

    if (RegQueryValueEx(key, name, nullptr, nullptr,
                        reinterpret_cast<LPBYTE>(result.data()), &size)
            != ERROR_SUCCESS) {
        RegCloseKey(key);
        return default_value;
    }

    RegCloseKey(key);

    // Remove trailing null terminator(s) if present
    while (!result.empty() && result.back() == L'\0') {
        result.pop_back();
    }

    return result;
}

bool AppSettings::read_bool(const wchar_t* name, bool default_value) const {
    HKEY key;
    if (RegOpenKeyEx(HKEY_CURRENT_USER, constants::kRegistryRoot.data(),
                     0, KEY_READ, &key) != ERROR_SUCCESS) {
        return default_value;
    }

    DWORD value = 0;
    DWORD size = sizeof(value);
    const bool found = (RegQueryValueEx(
        key, name, nullptr, nullptr,
        reinterpret_cast<LPBYTE>(&value), &size
    ) == ERROR_SUCCESS);

    RegCloseKey(key);
    return found ? (value != 0) : default_value;
}

void AppSettings::write_string(const wchar_t* name, const std::wstring& value) {
    HKEY key;
    if (RegCreateKeyEx(HKEY_CURRENT_USER, constants::kRegistryRoot.data(),
                       0, nullptr, 0, KEY_WRITE, nullptr, &key,
                       nullptr) == ERROR_SUCCESS) {
        RegSetValueEx(key, name, 0, REG_SZ,
                      reinterpret_cast<const BYTE*>(value.c_str()),
                      static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t)));
        RegCloseKey(key);
    }
}

void AppSettings::write_bool(const wchar_t* name, bool value) {
    HKEY key;
    if (RegCreateKeyEx(HKEY_CURRENT_USER, constants::kRegistryRoot.data(),
                       0, nullptr, 0, KEY_WRITE, nullptr, &key,
                       nullptr) == ERROR_SUCCESS) {
        DWORD dword_val = value ? 1 : 0;
        RegSetValueEx(key, name, 0, REG_DWORD,
                      reinterpret_cast<const BYTE*>(&dword_val),
                      sizeof(dword_val));
        RegCloseKey(key);
    }
}

} // namespace reflection
