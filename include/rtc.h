#pragma once
#include "types.h"

typedef struct {
    int year;
    int month;
    int day;
    int hour;
    int minute;
    int second;
    int weekday;   /* 0 = Sunday */
} rtc_time_t;

void rtc_read(rtc_time_t *t);
const char *rtc_month_name(int m);
const char *rtc_weekday_name(int w);
void rtc_format_date(const rtc_time_t *t, char *buf, int max);
void rtc_format_time(const rtc_time_t *t, char *buf, int max);
void rtc_format_iso(const rtc_time_t *t, char *buf, int max);
