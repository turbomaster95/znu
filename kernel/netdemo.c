/*
 * netdemo.c - Znu network test, on the znet stack.
 *
 * znet has no tcpip thread and no raw-PCB layer: the stack runs under
 * its own core lock and surfaces everything through callbacks. This
 * demo therefore runs as a kernel thread started after net_init():
 *   ICMP  -> znet_icmpv4_ping + the ICMP monitor callback
 *   ARP   -> znet_ipv4_resolve (sends the request on a cache miss)
 *   DNS   -> znet_dns_resolve (async, single in-flight query)
 *   TCP   -> znet_tcp_connect/send + rx/event callbacks
 *
 * Callbacks fire from net_poll() (e1000 RX interrupt) and net_tick()
 * (timer interrupt), so the thread just sleeps between phases while
 * the stack does its thing.
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include <proc.h>
#include <znet/znet.h>

#define NETDEMO_LOG(...) debugln(__VA_ARGS__)

#define NETDEMO_HTTP_HOST  "example.com"
#define NETDEMO_HTTP_PORT  80
#define NETDEMO_LOCAL_PORT 40000   /* znet needs an explicit local port */
#define NETDEMO_PING_ID    0x5A4E
#define NETDEMO_PING_SEQ   1

static znet_tcp_pcb_t *http_pcb;

static void netdemo_tcp_start(const znet_ip_addr_t *addr, znet_if_t *nif);

/* --- ICMP --------------------------------------------------------- */

/*
 * znet surfaces ICMP messages through one monitor callback instead of
 * a raw PCB. Note: it hands us type/code/src only - the echo ident/seq
 * are not exposed yet, so we log every echo reply rather than only
 * ours. (Adding ident/seq to the callback would be a small API change.)
 */
static void netdemo_icmp_rx(znet_if_t *nif, const znet_ip_addr_t *src,
                            uint8_t type, uint8_t code)
{
    char buf[ZNET_MAX_ADDR_STR];

    (void)nif;
    (void)code;
    if (type == ZNET_ICMP_ECHORESP) {
        znet_ip_to_str(buf, sizeof(buf), src);
        NETDEMO_LOG("[NETTEST] ICMP echo reply from %s\n", buf);
    }
}

static void netdemo_ping(znet_if_t *nif)
{
    znet_ipv4_addr_t destination;
    static uint16_t ping_sequence = NETDEMO_PING_SEQ;

    znet_ipv4_from_str(&destination, "10.0.2.2");

    NETDEMO_LOG("[NETTEST] Sending ICMP echo request to 10.0.2.2\n");

    /* znet builds + checksums the header; payload is ours to pass */
    if (znet_icmpv4_ping(nif, &destination, NETDEMO_PING_ID,
                         ping_sequence++, NULL, 0) != 0) {
        NETDEMO_LOG("[NETTEST] ICMP transmission failed\n");
    }
}

/* --- TCP / HTTP ---------------------------------------------------- */

/*
 * znet ACKs received data itself and hands us the payload directly -
 * no pbuf to free, no tcp_recved(). The buffer is only valid inside
 * the callback; copy it if you need it later.
 */
static void netdemo_tcp_rx(znet_tcp_pcb_t *pcb, const uint8_t *data,
                           size_t len)
{
    (void)pcb;
    NETDEMO_LOG("[NETTEST] Received %u HTTP response bytes\n",
                (unsigned int)len);
}

/*
 * Connection lifecycle comes through one event callback (no separate
 * connected/err callbacks). Callbacks run with the stack's lock
 * released, so it is safe to send from inside them.
 */
static void netdemo_tcp_event(znet_tcp_pcb_t *pcb, int event)
{
    if (event == ZNET_TCP_EV_CONNECTED) {
        static const char request[] =
            "GET / HTTP/1.1\r\n"
            "Host: example.com\r\n"
            "Connection: close\r\n"
            "User-Agent: Znu-NetTest/0.1\r\n"
            "\r\n";

        NETDEMO_LOG("[NETTEST] TCP connected! Sending HTTP request\n");
        if (znet_tcp_send(pcb, request, sizeof(request) - 1) < 0)
            NETDEMO_LOG("[NETTEST] znet_tcp_send failed\n");
    } else if (event == ZNET_TCP_EV_CLOSED) {
        NETDEMO_LOG("[NETTEST] HTTP server closed connection\n");
        znet_tcp_close(pcb);      /* finish our side: FIN, LAST_ACK */
        http_pcb = NULL;
    } else if (event == ZNET_TCP_EV_RESET) {
        NETDEMO_LOG("[NETTEST] TCP connection reset\n");
        http_pcb = NULL;          /* pcb is already freed by the stack */
    }
}

static void netdemo_tcp_start(const znet_ip_addr_t *addr, znet_if_t *nif)
{
    if (http_pcb != NULL) {
        NETDEMO_LOG("[NETTEST] HTTP connection already active\n");
        return;
    }

    http_pcb = znet_tcp_new();
    if (http_pcb == NULL) {
        NETDEMO_LOG("[NETTEST] Failed to allocate TCP PCB\n");
        return;
    }

    NETDEMO_LOG("[NETTEST] Connecting to example.com:%d\n",
                NETDEMO_HTTP_PORT);

    if (znet_tcp_connect(http_pcb, nif, addr, NETDEMO_HTTP_PORT,
                         NETDEMO_LOCAL_PORT,
                         netdemo_tcp_rx, netdemo_tcp_event) != 0) {
        NETDEMO_LOG("[NETTEST] tcp connect failed\n");
        znet_tcp_abort(http_pcb);
        http_pcb = NULL;
    }
}

