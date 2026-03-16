# Reflection

Mirror your iPad screen to your Mac over USB. Zero latency, read-only display using native macOS frameworks.

## Installation

1. Download the latest `.dmg` from [Releases](https://github.com/SatanshuMishra/reflection/releases)
2. Open the DMG and drag **Reflection** into the **Applications** folder
3. Launch Reflection from Applications

### Opening Reflection for the First Time

Reflection is distributed as an open-source, ad-hoc signed application. macOS Gatekeeper may display a warning that the app is from an unidentified developer.

**To open Reflection:**

1. **Right-click** (or Control-click) the app and select **Open** — on macOS versions before Sequoia 15.1, this is sufficient
2. On **macOS Sequoia 15.1+**, go to **System Settings → Privacy & Security**, scroll down, and click **Open Anyway** next to the Reflection entry
3. Alternatively, run this command in Terminal before first launch:

   ```bash
   xattr -cr /Applications/Reflection.app
   ```

   This removes the quarantine attribute that triggers the Gatekeeper warning.

> This is standard behavior for all macOS apps distributed outside the App Store without a paid Apple Developer ID ($99/year). The app contains no malware — the source code is fully open and auditable.

## Usage

1. Connect your iPad to your Mac via USB
2. If prompted, tap **Trust** on your iPad
3. Grant Reflection camera access when prompted (iPads appear as camera devices through CoreMediaIO)
4. Click **Mirror** next to your iPad in the device list

## Requirements

- macOS 13.0 (Ventura) or later
- iPad connected via USB cable
- Camera permission granted

## Building from Source

```bash
# Build
swift build

# Run tests
swift test

# Create .app bundle
./scripts/make-app-bundle.sh

# Create .app bundle + DMG
./scripts/make-app-bundle.sh release --dmg
```

Requires Xcode 16+ with Swift 6.0.

## Roadmap

High-level overview of planned features and platform support. Items are roughly ordered by priority.

### Mirroring

- [x] **USB mirroring** — Mirror iPad screen over a wired USB connection using CoreMediaIO
- [ ] **Wireless mirroring** — Mirror iPad screen over Wi-Fi without a cable (AirPlay or custom protocol)
- [ ] **Audio passthrough** — Route iPad audio to the Mac alongside the video feed
- [ ] **Multi-device mirroring** — Display multiple iPads simultaneously in separate windows

### Cross-Platform

- [ ] **Windows support** — Native Windows build using platform-equivalent capture APIs
- [ ] **Linux support** — Native Linux build with V4L2/PipeWire capture backend

### App Distribution

- [ ] **Sparkle auto-updates** — In-app update notifications and one-click upgrades via [Sparkle](https://sparkle-project.org/) (if user demand warrants it)

## Contributing

Contributions are welcome! Check out the [**Contributing Guide**](CONTRIBUTING.md) to get started.

## Community

- [**Contributing Guide**](CONTRIBUTING.md) — How to report bugs, suggest features, and submit code
- [**Code of Conduct**](CODE_OF_CONDUCT.md) — Standards for community participation
- [**Security Policy**](SECURITY.md) — How to report vulnerabilities privately

## License

[MIT License](LICENSE) — see the full text for details.
