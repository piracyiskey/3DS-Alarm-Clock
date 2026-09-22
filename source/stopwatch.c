#include "stopwatch.h"
#include <3ds/os.h>

#define MAX_MS ((u64)STOPWATCH_CAP_HOURS * 3600ULL * 1000ULL)

void stopwatch_init(Stopwatch* sw)
{
    sw->base_ms = 0;
    sw->accumulated_ms = 0;
    sw->state = SW_IDLE;
}

void stopwatch_start(Stopwatch* sw)
{
    sw->base_ms = osGetTime();
    sw->accumulated_ms = 0;
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
    sw->state = SW_IDLE;
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

    *cs = (int)((elapsed_ms / 10) % 100);
    *ss = (int)((elapsed_ms / 1000) % 60);
    *mm = (int)((elapsed_ms / 60000) % 60);
    *hh = (int)(elapsed_ms / 3600000);

    *show_hours = (*hh > 0);
}
