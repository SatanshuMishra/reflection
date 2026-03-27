# Changelog

All notable changes to the Windows version of Reflection are documented here.
This project follows [Semantic Versioning](https://semver.org/).

## [1.7.0] - 2026-03-26

### Added
- **Automatic updates via WinSparkle** — The app checks for new versions every 24 hours via a signed appcast feed on GitHub Pages. When an update is available, a native dialog shows release notes and offers one-click install. Updates are verified with Ed25519 signatures before installation.
- **"Check for Updates..." menu item** — Added to the system tray context menu for manual update checks.
- **"Automatic Updates" settings toggle** — New toggle in Settings to enable/disable automatic update checking (default: on).
- `scripts/generate-appcast.ps1` — CI script that signs the installer with Ed25519 and generates the WinSparkle appcast XML.

### Changed
- Release workflow now signs installers with Ed25519 and deploys appcast.xml to GitHub Pages for auto-update discovery.

## [1.6.3] - 2026-03-26

### Fixed
- **Crash on launch: `ffi-7.dll was not found`** — GStreamer DLL names were hardcoded (`ffi-8.dll`) but GStreamer 1.24 ships `ffi-7.dll`. Replaced hardcoded lists with automatic PE import-table walking via `dumpbin`/`objdump` that discovers the correct DLLs regardless of GStreamer version.
- **Uninstall leaves all files behind** — The app remained running during uninstall (launched at postinstall, never terminated), locking all 36 files. Added `taskkill` and `CloseApplications=force` to the Inno Setup uninstaller.
- **WebView2 cache not fully cleaned** — `delete_webview2_data()` only deleted the `WebView2/` subdirectory, missing `EBWebView/` (~29 MB). Now deletes the entire `%LOCALAPPDATA%\Reflection` directory with safety validation.
- **Duplicate firewall rules** — Both the installer and runtime UI created rules without checking for existing entries. Both now use idempotent delete-then-add pattern.
- **Log file locked during cleanup** — Logger shutdown now occurs before log file deletion so `DeleteFileW` can succeed.
- **UAC prompt during hidden uninstall** — `--uninstall-cleanup` no longer calls `reset_firewall()` (which triggered a redundant UAC dialog). The installer handles firewall cleanup directly with inherited admin privileges.

### Added
- `scripts/bundle-gstreamer.ps1` — Recursive DLL dependency walker for GStreamer bundling (replaces hardcoded lists in CMakeLists.txt and release.yml).
- `scripts/clean-reflection.ps1` — Standalone cleanup script for manual system recovery when the uninstaller fails or for dev workflows.

### Changed
- CMakeLists.txt DLL bundling now works for both MSYS2 and MSVC GStreamer installations (previously only handled MSYS2).
- Installer firewall rule deletion uses name-only match (no program filter) to catch rules from any exe path.

## [1.6.2] - 2026-03-24

### Added
- Security hardening, RDP rendering support, dev reset, instance isolation.
- CI hardening with pinned actions, verification, and artifact validation.
