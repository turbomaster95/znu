#include <kernel/display.h>
#include <stdint.h>

#include <kernel/colors.h>

void draw_kernel_gui() {
    // 1. Clear the background (Catppuccin Base)
    draw_rect(0, 0, 1920, 1080, CAT_BASE);

    // 2. Draw a Window Frame (Glassmorphism inspired)
    uint32_t win_x = 100, win_y = 100;
    uint32_t win_w = 600, win_h = 400;
    
    // Window Body (Surface0 with subtle transparency if supported, but here just color)
    draw_rect(win_x, win_y, win_w, win_h, CAT_SURFACE0);
    draw_outline_rect(win_x, win_y, win_w, win_h, CAT_OVERLAY0);

    // 3. Draw Title Bar
    uint32_t bar_h = 35;
    draw_rect(win_x, win_y, win_w, bar_h, CAT_MANTLE);
    
    // 4. Draw Window Buttons (Mac-style)
    uint32_t btn_radius = 6;
    uint32_t start_x = win_x + 15;
    uint32_t start_y = win_y + 12;
    
    draw_rect(start_x, start_y, 12, 12, CAT_RED);    // Close
    draw_rect(start_x + 20, start_y, 12, 12, CAT_YELLOW); // Minimize
    draw_rect(start_x + 40, start_y, 12, 12, CAT_GREEN);  // Maximize

    // 5. Add Text using TTY
    terminal_writestring("\033[1;34mZnu System Monitor\033[0m");
    terminal_writestring("Kernel: \033[1;32mv0.1.0-alpha\033[0m");
    terminal_writestring("Status: \033[1;36mAll Systems Nominal\033[0m");

    // 6. Decorative line (Mauve)
    draw_line(win_x + 20, win_y + 150, win_x + win_w - 20, win_y + 150, CAT_MAUVE);
}

