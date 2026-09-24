#include "save.h"
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

void save_ensure_dir(void)
{
    mkdir("sdmc:/3ds", 0777);
    mkdir(SAVE_DIR, 0777);
    mkdir(SAVE_DIR "/ringtones", 0777);
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
    save_ensure_dir();
    FILE* f = fopen(SAVE_PATH, "wb");
    if (!f) return false;

    size_t n = fwrite(data, 1, sizeof(*data), f);
    fclose(f);
    return n == sizeof(*data);
}

void save_reset(SaveData* data)
{
    data->time_offset_s    = 0;
    data->date_offset_days = 0;
    save_write(data);
}
