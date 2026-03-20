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

constexpr int kOnboardingWidth = 800;
constexpr int kOnboardingHeight = 600;

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
// Timing (matching macOS FrameStaleMonitor)
// --------------------------------------------------------------------------
constexpr double kFrameStaleThresholdSec = 1.0;
constexpr uint64_t kFrameCheckIntervalMs = 500;

// --------------------------------------------------------------------------
// Animation
// --------------------------------------------------------------------------
constexpr double kAnimationDurationSec = 0.3;

// --------------------------------------------------------------------------
// Registry Keys
// --------------------------------------------------------------------------
constexpr std::wstring_view kRegistryRoot = L"Software\\Reflection";
constexpr std::wstring_view kRegKeyServerName = L"ServerName";
constexpr std::wstring_view kRegKeyStartOnLogin = L"StartOnLogin";
constexpr std::wstring_view kRegKeyMinimizeToTray = L"MinimizeToTray";
constexpr std::wstring_view kRegKeyOnboardingCompleted = L"OnboardingCompleted";
constexpr std::wstring_view kRegKeyMirrorWindowX = L"MirrorWindowX";
constexpr std::wstring_view kRegKeyMirrorWindowY = L"MirrorWindowY";
constexpr std::wstring_view kRegKeyMirrorWindowW = L"MirrorWindowWidth";
constexpr std::wstring_view kRegKeyMirrorWindowH = L"MirrorWindowHeight";

// Auto-start registry path
constexpr std::wstring_view kRunRegistryPath =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr std::wstring_view kRunRegistryValueName = L"Reflection";

// --------------------------------------------------------------------------
// Update Checker
// --------------------------------------------------------------------------
constexpr std::string_view kGitHubApiUrl =
    "https://api.github.com/repos/SatanshuMishra/reflection/releases/latest";
constexpr std::wstring_view kLastSeenVersionKey = L"LastSeenVersion";

// --------------------------------------------------------------------------
// Logging
// --------------------------------------------------------------------------
constexpr std::wstring_view kLogFileName = L"reflection.log";

// --------------------------------------------------------------------------
// System Tray
// --------------------------------------------------------------------------
constexpr uint32_t kTrayIconId = 1;
constexpr uint32_t kWmTrayIcon = 0x8000 + 1; // WM_APP + 1

} // namespace reflection::constants
