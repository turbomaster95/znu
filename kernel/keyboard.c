#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#define KB_BUF_SIZE 256

static uint8_t kb_buffer[KB_BUF_SIZE];
static volatile size_t kb_head = 0;
static volatile size_t kb_tail = 0;

static uint8_t shift_pressed = 0;

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

static const char ascii_shifted[128] = {
    [0x02] = '!', [0x03] = '@', [0x04] = '#', [0x05] = '$', [0x06] = '%',
    [0x07] = '^', [0x08] = '&', [0x09] = '*', [0x0A] = '(', [0x0B] = ')',
    [0x0C] = '_', [0x0D] = '+', 
    [0x10] = 'Q', [0x11] = 'W', [0x12] = 'E', [0x13] = 'R', [0x14] = 'T',
    [0x15] = 'Y', [0x16] = 'U', [0x17] = 'I', [0x18] = 'O', [0x19] = 'P',
    [0x1A] = '{', [0x1B] = '}', 
    [0x1E] = 'A', [0x1F] = 'S', [0x20] = 'D', [0x21] = 'F', [0x22] = 'G',
    [0x23] = 'H', [0x24] = 'J', [0x25] = 'K', [0x26] = 'L', [0x27] = ':',
    [0x28] = '"', [0x29] = '~', 
    [0x2B] = '|',
    [0x2C] = 'Z', [0x2D] = 'X', [0x2E] = 'C', [0x2F] = 'V', [0x30] = 'B',
    [0x31] = 'N', [0x32] = 'M', [0x33] = '<', [0x34] = '>', [0x35] = '?',
};

void keyboard_handle_scancode(uint8_t scancode) {
    // Handle shift state
    if (scancode == 0x2A || scancode == 0x36) {
        shift_pressed = true;
        return;
    }
    if (scancode == 0xAA || scancode == 0xB6) {
        shift_pressed = false;
        return;
    }

    // Ignore other release codes
    if (scancode & 0x80) {
        return;
    }

    char c = 0;
    if (scancode < 128) {
        if (shift_pressed && ascii_shifted[scancode]) {
            c = ascii_shifted[scancode];
        } else {
            c = ascii[scancode];
        }
    }

    if (c) {
	debug_putchar(c);
        size_t next = (kb_head + 1) % KB_BUF_SIZE;
        if (next != kb_tail) {
            kb_buffer[kb_head] = c;
            kb_head = next;
        } else {
            // debugln("[kb]   Buffer full!");
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
