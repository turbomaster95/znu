#include <kernel/initcall.h>
#include <stdlib.h>

extern initcall_t __initcall0_start[];
extern initcall_t __initcall1_start[];
extern initcall_t __initcall2_start[];
extern initcall_t __initcall3_start[];
extern initcall_t __initcall4_start[];
extern initcall_t __initcall5_start[];
extern initcall_t __initcall6_start[];
extern initcall_t __initcall7_start[];
extern initcall_t __initcall_end[];

static initcall_t *level_boundaries[] = {
    __initcall0_start,
    __initcall1_start,
    __initcall2_start,
    __initcall3_start,
    __initcall4_start,
    __initcall5_start,
    __initcall6_start,
    __initcall7_start,
    __initcall_end
};

void run_initcall_level(int level) {
    if (level < 0 || level >= 8) return;

    initcall_t *start = level_boundaries[level];
    initcall_t *end   = level_boundaries[level + 1];

    debugln("[initcall] Running level %d...", level);
    for (initcall_t *call = start; call < end; call++) {
        if (*call) {
            debugln("[initcall] Running initcall %p..", *call);
            int ret = (*call)();
            if (ret != 0) {
                debugwarn("[initcall] Initcall %p failed with code %d", *call, ret);
            }
        }
    }
}

void run_initcall_range(int start_level, int end_level) {
    for (int l = start_level; l <= end_level; l++) {
        run_initcall_level(l);
    }
}

void do_initcalls(void) {
    debugln("[initcall] Running all initcalls!");
    run_initcall_range(0, 7);
}
