# Changelog

All notable changes to the Windows version of Reflection are documented here.
This project follows [Semantic Versioning](https://semver.org/).

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
