#include "clock.h"
#include <3ds.h>

static bool g_has_offset;
static s64  g_target_time_ms;
static s64  g_hw_rtc_at_save_ms;

s64 clock_get_hw_rtc_ms(void)
{
    s64 rtc_offset = 0;
    /* Config block 0x30001 holds the user-adjustable RTC offset (ms).
       If the read fails, rtc_offset stays 0 → hw_rtc == osGetTime(),
       which degrades gracefully (no system-clock-change immunity). */
    CFGU_GetConfigInfoBlk2(sizeof(rtc_offset), 0x30001, &rtc_offset);
    return (s64)osGetTime() - rtc_offset;
}

void clock_init(const ClockSaveData* save)
{
    if (save && save->magic == CLOCK_SAVE_MAGIC && save->version == 1) {
        g_has_offset       = true;
        g_target_time_ms   = save->target_time_ms;
        g_hw_rtc_at_save_ms = save->hw_rtc_at_save_ms;
    } else {
        g_has_offset       = false;
        g_target_time_ms   = 0;
        g_hw_rtc_at_save_ms = 0;
    }
}

static s64 get_app_time_ms(void)
{
    if (!g_has_offset)
        return (s64)osGetTime();

    s64 hw_now  = clock_get_hw_rtc_ms();
    s64 elapsed = hw_now - g_hw_rtc_at_save_ms;
    return g_target_time_ms + elapsed;
}

void clock_get_hms(int* h, int* m, int* s)
{
    s64 ms      = get_app_time_ms();
    s64 total_s = ms / 1000;
    int tod     = (int)(total_s % 86400);
    if (tod < 0) tod += 86400;

    *h = tod / 3600;
    *m = (tod % 3600) / 60;
    *s = tod % 60;
}

void clock_apply_edit(int h, int m, int s, ClockSaveData* out)
{
    /* Compute the delta between the desired time-of-day and the current one,
       then shift the internal target time by that amount. */
    s64 app_now = get_app_time_ms();
    s64 total_s = app_now / 1000;

    int current_tod = (int)(total_s % 86400);
    if (current_tod < 0) current_tod += 86400;

    int desired_tod = h * 3600 + m * 60 + s;
    s64 diff_ms     = (s64)(desired_tod - current_tod) * 1000;

    s64 hw_now = clock_get_hw_rtc_ms();

    g_target_time_ms   = app_now + diff_ms;
    g_hw_rtc_at_save_ms = hw_now;
    g_has_offset        = true;

    out->magic             = CLOCK_SAVE_MAGIC;
    out->version           = 1;
    out->target_time_ms    = g_target_time_ms;
    out->hw_rtc_at_save_ms = g_hw_rtc_at_save_ms;
}

void clock_reset(void)
{
    g_has_offset       = false;
    g_target_time_ms   = 0;
    g_hw_rtc_at_save_ms = 0;
}

bool clock_has_offset(void)
{
    return g_has_offset;
}
