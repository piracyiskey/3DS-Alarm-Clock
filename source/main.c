#include <3ds.h>
#include <3ds/services/gsplcd.h>
#include <3ds/applets/swkbd.h>
#include <citro3d.h>
#include <citro2d.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>

#include "clock.h"
#include "save.h"
#include "ui.h"
#include "stopwatch.h"
#include "timer.h"
#include "alarm.h"
#include "audio.h"
#include "world_clock.h"
#include "manual.h"
#include "about.h"
/* Binary assets generated from gfx/icons.t3s */
extern const u8 icons_t3x[];
extern const u8 icons_t3x_end[];
#define icons_t3x_size     ((u32)(icons_t3x_end - icons_t3x))
#include "icons.h"

#define SOC_ALIGN      0x1000
#define SOC_BUFFERSIZE 0x100000
static u32* SOC_buffer = NULL;

static void socShutdown(void) { socExit(); }

/* --- Display & Power Management --- */
static DisplayPowerMode s_screen_mode = SCREEN_MODE_ALL_ON;
static u32 s_lr_debounce = 0;

static void lcd_cleanup(void)
{
    if (s_screen_mode != SCREEN_MODE_ALL_ON) {
        if (R_SUCCEEDED(gspLcdInit())) {
            GSPLCD_PowerOnBacklight(GSPLCD_SCREEN_BOTH);
            gspLcdExit();
        }
        s_screen_mode = SCREEN_MODE_ALL_ON;
    }
}

static void set_screen_mode(DisplayPowerMode new_mode)
{
    if (new_mode == s_screen_mode) return;

    if (R_SUCCEEDED(gspLcdInit())) {
        switch (new_mode) {
        case SCREEN_MODE_ALL_ON:
            GSPLCD_PowerOnBacklight(GSPLCD_SCREEN_BOTH);
            break;
        case SCREEN_MODE_BOTTOM_OFF:
            GSPLCD_PowerOnBacklight(GSPLCD_SCREEN_TOP);
            GSPLCD_PowerOffBacklight(GSPLCD_SCREEN_BOTTOM);
            break;
        case SCREEN_MODE_ALL_OFF:
            GSPLCD_PowerOffBacklight(GSPLCD_SCREEN_BOTH);
            break;
        }
        gspLcdExit();
    }
    s_screen_mode = new_mode;
}

/* --- Telemetry Services (PTMU / MCUHWC) --- */
static bool ptmu_ok = false;

static void telemetry_exit(void)
{
    if (ptmu_ok) { ptmuExit(); ptmu_ok = false; }
}

static u8   telemetry_wifi_bars = 0;
static u8   telemetry_battery_percent = 100;
static bool telemetry_is_charging = false;
static u32  telemetry_poll_counter = 0;

static void telemetry_update(bool force)
{
    telemetry_poll_counter++;
    if (!force && telemetry_poll_counter < 60) return;
    telemetry_poll_counter = 0;

    /* 1. Wi-Fi signal strength (0..3) */
    telemetry_wifi_bars = osGetWifiStrength();

    /* 2. Battery percentage: MCUHWC with PTMU fallback.
     * Open mcu::HWC on-demand only for the read and close immediately
     * so that the single-session MCU service port is never held open. */
    bool got_percent = false;
    if (R_SUCCEEDED(mcuHwcInit())) {
        u8 level = 0;
        if (R_SUCCEEDED(MCUHWC_GetBatteryLevel(&level))) {
            telemetry_battery_percent = (level > 100) ? 100 : level;
            got_percent = true;
        }
        mcuHwcExit();
    }

    if (!got_percent && ptmu_ok) {
        u8 ptm_level = 0;
        if (R_SUCCEEDED(PTMU_GetBatteryLevel(&ptm_level))) {
            switch (ptm_level) {
            case 0:  telemetry_battery_percent = 5;   break;
            case 1:  telemetry_battery_percent = 20;  break;
            case 2:  telemetry_battery_percent = 40;  break;
            case 3:  telemetry_battery_percent = 70;  break;
            case 4:  telemetry_battery_percent = 90;  break;
            case 5:
            default: telemetry_battery_percent = 100; break;
            }
        }
    }

    /* 3. Charging status via PTMU */
    if (ptmu_ok) {
        u8 charge_state = 0;
        if (R_SUCCEEDED(PTMU_GetBatteryChargeState(&charge_state))) {
            telemetry_is_charging = (charge_state != 0);
        }
    }

    /* 4. Headphone-aware sleep policy:
     * When headphones/AUX are connected to the 3.5mm jack, the hardware headphone amp
     * remains powered even when the clamshell lid is closed. Disallowing sleep allows
     * the alarm to continue ticking and ring through headphones/AUX with the lid closed.
     * When unplugged, allow normal sleep to prevent silent battery drain into disconnected internal speakers. */
    bool headphones_inserted = false;
    if (R_SUCCEEDED(DSP_GetHeadphoneStatus(&headphones_inserted))) {
        aptSetSleepAllowed(!headphones_inserted);
    }
}


static aptHookCookie apt_cookie;

static void apt_hook_callback(APT_HookType hook, void* param) {
    (void)param;
    if (hook == APTHOOK_ONSUSPEND) {
        /* App is being suspended (e.g. HOME button or sleep).
         * 1. Flush any pending background save so disk I/O settles cleanly. */
        save_flush();

        /* 2. Suspend audio so NDSP/DSP operations pause cleanly before DSP sleep. */
        audio_suspend();

        /* 3. Turn both LCD backlights ON so Home Menu is fully visible and usable. */
        if (s_screen_mode != SCREEN_MODE_ALL_ON) {
            if (R_SUCCEEDED(gspLcdInit())) {
                GSPLCD_PowerOnBacklight(GSPLCD_SCREEN_BOTH);
                gspLcdExit();
            }
        }
    } else if (hook == APTHOOK_ONRESTORE || hook == APTHOOK_ONWAKEUP) {
        /* App is being restored from Home Menu or waking from sleep.
         * 1. Restore LCD backlight mode matching current s_screen_mode. */
        if (s_screen_mode != SCREEN_MODE_ALL_ON) {
            if (R_SUCCEEDED(gspLcdInit())) {
                if (s_screen_mode == SCREEN_MODE_BOTTOM_OFF) {
                    GSPLCD_PowerOnBacklight(GSPLCD_SCREEN_TOP);
                    GSPLCD_PowerOffBacklight(GSPLCD_SCREEN_BOTTOM);
                } else if (s_screen_mode == SCREEN_MODE_ALL_OFF) {
                    GSPLCD_PowerOffBacklight(GSPLCD_SCREEN_BOTH);
                }
                gspLcdExit();
            }
        } else {
            if (R_SUCCEEDED(gspLcdInit())) {
                GSPLCD_PowerOnBacklight(GSPLCD_SCREEN_BOTH);
                gspLcdExit();
            }
        }

        /* 2. Safely resume audio playback if an alarm/timer was active. */
        audio_resume();
    }
}

/* Settings sub-states */
typedef enum {
    SET_MAIN,
    SET_TIME_DATE_MENU,
    SET_EDIT_TIME,
    SET_EDIT_DATE,
    SET_CONFIRM_RESET,
    SET_CONFIRM_RESET_TIME,
    SET_SAVE_OK,
    SET_DISPLAY,
    SET_MANUAL,
    SET_ABOUT
} SettingsSubState;

#define TOUCH_SLOP_PX 8.0f

/* Hold-repeat helper for touch arrows and D-pad */
typedef struct {
    u32  frames;
    bool triggered;
} HoldRepeat;

static bool touch_hit(u16 px, u16 py, const HitRect* r)
{
    return px >= r->x && px < r->x + r->w &&
           py >= r->y && py < r->y + r->h;
}

static void hold_repeat_update_ex(HoldRepeat* hr, bool held, u32 delay, u32 interval)
{
    hr->triggered = false;
    if (!held) { hr->frames = 0; return; }

    if (hr->frames == 0)
        hr->triggered = true;                           /* first press */
    else if (hr->frames >= delay && (hr->frames - delay) % interval == 0)
        hr->triggered = true;                           /* repeat */

    hr->frames++;
}

static void hold_repeat_update(HoldRepeat* hr, bool held)
{
    hold_repeat_update_ex(hr, held, 30, 6);
}

static void step_year(int* y, int* m, int* d, int delta)
{
    *y += delta;
    if (*y < 1900) *y = 1900;
    if (*y > 2100) *y = 2100;
    int max_d = days_in_month(*y, *m);
    if (*d > max_d) *d = max_d;
}

static void step_month(int* y, int* m, int* d, int delta)
{
    *m += delta;
    if (*m < 1) *m = 12;
    if (*m > 12) *m = 1;
    int max_d = days_in_month(*y, *m);
    if (*d > max_d) *d = max_d;
}

static void step_day(int* y, int* m, int* d, int delta)
{
    int max_d = days_in_month(*y, *m);
    *d += delta;
    if (*d < 1) *d = max_d;
    if (*d > max_d) *d = 1;
}

static void step_date_col(int col, int delta, int* y, int* m, int* d, DateFormat fmt)
{
    if (fmt == DATEFMT_ISO) {
        if (col == 1) step_year(y, m, d, delta);
        else if (col == 2) step_month(y, m, d, delta);
        else if (col == 3) step_day(y, m, d, delta);
    } else if (fmt == DATEFMT_US) {
        if (col == 1) step_month(y, m, d, delta);
        else if (col == 2) step_day(y, m, d, delta);
        else if (col == 3) step_year(y, m, d, delta);
    } else { /* DATEFMT_EUR */
        if (col == 1) step_day(y, m, d, delta);
        else if (col == 2) step_month(y, m, d, delta);
        else if (col == 3) step_year(y, m, d, delta);
    }
}

