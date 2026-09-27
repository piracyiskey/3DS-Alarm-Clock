#include "alarm.h"
#include "save.h"
#include "clock.h"
#include "audio.h"
#include <stdio.h>
#include <string.h>

void alarm_sort(SaveData* save) {
    if (!save || save->alarm_count <= 1) return;

    for (int i = 1; i < save->alarm_count; i++) {
        AlarmEntry key = save->alarms[i];
        u16 key_time = (u16)key.hour * 60 + key.minute;
        int j = i - 1;
        while (j >= 0 && ((u16)save->alarms[j].hour * 60 + save->alarms[j].minute) > key_time) {
            save->alarms[j + 1] = save->alarms[j];
            j--;
        }
        save->alarms[j + 1] = key;
    }

    for (int i = 0; i < MAX_ALARMS; i++) {
        if (i < save->alarm_count) {
            save->alarms[i].id = (u8)i;
        } else {
            save->alarms[i].id = ALARM_INVALID;
        }
    }
}

int alarm_add(SaveData* save, u8 hour, u8 minute, u8 repeat_mode, u8 ringtone_id, const char* label) {
    if (!save) return -1;
    if (save->alarm_count >= MAX_ALARMS) return -1;

    u16 new_time = (u16)hour * 60 + minute;
    int insert_idx = save->alarm_count;

    for (int i = 0; i < save->alarm_count; i++) {
        u16 curr_time = (u16)save->alarms[i].hour * 60 + save->alarms[i].minute;
        if (new_time < curr_time) {
            insert_idx = i;
            break;
        }
    }

    /* Shift elements right from insert_idx to make room */
    for (int i = save->alarm_count; i > insert_idx; i--) {
        save->alarms[i] = save->alarms[i - 1];
    }

    /* Guard against immediate triggering if scheduled time for today already passed */
    s64 now = get_display_time_seconds();
    int y, m, d;
    s64 now_days = now / 86400;
    if ((now % 86400) < 0) now_days--;
    days_to_ymd(now_days, &y, &m, &d);
    s64 today_fire = ymd_to_days(y, m, d) * 86400LL + hour * 3600LL + minute * 60LL;
    s64 last_epoch = (today_fire <= now) ? today_fire : (today_fire - 86400LL);

    save->alarms[insert_idx].id = (u8)insert_idx;
    save->alarms[insert_idx].enabled = true;
    save->alarms[insert_idx].hour = hour;
    save->alarms[insert_idx].minute = minute;
    save->alarms[insert_idx].repeat_mode = repeat_mode;
    save->alarms[insert_idx].ringtone_id = ringtone_id;
    save->alarms[insert_idx].last_fired_epoch = last_epoch;
    if (label) {
        snprintf(save->alarms[insert_idx].label, sizeof(save->alarms[insert_idx].label), "%s", label);
    } else {
        save->alarms[insert_idx].label[0] = '\0';
    }

    save->alarm_count++;

    /* Update IDs to match positions */
    for (int i = 0; i < MAX_ALARMS; i++) {
        if (i < save->alarm_count) {
            save->alarms[i].id = (u8)i;
        } else {
            save->alarms[i].id = ALARM_INVALID;
        }
    }

    save_write(save);
    return insert_idx;
}

void alarm_delete(SaveData* save, int index) {
    if (!save || index < 0 || index >= MAX_ALARMS) return;
    if (save->alarms[index].id != ALARM_INVALID) {
        save->alarm_count--;
        
        /* Compact array */
        for (int i = index; i < MAX_ALARMS - 1; i++) {
            save->alarms[i] = save->alarms[i + 1];
        }
        /* Invalidate the last slot since everything shifted left */
        save->alarms[MAX_ALARMS - 1].id = ALARM_INVALID;
        save->alarms[MAX_ALARMS - 1].enabled = false;
        save->alarms[MAX_ALARMS - 1].label[0] = '\0';
        
        /* Update IDs to match new positions */
        for (int i = 0; i < MAX_ALARMS; i++) {
            if (save->alarms[i].id != ALARM_INVALID) {
                save->alarms[i].id = (u8)i;
            }
        }
        
        save_write(save);
    }
}

