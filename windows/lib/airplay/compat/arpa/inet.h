/*
 * Shim for <arpa/inet.h> (POSIX) on Windows.
 *
 * inet_pton, inet_ntop, inet_addr, inet_ntoa are provided by
 * <ws2tcpip.h> / <winsock2.h>, already included via win_compat_pre.h.
 *
 * This file exists solely to satisfy any #include directives.
 */

#ifndef SHIM_ARPA_INET_H
#define SHIM_ARPA_INET_H

/* All inet_* functions are in <ws2tcpip.h>, already included. */

#endif /* SHIM_ARPA_INET_H */
