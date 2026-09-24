#pragma once
#include <3ds/types.h>
#include <stdbool.h>

#define SAVE_MAGIC   0x434C4B32  /* "CLK2" */
#define SAVE_VERSION 2

typedef enum {
    DATEFMT_ISO,  /* YYYY-MM-DD */
    DATEFMT_EUR,  /* DD/MM/YYYY */
    DATEFMT_US    /* MM/DD/YYYY */
} DateFormat;

typedef struct {
    u32       magic;            /* SAVE_MAGIC */
    u32       version;          /* SAVE_VERSION */
    s32       time_offset_s;    /* Seconds offset from hardware RTC time-of-day */
    s32       date_offset_days; /* Days offset from hardware RTC date */
    u8        date_format;      /* DateFormat enum stored as u8 */
    u8        reserved[23];     /* Pad to 40 bytes, reserved for future alarm data */
} SaveData;                     /* 40 bytes */

bool save_exists(void);
bool save_read(SaveData* out);
bool save_write(const SaveData* data);
void save_reset(SaveData* data);
void save_init_default(SaveData* data);
void save_ensure_dir(void);
