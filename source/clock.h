#pragma once
#include <3ds/types.h>
#include <stdbool.h>
#include <stddef.h>
#include "save.h"

/* Call cfguInit() before using clock functions. */
void clock_init(s32 time_offset_s, s32 date_offset_days);
void clock_get_hms(int* h, int* m, int* s);
void clock_get_ymd(int* y, int* m, int* d);
s64  get_display_time_seconds(void);
void clock_apply_time_edit(int h, int m, int s, SaveData* save);
void clock_apply_date_edit(int y, int m, int d, SaveData* save);
void clock_reset(SaveData* save);
void clock_reset_time(SaveData* save);

/* Date calculation and formatting helpers */
bool is_leap_year(int y);
int  days_in_month(int y, int m);
int  day_of_week(int y, int m, int d);
s64  ymd_to_days(int y, int m, int d);
void days_to_ymd(s64 total_days, int* y, int* m, int* d);
void format_date_string(char* out, size_t sz, int y, int m, int d, DateFormat fmt);
