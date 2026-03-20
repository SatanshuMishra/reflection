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

# vcpkg integration (auto-detected via CMAKE_TOOLCHAIN_FILE)
find_package(OpenSSL REQUIRED)

# FDK-AAC via vcpkg (optional — needed for audio in later milestones)
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(FDK_AAC fdk-aac)
endif()
if(NOT FDK_AAC_FOUND)
    find_library(FDK_AAC_LIBRARIES NAMES fdk-aac QUIET)
    find_path(FDK_AAC_INCLUDE_DIRS NAMES fdk-aac/aacenc_lib.h QUIET)
endif()

# --------------------------------------------------------------------------
# AirPlay core library (RPiPlay lib/ — downloaded via FetchContent)
# --------------------------------------------------------------------------
add_subdirectory(lib/airplay)

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
    src/airplay/RPiPlayCore.cpp
    src/mdns/MdnsServiceRecord.cpp
    src/mdns/MjanssonMdnsAdvertiser.cpp
    src/mdns/NativeMdnsAdvertiser.cpp
    src/decode/MFVideoDecoder.cpp
    src/decode/FdkAacDecoder.cpp
    src/render/D3D11Renderer.cpp
    src/render/WasapiRenderer.cpp
    src/views/MirrorWindow.cpp
    src/views/OnboardingWindow.cpp
    src/views/OverlayRenderer.cpp
    src/settings/AppSettings.cpp
    src/settings/AutoStartService.cpp
    src/utilities/Logger.cpp
    src/utilities/UpdateChecker.cpp
)

add_executable(Reflection WIN32 ${REFLECTION_SOURCES})

target_include_directories(Reflection PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(Reflection PRIVATE
    # Vendored libraries
    airplay_core
    mdns

    # vcpkg dependencies
    OpenSSL::SSL
    OpenSSL::Crypto

    # Windows system libraries
    d3d11
    dxgi
    d3dcompiler
    mf
    mfplat
    mfreadwrite
    mfuuid
    wmcodecdspuuid
    ws2_32
    iphlpapi
    ole32
    oleaut32
    uuid
    shell32
    comctl32
    winhttp
)

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

# Application manifest for DPI awareness and UTF-8
set_target_properties(Reflection PROPERTIES
    VS_DPI_AWARE "PerMonitor"
)

# --------------------------------------------------------------------------
# Tests
# --------------------------------------------------------------------------
if(BUILD_TESTS)
    enable_testing()
    find_package(GTest CONFIG REQUIRED)
    add_subdirectory(tests)
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
