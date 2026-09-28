#include "audio.h"
#include <3ds.h>
#include <mpg123.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>
#include <strings.h>

/* --- Audio Streaming Configuration --- */
#define AUDIO_NUM_BUFFERS       3       /* Triple buffering for zero underrun */
#define AUDIO_SAMPLES_PER_BUF   4096    /* ~93ms @ 44.1kHz, ~186ms @ 22.05kHz */
#define AUDIO_MAX_CHANNELS      2       /* Stereo */
#define AUDIO_BYTES_PER_FRAME   (AUDIO_MAX_CHANNELS * sizeof(s16)) /* 4 bytes */
#define AUDIO_BUF_SIZE_BYTES    (AUDIO_SAMPLES_PER_BUF * AUDIO_BYTES_PER_FRAME) /* 16384 bytes, 32-byte aligned */
#define AUDIO_THREAD_STACK_SZ   (32 * 1024) /* 32KB stack for audio worker thread */

/* Embedded binary PCM16 blobs (converted via ffmpeg at build time) */
extern const u8 default_alarm_bin[];
extern const u8 default_alarm_bin_end[];
extern const u8 lofi_bin[];
extern const u8 lofi_bin_end[];
extern const u8 timer_bin[];
extern const u8 timer_bin_end[];
extern const u8 digital_clock_bin[];
extern const u8 digital_clock_bin_end[];
extern const u8 xmas_bin[];
extern const u8 xmas_bin_end[];

/* Registry of available ringtones */
static RingtoneInfo ringtones[AUDIO_NUM_BUILTIN_TONES + AUDIO_MAX_USER_TONES];
static int ringtone_count = 0;

/* Dedicated Timer Ringtone */
static RingtoneInfo  s_timer_tone;
static bool          s_is_timer_active    = false;

/* Playback engine state */
static u8            s_active_ringtone_id = 0;
static volatile bool s_is_playing         = false;
static volatile bool s_is_looping         = true;
static volatile bool s_eof_reached        = false;
static volatile bool s_audio_suspended     = false;
static volatile bool s_audio_quit         = false;
static float         s_volume             = 1.0f;

/* Active stream parameters */
static u32           s_current_rate       = 22050;
static u32           s_current_channels   = 2;
static u32           s_pcm_offset         = 0;

/* Hardware audio buffers & mpg123 */
static ndspWaveBuf   s_waveBuf[AUDIO_NUM_BUFFERS];
static u8*           s_linear_buf         = NULL;
static mpg123_handle* s_mpg               = NULL;

/* Threading & Synchronization */
static Thread        s_audio_thread       = NULL;
static LightEvent    s_audio_event;
static LightLock     s_audio_lock;

/* Forward declarations */
static void refill_and_queue_buffer(int buf_idx);
static void audio_stop_internal(void);

/* NDSP frame interrupt callback: signals the background audio thread */
static void ndsp_callback(void* arg) {
    (void)arg;
    if (s_audio_quit) return;
    LightEvent_Signal(&s_audio_event);
}

/* Background audio decoding thread: refills buffers whenever NDSP marks them done */
static void audio_thread_entry(void* arg) {
    (void)arg;
    while (!s_audio_quit) {
        LightLock_Lock(&s_audio_lock);
        if (s_is_playing && !s_audio_suspended) {
            bool any_busy = false;
            for (int i = 0; i < AUDIO_NUM_BUFFERS; i++) {
                if (s_waveBuf[i].status == NDSP_WBUF_DONE) {
                    if (!s_eof_reached) {
                        refill_and_queue_buffer(i);
                        if (s_waveBuf[i].status != NDSP_WBUF_DONE) {
                            any_busy = true;
                        }
                    }
                } else {
                    any_busy = true;
                }
            }
            if (!s_is_looping && s_eof_reached && !any_busy) {
                audio_stop_internal();
            }
        }
        LightLock_Unlock(&s_audio_lock);

        /* Sleep cooperatively until NDSP signals or audio_play/stop pings */
        LightEvent_Wait(&s_audio_event);
    }
}

