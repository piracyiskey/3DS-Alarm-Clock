#include "stopwatch.h"
#include <3ds/os.h>

#define MAX_MS ((u64)STOPWATCH_CAP_HOURS * 3600ULL * 1000ULL)

void stopwatch_init(Stopwatch* sw)
{
    sw->base_ms = 0;
    sw->accumulated_ms = 0;
    sw->last_lap_total_ms = 0;
    sw->lap_count = 0;
    sw->scroll_row = 0;
    sw->state = SW_IDLE;
}

void stopwatch_start(Stopwatch* sw)
{
    sw->base_ms = osGetTime();
    sw->accumulated_ms = 0;
    sw->last_lap_total_ms = 0;
    sw->lap_count = 0;
    sw->scroll_row = 0;
    sw->state = SW_RUNNING;
}

void stopwatch_pause(Stopwatch* sw)
{
    if (sw->state == SW_RUNNING) {
        u64 now = osGetTime();
        if (now >= sw->base_ms)
            sw->accumulated_ms += (now - sw->base_ms);
        if (sw->accumulated_ms >= MAX_MS)
            sw->accumulated_ms = MAX_MS;
        sw->state = SW_PAUSED;
    }
}

void stopwatch_resume(Stopwatch* sw)
{
    if (sw->state == SW_PAUSED) {
        if (sw->accumulated_ms < MAX_MS) {
            sw->base_ms = osGetTime();
            sw->state = SW_RUNNING;
        }
    }
}

void stopwatch_reset(Stopwatch* sw)
{
    sw->base_ms = 0;
    sw->accumulated_ms = 0;
    sw->last_lap_total_ms = 0;
    sw->lap_count = 0;
    sw->scroll_row = 0;
    sw->state = SW_IDLE;
}

bool stopwatch_lap(Stopwatch* sw)
{
    if (!sw || sw->state != SW_RUNNING) return false;
    if (sw->lap_count >= MAX_STOPWATCH_LAPS) return false;

    u64 now = osGetTime();
    u64 elapsed_ms = sw->accumulated_ms;
    if (now >= sw->base_ms)
        elapsed_ms += (now - sw->base_ms);
    if (elapsed_ms >= MAX_MS)
        elapsed_ms = MAX_MS;

    u64 lap_time = (elapsed_ms >= sw->last_lap_total_ms) ? (elapsed_ms - sw->last_lap_total_ms) : 0;

    sw->laps[sw->lap_count].lap_time_ms = lap_time;
    sw->laps[sw->lap_count].total_time_ms = elapsed_ms;
    sw->last_lap_total_ms = elapsed_ms;
    sw->lap_count++;

    /* Auto-scroll downward so newest lap is in view (visible_rows = 6) */
    if (sw->lap_count > 6) {
        sw->scroll_row = sw->lap_count - 6;
    } else {
        sw->scroll_row = 0;
    }

    return true;
}

void stopwatch_scroll(Stopwatch* sw, int delta)
{
    if (!sw || sw->lap_count <= 6) return;
    int max_scroll = sw->lap_count - 6;
    sw->scroll_row += delta;
    if (sw->scroll_row < 0) sw->scroll_row = 0;
    if (sw->scroll_row > max_scroll) sw->scroll_row = max_scroll;
}

void stopwatch_format_time(u64 ms, int* hh, int* mm, int* ss, int* cs, bool* show_hours)
{
    if (ms >= MAX_MS) ms = MAX_MS;
    *cs = (int)((ms / 10) % 100);
    *ss = (int)((ms / 1000) % 60);
    *mm = (int)((ms / 60000) % 60);
    *hh = (int)(ms / 3600000);
    *show_hours = (*hh > 0);
}

void stopwatch_get_display(Stopwatch* sw, int* hh, int* mm, int* ss, int* cs, bool* show_hours)
{
    u64 elapsed_ms = sw->accumulated_ms;

    if (sw->state == SW_RUNNING) {
        u64 now = osGetTime();
        if (now >= sw->base_ms)
            elapsed_ms += (now - sw->base_ms);

        if (elapsed_ms >= MAX_MS) {
            elapsed_ms = MAX_MS;
            sw->accumulated_ms = MAX_MS;
            sw->state = SW_PAUSED; /* Cap reached: stop */
        }
    }

    stopwatch_format_time(elapsed_ms, hh, mm, ss, cs, show_hours);
}
