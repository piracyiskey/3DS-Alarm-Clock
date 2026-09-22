#include <3ds.h>
#include <citro3d.h>
#include <citro2d.h>
#include <string.h>

#include "clock.h"
#include "save.h"
#include "ui.h"

/* ----- State machine ----- */

typedef enum {
    STATE_FIRST_BOOT,
    STATE_MAIN,
    STATE_EDIT,
    STATE_CONFIRM_RESET,
    STATE_SAVE_SUCCESS,
} BottomState;

/* ----- Hold-repeat helper for touch arrows ----- */

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

/* ----- Entry point ----- */

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

    /* --- Load save & init clock --- */
    BottomState state;
    ClockSaveData save;
    int first_boot_frames = 0;

    if (save_read(&save)) {
        clock_init(&save);
        state = STATE_MAIN;
    } else {
        clock_init(NULL);
        state = STATE_FIRST_BOOT;
        save_ensure_dir();
    }

    /* --- Edit state --- */
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

        /* --- Input handling per state --- */
        switch (state) {

        case STATE_FIRST_BOOT:
            first_boot_frames++;
            if (first_boot_frames > 30) {
                if ((kDown & KEY_A) ||
                    (tDown && touch_hit(touch.px, touch.py, &BTN_OK)))
                    state = STATE_MAIN;
            }
            break;

        case STATE_MAIN:
            if (tDown) {
                if (touch_hit(touch.px, touch.py, &BTN_EDIT)) {
                    clock_get_hms(&edit_h, &edit_m, &edit_s);
                    memset(hr, 0, sizeof(hr));
                    state = STATE_EDIT;
                } else if (touch_hit(touch.px, touch.py, &BTN_RESET)) {
                    state = STATE_CONFIRM_RESET;
                }
            }
            break;

        case STATE_EDIT:
            /* Back */
            if ((kDown & KEY_B) ||
                (tDown && touch_hit(touch.px, touch.py, &BTN_BACK))) {
                state = STATE_MAIN;
                break;
            }
            /* Save */
            if (tDown && touch_hit(touch.px, touch.py, &BTN_SAVE)) {
                ClockSaveData new_save;
                clock_apply_edit(edit_h, edit_m, edit_s, &new_save);
                save_write(&new_save);
                state = STATE_SAVE_SUCCESS;
                break;
            }

            /* Arrow hold-repeat */
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
            break;

        case STATE_CONFIRM_RESET:
            if (tDown) {
                if (touch_hit(touch.px, touch.py, &BTN_CONFIRM)) {
                    clock_reset();
                    save_delete();
                    state = STATE_MAIN;
                } else if (touch_hit(touch.px, touch.py, &BTN_CANCEL)) {
                    state = STATE_MAIN;
                }
            }
            if (kDown & KEY_B)
                state = STATE_MAIN;
            break;

        case STATE_SAVE_SUCCESS:
            if ((kDown & KEY_A) ||
                (tDown && touch_hit(touch.px, touch.py, &BTN_OK)))
                state = STATE_MAIN;
            break;
        }

        /* --- Render --- */
        int h, m, s;
        clock_get_hms(&h, &m, &s);

        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
        C2D_TextBufClear(textBuf);

        /* Top screen — clock */
        C2D_TargetClear(top, CLR_BG);
        C2D_SceneBegin(top);
        ui_draw_clock(textBuf, h, m, s);

        /* Bottom screen — state-dependent */
        C2D_TargetClear(bot, CLR_BG);
        C2D_SceneBegin(bot);

        switch (state) {
        case STATE_FIRST_BOOT:
            ui_draw_first_boot(textBuf, first_boot_frames > 30);
            break;
        case STATE_MAIN:
            ui_draw_main(textBuf);
            break;
        case STATE_EDIT:
            ui_draw_edit(textBuf, edit_h, edit_m, edit_s);
            break;
        case STATE_CONFIRM_RESET:
            ui_draw_main(textBuf);
            ui_draw_modal_confirm(textBuf);
            break;
        case STATE_SAVE_SUCCESS:
            ui_draw_main(textBuf);
            ui_draw_modal_success(textBuf);
            break;
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
