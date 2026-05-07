#ifndef _X86_H
#define _X86_H

#include <stddef.h>

// This .h file is used for .c files in arch/x86 whom need exporting some functions but dont really need their own whole .h file

void ps2_init(void);
void pat_init(void);

#endif
