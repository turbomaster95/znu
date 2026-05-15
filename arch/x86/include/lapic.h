#ifndef LAPIC_H
#define LAPIC_H

#include <stdint.h>

#define LAPIC_REG_ID          0x0020
#define LAPIC_REG_SVR         0x00F0
#define LAPIC_REG_LVT_TIMER  0x00320
#define LAPIC_REG_INITIAL_COUNT 0x00380
#define LAPIC_REG_CURRENT_COUNT 0x00390
#define LAPIC_REG_DIVIDE_CONF 0x003E0
#define LAPIC_REG_EOI         0x00B0
#define LAPIC_TIMER_VECTOR 0xF0
#define LAPIC_REG_ICR_LOW   0x300
#define LAPIC_REG_ICR_HIGH  0x310

void lapic_init(void);
uint32_t lapic_read(uint32_t offset);
void lapic_write(uint32_t offset, uint32_t value);
void lapic_eoi(void);
void calibrate_lapic_timer(void);
void lapic_timer_isr(void);
void sleep(uint32_t ms);
extern void lapic_timer_isr_wrapper(void);
void lapic_send_ipi(uint8_t target_apic_id, uint32_t icr_low);
void calibrate_lapic_timer_no_irq();

#endif // LAPIC_H
