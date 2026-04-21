#ifndef LAPIC_H
#define LAPIC_H

#include <stdint.h> // For uint types

void lapic_init(void);
void lapic_eoi(void);
void calibrate_lapic_timer(void);
void lapic_timer_isr(void); // C ISR function
void sleep(uint32_t ms); // Power-saving sleep function
extern void lapic_timer_isr_wrapper(void);

#endif // LAPIC_H
