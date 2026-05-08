#include <stdint.h>
#include <stdbool.h>
#include <tsc.h>
#include <pi.h> 
#include <rtc.h> 
#include <stdlib.h> // For debugln, debugerr, debugwarn
#include <limine.h> // To check for bootloader info if needed

#define CMOS_PORT_DATA 0x71
#define CMOS_PORT_CMD  0x70

// CMOS registers for time and date
#define RTC_SECONDS     0x00
#define RTC_MINUTES     0x01
#define RTC_HOURS       0x02
#define RTC_DAY         0x07
#define RTC_MONTH       0x08
#define RTC_YEAR        0x09
#define RTC_CENTURY     0x32 // Common register for century

// Status Register A bits
#define RTC_SR_A_UIP (1 << 7) // Update In Progress bit

// CMOS Status Register B bits (for format information)
// Bit 1: 24/12 hour format. 0 = 24-hour, 1 = 12-hour AM/PM
// Bit 2: Daylight Saving Time enable (usually not used by OS)
#define RTC_SR_B_24HR (1 << 1) 

static void nmi_disable(void) {
    // Read the current value of Register A
    outb(CMOS_PORT_CMD, 0x0A); 
    uint8_t reg_a = inb(CMOS_PORT_DATA);
    
    // Set bit 7 to disable NMI
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

// Helper to convert BCD to binary
static uint8_t bcd_to_binary(uint8_t bcd) {
    return ((bcd >> 4) * 10) + (bcd & 0x0F);
}

// Function to initialize RTC communication
bool rtc_init(void) {
    debugln("[RTC] Initializing RTC...");

    // Temporarily disable NMI for CMOS access
    nmi_disable();

    // Read Status Register B to check for 24-hour format
    uint8_t reg_b = 0;
    outb(CMOS_PORT_CMD, 0x0B); // Status Register B
    reg_b = inb(CMOS_PORT_DATA);

    bool is_24_hour_format = !(reg_b & RTC_SR_B_24HR); // Bit 1 clear means 24-hour format

    // Perform a basic read to check if RTC is accessible and responding.
    // Reading seconds is a good indicator.
    uint8_t seconds = 0;
    outb(CMOS_PORT_CMD, RTC_SECONDS);
    seconds = inb(CMOS_PORT_DATA);

    // Re-enable NMI
    nmi_enable();

    // Basic sanity check: seconds should be between 00 and 59 in BCD.
    if ((seconds & 0xF) > 9 || (seconds >> 4) > 5) {
        debugerr("[RTC] RTC not responding or invalid data detected (seconds: %x).", seconds);
        return false;
    }

    debugln("[RTC] RTC initialized successfully. Format: %s", is_24_hour_format ? "24-hour" : "12-hour AM/PM");
    return true;
}

// Function to read the current time from RTC
bool rtc_read_time(rtc_time_t *time) {
    if (!time) {
        debugerr("[RTC] rtc_read_time: NULL time pointer provided.");
        return false;
    }

    nmi_disable();

    // Wait for the Update In Progress (UIP) bit to be clear in Status Register A.
    // This ensures we read consistent time data, as the RTC updates every second.
    uint8_t sr_a;
    do {
        outb(CMOS_PORT_CMD, 0x0A); // Status Register A
        sr_a = inb(CMOS_PORT_DATA);
        // Small delay to avoid busy-waiting too aggressively, though hlt is better
        // __asm__ volatile("pause"); 
    } while (sr_a & RTC_SR_A_UIP); // Loop while Update In Progress bit is set

    // Read all time components.
    uint8_t temp_buf[7]; // Seconds, Minutes, Hours, Day, Month, Year, Century

    outb(CMOS_PORT_CMD, RTC_SECONDS);       temp_buf[0] = inb(CMOS_PORT_DATA);
    outb(CMOS_PORT_CMD, RTC_MINUTES);       temp_buf[1] = inb(CMOS_PORT_DATA);
    outb(CMOS_PORT_CMD, RTC_HOURS);         temp_buf[2] = inb(CMOS_PORT_DATA);
    outb(CMOS_PORT_CMD, RTC_DAY);           temp_buf[3] = inb(CMOS_PORT_DATA);
    outb(CMOS_PORT_CMD, RTC_MONTH);         temp_buf[4] = inb(CMOS_PORT_DATA);
    outb(CMOS_PORT_CMD, RTC_YEAR);          temp_buf[5] = inb(CMOS_PORT_DATA);
    outb(CMOS_PORT_CMD, RTC_CENTURY);       temp_buf[6] = inb(CMOS_PORT_DATA);

    nmi_enable();

    // Convert BCD to binary and populate the struct.
    // We need to check the 24-hour format from Status Register B.
    // For simplicity in this initial implementation, we'll assume 24-hour format
    // or try to infer it. Reading SR_B again here is safer.
    outb(CMOS_PORT_CMD, 0x0B); // Status Register B
    uint8_t sr_b = inb(CMOS_PORT_DATA);
    bool is_24_hour_format = !(sr_b & RTC_SR_B_24HR); // Bit 1 clear means 24-hour format

    time->second = bcd_to_binary(temp_buf[0]);
    time->minute = bcd_to_binary(temp_buf[1]);
    
    if (is_24_hour_format) {
        time->hour = bcd_to_binary(temp_buf[2]);
    } else {
        // Handle 12-hour format (AM/PM)
        uint8_t hour_12 = bcd_to_binary(temp_buf[2]);
        bool is_pm = (hour_12 & 0x80); // Check AM/PM bit
        hour_12 &= ~0x80; // Clear AM/PM bit
        if (hour_12 == 0) hour_12 = 12; // 12 AM is 00:xx in 24h, but RTC shows 12.
        time->hour = (is_pm && hour_12 < 12) ? hour_12 + 12 : hour_12;
        // Note: This doesn't explicitly use AM/PM flag but converts to 24h.
    }
    
    time->day    = bcd_to_binary(temp_buf[3]);
    time->month  = bcd_to_binary(temp_buf[4]);
    time->year   = bcd_to_binary(temp_buf[5]);
    time->century= bcd_to_binary(temp_buf[6]);

    // Basic validation (optional but good practice)
    if (time->year > 99 || time->month == 0 || time->month > 12 || time->day == 0 || time->day > 31 || time->hour >= 24 || time->minute >= 60 || time->second >= 60) {
         debugwarn("[RTC] RTC read yielded potentially invalid date/time: C%d Y%02d M%02d D%02d %02d:%02d:%02d",
                   time->century, time->year, time->month, time->day, time->hour, time->minute, time->second);
         // Potentially return false or handle error more robustly
         // For now, we'll proceed but warn.
         return false; // Indicate a potential issue
    }

    debugln("[RTC] Read time: C%d Y%02d M%02d D%02d %02d:%02d:%02d",
            time->century, time->year, time->month, time->day, time->hour, time->minute, time->second);

    return true;
}

bool rtc_write_time(const rtc_time_t *time) {
    debugln("[RTC] rtc_write_time not implemented yet.");
    return false;
}
