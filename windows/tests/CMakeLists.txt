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
    airplay/RPiPlayCoreTests.cpp
)

add_executable(ReflectionTests ${TEST_SOURCES})

target_include_directories(ReflectionTests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}           # For mock headers
    ${CMAKE_SOURCE_DIR}/src               # For production headers
)

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

    # RPiPlayCore — links against airplay_core static lib
    ${CMAKE_SOURCE_DIR}/src/airplay/RPiPlayCore.cpp
)

target_link_libraries(ReflectionTests PRIVATE
    GTest::gtest
    GTest::gtest_main

    # RPiPlay core library (needed by RPiPlayCore.cpp)
    airplay_core
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
)

gtest_discover_tests(ReflectionTests)
