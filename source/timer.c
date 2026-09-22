#include "timer.h"
#include <3ds/os.h>

void timer_init(Timer* tmr)
{
    tmr->target_h = 0;
    tmr->target_m = 0;
    tmr->target_s = 0;
    tmr->deadline_ms = 0;
    tmr->remaining_at_pause_ms = 0;
    tmr->state = TMR_ADJUST;
}

bool timer_start(Timer* tmr)
{
    u64 duration_s = (u64)tmr->target_h * 3600ULL +
                     (u64)tmr->target_m * 60ULL +
                     (u64)tmr->target_s;
    if (duration_s == 0) return false;

    u64 duration_ms = duration_s * 1000ULL;
    tmr->deadline_ms = osGetTime() + duration_ms;
    tmr->remaining_at_pause_ms = duration_ms;
    tmr->state = TMR_RUNNING;
    return true;
}

void timer_pause(Timer* tmr)
{
    if (tmr->state == TMR_RUNNING) {
        u64 now = osGetTime();
        if (now < tmr->deadline_ms) {
            tmr->remaining_at_pause_ms = tmr->deadline_ms - now;
            tmr->state = TMR_PAUSED;
        } else {
            tmr->remaining_at_pause_ms = 0;
            tmr->state = TMR_EXPIRED;
        }
    }
}

void timer_resume(Timer* tmr)
{
    if (tmr->state == TMR_PAUSED) {
        if (tmr->remaining_at_pause_ms > 0) {
            tmr->deadline_ms = osGetTime() + tmr->remaining_at_pause_ms;
            tmr->state = TMR_RUNNING;
        } else {
            tmr->state = TMR_EXPIRED;
        }
    }
}

void timer_reset(Timer* tmr)
{
    tmr->deadline_ms = 0;
    tmr->remaining_at_pause_ms = 0;
    tmr->state = TMR_ADJUST;
}

void timer_dismiss(Timer* tmr)
{
    tmr->deadline_ms = 0;
    tmr->remaining_at_pause_ms = 0;
    tmr->state = TMR_ADJUST;
}

void timer_get_display(Timer* tmr, int* hh, int* mm, int* ss)
{
    if (tmr->state == TMR_ADJUST || tmr->state == TMR_EXPIRED) {
        *hh = 0;
        *mm = 0;
        *ss = 0;
        return;
    }

    u64 remain_ms = 0;
    if (tmr->state == TMR_RUNNING) {
        u64 now = osGetTime();
        if (now < tmr->deadline_ms) {
            remain_ms = tmr->deadline_ms - now;
        } else {
            remain_ms = 0;
            tmr->state = TMR_EXPIRED;
        }
    } else if (tmr->state == TMR_PAUSED) {
        remain_ms = tmr->remaining_at_pause_ms;
    }

    u64 total_s = (remain_ms + 999ULL) / 1000ULL;
    *hh = (int)(total_s / 3600);
    *mm = (int)((total_s % 3600) / 60);
    *ss = (int)(total_s % 60);
}

bool timer_is_expired(Timer* tmr)
{
    if (tmr->state == TMR_RUNNING) {
        if (osGetTime() >= tmr->deadline_ms) {
            tmr->state = TMR_EXPIRED;
            return true;
        }
    }
    return (tmr->state == TMR_EXPIRED);
}
