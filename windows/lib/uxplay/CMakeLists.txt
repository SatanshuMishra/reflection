# UxPlay core library — builds UxPlay's lib/ as a static library.
#
# Downloads UxPlay via FetchContent, compiles all C sources from lib/
# EXCEPT dnssd.c (we handle mDNS ourselves via mjansson/mdns.h).
# A stub dnssd implementation is provided to satisfy raop.c's header dependency.
#
# Windows compat layer (compat/ directory) provides:
#   - POSIX header shims (netinet/tcp.h, netinet/in.h, endian.h, etc.)
#   - Force-included win_compat_pre.h (Winsock2, cond vars, snprintf fix)

include(FetchContent)

FetchContent_Declare(
    uxplay
    GIT_REPOSITORY https://github.com/FDH2/UxPlay.git
    GIT_TAG        v1.73.4
    GIT_SHALLOW    TRUE
)

# Download UxPlay source ONLY — do NOT call add_subdirectory on it.
# UxPlay's CMakeLists.txt has its own build system with GStreamer dependencies
# that we don't want. We compile lib/ sources ourselves.
FetchContent_GetProperties(uxplay)
if(NOT uxplay_POPULATED)
    FetchContent_Populate(uxplay)

    # ---- Post-download patches for MSVC compatibility ----

    # MSVC does not support C99 Variable Length Arrays (VLAs).
    # Patch all VLAs in lib/ to use _alloca() (stack allocation, no free needed).
    set(_uxplay_lib_dir "${uxplay_SOURCE_DIR}/lib")

    # Patch raop_rtp_mirror.c — VLA for sps_pps buffer
    set(_mirror_file "${_uxplay_lib_dir}/raop_rtp_mirror.c")
    if(EXISTS "${_mirror_file}")
        file(READ "${_mirror_file}" _mirror_content)
        string(REPLACE
            "unsigned char sps_pps[sps_pps_len]"
            "unsigned char *sps_pps = (unsigned char *)_alloca(sps_pps_len)"
            _mirror_content "${_mirror_content}"
        )
        file(WRITE "${_mirror_file}" "${_mirror_content}")
        message(STATUS "Patched raop_rtp_mirror.c: VLA → _alloca() for MSVC")
    endif()

    # Patch mirror_buffer.c — VLA for encrypted_data
    set(_mbuf_file "${_uxplay_lib_dir}/mirror_buffer.c")
    if(EXISTS "${_mbuf_file}")
        file(READ "${_mbuf_file}" _mbuf_content)
        # Common VLA patterns in mirror_buffer.c
        string(REGEX REPLACE
            "unsigned char ([a-zA-Z_]+)\\[([a-zA-Z_]+)\\];"
            "unsigned char *\\1 = (unsigned char *)_alloca(\\2);"
            _mbuf_content "${_mbuf_content}"
        )
        file(WRITE "${_mbuf_file}" "${_mbuf_content}")
        message(STATUS "Patched mirror_buffer.c: VLA → _alloca() for MSVC")
    endif()

    # Patch raop_buffer.c — VLA for decrypted_data
    set(_rbuf_file "${_uxplay_lib_dir}/raop_buffer.c")
    if(EXISTS "${_rbuf_file}")
        file(READ "${_rbuf_file}" _rbuf_content)
        string(REGEX REPLACE
            "unsigned char ([a-zA-Z_]+)\\[([a-zA-Z_]+)\\];"
            "unsigned char *\\1 = (unsigned char *)_alloca(\\2);"
            _rbuf_content "${_rbuf_content}"
        )
        file(WRITE "${_rbuf_file}" "${_rbuf_content}")
        message(STATUS "Patched raop_buffer.c: VLA → _alloca() for MSVC")
    endif()

    # Patch compat.h — add <pthread.h> to the Windows path.
    # UxPlay's compat.h includes <pthread.h> only on Unix (#else branch).
    # On Windows, threads.h needs pthread_t, pthread_mutex_t, pthread_cond_t
    # which are provided by MinGW's winpthreads but not auto-included.
    set(_compat_file "${_uxplay_lib_dir}/compat.h")
    if(EXISTS "${_compat_file}")
        file(READ "${_compat_file}" _compat_content)
        string(REPLACE
            "#include <windows.h>"
            "#include <windows.h>\n#include <pthread.h>"
            _compat_content "${_compat_content}"
        )
        file(WRITE "${_compat_file}" "${_compat_content}")
        message(STATUS "Patched compat.h: added <pthread.h> to Windows path")
    endif()
