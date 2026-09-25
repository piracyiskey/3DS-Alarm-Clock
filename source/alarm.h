#pragma once
#include <3ds/types.h>
#include <stdbool.h>

#define MAX_ALARMS    16
#define ALARM_INVALID 0xFF

typedef enum {
    REPEAT_ONCE,       /* Fire once, then auto-disable */
    REPEAT_DAILY,      /* Fire every day */
    REPEAT_WEEKDAYS,   /* Mon-Fri */
    REPEAT_WEEKENDS    /* Sat-Sun */
} AlarmRepeatMode;

typedef struct {
    u8   id;                     /* 0..MAX_ALARMS-1, or ALARM_INVALID if slot empty */
    bool enabled;                /* Toggle: armed or disarmed */
    u8   hour;                   /* 0-23 */
    u8   minute;                 /* 0-59 */
    u8   repeat_mode;            /* AlarmRepeatMode cast to u8 */
    u8   ringtone_id;            /* Index into ringtone table */
    u8   _pad[2];                /* Align to 8-byte boundary */
    u64  last_fired_epoch;       /* app_time seconds when alarm last fired (monotonic guard) */
} AlarmEntry;                    /* 16 bytes */

typedef enum {
    ALARM_STATE_IDLE,     /* No alarm currently ringing */
    ALARM_STATE_RINGING   /* One or more alarms are actively firing */
} AlarmRingState;

typedef struct {
    /* Which alarms are currently ringing (bitmask, bit N = alarm index N) */
    u16  ringing_mask;

    /* Ring start timestamp (app_time seconds) for 10-minute auto-silence */
    u64  ring_start_epoch;

    /* Currently playing ringtone_id (for audio switching on cascade) */
    u8   active_ringtone_id;

    AlarmRingState state;
    bool missed_alarm;
    int  missed_count;
    u32  ring_frames;
} AlarmSystem;

struct SaveData;

int  alarm_add(struct SaveData* save, u8 hour, u8 minute, u8 repeat_mode, u8 ringtone_id);
void alarm_delete(struct SaveData* save, int index);
void alarm_update(struct SaveData* save, int index);

void alarm_sys_init(AlarmSystem* sys);
s64  alarm_calc_next_fire_epoch(s64 now_epoch, const AlarmEntry* alarm);
void alarm_tick(struct SaveData* save, AlarmSystem* sys);
void alarm_dismiss_all(struct SaveData* save, AlarmSystem* sys);
void alarm_check_startup_missed(struct SaveData* save, AlarmSystem* sys, s64 now);

int  alarm_calc_wrap_index(int current_idx, int total_count, int direction);
void alarm_calc_viewport_scroll(float* scroll_y, int selected_idx);
