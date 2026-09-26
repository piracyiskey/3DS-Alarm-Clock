#include "world_clock.h"
#include "clock.h"
#include "save.h"
#include <stddef.h>

static const CityTimezone s_city_db[] = {
    {"Accra", "Ghana", 0, 0, WORLD_DST_NONE},
    {"Adelaide", "Australia", 9, 30, WORLD_DST_AU},
    {"Almaty", "Kazakhstan", 5, 0, WORLD_DST_NONE},
    {"Amsterdam", "Netherlands", 1, 0, WORLD_DST_EU},
    {"Anchorage", "United States", -9, 0, WORLD_DST_USA},
    {"Athens", "Greece", 2, 0, WORLD_DST_EU},
    {"Auckland", "New Zealand", 12, 0, WORLD_DST_NZ},
    {"Bangkok", "Thailand", 7, 0, WORLD_DST_NONE},
    {"Beijing", "China", 8, 0, WORLD_DST_NONE},
    {"Berlin", "Germany", 1, 0, WORLD_DST_EU},
    {"Bogota", "Colombia", -5, 0, WORLD_DST_NONE},
    {"Boston", "United States", -5, 0, WORLD_DST_USA},
    {"Brisbane", "Australia", 10, 0, WORLD_DST_NONE},
    {"Brussels", "Belgium", 1, 0, WORLD_DST_EU},
    {"Buenos Aires", "Argentina", -3, 0, WORLD_DST_NONE},
    {"Cairo", "Egypt", 2, 0, WORLD_DST_EU},
    {"Calgary", "Canada", -7, 0, WORLD_DST_USA},
    {"Cape Town", "South Africa", 2, 0, WORLD_DST_NONE},
    {"Caracas", "Venezuela", -4, 0, WORLD_DST_NONE},
    {"Casablanca", "Morocco", 1, 0, WORLD_DST_NONE},
    {"Chicago", "United States", -6, 0, WORLD_DST_USA},
    {"Dallas", "United States", -6, 0, WORLD_DST_USA},
    {"Denver", "United States", -7, 0, WORLD_DST_USA},
    {"Dhaka", "Bangladesh", 6, 0, WORLD_DST_NONE},
    {"Doha", "Qatar", 3, 0, WORLD_DST_NONE},
    {"Dubai", "UAE", 4, 0, WORLD_DST_NONE},
    {"Dublin", "Ireland", 0, 0, WORLD_DST_EU},
    {"Frankfurt", "Germany", 1, 0, WORLD_DST_EU},
    {"Hanoi", "Vietnam", 7, 0, WORLD_DST_NONE},
    {"Havana", "Cuba", -5, 0, WORLD_DST_USA},
    {"Helsinki", "Finland", 2, 0, WORLD_DST_EU},
    {"Ho Chi Minh City", "Vietnam", 7, 0, WORLD_DST_NONE},
    {"Hong Kong", "Hong Kong", 8, 0, WORLD_DST_NONE},
    {"Honolulu", "United States", -10, 0, WORLD_DST_NONE},
    {"Houston", "United States", -6, 0, WORLD_DST_USA},
    {"Istanbul", "Turkey", 3, 0, WORLD_DST_NONE},
    {"Jakarta", "Indonesia", 7, 0, WORLD_DST_NONE},
    {"Jerusalem", "Israel", 2, 0, WORLD_DST_EU},
    {"Johannesburg", "South Africa", 2, 0, WORLD_DST_NONE},
    {"Karachi", "Pakistan", 5, 0, WORLD_DST_NONE},
    {"Kathmandu", "Nepal", 5, 45, WORLD_DST_NONE},
    {"Kyiv", "Ukraine", 2, 0, WORLD_DST_EU},
    {"Kuala Lumpur", "Malaysia", 8, 0, WORLD_DST_NONE},
    {"Lagos", "Nigeria", 1, 0, WORLD_DST_NONE},
    {"Lima", "Peru", -5, 0, WORLD_DST_NONE},
    {"Lisbon", "Portugal", 0, 0, WORLD_DST_EU},
    {"London", "United Kingdom", 0, 0, WORLD_DST_EU},
    {"Los Angeles", "United States", -8, 0, WORLD_DST_USA},
    {"Madrid", "Spain", 1, 0, WORLD_DST_EU},
    {"Manila", "Philippines", 8, 0, WORLD_DST_NONE},
    {"Melbourne", "Australia", 10, 0, WORLD_DST_AU},
    {"Mexico City", "Mexico", -6, 0, WORLD_DST_NONE},
    {"Miami", "United States", -5, 0, WORLD_DST_USA},
    {"Montreal", "Canada", -5, 0, WORLD_DST_USA},
    {"Moscow", "Russia", 3, 0, WORLD_DST_NONE},
    {"Mumbai", "India", 5, 30, WORLD_DST_NONE},
    {"Nairobi", "Kenya", 3, 0, WORLD_DST_NONE},
    {"New Delhi", "India", 5, 30, WORLD_DST_NONE},
    {"New York", "United States", -5, 0, WORLD_DST_USA},
    {"Osaka", "Japan", 9, 0, WORLD_DST_NONE},
    {"Oslo", "Norway", 1, 0, WORLD_DST_EU},
    {"Paris", "France", 1, 0, WORLD_DST_EU},
    {"Perth", "Australia", 8, 0, WORLD_DST_NONE},
    {"Phoenix", "United States", -7, 0, WORLD_DST_NONE},
    {"Prague", "Czech Republic", 1, 0, WORLD_DST_EU},
    {"Reykjavik", "Iceland", 0, 0, WORLD_DST_NONE},
    {"Rio de Janeiro", "Brazil", -3, 0, WORLD_DST_NONE},
    {"Riyadh", "Saudi Arabia", 3, 0, WORLD_DST_NONE},
    {"Rome", "Italy", 1, 0, WORLD_DST_EU},
    {"San Francisco", "United States", -8, 0, WORLD_DST_USA},
    {"Santiago", "Chile", -4, 0, WORLD_DST_NONE},
    {"São Paulo", "Brazil", -3, 0, WORLD_DST_NONE},
    {"Seattle", "United States", -8, 0, WORLD_DST_USA},
    {"Seoul", "South Korea", 9, 0, WORLD_DST_NONE},
    {"Singapore", "Singapore", 8, 0, WORLD_DST_NONE},
    {"Stockholm", "Sweden", 1, 0, WORLD_DST_EU},
    {"Sydney", "Australia", 10, 0, WORLD_DST_AU},
    {"Taipei", "Taiwan", 8, 0, WORLD_DST_NONE},
    {"Tokyo", "Japan", 9, 0, WORLD_DST_NONE},
    {"Toronto", "Canada", -5, 0, WORLD_DST_USA},
    {"Vancouver", "Canada", -8, 0, WORLD_DST_USA},
    {"Vienna", "Austria", 1, 0, WORLD_DST_EU},
    {"Warsaw", "Poland", 1, 0, WORLD_DST_EU},
    {"Zurich", "Switzerland", 1, 0, WORLD_DST_EU}
};

