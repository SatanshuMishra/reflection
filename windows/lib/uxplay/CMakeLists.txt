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

    # NOTE: mirror_buffer.c and raop_buffer.c have NO VLAs.
    # The previous regex-based patch was too greedy and corrupted struct members.
    # Only raop_rtp_mirror.c has an actual VLA (patched above with explicit string replace).

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

    # Patch threads.h — THREAD_CREATE sets handle=0 on failure, but with
    # pthreads4w, pthread_t is a struct {void *p; size_t x;}, not int.
    # Replace the macro with a memset-based zero-init.
    set(_threads_file "${_uxplay_lib_dir}/threads.h")
    if(EXISTS "${_threads_file}")
        file(READ "${_threads_file}" _threads_content)
        string(REPLACE
            "if (pthread_create(&(handle), NULL, func, arg)) handle = 0"
            "if (pthread_create(&(handle), NULL, func, arg)) memset(&(handle), 0, sizeof(handle))"
            _threads_content "${_threads_content}")
        file(WRITE "${_threads_file}" "${_threads_content}")
        message(STATUS "Patched threads.h: handle=0 → memset zero-init for pthreads4w")
    endif()

    # Patch httpd.c — add explicit MUTEX_CREATE for run_mutex.
    # On Linux, calloc'd pthread_mutex_t works (= PTHREAD_MUTEX_INITIALIZER).
    # On pthreads4w, pthread_mutex_t is a struct with a pointer that MUST be
    # initialized via pthread_mutex_init(), or it segfaults on first lock.
    set(_httpd_file "${_uxplay_lib_dir}/httpd.c")
    if(EXISTS "${_httpd_file}")
        file(READ "${_httpd_file}" _httpd_content)
        string(REPLACE
            "httpd->running = 0;\n    httpd->joined = 1;"
            "MUTEX_CREATE(httpd->run_mutex);\n    httpd->running = 0;\n    httpd->joined = 1;"
            _httpd_content "${_httpd_content}")
        file(WRITE "${_httpd_file}" "${_httpd_content}")
        message(STATUS "Patched httpd.c: added MUTEX_CREATE for pthreads4w")
    endif()

    # Patch raop.c — add raop_get_pk_str() to expose the generated public key.
    # Our mDNS advertiser needs this to send the correct pk in TXT records.
    # Guard: only apply if not already patched.
    set(_raop_c_file "${_uxplay_lib_dir}/raop.c")
    if(EXISTS "${_raop_c_file}")
        file(READ "${_raop_c_file}" _raop_c_content)
        if(NOT _raop_c_content MATCHES "raop_get_pk_str")
            string(REPLACE
                "void *\nraop_get_callback_cls(raop_t *raop) {\n    assert(raop);\n    return raop->callbacks.cls;\n}"
                "void *\nraop_get_callback_cls(raop_t *raop) {\n    assert(raop);\n    return raop->callbacks.cls;\n}\n\nconst char *\nraop_get_pk_str(raop_t *raop) {\n    assert(raop);\n    return raop->pk_str;\n}"
                _raop_c_content "${_raop_c_content}")
            file(WRITE "${_raop_c_file}" "${_raop_c_content}")
            message(STATUS "Patched raop.c: added raop_get_pk_str()")
        else()
            message(STATUS "raop.c: raop_get_pk_str already present (skipping patch)")
        endif()
    endif()

    # Patch raop.h — declare raop_get_pk_str
    set(_raop_h_file "${_uxplay_lib_dir}/raop.h")
    if(EXISTS "${_raop_h_file}")
        file(READ "${_raop_h_file}" _raop_h_content)
        if(NOT _raop_h_content MATCHES "raop_get_pk_str")
            string(REPLACE
                "RAOP_API void *raop_get_callback_cls(raop_t *raop);"
                "RAOP_API void *raop_get_callback_cls(raop_t *raop);\nRAOP_API const char *raop_get_pk_str(raop_t *raop);"
                _raop_h_content "${_raop_h_content}")
            file(WRITE "${_raop_h_file}" "${_raop_h_content}")
            message(STATUS "Patched raop.h: declared raop_get_pk_str()")
        else()
            message(STATUS "raop.h: raop_get_pk_str already declared (skipping patch)")
        endif()
    endif()

    # Also patch raop.c if it has uninitialized mutexes
    set(_raop_file "${_uxplay_lib_dir}/raop_rtp.c")
    if(EXISTS "${_raop_file}")
        file(READ "${_raop_file}" _raop_rtp_content)
        # Check for calloc + MUTEX_LOCK without MUTEX_CREATE
        if(_raop_rtp_content MATCHES "calloc.*raop_rtp_s" AND NOT _raop_rtp_content MATCHES "MUTEX_CREATE")
            # Find the init function and add MUTEX_CREATE after calloc
            string(REPLACE
                "raop_rtp->running = 0;\n    raop_rtp->joined = 1;"
                "MUTEX_CREATE(raop_rtp->run_mutex);\n    raop_rtp->running = 0;\n    raop_rtp->joined = 1;"
                _raop_rtp_content "${_raop_rtp_content}")
            file(WRITE "${_raop_file}" "${_raop_rtp_content}")
            message(STATUS "Patched raop_rtp.c: added MUTEX_CREATE for pthreads4w")
        endif()
    endif()
