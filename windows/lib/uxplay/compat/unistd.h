/*
 * Shim for <unistd.h> (POSIX) on Windows.
 *
 * Provides minimal compatibility for POSIX functions used by RPiPlay.
 */

#ifndef SHIM_UNISTD_H
#define SHIM_UNISTD_H

#include <io.h>       /* _read, _write, _close, _dup, _dup2 */
#include <process.h>  /* _getpid */

/* Map POSIX names to MSVC underscored equivalents */
#ifndef close
#define close _close
#endif

#ifndef read
#define read _read
#endif

#ifndef write
#define write _write
#endif

#ifndef dup
#define dup _dup
#endif

#ifndef dup2
#define dup2 _dup2
#endif

#ifndef getpid
#define getpid _getpid
#endif

/* sleep() in seconds — POSIX.  Windows has Sleep() in milliseconds. */
#ifndef sleep
#define sleep(s) Sleep((s) * 1000)
#endif

/* usleep() in microseconds — POSIX.  Approximate with Sleep(). */
#ifndef usleep
#define usleep(us) Sleep((us) / 1000)
#endif

#endif /* SHIM_UNISTD_H */
