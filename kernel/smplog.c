#include <sync.h>
#include <string.h>
#include <stdlib.h>

#define LOG_QUEUE_SIZE 16
#define LOG_MSG_LIMIT  128

typedef struct {
    char text[LOG_MSG_LIMIT];
    volatile uint32_t ready; // 0 = empty/read, 1 = writing, 2 = ready to print
} smp_msg_t;

static smp_msg_t g_log_queue[LOG_QUEUE_SIZE];
static volatile uint32_t g_queue_write_idx = 0;
static volatile uint32_t g_queue_read_idx = 0;

void smp_enqueue_log(const char* str) {
    uint32_t slot = atomic_fetch_add(&g_queue_write_idx, 1) % LOG_QUEUE_SIZE;

    while (atomic_load(&g_log_queue[slot].ready) != 0) {
        __asm__ volatile("pause");
    }

    atomic_store(&g_log_queue[slot].ready, 1);

    strncpy(g_log_queue[slot].text, str, LOG_MSG_LIMIT - 1);
    g_log_queue[slot].text[LOG_MSG_LIMIT - 1] = '\0';

    atomic_store(&g_log_queue[slot].ready, 2);
}

void smp_flush_logs_to_screen(void) {
    extern int get_cpu_id(void);
    if (get_cpu_id() != 0) return;

    uint32_t slot = g_queue_read_idx % LOG_QUEUE_SIZE;

    while (atomic_load(&g_log_queue[slot].ready) == 2) {
        extern void debug_write(const char* data);
        debug_write(g_log_queue[slot].text);

        atomic_store(&g_log_queue[slot].ready, 0);

        g_queue_read_idx++;
        slot = g_queue_read_idx % LOG_QUEUE_SIZE;
    }
}
