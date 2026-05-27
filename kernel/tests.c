#include <stdlib.h>
#include <mailbox.h>

void worker_hello_task(void *arg) {
    int core_id = (int)(uintptr_t)arg;
    debugln("[worker] Hello from core %d!", core_id);
}

void test_smp_workers(int total_cores) {
    debugln("[bsp] Sending tasks to all APs...");

    for (int i = 1; i < total_cores; i++) {
        mailbox_send_task(i, worker_hello_task, (void*)(uintptr_t)i);
    }
}
