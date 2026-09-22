#pragma once
#include <3ds/types.h>
#include <stdbool.h>

#define CLOCK_SAVE_MAGIC 0x434C4B30 /* "CLK0" */

typedef struct {
    u32 magic;
    u32 version;
    s64 target_time_ms;    /* Desired app time at moment of save (ms since 1900) */
    s64 hw_rtc_at_save_ms; /* Hardware RTC value at moment of save */
} ClockSaveData;

/* Call cfguInit() before using these functions. */
void clock_init(const ClockSaveData* save);
void clock_get_hms(int* h, int* m, int* s);
s64  clock_get_hw_rtc_ms(void);
void clock_apply_edit(int h, int m, int s, ClockSaveData* out);
void clock_reset(void);
bool clock_has_offset(void);
