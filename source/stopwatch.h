#pragma once
#include <3ds/types.h>
#include <stdbool.h>

#define STOPWATCH_CAP_HOURS 500
#define MAX_STOPWATCH_LAPS  200

typedef enum {
    SW_IDLE,
    SW_RUNNING,
    SW_PAUSED
} StopwatchState;

typedef struct {
    u64 lap_time_ms;   /* Duration of this specific lap */
    u64 total_time_ms; /* Cumulative elapsed time when lap was recorded */
} LapRecord;

typedef struct {
    u64            base_ms;
    u64            accumulated_ms;
    u64            last_lap_total_ms;
    StopwatchState state;
    LapRecord      laps[MAX_STOPWATCH_LAPS];
    int            lap_count;
    int            scroll_row;
} Stopwatch;

void stopwatch_init(Stopwatch* sw);
void stopwatch_start(Stopwatch* sw);
void stopwatch_pause(Stopwatch* sw);
void stopwatch_resume(Stopwatch* sw);
void stopwatch_reset(Stopwatch* sw);
bool stopwatch_lap(Stopwatch* sw);
void stopwatch_scroll(Stopwatch* sw, int delta);
void stopwatch_get_display(Stopwatch* sw, int* hh, int* mm, int* ss, int* cs, bool* show_hours);
void stopwatch_format_time(u64 ms, int* hh, int* mm, int* ss, int* cs, bool* show_hours);
