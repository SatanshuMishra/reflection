/*
 * Shim for <endian.h> (Linux-specific) on Windows.
 *
 * byteutils.c uses htobe64/be64toh from <endian.h> to implement
 * htonll/ntohll. On Windows, we provide these via _byteswap_uint64.
 */

#ifndef SHIM_ENDIAN_H
#define SHIM_ENDIAN_H

#include <stdlib.h>  /* _byteswap_uint64 */

/* Windows is always little-endian (x86, x64, ARM64 in LE mode) */
#ifndef htobe64
#define htobe64(x) _byteswap_uint64(x)
#endif

#ifndef be64toh
#define be64toh(x) _byteswap_uint64(x)
#endif

#ifndef htobe32
#define htobe32(x) _byteswap_ulong(x)
#endif

#ifndef be32toh
#define be32toh(x) _byteswap_ulong(x)
#endif

#ifndef htobe16
#define htobe16(x) _byteswap_ushort(x)
#endif

#ifndef be16toh
#define be16toh(x) _byteswap_ushort(x)
#endif

#endif /* SHIM_ENDIAN_H */
