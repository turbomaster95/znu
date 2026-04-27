#include <stddef.h>
#include <stdint.h>

#define KB_BUF_SIZE 256

static uint8_t kb_buffer[KB_BUF_SIZE];
static volatile size_t kb_head = 0;
static volatile size_t kb_tail = 0;

/**
 * Scan Code Set 1 ASCII Mapping
 * Using designated initializers for clarity.
 */
static const char ascii[128] = {
    [0x01] = 27,   // Escape
    [0x02] = '1', [0x03] = '2', [0x04] = '3', [0x05] = '4', [0x06] = '5',
    [0x07] = '6', [0x08] = '7', [0x09] = '8', [0x0A] = '9', [0x0B] = '0',
    [0x0C] = '-', [0x0D] = '=', 
    [0x0E] = '\b', // Backspace
    [0x0F] = '\t', // Tab
    [0x10] = 'q', [0x11] = 'w', [0x12] = 'e', [0x13] = 'r', [0x14] = 't',
    [0x15] = 'y', [0x16] = 'u', [0x17] = 'i', [0x18] = 'o', [0x19] = 'p',
    [0x1A] = '[', [0x1B] = ']', 
    [0x1C] = '\n', // Enter
    [0x1E] = 'a', [0x1F] = 's', [0x20] = 'd', [0x21] = 'f', [0x22] = 'g',
    [0x23] = 'h', [0x24] = 'j', [0x25] = 'k', [0x26] = 'l', [0x27] = ';',
    [0x28] = '\'',[0x29] = '`', 
    [0x2B] = '\\',
    [0x2C] = 'z', [0x2D] = 'x', [0x2E] = 'c', [0x2F] = 'v', [0x30] = 'b',
    [0x31] = 'n', [0x32] = 'm', [0x33] = ',', [0x34] = '.', [0x35] = '/',
    [0x37] = '*',
    [0x39] = ' ',  // Space
};

void keyboard_handle_scancode(uint8_t scancode) {
    // Ignore release codes (break codes)
    if (scancode & 0x80) return;

    if (scancode < 128 && ascii[scancode]) {
        size_t next = (kb_head + 1) % KB_BUF_SIZE;
        if (next != kb_tail) {
            kb_buffer[kb_head] = ascii[scancode];
            kb_head = next;
        }
    }
}

size_t keyboard_read(char* buf, size_t count) {
    size_t i = 0;
    while (i < count && kb_head != kb_tail) {
        buf[i++] = (char)kb_buffer[kb_tail];
        kb_tail = (kb_tail + 1) % KB_BUF_SIZE;
    }
    return i;
}