/* Refill a single wave buffer and submit to NDSP */
static void refill_and_queue_buffer(int buf_idx) {
    RingtoneInfo* info = s_is_timer_active ? &s_timer_tone : &ringtones[s_active_ringtone_id];
    u32 frame_size = s_current_channels * sizeof(s16);
    u32 target_bytes = AUDIO_SAMPLES_PER_BUF * frame_size;
    u8* dest = (u8*)s_waveBuf[buf_idx].data_vaddr;
    u32 filled = 0;

    if (info->source == RINGTONE_SRC_SD_MP3) {
        if (!s_mpg) return;

        while (filled < target_bytes) {
            size_t bytes_read = 0;
            int err = mpg123_read(s_mpg, dest + filled, target_bytes - filled, &bytes_read);
            filled += (u32)bytes_read;

            if (err == MPG123_DONE || bytes_read == 0) {
                if (s_is_looping) {
                    /* End of MP3 file reached: loop back to sample frame 0 */
                    if (mpg123_seek(s_mpg, 0, SEEK_SET) < 0) {
                        break;
                    }
                } else {
                    s_eof_reached = true;
                    break;
                }
            } else if (err != MPG123_OK && err != MPG123_NEW_FORMAT) {
                if (!s_is_looping) s_eof_reached = true;
                break;
            }
        }
    } else {
        /* Built-in PCM16 streaming from .rodata */
        while (filled < target_bytes) {
            u32 remaining_in_pcm = info->pcm_size - s_pcm_offset;
            u32 need = target_bytes - filled;
            u32 to_copy = (need < remaining_in_pcm) ? need : remaining_in_pcm;

            /* Strict sample frame alignment to prevent 1-byte channel/byte desync */
            to_copy -= (to_copy % frame_size);
            if (to_copy == 0) {
                if (s_is_looping) {
                    s_pcm_offset = 0;
                    continue;
                } else {
                    s_eof_reached = true;
                    break;
                }
            }

            memcpy(dest + filled, info->pcm_data + s_pcm_offset, to_copy);
            filled += to_copy;
            s_pcm_offset += to_copy;

            if (s_pcm_offset >= info->pcm_size) {
                if (s_is_looping) {
                    s_pcm_offset = 0; /* Clean loop wrap */
                } else {
                    s_eof_reached = true;
                    break;
                }
            }
        }
    }

    if (filled == 0) {
        s_waveBuf[buf_idx].nsamples = 0;
        return;
    }

    if (filled < target_bytes) {
        memset(dest + filled, 0, target_bytes - filled);
    }

    s_waveBuf[buf_idx].nsamples = filled / frame_size;

    /* Flush ARM11 L1 data cache using 32-byte rounded length for DSP DMA coherence */
    DSP_FlushDataCache(s_waveBuf[buf_idx].data_vaddr, (filled + 31) & ~31);
    ndspChnWaveBufAdd(0, &s_waveBuf[buf_idx]);
}

