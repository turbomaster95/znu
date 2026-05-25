#include <stddef.h>
#include <stdint.h>

void e1000_send_packet(const void *buffer, uint16_t length) {
    (void)buffer;
    (void)length;
}

int e1000_receive_packet(void *buffer, uint16_t length) {
    (void)buffer;
    (void)length;
    return 0; 
}

void e1000_get_mac_address(uint8_t *mac_out) {
    if (mac_out) {
        mac_out[0] = 0x00;
        mac_out[1] = 0x00;
        mac_out[2] = 0x00;
        mac_out[3] = 0x00;
        mac_out[4] = 0x00;
        mac_out[5] = 0x00;
    }
}
