#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include <lwip/opt.h>
#include <lwip/netif.h>
#include <lwip/tcpip.h>
#include <lwip/etharp.h>
#include <lwip/raw.h>
#include <lwip/pbuf.h>
#include <lwip/prot/icmp.h>
#include <lwip/inet_chksum.h>
#include <lwip/dns.h>
#include <lwip/tcp.h>
#include <lwip/ip_addr.h>
#include <lwip/ip4_addr.h>
#include "lwip/raw.h"
#include "lwip/pbuf.h"
#include "lwip/netif.h"
#include "lwip/inet_chksum.h"
#include "lwip/prot/icmp.h"
#include "lwip/prot/ip.h"

#define NETDEMO_LOG(...) printf(__VA_ARGS__)

#define NETDEMO_HTTP_HOST "example.com"
#define NETDEMO_HTTP_PORT 80
#define NETDEMO_PING_ID   0x5A4E
#define NETDEMO_PING_SEQ  1

static struct raw_pcb *ping_pcb;
static struct tcp_pcb *http_pcb;
static uint16_t ping_sequence = NETDEMO_PING_SEQ;

static void netdemo_tcp_start(const ip_addr_t *addr);
static void netdemo_dns_found(const char *name, const ip_addr_t *addr, void *arg);

static u8_t netdemo_ping_recv(void *arg, struct raw_pcb *pcb, struct pbuf *p, const ip_addr_t *addr) {
    struct icmp_echo_hdr hdr;

    if (p == NULL)
        return 0;

    if (p->tot_len < sizeof(hdr) || pbuf_copy_partial(p, &hdr, sizeof(hdr), 0) != sizeof(hdr)) {
        pbuf_free(p);
        return 0;
    }

    if (hdr.type == ICMP_ER && lwip_ntohs(hdr.id) == NETDEMO_PING_ID) {
        NETDEMO_LOG("[NETTEST] ICMP reply from %s, sequence=%u\n", ipaddr_ntoa(addr), lwip_ntohs(hdr.seqno));
    }

    pbuf_free(p);
    return 1;
}

static void netdemo_ping(void) {
    ip_addr_t destination;
    struct pbuf *p;
    struct icmp_echo_hdr hdr;

    if (ping_pcb == NULL) {
        ping_pcb = raw_new(IP_PROTO_ICMP);
        if (ping_pcb == NULL) {
            NETDEMO_LOG("[NETTEST] Failed to create ICMP PCB\n");
            return;
        }

        raw_bind_netif(ping_pcb, netif_default);
        raw_recv(ping_pcb, netdemo_ping_recv, NULL);
    }

    IP_ADDR4(&destination, 10, 0, 2, 2);

    memset(&hdr, 0, sizeof(hdr));
    ICMPH_TYPE_SET(&hdr, ICMP_ECHO);
    ICMPH_CODE_SET(&hdr, 0);
    hdr.id = lwip_htons(NETDEMO_PING_ID);
    hdr.seqno = lwip_htons(ping_sequence++);
    hdr.chksum = inet_chksum(&hdr, sizeof(hdr));

    p = pbuf_alloc(PBUF_IP, sizeof(hdr), PBUF_RAM);
    if (p == NULL) {
        NETDEMO_LOG("[NETTEST] Failed to allocate ICMP packet\n");
        return;
    }

    if (pbuf_take(p, &hdr, sizeof(hdr)) != ERR_OK) {
        pbuf_free(p);
        NETDEMO_LOG("[NETTEST] Failed to construct ICMP packet\n");
        return;
    }

    NETDEMO_LOG("[NETTEST] Sending ICMP echo request to 10.0.2.2\n");

    err_t err = raw_sendto(ping_pcb, p, &destination);
    if (err != ERR_OK)
        NETDEMO_LOG("[NETTEST] ICMP transmission failed: %d\n", err);

    pbuf_free(p);
}

static void netdemo_tcp_error(void *arg, err_t err) {
    http_pcb = NULL;
    NETDEMO_LOG("[NETTEST] TCP error: %d\n", err);
}

static err_t netdemo_tcp_recv(void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t err) {
    if (p == NULL) {
        NETDEMO_LOG("[NETTEST] HTTP server closed connection\n");
        tcp_close(pcb);
        http_pcb = NULL;
        return ERR_OK;
    }

    if (err != ERR_OK) {
        pbuf_free(p);
        return err;
    }

    NETDEMO_LOG("[NETTEST] Received %u HTTP response bytes\n", (unsigned int)p->tot_len);

    tcp_recved(pcb, p->tot_len);
    pbuf_free(p);
    return ERR_OK;
}

