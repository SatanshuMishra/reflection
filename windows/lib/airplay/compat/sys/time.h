/*
 * Shim for <sys/time.h> (POSIX) on Windows.
 *
 * Provides struct timeval (from <winsock2.h>) and gettimeofday().
 * <winsock2.h> is already included via win_compat_pre.h.
 */

#ifndef SHIM_SYS_TIME_H
#define SHIM_SYS_TIME_H

/* struct timeval is defined in <winsock2.h>, already included. */

#endif /* SHIM_SYS_TIME_H */