#define CITY_DB_COUNT ((int)(sizeof(s_city_db) / sizeof(s_city_db[0])))

int world_clock_get_total_cities(void) {
    return CITY_DB_COUNT;
}

const CityTimezone* world_clock_get_city_info(u8 city_id) {
    if (city_id >= CITY_DB_COUNT) return &s_city_db[0];
    return &s_city_db[city_id];
}

bool world_clock_is_dst_active(DstRule rule, int year, int month, int day, int hour) {
    if (rule == WORLD_DST_NONE) return false;

    if (rule == WORLD_DST_USA) {
        /* US & Canada: Second Sunday in March (02:00) to First Sunday in November (02:00) */
        if (month < 3 || month > 11) return false;
        if (month > 3 && month < 11) return true;

        if (month == 3) {
            /* 2nd Sunday in March: March 1st dow -> 1st Sunday -> +7 days */
            int dow1 = day_of_week(year, 3, 1);
            int sun1 = 1 + (7 - dow1) % 7;
            int sun2 = sun1 + 7;
            if (day > sun2) return true;
            if (day < sun2) return false;
            return hour >= 2;
        } else { /* month == 11 */
            int dow1 = day_of_week(year, 11, 1);
            int sun1 = 1 + (7 - dow1) % 7;
            if (day < sun1) return true;
            if (day > sun1) return false;
            return hour < 2;
        }
    } else if (rule == WORLD_DST_EU) {
        /* Europe & UK: Last Sunday in March (01:00 UTC) to Last Sunday in October (01:00 UTC) */
        if (month < 3 || month > 10) return false;
        if (month > 3 && month < 10) return true;

        if (month == 3) {
            int last_sun = 31 - day_of_week(year, 3, 31);
            if (day > last_sun) return true;
            if (day < last_sun) return false;
            return hour >= 1;
        } else { /* month == 10 */
            int last_sun = 31 - day_of_week(year, 10, 31);
            if (day < last_sun) return true;
            if (day > last_sun) return false;
            return hour < 1;
        }
    } else if (rule == WORLD_DST_AU) {
        /* Australia (NSW/VIC/SA): 1st Sunday in October (02:00) to 1st Sunday in April (03:00) */
        if (month > 4 && month < 10) return false;
        if (month < 4 || month > 10) return true;

        if (month == 4) {
            int dow1 = day_of_week(year, 4, 1);
            int sun1 = 1 + (7 - dow1) % 7;
            if (day < sun1) return true;
            if (day > sun1) return false;
            return hour < 3;
        } else { /* month == 10 */
            int dow1 = day_of_week(year, 10, 1);
            int sun1 = 1 + (7 - dow1) % 7;
            if (day > sun1) return true;
            if (day < sun1) return false;
            return hour >= 2;
        }
    } else if (rule == WORLD_DST_NZ) {
        /* New Zealand: Last Sunday in September (02:00) to 1st Sunday in April (03:00) */
        if (month > 4 && month < 9) return false;
        if (month < 4 || month > 9) return true;

        if (month == 4) {
            int dow1 = day_of_week(year, 4, 1);
            int sun1 = 1 + (7 - dow1) % 7;
            if (day < sun1) return true;
            if (day > sun1) return false;
            return hour < 3;
        } else { /* month == 9 */
            int last_sun = 30 - day_of_week(year, 9, 30);
            if (day > last_sun) return true;
            if (day < last_sun) return false;
            return hour >= 2;
        }
    }

    return false;
}

