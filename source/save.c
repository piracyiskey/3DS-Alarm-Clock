#include "save.h"
#include "world_clock.h"
#include "alarm.h"
#include <3ds.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

_Static_assert(sizeof(SaveData) == 1360, "SaveData must be exactly 1360 bytes");

#define SAVE_DIR  "sdmc:/3ds/3ds-clock"
#define SAVE_PATH SAVE_DIR "/save.dat"

#define V1_MAGIC  0x434C4B30  /* "CLK0" */

typedef struct {
    u32 magic;              /* 0x434C4B30 "CLK0" */
    u32 version;            /* 1 */
    s64 target_time_ms;     /* ms since 1900 */
    s64 hw_rtc_at_save_ms;  /* hardware RTC snapshot */
} ClockSaveDataV1;

typedef struct {
    u32       magic;
    u32       version;
    s32       time_offset_s;
    s32       date_offset_days;
    u8        date_format;
    u8        reserved[23];
} SaveDataV2;

typedef struct {
    u8   id;
    bool enabled;
    u8   hour;
    u8   minute;
    u8   repeat_mode;
    u8   ringtone_id;
    u8   _pad[2];
    u64  last_fired_epoch;
} AlarmEntryV3;

typedef struct {
    u32          magic;
    u32          version;
    s32          time_offset_s;
    s32          date_offset_days;
    u8           date_format;
    u8           alarm_count;
    u8           home_city_id;
    u8           world_city_count;
    u8           reserved_header[4];
    AlarmEntryV3 alarms[16];
    u8           world_cities[16];
} SaveDataV3;

_Static_assert(sizeof(SaveDataV3) == 296, "SaveDataV3 must be exactly 296 bytes");

#define SAVE_STACK_SIZE (4 * 1024)
static Thread        s_save_thread = NULL;
static LightEvent    s_save_event;
static LightLock     s_save_lock;
static SaveData      s_pending_save;
static volatile bool s_save_pending = false;
static volatile bool s_save_quit = false;

static bool save_write_to_disk(const SaveData* data)
{
    /* Open existing file for in-place overwrite to avoid FAT table re-allocations */
    FILE* f = fopen(SAVE_PATH, "r+b");
    if (!f) {
        /* File does not exist yet; create with "wb" */
        f = fopen(SAVE_PATH, "wb");
    }
    if (!f) return false;

    size_t n = fwrite(data, 1, sizeof(*data), f);
    fflush(f);
    fclose(f);
    return n == sizeof(*data);
}

static void save_thread_entry(void* arg)
{
    (void)arg;
    while (!s_save_quit) {
        LightEvent_Wait(&s_save_event);
        if (s_save_quit && !s_save_pending) break;

        SaveData to_write;
        bool has_work = false;

        LightLock_Lock(&s_save_lock);
        if (s_save_pending) {
            to_write = s_pending_save;
            s_save_pending = false;
            has_work = true;
        }
        LightLock_Unlock(&s_save_lock);

        if (has_work) {
            save_write_to_disk(&to_write);
        }
    }
}

void save_ensure_dir(void)
{
    mkdir("sdmc:/3ds", 0777);
    mkdir(SAVE_DIR, 0777);
    mkdir(SAVE_DIR "/ringtones", 0777);
}

void save_init(void)
{
    if (s_save_thread) return;

    save_ensure_dir();

    LightEvent_Init(&s_save_event, RESET_ONESHOT);
    LightLock_Init(&s_save_lock);
    s_save_pending = false;
    s_save_quit = false;

    /* Priority 0x31 (lower than audio at 0x18 and main thread at 0x30) */
    s_save_thread = threadCreate(save_thread_entry, NULL, SAVE_STACK_SIZE, 0x31, -2, false);
}

void save_flush(void)
{
    if (!s_save_thread) return;

    while (s_save_pending) {
        svcSleepThread(2 * 1000 * 1000LL); /* 2ms */
    }
    svcSleepThread(5 * 1000 * 1000LL);     /* 5ms settling */
}

void save_exit(void)
{
    if (s_save_thread) {
        s_save_quit = true;
        LightEvent_Signal(&s_save_event);
        threadJoin(s_save_thread, U64_MAX);
        threadFree(s_save_thread);
        s_save_thread = NULL;
    }
}

void save_init_default(SaveData* data)
{
    memset(data, 0, sizeof(*data));
    data->magic            = SAVE_MAGIC;
    data->version          = SAVE_VERSION;
    data->time_offset_s    = 0;
    data->date_offset_days = 0;
    data->date_format      = (u8)DATEFMT_EUR;
    data->alarm_count      = 0;
    for (int i = 0; i < MAX_ALARMS; i++) {
        data->alarms[i].id = ALARM_INVALID;
        data->alarms[i].label[0] = '\0';
    }
    data->home_city_id     = 46; /* London */
    data->world_city_count = 3;
    data->world_cities[0]  = 78; /* Tokyo */
    data->world_cities[1]  = 46; /* London */
    data->world_cities[2]  = 58; /* New York */
}

bool save_exists(void)
{
    FILE* f = fopen(SAVE_PATH, "rb");
    if (!f) return false;
    fclose(f);
    return true;
}

