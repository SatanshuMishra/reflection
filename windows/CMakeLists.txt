cmake_minimum_required(VERSION 3.24)

project(Reflection
    VERSION 1.6.0
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

# Application manifest for DPI awareness and UTF-8
set_target_properties(Reflection PROPERTIES
    VS_DPI_AWARE "PerMonitor"
)

# Copy UI assets (HTML/CSS/JS) next to the executable
set(UI_ASSETS_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src/ui/assets")
add_custom_command(TARGET Reflection POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${UI_ASSETS_SRC}"
        "$<TARGET_FILE_DIR:Reflection>/assets"
    COMMENT "Copying UI assets to build output"
)

# --------------------------------------------------------------------------
# Copy MSYS2 GStreamer runtime DLLs next to the executable.
# MSVC links against MSYS2 .dll.a import libraries at build time, but
# at runtime the actual .dll files must be discoverable. The industry
# standard approach is to co-locate them with the executable.
# --------------------------------------------------------------------------
if(USE_UXPLAY AND GST_LIBRARY_DIRS MATCHES "msys64")
    # Derive the MSYS2 bin directory from the lib directory
    string(REPLACE "/lib" "/bin" _MSYS2_BIN_DIR "${GST_LIBRARY_DIRS}")
    # Also handle backslash paths
    string(REPLACE "\\lib" "\\bin" _MSYS2_BIN_DIR "${_MSYS2_BIN_DIR}")

    # Complete dependency tree for our GStreamer usage (recursive objdump scan
    # PLUS runtime-loaded deps via LoadLibrary).
    # ALL MSYS2 DLLs must be co-located with the exe to prevent PATH pollution
    # from Git for Windows (C:\Program Files\Git\mingw64\bin) which ships
    # incompatible mingw64 versions of the same DLLs.
    #
    # Import-table deps: gstreamer, gstapp, gstvideo, gstbase, glib, gobject,
    #   gmodule, intl, iconv, winpthread, gcc_s_seh, pcre2, ffi, orc
    # Runtime-loaded (via libintl gettext IDN support):
    #   libidn2-0, libunistring-5
    set(_MSYS2_DLLS
        libgstreamer-1.0-0.dll
        libgstapp-1.0-0.dll
        libgstvideo-1.0-0.dll
        libgstbase-1.0-0.dll
        libglib-2.0-0.dll
        libgobject-2.0-0.dll
        libgmodule-2.0-0.dll
        libintl-8.dll
        libiconv-2.dll
        libidn2-0.dll
        libunistring-5.dll
        libwinpthread-1.dll
        libgcc_s_seh-1.dll
        libpcre2-8-0.dll
        libffi-8.dll
        liborc-0.4-0.dll
    )

    foreach(_dll ${_MSYS2_DLLS})
        set(_dll_path "${_MSYS2_BIN_DIR}/${_dll}")
        if(EXISTS "${_dll_path}")
            add_custom_command(TARGET Reflection POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${_dll_path}"
                    "$<TARGET_FILE_DIR:Reflection>/${_dll}"
            )
        else()
            message(WARNING "MSYS2 DLL not found: ${_dll_path}")
        endif()
    endforeach()

    message(STATUS "MSYS2 GStreamer DLLs will be copied to build output (${_MSYS2_BIN_DIR})")

    # ------------------------------------------------------------------
    # GStreamer PLUGIN DLLs -- loaded dynamically by gst_init().
    # These provide the actual codec/element implementations (h264parse,
    # d3d11h264dec, etc.). Without them the app cannot decode or render.
    # Plugins go in a plugins/ subdirectory next to the executable so
    # configure_gstreamer_environment() can set GST_PLUGIN_PATH to it.
    # ------------------------------------------------------------------
    # Derive plugin directory from the library directory (not bin — avoids double-replace)
    string(REGEX REPLACE "[/\\\\]bin[/\\\\]?\\.\\.[/\\\\]bin$" "" _MSYS2_PREFIX "${_MSYS2_BIN_DIR}")
    string(REGEX REPLACE "[/\\\\]bin$" "" _MSYS2_PREFIX "${_MSYS2_PREFIX}")
    set(_GST_PLUGIN_SRC_DIR "${_MSYS2_PREFIX}/lib/gstreamer-1.0")

    set(_GST_PLUGINS
        libgstcoreelements.dll      # queue, tee, fakesink (GStreamer core)
        libgstapp.dll               # appsrc
        libgstvideoparsersbad.dll   # h264parse
        libgstd3d11.dll             # d3d11h264dec, d3d11videosink
        libgstlibav.dll             # avdec_h264, avdec_aac (FFmpeg decoders)
        libgstvideoconvertscale.dll # videoconvert, videoscale
        libgstaudioconvert.dll      # audioconvert
        libgstaudioresample.dll     # audioresample
        libgstaudioparsers.dll      # aacparse
        libgstwasapi.dll            # wasapisink (Windows audio output)
        libgstautodetect.dll        # autoaudiosink fallback
    )

    # Create plugins/ directory
    add_custom_command(TARGET Reflection POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:Reflection>/plugins"
        COMMENT "Creating GStreamer plugins directory"
    )

    foreach(_plugin ${_GST_PLUGINS})
        set(_plugin_path "${_GST_PLUGIN_SRC_DIR}/${_plugin}")
        if(EXISTS "${_plugin_path}")
            add_custom_command(TARGET Reflection POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${_plugin_path}"
                    "$<TARGET_FILE_DIR:Reflection>/plugins/${_plugin}"
            )
        else()
            message(WARNING "GStreamer plugin not found: ${_plugin_path}")
        endif()
    endforeach()

    message(STATUS "GStreamer plugins will be copied from: ${_GST_PLUGIN_SRC_DIR}")
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
