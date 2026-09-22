#pragma once
#include <3ds/types.h>
#include <stdbool.h>

typedef enum {
    TMR_ADJUST,
    TMR_RUNNING,
    TMR_PAUSED,
    TMR_EXPIRED
} TimerState;

typedef struct {
    int        target_h;
    int        target_m;
    int        target_s;
    u64        deadline_ms;
    u64        remaining_at_pause_ms;
    TimerState state;
} Timer;

void timer_init(Timer* tmr);
bool timer_start(Timer* tmr);
void timer_pause(Timer* tmr);
void timer_resume(Timer* tmr);
void timer_reset(Timer* tmr);
void timer_dismiss(Timer* tmr);
void timer_get_display(Timer* tmr, int* hh, int* mm, int* ss);
bool timer_is_expired(Timer* tmr);
