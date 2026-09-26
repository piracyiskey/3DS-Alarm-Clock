#include "save.h"
#include <3ds.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

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
    }
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
        return n == sizeof(*out);
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
