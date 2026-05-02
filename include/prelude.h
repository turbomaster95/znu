// kernel_prelude.h
#ifndef _PRELUDE_H
#define _PRELUDE_H

#define PERFORM __attribute__((no_sanitize("undefined", "address", "thread"), noinline))

#endif
