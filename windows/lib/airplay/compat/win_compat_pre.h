/*
 * win_compat_pre.h — Force-included before all RPiPlay source files on Windows.
 *
 * This header solves portability issues between RPiPlay (Linux C99) and MSVC:
 *
 * 1. Ensures WIN32 is defined so RPiPlay's compat.h takes the Windows path.
 * 2. Includes <winsock2.h> before <windows.h> to prevent struct redefinition.
 * 3. Prevents RPiPlay's snprintf→_snprintf redefinition (modern MSVC conflict).
 * 4. Provides cond_handle_t and COND_* macros (missing from threads.h Win32).
 * 5. Defines SOL_TCP as IPPROTO_TCP (Linux constant not on Windows).
 * 6. Provides struct timespec, clock_gettime(), gettimeofday() (POSIX time).
 * 7. Provides pthread_cond_timedwait() used by raop_ntp.c.
 *
 * Applied via: target_compile_options(airplay_core PRIVATE /FIwin_compat_pre.h)
 */

#ifndef WIN_COMPAT_PRE_H
#define WIN_COMPAT_PRE_H

/* --- Platform detection -------------------------------------------------- */
#ifndef WIN32
#define WIN32
#endif

#ifndef _WIN32
#define _WIN32
#endif

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

/* --- Winsock2 must come before windows.h --------------------------------- */
#include <winsock2.h>
#include <ws2tcpip.h>
#include <mstcpip.h>   /* TCP_KEEPIDLE, TCP_KEEPINTVL, TCP_KEEPCNT (Win10 1709+) */
#include <windows.h>

/* --- SOL_TCP: Linux-specific, maps to IPPROTO_TCP on Windows ------------- */
/* RPiPlay's raop_rtp_mirror.c uses setsockopt(fd, SOL_TCP, TCP_KEEPIDLE, ...)
 * On Windows, use IPPROTO_TCP (both have the same underlying value: 6). */
#ifndef SOL_TCP
#define SOL_TCP IPPROTO_TCP
#endif

/* --- Prevent snprintf redefinition --------------------------------------- */
/* RPiPlay's compat.h does: #ifndef snprintf / #define snprintf _snprintf
 * Modern MSVC CRT has proper snprintf; the macro causes #error in <stdio.h>.
 * Including <stdio.h> first and defining an identity macro blocks compat.h. */
#include <stdio.h>
#ifndef snprintf
#define snprintf snprintf
#endif

/* --- struct timespec (POSIX) --------------------------------------------- */
/* raop_ntp.c uses struct timespec for NTP timing and condition waits.
 * MSVC's UCRT <time.h> (VS 2015+) already defines struct timespec as part
 * of C11 support. No custom definition needed — just include <time.h>. */
#include <time.h>

/* --- CLOCK_REALTIME + clock_gettime() (POSIX) ---------------------------- */
/* raop_ntp.c calls clock_gettime(CLOCK_REALTIME, &time) to get microsecond
 * wall-clock time. Windows equivalent: GetSystemTimeAsFileTime(). */
#ifndef CLOCK_REALTIME
#define CLOCK_REALTIME 0
#endif

static __inline int clock_gettime(int clk_id, struct timespec *tp) {
    FILETIME ft;
    ULARGE_INTEGER uli;
    (void)clk_id;

    GetSystemTimeAsFileTime(&ft);
    uli.LowPart = ft.dwLowDateTime;
    uli.HighPart = ft.dwHighDateTime;

    /* FILETIME: 100ns intervals since 1601-01-01 UTC.
     * Unix epoch offset: 116444736000000000 * 100ns = 1970-01-01. */
    uli.QuadPart -= 116444736000000000ULL;
    tp->tv_sec  = (long)(uli.QuadPart / 10000000ULL);
    tp->tv_nsec = (long)((uli.QuadPart % 10000000ULL) * 100);
    return 0;
}

