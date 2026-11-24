#include <netdev.h>
#include <net.h>
#include <lwip/init.h>
#include <lwip/netif.h>
#include <lwip/etharp.h>
#include <lwip/pbuf.h>
#include <lwip/tcpip.h>
#include <kernel/logging.h>
#include <sync.h>
#include <proc.h>

#define NETDEV_MAX 8

static netdev_t *netdev_table[NETDEV_MAX];
size_t netdev_count = 0;

static semaphore_t tcpip_ready;
static volatile int tcpip_initialized = 0;

int netdev_register(netdev_t *dev) {
    if (!dev || !dev->ops || !dev->ops->transmit || !dev->ops->receive || netdev_count >= NETDEV_MAX) return -1;
    netdev_table[netdev_count++] = dev;
    return 0;
}

static void netdev_tcpip_init_done(void *arg) {
    (void)arg;
    tcpip_initialized = 1;
    semaphore_signal(&tcpip_ready);
}

void net_init(void) {
    debugln("NET: entering net_init\n");

    semaphore_init(&tcpip_ready, 0);

    debugln("NET: starting lwIP TCP/IP thread\n");
    tcpip_init(netdev_tcpip_init_done, NULL);

    debugln("NET: waiting for TCP/IP initialization\n");
    semaphore_wait(&tcpip_ready);

    if (!tcpip_initialized) {
        debugln("NET: TCP/IP initialization failed\n");
        return;
    }

    debugln("NET: TCP/IP initialization complete\n");
    debugln("NET: leaving net_init\n");
}
