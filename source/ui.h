#pragma once
#include <citro2d.h>
#include <stdbool.h>

/* Hit rectangle for touch input */
typedef struct { float x, y, w, h; } HitRect;

/* Color palette — dark grey background, white text */
#define CLR_BG          C2D_Color32(0x30, 0x30, 0x30, 0xFF)
#define CLR_TEXT        C2D_Color32(0xFF, 0xFF, 0xFF, 0xFF)
#define CLR_TEXT_DIM    C2D_Color32(0x99, 0x99, 0x99, 0xFF)
#define CLR_BTN         C2D_Color32(0x50, 0x50, 0x50, 0xFF)
#define CLR_MODAL_BG    C2D_Color32(0x40, 0x40, 0x40, 0xFF)
#define CLR_OVERLAY     C2D_Color32(0x00, 0x00, 0x00, 0xA0)

#define CLR_TAB_ACTIVE  C2D_Color32(0x50, 0x50, 0x50, 0xFF)
#define CLR_TAB_INACT   C2D_Color32(0x22, 0x22, 0x22, 0xFF)
#define CLR_TAB_SEP     C2D_Color32(0x40, 0x40, 0x40, 0xFF)

/* Modes */
typedef enum {
    MODE_CLOCK,
    MODE_STOPWATCH,
    MODE_TIMER,
    MODE_SETTINGS
} AppMode;

/* --- Tab bar layout constants (docked at y=200, h=40) --- */
extern const HitRect TAB_CLOCK;
extern const HitRect TAB_STOPWATCH;
extern const HitRect TAB_TIMER;
extern const HitRect TAB_SETTINGS;

/* --- Shared Arrow layout (used for Clock Edit and Timer Adjust) --- */
extern const HitRect ARROW_H_UP,   ARROW_H_DOWN;
extern const HitRect ARROW_M_UP,   ARROW_M_DOWN;
extern const HitRect ARROW_S_UP,   ARROW_S_DOWN;

/* --- Stopwatch buttons --- */
extern const HitRect BTN_SW_START;
extern const HitRect BTN_SW_PAUSE;
extern const HitRect BTN_SW_RESUME;
extern const HitRect BTN_SW_RESET;

/* --- Timer buttons --- */
extern const HitRect BTN_TMR_START;
extern const HitRect BTN_TMR_PAUSE;
extern const HitRect BTN_TMR_RESUME;
extern const HitRect BTN_TMR_RESET;

/* --- Settings buttons --- */
extern const HitRect BTN_SET_RESET;
extern const HitRect BTN_SET_EDIT;
extern const HitRect BTN_SET_BACK;
extern const HitRect BTN_SET_SAVE;

/* --- Modal buttons --- */
extern const HitRect BTN_OK;
extern const HitRect BTN_CANCEL;
extern const HitRect BTN_CONFIRM;

/* --- Drawing functions --- */

/* Top screen */
void ui_draw_top_clock(C2D_TextBuf buf, int h, int m, int s);
void ui_draw_top_stopwatch(C2D_TextBuf buf, int hh, int mm, int ss, int cs, bool show_hours);
void ui_draw_top_timer(C2D_TextBuf buf, int hh, int mm, int ss);

/* Bottom screen — navigation */
void ui_draw_tab_bar(C2D_TextBuf buf, AppMode active);

/* Bottom screen — Clock mode */
void ui_draw_clock_bottom(C2D_TextBuf buf);

/* Bottom screen — Stopwatch mode */
void ui_draw_stopwatch_idle(C2D_TextBuf buf);
void ui_draw_stopwatch_running(C2D_TextBuf buf);
void ui_draw_stopwatch_paused(C2D_TextBuf buf);

/* Bottom screen — Timer mode */
void ui_draw_timer_adjust(C2D_TextBuf buf, int h, int m, int s);
void ui_draw_timer_running(C2D_TextBuf buf);
void ui_draw_timer_paused(C2D_TextBuf buf);
void ui_draw_timer_expired_modal(C2D_TextBuf buf);

/* Bottom screen — Settings mode */
void ui_draw_settings_main(C2D_TextBuf buf);
void ui_draw_settings_edit(C2D_TextBuf buf, int h, int m, int s);
void ui_draw_modal_confirm(C2D_TextBuf buf);
void ui_draw_modal_success(C2D_TextBuf buf);

/* First boot */
void ui_draw_first_boot(C2D_TextBuf buf, bool show_ok);