void alarm_update(SaveData* save, int index) {
    if (!save || index < 0 || index >= MAX_ALARMS) return;
    if (save->alarms[index].id != ALARM_INVALID) {
        save_write(save);
    }
}

void alarm_sys_init(AlarmSystem* sys) {
    if (sys) {
        sys->ringing_mask = 0;
        sys->ring_start_epoch = 0;
        sys->active_ringtone_id = 0;
        sys->state = ALARM_STATE_IDLE;
        sys->missed_alarm = false;
        sys->missed_count = 0;
        sys->ring_frames = 0;
    }
}

s64 alarm_calc_next_fire_epoch(s64 now_epoch, const AlarmEntry* alarm) {
    if (!alarm || !alarm->enabled) return -1;

    int y, m, d;
    s64 now_days = now_epoch / 86400;
    if ((now_epoch % 86400) < 0) now_days--; 
    days_to_ymd(now_days, &y, &m, &d);

    s64 today_fire_epoch = ymd_to_days(y, m, d) * 86400LL + alarm->hour * 3600LL + alarm->minute * 60LL;
    s64 candidate = today_fire_epoch;

    if (candidate <= now_epoch) {
        candidate += 86400LL;
    }

    while (true) {
        s64 c_days = candidate / 86400LL;
        if ((candidate % 86400LL) < 0) c_days--;
        days_to_ymd(c_days, &y, &m, &d);
        int dow = day_of_week(y, m, d);

        if (alarm->repeat_mode == REPEAT_ONCE || alarm->repeat_mode == REPEAT_DAILY) {
            return candidate;
        } else if (alarm->repeat_mode == REPEAT_WEEKDAYS) {
            if (dow >= 1 && dow <= 5) return candidate;
        } else if (alarm->repeat_mode == REPEAT_WEEKENDS) {
            if (dow == 0 || dow == 6) return candidate;
        }
        candidate += 86400LL;
    }
    return -1;
}

static s64 prev_app_time = 0;

static void alarm_check_missed(SaveData* save, AlarmSystem* sys, s64 prev, s64 now) {
    for (int i = 0; i < save->alarm_count; i++) {
        AlarmEntry* alarm = &save->alarms[i];
        if (!alarm->enabled) continue;

        s64 expected = alarm_calc_next_fire_epoch(prev, alarm);
        if (expected != -1 && expected <= now) {
            if (now - expected <= 600) {
                if (alarm->last_fired_epoch < (u64)expected) {
                    alarm->last_fired_epoch = (u64)expected;
                    sys->ringing_mask |= (1U << i);
                    if (sys->state == ALARM_STATE_IDLE) {
                        sys->state = ALARM_STATE_RINGING;
                        sys->ring_start_epoch = now;
                        sys->active_ringtone_id = alarm->ringtone_id;
                        audio_play(sys->active_ringtone_id);
                    } else {
                        sys->ring_start_epoch = now;
                        if (sys->active_ringtone_id != alarm->ringtone_id) {
                            sys->active_ringtone_id = alarm->ringtone_id;
                            audio_play(sys->active_ringtone_id);
                        }
                    }
                }
            } else {
                /* Older than 10 minutes: silently advance */
                alarm->last_fired_epoch = expected;
                if (alarm->repeat_mode == REPEAT_ONCE) {
                    alarm->enabled = false;
                }
            }
        }
    }
    save_write(save);
}

