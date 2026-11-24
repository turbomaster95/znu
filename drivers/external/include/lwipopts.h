#include <errno.h>

#define NO_SYS                 0
#define LWIP_SOCKET            0     /* we use raw API, not sockets */
#define LWIP_NETCONN           0
#define LWIP_TCP               1
#define LWIP_UDP               1
#define LWIP_DNS               1
#define LWIP_DHCP              1
#define LWIP_ARP               1
#define LWIP_ICMP              1
#define LWIP_IGMP              0
#define LWIP_IPV6              0

#define MEM_SIZE               (64 * 1024)
#define PBUF_POOL_SIZE         64
#define PBUF_POOL_BUFSIZE      1580
#define MEMP_NUM_TCP_PCB       16
#define MEMP_NUM_TCP_SEG       32
#define TCP_MSS                1460
#define TCP_WND                (8 * TCP_MSS)
#define TCP_SND_BUF            (8 * TCP_MSS)
#define TCP_SND_QUEUELEN       16
#define TCP_MAXRTX             12

#define DNS_TABLE_SIZE         4
#define DNS_MAX_NAME_LENGTH    128

#define LWIP_NETIF_STATUS_CALLBACK  1
#define LWIP_NETIF_LINK_CALLBACK    1
#define LWIP_DHCP_CHECK_LINK_UP     1
#define LWIP_DONT_PROVIDE_BYTEORDER_FUNCTIONS 1   /* keep net.h's inline htons */

#define SYS_LIGHTWEIGHT_PROT  1
#define LWIP_ALLOW_SYS_NO_TIMEOUTS 1

#define LWIP_NOASSERT 1

#define SSIZE_MAX 0x7fffffffffffffffLL

#define LWIP_NO_UNISTD_H 1