void audio_scan_sd_ringtones(void) {
    ringtone_count = 0;

    /* Slot 0: Default Alarm (Embedded PCM16) */
    strncpy(ringtones[ringtone_count].name, "Default Alarm", 31);
    ringtones[ringtone_count].name[31] = '\0';
    ringtones[ringtone_count].source = RINGTONE_SRC_BUILTIN;
    ringtones[ringtone_count].pcm_data = default_alarm_bin;
    ringtones[ringtone_count].pcm_size = (u32)(default_alarm_bin_end - default_alarm_bin);
    ringtones[ringtone_count].sample_rate = 22050;
    ringtones[ringtone_count].channels = 2;
    ringtones[ringtone_count].sd_path[0] = '\0';
    ringtone_count++;

    /* Slot 1: Lo-Fi (Embedded PCM16) */
    strncpy(ringtones[ringtone_count].name, "Lo-Fi", 31);
    ringtones[ringtone_count].name[31] = '\0';
    ringtones[ringtone_count].source = RINGTONE_SRC_BUILTIN;
    ringtones[ringtone_count].pcm_data = lofi_bin;
    ringtones[ringtone_count].pcm_size = (u32)(lofi_bin_end - lofi_bin);
    ringtones[ringtone_count].sample_rate = 22050;
    ringtones[ringtone_count].channels = 2;
    ringtones[ringtone_count].sd_path[0] = '\0';
    ringtone_count++;

    /* Slot 2: Digital Clock (Embedded PCM16) */
    strncpy(ringtones[ringtone_count].name, "Digital Clock", 31);
    ringtones[ringtone_count].name[31] = '\0';
    ringtones[ringtone_count].source = RINGTONE_SRC_BUILTIN;
    ringtones[ringtone_count].pcm_data = digital_clock_bin;
    ringtones[ringtone_count].pcm_size = (u32)(digital_clock_bin_end - digital_clock_bin);
    ringtones[ringtone_count].sample_rate = 22050;
    ringtones[ringtone_count].channels = 2;
    ringtones[ringtone_count].sd_path[0] = '\0';
    ringtone_count++;

    /* Slot 3: Christmas (Embedded PCM16) */
    strncpy(ringtones[ringtone_count].name, "Christmas", 31);
    ringtones[ringtone_count].name[31] = '\0';
    ringtones[ringtone_count].source = RINGTONE_SRC_BUILTIN;
    ringtones[ringtone_count].pcm_data = xmas_bin;
    ringtones[ringtone_count].pcm_size = (u32)(xmas_bin_end - xmas_bin);
    ringtones[ringtone_count].sample_rate = 22050;
    ringtones[ringtone_count].channels = 2;
    ringtones[ringtone_count].sd_path[0] = '\0';
    ringtone_count++;

    /* Slot 4..19: User SD Card MP3 Ringtones */
    DIR* dir = opendir("sdmc:/3ds/3ds-clock/ringtones");
    if (dir) {
        struct dirent* ent;
        while ((ent = readdir(dir)) != NULL && ringtone_count < AUDIO_NUM_BUILTIN_TONES + AUDIO_MAX_USER_TONES) {
            int len = strlen(ent->d_name);
            if (len > 4 && strcasecmp(ent->d_name + len - 4, ".mp3") == 0) {
                snprintf(ringtones[ringtone_count].sd_path, sizeof(ringtones[ringtone_count].sd_path),
                         "sdmc:/3ds/3ds-clock/ringtones/%s", ent->d_name);
                ringtones[ringtone_count].source = RINGTONE_SRC_SD_MP3;
                ringtones[ringtone_count].pcm_data = NULL;
                ringtones[ringtone_count].pcm_size = 0;
                ringtones[ringtone_count].sample_rate = 44100;
                ringtones[ringtone_count].channels = 2;

                snprintf(ringtones[ringtone_count].name, sizeof(ringtones[ringtone_count].name), "%s", ent->d_name);
                char* dot = strrchr(ringtones[ringtone_count].name, '.');
                if (dot) *dot = '\0';

                ringtone_count++;
            }
        }
        closedir(dir);
    }
}

int audio_get_ringtone_count(void) {
    return ringtone_count;
}

const char* audio_get_ringtone_name(int index) {
    if (index < 0 || index >= ringtone_count) return "Unknown";
    return ringtones[index].name;
}

void audio_set_volume(float volume) {
    if (volume < 0.0f) volume = 0.0f;
    if (volume > 1.0f) volume = 1.0f;
    s_volume = volume;

    float mix[12];
    memset(mix, 0, sizeof(mix));
    mix[0] = s_volume;
    mix[1] = s_volume;
    ndspChnSetMix(0, mix);
}

float audio_get_volume(void) {
    return s_volume;
}

