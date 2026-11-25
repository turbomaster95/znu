#include <znet/znet.h>
#include <znet/znet_platform.h>
#include <page.h>
#include <generated/nifties.h>
#include <sync.h>
#include <kernel/logging.h>
#include <string.h>
#include <e1000.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>

/* ---- platform: allocation (kmalloc is IRQ-safe in Znu) ---- */
void *znet_alloc(size_t size)          { return kmalloc((uint64_t)size); }
void  znet_free(void *ptr)             { kfree(ptr); }

/* ---- platform: time (ms) ---- */
uint64_t znet_uptime_ms(void)          { return (nifties * 1000ULL) / CONFIG_HZ; }

/* ---- platform: lock (spinlock, IRQ-safe) ---- */
static spinlock_t znet_core_lock = SPINLOCK_INIT;
struct znet_lock_real { spinlock_t sl; };

void znet_lock_init(znet_lock_t *lock)
{
    /* storage is a fixed 64-byte region; hold the real spinlock there */
    spinlock_t *sl = (spinlock_t *)(void *)lock->opaque_storage;
    *sl = znet_core_lock;
}
void znet_lock_acquire(znet_lock_t *lock)
{
    spinlock_t *sl = (spinlock_t *)(void *)lock->opaque_storage;
    spinlock_acquire(sl);
}
void znet_lock_release(znet_lock_t *lock)
{
    spinlock_t *sl = (spinlock_t *)(void *)lock->opaque_storage;
    spinlock_release(sl);
}

/* ---- platform: logging (vsnprintf then the kernel log macros) ---- */
#include <stdarg.h>
#include <stdio.h>
static void znet_vlog(int lvl, const char *fmt, va_list ap)
{
    char buf[192];
    vsnprintf(buf, sizeof(buf), fmt, ap);
    switch (lvl) {
    case 0: p_trace("znet", "%s", buf); break;
    case 1: p_info("znet", "%s", buf);  break;
    case 2: p_warn("znet", "%s", buf);  break;
    default: p_warn("znet", "%s", buf); break;
    }
}
void znet_log_trace(const char *fmt, ...) { va_list a; va_start(a, fmt); znet_vlog(0, fmt, a); va_end(a); }
void znet_log_info(const char *fmt, ...)  { va_list a; va_start(a, fmt); znet_vlog(1, fmt, a); va_end(a); }
void znet_log_warn(const char *fmt, ...)  { va_list a; va_start(a, fmt); znet_vlog(2, fmt, a); va_end(a); }
void znet_log_error(const char *fmt, ...) { va_list a; va_start(a, fmt); znet_vlog(3, fmt, a); va_end(a); }

/* ---- the one network interface (e1000 netdev for now) ---- */
static int znet_e1000_transmit(znet_if_t *nif, const uint8_t *frame, size_t len)
{
    (void)nif;
    /* e1000_send_packet() returns the byte count sent (>0) on success,
     * -1 on failure - NOT 0-on-success like znet expects. */
    return e1000_send_packet((void *)(uintptr_t)frame, len) < 0 ? -1 : 0;
}

static znet_if_t s_e1000_if = {
    .name     = "eth0",
    .mtu      = 1500,
    .transmit = znet_e1000_transmit,
};

static uint8_t s_rx_frame[ZNET_MAX_PACKET];

void net_poll(void)
{
    int got;

    while ((got = e1000_receive_packet(s_rx_frame, sizeof(s_rx_frame))) > 0) {
        znet_pbuf_t *p = znet_pbuf_alloc((size_t)got);
        if (p == NULL) {
            /* allocation failure: drop this frame, keep polling */
            continue;
        }
        if (znet_pbuf_append(p, s_rx_frame, (size_t)got) != 0) {
            znet_pbuf_free(p);
            continue;
        }
        znet_input(&s_e1000_if, p);   /* stack takes ownership of p     */
    }
}

void net_tick(void)
{
    znet_poll_timers();
}

void net_init(void)
{
    znet_init();

    e1000_get_mac_address(s_e1000_if.mac.octets);

    /* Defaults; these should come from configuration/DHCP later. */
    znet_ipv4_from_str(&s_e1000_if.addr,     "10.0.2.15");
    znet_ipv4_from_str(&s_e1000_if.netmask,  "255.255.255.0");
    znet_ipv4_from_str(&s_e1000_if.gateway,  "10.0.2.2");

    if (znet_if_register(&s_e1000_if) != 0) {
        p_warn("znet", "failed to register eth0");
        return;
    }
    znet_if_up(&s_e1000_if);
    {
        char macs[24];
        znet_mac_to_str(macs, sizeof(macs), &s_e1000_if.mac);
        p_info("znet", "eth0 up: MAC %s", macs);
    }
}