/* --- gettimeofday() (POSIX) ---------------------------------------------- */
/* raop_ntp.c uses gettimeofday() to compute condition variable timeouts.
 * struct timeval is already defined by <winsock2.h>. */
#ifndef _GETTIMEOFDAY_DEFINED
#define _GETTIMEOFDAY_DEFINED

static __inline int gettimeofday(struct timeval *tv, void *tz) {
    FILETIME ft;
    ULARGE_INTEGER uli;
    (void)tz;

    GetSystemTimeAsFileTime(&ft);
    uli.LowPart = ft.dwLowDateTime;
    uli.HighPart = ft.dwHighDateTime;

    uli.QuadPart -= 116444736000000000ULL;
    tv->tv_sec  = (long)(uli.QuadPart / 10000000ULL);
    tv->tv_usec = (long)((uli.QuadPart % 10000000ULL) / 10);
    return 0;
}
#endif

/* --- Condition variable support for Win32 -------------------------------- */
/* RPiPlay's threads.h WIN32 branch provides thread_handle_t, mutex_handle_t,
 * THREAD_CREATE, MUTEX_LOCK, etc. but is MISSING condition variables.
 * raop_ntp.c uses cond_handle_t for NTP synchronization.
 *
 * Implementation: Windows Event objects (auto-reset). Simple but sufficient
 * for RPiPlay's single-waiter NTP pattern. */
#ifndef COND_HANDLE_DEFINED
#define COND_HANDLE_DEFINED

typedef HANDLE cond_handle_t;

#define COND_CREATE(cond)  ((cond) = CreateEvent(NULL, FALSE, FALSE, NULL))
#define COND_SIGNAL(cond)  SetEvent(cond)
#define COND_DESTROY(cond) CloseHandle(cond)

/* COND_WAIT: release mutex, wait on event, re-acquire mutex.
 * Matches the pthread_cond_wait(cond, mutex) contract. */
#define COND_WAIT(cond, mutex) do { \
    ReleaseMutex(mutex); \
    WaitForSingleObject(cond, INFINITE); \
    WaitForSingleObject(mutex, INFINITE); \
} while (0)

#endif /* COND_HANDLE_DEFINED */

/* --- pthread_cond_timedwait() (POSIX) ------------------------------------ */
/* raop_ntp.c calls pthread_cond_timedwait() DIRECTLY (not through a macro).
 * This takes an absolute struct timespec timeout, which we convert to a
 * relative millisecond timeout for WaitForSingleObject().
 *
 * Both cond_handle_t and mutex_handle_t are HANDLE on Win32. */
static __inline int pthread_cond_timedwait(
    cond_handle_t *cond, HANDLE *mutex, const struct timespec *abstime)
{
    struct timespec now;
    long long now_ms, abs_ms;
    DWORD timeout_ms;
    DWORD result;

    clock_gettime(CLOCK_REALTIME, &now);
    now_ms = (long long)now.tv_sec * 1000 + now.tv_nsec / 1000000;
    abs_ms = (long long)abstime->tv_sec * 1000 + abstime->tv_nsec / 1000000;
    timeout_ms = (abs_ms > now_ms) ? (DWORD)(abs_ms - now_ms) : 0;

    ReleaseMutex(*mutex);
    result = WaitForSingleObject(*cond, timeout_ms);
    WaitForSingleObject(*mutex, INFINITE);
    return (result == WAIT_TIMEOUT) ? 1 : 0;
}

/* --- VLA workaround ------------------------------------------------------ */
/* MSVC does not support C99 Variable Length Arrays (VLAs).
 * RPiPlay's raop_rtp_mirror.c uses: unsigned char sps_pps[sps_pps_len]
 * We use _alloca() for stack allocation (no free needed). */
#include <malloc.h>  /* _alloca */

/* --- Provide timeGetTime if memalign.h needs it -------------------------- */
#pragma comment(lib, "winmm.lib")

#endif /* WIN_COMPAT_PRE_H */
