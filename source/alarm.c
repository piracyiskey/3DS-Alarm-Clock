#include "alarm.h"
#include "save.h"
#include "clock.h"
#include "audio.h"
#include <stdio.h>

int alarm_add(SaveData* save, u8 hour, u8 minute, u8 repeat_mode, u8 ringtone_id) {
    if (!save) return -1;
    if (save->alarm_count >= MAX_ALARMS) return -1;

    for (int i = 0; i < MAX_ALARMS; i++) {
        if (save->alarms[i].id == ALARM_INVALID) {
            save->alarms[i].id = (u8)i;
            save->alarms[i].enabled = true;
            save->alarms[i].hour = hour;
            save->alarms[i].minute = minute;
            save->alarms[i].repeat_mode = repeat_mode;
            save->alarms[i].ringtone_id = ringtone_id;

            /* Guard against immediate triggering if scheduled time for today already passed */
            s64 now = get_display_time_seconds();
            int y, m, d;
            s64 now_days = now / 86400;
            if ((now % 86400) < 0) now_days--;
            days_to_ymd(now_days, &y, &m, &d);
            s64 today_fire = ymd_to_days(y, m, d) * 86400LL + hour * 3600LL + minute * 60LL;
            if (today_fire <= now) {
                save->alarms[i].last_fired_epoch = today_fire;
            } else {
                save->alarms[i].last_fired_epoch = today_fire - 86400LL;
            }

            save->alarm_count++;
            save_write(save);
            return i;
        }
    }
    return -1;
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
                if (alarm->last_fired_epoch < expected) {
                    alarm->last_fired_epoch = expected;
                    sys->ringing_mask |= (1 << i);
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
            if (alarm->last_fired_epoch < today_fire_epoch) {
                alarm->last_fired_epoch = today_fire_epoch;
                sys->ringing_mask |= (1 << i);
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
        if (sys->ringing_mask & (1 << i)) {
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

