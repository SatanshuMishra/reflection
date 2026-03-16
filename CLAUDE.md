# Reflection

macOS app that mirrors an iPad's screen over USB in a read-only window. Uses CoreMediaIO device discovery and AVFoundation capture — the same mechanism as QuickTime Player. No external dependencies. Swift 6.0, macOS 13+.

## Quick Reference

```bash
swift build                                    # Build (requires Xcode 16+)
swift test                                     # Run all tests (45+)
./scripts/make-app-bundle.sh [debug|release]   # Create .app bundle (release default)
open Reflection.app                            # Launch built app
```

If `swift test` fails with "no such module 'XCTest'":
```bash
export DEVELOPER_DIR=/Applications/Xcode.app/Contents/Developer
```

## Architecture

```
Sources/Reflection/
├── App/        → Entry point (ReflectionApp), AppDelegate, window lifecycle
├── Capture/    → ScreenCapture protocol, USBCapture, DeviceDiscovery, FrameStaleMonitor
├── Session/    → MirrorSessionManager (orchestration), DeviceModel (value type)
├── Views/      → SwiftUI views, MirrorWindowController, PreviewLayerView (NSViewRepresentable)
└── Utilities/  → Constants, Logger extensions
```

### Layer Flow

`ReflectionApp` → `MirrorSessionManager` → `USBCapture` (conforms to `ScreenCapture` protocol)
`AppDelegate` → `MirrorWindowController` → `MirrorWindowView` (SwiftUI) → `PreviewLayerView`

### Key Patterns

- **`ScreenCapture` protocol** — abstraction over capture backends. `USBCapture` is the only implementation today; protocol exists for future backends (wireless, ScreenCaptureKit)
- **`AsyncStream<CaptureState>`** — state changes flow from capture layer to UI via async streams, not Combine
- **`@MainActor` managers** — `MirrorSessionManager` is `@Observable` and `@MainActor`-isolated for SwiftUI
- **`NSViewRepresentable`** — bridges `AVCaptureVideoPreviewLayer` and `NSVisualEffectView` to SwiftUI
- **NotificationCenter** — window close events (`.mirrorWindowClosed`) trigger session cleanup in `MirrorSessionManager`

## Key Design Decisions

| Decision | Rationale |
|----------|-----------|
| `AVCaptureVideoPreviewLayer` | Zero-copy GPU rendering path — lowest possible latency |
| `NSWindowController` | SwiftUI cannot enforce aspect ratio on window resize |
| `FrameStaleMonitor` (separate class) | Independently testable without AVFoundation hardware |
| `AVCaptureVideoDataOutput` alongside preview | Monitors frame timestamps without processing pixels — detects iPad lock/sleep |
| No external packages | Keeps build simple; Apple frameworks provide everything needed |

## Testing

- **Mock strategy:** `MockScreenCapture` replaces all hardware dependencies
- **Concurrency:** All tests are `@MainActor` for Swift 6 strict concurrency safety
- **Timing:** Async tests use `Task.sleep` for stale-frame timing assertions
- **Coverage target:** 80%+

Test files: `Tests/ReflectionTests/{Capture,Session}/`

## CI/CD

| Workflow | Trigger | What it does |
|----------|---------|--------------|
| `ci.yml` | Push/PR to `main` | Build + test (Xcode 16.2) |
| `release.yml` | `v*` tag push | Build release → .app → .dmg → GitHub Release |

**Creating a release:**
```bash
git tag -a v1.1.0 -m "v1.1.0: description of changes"
git push origin v1.1.0
```

Code signing + notarization run automatically when Apple Developer secrets are configured. Without secrets, builds are ad-hoc signed.

**Required secrets for signed releases:** `APPLE_CERTIFICATE_BASE64`, `APPLE_CERTIFICATE_PASSWORD`, `KEYCHAIN_PASSWORD`, `APPLE_TEAM_NAME`, `APPLE_TEAM_ID`, `APPLE_ID`, `APPLE_APP_SPECIFIC_PASSWORD`

## Common Gotchas

- **CoreMediaIO setup:** Must set `kCMIOHardwarePropertyAllowScreenCaptureDevices = 1` before any device discovery call. This is done in `DeviceDiscovery.enableScreenCaptureDevices()`.
- **Camera entitlement only:** The app needs `com.apple.security.device.camera` — no USB entitlement required (CoreMediaIO surfaces iPads as camera devices).
- **`startRunning()` blocks:** `AVCaptureSession.startRunning()` is a blocking call — must dispatch to a background thread.
- **Swift 6 concurrency:**
  - Use `nonisolated(unsafe)` for non-Sendable properties accessed in `deinit`
  - Use `@unchecked Sendable` for classes with NSLock-protected state
  - Extract sync helper methods when NSLock can't be used in async contexts
- **Frame monitoring:** `FrameStaleMonitor` checks every 0.5s if the last frame is >1s old. When stale (iPad locked/sleeping), `MirrorWindowView` shows a blur overlay with lock message.

## Version

Current: **v1.1.0** — See `.claude/rules/versioning.md` for SemVer policy.