/* --- DNS ------------------------------------------------------------ */

static void netdemo_dns_found(int err, const znet_ip_addr_t *addr,
                              void *ctx)
{
    char buf[ZNET_MAX_ADDR_STR];

    if (err != 0 || addr == NULL) {
        NETDEMO_LOG("[NETTEST] DNS lookup failed: %s\n",
                    NETDEMO_HTTP_HOST);
        return;
    }

    znet_ip_to_str(buf, sizeof(buf), addr);
    NETDEMO_LOG("[NETTEST] DNS resolved %s -> %s\n",
                NETDEMO_HTTP_HOST, buf);
    netdemo_tcp_start(addr, (znet_if_t *)ctx);
}

static void netdemo_dns(znet_if_t *nif)
{
    znet_ipv4_addr_t dns_server;
    znet_mac_t server_mac;

    znet_ipv4_from_str(&dns_server, "10.0.2.3");
    znet_dns_set_server(&dns_server);

    /* Warm the ARP entry for the DNS server first: znet drops an
     * outgoing packet on an ARP cache miss (no lwIP-style "queue
     * pending" yet), so the very first query to a cold neighbor
     * would fail with -1 while sending only the ARP request. The
     * retry tick would recover, but only after 2s. */
    if (znet_ipv4_resolve(nif, &dns_server, &server_mac) != 0) {
        NETDEMO_LOG("[NETTEST] ARP for DNS server pending, warming...\n");
        kthread_sleep_ms(500);    /* let the ARP reply teach the cache */
    }

    NETDEMO_LOG("[NETTEST] Resolving %s\n", NETDEMO_HTTP_HOST);

    /* znet DNS is async-only (no cache, one query at a time): the
     * callback fires once with the answer, or err != 0 after the
     * retry budget. Retries are driven by net_tick() ->
     * znet_poll_timers(), so keep the timer ticking. We pass nif
     * through the ctx so the callback can start TCP on it. */
    if (znet_dns_resolve(nif, NETDEMO_HTTP_HOST,
                         netdemo_dns_found, nif) != 0) {
        /* NOTE: this fires on *immediate* rejection only (bad args,
         * query already active, no server set, or the first UDP send
         * failing - e.g. still no ARP entry); timeout/retry failures
         * come through the callback after ~2 retries x 2s. */
        NETDEMO_LOG("[NETTEST] DNS request failed to start\n");
    }
}

/* --- main ------------------------------------------------------------ */

static void netdemo_run(void *arg)
{
    char buf[ZNET_MAX_ADDR_STR];
    znet_if_t *nif = znet_if_by_index(0);   /* eth0 from the port file */

    (void)arg;

    /* let the boot log settle before we start talking */
    kthread_sleep_ms(500);

    if (nif == NULL) {
        NETDEMO_LOG("[NETTEST] No network interface!\n");
        return;
    }

    NETDEMO_LOG("\n========== ZNU NETWORK TEST ==========\n");

    NETDEMO_LOG("[NETTEST] Interface: %s\n", nif->name);
    znet_mac_to_str(buf, sizeof(buf), &nif->mac);
    NETDEMO_LOG("[NETTEST] MAC: %s\n", buf);

    znet_ipv4_to_str(buf, sizeof(buf), &nif->addr);
    NETDEMO_LOG("[NETTEST] IPv4: %s\n", buf);
    znet_ipv4_to_str(buf, sizeof(buf), &nif->netmask);
    NETDEMO_LOG("[NETTEST] Netmask: %s\n", buf);
    znet_ipv4_to_str(buf, sizeof(buf), &nif->gateway);
    NETDEMO_LOG("[NETTEST] Gateway: %s\n", buf);

    if (!nif->_up) {
        NETDEMO_LOG("[NETTEST] Interface is administratively DOWN\n");
        return;
    }
    /* Physical link state lives in the NIC driver in Znu; znet itself
     * only tracks administratively up/down. */

    NETDEMO_LOG("[NETTEST] Interface is UP\n");

    znet_icmp_set_rx_cb(netdemo_icmp_rx);

    if (!znet_ipv4_is_unspecified(&nif->gateway)) {
        znet_mac_t gw_mac;

        NETDEMO_LOG("[NETTEST] Requesting ARP resolution for gateway\n");
        /* On a cache miss this SENDS the ARP request and returns -1
         * ("pending"); the reply teaches the cache via znet_input. */
        (void)znet_ipv4_resolve(nif, &nif->gateway, &gw_mac);
        kthread_sleep_ms(500);    /* let the ARP reply land */
    } else {
        NETDEMO_LOG("[NETTEST] No gateway configured; skipping ARP test\n");
    }

    netdemo_ping(nif);
    kthread_sleep_ms(1000);       /* round-trip time for the echo reply */

    netdemo_dns(nif);
    kthread_sleep_ms(3000);       /* DNS retries + TCP handshake + HTTP */

    NETDEMO_LOG("======================================\n\n");
}

void netdemo_start(void)
{
    process_t *t = kthread_create(netdemo_run, NULL, "netdemo");
    if (t == NULL)
        NETDEMO_LOG("[NETTEST] Failed to spawn netdemo thread\n");
}
