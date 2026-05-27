#ifndef PI_H
#define PI_H

#include <stdint.h>

void pit_init(uint32_t frequency);
void pic_remap(void);
uint16_t read_pit_count(void);
void msleep(uint32_t ms);

void lapic_sleep(uint32_t ms);

extern volatile uint64_t timer_ticks;

#endif