void alarm_tick(SaveData* save, AlarmSystem* sys) {
    if (!save || !sys) return;
    
    s64 now = get_display_time_seconds();
    
    if (prev_app_time == 0) {
        prev_app_time = now;
    }
    
    s64 delta = now - prev_app_time;
    if (delta > 120) {
        alarm_check_missed(save, sys, prev_app_time, now);
    }
    
    for (int i = 0; i < save->alarm_count; i++) {
        AlarmEntry* alarm = &save->alarms[i];
        if (!alarm->enabled) continue;
        
        int y, m, d;
        s64 now_days = now / 86400;
        if ((now % 86400) < 0) now_days--;
        days_to_ymd(now_days, &y, &m, &d);
        s64 today_fire_epoch = ymd_to_days(y, m, d) * 86400LL + alarm->hour * 3600LL + alarm->minute * 60LL;
        
        int dow = day_of_week(y, m, d);
        bool valid_today = false;
        if (alarm->repeat_mode == REPEAT_ONCE || alarm->repeat_mode == REPEAT_DAILY) valid_today = true;
        else if (alarm->repeat_mode == REPEAT_WEEKDAYS && dow >= 1 && dow <= 5) valid_today = true;
        else if (alarm->repeat_mode == REPEAT_WEEKENDS && (dow == 0 || dow == 6)) valid_today = true;
        
        if (valid_today && now >= today_fire_epoch && now < today_fire_epoch + 60) {
            if (alarm->last_fired_epoch < (u64)today_fire_epoch) {
                alarm->last_fired_epoch = (u64)today_fire_epoch;
                sys->ringing_mask |= (1U << i);
                printf("ALARM %d TRIGGERED!\n", i);
                if (sys->state == ALARM_STATE_IDLE) {
                    sys->state = ALARM_STATE_RINGING;
                    sys->ring_start_epoch = now;
                    sys->active_ringtone_id = alarm->ringtone_id;
                    audio_play(sys->active_ringtone_id);
                } else {
                    sys->ring_start_epoch = now;
                    if (sys->active_ringtone_id != alarm->ringtone_id) {
                        sys->active_ringtone_id = alarm->ringtone_id;
                        audio_play(sys->active_ringtone_id);
                    }
                }
                save_write(save);
            }
        }
    }
    
    if (sys->state == ALARM_STATE_RINGING && (now - sys->ring_start_epoch >= 600)) {
        alarm_dismiss_all(save, sys);
    }
    
    prev_app_time = now;
}

void alarm_dismiss_all(SaveData* save, AlarmSystem* sys) {
    if (!save || !sys) return;
    
    for (int i = 0; i < save->alarm_count; i++) {
        if (sys->ringing_mask & (1U << i)) {
            if (save->alarms[i].repeat_mode == REPEAT_ONCE) {
                save->alarms[i].enabled = false;
            }
        }
    }
    
    sys->ringing_mask = 0;
    sys->state = ALARM_STATE_IDLE;
    audio_stop();
    save_write(save);
}

void alarm_check_startup_missed(SaveData* save, AlarmSystem* sys, s64 now) {
    if (!save || !sys) return;
    int missed_count = 0;
    for (int i = 0; i < save->alarm_count; i++) {
        AlarmEntry* a = &save->alarms[i];
        if (!a->enabled) continue;
        if (a->last_fired_epoch > 0) {
            s64 expected = alarm_calc_next_fire_epoch(a->last_fired_epoch, a);
            if (expected != -1 && expected < now) {
                missed_count++;
                if (a->repeat_mode == REPEAT_ONCE) {
                    a->enabled = false;
                }
                a->last_fired_epoch = expected;
            }
        }
    }
    if (missed_count > 0) {
        sys->missed_alarm = true;
        sys->missed_count = missed_count;
        save_write(save);
    }
}

int alarm_calc_wrap_index(int current_idx, int total_count, int direction) {
    if (total_count <= 0) return -1;
    if (direction > 0) { /* Down / Next */
        if (current_idx < 0) return 0;
        return (current_idx + 1) % total_count;
    } else if (direction < 0) { /* Up / Prev */
        if (current_idx <= 0) return total_count - 1;
        return current_idx - 1;
    }
    return current_idx;
}

void alarm_calc_viewport_scroll(float* scroll_y, int selected_idx) {
    if (!scroll_y || selected_idx < 0) return;
    float y = 36.0f - *scroll_y + selected_idx * 52.0f;
    if (y > 146.0f) *scroll_y += (y - 146.0f);
    if (y < 36.0f)  *scroll_y -= (36.0f - y);
}

