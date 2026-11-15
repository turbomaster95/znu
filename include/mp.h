#ifndef _MP_H
#define _MP_H

#include <arch/mp.h>

void mp_discover_topology(void);
void mp_init(void);
void mp_boot_core(uint8_t id);

#endif
