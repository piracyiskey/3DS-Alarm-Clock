#pragma once
#include <3ds/types.h>
#include <stdbool.h>

#include "alarm.h"

#define SAVE_MAGIC   0x434C4B32  /* "CLK2" */
#define SAVE_VERSION 4

#define MAX_WORLD_CITIES 32

typedef enum {
    DATEFMT_ISO,  /* YYYY-MM-DD */
    DATEFMT_EUR,  /* DD/MM/YYYY */
    DATEFMT_US    /* MM/DD/YYYY */
} DateFormat;

typedef struct SaveData {
    u32        magic;                            /*    4 bytes */
    u32        version;                          /*    4 bytes */
    s32        time_offset_s;                    /*    4 bytes */
    s32        date_offset_days;                 /*    4 bytes */
    u8         date_format;                      /*    1 byte  */
    u8         alarm_count;                      /*    1 byte - number of valid alarms (0..32) */
    u8         home_city_id;                     /*    1 byte - home reference city ID */
    u8         world_city_count;                 /*    1 byte - count of active world clock cities (0..32) */
    u8         auto_sleep_idx;                   /*    1 byte - auto turn off display (0 = Never, 1..7 = 1m..60m) */
    u8         reserved_header[7];               /*    7 bytes - future header fields */
    AlarmEntry alarms[MAX_ALARMS];               /* 1280 bytes - 32 × 40 bytes */
    u8         world_cities[MAX_WORLD_CITIES];   /*   32 bytes - 32 × 1 byte */
    u8         reserved_tail[16];                /*   16 bytes - future expansion */
} SaveData;                                      /* Total: exactly 1360 bytes */

void save_init(void);
void save_exit(void);
void save_flush(void);
bool save_read(SaveData* out);
bool save_write(const SaveData* data);
void save_reset(SaveData* data);
void save_init_default(SaveData* data);
void save_ensure_dir(void);
