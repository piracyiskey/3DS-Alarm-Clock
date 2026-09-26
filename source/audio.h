#pragma once
#include <3ds/types.h>
#include <stdbool.h>

#define AUDIO_MAX_USER_TONES 16

typedef enum {
    RINGTONE_SRC_BUILTIN,   /* Embedded PCM16 blob */
    RINGTONE_SRC_SD_MP3     /* MP3 file on SD card */
} RingtoneSrc;

typedef struct {
    char        name[32];
    RingtoneSrc source;
    
    /* Built-in fields: */
    const u8*   pcm_data;      /* Pointer to embedded PCM16 data */
    u32         pcm_size;      /* Size in bytes */
    u32         sample_rate;   /* 22050 or 44100 */
    u32         channels;      /* 1 = mono, 2 = stereo */
    
    /* SD fields: */
    char        sd_path[128];  /* Full sdmc:/ path */
} RingtoneInfo;

/* Lifecycle & Initialization */
void        audio_init(void);
void        audio_exit(void);

/* Ringtone Directory Scanning & Selection */
void        audio_scan_sd_ringtones(void);
int         audio_get_ringtone_count(void);
const char* audio_get_ringtone_name(int index);

/* Playback Control */
void        audio_play(u8 ringtone_id);
void        audio_play_timer(void);
void        audio_stop(void);
void        audio_tick(void);
bool        audio_is_playing(void);

/* Volume & Mixing (0.0f = silent, 1.0f = full volume) */
void        audio_set_volume(float volume);
float       audio_get_volume(void);

/* Suspend & Resume for APT transitions (e.g. HOME Menu) */
void        audio_suspend(void);
void        audio_resume(void);
