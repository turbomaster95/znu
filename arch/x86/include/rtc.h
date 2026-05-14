#ifndef RTC_H
#define RTC_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>

typedef struct {
    uint8_t  second;
    uint8_t  minute;
    uint8_t  hour;
    uint8_t  day;
    uint8_t  month;
    uint8_t  year;
    uint8_t  century;
} rtc_time_t;

bool rtc_init(void);
bool rtc_read_time(rtc_time_t *time);
bool rtc_write_time(const rtc_time_t *time);

#endif /* RTC_H */
