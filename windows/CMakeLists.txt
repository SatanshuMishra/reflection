cmake_minimum_required(VERSION 3.24)

project(Reflection
    VERSION 1.0.0
    DESCRIPTION "AirPlay iPad screen mirroring for Windows"
    LANGUAGES C CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build options
option(BUILD_TESTS "Build unit tests" ON)
option(BUILD_INSTALLER "Build MSI installer" OFF)
option(USE_UXPLAY "Use UxPlay instead of RPiPlay for AirPlay protocol" ON)

# vcpkg integration (auto-detected via CMAKE_TOOLCHAIN_FILE)
find_package(OpenSSL REQUIRED)

# libplist — required by RPiPlay for AirPlay RTSP plist parsing
find_path(PLIST_INCLUDE_DIR NAMES plist/plist.h)
find_library(PLIST_LIBRARY NAMES plist plist-2.0 libplist libplist-2.0)
if(NOT PLIST_INCLUDE_DIR OR NOT PLIST_LIBRARY)
    message(FATAL_ERROR
        "libplist not found. Install via: vcpkg install libplist:x64-windows\n"
        "Then re-run cmake with -DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake"
    )
endif()
message(STATUS "Found libplist: ${PLIST_LIBRARY}")

# GStreamer — required for decode/render pipeline when USE_UXPLAY is ON
find_package(PkgConfig QUIET)
if(USE_UXPLAY)
    if(PkgConfig_FOUND)
        # Use pkg-config (works with both MSYS2 and GStreamer MSVC installer)
        # For MSVC installer: set PKG_CONFIG_PATH=C:/gstreamer/1.0/msvc_x86_64/lib/pkgconfig
        # and ensure C:/gstreamer/1.0/msvc_x86_64/bin is in PATH
        pkg_check_modules(GST REQUIRED gstreamer-1.0)
        pkg_check_modules(GST_APP REQUIRED gstreamer-app-1.0)
        pkg_check_modules(GST_VIDEO REQUIRED gstreamer-video-1.0)

        message(STATUS "Found GStreamer: ${GST_VERSION}")
        message(STATUS "  Include dirs: ${GST_INCLUDE_DIRS}")
        message(STATUS "  Libraries: ${GST_LIBRARIES}")
    else()
        # Fallback: try to find GStreamer via environment variable
        if(DEFINED ENV{GSTREAMER_1_0_ROOT_MSVC_X86_64})
            set(GST_ROOT "$ENV{GSTREAMER_1_0_ROOT_MSVC_X86_64}")
            message(STATUS "Using GStreamer from GSTREAMER_1_0_ROOT_MSVC_X86_64: ${GST_ROOT}")
            set(GST_INCLUDE_DIRS "${GST_ROOT}/include/gstreamer-1.0" "${GST_ROOT}/include/glib-2.0" "${GST_ROOT}/lib/glib-2.0/include")
            set(GST_LIBRARY_DIRS "${GST_ROOT}/lib")
            set(GST_LIBRARIES gstreamer-1.0 glib-2.0 gobject-2.0)
            set(GST_APP_LIBRARIES gstapp-1.0)
            set(GST_VIDEO_LIBRARIES gstvideo-1.0)
            set(GST_APP_INCLUDE_DIRS ${GST_INCLUDE_DIRS})
            set(GST_VIDEO_INCLUDE_DIRS ${GST_INCLUDE_DIRS})
            set(GST_APP_LIBRARY_DIRS ${GST_LIBRARY_DIRS})
            set(GST_VIDEO_LIBRARY_DIRS ${GST_LIBRARY_DIRS})
        else()
            message(FATAL_ERROR
                "GStreamer not found. Install options:\n"
                "  1. MSVC: Install GStreamer MSVC from https://gstreamer.freedesktop.org/download/\n"
                "     Set PKG_CONFIG_PATH=C:/gstreamer/1.0/msvc_x86_64/lib/pkgconfig\n"
                "     Add C:/gstreamer/1.0/msvc_x86_64/bin to PATH\n"
                "  2. MSYS2: pacman -S mingw-w64-ucrt-x86_64-gstreamer mingw-w64-ucrt-x86_64-gst-plugins-base"
            )
        endif()
    endif()
endif()

# FDK-AAC via vcpkg (optional — only needed for legacy RPiPlay pipeline)
if(NOT USE_UXPLAY AND PkgConfig_FOUND)
    pkg_check_modules(FDK_AAC fdk-aac)
endif()
if(NOT USE_UXPLAY AND NOT FDK_AAC_FOUND)
    find_library(FDK_AAC_LIBRARIES NAMES fdk-aac QUIET)
    find_path(FDK_AAC_INCLUDE_DIRS NAMES fdk-aac/aacenc_lib.h QUIET)
endif()

# --------------------------------------------------------------------------
# AirPlay core library
# --------------------------------------------------------------------------
if(USE_UXPLAY)
    message(STATUS "Using UxPlay for AirPlay protocol (quality negotiation enabled)")
    add_subdirectory(lib/uxplay)
    set(AIRPLAY_CORE_LIB uxplay_core)
else()
    message(STATUS "Using RPiPlay for AirPlay protocol (legacy)")
    add_subdirectory(lib/airplay)
    set(AIRPLAY_CORE_LIB airplay_core)
endif()

# --------------------------------------------------------------------------
# mjansson/mdns (header-only)
# --------------------------------------------------------------------------
add_library(mdns INTERFACE)
target_include_directories(mdns INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/lib/mdns)

# --------------------------------------------------------------------------
# Main executable
# --------------------------------------------------------------------------
set(REFLECTION_SOURCES
    src/main.cpp
    src/app/App.cpp
    src/app/SystemTray.cpp
    src/app/WindowManager.cpp
    src/capture/CaptureState.cpp
    src/capture/CaptureError.cpp
    src/capture/AirPlayCapture.cpp
    src/session/MirrorSessionManager.cpp
    src/session/FrameStaleMonitor.cpp
    src/airplay/AirPlayService.cpp
    src/airplay/AirPlayCallbacks.cpp
    src/airplay/AirPlayCallbackBridge.cpp
    src/airplay/AirPlayTypes.cpp
    src/mdns/MdnsServiceRecord.cpp
    src/mdns/MjanssonMdnsAdvertiser.cpp
    src/mdns/NativeMdnsAdvertiser.cpp
    src/views/MirrorWindow.cpp
    src/views/OnboardingWindow.cpp
    src/views/OverlayRenderer.cpp
    src/settings/AppSettings.cpp
    src/settings/AutoStartService.cpp
    src/utilities/Logger.cpp
    src/utilities/UpdateChecker.cpp
)

# Add the appropriate AirPlay core wrapper and pipeline
if(USE_UXPLAY)
    # UxPlay + GStreamer path: GStreamer handles ALL decode/render
    list(APPEND REFLECTION_SOURCES
        src/airplay/UxPlayCore.cpp
        src/pipeline/GStreamerPipeline.cpp
    )
else()
    # Legacy RPiPlay path: uses MF/D3D11 decode/render
    list(APPEND REFLECTION_SOURCES
        src/airplay/RPiPlayCore.cpp
        src/decode/MFVideoDecoder.cpp
        src/decode/FdkAacDecoder.cpp
        src/render/D3D11Renderer.cpp
        src/render/WasapiRenderer.cpp
    )
endif()

add_executable(Reflection WIN32 ${REFLECTION_SOURCES})

target_include_directories(Reflection PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# GStreamer include directories
if(USE_UXPLAY)
    target_include_directories(Reflection PRIVATE
        ${GST_INCLUDE_DIRS}
        ${GST_APP_INCLUDE_DIRS}
        ${GST_VIDEO_INCLUDE_DIRS}
    )
endif()

target_link_libraries(Reflection PRIVATE
    # Vendored libraries
    ${AIRPLAY_CORE_LIB}
    mdns

    # vcpkg dependencies
    OpenSSL::SSL
    OpenSSL::Crypto

    # Windows system libraries (always needed)
    ws2_32
    iphlpapi
    ole32
    oleaut32
    uuid
    shell32
    comctl32
    winhttp
)

# Legacy path: D3D11 + Media Foundation (only when NOT using GStreamer)
if(NOT USE_UXPLAY)
    target_link_libraries(Reflection PRIVATE
        d3d11
        dxgi
        d3dcompiler
        mf
        mfplat
        mfreadwrite
        mfuuid
        wmcodecdspuuid
    )
endif()

# Link GStreamer libraries
if(USE_UXPLAY)
    target_link_libraries(Reflection PRIVATE
        ${GST_LIBRARIES}
        ${GST_APP_LIBRARIES}
        ${GST_VIDEO_LIBRARIES}
    )
    target_link_directories(Reflection PRIVATE
        ${GST_LIBRARY_DIRS}
        ${GST_APP_LIBRARY_DIRS}
        ${GST_VIDEO_LIBRARY_DIRS}
    )
endif()

# Link FDK-AAC if found
if(FDK_AAC_LIBRARIES)
    target_link_libraries(Reflection PRIVATE ${FDK_AAC_LIBRARIES})
    target_include_directories(Reflection PRIVATE ${FDK_AAC_INCLUDE_DIRS})
    target_compile_definitions(Reflection PRIVATE HAS_FDK_AAC=1)
endif()

# Windows-specific settings
target_compile_definitions(Reflection PRIVATE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    UNICODE
    _UNICODE
    _CRT_SECURE_NO_WARNINGS
)

# UxPlay compile definition
if(USE_UXPLAY)
    target_compile_definitions(Reflection PRIVATE USE_UXPLAY=1)
endif()

# Application manifest for DPI awareness and UTF-8
set_target_properties(Reflection PROPERTIES
    VS_DPI_AWARE "PerMonitor"
)

# --------------------------------------------------------------------------
# Tests
# --------------------------------------------------------------------------
if(BUILD_TESTS)
    find_package(GTest CONFIG QUIET)
    if(GTest_FOUND)
        enable_testing()
        add_subdirectory(tests)
    else()
        message(WARNING
            "GoogleTest not found — tests will be skipped.\n"
            "Install via: vcpkg install gtest:x64-windows\n"
            "Then re-run cmake with -DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake"
        )
    endif()
endif()

# --------------------------------------------------------------------------
# Installer (CPack + WiX)
# --------------------------------------------------------------------------
if(BUILD_INSTALLER)
    set(CPACK_GENERATOR "WIX")
    set(CPACK_PACKAGE_NAME "Reflection")
    set(CPACK_PACKAGE_VENDOR "SatanshuMishra")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Mirror your iPad screen wirelessly via AirPlay")
    set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
    set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
    set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})
    set(CPACK_PACKAGE_INSTALL_DIRECTORY "Reflection")
    set(CPACK_WIX_UPGRADE_GUID "A1B2C3D4-E5F6-7890-ABCD-EF1234567890")
    include(CPack)
endif()
