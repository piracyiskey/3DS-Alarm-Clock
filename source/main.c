#include <3ds.h>
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
/* Binary assets generated from gfx/icons.t3s */
extern const u8 icons_t3x[];
extern const u8 icons_t3x_end[];
#define icons_t3x_size     ((u32)(icons_t3x_end - icons_t3x))
#define icons_settings_idx 0

#define SOC_ALIGN      0x1000
#define SOC_BUFFERSIZE 0x100000
static u32* SOC_buffer = NULL;

static void socShutdown(void) { socExit(); }

/* Settings sub-states */
typedef enum {
    SET_MAIN,
    SET_EDIT_TIME,
    SET_EDIT_DATE,
    SET_CONFIRM_RESET,
    SET_SAVE_OK
} SettingsSubState;

/* Hold-repeat helper for touch arrows */
typedef struct {
    u32  frames;
    bool triggered;
} HoldRepeat;

static bool touch_hit(u16 px, u16 py, const HitRect* r)
{
    return px >= r->x && px < r->x + r->w &&
           py >= r->y && py < r->y + r->h;
}

static void hold_repeat_update(HoldRepeat* hr, bool held)
{
    hr->triggered = false;
    if (!held) { hr->frames = 0; return; }

    if (hr->frames == 0)
        hr->triggered = true;                           /* first press */
    else if (hr->frames >= 30 && (hr->frames - 30) % 6 == 0)
        hr->triggered = true;                           /* repeat ~10/s */

    hr->frames++;
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

    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
    C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
    C2D_Prepare();
    cfguInit();

    C3D_RenderTarget* top = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
    C3D_RenderTarget* bot = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);
    C2D_TextBuf textBuf   = C2D_TextBufNew(256);

    /* --- Sprite Sheet Init --- */
    C2D_SpriteSheet sprite_sheet = C2D_SpriteSheetLoadFromMem(icons_t3x, icons_t3x_size);
    C2D_Image settings_icon = { NULL, NULL };
    if (sprite_sheet) {
        settings_icon = C2D_SpriteSheetGetImage(sprite_sheet, icons_settings_idx);
    }

    audio_init();
    atexit(audio_exit);

    /* --- Clock & Save init --- */
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

    Stopwatch sw;
    stopwatch_init(&sw);

    Timer tmr;
    timer_init(&tmr);

    AlarmSystem alarm_sys;
    alarm_sys_init(&alarm_sys);

    /* Arrow edit state */
    int edit_h = 0, edit_m = 0, edit_s = 0;
    int edit_y = 2026, edit_mo = 1, edit_d = 1;
    DateFormat edit_fmt = DATEFMT_EUR;

    AlarmView alarm_view = ALARM_VIEW_LIST;
    AlarmListState alarm_list_state = {0.0f, 0.0f, false, -1};
    int edit_alarm_idx = -1;
    int edit_alarm_h = 0, edit_alarm_m = 0;
    u8 edit_alarm_repeat = REPEAT_ONCE;
    u8 edit_alarm_tone = 0;
    bool show_delete_confirm = false;

    HoldRepeat hr_time[6];      /* h↑ h↓ m↑ m↓ s↑ s↓ */
    HoldRepeat hr_date[6];      /* col1↑ col1↓ col2↑ col2↓ col3↑ col3↓ */
    HoldRepeat hr_alarm[4];     /* alarm h↑ h↓ m↑ m↓ */
    memset(hr_time, 0, sizeof(hr_time));
    memset(hr_date, 0, sizeof(hr_date));
    memset(hr_alarm, 0, sizeof(hr_alarm));

    /* === Main loop === */
    while (aptMainLoop()) {
        alarm_tick(&save, &alarm_sys);
        audio_tick();

        hidScanInput();
        u32 kDown = hidKeysDown();
        u32 kHeld = hidKeysHeld();

        if (kDown & KEY_START) break;

        touchPosition touch;
        hidTouchRead(&touch);
        bool tDown = (kDown & KEY_TOUCH) != 0;
        bool tHeld = (kHeld & KEY_TOUCH) != 0;

        /* Update timer expiration status */
        timer_is_expired(&tmr);

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
            if ((kDown & KEY_A) || (tDown && touch_hit(touch.px, touch.py, &BTN_ALARM_DISMISS))) {
                alarm_dismiss_all(&save, &alarm_sys);
            }
        }
        else if (alarm_sys.missed_alarm) {
            /* Missed alarm modal dismiss */
            if ((kDown & KEY_A) || (tDown && touch_hit(touch.px, touch.py, &BTN_ALARM_MISSED_OK))) {
                alarm_sys.missed_alarm = false;
            }
        }
        else if (!is_settings && active_mode == MODE_TIMER && tmr.state == TMR_EXPIRED) {
            /* Timer expired modal */
            if ((kDown & KEY_A) ||
                (tDown && touch_hit(touch.px, touch.py, &BTN_OK))) {
                timer_dismiss(&tmr);
            }
        }
        else if (is_settings) {
            /* Settings overlay views */
            switch (settings_sub) {
            case SET_MAIN:
                if ((kDown & KEY_B) ||
                    (tDown && touch_hit(touch.px, touch.py, &BTN_SET_BACK))) {
                    is_settings = false;
                } else if (tDown && touch_hit(touch.px, touch.py, &BTN_SET_EDIT)) {
                    clock_get_hms(&edit_h, &edit_m, &edit_s);
                    clock_get_ymd(&edit_y, &edit_mo, &edit_d);
                    edit_fmt = (DateFormat)save.date_format;
                    memset(hr_time, 0, sizeof(hr_time));
                    memset(hr_date, 0, sizeof(hr_date));
                    settings_sub = SET_EDIT_TIME;
                } else if (tDown && touch_hit(touch.px, touch.py, &BTN_SET_RESET)) {
                    settings_sub = SET_CONFIRM_RESET;
                }
                break;

            case SET_CONFIRM_RESET:
                if (tDown) {
                    if (touch_hit(touch.px, touch.py, &BTN_CONFIRM)) {
                        clock_reset(&save);
                        save_msg = "Reset to system time & date!";
                        settings_sub = SET_SAVE_OK;
                    } else if (touch_hit(touch.px, touch.py, &BTN_CANCEL)) {
                        settings_sub = SET_MAIN;
                    }
                }
                if (kDown & KEY_B) {
                    settings_sub = SET_MAIN;
                }
                break;

            case SET_EDIT_TIME:
                if ((kDown & KEY_B) ||
                    (tDown && touch_hit(touch.px, touch.py, &BTN_SET_BACK))) {
                    settings_sub = SET_MAIN;
                } else if (tDown && touch_hit(touch.px, touch.py, &BTN_SET_SAVE)) {
                    clock_apply_time_edit(edit_h, edit_m, edit_s, &save);
                    save_msg = "Time saved successfully!";
                    settings_sub = SET_SAVE_OK;
                } else if (tDown && touch_hit(touch.px, touch.py, &BTN_EDIT_DATE)) {
                    memset(hr_date, 0, sizeof(hr_date));
                    settings_sub = SET_EDIT_DATE;
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
                    settings_sub = SET_EDIT_TIME;
                } else if (tDown && touch_hit(touch.px, touch.py, &BTN_SET_SAVE)) {
                    save.date_format = (u8)edit_fmt;
                    clock_apply_date_edit(edit_y, edit_mo, edit_d, &save);
                    save_msg = "Date saved successfully!";
                    settings_sub = SET_SAVE_OK;
                } else if (tDown && touch_hit(touch.px, touch.py, &BTN_FMT_LEFT)) {
                    edit_fmt = (edit_fmt == DATEFMT_ISO) ? DATEFMT_US : (DateFormat)(edit_fmt - 1);
                } else if (tDown && touch_hit(touch.px, touch.py, &BTN_FMT_RIGHT)) {
                    edit_fmt = (edit_fmt == DATEFMT_US) ? DATEFMT_ISO : (DateFormat)(edit_fmt + 1);
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
                if ((kDown & KEY_A) ||
                    (tDown && touch_hit(touch.px, touch.py, &BTN_OK))) {
                    settings_sub = SET_MAIN;
                }
                break;
            }
        }
        else if (alarm_view == STATE_ALARM_ADD || alarm_view == STATE_ALARM_EDIT) {
            /* ====================================================== */
            /*  Dedicated Full-Screen Modal Input for Add / Edit Alarm*/
            /* ====================================================== */
            if (show_delete_confirm) {
                if ((kDown & KEY_B) || (tDown && touch_hit(touch.px, touch.py, &BTN_CANCEL))) {
                    show_delete_confirm = false;
                } else if ((kDown & KEY_A) || (tDown && touch_hit(touch.px, touch.py, &BTN_CONFIRM))) {
                    if (edit_alarm_idx >= 0 && edit_alarm_idx < save.alarm_count) {
                        alarm_delete(&save, edit_alarm_idx);
                        save_write(&save);
                        alarm_list_state.selected_index = -1;
                    }
                    show_delete_confirm = false;
                    alarm_view = ALARM_VIEW_LIST;
                }
            } else {
                bool is_new = (alarm_view == STATE_ALARM_ADD);

                if ((kDown & KEY_B) || (tDown && touch_hit(touch.px, touch.py, &BTN_ALARM_EDIT_CANCEL))) {
                    show_delete_confirm = false;
                    alarm_view = ALARM_VIEW_LIST;
                } else if (tDown && touch_hit(touch.px, touch.py, &BTN_ALARM_EDIT_SAVE)) {
                    if (is_new) {
                        alarm_add(&save, edit_alarm_h, edit_alarm_m, edit_alarm_repeat, edit_alarm_tone);
                    } else if (edit_alarm_idx >= 0 && edit_alarm_idx < save.alarm_count) {
                        save.alarms[edit_alarm_idx].hour = edit_alarm_h;
                        save.alarms[edit_alarm_idx].minute = edit_alarm_m;
                        save.alarms[edit_alarm_idx].repeat_mode = edit_alarm_repeat;
                        save.alarms[edit_alarm_idx].ringtone_id = edit_alarm_tone;
                        save.alarms[edit_alarm_idx].enabled = true; // Auto-enable on edit
                    }
                    save_write(&save);
                    show_delete_confirm = false;
                    alarm_view = ALARM_VIEW_LIST;
                } else if (!is_new && tDown && touch_hit(touch.px, touch.py, &BTN_ALARM_EDIT_DELETE)) {
                    show_delete_confirm = true;
                } else if (tDown && touch_hit(touch.px, touch.py, &BTN_ALARM_TONE_LEFT)) {
                    if (edit_alarm_tone == 0) edit_alarm_tone = audio_get_ringtone_count() - 1;
                    else edit_alarm_tone--;
                } else if (tDown && touch_hit(touch.px, touch.py, &BTN_ALARM_TONE_RIGHT)) {
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
        }
        else {
            /* Normal Mode Navigation (Tab bar visible at bottom) */
            bool tab_touched = false;
            if (tDown && touch.py >= 200) {
                if (touch_hit(touch.px, touch.py, &TAB_ALARM)) {
                    active_mode = MODE_ALARM;
                    tab_touched = true;
                } else if (touch_hit(touch.px, touch.py, &TAB_CLOCK)) {
                    active_mode = MODE_CLOCK;
                    tab_touched = true;
                } else if (touch_hit(touch.px, touch.py, &TAB_STOPWATCH)) {
                    active_mode = MODE_STOPWATCH;
                    tab_touched = true;
                } else if (touch_hit(touch.px, touch.py, &TAB_TIMER)) {
                    active_mode = MODE_TIMER;
                    tab_touched = true;
                }
            }

            /* Settings icon touched in top-right of header */
            if (!tab_touched && tDown && touch_hit(touch.px, touch.py, &BTN_SETTINGS_ICON)) {
                is_settings  = true;
                settings_sub = SET_MAIN;
                tab_touched  = true;
            }

            if (!tab_touched) {
                switch (active_mode) {
                case MODE_CLOCK:
                    /* No controls on bottom screen */
                    break;

                case MODE_STOPWATCH:
                    if (sw.state == SW_IDLE) {
                        if (tDown && touch_hit(touch.px, touch.py, &BTN_SW_START))
                            stopwatch_start(&sw);
                    } else if (sw.state == SW_RUNNING) {
                        if (tDown && touch_hit(touch.px, touch.py, &BTN_SW_PAUSE))
                            stopwatch_pause(&sw);
                        else if (tDown && touch_hit(touch.px, touch.py, &BTN_SW_RESET))
                            stopwatch_reset(&sw);
                    } else if (sw.state == SW_PAUSED) {
                        if (tDown && touch_hit(touch.px, touch.py, &BTN_SW_RESUME))
                            stopwatch_resume(&sw);
                        else if (tDown && touch_hit(touch.px, touch.py, &BTN_SW_RESET))
                            stopwatch_reset(&sw);
                    }
                    break;

                case MODE_ALARM:
                    if (tDown && touch_hit(touch.px, touch.py, &BTN_ALARM_ADD)) {
                        if (save.alarm_count < MAX_ALARMS) {
                            edit_alarm_idx = -1;
                            clock_get_hms(&edit_alarm_h, &edit_alarm_m, &edit_s);
                            edit_alarm_repeat = REPEAT_ONCE;
                            edit_alarm_tone = 0;
                            memset(hr_alarm, 0, sizeof(hr_alarm));
                            show_delete_confirm = false;
                            alarm_view = STATE_ALARM_ADD;
                        }
                    } else if (kDown & KEY_DDOWN) {
                        alarm_list_state.selected_index++;
                        if (alarm_list_state.selected_index >= save.alarm_count) alarm_list_state.selected_index = save.alarm_count - 1;
                        // Basic auto-scroll (simplification: jump to show)
                        float y = 36.0f - alarm_list_state.scroll_y + alarm_list_state.selected_index * 52.0f;
                        if (y > 150.0f) alarm_list_state.scroll_y += (y - 150.0f);
                    } else if (kDown & KEY_DUP) {
                        alarm_list_state.selected_index--;
                        if (alarm_list_state.selected_index < 0) alarm_list_state.selected_index = 0;
                        float y = 36.0f - alarm_list_state.scroll_y + alarm_list_state.selected_index * 52.0f;
                        if (y < 36.0f) alarm_list_state.scroll_y -= (36.0f - y);
                    } else if (kDown & KEY_A && alarm_list_state.selected_index >= 0) {
                        edit_alarm_idx = alarm_list_state.selected_index;
                        AlarmEntry* a = &save.alarms[edit_alarm_idx];
                        edit_alarm_h = a->hour;
                        edit_alarm_m = a->minute;
                        edit_alarm_repeat = a->repeat_mode;
                        edit_alarm_tone = a->ringtone_id;
                        memset(hr_alarm, 0, sizeof(hr_alarm));
                        show_delete_confirm = false;
                        alarm_view = STATE_ALARM_EDIT;
                    } else if (tDown && touch.py >= 34 && touch.py <= 198) {
                        /* Touch in scroll area */
                        alarm_list_state.is_dragging = true;
                        alarm_list_state.touch_start_y = touch.py;
                        
                        /* Check for hits on cards or toggle boxes */
                        float start_y = 36.0f - alarm_list_state.scroll_y;
                        for (int i = 0; i < save.alarm_count; i++) {
                            float y = start_y + i * 52.0f;
                            if (y > 200.0f || y + 48.0f < 34.0f) continue;
                            
                            /* Toggle box hit rect (roughly 300 - 30, y + 14, 20x20) */
                            HitRect toggle_rect = { 300.0f - 10.0f - 20.0f - 10.0f, y, 40.0f, 48.0f }; // padded
                            if (touch_hit(touch.px, touch.py, &toggle_rect)) {
                                save.alarms[i].enabled = !save.alarms[i].enabled;
                                save_write(&save);
                                alarm_list_state.is_dragging = false; // consume
                                break;
                            }
                            
                            /* Card hit rect */
                            HitRect card_rect = { 10.0f, y, 290.0f - 40.0f, 48.0f }; // Exclude toggle
                            if (touch_hit(touch.px, touch.py, &card_rect)) {
                                alarm_list_state.selected_index = i;
                                edit_alarm_idx = i;
                                AlarmEntry* a = &save.alarms[i];
                                edit_alarm_h = a->hour;
                                edit_alarm_m = a->minute;
                                edit_alarm_repeat = a->repeat_mode;
                                edit_alarm_tone = a->ringtone_id;
                                memset(hr_alarm, 0, sizeof(hr_alarm));
                                show_delete_confirm = false;
                                alarm_view = STATE_ALARM_EDIT;
                                alarm_list_state.is_dragging = false;
                                break;
                            }
                        }
                    } else if (tHeld && alarm_list_state.is_dragging) {
                        float delta_y = touch.py - alarm_list_state.touch_start_y;
                        alarm_list_state.scroll_y -= delta_y;
                        alarm_list_state.touch_start_y = touch.py;
                    } else if (!tHeld) {
                        alarm_list_state.is_dragging = false;
                    }
                    
                    /* Clamp scroll */
                    float max_scroll = (save.alarm_count * 52.0f) - 164.0f;
                    if (max_scroll < 0.0f) max_scroll = 0.0f;
                    if (alarm_list_state.scroll_y < 0.0f) alarm_list_state.scroll_y = 0.0f;
                    if (alarm_list_state.scroll_y > max_scroll) alarm_list_state.scroll_y = max_scroll;
                    break;

                case MODE_TIMER:
                    if (tmr.state == TMR_ADJUST) {
                        if (tDown && touch_hit(touch.px, touch.py, &BTN_TMR_START)) {
                            timer_start(&tmr);
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
                        if (tDown && touch_hit(touch.px, touch.py, &BTN_TMR_PAUSE))
                            timer_pause(&tmr);
                        else if (tDown && touch_hit(touch.px, touch.py, &BTN_TMR_RESET))
                            timer_reset(&tmr);
                    } else if (tmr.state == TMR_PAUSED) {
                        if (tDown && touch_hit(touch.px, touch.py, &BTN_TMR_RESUME))
                            timer_resume(&tmr);
                        else if (tDown && touch_hit(touch.px, touch.py, &BTN_TMR_RESET))
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
            ui_draw_alarm_ringing_top(textBuf, save.alarms[first_ringing].hour, save.alarms[first_ringing].minute, save.alarms[first_ringing].repeat_mode, alarm_sys.ring_frames);
            alarm_sys.ring_frames++;
        } else if (is_settings || active_mode == MODE_ALARM || active_mode == MODE_CLOCK) {
            int h, m, s;
            int y, mo, d;
            clock_get_hms(&h, &m, &s);
            clock_get_ymd(&y, &mo, &d);

            char date_str[64];
            format_date_string(date_str, sizeof(date_str), y, mo, d, (DateFormat)save.date_format);
            ui_draw_top_clock_with_date(textBuf, h, m, s, date_str);
        } else if (active_mode == MODE_STOPWATCH) {
            int sh, sm, ss, sc;
            bool show_hours;
            stopwatch_get_display(&sw, &sh, &sm, &ss, &sc, &show_hours);
            ui_draw_top_stopwatch(textBuf, sh, sm, ss, sc, show_hours);
        } else if (active_mode == MODE_TIMER) {
            int th, tm, ts;
            timer_get_display(&tmr, &th, &tm, &ts);
            ui_draw_top_timer(textBuf, th, tm, ts);
        }

        /* --- Bottom Screen Rendering --- */
        C2D_TargetClear(bot, CLR_BG);
        C2D_SceneBegin(bot);

        if (is_first_boot) {
            ui_draw_first_boot(textBuf, first_boot_frames > 30);
        } else if (is_settings) {
            switch (settings_sub) {
            case SET_MAIN:
                ui_draw_settings_main(textBuf);
                break;
            case SET_EDIT_TIME:
                ui_draw_settings_edit_time(textBuf, edit_h, edit_m, edit_s);
                break;
            case SET_EDIT_DATE:
                ui_draw_settings_edit_date(textBuf, edit_y, edit_mo, edit_d, edit_fmt);
                break;
            case SET_CONFIRM_RESET:
                ui_draw_settings_main(textBuf);
                ui_draw_modal_confirm(textBuf);
                break;
            case SET_SAVE_OK:
                ui_draw_settings_main(textBuf);
                ui_draw_modal_success(textBuf, save_msg);
                break;
            }
        } else if (alarm_view == STATE_ALARM_ADD || alarm_view == STATE_ALARM_EDIT) {
            const char* rname = audio_get_ringtone_name(edit_alarm_tone);
            bool is_new = (alarm_view == STATE_ALARM_ADD);
            ui_draw_alarm_edit(textBuf, edit_alarm_h, edit_alarm_m, edit_alarm_repeat, edit_alarm_tone, is_new, rname);
            if (show_delete_confirm) {
                ui_draw_alarm_delete_confirm(textBuf);
            }
        } else {
            /* Draw global header bar for non-alarm tabs (alarm list draws its header over list to clip cards) */
            if (active_mode != MODE_ALARM) {
                const char* title = "Clock";
                if (active_mode == MODE_STOPWATCH) title = "Stopwatch";
                else if (active_mode == MODE_TIMER) title = "Timer";
                ui_draw_header(textBuf, settings_icon, title);
            }

            /* Draw mode content */
            switch (active_mode) {
            case MODE_ALARM:
                ui_draw_alarm_list(textBuf, &save, &alarm_list_state, settings_icon);
                break;

            case MODE_CLOCK:
                ui_draw_clock_bottom(textBuf);
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
                if (tmr.state == TMR_ADJUST)
                    ui_draw_timer_adjust(textBuf, tmr.target_h, tmr.target_m, tmr.target_s);
                else if (tmr.state == TMR_RUNNING)
                    ui_draw_timer_running(textBuf);
                else if (tmr.state == TMR_PAUSED)
                    ui_draw_timer_paused(textBuf);
                else if (tmr.state == TMR_EXPIRED) {
                    ui_draw_timer_adjust(textBuf, tmr.target_h, tmr.target_m, tmr.target_s);
                    ui_draw_timer_expired_modal(textBuf);
                }
                break;
            }

            /* Draw persistent tab bar if modal is not active */
            if (tmr.state != TMR_EXPIRED && alarm_sys.state != ALARM_STATE_RINGING && !alarm_sys.missed_alarm) {
                ui_draw_tab_bar(textBuf, active_mode);
            }

            /* Draw Alarm Modals over everything else */
            if (alarm_sys.state == ALARM_STATE_RINGING) {
                int first_ringing = __builtin_ctz(alarm_sys.ringing_mask);
                ui_draw_alarm_ringing_bottom(textBuf, save.alarms[first_ringing].hour, save.alarms[first_ringing].minute, save.alarms[first_ringing].repeat_mode);
            } else if (alarm_sys.missed_alarm) {
                ui_draw_alarm_missed_modal(textBuf);
            }
        }

        C3D_FrameEnd(0);
    }

    /* --- Cleanup (reverse init order) --- */
    if (sprite_sheet) {
        C2D_SpriteSheetFree(sprite_sheet);
    }
    C2D_TextBufDelete(textBuf);
    C2D_Fini();
    C3D_Fini();
    cfguExit();
    gfxExit();
    return 0;
}