bool save_read(SaveData* out)
{
    if (s_save_pending) {
        save_flush();
    }

    FILE* f = fopen(SAVE_PATH, "rb");
    if (!f) return false;

    u32 header[2] = {0, 0};
    if (fread(header, sizeof(u32), 2, f) != 2) {
        fclose(f);
        return false;
    }

    if (header[0] == SAVE_MAGIC && header[1] == SAVE_VERSION) {
        rewind(f);
        size_t n = fread(out, 1, sizeof(*out), f);
        fclose(f);
        if (n != sizeof(*out)) return false;

        /* Validate / sanitize world clock fields for upgraded v3 saves */
        if (out->home_city_id >= world_clock_get_total_cities()) {
            out->home_city_id = 46; /* London */
        }
        if (out->world_city_count == 0 || out->world_city_count > MAX_WORLD_CITIES) {
            out->world_city_count = 3;
            out->world_cities[0] = 78; /* Tokyo */
            out->world_cities[1] = 46; /* London */
        } else {
            for (int i = 0; i < out->world_city_count; i++) {
                if (out->world_cities[i] >= world_clock_get_total_cities()) {
                    out->world_cities[i] = 0;
                }
            }
        }
        /* Sanitize alarm labels (ensure null termination) */
        for (int i = 0; i < MAX_ALARMS; i++) {
            out->alarms[i].label[ALARM_LABEL_LEN - 1] = '\0';
        }
        if (out->alarm_count > 1) {
            alarm_sort(out);
        }
        return true;
    }

    /* Check for v3 save and auto-migrate to v4 */
    if (header[0] == SAVE_MAGIC && header[1] == 3) {
        rewind(f);
        SaveDataV3 v3;
        size_t n = fread(&v3, 1, sizeof(v3), f);
        fclose(f);
        if (n != sizeof(v3)) return false;

        save_init_default(out);
        out->time_offset_s    = v3.time_offset_s;
        out->date_offset_days = v3.date_offset_days;
        out->date_format      = v3.date_format;
        out->alarm_count      = (v3.alarm_count <= 16) ? v3.alarm_count : 16;
        out->home_city_id     = v3.home_city_id;
        out->world_city_count = (v3.world_city_count <= 16) ? v3.world_city_count : 16;

        for (int i = 0; i < 16; i++) {
            out->alarms[i].id               = v3.alarms[i].id;
            out->alarms[i].enabled          = v3.alarms[i].enabled;
            out->alarms[i].hour             = v3.alarms[i].hour;
            out->alarms[i].minute           = v3.alarms[i].minute;
            out->alarms[i].repeat_mode      = v3.alarms[i].repeat_mode;
            out->alarms[i].ringtone_id      = v3.alarms[i].ringtone_id;
            out->alarms[i].last_fired_epoch = v3.alarms[i].last_fired_epoch;
            out->alarms[i].label[0]         = '\0';
        }

        for (int i = 0; i < 16; i++) {
            out->world_cities[i] = v3.world_cities[i];
        }

        if (out->alarm_count > 1) {
            alarm_sort(out);
        }

        /* Write upgraded v4 save file immediately */
        save_write(out);
        return true;
    }

    /* Check for v2 save and auto-migrate */
    if (header[0] == SAVE_MAGIC && header[1] == 2) {
        rewind(f);
        SaveDataV2 v2;
        size_t n = fread(&v2, 1, sizeof(v2), f);
        fclose(f);
        if (n != sizeof(v2)) return false;

        save_init_default(out);
        out->time_offset_s    = v2.time_offset_s;
        out->date_offset_days = v2.date_offset_days;
        out->date_format      = v2.date_format;

        /* Write updated v3 save file immediately */
        save_write(out);
        return true;
    }

    /* Check for v1 save and auto-migrate */
    if (header[0] == V1_MAGIC && header[1] == 1) {
        rewind(f);
        ClockSaveDataV1 v1;
        size_t n = fread(&v1, 1, sizeof(v1), f);
        fclose(f);
        if (n != sizeof(v1)) return false;

        s64 delta_ms = v1.target_time_ms - v1.hw_rtc_at_save_ms;
        s64 total_seconds = delta_ms / 1000;
        s32 date_offset_days = (s32)(total_seconds / 86400);
        s32 time_offset_s = (s32)(total_seconds % 86400);

        save_init_default(out);
        out->time_offset_s    = time_offset_s;
        out->date_offset_days = date_offset_days;
        out->date_format      = (u8)DATEFMT_EUR;

        /* Write updated v3 save file immediately */
        save_write(out);
        return true;
    }

    fclose(f);
    return false;
}

bool save_write(const SaveData* data)
{
    if (!data) return false;

    if (s_save_thread) {
        LightLock_Lock(&s_save_lock);
        s_pending_save = *data;
        s_save_pending = true;
        LightLock_Unlock(&s_save_lock);

        LightEvent_Signal(&s_save_event);
        return true;
    }

    /* Synchronous fallback if thread not yet running */
    return save_write_to_disk(data);
}

void save_reset(SaveData* data)
{
    data->time_offset_s    = 0;
    data->date_offset_days = 0;
    save_write(data);
}
