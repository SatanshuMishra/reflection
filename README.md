# Reflection

Mirror your iPad screen to your Mac or PC. Share presentations, demos, and more with the world.

## Installation

### macOS

1. Download the latest `.dmg` from [Releases](https://github.com/SatanshuMishra/reflection/releases)
2. Open the DMG and drag **Reflection** into the **Applications** folder
3. Launch Reflection from Applications

#### Opening Reflection for the First Time (macOS)

Reflection is distributed as an open-source, ad-hoc signed application. macOS Gatekeeper may display a warning that the app is from an unidentified developer.

**To open Reflection:**

1. **Right-click** (or Control-click) the app and select **Open** -- on macOS versions before Sequoia 15.1, this is sufficient
2. On **macOS Sequoia 15.1+**, go to **System Settings > Privacy & Security**, scroll down, and click **Open Anyway** next to the Reflection entry
3. Alternatively, run this command in Terminal before first launch:

   ```bash
   xattr -cr /Applications/Reflection.app
   ```

> This is standard behavior for all macOS apps distributed outside the App Store without a paid Apple Developer ID ($99/year). The app contains no malware -- the source code is fully open and auditable.

### Windows

**Recommended:** Download the **Setup installer** (`.exe`) from [Releases](https://github.com/SatanshuMishra/reflection/releases). The installer:
- Installs to Program Files with Start Menu shortcuts
- Configures Windows Firewall automatically
- Registers an uninstaller in Settings > Apps

**Portable:** Download the `.zip` if you prefer a standalone copy -- extract to any folder and run `Reflection.exe`.

To uninstall: **Settings > Apps > Reflection > Uninstall**, or use the Start Menu shortcut.

#### Windows SmartScreen Warning

Windows SmartScreen may display a warning that the app is from an unknown publisher. Click **More info**, then **Run anyway**. This is standard behavior for unsigned open-source applications.

## Usage

### macOS (USB)

1. Connect your iPad to your Mac via USB
2. If prompted, tap **Trust** on your iPad
3. Grant Reflection camera access when prompted (iPads appear as camera devices through CoreMediaIO)
4. Click **Mirror** next to your iPad in the device list

### Windows (AirPlay / Wireless)

1. Ensure your iPad and PC are on the same Wi-Fi network
2. Launch Reflection -- the onboarding wizard guides you through setup on first run
3. Open Control Center on your iPad, tap **Screen Mirroring**, and select your PC's name
4. Your iPad screen appears in the Reflection mirror window

## Requirements

### macOS

- macOS 13.0 (Ventura) or later
- iPad connected via USB cable
- Camera permission granted

### Windows

- Windows 10 version 1809 or later (Windows 11 recommended)
- iPad and PC on the same Wi-Fi network
- Windows Firewall exception for Reflection

## Building from Source

### macOS

```bash
swift build       # Build
swift test        # Run tests
```

Requires Xcode 16+ with Swift 6.0.

### Windows

**Prerequisites:**
- Visual Studio 2022 with C++ Desktop Development workload
- [MSYS2](https://www.msys2.org/) (for GStreamer)
- [vcpkg](https://vcpkg.io/) (for dependencies)

**MSYS2 packages:**
```bash
pacman -S mingw-w64-ucrt-x86_64-gstreamer \
          mingw-w64-ucrt-x86_64-gst-plugins-base \
          mingw-w64-ucrt-x86_64-gst-plugins-good \
          mingw-w64-ucrt-x86_64-gst-plugins-bad \
          mingw-w64-ucrt-x86_64-gst-libav
```

**Build:**
```bash
cd windows
cmake -B build -A x64 \
  -DCMAKE_TOOLCHAIN_FILE=<vcpkg-root>/scripts/buildsystems/vcpkg.cmake \
  -DVCPKG_TARGET_TRIPLET=x64-windows \
  -DBUILD_TESTS=ON

cmake --build build --config Release
ctest --test-dir build --config Release
```

Requires CMake 3.24+ and C++20 support.

## Roadmap

High-level overview of planned features and platform support.

### Mirroring

- [x] **USB mirroring** -- Mirror iPad screen over a wired USB connection (macOS)
- [x] **Wireless mirroring** -- Mirror iPad screen over Wi-Fi via AirPlay (Windows)
- [ ] **Audio passthrough** -- Route iPad audio alongside the video feed
- [ ] **Multi-device mirroring** -- Display multiple iPads simultaneously

### Cross-Platform

- [x] **Windows support**
- [ ] **Linux support**

### App Distribution

- [ ] **Sparkle auto-updates** -- In-app update notifications and one-click upgrades

## Contributing

Contributions are welcome! Check out the [**Contributing Guide**](CONTRIBUTING.md) to get started.

## Community

- [**Contributing Guide**](CONTRIBUTING.md) -- How to report bugs, suggest features, and submit code
- [**Code of Conduct**](CODE_OF_CONDUCT.md) -- Standards for community participation
- [**Security Policy**](SECURITY.md) -- How to report vulnerabilities privately

## License

[GPL-3.0 License](LICENSE) -- see the full text for details.

Third-party dependency licenses are documented in [THIRD_PARTY_LICENSES.md](THIRD_PARTY_LICENSES.md).
