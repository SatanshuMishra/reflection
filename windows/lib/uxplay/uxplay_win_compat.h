/*
 * uxplay_win_compat.h — Force-included before all UxPlay lib/ sources on MSVC.
 *
 * Provides POSIX functions that UxPlay expects but MSVC doesn't have:
 *   1. Winsock2 before windows.h (prevents struct redefinition)
 *   2. SOL_TCP → IPPROTO_TCP
 *   3. snprintf safety (prevent compat.h's _snprintf macro)
 *   4. clock_gettime() and CLOCK_REALTIME
 *   5. gettimeofday()
 *   6. ioctl() → ioctlsocket()
 *   7. _alloca for VLA replacements
 *
 * Unlike win_compat_pre.h (RPiPlay), this does NOT define:
 *   - cond_handle_t / COND_* macros (UxPlay uses real pthreads4w)
 *   - pthread_cond_timedwait shim (pthreads4w provides it)
 */

#ifndef UXPLAY_WIN_COMPAT_H
#define UXPLAY_WIN_COMPAT_H

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
#include <mstcpip.h>
#include <windows.h>

/* --- SOL_TCP: Linux-specific, maps to IPPROTO_TCP on Windows ------------- */
#ifndef SOL_TCP
#define SOL_TCP IPPROTO_TCP
#endif

/* --- Prevent snprintf redefinition --------------------------------------- */
#include <stdio.h>
#ifndef snprintf
#define snprintf snprintf
#endif

/* --- struct timespec (POSIX) --------------------------------------------- */
#include <time.h>

/* --- CLOCK_REALTIME + clock_gettime() (POSIX) ---------------------------- */
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

    uli.QuadPart -= 116444736000000000ULL;
    tp->tv_sec  = (long)(uli.QuadPart / 10000000ULL);
    tp->tv_nsec = (long)((uli.QuadPart % 10000000ULL) * 100);
    return 0;
}

/* --- gettimeofday() (POSIX) ---------------------------------------------- */
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

/* --- ioctl() → ioctlsocket() (POSIX → Winsock2) ------------------------- */
static __inline int ioctl(int fd, long cmd, int *argp) {
    u_long val = 0;
    int result = ioctlsocket((SOCKET)fd, cmd, &val);
    if (argp) *argp = (int)val;
    return result;
}

/* --- VLA workaround ------------------------------------------------------ */
#include <malloc.h>  /* _alloca */

/* --- timeGetTime --------------------------------------------------------- */
#pragma comment(lib, "winmm.lib")

#endif /* UXPLAY_WIN_COMPAT_H */
