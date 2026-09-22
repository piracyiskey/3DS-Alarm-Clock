#pragma once
#include <3ds/types.h>
#include <stdbool.h>

#define STOPWATCH_CAP_HOURS 500

typedef enum {
    SW_IDLE,
    SW_RUNNING,
    SW_PAUSED
} StopwatchState;

typedef struct {
    u64            base_ms;
    u64            accumulated_ms;
    StopwatchState state;
} Stopwatch;

void stopwatch_init(Stopwatch* sw);
void stopwatch_start(Stopwatch* sw);
void stopwatch_pause(Stopwatch* sw);
void stopwatch_resume(Stopwatch* sw);
void stopwatch_reset(Stopwatch* sw);
void stopwatch_get_display(Stopwatch* sw, int* hh, int* mm, int* ss, int* cs, bool* show_hours);
