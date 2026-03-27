# Unit tests for Reflection Windows

include(GoogleTest)

# --------------------------------------------------------------------------
# Test sources
# --------------------------------------------------------------------------
set(TEST_SOURCES
    # Milestone 1 tests
    capture/CaptureStateTests.cpp
    capture/FrameStaleMonitorTests.cpp
    session/MirrorSessionManagerTests.cpp
    session/DeviceModelTests.cpp
    settings/AppSettingsTests.cpp
    utilities/UpdateCheckerTests.cpp

    # Milestone 2 tests — mock-based
    mdns/MdnsServiceRecordTests.cpp
    mdns/MdnsAdvertiserTests.cpp
    airplay/AirPlayServiceTests.cpp
    airplay/AirPlayCallbackBridgeTests.cpp
    airplay/AirPlayTypesTests.cpp

    # Milestone 2 tests — native implementations
    mdns/NativeMdnsAdvertiserTests.cpp
)

# RPiPlayCore tests only when using RPiPlay backend
if(NOT USE_UXPLAY)
    list(APPEND TEST_SOURCES airplay/RPiPlayCoreTests.cpp)
endif()

add_executable(ReflectionTests ${TEST_SOURCES})

target_include_directories(ReflectionTests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}           # For mock headers
    ${CMAKE_SOURCE_DIR}/src               # For production headers
)

# UxPlay path needs GStreamer includes for GStreamerPipeline.h
# Filter out bare MSYS2 system include dirs that confuse MSVC.
if(USE_UXPLAY)
    set(_TEST_GST_INCLUDES
        ${GST_INCLUDE_DIRS}
        ${GST_APP_INCLUDE_DIRS}
        ${GST_VIDEO_INCLUDE_DIRS}
    )
    list(REMOVE_DUPLICATES _TEST_GST_INCLUDES)

    set(_TEST_GST_FILTERED "")
    foreach(_inc ${_TEST_GST_INCLUDES})
        if(_inc MATCHES "gstreamer" OR _inc MATCHES "glib")
            list(APPEND _TEST_GST_FILTERED "${_inc}")
        endif()
    endforeach()

    if(_TEST_GST_FILTERED)
        target_include_directories(ReflectionTests PRIVATE ${_TEST_GST_FILTERED})
    else()
        target_include_directories(ReflectionTests PRIVATE ${_TEST_GST_INCLUDES})
    endif()
endif()

# Link production code objects (not the WIN32 exe — link source directly)
# Only include sources that DON'T have heavy Windows/hardware dependencies.
# Tests use mocks for hardware-dependent code.
target_sources(ReflectionTests PRIVATE
    # Milestone 1 sources
    ${CMAKE_SOURCE_DIR}/src/session/FrameStaleMonitor.cpp
    ${CMAKE_SOURCE_DIR}/src/session/MirrorSessionManager.cpp
    ${CMAKE_SOURCE_DIR}/src/capture/AirPlayCapture.cpp
    ${CMAKE_SOURCE_DIR}/src/capture/CaptureState.cpp
    ${CMAKE_SOURCE_DIR}/src/capture/CaptureError.cpp
    ${CMAKE_SOURCE_DIR}/src/settings/AppSettings.cpp
    ${CMAKE_SOURCE_DIR}/src/utilities/Logger.cpp
    ${CMAKE_SOURCE_DIR}/src/utilities/UpdateChecker.cpp

    # Milestone 2 sources — pure C++ logic (no RPiPlay C headers)
    ${CMAKE_SOURCE_DIR}/src/mdns/MdnsServiceRecord.cpp
    ${CMAKE_SOURCE_DIR}/src/mdns/MjanssonMdnsAdvertiser.cpp
    ${CMAKE_SOURCE_DIR}/src/mdns/NativeMdnsAdvertiser.cpp
    ${CMAKE_SOURCE_DIR}/src/airplay/AirPlayService.cpp
    ${CMAKE_SOURCE_DIR}/src/airplay/AirPlayCallbackBridge.cpp
    ${CMAKE_SOURCE_DIR}/src/airplay/AirPlayTypes.cpp

    # AirPlay core wrapper — depends on which backend is selected
    $<$<BOOL:${USE_UXPLAY}>:${CMAKE_SOURCE_DIR}/src/airplay/UxPlayCore.cpp>
    $<$<NOT:$<BOOL:${USE_UXPLAY}>>:${CMAKE_SOURCE_DIR}/src/airplay/RPiPlayCore.cpp>
)

target_link_libraries(ReflectionTests PRIVATE
    GTest::gtest
    GTest::gtest_main

    # AirPlay core library (UxPlay or RPiPlay depending on config)
    ${AIRPLAY_CORE_LIB}
    mdns

    # OpenSSL (needed by airplay_core)
    OpenSSL::SSL
    OpenSSL::Crypto

    # Windows system libraries
    advapi32   # Registry API (AppSettings)
    ws2_32     # Winsock2 (NativeMdnsAdvertiser)
    iphlpapi   # GetAdaptersAddresses (NativeMdnsAdvertiser)
)

# Windows-specific defines (same as main exe)
target_compile_definitions(ReflectionTests PRIVATE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    UNICODE
    _UNICODE
    _CRT_SECURE_NO_WARNINGS
    $<$<BOOL:${USE_UXPLAY}>:USE_UXPLAY=1>
)

gtest_discover_tests(ReflectionTests)
