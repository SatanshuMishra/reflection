# AirPlay core library — builds RPiPlay's lib/ as a static library.
#
# Downloads RPiPlay via FetchContent, compiles all C sources from lib/
# EXCEPT dnssd.c (we handle mDNS ourselves via mjansson/mdns.h).
# A stub dnssd implementation is provided to satisfy raop.c's header dependency.
#
# Windows compat layer (compat/ directory) provides:
#   - POSIX header shims (netinet/tcp.h, netinet/in.h, endian.h, etc.)
#   - Force-included win_compat_pre.h (Winsock2, cond vars, snprintf fix)

include(FetchContent)

FetchContent_Declare(
    rpiplay
    GIT_REPOSITORY https://github.com/FD-/RPiPlay.git
    GIT_TAG        64d0341ed3bef098c940c9ed0675948870a271f9
    GIT_SHALLOW    TRUE
)

# Download RPiPlay source ONLY — do NOT call add_subdirectory on it.
# RPiPlay's CMakeLists.txt has cmake_minimum_required(VERSION 2.8) which
# modern CMake (3.19+) rejects. We don't need its build system anyway —
# we compile the lib/ sources ourselves below.
FetchContent_GetProperties(rpiplay)
if(NOT rpiplay_POPULATED)
    FetchContent_Populate(rpiplay)
endif()

# --------------------------------------------------------------------------
# Windows compatibility layer
# --------------------------------------------------------------------------
set(COMPAT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/compat")

# --------------------------------------------------------------------------
# RPiPlay lib/ sources (excluding dnssd.c — we handle mDNS ourselves)
# --------------------------------------------------------------------------
set(RPIPLAY_LIB_DIR "${rpiplay_SOURCE_DIR}/lib")

set(RPIPLAY_SOURCES
    ${RPIPLAY_LIB_DIR}/raop.c
    ${RPIPLAY_LIB_DIR}/raop_rtp.c
    ${RPIPLAY_LIB_DIR}/raop_rtp_mirror.c
    ${RPIPLAY_LIB_DIR}/raop_ntp.c
    ${RPIPLAY_LIB_DIR}/raop_buffer.c
    ${RPIPLAY_LIB_DIR}/mirror_buffer.c
    ${RPIPLAY_LIB_DIR}/httpd.c
    ${RPIPLAY_LIB_DIR}/http_request.c
    ${RPIPLAY_LIB_DIR}/http_response.c
    ${RPIPLAY_LIB_DIR}/crypto.c
    ${RPIPLAY_LIB_DIR}/fairplay_playfair.c
    ${RPIPLAY_LIB_DIR}/pairing.c
    ${RPIPLAY_LIB_DIR}/netutils.c
    ${RPIPLAY_LIB_DIR}/logger.c
    ${RPIPLAY_LIB_DIR}/byteutils.c
    ${RPIPLAY_LIB_DIR}/utils.c

    # llhttp (embedded HTTP parser)
    ${RPIPLAY_LIB_DIR}/llhttp/api.c
    ${RPIPLAY_LIB_DIR}/llhttp/http.c
    ${RPIPLAY_LIB_DIR}/llhttp/llhttp.c

    # FairPlay (PlayFair SAP crypto)
    ${RPIPLAY_LIB_DIR}/playfair/hand_garble.c
    ${RPIPLAY_LIB_DIR}/playfair/modified_md5.c
    ${RPIPLAY_LIB_DIR}/playfair/omg_hax.c
    ${RPIPLAY_LIB_DIR}/playfair/playfair.c
    ${RPIPLAY_LIB_DIR}/playfair/sap_hash.c

    # Stub dnssd — satisfies header dependency without Bonjour SDK
    ${CMAKE_CURRENT_SOURCE_DIR}/dnssd_stub.c
)

add_library(airplay_core STATIC ${RPIPLAY_SOURCES})

# Include paths:
#   1. compat/ — BEFORE RPiPlay so POSIX header shims are found first
#      (e.g., compat/netinet/tcp.h overrides the missing system header)
#   2. RPiPlay lib/ — public, for raop.h, stream.h, etc.
#   3. RPiPlay lib/llhttp and lib/playfair — private sub-libraries
#   4. libplist — for plist/plist.h used by raop_handlers.h
target_include_directories(airplay_core
    PUBLIC  ${RPIPLAY_LIB_DIR}
    PRIVATE ${COMPAT_DIR}
    PRIVATE ${RPIPLAY_LIB_DIR}/llhttp
    PRIVATE ${RPIPLAY_LIB_DIR}/playfair
)

# libplist include directory (found by root CMakeLists.txt)
if(PLIST_INCLUDE_DIR)
    target_include_directories(airplay_core PRIVATE ${PLIST_INCLUDE_DIR})
endif()

target_link_libraries(airplay_core
    PRIVATE
        OpenSSL::SSL
        OpenSSL::Crypto
        ws2_32
        winmm       # timeGetTime() used by RPiPlay's memalign.h
)

# Link libplist (found by root CMakeLists.txt)
if(PLIST_LIBRARY)
    target_link_libraries(airplay_core PRIVATE ${PLIST_LIBRARY})
endif()

# RPiPlay uses C99; suppress MSVC warnings for POSIX functions
target_compile_definitions(airplay_core PRIVATE
    WIN32
    WIN32_LEAN_AND_MEAN
    _CRT_SECURE_NO_WARNINGS
    _CRT_NONSTDC_NO_DEPRECATE
    _WINSOCK_DEPRECATED_NO_WARNINGS
)

if(MSVC)
    # Force-include Windows compatibility header before all RPiPlay sources.
    # This sets up Winsock2, condition variables, and snprintf — must come
    # before RPiPlay's own compat.h to prevent conflicts.
    target_compile_options(airplay_core PRIVATE
        /FI${COMPAT_DIR}/win_compat_pre.h
        /W3
        /wd4005   # macro redefinition (snprintf identity macro)
        /wd4244   # conversion, possible loss of data
        /wd4267   # size_t to int conversion
        /wd4996   # deprecated POSIX names
        /wd4133   # incompatible pointer types in third-party code
        /wd4013   # undefined function assumed extern (COND macros edge case)
        /wd4047   # differs in levels of indirection
        /wd4024   # different types for formal and actual parameter
        /wd4101   # unreferenced local variable
    )
else()
    target_compile_options(airplay_core PRIVATE -w)
endif()
