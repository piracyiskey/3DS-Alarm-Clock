#pragma once
#include <3ds/types.h>
#include <stdbool.h>

#define AUDIO_SAMPLERATE       22050
#define AUDIO_SAMPLES_PER_BUF  (AUDIO_SAMPLERATE / 30)  /* ~735 samples */
#define AUDIO_BYTES_PER_SAMPLE 4  /* stereo PCM16 */
#define AUDIO_MAX_USER_TONES   16
#define AUDIO_MAX_DECODE_BYTES (4 * 1024 * 1024)  /* 4MB cap */

typedef enum {
    RINGTONE_SRC_BUILTIN,   /* Embedded MP3 blob */
    RINGTONE_SRC_SD_MP3     /* MP3 file on SD card */
} RingtoneSrc;

typedef struct {
    char        name[32];
    RingtoneSrc source;
    
    /* Built-in fields: */
    const u8*   mp3_data;      /* Pointer to embedded MP3 data (NULL for SD) */
    u32         mp3_size;      /* Size in bytes (0 for SD) */
    
    /* SD fields: */
    char        sd_path[128];  /* Full sdmc:/ path (empty for built-in) */
} RingtoneInfo;

void audio_init(void);
void audio_exit(void);
void audio_scan_sd_ringtones(void);
int  audio_get_ringtone_count(void);
const char* audio_get_ringtone_name(int index);
void audio_play(u8 ringtone_id);
void audio_stop(void);
void audio_tick(void);
bool audio_is_playing(void);
