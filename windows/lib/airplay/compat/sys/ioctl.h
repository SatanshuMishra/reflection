/*
 * Shim for <sys/ioctl.h> (POSIX) on Windows.
 *
 * On Windows, ioctlsocket() from <winsock2.h> replaces ioctl() for sockets.
 * Already included via win_compat_pre.h.
 */

#ifndef SHIM_SYS_IOCTL_H
#define SHIM_SYS_IOCTL_H

/* ioctlsocket is in <winsock2.h>, already included. */

#endif /* SHIM_SYS_IOCTL_H */
