/**
 * Stub DNS-SD implementation for Reflection.
 *
 * UxPlay's raop.c expects a dnssd_t* for service discovery, but we handle
 * mDNS advertisement ourselves (via NativeMdnsAdvertiser + mjansson/mdns.h).
 *
 * This stub satisfies the linker without pulling in Apple's Bonjour SDK.
 * All functions are no-ops — the real mDNS work happens in our C++ layer.
 */

#include "dnssd.h"

#include <stdlib.h>
#include <string.h>

/* Opaque struct — just enough to not crash if someone inspects it */
struct dnssd_s {
    char name[256];
    char hw_addr[16];
    int  hw_addr_len;
    uint64_t features;
    char pk[256];
};

dnssd_t *
dnssd_init(const char *name, int name_len, const char *hw_addr, int hw_addr_len, int *error) {
    dnssd_t *dnssd = (dnssd_t *)calloc(1, sizeof(dnssd_t));
    if (!dnssd) {
        if (error) *error = DNSSD_ERROR_OUTOFMEM;
        return NULL;
    }

    if (name && name_len > 0) {
        int copy_len = name_len < (int)(sizeof(dnssd->name) - 1)
                     ? name_len : (int)(sizeof(dnssd->name) - 1);
        memcpy(dnssd->name, name, copy_len);
        dnssd->name[copy_len] = '\0';
    }
    if (hw_addr && hw_addr_len > 0) {
        int copy_len = hw_addr_len < (int)sizeof(dnssd->hw_addr)
                     ? hw_addr_len : (int)sizeof(dnssd->hw_addr);
        memcpy(dnssd->hw_addr, hw_addr, copy_len);
        dnssd->hw_addr_len = copy_len;
    }

    if (error) *error = DNSSD_ERROR_NOERROR;
    return dnssd;
}

int
dnssd_register_raop(dnssd_t *dnssd, unsigned short port) {
    (void)dnssd;
    (void)port;
    /* No-op: mDNS handled by NativeMdnsAdvertiser */
    return 0;
}

void
dnssd_unregister_raop(dnssd_t *dnssd) {
    (void)dnssd;
}

int
dnssd_register_airplay(dnssd_t *dnssd, unsigned short port) {
    (void)dnssd;
    (void)port;
    /* No-op: mDNS handled by NativeMdnsAdvertiser */
    return 0;
}

void
dnssd_unregister_airplay(dnssd_t *dnssd) {
    (void)dnssd;
}

const char *
dnssd_get_raop_txt(dnssd_t *dnssd, int *length) {
    (void)dnssd;
    if (length) *length = 0;
    return "";
}

const char *
dnssd_get_airplay_txt(dnssd_t *dnssd, int *length) {
    (void)dnssd;
    if (length) *length = 0;
    return "";
}

const char *
dnssd_get_name(dnssd_t *dnssd, int *length) {
    if (!dnssd) {
        if (length) *length = 0;
        return "";
    }
    if (length) *length = (int)strlen(dnssd->name);
    return dnssd->name;
}

const char *
dnssd_get_hw_addr(dnssd_t *dnssd, int *length) {
    if (!dnssd) {
        if (length) *length = 0;
        return "";
    }
    if (length) *length = dnssd->hw_addr_len;
    return dnssd->hw_addr;
}

uint64_t
dnssd_get_airplay_features(dnssd_t *dnssd) {
    if (!dnssd) return 0;
    return dnssd->features;
}

void
dnssd_set_airplay_features(dnssd_t *dnssd, uint64_t features) {
    if (dnssd) dnssd->features = features;
}

void
dnssd_set_pk(dnssd_t *dnssd, const char *pk_str) {
    if (!dnssd || !pk_str) return;
    strncpy(dnssd->pk, pk_str, sizeof(dnssd->pk) - 1);
    dnssd->pk[sizeof(dnssd->pk) - 1] = '\0';
}

void
dnssd_destroy(dnssd_t *dnssd) {
    free(dnssd);
}
