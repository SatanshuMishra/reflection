// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Satanshu Mishra

#pragma once

#include <cstdint>
#include <string_view>

namespace reflection::constants {

// --------------------------------------------------------------------------
// Window IDs & Names
// --------------------------------------------------------------------------
constexpr std::wstring_view kAppName = L"Reflection";
constexpr std::wstring_view kAppWindowClass = L"ReflectionMainClass";
constexpr std::wstring_view kMirrorWindowClass = L"ReflectionMirrorClass";
constexpr std::wstring_view kOnboardingWindowClass = L"ReflectionOnboardingClass";
constexpr std::wstring_view kStatusWindowClass = L"ReflectionStatusClass";

// --------------------------------------------------------------------------
// AirPlay Configuration
// --------------------------------------------------------------------------
constexpr std::string_view kDefaultServerName = "Reflection";
constexpr uint16_t kAirPlayPort = 7000;
constexpr uint16_t kRaopPort = 5000;

// mDNS service types (RFC 6762)
constexpr std::string_view kAirPlayMdnsType = "_airplay._tcp.local.";
constexpr std::string_view kRaopMdnsType = "_raop._tcp.local.";
constexpr std::string_view kMdnsMulticastAddress = "224.0.0.251";
constexpr uint16_t kMdnsPort = 5353;
constexpr uint32_t kMdnsDefaultTtl = 4500;

// AirPlay TXT record keys (required for iOS discovery)
// Based on RPiPlay/UxPlay known-good configurations
constexpr std::string_view kTxtKeyFeatures = "features";
constexpr std::string_view kTxtKeyModel = "model";
constexpr std::string_view kTxtKeyFlags = "flags";
constexpr std::string_view kTxtKeyPk = "pk";
constexpr std::string_view kTxtKeySrcvers = "srcvers";
constexpr std::string_view kTxtKeyVv = "vv";
constexpr std::string_view kTxtKeyPi = "pi";
constexpr std::string_view kTxtKeyDeviceId = "deviceid";

// AirPlay feature flags — RPiPlay's proven value
// 0x5A7FFEE6 enables screen mirroring, audio, photo, slideshow
constexpr std::string_view kTxtValueFeatures = "0x5A7FFEE6";
constexpr std::string_view kTxtValueModel = "AppleTV3,2";
constexpr std::string_view kTxtValueFlags = "0x4";
constexpr std::string_view kTxtValueSrcvers = "220.68";
constexpr std::string_view kTxtValueVv = "2";
// Ed25519 public key (matches RPiPlay's hardcoded key)
constexpr std::string_view kTxtValuePk =
    "b07727d6f6cd6e08b58ede525ec3cdeaa252ad9f683feb212ef8a205246554e7";
// Protocol info UUID (matches RPiPlay)
constexpr std::string_view kTxtValuePi = "2e388006-13ba-4041-9a67-25dd4a43d536";

// --------------------------------------------------------------------------
// Window Dimensions
// --------------------------------------------------------------------------
constexpr int kDefaultMirrorWidth = 1024;
constexpr int kDefaultMirrorHeight = 768;
constexpr int kMinMirrorWidth = 320;
constexpr int kMinMirrorHeight = 240;
constexpr double kDefaultAspectRatio = 4.0 / 3.0;

constexpr int kOnboardingWidth = 900;
constexpr int kOnboardingHeight = 800;

constexpr int kStatusWindowWidth = 480;
constexpr int kStatusWindowHeight = 520;

// --------------------------------------------------------------------------
// Colors (matching macOS app: near-black #121215)
// --------------------------------------------------------------------------
constexpr uint8_t kBgColorR = 0x12;
constexpr uint8_t kBgColorG = 0x12;
constexpr uint8_t kBgColorB = 0x15;

// As COLORREF (BGR format for Win32)
constexpr uint32_t kBackgroundColor = (kBgColorB << 16) | (kBgColorG << 8) | kBgColorR;

// As D3D float array {R, G, B, A} normalized to 0-1
constexpr float kBgClearColor[4] = {
    kBgColorR / 255.0f,
    kBgColorG / 255.0f,
    kBgColorB / 255.0f,
    1.0f
};

// --------------------------------------------------------------------------
// Mirror Window Overlay (hover toolbar)
// --------------------------------------------------------------------------
constexpr int kOverlayHeight = 48;
constexpr int kOverlayButtonWidth = 46;
constexpr int kOverlayPaddingX = 12;
constexpr int kOverlayTextSize = 13;     // Segoe UI, points

// Overlay background: slightly lighter than app background (#1E1E22)
constexpr uint8_t kOverlayBgR = 0x1E;
constexpr uint8_t kOverlayBgG = 0x1E;
constexpr uint8_t kOverlayBgB = 0x22;
constexpr uint8_t kOverlayBgAlpha = 0xD9;  // ~85% opacity

// As COLORREF (BGR format for Win32 GDI)
constexpr uint32_t kOverlayBgColor =
    (kOverlayBgB << 16) | (kOverlayBgG << 8) | kOverlayBgR;

// Button hover highlight (#2A2A30)
constexpr uint32_t kOverlayHoverColor =
    (0x30u << 16) | (0x2Au << 8) | 0x2Au;

// Close button hover (Windows standard red #C42B1C)
constexpr uint32_t kOverlayCloseHoverColor =
    (0x1Cu << 16) | (0x2Bu << 8) | 0xC4u;

// Overlay auto-hide delay after mouse leaves (milliseconds)
constexpr uint32_t kOverlayHideDelayMs = 1000;

// Timer IDs (kRenderTimerId = 1 already used)
constexpr uintptr_t kOverlayHideTimerId = 2;
constexpr uintptr_t kOverlayFadeTimerId = 3;

// Fade animation timing
constexpr uint32_t kOverlayFadeStepMs = 16;       // ~60fps timer interval
constexpr uint32_t kOverlayFadeDurationMs = 200;   // Total fade duration

// Session duration stopwatch (1-second tick in overlay)
constexpr uintptr_t kStopwatchTimerId = 4;
constexpr uint32_t kStopwatchIntervalMs = 1000;

// Custom message: overlay child reports mouse activity to parent.
// Uses WM_APP range (0x8000+) for safe inter-window communication.
// WM_USER (0x0400) is reserved for intra-class use only.
constexpr uint32_t kWmOverlayMouseActivity = 0x8000 + 10;  // WM_APP + 10

// --------------------------------------------------------------------------
// Timing (matching macOS FrameStaleMonitor)
// --------------------------------------------------------------------------
constexpr double kFrameStaleThresholdSec = 1.0;
constexpr uint64_t kFrameCheckIntervalMs = 500;

// --------------------------------------------------------------------------
// Animation
// --------------------------------------------------------------------------
constexpr double kAnimationDurationSec = 0.3;

// --------------------------------------------------------------------------
// Instance Name — separates Debug (dev) and Release (installed) data.
//
// Debug builds use "Reflection-Dev" so dev testing never pollutes the
// installed app's registry, WebView2 cache, firewall rules, or auto-start.
// Follows the Chrome (Chrome/Chrome Canary) and VS Code (Code/Code Insiders)
// pattern for build-variant isolation.
// --------------------------------------------------------------------------
#ifdef _DEBUG
constexpr std::wstring_view kAppInstanceName = L"Reflection-Dev";
constexpr std::wstring_view kRegistryRoot = L"Software\\Reflection-Dev";
constexpr std::wstring_view kRunRegistryValueName = L"Reflection-Dev";
constexpr std::wstring_view kWebViewSubdir = L"Reflection-Dev";
#else
constexpr std::wstring_view kAppInstanceName = L"Reflection";
constexpr std::wstring_view kRegistryRoot = L"Software\\Reflection";
constexpr std::wstring_view kRunRegistryValueName = L"Reflection";
constexpr std::wstring_view kWebViewSubdir = L"Reflection";
#endif

// --------------------------------------------------------------------------
// Registry Value Names (shared across all instances)
// --------------------------------------------------------------------------
constexpr std::wstring_view kRegKeyServerName = L"ServerName";
constexpr std::wstring_view kRegKeyStartOnLogin = L"StartOnLogin";
constexpr std::wstring_view kRegKeyMinimizeToTray = L"MinimizeToTray";
constexpr std::wstring_view kRegKeyOnboardingCompleted = L"OnboardingCompleted";
constexpr std::wstring_view kRegKeyTheme = L"Theme";
constexpr std::wstring_view kRegKeyMirrorWindowX = L"MirrorWindowX";
constexpr std::wstring_view kRegKeyMirrorWindowY = L"MirrorWindowY";
constexpr std::wstring_view kRegKeyMirrorWindowW = L"MirrorWindowWidth";
constexpr std::wstring_view kRegKeyMirrorWindowH = L"MirrorWindowHeight";

// Auto-start registry path (shared system key — only the VALUE name is instance-qualified)
constexpr std::wstring_view kRunRegistryPath =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";

// --------------------------------------------------------------------------
// Update Checker
// --------------------------------------------------------------------------
constexpr std::string_view kGitHubApiUrl =
    "https://api.github.com/repos/SatanshuMishra/reflection/releases/latest";
constexpr std::wstring_view kLastSeenVersionKey = L"LastSeenVersion";

// --------------------------------------------------------------------------
// Auto-Update (WinSparkle)
// --------------------------------------------------------------------------
constexpr std::string_view kAppcastUrl =
    "https://satanshumishra.github.io/reflection/appcast.xml";

// Ed25519 public key for update signature verification (base64).
// Generated with: winsparkle-tool generate-key
// Private key stored as GitHub Actions secret WINSPARKLE_EDDSA_PRIVATE_KEY.
constexpr std::string_view kEdDsaPublicKey =
    "gOGNoc7imBogO/zxFakOlD2nfgBkeW7TB/bl8tvQUDo=";

constexpr std::wstring_view kRegKeyAutoUpdateEnabled = L"AutoUpdateEnabled";

// Check for updates every 12 hours (43200 seconds)
constexpr int kUpdateCheckIntervalSec = 43200;

// --------------------------------------------------------------------------
// Logging
// --------------------------------------------------------------------------
constexpr std::wstring_view kLogFileName = L"reflection.log";

// --------------------------------------------------------------------------
// System Tray
// --------------------------------------------------------------------------
constexpr uint32_t kTrayIconId = 1;
constexpr uint32_t kWmTrayIcon = 0x8000 + 1; // WM_APP + 1

// --------------------------------------------------------------------------
// Custom Window Messages (WM_APP = 0x8000)
// --------------------------------------------------------------------------
// Note: kWmTrayIcon uses WM_APP + 1 (0x8001)
constexpr uint32_t kWmIpadConnected = 0x8000 + 2;       // WM_APP + 2
constexpr uint32_t kWmIpadDisconnected = 0x8000 + 3;     // WM_APP + 3
constexpr uint32_t kWmMirrorWindowClosed = 0x8000 + 4;   // WM_APP + 4
constexpr uint32_t kWmThemeChanged = 0x8000 + 5;          // WM_APP + 5
constexpr uint32_t kWmOnboardingComplete = 0x8000 + 6;    // WM_APP + 6
constexpr uint32_t kWmServerNameChanged = 0x8000 + 7;     // WM_APP + 7
constexpr uint32_t kWmFirewallGranted = 0x8000 + 8;       // WM_APP + 8
constexpr uint32_t kWmForceReannounce = 0x8000 + 9;      // WM_APP + 9

// --------------------------------------------------------------------------
// Render Timer
// --------------------------------------------------------------------------
constexpr uintptr_t kRenderTimerId = 1;
constexpr uint32_t kRenderTimerIntervalMs = 16;  // ~60fps polling (renders only when new frame available)

} // namespace reflection::constants