static err_t netdemo_tcp_connected(void *arg, struct tcp_pcb *pcb, err_t err) {
    static const char request[] =
        "GET / HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "Connection: close\r\n"
        "User-Agent: Znu-NetTest/0.1\r\n"
        "\r\n";

    if (err != ERR_OK) {
        NETDEMO_LOG("[NETTEST] TCP connection failed: %d\n", err);
        tcp_close(pcb);
        http_pcb = NULL;
        return err;
    }

    NETDEMO_LOG("[NETTEST] TCP connected! Sending HTTP request\n");

    tcp_recv(pcb, netdemo_tcp_recv);
    err = tcp_write(pcb, request, sizeof(request) - 1, TCP_WRITE_FLAG_COPY);

    if (err != ERR_OK) {
        NETDEMO_LOG("[NETTEST] tcp_write failed: %d\n", err);
        tcp_abort(pcb);
        http_pcb = NULL;
        return ERR_ABRT;
    }

    err = tcp_output(pcb);
    if (err != ERR_OK)
        NETDEMO_LOG("[NETTEST] tcp_output failed: %d\n", err);

    return ERR_OK;
}

static void netdemo_tcp_start(const ip_addr_t *addr) {
    if (http_pcb != NULL) {
        NETDEMO_LOG("[NETTEST] HTTP connection already active\n");
        return;
    }

    http_pcb = tcp_new();
    if (http_pcb == NULL) {
        NETDEMO_LOG("[NETTEST] Failed to allocate TCP PCB\n");
        return;
    }

    tcp_arg(http_pcb, NULL);
    tcp_err(http_pcb, netdemo_tcp_error);

    NETDEMO_LOG("[NETTEST] Connecting to example.com:%d\n", NETDEMO_HTTP_PORT);

    err_t err = tcp_connect(http_pcb, addr, NETDEMO_HTTP_PORT, netdemo_tcp_connected);
    if (err != ERR_OK) {
        NETDEMO_LOG("[NETTEST] tcp_connect failed: %d\n", err);
        tcp_abort(http_pcb);
        http_pcb = NULL;
    }
}

static void netdemo_dns_found(const char *name, const ip_addr_t *addr, void *arg) {
    if (addr == NULL) {
        NETDEMO_LOG("[NETTEST] DNS lookup failed: %s\n", name);
        return;
    }

    NETDEMO_LOG("[NETTEST] DNS resolved %s -> %s\n", name, ipaddr_ntoa(addr));
    netdemo_tcp_start(addr);
}

static void netdemo_dns(void) {
    ip_addr_t dns_server;
    ip_addr_t resolved;
    
    IP_ADDR4(&dns_server, 10, 0, 2, 3);
    dns_setserver(0, &dns_server);

    NETDEMO_LOG("[NETTEST] Resolving %s\n", NETDEMO_HTTP_HOST);

    err_t err = dns_gethostbyname(NETDEMO_HTTP_HOST, &resolved, netdemo_dns_found, NULL);

    if (err == ERR_OK) {
        NETDEMO_LOG("[NETTEST] DNS cache hit: %s -> %s\n", NETDEMO_HTTP_HOST, ipaddr_ntoa(&resolved));
        netdemo_tcp_start(&resolved);
    } else if (err != ERR_INPROGRESS) {
        NETDEMO_LOG("[NETTEST] DNS request failed: %d\n", err);
    }
}

static void netdemo_start_core(void *arg) {
    if (netif_default == NULL) {
        NETDEMO_LOG("[NETTEST] No default network interface!\n");
        return;
    }

    NETDEMO_LOG("\n========== ZNU NETWORK TEST ==========\n");

    NETDEMO_LOG("[NETTEST] Interface: %c%c\n", netif_default->name[0], netif_default->name[1]);
    NETDEMO_LOG("[NETTEST] MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
        netif_default->hwaddr[0], netif_default->hwaddr[1],
        netif_default->hwaddr[2], netif_default->hwaddr[3],
        netif_default->hwaddr[4], netif_default->hwaddr[5]);

    NETDEMO_LOG("[NETTEST] IPv4: %s\n", ip4addr_ntoa(netif_ip4_addr(netif_default)));
    NETDEMO_LOG("[NETTEST] Netmask: %s\n", ip4addr_ntoa(netif_ip4_netmask(netif_default)));
    NETDEMO_LOG("[NETTEST] Gateway: %s\n", ip4addr_ntoa(netif_ip4_gw(netif_default)));

    if (!netif_is_up(netif_default)) {
        NETDEMO_LOG("[NETTEST] Interface is administratively DOWN\n");
        return;
    }

    if (!netif_is_link_up(netif_default)) {
        NETDEMO_LOG("[NETTEST] Physical link is DOWN\n");
        return;
    }

    NETDEMO_LOG("[NETTEST] Interface is UP\n");

    if (!ip4_addr_isany(netif_ip4_gw(netif_default))) {
        ip4_addr_t gateway;
        ip4_addr_copy(gateway, *netif_ip4_gw(netif_default));

        NETDEMO_LOG("[NETTEST] Requesting ARP resolution for gateway\n");
        etharp_request(netif_default, &gateway);
    } else {
        NETDEMO_LOG("[NETTEST] No gateway configured; skipping ARP test\n");
    }

    netdemo_ping();
    netdemo_dns();

    NETDEMO_LOG("======================================\n\n");
}

void netdemo_start(void) {
    err_t err = tcpip_callback(netdemo_start_core, NULL);

    if (err != ERR_OK)
        NETDEMO_LOG("[NETTEST] Failed to schedule network tests: %d\n", err);
}
