/* Minimal Desktop - CMOS real-time clock */
#include "rtc.h"
#include "io.h"
#include "string.h"
#include "console.h"

#define CMOS_INDEX 0x70
#define CMOS_DATA  0x71

static u8 cmos_read(u8 reg) {
    outb(CMOS_INDEX, (u8)(reg | 0x80));   /* NMI disable while reading */
    return inb(CMOS_DATA);
}

static bool rtc_updating(void) {
    return (cmos_read(0x0A) & 0x80) != 0;
}

static int bcd_to_bin(u8 v) {
    return (v & 0x0F) + ((v >> 4) * 10);
}

void rtc_read(rtc_time_t *t) {
    for (u32 guard = 0; guard < 1000000 && rtc_updating(); guard++) { }

    u8 sec = cmos_read(0x00);
    u8 min = cmos_read(0x02);
    u8 hour = cmos_read(0x04);
    u8 day = cmos_read(0x07);
    u8 mon = cmos_read(0x08);
    u8 year = cmos_read(0x09);
    u8 wday = cmos_read(0x06);
    u8 status_b = cmos_read(0x0B);

    bool bcd = (status_b & 0x04) == 0;
    if (bcd) {
        u8 pmbit = (u8)(hour & 0x80);
        sec = (u8)bcd_to_bin(sec);
        min = (u8)bcd_to_bin(min);
        hour = (u8)(bcd_to_bin((u8)(hour & 0x7F)) | pmbit);
        day = (u8)bcd_to_bin(day);
        mon = (u8)bcd_to_bin(mon);
        year = (u8)bcd_to_bin(year);
    }

    bool pm = (hour & 0x80) != 0;
    hour &= 0x7F;
    if (!(status_b & 0x02)) {           /* 12-hour mode */
        if (pm && hour != 12) hour += 12;
        if (!pm && hour == 12) hour = 0;
    }

    t->second = sec;
    t->minute = min;
    t->hour = hour;
    t->day = day;
    t->month = mon;
    t->year = 2000 + year;
    t->weekday = (wday >= 1 && wday <= 7) ? (wday % 7) : 0;   /* 1=Sunday */
}

const char *rtc_month_name(int m) {
    static const char *names[] = { "?", "Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                   "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
    return (m >= 1 && m <= 12) ? names[m] : "?";
}

const char *rtc_weekday_name(int w) {
    static const char *names[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
    return (w >= 0 && w < 7) ? names[w] : "???";
}

void rtc_format_date(const rtc_time_t *t, char *buf, int max) {
    ksnprintf(buf, (size_t)max, "%s %s %02d %02d:%02d:%02d %04d",
              rtc_weekday_name(t->weekday), rtc_month_name(t->month), t->day,
              t->hour, t->minute, t->second, t->year);
}

void rtc_format_time(const rtc_time_t *t, char *buf, int max) {
    ksnprintf(buf, (size_t)max, "%02d:%02d:%02d", t->hour, t->minute, t->second);
}

void rtc_format_iso(const rtc_time_t *t, char *buf, int max) {
    ksnprintf(buf, (size_t)max, "%04d-%02d-%02d %02d:%02d:%02d",
              t->year, t->month, t->day, t->hour, t->minute, t->second);
}
