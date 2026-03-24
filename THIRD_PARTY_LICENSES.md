# Third-Party Licenses

Reflection uses the following third-party libraries and components.

## UxPlay

- **Version**: v1.73.4
- **License**: GPL-3.0-or-later
- **Source**: https://github.com/FDH2/UxPlay
- **Integration**: Fetched via CMake FetchContent, compiled as static library
- **Purpose**: AirPlay protocol implementation (RTSP/RTP, FairPlay authentication)

## RPiPlay

- **License**: GPL-3.0
- **Source**: https://github.com/FDH2/RPiPlay
- **Integration**: Vendored in `lib/airplay/`
- **Purpose**: Legacy AirPlay protocol backend

## mjansson/mdns

- **License**: The Unlicense (public domain)
- **Source**: https://github.com/mjansson/mdns
- **Integration**: Header-only library in `windows/lib/mdns/`
- **Purpose**: mDNS multicast service advertisement

## GStreamer

- **License**: LGPL-2.1-or-later
- **Source**: https://gstreamer.freedesktop.org
- **Integration**: Runtime DLLs, dynamically linked
- **Purpose**: Hardware-accelerated video decode and render pipeline

## OpenSSL

- **License**: Apache-2.0
- **Source**: https://www.openssl.org
- **Integration**: vcpkg dependency, dynamically linked
- **Purpose**: TLS/SSL for AirPlay protocol encryption

## libplist

- **License**: LGPL-2.1-or-later
- **Source**: https://libimobiledevice.org
- **Integration**: vcpkg dependency, dynamically linked
- **Purpose**: Apple property list parsing for AirPlay RTSP messages

## pthreads4w

- **License**: Apache-2.0
- **Source**: https://sourceforge.net/projects/pthreads4w
- **Integration**: vcpkg dependency, statically linked (MSVC builds)
- **Purpose**: POSIX threads implementation for Windows

## WebView2 SDK

- **License**: Microsoft Software License
- **Source**: https://developer.microsoft.com/en-us/microsoft-edge/webview2
- **Integration**: NuGet package, static loader library
- **Purpose**: Embedded Chromium-based UI for onboarding and settings panels
