#include "audio.h"
#include <3ds.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>

#define DR_MP3_IMPLEMENTATION
#include "dr_mp3.h"

extern const u8 default_alarm_bin[];
extern const u8 default_alarm_bin_end[];
extern const u8 lofi_bin[];
extern const u8 lofi_bin_end[];

static RingtoneInfo ringtones[2 + AUDIO_MAX_USER_TONES];
static int ringtone_count = 0;

static bool is_playing = false;
static u32 playback_offset = 0;

static ndspWaveBuf waveBuf[2];
static void* streamBuf = NULL;

static drmp3_int16* decoded_buf = NULL;
static u32 decoded_size = 0;       /* in bytes */

void audio_scan_sd_ringtones(void) {
    ringtone_count = 0;
    
    strncpy(ringtones[ringtone_count].name, "Default Alarm", 31);
    ringtones[ringtone_count].name[31] = '\0';
    ringtones[ringtone_count].source = RINGTONE_SRC_BUILTIN;
    ringtones[ringtone_count].mp3_data = default_alarm_bin;
    ringtones[ringtone_count].mp3_size = (u32)(default_alarm_bin_end - default_alarm_bin);
    ringtone_count++;
    
    strncpy(ringtones[ringtone_count].name, "Lo-Fi", 31);
    ringtones[ringtone_count].name[31] = '\0';
    ringtones[ringtone_count].source = RINGTONE_SRC_BUILTIN;
    ringtones[ringtone_count].mp3_data = lofi_bin;
    ringtones[ringtone_count].mp3_size = (u32)(lofi_bin_end - lofi_bin);
    ringtone_count++;
    
    DIR* dir = opendir("sdmc:/3ds/3ds-clock/ringtones");
    if (dir) {
        struct dirent* ent;
        while ((ent = readdir(dir)) != NULL && ringtone_count < 2 + AUDIO_MAX_USER_TONES) {
            int len = strlen(ent->d_name);
            if (len > 4 && strcasecmp(ent->d_name + len - 4, ".mp3") == 0) {
                snprintf(ringtones[ringtone_count].sd_path, sizeof(ringtones[ringtone_count].sd_path), "sdmc:/3ds/3ds-clock/ringtones/%s", ent->d_name);
                ringtones[ringtone_count].source = RINGTONE_SRC_SD_MP3;
                ringtones[ringtone_count].mp3_data = NULL;
                ringtones[ringtone_count].mp3_size = 0;
                
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

void audio_init(void) {
    ndspInit();
    ndspSetOutputMode(NDSP_OUTPUT_STEREO);
    ndspChnSetInterp(0, NDSP_INTERP_LINEAR);
    ndspChnSetRate(0, AUDIO_SAMPLERATE);
    ndspChnSetFormat(0, NDSP_FORMAT_STEREO_PCM16);
    
    float mix[12];
    memset(mix, 0, sizeof(mix));
    mix[0] = 1.0;
    mix[1] = 1.0;
    ndspChnSetMix(0, mix);
    
    streamBuf = linearAlloc(AUDIO_SAMPLES_PER_BUF * AUDIO_BYTES_PER_SAMPLE * 2);
    
    memset(waveBuf, 0, sizeof(waveBuf));
    waveBuf[0].data_vaddr = streamBuf;
    waveBuf[0].nsamples = AUDIO_SAMPLES_PER_BUF;
    waveBuf[1].data_vaddr = (u8*)streamBuf + (AUDIO_SAMPLES_PER_BUF * AUDIO_BYTES_PER_SAMPLE);
    waveBuf[1].nsamples = AUDIO_SAMPLES_PER_BUF;
    
    /* Ensure ringtones folder exists on SD before scanning */
    mkdir("sdmc:/3ds", 0777);
    mkdir("sdmc:/3ds/3ds-clock", 0777);
    mkdir("sdmc:/3ds/3ds-clock/ringtones", 0777);
    
    audio_scan_sd_ringtones();
}

static u32 current_channels = 2;

void audio_play(u8 ringtone_id) {
    audio_stop();
    
    if (ringtone_id >= ringtone_count) ringtone_id = 0;
    
    RingtoneInfo* info = &ringtones[ringtone_id];
    
    /* Heap-allocate drmp3 — the struct is ~30KB+ which overflows the 3DS stack */
    drmp3* mp3 = (drmp3*)malloc(sizeof(drmp3));
    if (!mp3) {
        if (ringtone_id != 0) audio_play(0);
        return;
    }
    
    drmp3_bool32 ok;
    if (info->source == RINGTONE_SRC_SD_MP3) {
        ok = drmp3_init_file(mp3, info->sd_path, NULL);
    } else {
        ok = drmp3_init_memory(mp3, info->mp3_data, info->mp3_size, NULL);
    }
    
    if (!ok) {
        free(mp3);
        if (ringtone_id != 0) audio_play(0);
        return;
    }
    
    current_channels = mp3->channels;
    
    /* Read up to AUDIO_MAX_DECODE_BYTES into a heap buffer */
    drmp3_uint64 total_frames = drmp3_get_pcm_frame_count(mp3);
    if (total_frames == 0 || mp3->channels == 0) {
        drmp3_uninit(mp3);
        free(mp3);
        if (ringtone_id != 0) audio_play(0);
        return;
    }
    
    drmp3_uint64 bytes_per_frame = mp3->channels * sizeof(drmp3_int16);
    drmp3_uint64 max_frames = AUDIO_MAX_DECODE_BYTES / bytes_per_frame;
    drmp3_uint64 frames_to_read = (total_frames > max_frames) ? max_frames : total_frames;
    
    u32 alloc_size = (u32)(frames_to_read * bytes_per_frame);
    decoded_buf = (drmp3_int16*)malloc(alloc_size);
    if (!decoded_buf) {
        drmp3_uninit(mp3);
        free(mp3);
        if (ringtone_id != 0) audio_play(0);
        return;
    }
    
    drmp3_uint64 frames_read = drmp3_read_pcm_frames_s16(mp3, frames_to_read, decoded_buf);
    decoded_size = (u32)(frames_read * bytes_per_frame);
    
    u32 sample_rate = mp3->sampleRate;
    drmp3_uninit(mp3);
    free(mp3);
    
    if (decoded_size == 0) {
        free(decoded_buf);
        decoded_buf = NULL;
        if (ringtone_id != 0) audio_play(0);
        return;
    }
    
    ndspChnSetRate(0, sample_rate);
    ndspChnSetFormat(0, current_channels == 2 ? NDSP_FORMAT_STEREO_PCM16 : NDSP_FORMAT_MONO_PCM16);
    
    playback_offset = 0;
    
    for (int i = 0; i < 2; i++) {
        int bytes_to_copy = AUDIO_SAMPLES_PER_BUF * current_channels * sizeof(s16);
        int remaining = decoded_size - playback_offset;
        
        if (bytes_to_copy > remaining) {
            memcpy((void*)waveBuf[i].data_vaddr, (u8*)decoded_buf + playback_offset, remaining);
            memcpy((void*)((u8*)waveBuf[i].data_vaddr + remaining), decoded_buf, bytes_to_copy - remaining);
            playback_offset = bytes_to_copy - remaining;
        } else {
            memcpy((void*)waveBuf[i].data_vaddr, (u8*)decoded_buf + playback_offset, bytes_to_copy);
            playback_offset += bytes_to_copy;
            if (playback_offset >= decoded_size) playback_offset = 0;
        }
        
        DSP_FlushDataCache(waveBuf[i].data_vaddr, bytes_to_copy);
        ndspChnWaveBufAdd(0, &waveBuf[i]);
    }
    
    is_playing = true;
}

void audio_tick(void) {
    if (!is_playing || !decoded_buf) return;
    
    int bytes_per_sample = current_channels * sizeof(s16);
    int chunk_size = AUDIO_SAMPLES_PER_BUF * bytes_per_sample;
    
    for (int i = 0; i < 2; i++) {
        if (waveBuf[i].status == NDSP_WBUF_DONE) {
            int remaining = decoded_size - playback_offset;
            
            if (chunk_size > remaining) {
                memcpy((void*)waveBuf[i].data_vaddr, (u8*)decoded_buf + playback_offset, remaining);
                memcpy((void*)((u8*)waveBuf[i].data_vaddr + remaining), decoded_buf, chunk_size - remaining);
                playback_offset = chunk_size - remaining;
            } else {
                memcpy((void*)waveBuf[i].data_vaddr, (u8*)decoded_buf + playback_offset, chunk_size);
                playback_offset += chunk_size;
                if (playback_offset >= decoded_size) playback_offset = 0;
            }
            
            DSP_FlushDataCache(waveBuf[i].data_vaddr, chunk_size);
            ndspChnWaveBufAdd(0, &waveBuf[i]);
        }
    }
}

void audio_stop(void) {
    if (!is_playing) return;
    
    ndspChnReset(0);
    ndspChnWaveBufClear(0);
    
    if (decoded_buf) {
        free(decoded_buf);
        decoded_buf = NULL;
    }
    
    is_playing = false;
}

bool audio_is_playing(void) {
    return is_playing;
}

void audio_exit(void) {
    audio_stop();
    if (streamBuf) {
        linearFree(streamBuf);
        streamBuf = NULL;
    }
    ndspExit();
}