void audio_init(void) {
    ndspInit();
    ndspSetOutputMode(NDSP_OUTPUT_STEREO);
    ndspSetCallback(ndsp_callback, NULL);

    mpg123_init();

    /* Allocate triple ring buffer in 3DS linear memory (strictly 32-byte cache aligned) */
    s_linear_buf = (u8*)linearAlloc(AUDIO_NUM_BUFFERS * AUDIO_BUF_SIZE_BYTES);

    memset(s_waveBuf, 0, sizeof(s_waveBuf));
    for (int i = 0; i < AUDIO_NUM_BUFFERS; i++) {
        s_waveBuf[i].data_vaddr = s_linear_buf + (i * AUDIO_BUF_SIZE_BYTES);
        s_waveBuf[i].status = NDSP_WBUF_DONE;
    }

    LightEvent_Init(&s_audio_event, RESET_ONESHOT);
    LightLock_Init(&s_audio_lock);
    s_audio_quit = false;

    /* Start dedicated audio worker thread */
    s_audio_thread = threadCreate(audio_thread_entry, NULL, AUDIO_THREAD_STACK_SZ, 0x18, -1, false);

    /* Ensure standard ringtones directory exists on SD */
    mkdir("sdmc:/3ds", 0777);
    mkdir("sdmc:/3ds/3ds-clock", 0777);
    mkdir("sdmc:/3ds/3ds-clock/ringtones", 0777);

    /* Dedicated Timer Tone Setup */
    strncpy(s_timer_tone.name, "Timer", 31);
    s_timer_tone.name[31] = '\0';
    s_timer_tone.source = RINGTONE_SRC_BUILTIN;
    s_timer_tone.pcm_data = timer_bin;
    s_timer_tone.pcm_size = (u32)(timer_bin_end - timer_bin);
    s_timer_tone.sample_rate = 22050;
    s_timer_tone.channels = 2;
    s_timer_tone.sd_path[0] = '\0';

    audio_scan_sd_ringtones();
}

static void audio_stop_internal(void) {
    if (!s_is_playing && !s_mpg && !s_is_timer_active) return;

    s_is_playing = false;
    s_is_timer_active = false;
    s_is_looping = true;
    s_eof_reached = false;

    ndspChnReset(0);
    ndspChnWaveBufClear(0);

    for (int i = 0; i < AUDIO_NUM_BUFFERS; i++) {
        s_waveBuf[i].status = NDSP_WBUF_DONE;
    }

    if (s_mpg) {
        mpg123_close(s_mpg);
    }
}

void audio_stop(void) {
    LightLock_Lock(&s_audio_lock);
    audio_stop_internal();
    LightLock_Unlock(&s_audio_lock);
}

static void audio_play_internal(u8 ringtone_id, bool loop) {
    LightLock_Lock(&s_audio_lock);

    audio_stop_internal();
    s_is_timer_active = false;
    s_is_looping = loop;
    s_eof_reached = false;

    if (ringtone_id >= ringtone_count) {
        ringtone_id = 0;
    }
    s_active_ringtone_id = ringtone_id;
    RingtoneInfo* info = &ringtones[ringtone_id];

    if (info->source == RINGTONE_SRC_SD_MP3) {
        int err = MPG123_OK;
        if (!s_mpg) {
            s_mpg = mpg123_new(NULL, &err);
        }
        if (!s_mpg || mpg123_open(s_mpg, info->sd_path) != MPG123_OK) {
            LightLock_Unlock(&s_audio_lock);
            if (ringtone_id != 0) audio_play_internal(0, loop);
            return;
        }

        long rate = 0;
        int channels = 0, encoding = 0;
        if (mpg123_getformat(s_mpg, &rate, &channels, &encoding) != MPG123_OK) {
            mpg123_close(s_mpg);
            LightLock_Unlock(&s_audio_lock);
            if (ringtone_id != 0) audio_play_internal(0, loop);
            return;
        }

        /* Force signed 16-bit PCM output format */
        mpg123_format_none(s_mpg);
        mpg123_format(s_mpg, rate, channels, MPG123_ENC_SIGNED_16);

        s_current_rate = (u32)rate;
        s_current_channels = (u32)channels;
    } else {
        s_current_rate = info->sample_rate;
        s_current_channels = info->channels;
        s_pcm_offset = 0;
    }

    /* Configure NDSP Channel 0 */
    ndspChnReset(0);
    ndspChnSetInterp(0, NDSP_INTERP_POLYPHASE);
    ndspChnSetRate(0, (float)s_current_rate);
    ndspChnSetFormat(0, (s_current_channels == 1) ? NDSP_FORMAT_MONO_PCM16 : NDSP_FORMAT_STEREO_PCM16);
    audio_set_volume(s_volume);

    /* Prime and queue initial wave buffers */
    for (int i = 0; i < AUDIO_NUM_BUFFERS; i++) {
        s_waveBuf[i].status = NDSP_WBUF_DONE;
        if (!s_eof_reached) {
            refill_and_queue_buffer(i);
        }
    }

    s_is_playing = true;
    LightLock_Unlock(&s_audio_lock);
}

