#include <stddef.h>
#include <stdint.h>

// keyboard.c
#define KB_BUF_SIZE 256
static uint8_t kb_buffer[KB_BUF_SIZE];
static volatile size_t kb_head = 0;
static volatile size_t kb_tail = 0;

void keyboard_handle_scancode(uint8_t scancode) {
    if (scancode & 0x80) return;  // ignore break codes for now

    static const char ascii[] = {
        0,  27, '1','2','3','4','5','6','7','8','9','0','-','=','\b',
        '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',
        0,'a','s','d','f','g','h','j','k','l',';','\'','`',
        0,'\\','z','x','c','v','b','n','m',',','.','/',0,
        '*',0,' ',0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    };

    if (scancode < sizeof(ascii) && ascii[scancode]) {
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
        buf[i++] = kb_buffer[kb_tail];
        kb_tail = (kb_tail + 1) % KB_BUF_SIZE;
    }
    return i;
}
