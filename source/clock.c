#include "clock.h"
#include <3ds.h>
#include <stdio.h>

static bool g_has_offset;
static s32  g_time_offset_s;
static s32  g_date_offset_days;

bool is_leap_year(int y)
{
    return (y % 4 == 0 && y % 100 != 0) || (y % 400 == 0);
}

int days_in_month(int y, int m)
{
    static const int mdays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (m < 1 || m > 12) return 30;
    if (m == 2 && is_leap_year(y)) return 29;
    return mdays[m - 1];
}

int day_of_week(int y, int m, int d)
{
    /* Sakamoto's algorithm: returns 0=Sun, 1=Mon, ..., 6=Sat */
    static const int t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    if (m < 3) y--;
    int dow = (y + y / 4 - y / 100 + y / 400 + t[m - 1] + d) % 7;
    if (dow < 0) dow += 7;
    return dow;
}

s64 ymd_to_days(int y, int m, int d)
{
    /* Returns total days from 1900-01-01 to (y, m, d) */
    s64 days = 0;
    for (int yr = 1900; yr < y; yr++)
        days += is_leap_year(yr) ? 366 : 365;
    for (int mo = 1; mo < m; mo++)
        days += days_in_month(y, mo);
    days += d - 1;
    return days;
}

void days_to_ymd(s64 total_days, int* y, int* m, int* d)
{
    int year = 1900;
    while (true) {
        int ydays = is_leap_year(year) ? 366 : 365;
        if (total_days < ydays) break;
        total_days -= ydays;
        year++;
    }

    static const int mdays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    int month = 0;
    while (month < 11) {
        int md = mdays[month];
        if (month == 1 && is_leap_year(year)) md = 29;
        if (total_days < md) break;
        total_days -= md;
        month++;
    }

    *y = year;
    *m = month + 1;
    *d = (int)total_days + 1;
}

void format_date_string(char* out, size_t sz, int y, int m, int d, DateFormat fmt)
{
    static const char* const dow_names[] = {
        "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"
    };
    int dow = day_of_week(y, m, d);
    if (dow < 0 || dow > 6) dow = 0;

    switch (fmt) {
    case DATEFMT_ISO:
        snprintf(out, sz, "%s, %04d-%02d-%02d", dow_names[dow], y, m, d);
        break;
    case DATEFMT_EUR:
        snprintf(out, sz, "%s, %02d/%02d/%04d", dow_names[dow], d, m, y);
        break;
    case DATEFMT_US:
        snprintf(out, sz, "%s, %02d/%02d/%04d", dow_names[dow], m, d, y);
        break;
    default:
        snprintf(out, sz, "%s, %02d/%02d/%04d", dow_names[dow], d, m, y);
        break;
    }
}


static s64 get_system_time_seconds(void)
{
    s64 sys_ms = (s64)osGetTime();
    return sys_ms / 1000;
}

void clock_init(s32 time_offset_s, s32 date_offset_days)
{
    g_time_offset_s    = time_offset_s;
    g_date_offset_days = date_offset_days;
    g_has_offset       = (time_offset_s != 0 || date_offset_days != 0);
}

s64 get_display_time_seconds(void)
{
    s64 sys_s = get_system_time_seconds();
    return sys_s + ((s64)g_date_offset_days * 86400LL) + (s64)g_time_offset_s;
}

void clock_get_hms(int* h, int* m, int* s)
{
    s64 display_s = get_display_time_seconds();
    int tod = (int)(display_s % 86400);
    if (tod < 0) tod += 86400;

    *h = tod / 3600;
    *m = (tod % 3600) / 60;
    *s = tod % 60;
}

void clock_get_ymd(int* y, int* m, int* d)
{
    s64 display_s  = get_display_time_seconds();
    s64 total_days = display_s / 86400;
    int tod        = (int)(display_s % 86400);
    if (tod < 0) total_days--;

    days_to_ymd(total_days, y, m, d);
}

void clock_apply_time_edit(int h, int m, int s, SaveData* save)
{
    int desired_tod = h * 3600 + m * 60 + s;

    s64 sys_s   = get_system_time_seconds();
    int sys_tod = (int)(sys_s % 86400);
    if (sys_tod < 0) sys_tod += 86400;

    g_time_offset_s = desired_tod - sys_tod;
    g_has_offset    = (g_time_offset_s != 0 || g_date_offset_days != 0);

    if (save) {
        save->time_offset_s    = g_time_offset_s;
        save->date_offset_days = g_date_offset_days;
        save_write(save);
    }
}

void clock_apply_date_edit(int y, int m, int d, SaveData* save)
{
    s64 target_days = ymd_to_days(y, m, d);

    s64 sys_s    = get_system_time_seconds();
    s64 sys_days = sys_s / 86400;
    int sys_tod  = (int)(sys_s % 86400);
    if (sys_tod < 0) sys_days--;

    g_date_offset_days = (s32)(target_days - sys_days);
    g_has_offset       = (g_time_offset_s != 0 || g_date_offset_days != 0);

    if (save) {
        save->time_offset_s    = g_time_offset_s;
        save->date_offset_days = g_date_offset_days;
        save_write(save);
    }
}

void clock_reset(SaveData* save)
{
    g_time_offset_s    = 0;
    g_date_offset_days = 0;
    g_has_offset       = false;

    if (save) {
        save_reset(save);
    }
}

void clock_reset_time(SaveData* save)
{
    g_time_offset_s = 0;
    g_has_offset    = (g_date_offset_days != 0);

    if (save) {
        save->time_offset_s = 0;
        save_write(save);
    }
}

bool clock_has_offset(void)
{
    return g_has_offset;
}

