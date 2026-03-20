#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#include <string>
#include <system_error>

namespace reflection::win_utils {

/// Convert HRESULT to a descriptive error string.
inline std::string hresult_to_string(HRESULT hr) {
    return std::system_category().message(hr);
}

/// Check HRESULT and throw std::runtime_error on failure.
inline void throw_if_failed(HRESULT hr, const char* context) {
    if (FAILED(hr)) {
        throw std::runtime_error(
            std::string(context) + ": " + hresult_to_string(hr)
        );
    }
}

/// Convert UTF-8 string to wide string (UTF-16).
inline std::wstring utf8_to_wide(const std::string& utf8) {
    if (utf8.empty()) return {};
    const int size = MultiByteToWideChar(
        CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0
    );
    std::wstring result(size, L'\0');
    MultiByteToWideChar(
        CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()),
        result.data(), size
    );
    return result;
}

/// Convert wide string (UTF-16) to UTF-8 string.
inline std::string wide_to_utf8(const std::wstring& wide) {
    if (wide.empty()) return {};
    const int size = WideCharToMultiByte(
        CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()),
        nullptr, 0, nullptr, nullptr
    );
    std::string result(size, '\0');
    WideCharToMultiByte(
        CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()),
        result.data(), size, nullptr, nullptr
    );
    return result;
}

} // namespace reflection::win_utils