void world_clock_calculate_time(
    u8 city_id,
    u8 home_city_id,
    int local_h, int local_m, int local_s,
    int local_y, int local_mo, int local_d,
    int* out_h, int* out_m, int* out_s,
    int* out_day_offset,
    int* out_diff_hours,
    int* out_diff_mins
) {
    const CityTimezone* home   = world_clock_get_city_info(home_city_id);
    const CityTimezone* target = world_clock_get_city_info(city_id);

    int home_dst = world_clock_is_dst_active((DstRule)home->dst_rule, local_y, local_mo, local_d, local_h) ? 1 : 0;
    int home_total_mins = (home->utc_offset_h + home_dst) * 60 + home->utc_offset_m;

    int target_dst = world_clock_is_dst_active((DstRule)target->dst_rule, local_y, local_mo, local_d, local_h) ? 1 : 0;
    int target_total_mins = (target->utc_offset_h + target_dst) * 60 + target->utc_offset_m;

    int diff_mins = target_total_mins - home_total_mins;

    /* Compute target total seconds within day */
    s64 local_day_secs = (s64)local_h * 3600 + (s64)local_m * 60 + local_s;
    s64 target_day_secs = local_day_secs + ((s64)diff_mins * 60);

    int day_offset = 0;
    while (target_day_secs < 0) {
        target_day_secs += 86400;
        day_offset--;
    }
    while (target_day_secs >= 86400) {
        target_day_secs -= 86400;
        day_offset++;
    }

    if (out_h) *out_h = (int)(target_day_secs / 3600);
    if (out_m) *out_m = (int)((target_day_secs % 3600) / 60);
    if (out_s) *out_s = local_s;
    if (out_day_offset) *out_day_offset = day_offset;
    if (out_diff_hours) *out_diff_hours = diff_mins / 60;
    if (out_diff_mins)  *out_diff_mins  = diff_mins % 60;
}

bool world_clock_is_daytime(int hour) {
    return (hour >= 6 && hour < 18);
}

bool world_clock_has_city(const SaveData* save, u8 city_id) {
    if (!save) return false;
    for (int i = 0; i < save->world_city_count; i++) {
        if (save->world_cities[i] == city_id) return true;
    }
    return false;
}

bool world_clock_add_city(SaveData* save, u8 city_id) {
    if (!save || city_id >= world_clock_get_total_cities()) return false;
    if (save->world_city_count >= MAX_WORLD_CITIES) return false;
    if (world_clock_has_city(save, city_id)) return false;
    save->world_cities[save->world_city_count++] = city_id;
    save_write(save);
    return true;
}

bool world_clock_remove_city(SaveData* save, int index) {
    if (!save || index < 0 || index >= save->world_city_count) return false;
    u8 removed_city_id = save->world_cities[index];
    for (int i = index; i < save->world_city_count - 1; i++) {
        save->world_cities[i] = save->world_cities[i + 1];
    }
    save->world_city_count--;
    if (save->home_city_id == removed_city_id && save->world_city_count > 0) {
        save->home_city_id = save->world_cities[0];
    }
    save_write(save);
    return true;
}

void world_clock_set_home_city(SaveData* save, u8 city_id) {
    if (!save || city_id >= world_clock_get_total_cities()) return;
    save->home_city_id = city_id;
    save_write(save);
}

int world_clock_calc_wrap_index(int current_idx, int total_count, int direction) {
    if (total_count <= 0) return -1;
    if (direction > 0) {
        if (current_idx < 0) return 0;
        return (current_idx + 1) % total_count;
    } else if (direction < 0) {
        if (current_idx <= 0) return total_count - 1;
        return current_idx - 1;
    }
    return current_idx;
}

void world_clock_calc_viewport_scroll(float* scroll_y, int selected_idx, float item_h, float gap, float view_top, float view_bot) {
    if (!scroll_y || selected_idx < 0) return;
    float item_y = view_top - *scroll_y + selected_idx * (item_h + gap);
    if (item_y + item_h > view_bot) {
        *scroll_y += (item_y + item_h - view_bot);
    }
    if (item_y < view_top) {
        *scroll_y -= (view_top - item_y);
    }
}
