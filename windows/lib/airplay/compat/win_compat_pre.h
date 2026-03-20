/*
 * win_compat_pre.h — Force-included before all RPiPlay source files on Windows.
 *
 * This header solves five portability issues:
 *
 * 1. Ensures WIN32 is defined so RPiPlay's compat.h takes the Windows path
 *    (MSVC predefines _WIN32 but NOT WIN32 — that comes from <windows.h>).
 *
 * 2. Includes <winsock2.h> before <windows.h> to prevent the classic
 *    winsock.h vs winsock2.h struct redefinition conflict.
 *
 * 3. Prevents RPiPlay's compat.h from redefining snprintf to _snprintf,
 *    which conflicts with modern MSVC's CRT (VS 2015+).
 *
 * 4. Provides cond_handle_t and COND_* macros missing from RPiPlay's
 *    threads.h WIN32 branch (used by raop_ntp.c).
 *
 * 5. WIN32_LEAN_AND_MEAN prevents <windows.h> from pulling in winsock.h.
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
#include <windows.h>

/* --- Prevent snprintf redefinition --------------------------------------- */
/* RPiPlay's compat.h does: #ifndef snprintf / #define snprintf _snprintf
 * Modern MSVC CRT has proper snprintf; the macro causes #error in <stdio.h>.
 * Including <stdio.h> first and defining an identity macro blocks compat.h. */
#include <stdio.h>
#ifndef snprintf
#define snprintf snprintf
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

/* COND_TIMEDWAIT: same but with timeout in milliseconds.
 * Returns non-zero if timed out. */
#define COND_TIMEDWAIT(cond, mutex, timeout_ms) ( \
    ReleaseMutex(mutex), \
    WaitForSingleObject(cond, timeout_ms), \
    WaitForSingleObject(mutex, INFINITE) \
)

#endif /* COND_HANDLE_DEFINED */

/* --- Provide timeGetTime if memalign.h needs it -------------------------- */
#pragma comment(lib, "winmm.lib")

#endif /* WIN_COMPAT_PRE_H */
