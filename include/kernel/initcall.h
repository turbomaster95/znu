#ifndef _KERNEL_INITCALL_H
#define _KERNEL_INITCALL_H

#include <stddef.h>

typedef int (*initcall_t)(void);
typedef void (*exitcall_t)(void);

#define __define_initcall(fn, id) \
    static initcall_t __initcall_##fn##_##id __attribute__((used, section(".initcall" #id ".init"))) = fn

#define pure_initcall(fn)       __define_initcall(fn, 0)
#define core_initcall(fn)       __define_initcall(fn, 1)
#define postcore_initcall(fn)   __define_initcall(fn, 2)
#define arch_initcall(fn)       __define_initcall(fn, 3)
#define subsys_initcall(fn)     __define_initcall(fn, 4)
#define fs_initcall(fn)         __define_initcall(fn, 5)
#define device_initcall(fn)     __define_initcall(fn, 6)
#define late_initcall(fn)       __define_initcall(fn, 7)

#ifndef MODULE
    /* Built-in Kernel Code */
    #define module_init(fn) late_initcall(fn)
    #define module_exit(fn) static void __attribute__((unused)) __exit_##fn(void) { fn(); }
#else
    /* Dynamic Kernel Module (.ko) */
    #define module_init(fn) \
        int init_module(void) __attribute__((used, visibility("default"), alias(#fn)));

    #define module_exit(fn) \
        void cleanup_module(void) __attribute__((used, visibility("default"), alias(#fn)));
#endif

void run_initcall_level(int level);
void run_initcall_range(int start_level, int end_level);
void do_initcalls(void);

#endif
