#ifndef NETDEV_H
#define NETDEV_H

#include <stdint.h>
#include <stddef.h>

#define NETDEV_NAME_LEN 16
#define NETDEV_MAX 8

typedef struct netdev netdev_t;

typedef struct {
    int (*transmit)(netdev_t *dev, const void *data, size_t len);
    int (*receive)(netdev_t *dev, void *buffer, size_t capacity);
} netdev_ops_t;

struct netdev {
    char name[NETDEV_NAME_LEN];
    uint8_t hwaddr[6];
    uint16_t mtu;
    uint32_t flags;
    const netdev_ops_t *ops;
    void *driver_data;
    void *stack_data;
};

int netdev_register(netdev_t *dev);
extern size_t netdev_count;
netdev_t *netdev_get(size_t index);

#endif
