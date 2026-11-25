#include <netdev.h>
#include <net.h>
#include <kernel/logging.h>
#include <sync.h>
#include <proc.h>

static netdev_t *netdev_table[NETDEV_MAX];
size_t netdev_count = 0;

int netdev_register(netdev_t *dev) {
    if (!dev || !dev->ops || !dev->ops->transmit || !dev->ops->receive || netdev_count >= NETDEV_MAX) return -1;
    netdev_table[netdev_count++] = dev;
    return 0;
}
