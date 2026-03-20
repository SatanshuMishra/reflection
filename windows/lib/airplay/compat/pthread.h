/*
 * Shim for <pthread.h> (POSIX) on Windows.
 *
 * RPiPlay's compat.h includes <pthread.h> in the POSIX branch.
 * With WIN32 defined, compat.h takes the Windows branch and doesn't
 * include this. But in case any other file includes it directly,
 * this shim prevents a missing-header error.
 *
 * RPiPlay uses its own threading macros (THREAD_CREATE, MUTEX_LOCK, etc.)
 * defined in threads.h, not raw pthread calls.
 */

#ifndef SHIM_PTHREAD_H
#define SHIM_PTHREAD_H

/* Threading is handled by RPiPlay's threads.h (Win32 branch)
 * and our COND_* additions in win_compat_pre.h. */

#endif /* SHIM_PTHREAD_H */
