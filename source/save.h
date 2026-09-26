#pragma once
#include <3ds/types.h>
#include <stdbool.h>

#include "alarm.h"

#define SAVE_MAGIC   0x434C4B32  /* "CLK2" */
#define SAVE_VERSION 3

typedef enum {
    DATEFMT_ISO,  /* YYYY-MM-DD */
    DATEFMT_EUR,  /* DD/MM/YYYY */
    DATEFMT_US    /* MM/DD/YYYY */
} DateFormat;

typedef struct SaveData {
    u32        magic;                   /*  4 bytes */
    u32        version;                 /*  4 bytes */
    s32        time_offset_s;           /*  4 bytes */
    s32        date_offset_days;        /*  4 bytes */
    u8         date_format;             /*  1 byte  */
    u8         alarm_count;             /*  1 byte  — number of valid alarms */
    u8         reserved_header[6];      /*  6 bytes — future header fields */
    AlarmEntry alarms[MAX_ALARMS];      /* 16 × 16 = 256 bytes */
    u8         reserved_tail[16];       /* 16 bytes — future expansion */
} SaveData;                             /* Total: 296 bytes */

void save_init(void);
void save_exit(void);
void save_flush(void);
bool save_exists(void);
bool save_read(SaveData* out);
bool save_write(const SaveData* data);
void save_reset(SaveData* data);
void save_init_default(SaveData* data);
void save_ensure_dir(void);
