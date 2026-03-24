// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Satanshu Mishra

#pragma once

#include <string>

namespace reflection {

/// Application settings backed by Windows Registry.
/// Mirrors the macOS AppSettings (UserDefaults-backed).
///
/// Registry location: HKCU\Software\Reflection
class AppSettings {
public:
    AppSettings();

    // Getters
    [[nodiscard]] std::wstring server_name() const;
    [[nodiscard]] bool start_on_login() const;
    [[nodiscard]] bool minimize_to_tray() const;
    [[nodiscard]] std::string theme() const;
    [[nodiscard]] bool firewall_configured() const;

    // Setters (persist to registry immediately)
    void set_server_name(const std::wstring& name);
    void set_start_on_login(bool enabled);
    void set_minimize_to_tray(bool enabled);
    void set_theme(const std::string& theme);
    void set_firewall_configured(bool configured);

private:
    // Registry helpers
    std::wstring read_string(const wchar_t* name, const std::wstring& default_value) const;
    bool read_bool(const wchar_t* name, bool default_value) const;
    void write_string(const wchar_t* name, const std::wstring& value);
    void write_bool(const wchar_t* name, bool value);
};

} // namespace reflection
