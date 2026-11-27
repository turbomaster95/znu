#include <stdio.h>
#include <stdlib.h>

/**
 * abort: Abnormally terminate the current execution context.
 */
NORETURN void abort(void) {
    panic("abort() called in kernel-space");
    // spin forever
    for (;;) { }
}

