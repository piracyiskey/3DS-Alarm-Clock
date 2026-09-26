#pragma once
#include <3ds/types.h>
#include <stdbool.h>

#define MAX_WORLD_CITIES 32

typedef enum {
    WORLD_DST_NONE = 0,
    WORLD_DST_USA,      /* North America (2nd Sun Mar -> 1st Sun Nov, +1h) */
    WORLD_DST_EU,       /* Europe & UK (last Sun Mar -> last Sun Oct, +1h) */
    WORLD_DST_AU,       /* Australia NSW/VIC/SA (1st Sun Oct -> 1st Sun Apr, +1h) */
    WORLD_DST_NZ        /* New Zealand (last Sun Sep -> 1st Sun Apr, +1h) */
} DstRule;

typedef struct {
    const char* city;         /* e.g. "Tokyo" */
    const char* country;      /* e.g. "Japan" */
    s8          utc_offset_h; /* Base UTC offset hours (-12..+14) */
    s8          utc_offset_m; /* Base UTC offset minutes (0, 30, 45) */
    u8          dst_rule;     /* DstRule enum */
} CityTimezone;

/* Query Database */
int                 world_clock_get_total_cities(void);
const CityTimezone* world_clock_get_city_info(u8 city_id);

/* Time & Offset Calculations */
bool world_clock_is_dst_active(DstRule rule, int year, int month, int day, int hour);

void world_clock_calculate_time(
    u8 city_id,
    u8 home_city_id,
    int local_h, int local_m, int local_s,
    int local_y, int local_mo, int local_d,
    int* out_h, int* out_m, int* out_s,
    int* out_day_offset,
    int* out_diff_hours,
    int* out_diff_mins
);

bool world_clock_is_daytime(int hour);

/* Save Data List Management */
typedef struct SaveData SaveData;

bool world_clock_has_city(const SaveData* save, u8 city_id);
bool world_clock_add_city(SaveData* save, u8 city_id);
bool world_clock_remove_city(SaveData* save, int index);
void world_clock_set_home_city(SaveData* save, u8 city_id);

/* Navigation & Viewport Scroll Helpers */
int  world_clock_calc_wrap_index(int current_idx, int total_count, int direction);
void world_clock_calc_viewport_scroll(float* scroll_y, int selected_idx, float item_h, float gap, float view_top, float view_bot);

