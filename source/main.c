#include <3ds.h>
#include <citro3d.h>
#include <citro2d.h>
#include <string.h>

#include "clock.h"
#include "save.h"
#include "ui.h"
#include "stopwatch.h"
#include "timer.h"
/* Binary assets generated from gfx/icons.t3s */
extern const u8 icons_t3x[];
extern const u8 icons_t3x_end[];
#define icons_t3x_size     ((u32)(icons_t3x_end - icons_t3x))
#define icons_settings_idx 0

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

    /* Arrow edit state */
    int edit_h = 0, edit_m = 0, edit_s = 0;
    int edit_y = 2026, edit_mo = 1, edit_d = 1;
    DateFormat edit_fmt = DATEFMT_EUR;

    HoldRepeat hr_time[6];      /* h↑ h↓ m↑ m↓ s↑ s↓ */
    HoldRepeat hr_date[6];      /* col1↑ col1↓ col2↑ col2↓ col3↑ col3↓ */
    memset(hr_time, 0, sizeof(hr_time));
    memset(hr_date, 0, sizeof(hr_date));

    /* === Main loop === */
    while (aptMainLoop()) {
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
                case MODE_ALARM:
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

        if (is_settings || active_mode == MODE_ALARM || active_mode == MODE_CLOCK) {
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
        } else {
            /* Draw global header bar at top */
            const char* title = "Clock";
            if (active_mode == MODE_ALARM) title = "Alarm";
            else if (active_mode == MODE_STOPWATCH) title = "Stopwatch";
            else if (active_mode == MODE_TIMER) title = "Timer";

            ui_draw_header(textBuf, settings_icon, title);

            /* Draw mode content */
            switch (active_mode) {
            case MODE_ALARM:
                ui_draw_alarm_placeholder(textBuf);
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

            /* Draw persistent tab bar if timer expired modal is not active */
            if (tmr.state != TMR_EXPIRED) {
                ui_draw_tab_bar(textBuf, active_mode);
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
