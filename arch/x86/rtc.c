#include <stdint.h>
#include <stdbool.h>
#include <tsc.h>
#include <pi.h> 
#include <rtc.h> 
#include <stdlib.h>
#include <limine.h>

#define CMOS_PORT_DATA 0x71
#define CMOS_PORT_CMD  0x70

#define RTC_SECONDS      0x00
#define RTC_MINUTES      0x02
#define RTC_HOURS        0x04
#define RTC_DAY          0x07  
#define RTC_MONTH        0x08
#define RTC_YEAR         0x09
#define RTC_CENTURY 	 0x32

#define RTC_SR_A_UIP (1 << 7)

#define RTC_SR_B_24HR (1 << 1) 

static void nmi_disable(void) {
    outb(CMOS_PORT_CMD, 0x0A); 
    uint8_t reg_a = inb(CMOS_PORT_DATA);
    
    outb(CMOS_PORT_CMD, 0x0A);
    outb(CMOS_PORT_DATA, reg_a | 0x80); 
}

static void nmi_enable(void) {
    // Read the current value of Register A
    outb(CMOS_PORT_CMD, 0x0A); 
    uint8_t reg_a = inb(CMOS_PORT_DATA);

    // Clear bit 7 to enable NMI
    outb(CMOS_PORT_CMD, 0x0A);
    outb(CMOS_PORT_DATA, reg_a & ~0x80); 
}

static uint8_t bcd_to_binary(uint8_t bcd) {
    return ((bcd >> 4) * 10) + (bcd & 0x0F);
}

bool rtc_init(void) {
    debugln("[RTC] Initializing RTC...");

    nmi_disable();

    uint8_t reg_b = 0;
    outb(CMOS_PORT_CMD, 0x0B); // Status Register B
    reg_b = inb(CMOS_PORT_DATA);

    bool is_24_hour_format = !(reg_b & RTC_SR_B_24HR); // Bit 1 clear means 24-hour format

    uint8_t seconds = 0;
    outb(CMOS_PORT_CMD, RTC_SECONDS);
    seconds = inb(CMOS_PORT_DATA);

    nmi_enable();

    if ((seconds & 0xF) > 9 || (seconds >> 4) > 5) {
        debugerr("[RTC] RTC not responding or invalid data detected (seconds: %x).", seconds);
        return false;
    }

    debugln("[RTC] RTC initialized successfully. Format: %s", is_24_hour_format ? "24-hour" : "12-hour AM/PM");
    return true;
}

bool rtc_read_time(rtc_time_t *time) {
    if (!time) {
        debugerr("[RTC] rtc_read_time: NULL time pointer provided.");
        return false;
    }

    nmi_disable();

    uint8_t sr_a;
    do {
        outb(CMOS_PORT_CMD, 0x0A);
        sr_a = inb(CMOS_PORT_DATA);
    } while (sr_a & RTC_SR_A_UIP);

    uint8_t temp_buf[7]; 
    outb(CMOS_PORT_CMD, RTC_SECONDS);  temp_buf[0] = inb(CMOS_PORT_DATA);
    outb(CMOS_PORT_CMD, RTC_MINUTES);  temp_buf[1] = inb(CMOS_PORT_DATA);
    outb(CMOS_PORT_CMD, RTC_HOURS);    temp_buf[2] = inb(CMOS_PORT_DATA);
    outb(CMOS_PORT_CMD, RTC_DAY);      temp_buf[3] = inb(CMOS_PORT_DATA);
    outb(CMOS_PORT_CMD, RTC_MONTH);    temp_buf[4] = inb(CMOS_PORT_DATA);
    outb(CMOS_PORT_CMD, RTC_YEAR);     temp_buf[5] = inb(CMOS_PORT_DATA);
    outb(CMOS_PORT_CMD, RTC_CENTURY);  temp_buf[6] = inb(CMOS_PORT_DATA);

    outb(CMOS_PORT_CMD, 0x0B);
    uint8_t sr_b = inb(CMOS_PORT_DATA);

    nmi_enable();

    bool is_24_hour_format = (sr_b & RTC_SR_B_24HR); 
    bool is_binary_mode = (sr_b & 0x04); // Bit 2: Data Mode (0 = BCD, 1 = Binary)

    if (!is_binary_mode) {
        time->second = bcd_to_binary(temp_buf[0]);
        time->minute = bcd_to_binary(temp_buf[1]);
        
        if (is_24_hour_format) {
            time->hour = bcd_to_binary(temp_buf[2]);
        } else {
            bool is_pm = temp_buf[2] & 0x80;
            uint8_t hour_bcd = temp_buf[2] & 0x7F;
            uint8_t hour_12 = bcd_to_binary(hour_bcd);

            if (is_pm && hour_12 < 12) hour_12 += 12;
            if (!is_pm && hour_12 == 12) hour_12 = 0;
            time->hour = hour_12;
        }

        time->day     = bcd_to_binary(temp_buf[3]);
        time->month   = bcd_to_binary(temp_buf[4]);
        time->year    = bcd_to_binary(temp_buf[5]);
        time->century = bcd_to_binary(temp_buf[6]);
    } else {
        time->second = temp_buf[0];
        time->minute = temp_buf[1];
        
        if (is_24_hour_format) {
            time->hour = temp_buf[2];
        } else {
            bool is_pm = temp_buf[2] & 0x80;
            uint8_t hour_12 = temp_buf[2] & 0x7F;
            if (is_pm && hour_12 < 12) hour_12 += 12;
            if (!is_pm && hour_12 == 12) hour_12 = 0;
            time->hour = hour_12;
        }

        time->day     = temp_buf[3];
        time->month   = temp_buf[4];
        time->year    = temp_buf[5];
        time->century = temp_buf[6];
    }

    if (time->month == 0 || time->month > 12 || 
        time->day == 0 || time->day > 31 || 
        time->hour >= 24 || time->minute >= 60 || time->second >= 60) {
        
        debugwarn("[RTC] Invalid RTC data: C%02d Y%02d M%02d D%02d %02d:%02d:%02d",
                  time->century, time->year, time->month, time->day, 
                  time->hour, time->minute, time->second);
        return false;
    }

    debugln("[RTC] Read time: %02d%02d-%02d-%02d %02d:%02d:%02d",
            time->century, time->year, time->month, time->day, 
            time->hour, time->minute, time->second);

    return true;
}

bool rtc_write_time(const rtc_time_t *time) {
    debugln("[RTC] rtc_write_time not implemented yet.");
    return false;
}