static void alarm_toggle_entry(SaveData* save, int idx)
{
    if (!save || idx < 0 || idx >= save->alarm_count) return;
    save->alarms[idx].enabled = !save->alarms[idx].enabled;
    if (save->alarms[idx].enabled) {
        s64 now_sec = get_display_time_seconds();
        int y, m, d;
        s64 now_days = now_sec / 86400;
        if ((now_sec % 86400) < 0) now_days--;
        days_to_ymd(now_days, &y, &m, &d);
        s64 today_fire = ymd_to_days(y, m, d) * 86400LL + save->alarms[idx].hour * 3600LL + save->alarms[idx].minute * 60LL;
        if (today_fire <= now_sec) {
            save->alarms[idx].last_fired_epoch = today_fire;
        } else {
            save->alarms[idx].last_fired_epoch = today_fire - 86400LL;
        }
    }
    save_write(save);
}

/* Entry point */
int main(int argc, char* argv[])
{
    /* --- Service init --- */
    gfxInitDefault();
    atexit(gfxExit);

    SOC_buffer = (u32*)memalign(SOC_ALIGN, SOC_BUFFERSIZE);
    if (SOC_buffer && socInit(SOC_buffer, SOC_BUFFERSIZE) == 0) {
        atexit(socShutdown);
        link3dsStdio();
    }

    atexit(lcd_cleanup);

    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
    C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
    C2D_Prepare();
    cfguInit();

    C3D_RenderTarget* top = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
    C3D_RenderTarget* bot = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);
    C2D_TextBuf textBuf   = C2D_TextBufNew(4096);

    /* --- Sprite Sheet Init --- */
    C2D_SpriteSheet sprite_sheet = C2D_SpriteSheetLoadFromMem(icons_t3x, icons_t3x_size);
    C2D_Image settings_icon = { NULL, NULL };
    C2D_Image trash_icon = { NULL, NULL };
    C2D_Image add_icon = { NULL, NULL };
    C2D_Image tab_icons[4] = { { NULL, NULL } };
    if (sprite_sheet) {
        settings_icon = C2D_SpriteSheetGetImage(sprite_sheet, icons_settings_idx);
        trash_icon = C2D_SpriteSheetGetImage(sprite_sheet, icons_trash_2_idx);
        add_icon = C2D_SpriteSheetGetImage(sprite_sheet, icons_add_idx);
        tab_icons[MODE_ALARM]     = C2D_SpriteSheetGetImage(sprite_sheet, icons_alarm_idx);
        tab_icons[MODE_CLOCK]     = C2D_SpriteSheetGetImage(sprite_sheet, icons_clock_idx);
        tab_icons[MODE_STOPWATCH] = C2D_SpriteSheetGetImage(sprite_sheet, icons_stopwatch_idx);
        tab_icons[MODE_TIMER]     = C2D_SpriteSheetGetImage(sprite_sheet, icons_timer_idx);

        if (settings_icon.tex) {
            C3D_TexSetFilter(settings_icon.tex, GPU_LINEAR, GPU_LINEAR);
        }
    }

    audio_init();
    atexit(audio_exit);
    aptSetHomeAllowed(true);
    aptHook(&apt_cookie, apt_hook_callback, NULL);

    ptmu_ok = R_SUCCEEDED(ptmuInit());
    atexit(telemetry_exit);

    /* --- Clock & Save init --- */
    save_init();
    atexit(save_exit);

    SaveData save;
    bool is_first_boot = false;
    int first_boot_frames = 0;

    if (save_read(&save)) {
        clock_init(save.time_offset_s, save.date_offset_days);
    } else {
        save_init_default(&save);
        save_write(&save);
        clock_init(save.time_offset_s, save.date_offset_days);
        is_first_boot = true;
    }

    /* --- Multi-mode state --- */
    AppMode active_mode = MODE_ALARM;
    bool is_settings    = false;
    SettingsSubState settings_sub = SET_MAIN;
    const char* save_msg = "Saved successfully!";

    static Stopwatch sw;
    stopwatch_init(&sw);

    Timer tmr;
    timer_init(&tmr);
    bool timer_ringing = false;
    s64  timer_ring_start_epoch = 0;
    u32  timer_ring_frames = 0;

    AlarmSystem alarm_sys;
    alarm_sys_init(&alarm_sys);
    alarm_check_startup_missed(&save, &alarm_sys, get_display_time_seconds());

    /* Arrow edit state */
    int edit_h = 0, edit_m = 0, edit_s = 0;
    int edit_y = 2026, edit_mo = 1, edit_d = 1;
    DateFormat edit_fmt = DATEFMT_EUR;

    AlarmView alarm_view = ALARM_VIEW_LIST;
    AlarmListState alarm_list_state = {0.0f, 0.0f, 0.0f, false, false, -1, false, false, -1};
    int edit_alarm_idx = -1;
    int edit_alarm_h = 0, edit_alarm_m = 0;
    u8 edit_alarm_repeat = REPEAT_ONCE;
    u8 edit_alarm_tone = 0;
    char edit_alarm_label[ALARM_LABEL_LEN] = "";
    bool show_delete_confirm = false;

    ClockView clock_view = CLOCK_VIEW_LIST;
    WorldClockListState world_clock_state = {0.0f, 0.0f, 0.0f, false, false, -1, false, 0};
    CityPickerState city_picker_state = {0.0f, 0.0f, 0.0f, false, false, -1, 0};
    int confirm_del_city_idx = -1;
    int confirm_home_city_id = -1;
    int alert_home_city_id   = -1;

    int   manual_topic_idx = 0;
    float manual_scroll_y  = 0.0f;
    bool  manual_is_dragging = false;
    float manual_touch_start_y = 0.0f;

    float about_scroll_y = 0.0f;
    bool  about_is_dragging = false;
    float about_touch_start_y = 0.0f;

    static const s64 k_auto_sleep_seconds[8] = {
        0,      /* Never */
        60,     /* 1 min */
        180,    /* 3 min */
        300,    /* 5 min */
        600,    /* 10 min */
        1200,   /* 20 min */
        1800,   /* 30 min */
        3600    /* 60 min */
    };
    s64 s_last_user_activity_sec = get_display_time_seconds();
    bool show_timer_zero_modal = false;

    HoldRepeat hr_time[6];      /* h↑ h↓ m↑ m↓ s↑ s↓ */
    HoldRepeat hr_date[6];      /* col1↑ col1↓ col2↑ col2↓ col3↑ col3↓ */
    HoldRepeat hr_alarm[4];     /* alarm h↑ h↓ m↑ m↓ */
    HoldRepeat hr_nav[2];       /* unified nav up, nav down (D-Pad + Circle Pad) */
    memset(hr_time, 0, sizeof(hr_time));
    memset(hr_date, 0, sizeof(hr_date));
    memset(hr_alarm, 0, sizeof(hr_alarm));
    memset(hr_nav, 0, sizeof(hr_nav));

    telemetry_update(true);

    /* === Main loop === */
    while (aptMainLoop()) {
        alarm_tick(&save, &alarm_sys);
        audio_tick();
        telemetry_update(false);

        hidScanInput();
        u32 kDown = hidKeysDown();
        u32 kHeld = hidKeysHeld();

        circlePosition circle;
        hidCircleRead(&circle);

        bool nav_up_held   = ((kHeld & KEY_DUP) != 0)   || (circle.dy > 40);
        bool nav_down_held = ((kHeld & KEY_DDOWN) != 0) || (circle.dy < -40);
        if (nav_up_held && nav_down_held) {
            nav_up_held = false;
            nav_down_held = false;
        }

        hold_repeat_update_ex(&hr_nav[0], nav_up_held, 25, 6);
        hold_repeat_update_ex(&hr_nav[1], nav_down_held, 25, 6);

        if (kDown & KEY_START) break;

        touchPosition touch;
        hidTouchRead(&touch);
        bool tDown = (kDown & KEY_TOUCH) != 0;
        bool tHeld = (kHeld & KEY_TOUCH) != 0;

        /* Update timer expiration status & trigger */
        bool tmr_expired_now = false;
        if (tmr.state == TMR_RUNNING && osGetTime() >= tmr.deadline_ms) {
            tmr.state = TMR_EXPIRED;
            tmr_expired_now = true;
        } else if (tmr.state == TMR_EXPIRED && !timer_ringing) {
            tmr_expired_now = true;
        }

        /* Strict Conflict Resolution: Alarm strictly takes priority over Timer */
        if (alarm_sys.state == ALARM_STATE_RINGING) {
            /* Scenario A: If timer was already ringing and alarm fires -> dismiss timer immediately */
            /* Scenario B: If timer expires while alarm is ringing -> dismiss timer silently, no modal, no audio */
            /* Scenario C: Simultaneous trigger on same frame -> Alarm wins, timer dismissed */
            if (timer_ringing || tmr_expired_now) {
                timer_ringing = false;
                timer_dismiss(&tmr);
            }
        } else {
            /* Alarm is not ringing */
            if (tmr_expired_now) {
                timer_ringing = true;
                timer_ring_start_epoch = get_display_time_seconds();
                timer_ring_frames = 0;
                audio_play_timer();
            }

            /* 5-minute (300-second) auto-silence timeout for Timer */
            if (timer_ringing) {
                s64 now_sec = get_display_time_seconds();
                if (now_sec - timer_ring_start_epoch >= 300) {
                    timer_ringing = false;
                    audio_stop();
                    timer_dismiss(&tmr);
                }
            }
        }

        /* Display Power Management: Track User Activity for Auto Turn Off */
        s64 now_sec = get_display_time_seconds();
        if ((kDown != 0) || tDown || tHeld) {
            s_last_user_activity_sec = now_sec;
        }

        /* Display Power Management: Automatic Alarm & Timer Preemption */
        if (alarm_sys.state == ALARM_STATE_RINGING || timer_ringing) {
            if (s_screen_mode != SCREEN_MODE_ALL_ON) {
                set_screen_mode(SCREEN_MODE_ALL_ON);
            }
            s_last_user_activity_sec = now_sec; /* Reset countdown while ringing */
        } else if (save.auto_sleep_idx > 0 && s_screen_mode != SCREEN_MODE_ALL_OFF) {
            s64 timeout_s = k_auto_sleep_seconds[save.auto_sleep_idx < 8 ? save.auto_sleep_idx : 0];
            if (timeout_s > 0 && (now_sec - s_last_user_activity_sec >= timeout_s)) {
                set_screen_mode(SCREEN_MODE_ALL_OFF);
            }
        }

        /* Display Power Management: Manual Wake Triggers (Touch or D-Pad) */
        if (s_screen_mode != SCREEN_MODE_ALL_ON) {
            if ((kDown & KEY_TOUCH) || (kDown & (KEY_DUP | KEY_DDOWN | KEY_DLEFT | KEY_DRIGHT))) {
                set_screen_mode(SCREEN_MODE_ALL_ON);
                /* Suppress wake event from activating covered UI controls */
                tDown = false;
                tHeld = false;
                kDown &= ~(KEY_TOUCH | KEY_DUP | KEY_DDOWN | KEY_DLEFT | KEY_DRIGHT);
                s_last_user_activity_sec = now_sec;
            }
        }

        /* Display Power Management: Global Hardware Hotkey (L + R) */
        if (s_lr_debounce > 0) {
            s_lr_debounce--;
        } else if ((kHeld & KEY_L) && (kHeld & KEY_R) && (kDown & (KEY_L | KEY_R))) {
            set_screen_mode(SCREEN_MODE_ALL_OFF);
            s_lr_debounce = 20; /* Debounce frames to prevent flickering */
        }

        /* Display Power Management: Night Standby Loop Throttling */
        if (s_screen_mode == SCREEN_MODE_ALL_OFF) {
            /* Night Standby Mode: Both backlights powered off.
             * Completely bypass C3D/C2D render loop to eliminate CPU/GPU load.
             * Sleep ~16.6ms to maintain 60 Hz tick for timers, alarms, and audio. */
            svcSleepThread(16 * 1000 * 1000LL);
            continue;
        }

        /* ========================================================== */
        /*  Input Handling                                            */
        /* ========================================================== */

        if (is_first_boot) {
            first_boot_frames++;
            if (first_boot_frames > 30) {
                if ((kDown & KEY_A) ||
                    (tDown && touch_hit(touch.px, touch.py, &BTN_OK)))
                    is_first_boot = false;
            }
        }
        else if (alarm_sys.state == ALARM_STATE_RINGING) {
            /* Ringing overlay dismiss */
            if ((kDown & (KEY_A | KEY_B)) || (tDown && touch_hit(touch.px, touch.py, &BTN_ALARM_DISMISS))) {
                alarm_dismiss_all(&save, &alarm_sys);
            }
        }
        else if (timer_ringing) {
            /* Timer ringing overlay dismiss (touch or KEY_A / KEY_B) */
            if ((kDown & (KEY_A | KEY_B)) || (tDown && touch_hit(touch.px, touch.py, &BTN_TIMER_DISMISS))) {
                timer_ringing = false;
                audio_stop();
                timer_dismiss(&tmr);
            }
        }
        else if (alarm_sys.missed_alarm) {
            /* Missed alarm modal dismiss */
            if ((kDown & (KEY_A | KEY_B)) || (tDown && touch_hit(touch.px, touch.py, &BTN_ALARM_MISSED_OK))) {
                alarm_sys.missed_alarm = false;
            }
        }
        else if (is_settings) {
            /* Settings overlay views */
            switch (settings_sub) {
            case SET_MAIN:
                if ((kDown & KEY_B) ||
                    (tDown && touch_hit(touch.px, touch.py, &BTN_SET_BACK))) {
                    is_settings = false;
                } else if (tDown && touch_hit(touch.px, touch.py, &BTN_SET_TIME_DATE)) {
                    settings_sub = SET_TIME_DATE_MENU;
                } else if (tDown && touch_hit(touch.px, touch.py, &BTN_SET_DISPLAY)) {
                    settings_sub = SET_DISPLAY;
                } else if (tDown && touch_hit(touch.px, touch.py, &BTN_SET_MANUAL)) {
                    settings_sub = SET_MANUAL;
                    manual_topic_idx = 0;
                    manual_scroll_y = 0.0f;
                    manual_is_dragging = false;
                } else if (tDown && touch_hit(touch.px, touch.py, &BTN_SET_ABOUT)) {
                    settings_sub = SET_ABOUT;
                    about_scroll_y = 0.0f;
                    about_is_dragging = false;
                }
                break;

            case SET_TIME_DATE_MENU:
                if ((kDown & KEY_B) ||
                    (tDown && touch_hit(touch.px, touch.py, &BTN_SET_BACK))) {
                    settings_sub = SET_MAIN;
                } else if (tDown && touch_hit(touch.px, touch.py, &BTN_SET_EDIT_TIME)) {
                    clock_get_hms(&edit_h, &edit_m, &edit_s);
                    clock_get_ymd(&edit_y, &edit_mo, &edit_d);
                    edit_fmt = (DateFormat)save.date_format;
                    memset(hr_time, 0, sizeof(hr_time));
                    settings_sub = SET_EDIT_TIME;
                } else if (tDown && touch_hit(touch.px, touch.py, &BTN_SET_EDIT_DATE_BTN)) {
                    clock_get_hms(&edit_h, &edit_m, &edit_s);
                    clock_get_ymd(&edit_y, &edit_mo, &edit_d);
                    edit_fmt = (DateFormat)save.date_format;
                    ui_update_date_hitboxes(edit_fmt);
                    memset(hr_date, 0, sizeof(hr_date));
                    settings_sub = SET_EDIT_DATE;
                } else if (tDown && touch_hit(touch.px, touch.py, &BTN_SET_RESET)) {
                    settings_sub = SET_CONFIRM_RESET;
                }
                break;

            case SET_DISPLAY:
                if ((kDown & KEY_B) ||
                    (tDown && touch_hit(touch.px, touch.py, &BTN_SET_BACK))) {
                    settings_sub = SET_MAIN;
                } else if (tDown && touch_hit(touch.px, touch.py, &BTN_DISP_BOTH_OFF)) {
                    set_screen_mode(SCREEN_MODE_ALL_OFF);
                } else if (tDown && touch_hit(touch.px, touch.py, &BTN_DISP_BOT_OFF)) {
                    set_screen_mode(SCREEN_MODE_BOTTOM_OFF);
                } else if (tDown && touch_hit(touch.px, touch.py, &BTN_DISP_AUTO_LEFT)) {
                    if (save.auto_sleep_idx == 0) save.auto_sleep_idx = 7;
                    else save.auto_sleep_idx--;
                    save_write(&save);
                } else if (tDown && touch_hit(touch.px, touch.py, &BTN_DISP_AUTO_RIGHT)) {
                    save.auto_sleep_idx = (save.auto_sleep_idx + 1) % 8;
                    save_write(&save);
                }
                break;

            case SET_CONFIRM_RESET:
                if ((kDown & KEY_A) || (tDown && touch_hit(touch.px, touch.py, &BTN_CONFIRM))) {
                    clock_reset(&save);
                    save_msg = "Reset to system time & date!";
                    settings_sub = SET_SAVE_OK;
                } else if ((kDown & KEY_B) || (tDown && touch_hit(touch.px, touch.py, &BTN_CANCEL))) {
                    settings_sub = SET_TIME_DATE_MENU;
                }
                break;

            case SET_CONFIRM_RESET_TIME:
                if ((kDown & KEY_A) || (tDown && touch_hit(touch.px, touch.py, &BTN_CONFIRM))) {
                    clock_reset_time(&save);
                    clock_get_hms(&edit_h, &edit_m, &edit_s);
                    save_msg = "Time reset to system time!";
                    settings_sub = SET_SAVE_OK;
                } else if ((kDown & KEY_B) || (tDown && touch_hit(touch.px, touch.py, &BTN_CANCEL))) {
                    settings_sub = SET_EDIT_TIME;
                }
                break;

            case SET_EDIT_TIME:
                if ((kDown & KEY_B) ||
                    (tDown && touch_hit(touch.px, touch.py, &BTN_SET_BACK))) {
                    settings_sub = SET_TIME_DATE_MENU;
                } else if (tDown && touch_hit(touch.px, touch.py, &BTN_SET_SAVE)) {
                    clock_apply_time_edit(edit_h, edit_m, edit_s, &save);
                    save_msg = "Time saved successfully!";
                    settings_sub = SET_SAVE_OK;
                } else if (tDown && touch_hit(touch.px, touch.py, &BTN_RESET_TIME)) {
                    settings_sub = SET_CONFIRM_RESET_TIME;
                } else {
                    hold_repeat_update(&hr_time[0], tHeld && touch_hit(touch.px, touch.py, &ARROW_H_UP));
                    hold_repeat_update(&hr_time[1], tHeld && touch_hit(touch.px, touch.py, &ARROW_H_DOWN));
                    hold_repeat_update(&hr_time[2], tHeld && touch_hit(touch.px, touch.py, &ARROW_M_UP));
                    hold_repeat_update(&hr_time[3], tHeld && touch_hit(touch.px, touch.py, &ARROW_M_DOWN));
                    hold_repeat_update(&hr_time[4], tHeld && touch_hit(touch.px, touch.py, &ARROW_S_UP));
                    hold_repeat_update(&hr_time[5], tHeld && touch_hit(touch.px, touch.py, &ARROW_S_DOWN));

                    if (hr_time[0].triggered) edit_h = (edit_h +  1) % 24;
                    if (hr_time[1].triggered) edit_h = (edit_h + 23) % 24;
                    if (hr_time[2].triggered) edit_m = (edit_m +  1) % 60;
                    if (hr_time[3].triggered) edit_m = (edit_m + 59) % 60;
                    if (hr_time[4].triggered) edit_s = (edit_s +  1) % 60;
                    if (hr_time[5].triggered) edit_s = (edit_s + 59) % 60;
                }
                break;

            case SET_EDIT_DATE:
                if ((kDown & KEY_B) ||
                    (tDown && touch_hit(touch.px, touch.py, &BTN_SET_BACK))) {
                    settings_sub = SET_TIME_DATE_MENU;
                } else if (tDown && touch_hit(touch.px, touch.py, &BTN_SET_SAVE)) {
                    save.date_format = (u8)edit_fmt;
                    clock_apply_date_edit(edit_y, edit_mo, edit_d, &save);
                    save_msg = "Date saved successfully!";
                    settings_sub = SET_SAVE_OK;
                } else if (tDown && touch_hit(touch.px, touch.py, &BTN_FMT_LEFT)) {
                    edit_fmt = (edit_fmt == DATEFMT_ISO) ? DATEFMT_US : (DateFormat)(edit_fmt - 1);
                    ui_update_date_hitboxes(edit_fmt);
                } else if (tDown && touch_hit(touch.px, touch.py, &BTN_FMT_RIGHT)) {
                    edit_fmt = (edit_fmt == DATEFMT_US) ? DATEFMT_ISO : (DateFormat)(edit_fmt + 1);
                    ui_update_date_hitboxes(edit_fmt);
                } else {
                    hold_repeat_update(&hr_date[0], tHeld && touch_hit(touch.px, touch.py, &ARROW_COL1_UP));
                    hold_repeat_update(&hr_date[1], tHeld && touch_hit(touch.px, touch.py, &ARROW_COL1_DOWN));
                    hold_repeat_update(&hr_date[2], tHeld && touch_hit(touch.px, touch.py, &ARROW_COL2_UP));
                    hold_repeat_update(&hr_date[3], tHeld && touch_hit(touch.px, touch.py, &ARROW_COL2_DOWN));
                    hold_repeat_update(&hr_date[4], tHeld && touch_hit(touch.px, touch.py, &ARROW_COL3_UP));
                    hold_repeat_update(&hr_date[5], tHeld && touch_hit(touch.px, touch.py, &ARROW_COL3_DOWN));

                    if (hr_date[0].triggered) step_date_col(1, +1, &edit_y, &edit_mo, &edit_d, edit_fmt);
                    if (hr_date[1].triggered) step_date_col(1, -1, &edit_y, &edit_mo, &edit_d, edit_fmt);
                    if (hr_date[2].triggered) step_date_col(2, +1, &edit_y, &edit_mo, &edit_d, edit_fmt);
                    if (hr_date[3].triggered) step_date_col(2, -1, &edit_y, &edit_mo, &edit_d, edit_fmt);
                    if (hr_date[4].triggered) step_date_col(3, +1, &edit_y, &edit_mo, &edit_d, edit_fmt);
                    if (hr_date[5].triggered) step_date_col(3, -1, &edit_y, &edit_mo, &edit_d, edit_fmt);
                }
                break;

            case SET_SAVE_OK:
                if ((kDown & (KEY_A | KEY_B)) ||
                    (tDown && touch_hit(touch.px, touch.py, &BTN_OK))) {
                    settings_sub = SET_TIME_DATE_MENU;
                }
                break;

            case SET_MANUAL:
                if ((kDown & KEY_B) || (tDown && touch_hit(touch.px, touch.py, &BTN_MANUAL_BACK))) {
                    settings_sub = SET_MAIN;
                } else if ((kDown & KEY_L) || (tDown && touch_hit(touch.px, touch.py, &BTN_MANUAL_PREV))) {
                    manual_topic_idx = (manual_topic_idx == 0) ? 1 : 0;
                    manual_scroll_y = 0.0f;
                } else if ((kDown & KEY_R) || (tDown && touch_hit(touch.px, touch.py, &BTN_MANUAL_NEXT))) {
                    manual_topic_idx = (manual_topic_idx == 0) ? 1 : 0;
                    manual_scroll_y = 0.0f;
                } else {
                    /* D-Pad and Circle Pad scrolling */
                    float max_s = manual_get_max_scroll(manual_topic_idx);
                    if ((kHeld & KEY_DUP) || (kHeld & KEY_CPAD_UP)) {
                        manual_scroll_y -= 12.0f;
                        if (manual_scroll_y < 0.0f) manual_scroll_y = 0.0f;
                    } else if ((kHeld & KEY_DDOWN) || (kHeld & KEY_CPAD_DOWN)) {
                        manual_scroll_y += 12.0f;
                        if (manual_scroll_y > max_s) manual_scroll_y = max_s;
                    }

                    /* Touch drag scrolling */
                    if (tDown && touch.py >= 36 && touch.py <= 236) {
                        manual_touch_start_y = touch.py;
                        manual_is_dragging = true;
                    } else if (tHeld && manual_is_dragging) {
                        float dy = touch.py - manual_touch_start_y;
                        manual_scroll_y -= dy;
                        manual_touch_start_y = touch.py;
                        if (manual_scroll_y < 0.0f) manual_scroll_y = 0.0f;
                        if (manual_scroll_y > max_s) manual_scroll_y = max_s;
                    } else if (!tHeld) {
                        manual_is_dragging = false;
                    }
                }
                break;

            case SET_ABOUT:
                if ((kDown & KEY_B) || (tDown && touch_hit(touch.px, touch.py, &BTN_SET_BACK))) {
                    settings_sub = SET_MAIN;
                } else {
                    float max_s = about_get_max_scroll();
                    if ((kHeld & KEY_DUP) || (kHeld & KEY_CPAD_UP)) {
                        about_scroll_y -= 12.0f;
                        if (about_scroll_y < 0.0f) about_scroll_y = 0.0f;
                    } else if ((kHeld & KEY_DDOWN) || (kHeld & KEY_CPAD_DOWN)) {
                        about_scroll_y += 12.0f;
                        if (about_scroll_y > max_s) about_scroll_y = max_s;
                    }

                    if (tDown && touch.py >= 36 && touch.py <= 240) {
                        about_touch_start_y = touch.py;
                        about_is_dragging = true;
                    } else if (tHeld && about_is_dragging) {
                        float dy = touch.py - about_touch_start_y;
                        about_scroll_y -= dy;
                        about_touch_start_y = touch.py;
                        if (about_scroll_y < 0.0f) about_scroll_y = 0.0f;
                        if (about_scroll_y > max_s) about_scroll_y = max_s;
                    } else if (!tHeld) {
                        about_is_dragging = false;
                    }
                }
                break;
            }
        }
        else if (alarm_view == STATE_ALARM_ADD || alarm_view == STATE_ALARM_EDIT) {
            /* ====================================================== */
            /*  Dedicated Full-Screen Modal Input for Add / Edit Alarm*/
            /* ====================================================== */
            bool is_new = (alarm_view == STATE_ALARM_ADD);

            if ((kDown & KEY_B) || (tDown && touch_hit(touch.px, touch.py, &BTN_ALARM_EDIT_CANCEL))) {
                audio_stop();
                alarm_view = ALARM_VIEW_LIST;
            } else if (tDown && touch_hit(touch.px, touch.py, &BTN_ALARM_EDIT_SAVE)) {
                audio_stop();
                if (is_new) {
                    int new_idx = alarm_add(&save, edit_alarm_h, edit_alarm_m, edit_alarm_repeat, edit_alarm_tone, edit_alarm_label);
                    if (new_idx >= 0) {
                        alarm_list_state.selected_index = new_idx;
                        alarm_calc_viewport_scroll(&alarm_list_state.scroll_y, new_idx);
                    }
                } else if (edit_alarm_idx >= 0 && edit_alarm_idx < save.alarm_count) {
                    save.alarms[edit_alarm_idx].hour = edit_alarm_h;
                    save.alarms[edit_alarm_idx].minute = edit_alarm_m;
                    save.alarms[edit_alarm_idx].repeat_mode = edit_alarm_repeat;
                    save.alarms[edit_alarm_idx].ringtone_id = edit_alarm_tone;
                    snprintf(save.alarms[edit_alarm_idx].label, sizeof(save.alarms[edit_alarm_idx].label), "%s", edit_alarm_label);
                    save.alarms[edit_alarm_idx].enabled = true; // Auto-enable on edit

                    /* Guard against instant firing if scheduled time for today already passed */
                    now_sec = get_display_time_seconds();
                    int y, m, d;
                    s64 now_days = now_sec / 86400;
                    if ((now_sec % 86400) < 0) now_days--;
                    days_to_ymd(now_days, &y, &m, &d);
                    s64 today_fire = ymd_to_days(y, m, d) * 86400LL + edit_alarm_h * 3600LL + edit_alarm_m * 60LL;
                    if (today_fire <= now_sec) {
                        save.alarms[edit_alarm_idx].last_fired_epoch = today_fire;
                    } else {
                        save.alarms[edit_alarm_idx].last_fired_epoch = today_fire - 86400LL;
                    }

                    alarm_sort(&save);
                    for (int i = 0; i < save.alarm_count; i++) {
                        if (save.alarms[i].hour == edit_alarm_h &&
                            save.alarms[i].minute == edit_alarm_m &&
                            strncmp(save.alarms[i].label, edit_alarm_label, ALARM_LABEL_LEN) == 0) {
                            alarm_list_state.selected_index = i;
                            alarm_calc_viewport_scroll(&alarm_list_state.scroll_y, i);
                            break;
                        }
                    }
                    save_write(&save);
                }
                alarm_view = ALARM_VIEW_LIST;
            } else if (tDown && touch_hit(touch.px, touch.py, &BTN_ALARM_LABEL_INPUT)) {
                audio_stop();
                SwkbdState swkbd;
                char kbd_buf[ALARM_LABEL_LEN];
                swkbdInit(&swkbd, SWKBD_TYPE_NORMAL, 2, ALARM_LABEL_LEN - 1);
                swkbdSetValidation(&swkbd, SWKBD_ANYTHING, 0, 0);
                swkbdSetFeatures(&swkbd, SWKBD_DEFAULT_QWERTY | SWKBD_DARKEN_TOP_SCREEN | SWKBD_ALLOW_HOME);
                swkbdSetHintText(&swkbd, "Alarm Label");
                swkbdSetInitialText(&swkbd, edit_alarm_label);
                SwkbdButton button = swkbdInputText(&swkbd, kbd_buf, sizeof(kbd_buf));
                if (button == SWKBD_BUTTON_CONFIRM) {
                    snprintf(edit_alarm_label, sizeof(edit_alarm_label), "%s", kbd_buf);
                }
            } else if (tDown && touch_hit(touch.px, touch.py, &BTN_ALARM_TONE_PREVIEW)) {
                if (audio_is_playing()) {
                    audio_stop();
                } else {
                    audio_play_preview(edit_alarm_tone);
                }
            } else if (tDown && touch_hit(touch.px, touch.py, &BTN_ALARM_TONE_LEFT)) {
                audio_stop();
                if (edit_alarm_tone == 0) edit_alarm_tone = audio_get_ringtone_count() - 1;
                else edit_alarm_tone--;
            } else if (tDown && touch_hit(touch.px, touch.py, &BTN_ALARM_TONE_RIGHT)) {
                audio_stop();
                edit_alarm_tone = (edit_alarm_tone + 1) % audio_get_ringtone_count();
            } else if (tDown && touch_hit(touch.px, touch.py, &BTN_ALARM_REPEAT_LEFT)) {
                if (edit_alarm_repeat == 0) edit_alarm_repeat = 3;
                else edit_alarm_repeat--;
            } else if (tDown && touch_hit(touch.px, touch.py, &BTN_ALARM_REPEAT_RIGHT)) {
                edit_alarm_repeat = (edit_alarm_repeat + 1) % 4;
            } else {
                hold_repeat_update(&hr_alarm[0], tHeld && touch_hit(touch.px, touch.py, &ARROW_ALARM_H_UP));
                hold_repeat_update(&hr_alarm[1], tHeld && touch_hit(touch.px, touch.py, &ARROW_ALARM_H_DOWN));
                hold_repeat_update(&hr_alarm[2], tHeld && touch_hit(touch.px, touch.py, &ARROW_ALARM_M_UP));
                hold_repeat_update(&hr_alarm[3], tHeld && touch_hit(touch.px, touch.py, &ARROW_ALARM_M_DOWN));

                if (hr_alarm[0].triggered) edit_alarm_h = (edit_alarm_h +  1) % 24;
                if (hr_alarm[1].triggered) edit_alarm_h = (edit_alarm_h + 23) % 24;
                if (hr_alarm[2].triggered) edit_alarm_m = (edit_alarm_m +  1) % 60;
                if (hr_alarm[3].triggered) edit_alarm_m = (edit_alarm_m + 59) % 60;
            }
        }
        else {
            /* ====================================================== */
            /*  Level 3: Primary Tab Host Navigation & Controls       */
            /* ====================================================== */
            bool nav_handled = false;

            /* Hardware Shortcut to Settings (SELECT) */
            if (kDown & KEY_SELECT) {
                is_settings  = true;
                settings_sub = SET_MAIN;
                show_timer_zero_modal = false;
                nav_handled  = true;
            }
            /* Global Tab Navigation via Shoulder Buttons (L / R) with Circular Wrapping */
            else if ((kDown & KEY_L) && !(kHeld & KEY_R)) {
                active_mode = (AppMode)((active_mode + 3) % 4);
                clock_view = CLOCK_VIEW_LIST;
                confirm_del_city_idx = -1;
                confirm_home_city_id = -1;
                alert_home_city_id   = -1;
                show_timer_zero_modal = false;
                nav_handled = true;
            }
            else if ((kDown & KEY_R) && !(kHeld & KEY_L)) {
                active_mode = (AppMode)((active_mode + 1) % 4);
                clock_view = CLOCK_VIEW_LIST;
                confirm_del_city_idx = -1;
                confirm_home_city_id = -1;
                alert_home_city_id   = -1;
                show_timer_zero_modal = false;
                nav_handled = true;
            }
            /* Stylus Touch Navigation: Docked Tab Bar */
            else if (tDown && touch.py >= 200 && !(active_mode == MODE_CLOCK && clock_view == CLOCK_VIEW_PICKER)) {
                show_timer_zero_modal = false;
                if (touch_hit(touch.px, touch.py, &TAB_ALARM)) {
                    active_mode = MODE_ALARM;
                    clock_view = CLOCK_VIEW_LIST;
                    confirm_del_city_idx = -1;
                    confirm_home_city_id = -1;
                    alert_home_city_id   = -1;
                    nav_handled = true;
                } else if (touch_hit(touch.px, touch.py, &TAB_CLOCK)) {
                    active_mode = MODE_CLOCK;
                    clock_view = CLOCK_VIEW_LIST;
                    confirm_del_city_idx = -1;
                    confirm_home_city_id = -1;
                    alert_home_city_id   = -1;
                    nav_handled = true;
                } else if (touch_hit(touch.px, touch.py, &TAB_STOPWATCH)) {
                    active_mode = MODE_STOPWATCH;
                    clock_view = CLOCK_VIEW_LIST;
                    confirm_del_city_idx = -1;
                    confirm_home_city_id = -1;
                    alert_home_city_id   = -1;
                    nav_handled = true;
                } else if (touch_hit(touch.px, touch.py, &TAB_TIMER)) {
                    active_mode = MODE_TIMER;
                    clock_view = CLOCK_VIEW_LIST;
                    confirm_del_city_idx = -1;
                    confirm_home_city_id = -1;
                    alert_home_city_id   = -1;
                    nav_handled = true;
                }
            }
            /* Stylus Touch Navigation: Header Settings Icon */
            else if (tDown && touch_hit(touch.px, touch.py, &BTN_SETTINGS_ICON) && !(active_mode == MODE_CLOCK && clock_view == CLOCK_VIEW_PICKER)) {
                is_settings  = true;
                settings_sub = SET_MAIN;
                confirm_del_city_idx = -1;
                confirm_home_city_id = -1;
                alert_home_city_id   = -1;
                show_timer_zero_modal = false;
                nav_handled  = true;
            }

            if (!nav_handled) {
                switch (active_mode) {
                case MODE_CLOCK:
                    if (clock_view == CLOCK_VIEW_PICKER) {
                        int total_cities = world_clock_get_total_cities();
                        if ((kDown & KEY_B) || (tDown && touch_hit(touch.px, touch.py, &BTN_CITY_PICKER_BACK))) {
                            clock_view = CLOCK_VIEW_LIST;
                        } else if (kDown & KEY_A) {
                            if (city_picker_state.selected_index >= 0 && city_picker_state.selected_index < total_cities) {
                                if (!world_clock_has_city(&save, (u8)city_picker_state.selected_index)) {
                                    world_clock_add_city(&save, (u8)city_picker_state.selected_index);
                                    clock_view = CLOCK_VIEW_LIST;
                                }
                            }
                        } else if (hr_nav[1].triggered) {
                            city_picker_state.selected_index = world_clock_calc_wrap_index(city_picker_state.selected_index, total_cities, +1);
                            world_clock_calc_viewport_scroll(&city_picker_state.scroll_y, city_picker_state.selected_index, 34.0f, 2.0f, 36.0f, 230.0f);
                        } else if (hr_nav[0].triggered) {
                            city_picker_state.selected_index = world_clock_calc_wrap_index(city_picker_state.selected_index, total_cities, -1);
                            world_clock_calc_viewport_scroll(&city_picker_state.scroll_y, city_picker_state.selected_index, 34.0f, 2.0f, 36.0f, 230.0f);
                        } else if (tDown && touch.py >= 34) {
                            city_picker_state.touch_start_y = touch.py;
                            city_picker_state.touch_start_x = touch.px;
                            city_picker_state.is_dragging = false;
                            city_picker_state.potential_tap = true;
                            city_picker_state.candidate_index = -1;

                            float start_y = 36.0f - city_picker_state.scroll_y;
                            for (int i = 0; i < total_cities; i++) {
                                float y = start_y + i * 36.0f;
                                if (y > 240.0f || y + 34.0f < 34.0f) continue;
                                HitRect item_rect = { 10.0f, y, 296.0f, 34.0f };
                                if (touch_hit(touch.px, touch.py, &item_rect)) {
                                    city_picker_state.candidate_index = i;
                                    break;
                                }
                            }
                        } else if (tHeld && (city_picker_state.potential_tap || city_picker_state.is_dragging)) {
                            float delta_y = touch.py - city_picker_state.touch_start_y;
                            if (!city_picker_state.is_dragging) {
                                if (fabsf(delta_y) > TOUCH_SLOP_PX) {
                                    city_picker_state.is_dragging = true;
                                    city_picker_state.potential_tap = false;
                                    city_picker_state.candidate_index = -1;
                                }
                            }
                            if (city_picker_state.is_dragging) {
                                city_picker_state.scroll_y -= delta_y;
                                city_picker_state.touch_start_y = touch.py;
                            }
                        } else if (!tHeld && (city_picker_state.potential_tap || city_picker_state.is_dragging)) {
                            if (city_picker_state.potential_tap && !city_picker_state.is_dragging) {
                                int idx = city_picker_state.candidate_index;
                                if (idx >= 0 && idx < total_cities) {
                                    city_picker_state.selected_index = idx;
                                    if (!world_clock_has_city(&save, (u8)idx)) {
                                        world_clock_add_city(&save, (u8)idx);
                                        clock_view = CLOCK_VIEW_LIST;
                                    }
                                }
                            }
                            city_picker_state.is_dragging = false;
                            city_picker_state.potential_tap = false;
                            city_picker_state.candidate_index = -1;
                        }

                        /* Clamp picker scroll */
                        float max_scroll = (total_cities * 36.0f) - 204.0f;
                        if (max_scroll < 0.0f) max_scroll = 0.0f;
                        if (city_picker_state.scroll_y < 0.0f) city_picker_state.scroll_y = 0.0f;
                        if (city_picker_state.scroll_y > max_scroll) city_picker_state.scroll_y = max_scroll;
                    } else {
                        /* CLOCK_VIEW_LIST */
                        if (confirm_del_city_idx >= 0) {
                            if ((kDown & KEY_A) || (tDown && touch_hit(touch.px, touch.py, &BTN_MODAL_CITY_DEL))) {
                                world_clock_remove_city(&save, confirm_del_city_idx);
                                confirm_del_city_idx = -1;
                                if (world_clock_state.selected_index >= save.world_city_count) {
                                    world_clock_state.selected_index = save.world_city_count - 1;
                                }
                            } else if ((kDown & KEY_B) || (tDown && touch_hit(touch.px, touch.py, &BTN_MODAL_CITY_CANCEL))) {
                                confirm_del_city_idx = -1;
                            }
                        } else if (confirm_home_city_id >= 0) {
                            if ((kDown & KEY_A) || (tDown && touch_hit(touch.px, touch.py, &BTN_MODAL_HOME_SET))) {
                                world_clock_set_home_city(&save, (u8)confirm_home_city_id);
                                confirm_home_city_id = -1;
                            } else if ((kDown & KEY_B) || (tDown && touch_hit(touch.px, touch.py, &BTN_MODAL_HOME_CANCEL))) {
                                confirm_home_city_id = -1;
                            }
                        } else if (alert_home_city_id >= 0) {
                            if ((kDown & (KEY_A | KEY_B)) || (tDown && touch_hit(touch.px, touch.py, &BTN_OK))) {
                                alert_home_city_id = -1;
                            }
                        } else {
                            if ((kDown & KEY_Y) || (tDown && touch_hit(touch.px, touch.py, &BTN_CLOCK_ADD))) {
                                if (save.world_city_count < MAX_WORLD_CITIES) {
                                    clock_view = CLOCK_VIEW_PICKER;
                                    city_picker_state.scroll_y = 0.0f;
                                    city_picker_state.selected_index = 0;
                                }
                            } else if (kDown & KEY_X) {
                                if (save.world_city_count > 0 && world_clock_state.selected_index >= 0 && world_clock_state.selected_index < save.world_city_count) {
                                    confirm_del_city_idx = world_clock_state.selected_index;
                                }
                            } else if (kDown & KEY_A) {
                                if (save.world_city_count > 0 && world_clock_state.selected_index >= 0 && world_clock_state.selected_index < save.world_city_count) {
                                    u8 cid = save.world_cities[world_clock_state.selected_index];
                                    if (cid != save.home_city_id) {
                                        confirm_home_city_id = cid;
                                    } else {
                                        alert_home_city_id = cid;
                                    }
                                }
                            } else if (save.world_city_count > 0 && hr_nav[1].triggered) {
                                world_clock_state.selected_index = world_clock_calc_wrap_index(world_clock_state.selected_index, save.world_city_count, +1);
                                world_clock_calc_viewport_scroll(&world_clock_state.scroll_y, world_clock_state.selected_index, 46.0f, 4.0f, 36.0f, 150.0f);
                            } else if (save.world_city_count > 0 && hr_nav[0].triggered) {
                                world_clock_state.selected_index = world_clock_calc_wrap_index(world_clock_state.selected_index, save.world_city_count, -1);
                                world_clock_calc_viewport_scroll(&world_clock_state.scroll_y, world_clock_state.selected_index, 46.0f, 4.0f, 36.0f, 150.0f);
                            } else if (tDown && touch.py >= 34 && touch.py <= 198) {
                                world_clock_state.touch_start_y = touch.py;
                                world_clock_state.touch_start_x = touch.px;
                                world_clock_state.is_dragging = false;
                                world_clock_state.potential_tap = true;
                                world_clock_state.candidate_index = -1;
                                world_clock_state.candidate_is_delete = false;

                                float start_y = 36.0f - world_clock_state.scroll_y;
                                for (int i = 0; i < save.world_city_count; i++) {
                                    float y = start_y + i * 50.0f;
                                    if (y > 200.0f || y + 46.0f < 34.0f) continue;

                                    HitRect del_rect = { 270.0f, y, 40.0f, 46.0f };
                                    if (touch_hit(touch.px, touch.py, &del_rect)) {
                                        world_clock_state.candidate_index = i;
                                        world_clock_state.candidate_is_delete = true;
                                        break;
                                    }

                                    HitRect card_rect = { 10.0f, y, 258.0f, 46.0f };
                                    if (touch_hit(touch.px, touch.py, &card_rect)) {
                                        world_clock_state.candidate_index = i;
                                        world_clock_state.candidate_is_delete = false;
                                        break;
                                    }
                                }
                            } else if (tHeld && (world_clock_state.potential_tap || world_clock_state.is_dragging)) {
                                float delta_y = touch.py - world_clock_state.touch_start_y;
                                if (!world_clock_state.is_dragging) {
                                    if (fabsf(delta_y) > TOUCH_SLOP_PX) {
                                        world_clock_state.is_dragging = true;
                                        world_clock_state.potential_tap = false;
                                        world_clock_state.candidate_index = -1;
                                    }
                                }
                                if (world_clock_state.is_dragging) {
                                    world_clock_state.scroll_y -= delta_y;
                                    world_clock_state.touch_start_y = touch.py;
                                }
                            } else if (!tHeld && (world_clock_state.potential_tap || world_clock_state.is_dragging)) {
                                if (world_clock_state.potential_tap && !world_clock_state.is_dragging) {
                                    int idx = world_clock_state.candidate_index;
                                    if (idx >= 0 && idx < save.world_city_count) {
                                        if (world_clock_state.candidate_is_delete) {
                                            confirm_del_city_idx = idx;
                                        } else {
                                            world_clock_state.selected_index = idx;
                                            u8 cid = save.world_cities[idx];
                                            if (cid != save.home_city_id) {
                                                confirm_home_city_id = cid;
                                            } else {
                                                alert_home_city_id = cid;
                                            }
                                        }
                                    }
                                }
                                world_clock_state.is_dragging = false;
                                world_clock_state.potential_tap = false;
                                world_clock_state.candidate_index = -1;
                            }

                            /* Clamp scroll */
                            float max_scroll = (save.world_city_count * 50.0f) - 164.0f;
                            if (max_scroll < 0.0f) max_scroll = 0.0f;
                            if (world_clock_state.scroll_y < 0.0f) world_clock_state.scroll_y = 0.0f;
                            if (world_clock_state.scroll_y > max_scroll) world_clock_state.scroll_y = max_scroll;
                        }
                    }
                    break;

                case MODE_STOPWATCH:
                    if (sw.state == SW_IDLE) {
                        if ((kDown & KEY_A) || (tDown && touch_hit(touch.px, touch.py, &BTN_SW_START)))
                            stopwatch_start(&sw);
                    } else if (sw.state == SW_RUNNING) {
                        if ((kDown & KEY_A) || (tDown && touch_hit(touch.px, touch.py, &BTN_SW_PAUSE)))
                            stopwatch_pause(&sw);
                        else if ((kDown & KEY_B) || (tDown && touch_hit(touch.px, touch.py, &BTN_SW_RESET)))
                            stopwatch_reset(&sw);
                        else if ((kDown & KEY_Y) || (tDown && touch_hit(touch.px, touch.py, &BTN_SW_LAP)))
                            stopwatch_lap(&sw);
                    } else if (sw.state == SW_PAUSED) {
                        if ((kDown & KEY_A) || (tDown && touch_hit(touch.px, touch.py, &BTN_SW_RESUME)))
                            stopwatch_resume(&sw);
                        else if ((kDown & KEY_B) || (tDown && (touch_hit(touch.px, touch.py, &BTN_SW_RESET) || touch_hit(touch.px, touch.py, &BTN_SW_RESET_PAUSED))))
                            stopwatch_reset(&sw);
                    }

                    /* Scroll laps when present */
                    if (sw.lap_count > 6) {
                        if (hr_nav[0].triggered) {
                            stopwatch_scroll(&sw, -1);
                        } else if (hr_nav[1].triggered) {
                            stopwatch_scroll(&sw, +1);
                        }
                    }
                    break;

                case MODE_ALARM:
                    if (show_delete_confirm) {
                        if ((kDown & KEY_B) || (tDown && touch_hit(touch.px, touch.py, &BTN_CANCEL))) {
                            show_delete_confirm = false;
                        } else if ((kDown & KEY_A) || (tDown && touch_hit(touch.px, touch.py, &BTN_CONFIRM))) {
                            if (edit_alarm_idx >= 0 && edit_alarm_idx < save.alarm_count) {
                                alarm_delete(&save, edit_alarm_idx);
                                if (alarm_list_state.selected_index >= save.alarm_count) {
                                    alarm_list_state.selected_index = save.alarm_count - 1;
                                }
                            }
                            show_delete_confirm = false;
                        }
                    } else {
                        /* Delete selected alarm via Physical Button (X) */
                        if (kDown & KEY_X) {
                            if (save.alarm_count > 0 && alarm_list_state.selected_index >= 0 && alarm_list_state.selected_index < save.alarm_count) {
                                edit_alarm_idx = alarm_list_state.selected_index;
                                show_delete_confirm = true;
                            }
                        } else if (kDown & KEY_Y) {
                            if (save.alarm_count > 0 && alarm_list_state.selected_index >= 0 && alarm_list_state.selected_index < save.alarm_count) {
                                alarm_toggle_entry(&save, alarm_list_state.selected_index);
                            }
                        } else if (kDown & KEY_A && alarm_list_state.selected_index >= 0 && alarm_list_state.selected_index < save.alarm_count) {
                            edit_alarm_idx = alarm_list_state.selected_index;
                            AlarmEntry* a = &save.alarms[edit_alarm_idx];
                            edit_alarm_h = a->hour;
                            edit_alarm_m = a->minute;
                            edit_alarm_repeat = a->repeat_mode;
                            edit_alarm_tone = a->ringtone_id;
                            snprintf(edit_alarm_label, sizeof(edit_alarm_label), "%s", a->label);
                            memset(hr_alarm, 0, sizeof(hr_alarm));
                            show_delete_confirm = false;
                            alarm_view = STATE_ALARM_EDIT;
                        } else if (save.alarm_count > 0 && hr_nav[1].triggered) { /* DOWN continuous hold-repeat (D-Pad + Circle Pad) with circular wrap */
                            alarm_list_state.selected_index = alarm_calc_wrap_index(alarm_list_state.selected_index, save.alarm_count, +1);
                            alarm_calc_viewport_scroll(&alarm_list_state.scroll_y, alarm_list_state.selected_index);
                        } else if (save.alarm_count > 0 && hr_nav[0].triggered) { /* UP continuous hold-repeat (D-Pad + Circle Pad) with circular wrap */
                            alarm_list_state.selected_index = alarm_calc_wrap_index(alarm_list_state.selected_index, save.alarm_count, -1);
                            alarm_calc_viewport_scroll(&alarm_list_state.scroll_y, alarm_list_state.selected_index);
                        } else if (tDown && touch_hit(touch.px, touch.py, &BTN_ALARM_ADD)) {
                            if (save.alarm_count < MAX_ALARMS) {
                                edit_alarm_idx = -1;
                                clock_get_hms(&edit_alarm_h, &edit_alarm_m, &edit_s);
                                edit_alarm_repeat = REPEAT_ONCE;
                                edit_alarm_tone = 0;
                                edit_alarm_label[0] = '\0';
                                memset(hr_alarm, 0, sizeof(hr_alarm));
                                show_delete_confirm = false;
                                alarm_view = STATE_ALARM_ADD;
                            }
                        } else if (tDown && touch.py >= 34 && touch.py <= 198) {
                            /* Touch down in scroll area: initiate touch-slop disambiguation */
                            alarm_list_state.touch_start_y = touch.py;
                            alarm_list_state.touch_start_x = touch.px;
                            alarm_list_state.is_dragging = false;
                            alarm_list_state.potential_tap = true;
                            alarm_list_state.candidate_index = -1;
                            alarm_list_state.candidate_is_toggle = false;
                            alarm_list_state.candidate_is_delete = false;
                            
                            float start_y = 36.0f - alarm_list_state.scroll_y;
                            for (int i = 0; i < save.alarm_count; i++) {
                                float y = start_y + i * 52.0f;
                                if (y > 200.0f || y + 48.0f < 34.0f) continue;
                                
                                HitRect del_rect = { 270.0f, y, 40.0f, 48.0f };
                                if (touch_hit(touch.px, touch.py, &del_rect)) {
                                    alarm_list_state.candidate_index = i;
                                    alarm_list_state.candidate_is_toggle = false;
                                    alarm_list_state.candidate_is_delete = true;
                                    break;
                                }

                                HitRect toggle_rect = { 230.0f, y, 38.0f, 48.0f };
                                if (touch_hit(touch.px, touch.py, &toggle_rect)) {
                                    alarm_list_state.candidate_index = i;
                                    alarm_list_state.candidate_is_toggle = true;
                                    alarm_list_state.candidate_is_delete = false;
                                    break;
                                }
                                
                                HitRect card_rect = { 10.0f, y, 220.0f, 48.0f };
                                if (touch_hit(touch.px, touch.py, &card_rect)) {
                                    alarm_list_state.candidate_index = i;
                                    alarm_list_state.candidate_is_toggle = false;
                                    alarm_list_state.candidate_is_delete = false;
                                    break;
                                }
                            }
                        } else if (tHeld && (alarm_list_state.potential_tap || alarm_list_state.is_dragging)) {
                            float delta_y = touch.py - alarm_list_state.touch_start_y;
                            if (!alarm_list_state.is_dragging) {
                                if (fabsf(delta_y) > TOUCH_SLOP_PX) {
                                    alarm_list_state.is_dragging = true;
                                    alarm_list_state.potential_tap = false;
                                    alarm_list_state.candidate_index = -1;
                                }
                            }
                            if (alarm_list_state.is_dragging) {
                                alarm_list_state.scroll_y -= delta_y;
                                alarm_list_state.touch_start_y = touch.py;
                            }
                        } else if (!tHeld && (alarm_list_state.potential_tap || alarm_list_state.is_dragging)) {
                            /* Touch release: register tap only if within slop threshold */
                            if (alarm_list_state.potential_tap && !alarm_list_state.is_dragging) {
                                int idx = alarm_list_state.candidate_index;
                                if (idx >= 0 && idx < save.alarm_count) {
                                    if (alarm_list_state.candidate_is_toggle) {
                                        alarm_toggle_entry(&save, idx);
                                    } else if (alarm_list_state.candidate_is_delete) {
                                        edit_alarm_idx = idx;
                                        alarm_list_state.selected_index = idx;
                                        show_delete_confirm = true;
                                    } else {
                                        alarm_list_state.selected_index = idx;
                                        edit_alarm_idx = idx;
                                        AlarmEntry* a = &save.alarms[idx];
                                        edit_alarm_h = a->hour;
                                        edit_alarm_m = a->minute;
                                        edit_alarm_repeat = a->repeat_mode;
                                        edit_alarm_tone = a->ringtone_id;
                                        snprintf(edit_alarm_label, sizeof(edit_alarm_label), "%s", a->label);
                                        memset(hr_alarm, 0, sizeof(hr_alarm));
                                        show_delete_confirm = false;
                                        alarm_view = STATE_ALARM_EDIT;
                                    }
                                }
                            }
                            alarm_list_state.is_dragging = false;
                            alarm_list_state.potential_tap = false;
                            alarm_list_state.candidate_index = -1;
                        }
                        
                        /* Clamp scroll */
                        float max_scroll = (save.alarm_count * 52.0f) - 164.0f;
                        if (max_scroll < 0.0f) max_scroll = 0.0f;
                        if (alarm_list_state.scroll_y < 0.0f) alarm_list_state.scroll_y = 0.0f;
                        if (alarm_list_state.scroll_y > max_scroll) alarm_list_state.scroll_y = max_scroll;
                    }
                    break;

                case MODE_TIMER:
                    if (show_timer_zero_modal) {
                        if ((kDown & (KEY_A | KEY_B)) || (tDown && touch_hit(touch.px, touch.py, &BTN_OK))) {
                            show_timer_zero_modal = false;
                        }
                    } else if (tmr.state == TMR_ADJUST || tmr.state == TMR_EXPIRED) {
                        if (tmr.state == TMR_EXPIRED) {
                            tmr.state = TMR_ADJUST;
                        }
                        if ((kDown & KEY_A) || (tDown && touch_hit(touch.px, touch.py, &BTN_TMR_START))) {
                            if (tmr.target_h == 0 && tmr.target_m == 0 && tmr.target_s == 0) {
                                show_timer_zero_modal = true;
                            } else {
                                timer_start(&tmr);
                            }
                        } else {
                            hold_repeat_update(&hr_time[0], tHeld && touch_hit(touch.px, touch.py, &ARROW_H_UP));
                            hold_repeat_update(&hr_time[1], tHeld && touch_hit(touch.px, touch.py, &ARROW_H_DOWN));
                            hold_repeat_update(&hr_time[2], tHeld && touch_hit(touch.px, touch.py, &ARROW_M_UP));
                            hold_repeat_update(&hr_time[3], tHeld && touch_hit(touch.px, touch.py, &ARROW_M_DOWN));
                            hold_repeat_update(&hr_time[4], tHeld && touch_hit(touch.px, touch.py, &ARROW_S_UP));
                            hold_repeat_update(&hr_time[5], tHeld && touch_hit(touch.px, touch.py, &ARROW_S_DOWN));

                            if (hr_time[0].triggered) tmr.target_h = (tmr.target_h +  1) % 100;
                            if (hr_time[1].triggered) tmr.target_h = (tmr.target_h + 99) % 100;
                            if (hr_time[2].triggered) tmr.target_m = (tmr.target_m +  1) % 60;
                            if (hr_time[3].triggered) tmr.target_m = (tmr.target_m + 59) % 60;
                            if (hr_time[4].triggered) tmr.target_s = (tmr.target_s +  1) % 60;
                            if (hr_time[5].triggered) tmr.target_s = (tmr.target_s + 59) % 60;
                        }
                    } else if (tmr.state == TMR_RUNNING) {
                        if ((kDown & KEY_A) || (tDown && touch_hit(touch.px, touch.py, &BTN_TMR_PAUSE)))
                            timer_pause(&tmr);
                        else if ((kDown & KEY_B) || (tDown && touch_hit(touch.px, touch.py, &BTN_TMR_RESET)))
                            timer_reset(&tmr);
                    } else if (tmr.state == TMR_PAUSED) {
                        if ((kDown & KEY_A) || (tDown && touch_hit(touch.px, touch.py, &BTN_TMR_RESUME)))
                            timer_resume(&tmr);
                        else if ((kDown & KEY_B) || (tDown && touch_hit(touch.px, touch.py, &BTN_TMR_RESET)))
                            timer_reset(&tmr);
                    }
                    break;
                }
            }
        }

        /* ========================================================== */
        /*  Rendering                                                 */
        /* ========================================================== */

        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
        C2D_TextBufClear(textBuf);

        /* --- Top Screen Rendering --- */
        C2D_TargetClear(top, CLR_BG);
        C2D_SceneBegin(top);

        if (alarm_sys.state == ALARM_STATE_RINGING) {
            int first_ringing = __builtin_ctz(alarm_sys.ringing_mask);
            ui_draw_alarm_ringing_top(textBuf, save.alarms[first_ringing].hour, save.alarms[first_ringing].minute, save.alarms[first_ringing].repeat_mode, save.alarms[first_ringing].label, alarm_sys.ring_frames);
            alarm_sys.ring_frames++;
        } else if (timer_ringing) {
            ui_draw_timer_ringing_top(textBuf, tmr.target_h, tmr.target_m, tmr.target_s, timer_ring_frames);
            timer_ring_frames++;
        } else if (active_mode == MODE_ALARM) {
            int h, m, s;
            int y, mo, d;
            clock_get_hms(&h, &m, &s);
            clock_get_ymd(&y, &mo, &d);

            char date_str[64];
            format_date_string(date_str, sizeof(date_str), y, mo, d, (DateFormat)save.date_format);

            now_sec = get_display_time_seconds();
            s64 next_fire = -1;
            int next_idx = -1;
            for (int i = 0; i < save.alarm_count; i++) {
                if (!save.alarms[i].enabled) continue;
                s64 fire = alarm_calc_next_fire_epoch(now_sec, &save.alarms[i]);
                if (fire != -1) {
                    if (next_fire == -1 || fire < next_fire) {
                        next_fire = fire;
                        next_idx = i;
                    }
                }
            }

            char alarm_status_str[64];
            if (next_idx != -1) {
                if (save.alarms[next_idx].label[0] != '\0') {
                    snprintf(alarm_status_str, sizeof(alarm_status_str), "Next alarm - %02d:%02d (%s)",
                             save.alarms[next_idx].hour, save.alarms[next_idx].minute, save.alarms[next_idx].label);
                } else {
                    snprintf(alarm_status_str, sizeof(alarm_status_str), "Next alarm - %02d:%02d",
                             save.alarms[next_idx].hour, save.alarms[next_idx].minute);
                }
            } else {
                snprintf(alarm_status_str, sizeof(alarm_status_str), "Next alarm - None");
            }

            ui_draw_top_clock_with_alarm_status(textBuf, h, m, s, date_str, alarm_status_str);
        } else if (is_settings) {
            int h, m, s;
            int y, mo, d;
            clock_get_hms(&h, &m, &s);
            clock_get_ymd(&y, &mo, &d);

            char date_str[64];
            format_date_string(date_str, sizeof(date_str), y, mo, d, (DateFormat)save.date_format);
            ui_draw_top_clock_with_date(textBuf, h, m, s, date_str);
        } else if (active_mode == MODE_CLOCK) {
            int h, m, s;
            int y, mo, d;
            clock_get_hms(&h, &m, &s);
            clock_get_ymd(&y, &mo, &d);

            char date_str[64];
            format_date_string(date_str, sizeof(date_str), y, mo, d, (DateFormat)save.date_format);
            const CityTimezone* home_tz = world_clock_get_city_info(save.home_city_id);
            ui_draw_top_clock_with_home(textBuf, h, m, s, date_str, home_tz->city, home_tz->country);
        } else if (active_mode == MODE_STOPWATCH) {
            int sh, sm, ss, sc;
            bool show_hours;
            stopwatch_get_display(&sw, &sh, &sm, &ss, &sc, &show_hours);
            ui_draw_top_stopwatch(textBuf, &sw, sh, sm, ss, sc, show_hours);
        } else if (active_mode == MODE_TIMER) {
            int th, tm, ts;
            timer_get_display(&tmr, &th, &tm, &ts);
            ui_draw_top_timer(textBuf, th, tm, ts);
        }

        /* Persistent Top Screen Telemetry Status Bar */
        if (alarm_sys.state != ALARM_STATE_RINGING && !timer_ringing) {
            ui_draw_top_status_bar(textBuf, telemetry_wifi_bars, telemetry_battery_percent, telemetry_is_charging);
        }

        /* --- Bottom Screen Rendering --- */
        if (s_screen_mode != SCREEN_MODE_BOTTOM_OFF) {
            C2D_TargetClear(bot, CLR_BG);
            C2D_SceneBegin(bot);

            if (alarm_sys.state == ALARM_STATE_RINGING) {
                int first_ringing = __builtin_ctz(alarm_sys.ringing_mask);
                ui_draw_alarm_ringing_bottom(textBuf, save.alarms[first_ringing].hour, save.alarms[first_ringing].minute, save.alarms[first_ringing].repeat_mode, save.alarms[first_ringing].label);
            } else if (timer_ringing) {
                ui_draw_timer_ringing_bottom(textBuf, tmr.target_h, tmr.target_m, tmr.target_s);
            } else if (alarm_sys.missed_alarm) {
                ui_draw_alarm_missed_modal(textBuf, alarm_sys.missed_count);
            } else if (is_first_boot) {
                ui_draw_first_boot(textBuf, first_boot_frames > 30);
            } else if (is_settings) {
                switch (settings_sub) {
                case SET_MAIN:
                    ui_draw_settings_main(textBuf);
                    break;
                case SET_TIME_DATE_MENU:
                    ui_draw_settings_time_date_menu(textBuf);
                    break;
                case SET_EDIT_TIME:
                    ui_draw_settings_edit_time(textBuf, edit_h, edit_m, edit_s);
                    break;
                case SET_EDIT_DATE:
                    ui_draw_settings_edit_date(textBuf, edit_y, edit_mo, edit_d, edit_fmt);
                    break;
                case SET_CONFIRM_RESET:
                    ui_draw_settings_time_date_menu(textBuf);
                    ui_draw_modal_confirm(textBuf);
                    break;
                case SET_CONFIRM_RESET_TIME:
                    ui_draw_settings_edit_time(textBuf, edit_h, edit_m, edit_s);
                    ui_draw_modal_confirm_reset_time(textBuf);
                    break;
                case SET_SAVE_OK:
                    ui_draw_settings_time_date_menu(textBuf);
                    ui_draw_modal_success(textBuf, save_msg);
                    break;
                case SET_DISPLAY:
                    ui_draw_settings_display(textBuf, save.auto_sleep_idx);
                    break;
                case SET_MANUAL:
                    manual_draw_bottom(textBuf, manual_topic_idx, manual_scroll_y);
                    break;
                case SET_ABOUT:
                    about_draw_bottom(textBuf, about_scroll_y);
                    break;
                }
            } else if (alarm_view == STATE_ALARM_ADD || alarm_view == STATE_ALARM_EDIT) {
                const char* rname = audio_get_ringtone_name(edit_alarm_tone);
                bool is_new = (alarm_view == STATE_ALARM_ADD);
                ui_draw_alarm_edit(textBuf, edit_alarm_h, edit_alarm_m, edit_alarm_repeat, edit_alarm_tone, is_new, rname, edit_alarm_label, audio_is_playing());
            } else {
                /* Draw global header bar for non-alarm/non-clock tabs (alarm list and world clock draw their own headers) */
                if (active_mode != MODE_ALARM && active_mode != MODE_CLOCK) {
                    const char* title = "Clock";
                    if (active_mode == MODE_STOPWATCH) title = "Stopwatch";
                    else if (active_mode == MODE_TIMER) title = "Timer";
                    ui_draw_header(textBuf, settings_icon, title);
                }

                /* Draw mode content */
                switch (active_mode) {
                case MODE_ALARM:
                    ui_draw_alarm_list(textBuf, &save, &alarm_list_state, settings_icon, trash_icon, add_icon);
                    if (show_delete_confirm) {
                        ui_draw_alarm_delete_confirm(textBuf);
                    }
                    break;

                case MODE_CLOCK:
                    if (clock_view == CLOCK_VIEW_PICKER) {
                        ui_draw_city_picker(textBuf, &save, &city_picker_state);
                    } else {
                        ui_draw_world_clock_list(textBuf, &save, &world_clock_state, settings_icon, trash_icon, add_icon);
                        if (confirm_del_city_idx >= 0 && confirm_del_city_idx < save.world_city_count) {
                            ui_draw_world_clock_delete_confirm(textBuf, save.world_cities[confirm_del_city_idx]);
                        } else if (confirm_home_city_id >= 0) {
                            ui_draw_world_clock_set_home_confirm(textBuf, (u8)confirm_home_city_id);
                        } else if (alert_home_city_id >= 0) {
                            ui_draw_world_clock_already_home(textBuf, (u8)alert_home_city_id);
                        }
                    }
                    break;

                case MODE_STOPWATCH:
                    if (sw.state == SW_IDLE)
                        ui_draw_stopwatch_idle(textBuf);
                    else if (sw.state == SW_RUNNING)
                        ui_draw_stopwatch_running(textBuf);
                    else if (sw.state == SW_PAUSED)
                        ui_draw_stopwatch_paused(textBuf);
                    break;

                case MODE_TIMER:
                    if (tmr.state == TMR_ADJUST || tmr.state == TMR_EXPIRED)
                        ui_draw_timer_adjust(textBuf, tmr.target_h, tmr.target_m, tmr.target_s);
                    else if (tmr.state == TMR_RUNNING)
                        ui_draw_timer_running(textBuf);
                    else if (tmr.state == TMR_PAUSED)
                        ui_draw_timer_paused(textBuf);

                    if (show_timer_zero_modal) {
                        ui_draw_modal_timer_zero(textBuf);
                    }
                    break;
                }

                /* Draw persistent tab bar */
                if (!(active_mode == MODE_CLOCK && clock_view == CLOCK_VIEW_PICKER)) {
                    ui_draw_tab_bar(textBuf, active_mode, tab_icons);
                }
            }
        }

        C3D_FrameEnd(0);
    }

    aptUnhook(&apt_cookie);

    /* --- Cleanup (reverse init order) --- */
    if (sprite_sheet) {
        C2D_SpriteSheetFree(sprite_sheet);
    }
    C2D_TextBufDelete(textBuf);
    C2D_Fini();
    C3D_Fini();
    cfguExit();
    return 0;
}