endif()

# --------------------------------------------------------------------------
# Windows compatibility layer — UxPlay-specific subset.
# Uses its own compat/ (unistd.h, netinet/) WITHOUT pthread.h shim.
# RPiPlay's compat/pthread.h is an empty shim (RPiPlay uses Win32 threads),
# but UxPlay needs the REAL pthreads4w <pthread.h>.
# --------------------------------------------------------------------------
set(COMPAT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/compat")

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
    # BEFORE ensures pthreads4w's real <pthread.h> is found before the
    # compat/ directory's empty RPiPlay shim that would shadow it.
    target_include_directories(uxplay_core BEFORE PRIVATE ${PTHREADS_INCLUDE_DIR})
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
# libplist 2.3.0+ exports plist_mem_free() (no local static needed).
# UxPlay uses #ifdef PLIST_210 / PLIST_230 to handle this.
#
# vcpkg ships libplist 2.7.0+ but its .pc file isn't always visible
# to pkg-config (especially when mixing vcpkg + MSYS2).
# Check the header directly for the reliable 2.3.0+ indicator.
set(PLIST_210_DEF "")
set(PLIST_230_DEF "")
if(PLIST_INCLUDE_DIR)
    # plist 2.3.0+ declares plist_mem_free as PLIST_API in the header
    file(STRINGS "${PLIST_INCLUDE_DIR}/plist/plist.h" _plist_memfree_lines
         REGEX "PLIST_API.*plist_mem_free")
    if(_plist_memfree_lines)
        message(STATUS "libplist >= 2.3.0 detected (plist_mem_free is PLIST_API) — defining PLIST_210 PLIST_230")
        set(PLIST_210_DEF "PLIST_210")
        set(PLIST_230_DEF "PLIST_230")
    else()
        # Check for 2.1.0+ by looking for plist_get_data_val with uint64_t
        file(STRINGS "${PLIST_INCLUDE_DIR}/plist/plist.h" _plist_data_lines
             REGEX "plist_get_data_val.*uint64_t")
        if(_plist_data_lines)
            message(STATUS "libplist >= 2.1.0 detected — defining PLIST_210")
            set(PLIST_210_DEF "PLIST_210")
        endif()
    endif()
else()
    # Assume modern libplist if include dir unavailable (vcpkg ships 2.7.0+)
    set(PLIST_210_DEF "PLIST_210")
    set(PLIST_230_DEF "PLIST_230")
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
    # Force-include UxPlay-specific compat header (NOT RPiPlay's win_compat_pre.h
    # which defines COND_*/pthread_cond_timedwait using Win32 HANDLEs that
    # conflict with pthreads4w's real pthread implementation).
    target_compile_options(uxplay_core PRIVATE
        /FI"pthread.h"
        /FI${CMAKE_CURRENT_SOURCE_DIR}/uxplay_win_compat.h
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
