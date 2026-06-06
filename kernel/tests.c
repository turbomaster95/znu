#include <stdlib.h>
#include <stdio.h>
#include <mailbox.h>
#include <net.h>
#include <e1000.h>

void worker_hello_task(void *arg) {
    int core_id = (int)(uintptr_t)arg;
    debugln("[worker] Hello from core %d!", core_id);
}

void test_smp_workers(int total_cores) {
    debugln("[bsp] Sending tasks to all APs...");

    for (int i = 1; i < total_cores; i++) {
        mailbox_send_task(i, worker_hello_task, (void*)(uintptr_t)i);
    }
}

static void test_arp(void)
{
    debugln("[NETTEST] === ARP Test ===", 1, 0);
    uint8_t gw_mac[6];
    if (net_arp_resolve(my_gw, gw_mac) == 0) {
        debugln("[NETTEST] ARP resolve gateway: %02x:%02x:%02x:%02x:%02x:%02x",
            1, 0, gw_mac[0], gw_mac[1], gw_mac[2], gw_mac[3], gw_mac[4], gw_mac[5]);
    } else {
        debugln("[NETTEST] ARP resolve FAILED", 2, 1);
    }
}

static void test_dns(const char *hostname)
{
    debugln("[NETTEST] === DNS Test: %s ===", 1, 0, hostname);
    uint8_t ip[4];
    if (dns_resolve(hostname, ip) == 0) {
        debugln("[NETTEST] DNS %s -> %d.%d.%d.%d", 1, 0,
            hostname, ip[0], ip[1], ip[2], ip[3]);
    } else {
        debugln("[NETTEST] DNS resolve FAILED for %s", 2, 1, hostname);
    }
}

static void test_tcp_connect(const uint8_t ip[4], uint16_t port)
{
    debugln("[NETTEST] === TCP Connect %d.%d.%d.%d:%d ===", 1, 0,
        ip[0], ip[1], ip[2], ip[3], port);
    int fd = tcp_connect(ip, port);
    if (fd >= 0) {
        debugln("[NETTEST] TCP connected (fd=%d)", 1, 0, fd);
        tcp_close(fd);
        debugln("[NETTEST] TCP closed", 1, 0);
    } else {
        debugln("[NETTEST] TCP connect FAILED", 2, 1);
    }
}

static void test_http_get(const uint8_t ip[4], uint16_t port, const char *path)
{
    debugln("[NETTEST] === HTTP GET %d.%d.%d.%d:%d%s ===", 1, 0,
        ip[0], ip[1], ip[2], ip[3], port, path);

    int fd = tcp_connect(ip, port);
    if (fd < 0) {
        debugln("[NETTEST] HTTP connect FAILED", 2, 1);
        return;
    }

    char req[256];
    snprintf(req, sizeof(req),
        "GET %s HTTP/1.0\r\n"
        "Host: test\r\n"
        "User-Agent: znu/0.1.0\r\n"
        "Connection: close\r\n"
        "\r\n", path);

    if (tcp_send(fd, req, strlen(req)) < 0) {
        debugln("[NETTEST] HTTP send FAILED", 2, 1);
        tcp_close(fd);
        return;
    }
    debugln("[NETTEST] HTTP request sent, receiving...", 1, 0);

    char buf[4096];
    int total = 0;
    int r;
    while ((r = tcp_recv(fd, buf + total, sizeof(buf) - total - 1)) > 0) {
        total += r;
        if (total >= sizeof(buf) - 1) break;
    }
    buf[total] = '\0';

    if (total > 0) {
        debugln("[NETTEST] HTTP received %d bytes", 1, 0, total);
        // Print first line of response
        char *nl = strchr(buf, '\n');
        if (nl) *nl = '\0';
        debugln("[NETTEST] HTTP status: %s", 1, 0, buf);
    } else {
        debugln("[NETTEST] HTTP receive FAILED (EOF or error)", 2, 1);
    }

    tcp_close(fd);
}

static void test_udp_echo(void)
{
    debugln("[NETTEST] === UDP Echo Test (requires external server) ===", 1, 0);
    debugln("[NETTEST] Skipped — implement UDP recv first", 3, 1);
}

void net_test_suite(void)
{
    debugln("[NETTEST] Starting network test suite...", 1, 0);

    // Wait a moment for link to come up
    for (volatile int i = 0; i < 10000000; i++);

    test_arp();

    // Test DNS against Google
    test_dns("google.com");

    // Test TCP to a well-known service
    // Example: 142.250.80.46 is one of google.com's IPs
    // Replace with an IP you can actually reach from your VM
    uint8_t google_ip[4] = {142, 250, 80, 46};
    test_tcp_connect(google_ip, 80);

    // Try a simple HTTP fetch
    test_http_get(google_ip, 80, "/");

    // Or test against a local echo server if you have one
    // uint8_t local_ip[4] = {10, 0, 2, 2};
    // test_tcp_connect(local_ip, 7);  // echo port

    test_udp_echo();

    debugln("[NETTEST] === Suite complete ===", 1, 0);
}
