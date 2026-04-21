#include <kernel/tty.h>
#include <stdint.h>

#define BG color_hex(0x1e1e2e)

void draw_kernel_gui() {
    // 1. Clear the background (Alan Walker Blue-ish)
    draw_rect(0, 0, 1920, 1080, BG);

    // 2. Draw a Window Frame
    uint32_t win_x = 100, win_y = 100;
    uint32_t win_w = 400, win_h = 250;
    
    // Window Body
    draw_rect(win_x, win_y, win_w, win_h, 0xCCCCCC);        // Light Gray Body
    draw_outline_rect(win_x, win_y, win_w, win_h, 0x000000); // Black Border

    // 3. Draw Title Bar
    uint32_t bar_h = 30;
    draw_rect(win_x, win_y, win_w, bar_h, 0x0000AA);        // Dark Blue Bar
    
    // 4. Draw "Close Button" (X)
    uint32_t btn_size = 20;
    uint32_t btn_x = win_x + win_w - 25;
    uint32_t btn_y = win_y + 5;
    draw_rect(btn_x, btn_y, btn_size, btn_size, 0xAA0000);  // Red Button
    // Draw the 'X' with lines
    draw_line(btn_x + 5, btn_y + 5, btn_x + 15, btn_y + 15, 0xFFFFFF);
    draw_line(btn_x + 15, btn_y + 5, btn_x + 5, btn_y + 15, 0xFFFFFF);

    // 5. Add Text using TTY
    // Note: You may need a function to manually set cursor_x/y
    set_cursor(win_x + 10, win_y + 7);
    terminal_writestring("DevaOS System Console");

    set_cursor(win_x + 20, win_y + 50);
    terminal_writestring("Kernel: v0.0.1-alpha");
    
    set_cursor(win_x + 20, win_y + 80);
    terminal_writestring("Status: Memory OK");

    // 6. Draw a decorative "CPU Load" separator line
    draw_line(win_x + 20, win_y + 120, win_x + win_w - 20, win_y + 120, 0x555555);
}

