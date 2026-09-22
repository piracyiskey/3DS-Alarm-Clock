#include <3ds.h>
#include <citro3d.h>
#include <citro2d.h>
#include <string.h>

#include "clock.h"
#include "save.h"
#include "ui.h"
#include "stopwatch.h"
#include "timer.h"

/* Settings sub-states */
typedef enum {
    SET_MAIN,
    SET_EDIT,
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

    /* --- Clock & Save init --- */
    ClockSaveData save;
    bool is_first_boot = false;
    int first_boot_frames = 0;

    if (save_read(&save)) {
        clock_init(&save);
    } else {
        clock_init(NULL);
        is_first_boot = true;
        save_ensure_dir();
    }

    /* --- Multi-mode state --- */
    AppMode active_mode = MODE_CLOCK;
    SettingsSubState settings_sub = SET_MAIN;

    Stopwatch sw;
    stopwatch_init(&sw);

    Timer tmr;
    timer_init(&tmr);

    /* Shared arrow edit state */
    int edit_h = 0, edit_m = 0, edit_s = 0;
    HoldRepeat hr[6];           /* h↑ h↓ m↑ m↓ s↑ s↓ */
    memset(hr, 0, sizeof(hr));

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
        else if (active_mode == MODE_TIMER && tmr.state == TMR_EXPIRED) {
            /* Timer expired modal */
            if ((kDown & KEY_A) ||
                (tDown && touch_hit(touch.px, touch.py, &BTN_OK))) {
                timer_dismiss(&tmr);
            }
        }
        else if (active_mode == MODE_SETTINGS && settings_sub == SET_CONFIRM_RESET) {
            /* Reset confirmation modal */
            if (tDown) {
                if (touch_hit(touch.px, touch.py, &BTN_CONFIRM)) {
                    clock_reset();
                    save_delete();
                    settings_sub = SET_MAIN;
                } else if (touch_hit(touch.px, touch.py, &BTN_CANCEL)) {
                    settings_sub = SET_MAIN;
                }
            }
            if (kDown & KEY_B)
                settings_sub = SET_MAIN;
        }
        else if (active_mode == MODE_SETTINGS && settings_sub == SET_SAVE_OK) {
            /* Save success modal */
            if ((kDown & KEY_A) ||
                (tDown && touch_hit(touch.px, touch.py, &BTN_OK))) {
                settings_sub = SET_MAIN;
            }
        }
        else if (active_mode == MODE_SETTINGS && settings_sub == SET_EDIT) {
            /* Clock edit view */
            if ((kDown & KEY_B) ||
                (tDown && touch_hit(touch.px, touch.py, &BTN_SET_BACK))) {
                settings_sub = SET_MAIN;
            } else if (tDown && touch_hit(touch.px, touch.py, &BTN_SET_SAVE)) {
                ClockSaveData new_save;
                clock_apply_edit(edit_h, edit_m, edit_s, &new_save);
                save_write(&new_save);
                settings_sub = SET_SAVE_OK;
            } else {
                /* Arrow hold-repeat for Clock edit */
                hold_repeat_update(&hr[0], tHeld && touch_hit(touch.px, touch.py, &ARROW_H_UP));
                hold_repeat_update(&hr[1], tHeld && touch_hit(touch.px, touch.py, &ARROW_H_DOWN));
                hold_repeat_update(&hr[2], tHeld && touch_hit(touch.px, touch.py, &ARROW_M_UP));
                hold_repeat_update(&hr[3], tHeld && touch_hit(touch.px, touch.py, &ARROW_M_DOWN));
                hold_repeat_update(&hr[4], tHeld && touch_hit(touch.px, touch.py, &ARROW_S_UP));
                hold_repeat_update(&hr[5], tHeld && touch_hit(touch.px, touch.py, &ARROW_S_DOWN));

                if (hr[0].triggered) edit_h = (edit_h +  1) % 24;
                if (hr[1].triggered) edit_h = (edit_h + 23) % 24;
                if (hr[2].triggered) edit_m = (edit_m +  1) % 60;
                if (hr[3].triggered) edit_m = (edit_m + 59) % 60;
                if (hr[4].triggered) edit_s = (edit_s +  1) % 60;
                if (hr[5].triggered) edit_s = (edit_s + 59) % 60;
            }
        }
        else {
            /* Normal Mode Navigation (Tab bar visible at bottom) */
            bool tab_touched = false;
            if (tDown && touch.py >= 200) {
                if (touch_hit(touch.px, touch.py, &TAB_CLOCK)) {
                    active_mode = MODE_CLOCK;
                    tab_touched = true;
                } else if (touch_hit(touch.px, touch.py, &TAB_STOPWATCH)) {
                    active_mode = MODE_STOPWATCH;
                    tab_touched = true;
                } else if (touch_hit(touch.px, touch.py, &TAB_TIMER)) {
                    active_mode = MODE_TIMER;
                    tab_touched = true;
                } else if (touch_hit(touch.px, touch.py, &TAB_SETTINGS)) {
                    active_mode = MODE_SETTINGS;
                    settings_sub = SET_MAIN;
                    tab_touched = true;
                }
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

                case MODE_TIMER:
                    if (tmr.state == TMR_ADJUST) {
                        if (tDown && touch_hit(touch.px, touch.py, &BTN_TMR_START)) {
                            timer_start(&tmr);
                        } else {
                            /* Arrow hold-repeat for Timer adjustment */
                            hold_repeat_update(&hr[0], tHeld && touch_hit(touch.px, touch.py, &ARROW_H_UP));
                            hold_repeat_update(&hr[1], tHeld && touch_hit(touch.px, touch.py, &ARROW_H_DOWN));
                            hold_repeat_update(&hr[2], tHeld && touch_hit(touch.px, touch.py, &ARROW_M_UP));
                            hold_repeat_update(&hr[3], tHeld && touch_hit(touch.px, touch.py, &ARROW_M_DOWN));
                            hold_repeat_update(&hr[4], tHeld && touch_hit(touch.px, touch.py, &ARROW_S_UP));
                            hold_repeat_update(&hr[5], tHeld && touch_hit(touch.px, touch.py, &ARROW_S_DOWN));

                            if (hr[0].triggered) tmr.target_h = (tmr.target_h +  1) % 100;
                            if (hr[1].triggered) tmr.target_h = (tmr.target_h + 99) % 100;
                            if (hr[2].triggered) tmr.target_m = (tmr.target_m +  1) % 60;
                            if (hr[3].triggered) tmr.target_m = (tmr.target_m + 59) % 60;
                            if (hr[4].triggered) tmr.target_s = (tmr.target_s +  1) % 60;
                            if (hr[5].triggered) tmr.target_s = (tmr.target_s + 59) % 60;
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

                case MODE_SETTINGS:
                    if (settings_sub == SET_MAIN && tDown) {
                        if (touch_hit(touch.px, touch.py, &BTN_SET_EDIT)) {
                            clock_get_hms(&edit_h, &edit_m, &edit_s);
                            memset(hr, 0, sizeof(hr));
                            settings_sub = SET_EDIT;
                        } else if (touch_hit(touch.px, touch.py, &BTN_SET_RESET)) {
                            settings_sub = SET_CONFIRM_RESET;
                        }
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

        switch (active_mode) {
        case MODE_CLOCK:
        case MODE_SETTINGS: {
            int h, m, s;
            clock_get_hms(&h, &m, &s);
            ui_draw_top_clock(textBuf, h, m, s);
            break;
        }

        case MODE_STOPWATCH: {
            int sh, sm, ss, sc;
            bool show_hours;
            stopwatch_get_display(&sw, &sh, &sm, &ss, &sc, &show_hours);
            ui_draw_top_stopwatch(textBuf, sh, sm, ss, sc, show_hours);
            break;
        }

        case MODE_TIMER: {
            int th, tm, ts;
            timer_get_display(&tmr, &th, &tm, &ts);
            ui_draw_top_timer(textBuf, th, tm, ts);
            break;
        }
        }

        /* --- Bottom Screen Rendering --- */
        C2D_TargetClear(bot, CLR_BG);
        C2D_SceneBegin(bot);

        if (is_first_boot) {
            ui_draw_first_boot(textBuf, first_boot_frames > 30);
        }
        else {
            /* Mode content */
            switch (active_mode) {
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

            case MODE_SETTINGS:
                if (settings_sub == SET_MAIN)
                    ui_draw_settings_main(textBuf);
                else if (settings_sub == SET_EDIT)
                    ui_draw_settings_edit(textBuf, edit_h, edit_m, edit_s);
                else if (settings_sub == SET_CONFIRM_RESET) {
                    ui_draw_settings_main(textBuf);
                    ui_draw_modal_confirm(textBuf);
                }
                else if (settings_sub == SET_SAVE_OK) {
                    ui_draw_settings_main(textBuf);
                    ui_draw_modal_success(textBuf);
                }
                break;
            }

            /* Draw persistent tab bar if not in modal / sub-editor */
            if (settings_sub != SET_EDIT &&
                settings_sub != SET_CONFIRM_RESET &&
                settings_sub != SET_SAVE_OK &&
                tmr.state != TMR_EXPIRED) {
                ui_draw_tab_bar(textBuf, active_mode);
            }
        }

        C3D_FrameEnd(0);
    }

    /* --- Cleanup (reverse init order) --- */
    C2D_TextBufDelete(textBuf);
    C2D_Fini();
    C3D_Fini();
    cfguExit();
    gfxExit();
    return 0;
}
