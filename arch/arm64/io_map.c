#include <arch/io.h>

struct port_map port_map_table[] = {
    { 0x3F8, 0x09000000UL },   /* COM1  > PL011 UART0 */
    { 0x2F8, 0x09000000UL },   /* COM2  > (same, for compat) */
    { 0x378, 0x09010000UL },   /* COM2  > PL011 UART1 (if present) */
};

int port_map_count = 3;
