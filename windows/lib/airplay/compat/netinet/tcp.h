/*
 * Shim for <netinet/tcp.h> (POSIX) on Windows.
 *
 * raop_rtp_mirror.c includes this directly for TCP_NODELAY.
 * On Windows, TCP_NODELAY is provided by <winsock2.h> which is
 * already included via win_compat_pre.h (force-included).
 *
 * This file exists solely to satisfy the #include directive.
 */

#ifndef SHIM_NETINET_TCP_H
#define SHIM_NETINET_TCP_H

/* TCP_NODELAY and other TCP socket options are in <winsock2.h>,
 * already included via win_compat_pre.h. Nothing else needed. */

#endif /* SHIM_NETINET_TCP_H */
