#ifndef _OFFSETS_H
#define _OFFSETS_H

#include <znu/basics.h>

#define DEFINE(sym, val) \
	asm volatile("\n.ascii \"->" STR(CONCAT(OFFSET_, sym)) " %0 " #val "\"" : : "i" (val))

#define OFFSET(sym, str, mem) \
	DEFINE(sym, offsetof(struct str, mem))

#define OFFSET_T(sym, str, mem) \
	DEFINE(sym, offsetof(str, mem))

#define COMMENT(x) \
	asm volatile("\n.ascii \"->#" x "\"")

#define BLANK() asm volatile("\n.ascii \"->\"" : : )

#endif
