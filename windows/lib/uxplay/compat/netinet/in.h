/*
 * Shim for <netinet/in.h> (POSIX) on Windows.
 *
 * byteutils.c includes this for htonl/htons/ntohl/ntohs and struct in_addr.
 * On Windows, these are provided by <winsock2.h> which is already
 * included via win_compat_pre.h (force-included).
 *
 * This file exists solely to satisfy the #include directive.
 */

#ifndef SHIM_NETINET_IN_H
#define SHIM_NETINET_IN_H

/* Network byte order functions (htonl, htons, ntohl, ntohs) and
 * struct in_addr are in <winsock2.h>, already included via
 * win_compat_pre.h. Nothing else needed. */

#endif /* SHIM_NETINET_IN_H */
