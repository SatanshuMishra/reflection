/*
 * Shim for <sys/socket.h> (POSIX) on Windows.
 *
 * Socket API (socket, bind, listen, connect, etc.) is provided by
 * <winsock2.h>, already included via win_compat_pre.h.
 *
 * This file exists solely to satisfy any #include directives.
 */

#ifndef SHIM_SYS_SOCKET_H
#define SHIM_SYS_SOCKET_H

/* Winsock2 provides the socket API, already included. */

#endif /* SHIM_SYS_SOCKET_H */
