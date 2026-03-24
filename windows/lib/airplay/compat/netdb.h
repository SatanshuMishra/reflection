/*
 * Shim for <netdb.h> (POSIX) on Windows.
 *
 * getaddrinfo, getnameinfo, etc. are in <ws2tcpip.h>,
 * already included via win_compat_pre.h.
 */

#ifndef SHIM_NETDB_H
#define SHIM_NETDB_H

/* All name resolution functions are in <ws2tcpip.h>, already included. */

#endif /* SHIM_NETDB_H */
