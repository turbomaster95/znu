#ifndef NETDEV_H
#define NETDEV_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define NETDEV_NAME_MAX 16
#define NETDEV_MAX_DEVICES 16
#define NETDEV_MAX_HWADDR 8

#define NETDEV_FLAG_UP          (1u << 0)
#define NETDEV_FLAG_BROADCAST   (1u << 1)
#define NETDEV_FLAG_ETHERNET    (1u << 2)
#define NETDEV_FLAG_PROMISC     (1u << 3)

typedef struct netdev netdev_t;

typedef struct {
    int (*transmit)(netdev_t *dev, const void *data, size_t len);
    int (*receive)(netdev_t *dev, void *buffer, size_t capacity);
    int (*set_mac)(netdev_t *dev, const uint8_t *mac, size_t len);
    int (*set_flags)(netdev_t *dev, uint32_t flags);
} netdev_ops_t;

struct netdev {
    char name[NETDEV_NAME_MAX];
    uint8_t hwaddr[NETDEV_MAX_HWADDR];
    uint8_t hwaddr_len;
    uint16_t mtu;
    uint32_t flags;
    const netdev_ops_t *ops;
    void *driver_data;
    bool registered;
    void *stack_data;
};

int netdev_register(netdev_t *dev);
int netdev_unregister(netdev_t *dev);
netdev_t *netdev_get(const char *name);
size_t netdev_count(void);
void netdev_poll_all(void);

#endif
