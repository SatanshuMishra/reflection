# Building Reflection for Windows

## Prerequisites

1. **Visual Studio 2022** (Community edition is free)
   - Install with "Desktop development with C++" workload
   - Ensure C++20 support is included (default with VS 2022 17.x)

2. **Git** — https://git-scm.com/download/win

3. **vcpkg** — C++ package manager
   ```powershell
   cd C:\
   git clone https://github.com/microsoft/vcpkg.git
   cd vcpkg
   .\bootstrap-vcpkg.bat
   ```
   Add `C:\vcpkg` to your PATH, or set `VCPKG_ROOT=C:\vcpkg`.

4. **CMake 3.24+** (included with Visual Studio, or install separately)

## Install Dependencies

```powershell
# From the vcpkg directory
vcpkg install openssl:x64-windows
vcpkg install gtest:x64-windows

# Optional (needed for audio playback in later milestones)
vcpkg install fdk-aac:x64-windows
```

## Clone and Build

```powershell
# Clone the repository
git clone https://github.com/SatanshuMishra/reflection.git
cd reflection\windows

# Configure (CMake will download RPiPlay via FetchContent automatically)
cmake -B build -A x64 ^
    -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake ^
    -DVCPKG_TARGET_TRIPLET=x64-windows ^
    -DBUILD_TESTS=ON

# Build
cmake --build build --config Release

# Run tests
ctest --test-dir build --config Release --output-on-failure
```

## Run the Application

```powershell
# Run directly
.\build\Release\Reflection.exe

# Or from the build directory
cd build\Release
.\Reflection.exe
```

When launched:
1. A window titled "Reflection" appears
2. The app advertises via mDNS on your local network
3. On your iPad, open Control Center → Screen Mirroring
4. "Reflection" should appear in the list
5. Tap it to start mirroring

## Troubleshooting

### iPad doesn't see "Reflection"

1. **Firewall**: Allow Reflection.exe through Windows Firewall
   - Windows Security → Firewall & network protection → Allow an app through firewall
   - Add `Reflection.exe` for both Private and Public networks
   - Alternatively, temporarily disable firewall to test

2. **Same network**: iPad and Windows PC must be on the same Wi-Fi/LAN subnet.
   The iPad sends mDNS queries to 224.0.0.251 — both devices must be able
   to reach this multicast address.

3. **Ports**: Reflection uses:
   - UDP 5353 (mDNS)
   - TCP 5000 (RAOP/RTSP)
   - TCP 7000 (AirPlay HTTP)

4. **Antivirus**: Some antivirus software blocks mDNS multicast. Try
   temporarily disabling it.

### Build fails: "openssl not found"

Ensure vcpkg installed OpenSSL and you're passing the correct toolchain file:
```powershell
vcpkg install openssl:x64-windows
cmake -B build -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake ...
```

### Build fails: "FetchContent RPiPlay download failed"

FetchContent downloads RPiPlay from GitHub during cmake configure.
If behind a proxy or firewall, you may need to set:
```powershell
set HTTP_PROXY=http://proxy:port
set HTTPS_PROXY=http://proxy:port
```

### Tests fail: network-dependent tests

Some NativeMdnsAdvertiser tests require active network interfaces.
If running in a VM without network, those tests may fail. This is expected —
run on a real Windows PC with network access.

## Project Structure

```
windows/
├── CMakeLists.txt          # Root build config
├── vcpkg.json              # Package dependencies
├── lib/
│   ├── airplay/            # RPiPlay (downloaded via FetchContent)
│   │   ├── CMakeLists.txt  # Compiles RPiPlay lib/ as static library
│   │   └── dnssd_stub.c    # Stub DNS-SD (we handle mDNS ourselves)
│   └── mdns/
│       └── mdns.h          # mjansson/mdns header-only library
├── src/                    # Application source
└── tests/                  # GoogleTest unit tests
```

## What Works (Current Milestone)

- ✅ mDNS advertisement (iPad discovers "Reflection")
- ✅ RAOP/RTSP server (accepts AirPlay connections via RPiPlay)
- ✅ FairPlay handshake (iPad authenticates with the receiver)
- ✅ H.264 frame reception (video data arrives via callbacks)
- ⏳ Video decoding + rendering (Milestone 3 — not yet implemented)
- ⏳ Audio decoding + playback (Milestone 4 — not yet implemented)
- ⏳ System tray integration (Milestone 6)