void audio_play(u8 ringtone_id) {
    audio_play_internal(ringtone_id, true);
}

void audio_play_preview(u8 ringtone_id) {
    audio_play_internal(ringtone_id, false);
}

void audio_play_timer(void) {
    LightLock_Lock(&s_audio_lock);

    audio_stop_internal();
    s_is_timer_active = true;
    s_is_looping = true;
    s_eof_reached = false;

    s_current_rate = s_timer_tone.sample_rate;
    s_current_channels = s_timer_tone.channels;
    s_pcm_offset = 0;

    /* Configure NDSP Channel 0 */
    ndspChnReset(0);
    ndspChnSetInterp(0, NDSP_INTERP_POLYPHASE);
    ndspChnSetRate(0, (float)s_current_rate);
    ndspChnSetFormat(0, (s_current_channels == 1) ? NDSP_FORMAT_MONO_PCM16 : NDSP_FORMAT_STEREO_PCM16);
    audio_set_volume(s_volume);

    /* Prime and queue initial wave buffers */
    for (int i = 0; i < AUDIO_NUM_BUFFERS; i++) {
        s_waveBuf[i].status = NDSP_WBUF_DONE;
        if (!s_eof_reached) {
            refill_and_queue_buffer(i);
        }
    }

    s_is_playing = true;
    LightLock_Unlock(&s_audio_lock);
}

void audio_tick(void) {
    /* Lightweight periodic watchdog to wake audio thread if any buffer is pending */
    if (s_is_playing) {
        LightEvent_Signal(&s_audio_event);
    }
}

bool audio_is_playing(void) {
    return s_is_playing;
}

void audio_suspend(void) {
    LightLock_Lock(&s_audio_lock);
    s_audio_suspended = true;
    if (s_is_playing) {
        ndspChnReset(0);
    }
    LightLock_Unlock(&s_audio_lock);
}

void audio_resume(void) {
    LightLock_Lock(&s_audio_lock);
    s_audio_suspended = false;
    if (s_is_playing) {
        ndspChnReset(0);
        ndspChnSetInterp(0, NDSP_INTERP_POLYPHASE);
        ndspChnSetRate(0, (float)s_current_rate);
        ndspChnSetFormat(0, (s_current_channels == 1) ? NDSP_FORMAT_MONO_PCM16 : NDSP_FORMAT_STEREO_PCM16);
        audio_set_volume(s_volume);

        for (int i = 0; i < AUDIO_NUM_BUFFERS; i++) {
            s_waveBuf[i].status = NDSP_WBUF_DONE;
            refill_and_queue_buffer(i);
        }
    }
    LightLock_Unlock(&s_audio_lock);
    LightEvent_Signal(&s_audio_event);
}

void audio_exit(void) {
    audio_stop();

    /* Gracefully terminate worker thread */
    if (s_audio_thread) {
        s_audio_quit = true;
        LightEvent_Signal(&s_audio_event);
        threadJoin(s_audio_thread, U64_MAX);
        threadFree(s_audio_thread);
        s_audio_thread = NULL;
    }

    if (s_mpg) {
        mpg123_delete(s_mpg);
        s_mpg = NULL;
    }

    mpg123_exit();

    if (s_linear_buf) {
        linearFree(s_linear_buf);
        s_linear_buf = NULL;
    }

    ndspExit();
}
