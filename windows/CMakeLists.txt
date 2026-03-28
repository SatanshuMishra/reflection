cmake_minimum_required(VERSION 3.24)

project(Reflection
    VERSION 1.8.5
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

# --------------------------------------------------------------------------
# WebView2 SDK — for modern UI panels (onboarding, settings, status window)
# The WebView2 Evergreen runtime is pre-installed on Windows 10/11.
#
# Download the NuGet package at configure time if not already present.
# The .nupkg is a zip file containing headers and static lib.
# --------------------------------------------------------------------------
set(WEBVIEW2_VERSION "1.0.2903.40")
set(WEBVIEW2_DIR "${CMAKE_CURRENT_BINARY_DIR}/webview2-sdk")
set(WEBVIEW2_INCLUDE_DIR "${WEBVIEW2_DIR}/build/native/include")
set(WEBVIEW2_LIB_DIR "${WEBVIEW2_DIR}/build/native/x64")

if(NOT EXISTS "${WEBVIEW2_INCLUDE_DIR}/WebView2.h")
    set(_wv2_nupkg "${CMAKE_CURRENT_BINARY_DIR}/webview2.nupkg")
    if(NOT EXISTS "${_wv2_nupkg}")
        message(STATUS "Downloading WebView2 SDK ${WEBVIEW2_VERSION}...")
        file(DOWNLOAD
            "https://www.nuget.org/api/v2/package/Microsoft.Web.WebView2/${WEBVIEW2_VERSION}"
            "${_wv2_nupkg}"
            SHOW_PROGRESS
            STATUS _dl_status
        )
        list(GET _dl_status 0 _dl_code)
        if(NOT _dl_code EQUAL 0)
            message(FATAL_ERROR "Failed to download WebView2 SDK: ${_dl_status}")
        endif()
    endif()
    message(STATUS "Extracting WebView2 SDK...")
    file(ARCHIVE_EXTRACT INPUT "${_wv2_nupkg}" DESTINATION "${WEBVIEW2_DIR}")
endif()

if(NOT EXISTS "${WEBVIEW2_INCLUDE_DIR}/WebView2.h")
    # Search for headers in case of different package layout
    file(GLOB_RECURSE _wv2_headers "${WEBVIEW2_DIR}/**/WebView2.h")
    if(_wv2_headers)
        list(GET _wv2_headers 0 _wv2_hdr)
        get_filename_component(WEBVIEW2_INCLUDE_DIR "${_wv2_hdr}" DIRECTORY)
    else()
        message(FATAL_ERROR "WebView2.h not found after extraction")
    endif()
    file(GLOB_RECURSE _wv2_libs "${WEBVIEW2_DIR}/**/WebView2LoaderStatic.lib")
    if(_wv2_libs)
        list(GET _wv2_libs 0 _wv2_lpath)
        get_filename_component(WEBVIEW2_LIB_DIR "${_wv2_lpath}" DIRECTORY)
    endif()
endif()

message(STATUS "WebView2 SDK: ${WEBVIEW2_INCLUDE_DIR}")
message(STATUS "WebView2 lib: ${WEBVIEW2_LIB_DIR}")

# --------------------------------------------------------------------------
# WinSparkle — auto-update library (Ed25519 signed appcast + native UI)
# Installed via vcpkg. Ships as a single DLL with zero dependencies.
# --------------------------------------------------------------------------
find_library(WINSPARKLE_LIBRARY NAMES WinSparkle REQUIRED)
find_path(WINSPARKLE_INCLUDE_DIR NAMES winsparkle/winsparkle.h REQUIRED)
# WinSparkle DLL path — vcpkg manifest mode places installed files
# under build/vcpkg_installed/<triplet>/bin/
get_filename_component(_WINSPARKLE_LIB_DIR "${WINSPARKLE_LIBRARY}" DIRECTORY)
get_filename_component(_WINSPARKLE_PREFIX "${_WINSPARKLE_LIB_DIR}" DIRECTORY)
find_file(WINSPARKLE_DLL NAMES WinSparkle.dll
    PATHS "${_WINSPARKLE_PREFIX}/bin"
          "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin"
    REQUIRED
)
message(STATUS "WinSparkle: ${WINSPARKLE_LIBRARY}")
message(STATUS "WinSparkle DLL: ${WINSPARKLE_DLL}")

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
    src/views/MirrorOverlay.cpp
    src/views/OnboardingWindow.cpp
    src/views/StatusWindow.cpp
    src/views/OverlayRenderer.cpp
    src/settings/AppSettings.cpp
    src/settings/AutoStartService.cpp
    src/utilities/Logger.cpp
    src/utilities/UpdateChecker.cpp
    src/utilities/NameGenerator.cpp
    src/ui/WebViewHost.cpp
    src/ui/ThemeManager.cpp
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

# Windows resource file (application icon)
set(REFLECTION_RC "${CMAKE_CURRENT_SOURCE_DIR}/src/resources/app.rc")

add_executable(Reflection WIN32 ${REFLECTION_SOURCES} ${REFLECTION_RC})

target_include_directories(Reflection PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${WEBVIEW2_INCLUDE_DIR}
    ${WINSPARKLE_INCLUDE_DIR}
)

# GStreamer include directories
# Filter out MSYS2 system-wide include paths (e.g., C:/msys64/ucrt64/include)
# that pkg-config injects — MSVC cannot parse GCC-specific headers from those paths.
# We only need the GStreamer-specific include dirs (gstreamer-1.0/, glib-2.0/).
if(USE_UXPLAY)
    set(_GST_ALL_INCLUDES
        ${GST_INCLUDE_DIRS}
        ${GST_APP_INCLUDE_DIRS}
        ${GST_VIDEO_INCLUDE_DIRS}
    )
    list(REMOVE_DUPLICATES _GST_ALL_INCLUDES)

    set(_GST_FILTERED_INCLUDES "")
    foreach(_inc ${_GST_ALL_INCLUDES})
        # Keep only GStreamer/GLib-specific include paths, skip bare system dirs
        if(_inc MATCHES "gstreamer" OR _inc MATCHES "glib")
            list(APPEND _GST_FILTERED_INCLUDES "${_inc}")
        endif()
    endforeach()

    if(_GST_FILTERED_INCLUDES)
        target_include_directories(Reflection PRIVATE ${_GST_FILTERED_INCLUDES})
        message(STATUS "GStreamer includes (filtered): ${_GST_FILTERED_INCLUDES}")
    else()
        # Fallback: use all includes if none matched the filter
        target_include_directories(Reflection PRIVATE ${_GST_ALL_INCLUDES})
        message(WARNING "GStreamer include filter matched nothing — using all: ${_GST_ALL_INCLUDES}")
    endif()
endif()

target_link_libraries(Reflection PRIVATE
    # Vendored libraries
    ${AIRPLAY_CORE_LIB}
    mdns

    # vcpkg dependencies
    OpenSSL::SSL
    OpenSSL::Crypto

    # WebView2 (static loader — no DLL dependency)
    ${WEBVIEW2_LIB_DIR}/WebView2LoaderStatic.lib

    # WinSparkle (auto-update)
    ${WINSPARKLE_LIBRARY}

    # Windows system libraries (always needed)
    ws2_32
    iphlpapi
    gdiplus
    dwmapi
    shcore
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
    if(MSVC AND GST_LIBRARY_DIRS MATCHES "msys64")
        # MSVC cannot link against MSYS2/MinGW .dll.a libraries directly.
        # Use the full paths to the .dll.a files — MSVC can handle them
        # when given as full paths (it delegates to the librarian).
        # If this fails, install GStreamer MSVC from:
        #   https://gstreamer.freedesktop.org/download/
        set(_GST_LINK_LIBS "")
        set(_GST_LIB_DIR "${GST_LIBRARY_DIRS}")
        foreach(_lib gstreamer-1.0 gstapp-1.0 gstbase-1.0 gstvideo-1.0
                     gobject-2.0 glib-2.0 intl)
            if(EXISTS "${_GST_LIB_DIR}/lib${_lib}.dll.a")
                list(APPEND _GST_LINK_LIBS "${_GST_LIB_DIR}/lib${_lib}.dll.a")
            elseif(EXISTS "${_GST_LIB_DIR}/lib${_lib}.a")
                list(APPEND _GST_LINK_LIBS "${_GST_LIB_DIR}/lib${_lib}.a")
            else()
                message(WARNING "GStreamer lib not found: ${_lib}")
            endif()
        endforeach()
        target_link_libraries(Reflection PRIVATE ${_GST_LINK_LIBS})
        message(STATUS "GStreamer linking (MSVC + MSYS2): ${_GST_LINK_LIBS}")
    else()
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

# Version string available at compile time (for WinSparkle app details)
target_compile_definitions(Reflection PRIVATE
    REFLECTION_VERSION="${CMAKE_PROJECT_VERSION}"
)

# Application manifest is embedded via app.rc → app.manifest.
# It declares Windows 10/11 compatibility (required for accurate OS version
# detection by WinSparkle), per-monitor DPI awareness, and UTF-8 code page.
# Do NOT set VS_DPI_AWARE here — it conflicts with the manifest's dpiAwareness.
# /MANIFEST:NO prevents the linker from auto-generating a second manifest
# resource, which would conflict with the one in app.rc (CVT1100 duplicate).
target_link_options(Reflection PRIVATE "/MANIFEST:NO")

# Copy UI assets (HTML/CSS/JS) next to the executable
set(UI_ASSETS_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src/ui/assets")
add_custom_command(TARGET Reflection POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${UI_ASSETS_SRC}"
        "$<TARGET_FILE_DIR:Reflection>/assets"
    COMMENT "Copying UI assets to build output"
)

# Copy WinSparkle.dll next to the executable (required at runtime)
add_custom_command(TARGET Reflection POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${WINSPARKLE_DLL}"
        "$<TARGET_FILE_DIR:Reflection>"
    COMMENT "Copying WinSparkle.dll to build output"
)

# --------------------------------------------------------------------------
# Bundle GStreamer runtime DLLs next to the executable.
# Uses automatic PE import-table walking (dumpbin/objdump) to discover all
# transitive DLL dependencies instead of hardcoded lists. This prevents
# breakage when GStreamer versions change DLL names (e.g., ffi-7 vs ffi-8).
# --------------------------------------------------------------------------
if(USE_UXPLAY)
    # Determine GStreamer bin/ and plugin/ directories for both MSYS2 and MSVC
    if(GST_LIBRARY_DIRS MATCHES "msys64")
        # GST_LIBRARY_DIRS from pkg_check_modules may be a CMake list —
        # extract only the first element for path derivation.
        list(GET GST_LIBRARY_DIRS 0 _GST_LIB_DIR_FIRST)
        string(REPLACE "/lib" "/bin" _GST_BIN_DIR "${_GST_LIB_DIR_FIRST}")
        string(REPLACE "\\lib" "\\bin" _GST_BIN_DIR "${_GST_BIN_DIR}")
        string(REGEX REPLACE "[/\\\\]bin$" "" _GST_PREFIX "${_GST_BIN_DIR}")
        set(_GST_PLUGIN_DIR "${_GST_PREFIX}/lib/gstreamer-1.0")
    elseif(DEFINED ENV{GSTREAMER_1_0_ROOT_MSVC_X86_64})
        set(_GST_BIN_DIR "$ENV{GSTREAMER_1_0_ROOT_MSVC_X86_64}/bin")
        set(_GST_PLUGIN_DIR "$ENV{GSTREAMER_1_0_ROOT_MSVC_X86_64}/lib/gstreamer-1.0")
    else()
        message(WARNING "GStreamer bin directory not found — DLLs will not be bundled")
    endif()

    if(DEFINED _GST_BIN_DIR)
        # Post-build: auto-discover and copy GStreamer DLLs + plugins
        add_custom_command(TARGET Reflection POST_BUILD
            COMMAND powershell -ExecutionPolicy Bypass -File
                "${CMAKE_CURRENT_SOURCE_DIR}/scripts/bundle-gstreamer.ps1"
                -ExePath "$<TARGET_FILE:Reflection>"
                -SearchDirs \"${_GST_BIN_DIR}\"
                -OutputDir "$<TARGET_FILE_DIR:Reflection>"
                -PluginSrcDir \"${_GST_PLUGIN_DIR}\"
                -PluginOutputDir "$<TARGET_FILE_DIR:Reflection>/plugins"
            COMMENT "Bundling GStreamer runtime DLLs (auto-discovered via import table)"
        )
        message(STATUS "GStreamer DLLs will be auto-bundled from: ${_GST_BIN_DIR}")
        message(STATUS "GStreamer plugins from: ${_GST_PLUGIN_DIR}")
    endif()
endif()

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
# Install rules (used by CPack for MSI/ZIP packaging)
# --------------------------------------------------------------------------
install(TARGETS Reflection RUNTIME DESTINATION .)
install(DIRECTORY "$<TARGET_FILE_DIR:Reflection>/assets"
        DESTINATION . OPTIONAL)
install(DIRECTORY "$<TARGET_FILE_DIR:Reflection>/plugins"
        DESTINATION . OPTIONAL)

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
    # PERMANENT: This GUID identifies Reflection for WiX/MSI upgrades.
    # Changing it causes Windows Installer to treat updates as separate products.
    # Generated 2026-03-24.
    set(CPACK_WIX_UPGRADE_GUID "BCF3A179-BA9E-4D4E-A3BB-129A81A5E57A")
    include(CPack)
endif()