endif()

# --------------------------------------------------------------------------
# Windows compatibility layer (shared with RPiPlay — same compat/ dir)
# --------------------------------------------------------------------------
set(COMPAT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../airplay/compat")

# --------------------------------------------------------------------------
# UxPlay lib/ sources (excluding dnssd.c — we handle mDNS ourselves)
# --------------------------------------------------------------------------
set(UXPLAY_LIB_DIR "${uxplay_SOURCE_DIR}/lib")

set(UXPLAY_SOURCES
    ${UXPLAY_LIB_DIR}/raop.c
    ${UXPLAY_LIB_DIR}/raop_rtp.c
    ${UXPLAY_LIB_DIR}/raop_rtp_mirror.c
    ${UXPLAY_LIB_DIR}/raop_ntp.c
    ${UXPLAY_LIB_DIR}/raop_buffer.c
    ${UXPLAY_LIB_DIR}/mirror_buffer.c
    ${UXPLAY_LIB_DIR}/httpd.c
    ${UXPLAY_LIB_DIR}/http_request.c
    ${UXPLAY_LIB_DIR}/http_response.c
    ${UXPLAY_LIB_DIR}/crypto.c
    ${UXPLAY_LIB_DIR}/fairplay_playfair.c
    ${UXPLAY_LIB_DIR}/pairing.c
    ${UXPLAY_LIB_DIR}/netutils.c
    ${UXPLAY_LIB_DIR}/logger.c
    ${UXPLAY_LIB_DIR}/byteutils.c
    ${UXPLAY_LIB_DIR}/utils.c
    ${UXPLAY_LIB_DIR}/compat.c
    ${UXPLAY_LIB_DIR}/airplay_video.c
    ${UXPLAY_LIB_DIR}/srp.c

    # llhttp (embedded HTTP parser)
    ${UXPLAY_LIB_DIR}/llhttp/api.c
    ${UXPLAY_LIB_DIR}/llhttp/http.c
    ${UXPLAY_LIB_DIR}/llhttp/llhttp.c

    # FairPlay (PlayFair SAP crypto)
    ${UXPLAY_LIB_DIR}/playfair/hand_garble.c
    ${UXPLAY_LIB_DIR}/playfair/modified_md5.c
    ${UXPLAY_LIB_DIR}/playfair/omg_hax.c
    ${UXPLAY_LIB_DIR}/playfair/playfair.c
    ${UXPLAY_LIB_DIR}/playfair/sap_hash.c

    # Stub dnssd — satisfies header dependency without Bonjour SDK
    ${CMAKE_CURRENT_SOURCE_DIR}/dnssd_stub.c
)

add_library(uxplay_core STATIC ${UXPLAY_SOURCES})

# Include paths:
#   1. compat/ — BEFORE UxPlay so POSIX header shims are found first
#   2. UxPlay lib/ — public, for raop.h, stream.h, etc.
#   3. UxPlay lib/llhttp and lib/playfair — private sub-libraries
#   4. libplist — for plist/plist.h used by raop_handlers.h
target_include_directories(uxplay_core
    PUBLIC  ${UXPLAY_LIB_DIR}
    PRIVATE ${COMPAT_DIR}
    PRIVATE ${UXPLAY_LIB_DIR}/llhttp
    PRIVATE ${UXPLAY_LIB_DIR}/playfair
)

# libplist include directory (found by root CMakeLists.txt)
if(PLIST_INCLUDE_DIR)
    target_include_directories(uxplay_core PRIVATE ${PLIST_INCLUDE_DIR})
endif()

# pthreads — UxPlay uses pthreads exclusively (no Windows threads path).
# On MSVC: requires pthreads4w (vcpkg install pthreads:x64-windows)
# On MinGW: winpthreads provides POSIX pthreads compatibility.
if(MSVC)
    # pthreads4w for MSVC — provides <pthread.h> and pthreadVC3.lib
    find_path(PTHREADS_INCLUDE_DIR NAMES pthread.h)
    find_library(PTHREADS_LIBRARY NAMES pthreadVC3 pthreadVC2 pthreadVCE3 pthreads)
    if(NOT PTHREADS_INCLUDE_DIR OR NOT PTHREADS_LIBRARY)
        message(FATAL_ERROR
            "pthreads not found for MSVC. Install via:\n"
            "  vcpkg install pthreads:x64-windows\n"
            "UxPlay requires POSIX pthreads for threading."
        )
    endif()
    message(STATUS "Found pthreads4w: ${PTHREADS_LIBRARY}")
    target_include_directories(uxplay_core PRIVATE ${PTHREADS_INCLUDE_DIR})
    target_link_libraries(uxplay_core PRIVATE ${PTHREADS_LIBRARY})
else()
    find_package(Threads REQUIRED)
    target_link_libraries(uxplay_core PRIVATE Threads::Threads)
endif()

target_link_libraries(uxplay_core
    PRIVATE
        OpenSSL::SSL
        OpenSSL::Crypto
        ws2_32
        winmm       # timeGetTime()
)

# Link libplist (found by root CMakeLists.txt)
if(PLIST_LIBRARY)
    target_link_libraries(uxplay_core PRIVATE ${PLIST_LIBRARY})
endif()

# Detect libplist version for API compatibility.
# libplist 2.1.0+ changed plist_get_data_val() signature.
# UxPlay uses #ifdef PLIST_210 to handle this.
if(PkgConfig_FOUND)
    pkg_check_modules(PLIST_VER QUIET libplist-2.0>=2.1.0)
    if(PLIST_VER_FOUND)
        message(STATUS "libplist >= 2.1.0 detected — defining PLIST_210")
        set(PLIST_210_DEF "PLIST_210")
    else()
        set(PLIST_210_DEF "")
    endif()
    # Also check for >= 2.3.0 (further API changes)
    pkg_check_modules(PLIST_230 QUIET libplist-2.0>=2.3.0)
    if(PLIST_230_FOUND)
        message(STATUS "libplist >= 2.3.0 detected — defining PLIST_230")
        set(PLIST_230_DEF "PLIST_230")
    else()
        set(PLIST_230_DEF "")
    endif()
else()
    # Assume modern libplist if pkg-config unavailable
    set(PLIST_210_DEF "PLIST_210")
    set(PLIST_230_DEF "")
endif()

# Compile definitions
target_compile_definitions(uxplay_core PRIVATE
    WIN32
    WIN32_LEAN_AND_MEAN
    _CRT_SECURE_NO_WARNINGS
    _CRT_NONSTDC_NO_DEPRECATE
    _WINSOCK_DEPRECATED_NO_WARNINGS
    # UxPlay-specific defines
    STANDALONE
    ${PLIST_210_DEF}
    ${PLIST_230_DEF}
)

if(MSVC)
    # Force-include pthread.h (from pthreads4w) before all UxPlay sources.
    # UxPlay's threads.h uses pthread_t, pthread_mutex_t, pthread_cond_t
    # which must be available before any UxPlay header is processed.
    target_compile_options(uxplay_core PRIVATE
        /FI"pthread.h"
        /FI${COMPAT_DIR}/win_compat_pre.h
        /W3
        /wd4005   # macro redefinition
        /wd4244   # conversion, possible loss of data
        /wd4267   # size_t to int conversion
        /wd4996   # deprecated POSIX names
        /wd4133   # incompatible pointer types
        /wd4013   # undefined function assumed extern
        /wd4047   # differs in levels of indirection
        /wd4024   # different types for formal and actual parameter
        /wd4018   # signed/unsigned mismatch
        /wd4101   # unreferenced local variable
        /wd4305   # truncation from int to unsigned char
        /wd4090   # different 'const' qualifiers
        /wd4146   # unary minus applied to unsigned type
    )
else()
    # MinGW/GCC path
    target_compile_options(uxplay_core PRIVATE
        -w               # Suppress all warnings for third-party code
        -pthread          # Ensure pthread headers and linking are available
    )

    # Check if <pthread.h> is available. GCC 15+ on UCRT64 may use MCF
    # threading by default, which doesn't include POSIX pthread headers.
    # In that case, the user needs: pacman -S mingw-w64-ucrt-x86_64-winpthreads-git
    include(CheckIncludeFile)
    check_include_file("pthread.h" HAVE_PTHREAD_H)
    if(NOT HAVE_PTHREAD_H)
        message(FATAL_ERROR
            "<pthread.h> not found! UxPlay requires POSIX pthreads.\n"
            "Your MinGW GCC may use MCF threading without pthread headers.\n"
            "Fix: pacman -S mingw-w64-ucrt-x86_64-winpthreads-git\n"
            "Then delete build dir and re-run cmake."
        )
    endif()
endif()
