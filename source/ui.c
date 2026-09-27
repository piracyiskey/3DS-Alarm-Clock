#include "ui.h"
#include "world_clock.h"
#include "clock.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

/* ------------------------------------------------------------------ */
/*  Stepper layout metrics (shared by time & date editors)             */
/* ------------------------------------------------------------------ */
/*  Stepper zone sits between the nav bar (y=36) and bottom actions.   */
/*  All arrow positions are computed at draw time from measured text   */
/*  height, so the constants below are for HitRect estimation only.   */

#define STEPPER_CY 122.0f      /* Vertical midpoint of digit row        */
#define ARROW_TRI_W 20.0f      /* Arrow triangle width  (px)            */
#define ARROW_TRI_H 14.0f      /* Arrow triangle height (px)            */
#define ARROW_PAD 5.0f         /* Gap between arrow tip and digit edge  */
#define BOTTOM_ACTION_Y 198.0f /* Top-Y of the bottom action row       */

/* ------------------------------------------------------------------ */
/*  Alarm Stepper & Selector layout metrics (Add & Edit Alarm views)   */
/* ------------------------------------------------------------------ */
/*  All arrow positions are computed at draw time from measured text   */
/*  height with symmetric padding, for easy micro adjustment.         */

#define ALARM_STEPPER_CY 78.0f    /* Vertical midpoint of alarm digit row  */
#define ALARM_STEPPER_SCALE 1.50f /* Scale for hh:mm digits                */
#define ALARM_ARROW_TRI_W 22.0f   /* Arrow triangle width (px)             */
#define ALARM_ARROW_TRI_H 11.0f   /* Arrow triangle height (px)            */
#define ALARM_ARROW_PAD 4.0f      /* Gap between arrow tip and digit edge  */
#define ALARM_COL_H_CX 105.0f     /* Center X of hours column              */
#define ALARM_COL_COLON_CX 160.0f /* Center X of colon                     */
#define ALARM_COL_M_CX 215.0f     /* Center X of minutes column            */

/* Centered selector units: [Label] [<] [ Value Container ] [>]       */
#define ALARM_SEL_TOTAL_W 248.0f     /* Total width of centered unit          */
#define ALARM_SEL_START_X 36.0f      /* (320 - 248) / 2 = 36px left/right pad */
#define ALARM_SEL_LABEL_R 90.0f      /* Right edge of label text (x=36..90)   */
#define ALARM_SEL_BTN_LEFT_X 96.0f   /* Left arrow button X                   */
#define ALARM_SEL_BOX_X 128.0f       /* Value container box X                 */
#define ALARM_SEL_BOX_W 124.0f       /* Value container box width             */
#define ALARM_SEL_BTN_RIGHT_X 256.0f /* Right arrow button X */
#define ALARM_SEL_BTN_W 28.0f        /* Arrow button width                    */
#define ALARM_SEL_ROW_H 26.0f        /* Row height                            */
#define ALARM_SEL_TONE_Y 128.0f      /* Tone row Y                            */
#define ALARM_SEL_REPEAT_Y 162.0f    /* Repeat row Y                          */

/* ------------------------------------------------------------------ */
/*  HitRect layout constants                                           */
/* ------------------------------------------------------------------ */

/* Tab bar tabs (320×240 bottom screen, docked at y=200, h=40) */
const HitRect TAB_ALARM = {0.0f, 200.0f, 80.0f, 40.0f};
const HitRect TAB_CLOCK = {80.0f, 200.0f, 80.0f, 40.0f};
const HitRect TAB_STOPWATCH = {160.0f, 200.0f, 80.0f, 40.0f};
const HitRect TAB_TIMER = {240.0f, 200.0f, 80.0f, 40.0f};

/* Global header bar hit target (top-right corner) */
const HitRect BTN_SETTINGS_ICON = {280.0f, 0.0f, 40.0f, 32.0f};

/* Time arrows — col centers x=80,160,240; Y estimated for scale 1.8 dh≈30 */
const HitRect ARROW_H_UP = {55.0f, 52.0f, 50.0f, 30.0f};
const HitRect ARROW_H_DOWN = {55.0f, 126.0f, 50.0f, 30.0f};
const HitRect ARROW_M_UP = {135.0f, 52.0f, 50.0f, 30.0f};
const HitRect ARROW_M_DOWN = {135.0f, 126.0f, 50.0f, 30.0f};
const HitRect ARROW_S_UP = {215.0f, 52.0f, 50.0f, 30.0f};
const HitRect ARROW_S_DOWN = {215.0f, 126.0f, 50.0f, 30.0f};

/* Date arrows — col centers x=70,160,250; Y estimated for scale 1.6 dh≈26 */
const HitRect ARROW_COL1_UP = {45.0f, 55.0f, 50.0f, 30.0f};
const HitRect ARROW_COL1_DOWN = {45.0f, 123.0f, 50.0f, 30.0f};
const HitRect ARROW_COL2_UP = {135.0f, 55.0f, 50.0f, 30.0f};
const HitRect ARROW_COL2_DOWN = {135.0f, 123.0f, 50.0f, 30.0f};
const HitRect ARROW_COL3_UP = {225.0f, 55.0f, 50.0f, 30.0f};
const HitRect ARROW_COL3_DOWN = {225.0f, 123.0f, 50.0f, 30.0f};

const HitRect BTN_FMT_LEFT = {20.0f, BOTTOM_ACTION_Y, 40.0f, 30.0f};
const HitRect BTN_FMT_RIGHT = {260.0f, BOTTOM_ACTION_Y, 40.0f, 30.0f};

/* Stopwatch buttons */
const HitRect BTN_SW_START = {90.0f, 75.0f, 140.0f, 50.0f};
const HitRect BTN_SW_LAP = {16.0f, 75.0f, 88.0f, 50.0f};
const HitRect BTN_SW_PAUSE = {116.0f, 75.0f, 88.0f, 50.0f};
const HitRect BTN_SW_RESET = {216.0f, 75.0f, 88.0f, 50.0f};
const HitRect BTN_SW_RESUME = {30.0f, 75.0f, 120.0f, 50.0f};
const HitRect BTN_SW_RESET_PAUSED = {170.0f, 75.0f, 120.0f, 50.0f};

/* Timer buttons */
const HitRect BTN_TMR_START = {90.0f, 36.0f, 140.0f, 26.0f};
const HitRect BTN_TMR_PAUSE = {30.0f, 75.0f, 120.0f, 50.0f};
const HitRect BTN_TMR_RESUME = {30.0f, 75.0f, 120.0f, 50.0f};
const HitRect BTN_TMR_RESET = {170.0f, 75.0f, 120.0f, 50.0f};

/* Alarm buttons */
const HitRect BTN_ALARM_ADD = {240.0f, 2.0f, 36.0f, 28.0f};
const HitRect BTN_ALARM_EDIT_SAVE = {245.0f, 4.0f, 65.0f, 28.0f};
const HitRect BTN_ALARM_EDIT_CANCEL = {10.0f, 4.0f, 65.0f, 28.0f};
const HitRect BTN_ALARM_EDIT_DELETE = {40.0f, 200.0f, 240.0f, 28.0f};
const HitRect BTN_ALARM_REPEAT_LEFT = {ALARM_SEL_BTN_LEFT_X, ALARM_SEL_REPEAT_Y,
                                       ALARM_SEL_BTN_W, ALARM_SEL_ROW_H};
const HitRect BTN_ALARM_REPEAT_RIGHT = {ALARM_SEL_BTN_RIGHT_X,
                                        ALARM_SEL_REPEAT_Y, ALARM_SEL_BTN_W,
                                        ALARM_SEL_ROW_H};
const HitRect BTN_ALARM_TONE_LEFT = {ALARM_SEL_BTN_LEFT_X, ALARM_SEL_TONE_Y,
                                     ALARM_SEL_BTN_W, ALARM_SEL_ROW_H};
const HitRect BTN_ALARM_TONE_RIGHT = {ALARM_SEL_BTN_RIGHT_X, ALARM_SEL_TONE_Y,
                                      ALARM_SEL_BTN_W, ALARM_SEL_ROW_H};
const HitRect BTN_ALARM_LABEL_INPUT = {96.0f, 196.0f, 188.0f, 26.0f};
const HitRect BTN_ALARM_DISMISS = {60.0f, 140.0f, 200.0f, 40.0f};
const HitRect BTN_ALARM_MISSED_OK = {60.0f, 140.0f, 200.0f, 40.0f};
const HitRect BTN_TIMER_DISMISS = {60.0f, 140.0f, 200.0f, 40.0f};

/* Dedicated 2-Column Alarm Stepper Arrows — col centers x=105, 215; Y estimated
 * for scale 1.5 dh≈28 */
const HitRect ARROW_ALARM_H_UP = {80.0f, 35.0f, 50.0f, 26.0f};
const HitRect ARROW_ALARM_H_DOWN = {80.0f, 89.0f, 50.0f, 28.0f};
const HitRect ARROW_ALARM_M_UP = {190.0f, 35.0f, 50.0f, 26.0f};
const HitRect ARROW_ALARM_M_DOWN = {190.0f, 89.0f, 50.0f, 28.0f};

/* Settings overlay buttons */
const HitRect BTN_SET_BACK = {10.0f, 4.0f, 70.0f, 28.0f};
const HitRect BTN_SET_SAVE = {240.0f, 4.0f, 70.0f, 28.0f};
const HitRect BTN_SET_EDIT = {40.0f, 56.0f, 240.0f, 40.0f};
const HitRect BTN_SET_RESET = {40.0f, 112.0f, 240.0f, 40.0f};
const HitRect BTN_SET_DISPLAY = {40.0f, 168.0f, 240.0f, 40.0f};
const HitRect BTN_EDIT_DATE = {80.0f, BOTTOM_ACTION_Y, 160.0f, 30.0f};

/* Display & Power Sub-Menu Buttons */
const HitRect BTN_DISP_BOTH_OFF = {40.0f, 72.0f, 240.0f, 48.0f};
const HitRect BTN_DISP_BOT_OFF  = {40.0f, 136.0f, 240.0f, 48.0f};

/* Modal buttons */
const HitRect BTN_OK = {110.0f, 145.0f, 100.0f, 40.0f};
const HitRect BTN_CANCEL = {40.0f, 145.0f, 100.0f, 40.0f};
const HitRect BTN_CONFIRM = {180.0f, 145.0f, 100.0f, 40.0f};

/* World Clock buttons */
const HitRect BTN_CLOCK_ADD         = { 240.0f, 2.0f, 36.0f, 28.0f };
const HitRect BTN_CITY_PICKER_BACK  = { 8.0f, 4.0f, 56.0f, 24.0f };
const HitRect BTN_MODAL_CITY_CANCEL = { 35.0f, 140.0f, 115.0f, 36.0f };
const HitRect BTN_MODAL_CITY_DEL    = { 170.0f, 140.0f, 115.0f, 36.0f };
const HitRect BTN_MODAL_HOME_CANCEL = { 35.0f, 140.0f, 115.0f, 36.0f };
const HitRect BTN_MODAL_HOME_SET    = { 170.0f, 140.0f, 115.0f, 36.0f };

/* ------------------------------------------------------------------ */
/*  Internal helpers                                                   */
/* ------------------------------------------------------------------ */

static void draw_button_scaled(C2D_TextBuf buf, const HitRect *r,
                               const char *label, float scale, u32 bg_color) {
  C2D_DrawRectSolid(r->x, r->y, 0.0f, r->w, r->h, bg_color);

  C2D_Text text;
  C2D_TextParse(&text, buf, label);
  C2D_TextOptimize(&text);

  float tw, th;
  C2D_TextGetDimensions(&text, scale, scale, &tw, &th);

  float tx = r->x + (r->w - tw) / 2.0f;
  float ty = r->y + (r->h - th) / 2.0f;
  C2D_DrawText(&text, C2D_WithColor, tx, ty, 0.0f, scale, scale, CLR_TEXT);
}

static void draw_button(C2D_TextBuf buf, const HitRect *r, const char *label) {
  draw_button_scaled(buf, r, label, 0.65f, CLR_BTN);
}

/* Helper to set hardware scissor in landscape user coordinates (320x240 or 400x240)
 * on the tilted portrait framebuffer (240x320 or 240x400) */
static void ui_set_scissor(GPU_SCISSORMODE mode, u32 x, u32 y, u32 w, u32 h) {
  C2D_Flush();
  if (mode == GPU_SCISSOR_DISABLE) {
    C3D_SetScissor(GPU_SCISSOR_DISABLE, 0, 0, 0, 0);
  } else {
    /* Rotate from landscape user coordinates (320x240) to physical framebuffer
     * (240x320) */
    u32 left = 240 - (y + h);
    u32 top = x;
    u32 right = 240 - y;
    u32 bottom = x + w;
    C3D_SetScissor(mode, left, top, right, bottom);
  }
}

static void draw_arrow_up(float cx, float cy, float w, float h, u32 clr) {
  C2D_DrawTriangle(cx, cy - h / 2.0f, clr,            /* tip   */
                   cx - w / 2.0f, cy + h / 2.0f, clr, /* bot-L */
                   cx + w / 2.0f, cy + h / 2.0f, clr, /* bot-R */
                   0.0f);
}

static void draw_arrow_down(float cx, float cy, float w, float h, u32 clr) {
  C2D_DrawTriangle(cx - w / 2.0f, cy - h / 2.0f, clr, /* top-L */
                   cx + w / 2.0f, cy - h / 2.0f, clr, /* top-R */
                   cx, cy + h / 2.0f, clr,            /* tip   */
                   0.0f);
}

static void draw_modal_bg(void) {
  C2D_DrawRectSolid(0, 0, 0.0f, 320, 240, CLR_OVERLAY);
  C2D_DrawRectSolid(20, 45, 0.0f, 280, 150, CLR_MODAL_BG);
}

static void draw_text_centered_x(C2D_TextBuf buf, const char *str, float y,
                                 float scale, float screen_w) {
  C2D_Text text;
  C2D_TextParse(&text, buf, str);
  C2D_TextOptimize(&text);

  float tw, th;
  C2D_TextGetDimensions(&text, scale, scale, &tw, &th);

  float x = (screen_w - tw) / 2.0f;
  C2D_DrawText(&text, C2D_WithColor, x, y, 0.0f, scale, scale, CLR_TEXT);
}

static void draw_time_editor(C2D_TextBuf buf, int h, int m, int s) {
  float scale = 1.8f;
  float cx_h = 80.0f;
  float cx_m = 160.0f;
  float cx_s = 240.0f;

  char hh[8], mm[4], ss[4];
  snprintf(hh, sizeof(hh), "%02d", h);
  snprintf(mm, sizeof(mm), "%02d", m);
  snprintf(ss, sizeof(ss), "%02d", s);

  C2D_Text t_h, t_m, t_s, t_col;
  C2D_TextParse(&t_h, buf, hh);
  C2D_TextOptimize(&t_h);
  C2D_TextParse(&t_m, buf, mm);
  C2D_TextOptimize(&t_m);
  C2D_TextParse(&t_s, buf, ss);
  C2D_TextOptimize(&t_s);
  C2D_TextParse(&t_col, buf, ":");
  C2D_TextOptimize(&t_col);

  /* Measure actual digit bounding box to centre everything */
  float dw, dh, cw, ch;
  C2D_TextGetDimensions(&t_h, scale, scale, &dw, &dh);
  C2D_TextGetDimensions(&t_col, scale, scale, &cw, &ch);

  /* Digit row: vertically centred on STEPPER_CY */
  float digit_y = STEPPER_CY - dh / 2.0f;

  C2D_DrawText(&t_h, C2D_WithColor, cx_h - dw / 2.0f, digit_y, 0.0f, scale,
               scale, CLR_TEXT);
  C2D_DrawText(&t_col, C2D_WithColor, 120.0f - cw / 2.0f, digit_y, 0.0f, scale,
               scale, CLR_TEXT);
  C2D_DrawText(&t_m, C2D_WithColor, cx_m - dw / 2.0f, digit_y, 0.0f, scale,
               scale, CLR_TEXT);
  C2D_DrawText(&t_col, C2D_WithColor, 200.0f - cw / 2.0f, digit_y, 0.0f, scale,
               scale, CLR_TEXT);
  C2D_DrawText(&t_s, C2D_WithColor, cx_s - dw / 2.0f, digit_y, 0.0f, scale,
               scale, CLR_TEXT);

  /* Arrows: symmetric pad from digit bounding box edges */
  float up_cy = digit_y - ARROW_PAD - ARROW_TRI_H / 2.0f;
  float down_cy = digit_y + dh + ARROW_PAD + ARROW_TRI_H / 2.0f;

  draw_arrow_up(cx_h, up_cy, ARROW_TRI_W, ARROW_TRI_H, CLR_TEXT);
  draw_arrow_up(cx_m, up_cy, ARROW_TRI_W, ARROW_TRI_H, CLR_TEXT);
  draw_arrow_up(cx_s, up_cy, ARROW_TRI_W, ARROW_TRI_H, CLR_TEXT);

  draw_arrow_down(cx_h, down_cy, ARROW_TRI_W, ARROW_TRI_H, CLR_TEXT);
  draw_arrow_down(cx_m, down_cy, ARROW_TRI_W, ARROW_TRI_H, CLR_TEXT);
  draw_arrow_down(cx_s, down_cy, ARROW_TRI_W, ARROW_TRI_H, CLR_TEXT);
}

/* ------------------------------------------------------------------ */
/*  Top Screen Display Functions (400×240)                            */
/* ------------------------------------------------------------------ */

void ui_draw_top_status_bar(C2D_TextBuf buf, u8 wifi_bars, u8 battery_percent,
                            bool is_charging) {
  /* Subtle status bar background strip (Y = 0..21, 1px bottom separator at Y =
   * 21) */
  C2D_DrawRectSolid(0.0f, 0.0f, 0.0f, 400.0f, 22.0f,
                    C2D_Color32(0x22, 0x22, 0x22, 0xFF));
  C2D_DrawRectSolid(0.0f, 21.0f, 0.0f, 400.0f, 1.0f,
                    C2D_Color32(0x38, 0x38, 0x38, 0xFF));

  /* ---------------- Left Section: Wi-Fi Status ---------------- */
  C2D_Text txt_wifi;
  C2D_TextParse(&txt_wifi, buf, "Wi-Fi");
  C2D_TextOptimize(&txt_wifi);
  C2D_DrawText(&txt_wifi, C2D_WithColor, 8.0f, 4.0f, 0.0f, 0.5f, 0.5f,
               CLR_TEXT_DIM);

  float tw, th;
  C2D_TextGetDimensions(&txt_wifi, 0.5f, 0.5f, &tw, &th);

  /* 3 procedural signal bars at X = 8 + tw + 6 */
  float bar_base_x = 8.0f + tw + 6.0f;
  float bar_bottom_y = 16.0f;
  u32 dim_clr = C2D_Color32(0x55, 0x55, 0x55, 0xFF);

  /* Bar 1: H = 4 */
  u32 b1_clr = (wifi_bars >= 1) ? CLR_TEXT : dim_clr;
  C2D_DrawRectSolid(bar_base_x + 0.0f, bar_bottom_y - 4.0f, 0.0f, 3.0f, 4.0f,
                    b1_clr);

  /* Bar 2: H = 7 */
  u32 b2_clr = (wifi_bars >= 2) ? CLR_TEXT : dim_clr;
  C2D_DrawRectSolid(bar_base_x + 5.0f, bar_bottom_y - 7.0f, 0.0f, 3.0f, 7.0f,
                    b2_clr);

  /* Bar 3: H = 10 */
  u32 b3_clr = (wifi_bars >= 3) ? CLR_TEXT : dim_clr;
  C2D_DrawRectSolid(bar_base_x + 10.0f, bar_bottom_y - 10.0f, 0.0f, 3.0f, 10.0f,
                    b3_clr);

  /* If Wi-Fi is 0 (disconnected / switch off), render a small "[Off]" label */
  if (wifi_bars == 0) {
    C2D_Text txt_off;
    C2D_TextParse(&txt_off, buf, "Off");
    C2D_TextOptimize(&txt_off);
    C2D_DrawText(&txt_off, C2D_WithColor, bar_base_x + 16.0f, 4.0f, 0.0f, 0.5f,
                 0.5f, C2D_Color32(0xC0, 0x50, 0x50, 0xFF));
  }

  /* ---------------- Right Section: Battery Status ---------------- */
  /* Battery icon dimensions & placement */
  float bat_right = 392.0f;
  float bat_w = 22.0f;
  float bat_h = 12.0f;
  float bat_x = bat_right - bat_w - 2.0f; /* 368.0f */
  float bat_y = 5.0f;

  /* Terminal nipple on the right */
  C2D_DrawRectSolid(bat_x + bat_w, bat_y + 3.0f, 0.0f, 2.0f, 6.0f, CLR_TAB_SEP);

  /* Outer battery border */
  C2D_DrawRectSolid(bat_x, bat_y, 0.0f, bat_w, bat_h, CLR_TAB_SEP);

  /* Inner dark hollow */
  C2D_DrawRectSolid(bat_x + 1.0f, bat_y + 1.0f, 0.0f, bat_w - 2.0f,
                    bat_h - 2.0f, C2D_Color32(0x18, 0x18, 0x18, 0xFF));

  /* Battery fill: max width = 18.0f */
  float max_fill_w = bat_w - 4.0f; /* 18.0f */
  float fill_w = (battery_percent * max_fill_w) / 100.0f;
  if (fill_w < 1.0f && battery_percent > 0)
    fill_w = 1.0f;
  if (fill_w > max_fill_w)
    fill_w = max_fill_w;

  u32 fill_clr;
  if (is_charging) {
    fill_clr = C2D_Color32(0x40, 0xD0, 0x40, 0xFF); /* Green when charging */
  } else if (battery_percent <= 15) {
    fill_clr = C2D_Color32(0xD0, 0x30, 0x30, 0xFF); /* Red when critical */
  } else if (battery_percent <= 30) {
    fill_clr = C2D_Color32(0xD0, 0xA0, 0x20, 0xFF); /* Amber when low */
  } else {
    fill_clr = CLR_TEXT; /* Neutral white */
  }

  if (fill_w > 0.0f) {
    C2D_DrawRectSolid(bat_x + 2.0f, bat_y + 2.0f, 0.0f, fill_w, bat_h - 4.0f,
                      fill_clr);
  }

  /* Battery Percentage String */
  char pct_str[16];
  snprintf(pct_str, sizeof(pct_str), "%d%%", battery_percent);

  C2D_Text txt_pct;
  C2D_TextParse(&txt_pct, buf, pct_str);
  C2D_TextOptimize(&txt_pct);

  float pw, ph;
  C2D_TextGetDimensions(&txt_pct, 0.5f, 0.5f, &pw, &ph);

  float pct_x = bat_x - 6.0f - pw;
  u32 pct_clr = is_charging ? C2D_Color32(0x40, 0xD0, 0x40, 0xFF) : CLR_TEXT;
  C2D_DrawText(&txt_pct, C2D_WithColor, pct_x, 4.0f, 0.0f, 0.5f, 0.5f, pct_clr);

  /* Charging Indicator: Procedural Lightning Bolt icon when charging */
  if (is_charging) {
    float bolt_x = pct_x - 12.0f;
    float bolt_y = 5.0f;
    u32 bolt_clr = C2D_Color32(0x40, 0xE0, 0x40, 0xFF);

    /* Upper triangle */
    C2D_DrawTriangle(bolt_x + 5.0f, bolt_y + 0.0f, bolt_clr, bolt_x + 1.0f,
                     bolt_y + 6.0f, bolt_clr, bolt_x + 5.0f, bolt_y + 6.0f,
                     bolt_clr, 0.0f);
    /* Lower triangle */
    C2D_DrawTriangle(bolt_x + 3.0f, bolt_y + 4.0f, bolt_clr, bolt_x + 7.0f,
                     bolt_y + 4.0f, bolt_clr, bolt_x + 3.0f, bolt_y + 11.0f,
                     bolt_clr, 0.0f);
  }
}

void ui_draw_top_clock_with_date(C2D_TextBuf buf, int h, int m, int s,
                                 const char *date_str) {
  /* Telemetry header */
  draw_text_centered_x(buf, "CLOCK", 38.0f, 0.55f, 400.0f);

  /* Main digits at scale 2.0 */
  char str[16];
  snprintf(str, sizeof(str), "%02d:%02d:%02d", h, m, s);

  C2D_Text text;
  C2D_TextParse(&text, buf, str);
  C2D_TextOptimize(&text);

  float scale = 2.0f;
  float tw, th;
  C2D_TextGetDimensions(&text, scale, scale, &tw, &th);

  float x = (400.0f - tw) / 2.0f;
  float y = 78.0f;
  C2D_DrawText(&text, C2D_WithColor, x, y, 0.0f, scale, scale, CLR_TEXT);

  /* Date display string below clock */
  if (date_str && date_str[0] != '\0') {
    draw_text_centered_x(buf, date_str, 152.0f, 0.65f, 400.0f);
  }
}

void ui_draw_top_clock_with_home(C2D_TextBuf buf, int h, int m, int s,
                                 const char *date_str, const char *home_city,
                                 const char *home_country) {
  /* Telemetry header */
  draw_text_centered_x(buf, "WORLD CLOCK", 34.0f, 0.55f, 400.0f);

  /* Main digits at scale 2.0 */
  char str[16];
  snprintf(str, sizeof(str), "%02d:%02d:%02d", h, m, s);

  C2D_Text text;
  C2D_TextParse(&text, buf, str);
  C2D_TextOptimize(&text);

  float scale = 2.0f;
  float tw, th;
  C2D_TextGetDimensions(&text, scale, scale, &tw, &th);

  float x = (400.0f - tw) / 2.0f;
  float y = 70.0f;
  C2D_DrawText(&text, C2D_WithColor, x, y, 0.0f, scale, scale, CLR_TEXT);

  /* Date display string below clock */
  if (date_str && date_str[0] != '\0') {
    draw_text_centered_x(buf, date_str, 142.0f, 0.65f, 400.0f);
  }

  /* Home reference city */
  if (home_city && home_country) {
    char home_str[64];
    snprintf(home_str, sizeof(home_str), "Home: %s, %s", home_city, home_country);
    draw_text_centered_x(buf, home_str, 175.0f, 0.48f, 400.0f);
  }
}

void ui_draw_top_clock_with_alarm_status(C2D_TextBuf buf, int h, int m, int s,
                                         const char *date_str,
                                         const char *alarm_status) {
  /* Telemetry header */
  draw_text_centered_x(buf, "ALARM", 34.0f, 0.55f, 400.0f);

  /* Main digits at scale 2.0 */
  char str[16];
  snprintf(str, sizeof(str), "%02d:%02d:%02d", h, m, s);

  C2D_Text text;
  C2D_TextParse(&text, buf, str);
  C2D_TextOptimize(&text);

  float scale = 2.0f;
  float tw, th;
  C2D_TextGetDimensions(&text, scale, scale, &tw, &th);

  float x = (400.0f - tw) / 2.0f;
  float y = 70.0f;
  C2D_DrawText(&text, C2D_WithColor, x, y, 0.0f, scale, scale, CLR_TEXT);

  /* Date display string below clock */
  if (date_str && date_str[0] != '\0') {
    draw_text_centered_x(buf, date_str, 142.0f, 0.65f, 400.0f);
  }

  /* Next alarm status line */
  if (alarm_status && alarm_status[0] != '\0') {
    draw_text_centered_x(buf, alarm_status, 175.0f, 0.48f, 400.0f);
  }
}

void ui_draw_top_stopwatch(C2D_TextBuf buf, const Stopwatch* sw, int hh, int mm, int ss, int cs,
                           bool show_hours) {
  if (!sw || sw->lap_count == 0) {
    draw_text_centered_x(buf, "STOPWATCH", 40.0f, 0.55f, 400.0f);

    char str[24];
    if (show_hours) {
      snprintf(str, sizeof(str), "%02d:%02d:%02d:%02d", hh, mm, ss, cs);
    } else {
      snprintf(str, sizeof(str), "%02d:%02d:%02d", mm, ss, cs);
    }

    C2D_Text text;
    C2D_TextParse(&text, buf, str);
    C2D_TextOptimize(&text);

    float scale = 2.0f;
    float tw, th;
    C2D_TextGetDimensions(&text, scale, scale, &tw, &th);

    float x = (400.0f - tw) / 2.0f;
    float y = (240.0f - th) / 2.0f + 14.0f;
    C2D_DrawText(&text, C2D_WithColor, x, y, 0.0f, scale, scale, CLR_TEXT);
    return;
  }

  /* --- Lap View (sw->lap_count > 0) --- */
  /* Top compact running clock centered vertically in Y = 22..68 */
  char clock_str[24];
  if (show_hours) {
    snprintf(clock_str, sizeof(clock_str), "%02d:%02d:%02d:%02d", hh, mm, ss, cs);
  } else {
    snprintf(clock_str, sizeof(clock_str), "%02d:%02d:%02d", mm, ss, cs);
  }

  C2D_Text top_clock;
  C2D_TextParse(&top_clock, buf, clock_str);
  C2D_TextOptimize(&top_clock);
  float clk_scale = 1.20f;
  float ctw, cth;
  C2D_TextGetDimensions(&top_clock, clk_scale, clk_scale, &ctw, &cth);
  float clk_x = (400.0f - ctw) / 2.0f;
  C2D_DrawText(&top_clock, C2D_WithColor, clk_x, 32.0f, 0.0f, clk_scale, clk_scale, CLR_TEXT);

  /* Column Headers Bar at Y = 68.0f .. 88.0f */
  C2D_DrawRectSolid(0.0f, 68.0f, 0.0f, 400.0f, 20.0f, C2D_Color32(0x22, 0x22, 0x22, 0xFF));
  C2D_DrawRectSolid(0.0f, 87.0f, 0.0f, 400.0f, 1.0f, C2D_Color32(0x38, 0x38, 0x38, 0xFF));

  C2D_Text h_lap, h_split, h_total;
  C2D_TextParse(&h_lap, buf, "LAP");
  C2D_TextOptimize(&h_lap);
  C2D_DrawText(&h_lap, C2D_WithColor, 24.0f, 71.0f, 0.0f, 0.45f, 0.45f, CLR_TEXT_DIM);

  C2D_TextParse(&h_split, buf, "TIME");
  C2D_TextOptimize(&h_split);
  C2D_DrawText(&h_split, C2D_WithColor, 145.0f, 71.0f, 0.0f, 0.45f, 0.45f, CLR_TEXT_DIM);

  C2D_TextParse(&h_total, buf, "TOTAL TIME");
  C2D_TextOptimize(&h_total);
  C2D_DrawText(&h_total, C2D_WithColor, 275.0f, 71.0f, 0.0f, 0.45f, 0.45f, CLR_TEXT_DIM);

  /* Viewport: Y = 88 to 238 (150px). Row height = 24px -> exactly 6 rows */
  ui_set_scissor(GPU_SCISSOR_NORMAL, 0, 88, 400, 150);

  int visible_rows = 6;
  for (int r = 0; r < visible_rows; r++) {
    int idx = sw->scroll_row + r;
    if (idx >= sw->lap_count) break;

    float row_y = 88.0f + (float)r * 24.0f;

    /* Subtle row divider */
    C2D_DrawRectSolid(16.0f, row_y + 23.0f, 0.0f, 368.0f, 1.0f, C2D_Color32(0x28, 0x28, 0x28, 0xFF));

    /* Lap number */
    char lap_num_str[16];
    snprintf(lap_num_str, sizeof(lap_num_str), "Lap %02d", idx + 1);
    C2D_Text t_num;
    C2D_TextParse(&t_num, buf, lap_num_str);
    C2D_TextOptimize(&t_num);
    C2D_DrawText(&t_num, C2D_WithColor, 24.0f, row_y + 3.0f, 0.0f, 0.48f, 0.48f, CLR_TEXT_DIM);

    /* Split (lap duration) */
    int l_hh, l_mm, l_ss, l_cs;
    bool l_hours;
    stopwatch_format_time(sw->laps[idx].lap_time_ms, &l_hh, &l_mm, &l_ss, &l_cs, &l_hours);

    char split_str[24];
    if (l_hours) {
      snprintf(split_str, sizeof(split_str), "%02d:%02d:%02d.%02d", l_hh, l_mm, l_ss, l_cs);
    } else {
      snprintf(split_str, sizeof(split_str), "%02d:%02d.%02d", l_mm, l_ss, l_cs);
    }
    C2D_Text t_split;
    C2D_TextParse(&t_split, buf, split_str);
    C2D_TextOptimize(&t_split);
    C2D_DrawText(&t_split, C2D_WithColor, 145.0f, row_y + 3.0f, 0.0f, 0.48f, 0.48f, CLR_TEXT);

    /* Total time */
    int t_hh, t_mm, t_ss, t_cs;
    bool t_hours;
    stopwatch_format_time(sw->laps[idx].total_time_ms, &t_hh, &t_mm, &t_ss, &t_cs, &t_hours);

    char total_str[24];
    if (t_hours) {
      snprintf(total_str, sizeof(total_str), "%02d:%02d:%02d.%02d", t_hh, t_mm, t_ss, t_cs);
    } else {
      snprintf(total_str, sizeof(total_str), "%02d:%02d.%02d", t_mm, t_ss, t_cs);
    }
    C2D_Text t_total;
    C2D_TextParse(&t_total, buf, total_str);
    C2D_TextOptimize(&t_total);
    C2D_DrawText(&t_total, C2D_WithColor, 275.0f, row_y + 3.0f, 0.0f, 0.48f, 0.48f, CLR_TEXT);
  }

  ui_set_scissor(GPU_SCISSOR_DISABLE, 0, 0, 0, 0);

  /* Scroll indicator bar if more than 6 laps */
  if (sw->lap_count > visible_rows) {
    float track_y = 88.0f;
    float track_h = 144.0f;
    float max_scroll = (float)(sw->lap_count - visible_rows);
    float thumb_h = ((float)visible_rows / (float)sw->lap_count) * track_h;
    if (thumb_h < 12.0f) thumb_h = 12.0f;
    float thumb_y = track_y + ((float)sw->scroll_row / max_scroll) * (track_h - thumb_h);
    C2D_DrawRectSolid(393.0f, thumb_y, 0.0f, 3.0f, thumb_h, C2D_Color32(0x55, 0x55, 0x55, 0xFF));
  }
}

void ui_draw_top_timer(C2D_TextBuf buf, int hh, int mm, int ss) {
  draw_text_centered_x(buf, "TIMER", 40.0f, 0.55f, 400.0f);

  char str[16];
  snprintf(str, sizeof(str), "%02d:%02d:%02d", hh, mm, ss);

  C2D_Text text;
  C2D_TextParse(&text, buf, str);
  C2D_TextOptimize(&text);

  float scale = 2.0f;
  float tw, th;
  C2D_TextGetDimensions(&text, scale, scale, &tw, &th);

  float x = (400.0f - tw) / 2.0f;
  float y = (240.0f - th) / 2.0f + 14.0f;
  C2D_DrawText(&text, C2D_WithColor, x, y, 0.0f, scale, scale, CLR_TEXT);
}

/* ------------------------------------------------------------------ */
/*  Bottom Screen Header & Navigation (320×240)                       */
/* ------------------------------------------------------------------ */

void ui_draw_header(C2D_TextBuf buf, C2D_Image settings_icon,
                    const char *title) {
  /* Header background bar (y=0..32) */
  C2D_DrawRectSolid(0.0f, 0.0f, 0.0f, 320.0f, 32.0f, CLR_TAB_INACT);
  C2D_DrawRectSolid(0.0f, 31.0f, 0.0f, 320.0f, 1.0f, CLR_TAB_SEP);

  /* Screen title at left */
  C2D_Text text;
  C2D_TextParse(&text, buf, title);
  C2D_TextOptimize(&text);
  C2D_DrawText(&text, C2D_WithColor, 12.0f, 6.0f, 0.0f, 0.65f, 0.65f, CLR_TEXT);

  /* Settings icon at top-right (24×24 at x=288, y=4) */
  if (settings_icon.subtex) {
    C2D_DrawImageAt(settings_icon, 288.0f, 4.0f, 0.0f, NULL, 24.0f / 50.0f,
                    24.0f / 50.0f);
  } else {
    C2D_Text fallback;
    C2D_TextParse(&fallback, buf, "[S]");
    C2D_TextOptimize(&fallback);
    C2D_DrawText(&fallback, C2D_WithColor, 288.0f, 6.0f, 0.0f, 0.6f, 0.6f,
                 CLR_TEXT);
  }
}

void ui_draw_tab_bar(C2D_TextBuf buf, AppMode active) {
  /* Top border line */
  C2D_DrawRectSolid(0.0f, 199.0f, 0.0f, 320.0f, 1.0f, CLR_TAB_SEP);

  static const HitRect *tabs[4] = {&TAB_ALARM, &TAB_CLOCK, &TAB_STOPWATCH,
                                   &TAB_TIMER};
  static const char *labels[4] = {"Alarm", "Clock", "SW", "Timer"};

  for (int i = 0; i < 4; i++) {
    const HitRect *r = tabs[i];
    u32 bg_clr = (i == (int)active) ? CLR_TAB_ACTIVE : CLR_TAB_INACT;
    u32 tx_clr = (i == (int)active) ? CLR_TEXT : CLR_TEXT_DIM;

    C2D_DrawRectSolid(r->x, r->y, 0.0f, r->w, r->h, bg_clr);

    /* Tab divider line */
    if (i > 0)
      C2D_DrawRectSolid(r->x, r->y, 0.0f, 1.0f, r->h, CLR_TAB_SEP);

    C2D_Text text;
    C2D_TextParse(&text, buf, labels[i]);
    C2D_TextOptimize(&text);

    float tw, th;
    C2D_TextGetDimensions(&text, 0.65f, 0.65f, &tw, &th);

    float tx = r->x + (r->w - tw) / 2.0f;
    float ty = r->y + (r->h - th) / 2.0f;
    C2D_DrawText(&text, C2D_WithColor, tx, ty, 0.0f, 0.65f, 0.65f, tx_clr);
  }
}

/* ------------------------------------------------------------------ */
/*  Bottom Screen Tab Modes                                           */
/* ------------------------------------------------------------------ */

void ui_draw_alarm_list(C2D_TextBuf buf, SaveData *save, AlarmListState *state,
                        C2D_Image settings_icon, C2D_Image trash_icon) {
  if (save->alarm_count == 0) {
    draw_text_centered_x(buf, "No alarms set", 95.0f, 0.65f, 320.0f);
    draw_text_centered_x(buf, "Tap [+] above to add an alarm", 125.0f, 0.50f, 320.0f);
  } else {
    /* Enable hardware scissor for scroll viewport: X: 0, Y: 34, W: 320, H: 164
     * (clamping content between Y=34 and Y=198) */
    ui_set_scissor(GPU_SCISSOR_NORMAL, 0, 34, 320, 164);

    float start_y = 36.0f - state->scroll_y;
    float card_h = 48.0f;
    float gap = 4.0f;

    for (int i = 0; i < save->alarm_count; i++) {
      float y = start_y + i * (card_h + gap);
      if (y > 200.0f || y + card_h < 34.0f)
        continue;

      u32 card_bg = (i == state->selected_index)
                        ? C2D_Color32(0x50, 0x60, 0x70, 0xFF)
                        : CLR_BTN;
      C2D_DrawRectSolid(10.0f, y, 0.0f, 300.0f, card_h, card_bg);

      /* Time text */
      char time_str[16];
      snprintf(time_str, sizeof(time_str), "%02d:%02d", save->alarms[i].hour,
               save->alarms[i].minute);

      C2D_Text text;
      C2D_TextParse(&text, buf, time_str);
      C2D_TextOptimize(&text);
      C2D_DrawText(&text, C2D_WithColor, 20.0f, y + 4.0f, 0.0f, 0.8f, 0.8f,
                   CLR_TEXT);

      /* Repeat mode & Label subtext */
      const char *rep_str = "";
      switch (save->alarms[i].repeat_mode) {
      case REPEAT_ONCE:
        rep_str = "Once";
        break;
      case REPEAT_DAILY:
        rep_str = "Daily";
        break;
      case REPEAT_WEEKDAYS:
        rep_str = "Weekdays";
        break;
      case REPEAT_WEEKENDS:
        rep_str = "Weekends";
        break;
      }

      char sub_str[64];
      if (save->alarms[i].label[0] != '\0') {
        snprintf(sub_str, sizeof(sub_str), "%s • %s", save->alarms[i].label, rep_str);
      } else {
        snprintf(sub_str, sizeof(sub_str), "%s", rep_str);
      }
      C2D_TextParse(&text, buf, sub_str);
      C2D_TextOptimize(&text);
      C2D_DrawText(&text, C2D_WithColor, 20.0f, y + 28.0f, 0.0f, 0.45f, 0.45f,
                   CLR_TEXT_DIM);

      /* Delete [✕] / trash touch button (rightmost element on card: 10px padding to card right edge at 310.0f) */
      float del_w = 24.0f;
      float del_h = 24.0f;
      float del_x = 300.0f - del_w;
      float del_y = y + (card_h - del_h) / 2.0f;
      C2D_DrawRectSolid(del_x, del_y, 0.0f, del_w, del_h, C2D_Color32(0x8A, 0x24, 0x24, 0xFF));

      if (trash_icon.subtex) {
        float icon_sz = 24.0f;
        float scale = icon_sz / (float)trash_icon.subtex->width;
        float icon_x = del_x + (del_w - icon_sz) / 2.0f;
        float icon_y = del_y + (del_h - icon_sz) / 2.0f;
        C2D_DrawImageAt(trash_icon, icon_x, icon_y, 0.0f, NULL, scale, scale);
      } else {
        C2D_Text txt_del;
        C2D_TextParse(&txt_del, buf, "X");
        C2D_TextOptimize(&txt_del);
        float tw_del, th_del;
        C2D_TextGetDimensions(&txt_del, 0.50f, 0.50f, &tw_del, &th_del);
        C2D_DrawText(&txt_del, C2D_WithColor, del_x + (del_w - tw_del) / 2.0f, del_y + (del_h - th_del) / 2.0f, 0.0f, 0.50f, 0.50f, CLR_TEXT);
      }

      /* Toggle switch / checkbox area (left of delete button with generous spacing to avoid misclicks) */
      float box_size = 20.0f;
      float box_x = del_x - 14.0f - box_size;
      float box_y = y + (card_h - box_size) / 2.0f;

      if (save->alarms[i].enabled) {
        C2D_DrawRectSolid(box_x, box_y, 0.0f, box_size, box_size,
                          C2D_Color32(0x40, 0xC0, 0x40, 0xFF));
      } else {
        C2D_DrawRectSolid(box_x, box_y, 0.0f, box_size, box_size,
                          C2D_Color32(0x20, 0x20, 0x20, 0xFF));
      }
    }

    /* Disable hardware scissor immediately so header, tab bar, and buttons are
     * not clipped */
    ui_set_scissor(GPU_SCISSOR_DISABLE, 0, 0, 0, 0);
  }

  /* Draw header over the scrolling content */
  ui_draw_header(buf, settings_icon, "Alarms");

  /* Draw Add Button (+) on top of header */
  draw_button_scaled(buf, &BTN_ALARM_ADD, "+", 0.70f, CLR_BTN);
}

void ui_draw_alarm_edit(C2D_TextBuf buf, int h, int m, u8 repeat_mode,
                        u8 ringtone_id, bool is_new,
                        const char *ringtone_name,
                        const char *label) {
  /* 1. Header (Nav bar) */
  C2D_DrawRectSolid(0.0f, 0.0f, 0.0f, 320.0f, 36.0f, CLR_TAB_INACT);
  C2D_DrawRectSolid(0.0f, 35.0f, 0.0f, 320.0f, 1.0f, CLR_TAB_SEP);

  draw_text_centered_x(buf, is_new ? "Add Alarm" : "Edit Alarm", 9.0f, 0.60f,
                       320.0f);

  draw_button_scaled(buf, &BTN_ALARM_EDIT_CANCEL, "Cancel", 0.55f, CLR_BTN);
  draw_button_scaled(buf, &BTN_ALARM_EDIT_SAVE, "Save", 0.55f, CLR_BTN);

  /* 2. Time Stepper (hh : mm) */
  char str_h[8], str_m[8];
  snprintf(str_h, sizeof(str_h), "%02d", h);
  snprintf(str_m, sizeof(str_m), "%02d", m);

  C2D_Text th, tm, tcol;
  C2D_TextParse(&th, buf, str_h);
  C2D_TextParse(&tm, buf, str_m);
  C2D_TextParse(&tcol, buf, ":");
  C2D_TextOptimize(&th);
  C2D_TextOptimize(&tm);
  C2D_TextOptimize(&tcol);

  float tw_h, th_h, tw_m, th_m, tw_c, th_c;
  C2D_TextGetDimensions(&th, ALARM_STEPPER_SCALE, ALARM_STEPPER_SCALE, &tw_h,
                        &th_h);
  C2D_TextGetDimensions(&tm, ALARM_STEPPER_SCALE, ALARM_STEPPER_SCALE, &tw_m,
                        &th_m);
  C2D_TextGetDimensions(&tcol, ALARM_STEPPER_SCALE, ALARM_STEPPER_SCALE, &tw_c,
                        &th_c);

  /* Digit row: vertically centered on ALARM_STEPPER_CY */
  float digit_y = ALARM_STEPPER_CY - th_h / 2.0f;

  C2D_DrawText(&th, C2D_WithColor, ALARM_COL_H_CX - tw_h / 2.0f, digit_y, 0.0f,
               ALARM_STEPPER_SCALE, ALARM_STEPPER_SCALE, CLR_TEXT);
  C2D_DrawText(&tcol, C2D_WithColor, ALARM_COL_COLON_CX - tw_c / 2.0f, digit_y,
               0.0f, ALARM_STEPPER_SCALE, ALARM_STEPPER_SCALE, CLR_TEXT);
  C2D_DrawText(&tm, C2D_WithColor, ALARM_COL_M_CX - tw_m / 2.0f, digit_y, 0.0f,
               ALARM_STEPPER_SCALE, ALARM_STEPPER_SCALE, CLR_TEXT);

  /* Arrows: symmetric pad from digit bounding box edges */
  float up_cy = digit_y - ALARM_ARROW_PAD - ALARM_ARROW_TRI_H / 2.0f;
  float down_cy = digit_y + th_h + ALARM_ARROW_PAD + ALARM_ARROW_TRI_H / 2.0f;

  draw_arrow_up(ALARM_COL_H_CX, up_cy, ALARM_ARROW_TRI_W, ALARM_ARROW_TRI_H,
                CLR_TEXT);
  draw_arrow_up(ALARM_COL_M_CX, up_cy, ALARM_ARROW_TRI_W, ALARM_ARROW_TRI_H,
                CLR_TEXT);
  draw_arrow_down(ALARM_COL_H_CX, down_cy, ALARM_ARROW_TRI_W, ALARM_ARROW_TRI_H,
                  CLR_TEXT);
  draw_arrow_down(ALARM_COL_M_CX, down_cy, ALARM_ARROW_TRI_W, ALARM_ARROW_TRI_H,
                  CLR_TEXT);

  /* 3. Ringtone Selector — Centered unit: [Tone:] [<] [ Value Container ] [>]
   */
  C2D_Text txt_tone;
  C2D_TextParse(&txt_tone, buf, "Tone:");
  C2D_TextOptimize(&txt_tone);
  float tw_tl, th_tl;
  C2D_TextGetDimensions(&txt_tone, 0.55f, 0.55f, &tw_tl, &th_tl);
  float y_tone_lbl = ALARM_SEL_TONE_Y + (ALARM_SEL_ROW_H - th_tl) / 2.0f;
  C2D_DrawText(&txt_tone, C2D_WithColor, ALARM_SEL_LABEL_R - tw_tl, y_tone_lbl,
               0.0f, 0.55f, 0.55f, CLR_TEXT);

  draw_button_scaled(buf, &BTN_ALARM_TONE_LEFT, "<", 0.55f, CLR_BTN);
  draw_button_scaled(buf, &BTN_ALARM_TONE_RIGHT, ">", 0.55f, CLR_BTN);

  /* Ringtone value container */
  C2D_DrawRectSolid(ALARM_SEL_BOX_X, ALARM_SEL_TONE_Y, 0.0f, ALARM_SEL_BOX_W,
                    ALARM_SEL_ROW_H, C2D_Color32(0x22, 0x22, 0x22, 0xFF));
  C2D_Text txt_rn;
  C2D_TextParse(&txt_rn, buf, ringtone_name ? ringtone_name : "Default");
  C2D_TextOptimize(&txt_rn);
  float tw_rn, th_rn;
  C2D_TextGetDimensions(&txt_rn, 0.50f, 0.50f, &tw_rn, &th_rn);
  C2D_DrawText(&txt_rn, C2D_WithColor,
               ALARM_SEL_BOX_X + (ALARM_SEL_BOX_W - tw_rn) / 2.0f,
               ALARM_SEL_TONE_Y + (ALARM_SEL_ROW_H - th_rn) / 2.0f, 0.0f, 0.50f,
               0.50f, CLR_TEXT);

  /* 4. Repeat Selector — Centered unit: [Repeat:] [<] [ Value Container ] [>]
   */
  C2D_Text txt_rep;
  C2D_TextParse(&txt_rep, buf, "Repeat:");
  C2D_TextOptimize(&txt_rep);
  float tw_rl, th_rl;
  C2D_TextGetDimensions(&txt_rep, 0.55f, 0.55f, &tw_rl, &th_rl);
  float y_rep_lbl = ALARM_SEL_REPEAT_Y + (ALARM_SEL_ROW_H - th_rl) / 2.0f;
  C2D_DrawText(&txt_rep, C2D_WithColor, ALARM_SEL_LABEL_R - tw_rl, y_rep_lbl,
               0.0f, 0.55f, 0.55f, CLR_TEXT);

  draw_button_scaled(buf, &BTN_ALARM_REPEAT_LEFT, "<", 0.55f, CLR_BTN);
  draw_button_scaled(buf, &BTN_ALARM_REPEAT_RIGHT, ">", 0.55f, CLR_BTN);

  /* Repeat value container */
  C2D_DrawRectSolid(ALARM_SEL_BOX_X, ALARM_SEL_REPEAT_Y, 0.0f, ALARM_SEL_BOX_W,
                    ALARM_SEL_ROW_H, C2D_Color32(0x22, 0x22, 0x22, 0xFF));
  const char *rep_str = "Once";
  switch (repeat_mode) {
  case REPEAT_ONCE:
    rep_str = "Once";
    break;
  case REPEAT_DAILY:
    rep_str = "Daily";
    break;
  case REPEAT_WEEKDAYS:
    rep_str = "Weekdays";
    break;
  case REPEAT_WEEKENDS:
    rep_str = "Weekends";
    break;
  }
  C2D_Text txt_rm;
  C2D_TextParse(&txt_rm, buf, rep_str);
  C2D_TextOptimize(&txt_rm);
  float tw_rm, th_rm;
  C2D_TextGetDimensions(&txt_rm, 0.50f, 0.50f, &tw_rm, &th_rm);
  C2D_DrawText(&txt_rm, C2D_WithColor,
               ALARM_SEL_BOX_X + (ALARM_SEL_BOX_W - tw_rm) / 2.0f,
               ALARM_SEL_REPEAT_Y + (ALARM_SEL_ROW_H - th_rm) / 2.0f, 0.0f,
               0.50f, 0.50f, CLR_TEXT);

  /* 5. Label Input Row — [Label:] [ Text Box ] */
  C2D_Text txt_lbl;
  C2D_TextParse(&txt_lbl, buf, "Label:");
  C2D_TextOptimize(&txt_lbl);
  float tw_lbl, th_lbl;
  C2D_TextGetDimensions(&txt_lbl, 0.55f, 0.55f, &tw_lbl, &th_lbl);
  float y_lbl = BTN_ALARM_LABEL_INPUT.y + (BTN_ALARM_LABEL_INPUT.h - th_lbl) / 2.0f;
  C2D_DrawText(&txt_lbl, C2D_WithColor, ALARM_SEL_LABEL_R - tw_lbl, y_lbl,
               0.0f, 0.55f, 0.55f, CLR_TEXT);

  /* Input box */
  C2D_DrawRectSolid(BTN_ALARM_LABEL_INPUT.x, BTN_ALARM_LABEL_INPUT.y, 0.0f,
                    BTN_ALARM_LABEL_INPUT.w, BTN_ALARM_LABEL_INPUT.h,
                    C2D_Color32(0x22, 0x26, 0x30, 0xFF));
  /* Subtle border */
  C2D_DrawRectSolid(BTN_ALARM_LABEL_INPUT.x, BTN_ALARM_LABEL_INPUT.y, 0.0f, BTN_ALARM_LABEL_INPUT.w, 1.0f, C2D_Color32(0x40, 0x48, 0x58, 0xFF));
  C2D_DrawRectSolid(BTN_ALARM_LABEL_INPUT.x, BTN_ALARM_LABEL_INPUT.y + BTN_ALARM_LABEL_INPUT.h - 1.0f, 0.0f, BTN_ALARM_LABEL_INPUT.w, 1.0f, C2D_Color32(0x40, 0x48, 0x58, 0xFF));
  C2D_DrawRectSolid(BTN_ALARM_LABEL_INPUT.x, BTN_ALARM_LABEL_INPUT.y, 0.0f, 1.0f, BTN_ALARM_LABEL_INPUT.h, C2D_Color32(0x40, 0x48, 0x58, 0xFF));
  C2D_DrawRectSolid(BTN_ALARM_LABEL_INPUT.x + BTN_ALARM_LABEL_INPUT.w - 1.0f, BTN_ALARM_LABEL_INPUT.y, 0.0f, 1.0f, BTN_ALARM_LABEL_INPUT.h, C2D_Color32(0x40, 0x48, 0x58, 0xFF));

  const char *disp_lbl = (label && label[0] != '\0') ? label : "Tap to add label...";
  u32 lbl_clr = (label && label[0] != '\0') ? CLR_TEXT : CLR_TEXT_DIM;
  C2D_Text txt_val;
  C2D_TextParse(&txt_val, buf, disp_lbl);
  C2D_TextOptimize(&txt_val);
  float tw_val, th_val;
  C2D_TextGetDimensions(&txt_val, 0.48f, 0.48f, &tw_val, &th_val);
  C2D_DrawText(&txt_val, C2D_WithColor, BTN_ALARM_LABEL_INPUT.x + 8.0f,
               BTN_ALARM_LABEL_INPUT.y + (BTN_ALARM_LABEL_INPUT.h - th_val) / 2.0f,
               0.0f, 0.48f, 0.48f, lbl_clr);
}

void ui_draw_alarm_delete_confirm(C2D_TextBuf buf) {
  draw_modal_bg();
  draw_text_centered_x(buf, "Delete this alarm?", 90.0f, 0.70f, 320.0f);
  draw_button(buf, &BTN_CANCEL, "Cancel");
  draw_button_scaled(buf, &BTN_CONFIRM, "Confirm", 0.65f,
                     C2D_Color32(0x8A, 0x24, 0x24, 0xFF));
}

void ui_draw_alarm_ringing_top(C2D_TextBuf buf, int h, int m, u8 repeat_mode,
                               const char *label, u32 frame_counter) {
  C2D_DrawRectSolid(0, 0, 0, 400, 240, C2D_Color32(0x20, 0x00, 0x00, 0xFF));

  float scale = 2.0f + 0.1f * sinf(frame_counter * 0.05f);

  char time_str[16];
  snprintf(time_str, sizeof(time_str), "%02d:%02d", h, m);

  C2D_Text t;
  C2D_TextParse(&t, buf, time_str);
  C2D_TextOptimize(&t);

  float tw, th;
  C2D_TextGetDimensions(&t, scale, scale, &tw, &th);
  C2D_DrawText(&t, C2D_WithColor, (400.0f - tw) / 2.0f, 85.0f, 0.0f, scale,
               scale, CLR_TEXT);

  draw_text_centered_x(buf, "ALARM", 30.0f, 1.0f, 400.0f);

  const char *rep_str = "";
  switch (repeat_mode) {
  case REPEAT_ONCE:
    rep_str = "Once";
    break;
  case REPEAT_DAILY:
    rep_str = "Daily";
    break;
  case REPEAT_WEEKDAYS:
    rep_str = "Weekdays";
    break;
  case REPEAT_WEEKENDS:
    rep_str = "Weekends";
    break;
  }
  const char *status_str = (label && label[0] != '\0') ? label : rep_str;
  draw_text_centered_x(buf, status_str, 175.0f, 0.7f, 400.0f);
}

void ui_draw_alarm_ringing_bottom(C2D_TextBuf buf, int h, int m,
                                  u8 repeat_mode, const char *label) {
  C2D_DrawRectSolid(0, 0, 0, 320, 240, CLR_OVERLAY);
  C2D_DrawRectSolid(20, 20, 0, 280, 200, CLR_MODAL_BG);

  draw_text_centered_x(buf, "ALARM", 35.0f, 0.9f, 320.0f);

  char time_str[16];
  snprintf(time_str, sizeof(time_str), "%02d:%02d", h, m);
  draw_text_centered_x(buf, time_str, 65.0f, 1.4f, 320.0f);

  const char *rep_str = "";
  switch (repeat_mode) {
  case REPEAT_ONCE:
    rep_str = "Once";
    break;
  case REPEAT_DAILY:
    rep_str = "Daily";
    break;
  case REPEAT_WEEKDAYS:
    rep_str = "Weekdays";
    break;
  case REPEAT_WEEKENDS:
    rep_str = "Weekends";
    break;
  }
  const char *status_str = (label && label[0] != '\0') ? label : rep_str;
  draw_text_centered_x(buf, status_str, 105.0f, 0.6f, 320.0f);

  draw_button_scaled(buf, &BTN_ALARM_DISMISS, "DISMISS", 0.70f,
                     C2D_Color32(0x8A, 0x24, 0x24, 0xFF));
}

void ui_draw_alarm_missed_modal(C2D_TextBuf buf, int missed_count) {
  C2D_DrawRectSolid(0, 0, 0, 320, 240, CLR_OVERLAY);
  C2D_DrawRectSolid(20, 40, 0, 280, 160, CLR_MODAL_BG);

  draw_text_centered_x(buf, "Missed Alarm", 60.0f, 0.8f, 320.0f);
  char msg[64];
  if (missed_count > 1) {
    snprintf(msg, sizeof(msg), "You missed %d alarms while", missed_count);
  } else {
    snprintf(msg, sizeof(msg), "You missed an alarm while");
  }
  draw_text_centered_x(buf, msg, 90.0f, 0.5f, 320.0f);
  draw_text_centered_x(buf, "the application was closed.", 110.0f, 0.5f,
                       320.0f);

  draw_button(buf, &BTN_ALARM_MISSED_OK, "OK");
}

void ui_draw_clock_bottom(C2D_TextBuf buf) {
  draw_text_centered_x(buf, "World Clock", 85.0f, 0.75f, 320.0f);
}

void ui_draw_world_clock_list(C2D_TextBuf buf, SaveData* save, WorldClockListState* state,
                              C2D_Image settings_icon, C2D_Image trash_icon) {
  if (save->world_city_count == 0) {
    draw_text_centered_x(buf, "No world cities added", 95.0f, 0.65f, 320.0f);
    draw_text_centered_x(buf, "Tap [+] above to add a city", 125.0f, 0.50f, 320.0f);
  } else {
    /* Scissor viewport: X: 0, Y: 34, W: 320, H: 164 (clamping content between Y=34 and Y=198) */
    ui_set_scissor(GPU_SCISSOR_NORMAL, 0, 34, 320, 164);

    float start_y = 36.0f - state->scroll_y;
    float card_h = 46.0f;
    float gap = 4.0f;

    int cur_h, cur_m, cur_s;
    int cur_y, cur_mo, cur_d;
    clock_get_hms(&cur_h, &cur_m, &cur_s);
    clock_get_ymd(&cur_y, &cur_mo, &cur_d);

    for (int i = 0; i < save->world_city_count; i++) {
      float y = start_y + i * (card_h + gap);
      if (y > 200.0f || y + card_h < 34.0f)
        continue;

      u8 city_id = save->world_cities[i];
      const CityTimezone* tz = world_clock_get_city_info(city_id);

      u32 card_bg = (i == state->selected_index)
                        ? C2D_Color32(0x48, 0x58, 0x6E, 0xFF)
                        : CLR_BTN;
      C2D_DrawRectSolid(10.0f, y, 0.0f, 300.0f, card_h, card_bg);

      /* Compute local time for target city */
      int th = 0, tm = 0, ts = 0, day_off = 0, diff_h = 0, diff_m = 0;
      world_clock_calculate_time(city_id, save->home_city_id,
                                 cur_h, cur_m, cur_s,
                                 cur_y, cur_mo, cur_d,
                                 &th, &tm, &ts,
                                 &day_off, &diff_h, &diff_m);

      /* Left: City name */
      C2D_Text txt;
      C2D_TextParse(&txt, buf, tz->city);
      C2D_TextOptimize(&txt);
      C2D_DrawText(&txt, C2D_WithColor, 20.0f, y + 5.0f, 0.0f, 0.65f, 0.65f, CLR_TEXT);

      /* Left Subtext: Country & relative offset badge */
      char badge[64];
      if (city_id == save->home_city_id) {
        snprintf(badge, sizeof(badge), "Home City • %s", tz->country);
      } else {
        const char* day_name = (day_off == 0) ? "Today" : ((day_off > 0) ? "Tomorrow" : "Yesterday");
        if (diff_m != 0) {
          snprintf(badge, sizeof(badge), "%s, %+d:%02d hrs • %s", day_name, diff_h, abs(diff_m), tz->country);
        } else {
          snprintf(badge, sizeof(badge), "%s, %+d hrs • %s", day_name, diff_h, tz->country);
        }
      }
      C2D_TextParse(&txt, buf, badge);
      C2D_TextOptimize(&txt);
      C2D_DrawText(&txt, C2D_WithColor, 20.0f, y + 27.0f, 0.0f, 0.45f, 0.45f, CLR_TEXT_DIM);

      /* Right side: Delete touch button (trash icon) */
      float del_w = 24.0f;
      float del_h = 24.0f;
      float del_x = 300.0f - del_w; /* 276.0f: right edge at 300.0f, exactly 10.0f padding to card right edge at 310.0f */
      float del_y = y + (card_h - del_h) / 2.0f;
      C2D_DrawRectSolid(del_x, del_y, 0.0f, del_w, del_h, C2D_Color32(0x8A, 0x24, 0x24, 0xFF));

      if (trash_icon.subtex) {
        float icon_sz = 24.0f;
        float scale = icon_sz / (float)trash_icon.subtex->width;
        float icon_x = del_x + (del_w - icon_sz) / 2.0f;
        float icon_y = del_y + (del_h - icon_sz) / 2.0f;
        C2D_DrawImageAt(trash_icon, icon_x, icon_y, 0.0f, NULL, scale, scale);
      } else {
        C2D_Text txt_del;
        C2D_TextParse(&txt_del, buf, "X");
        C2D_TextOptimize(&txt_del);
        float tw_x, th_x;
        C2D_TextGetDimensions(&txt_del, 0.50f, 0.50f, &tw_x, &th_x);
        C2D_DrawText(&txt_del, C2D_WithColor, del_x + (del_w - tw_x) / 2.0f, del_y + (del_h - th_x) / 2.0f, 0.0f, 0.50f, 0.50f, CLR_TEXT);
      }

      /* Right side: Time string (e.g. "18:25") */
      char time_str[16];
      snprintf(time_str, sizeof(time_str), "%02d:%02d", th, tm);
      C2D_Text txt_time;
      C2D_TextParse(&txt_time, buf, time_str);
      C2D_TextOptimize(&txt_time);
      float tw_time, th_time;
      C2D_TextGetDimensions(&txt_time, 0.80f, 0.80f, &tw_time, &th_time);
      float time_x = floorf(del_x - 14.0f - tw_time);
      C2D_DrawText(&txt_time, C2D_WithColor, time_x, y + 4.0f, 0.0f, 0.80f, 0.80f, CLR_TEXT);

      /* Day/Night indicator */
      bool is_day = world_clock_is_daytime(th);
      const char* dn_str = is_day ? "DAY" : "NIGHT";
      u32 dn_clr = is_day ? C2D_Color32(0xFF, 0xDA, 0x44, 0xFF) : C2D_Color32(0x80, 0xA0, 0xD0, 0xFF);
      C2D_Text txt_dn;
      C2D_TextParse(&txt_dn, buf, dn_str);
      C2D_TextOptimize(&txt_dn);
      float tw_dn, th_dn;
      C2D_TextGetDimensions(&txt_dn, 0.40f, 0.40f, &tw_dn, &th_dn);
      C2D_DrawText(&txt_dn, C2D_WithColor, floorf(del_x - 14.0f - tw_dn), y + 28.0f, 0.0f, 0.40f, 0.40f, dn_clr);
    }

    ui_set_scissor(GPU_SCISSOR_DISABLE, 0, 0, 0, 0);
  }

  /* Draw header over the scrolling content */
  ui_draw_header(buf, settings_icon, "World Clock");

  /* Draw Add Button (+) on top of header */
  draw_button_scaled(buf, &BTN_CLOCK_ADD, "+", 0.70f, CLR_BTN);
}

void ui_draw_city_picker(C2D_TextBuf buf, const SaveData* save, CityPickerState* state) {
  int total_cities = world_clock_get_total_cities();

  /* Scissor for scroll viewport: Y: 34 to 238 */
  ui_set_scissor(GPU_SCISSOR_NORMAL, 0, 34, 320, 204);

  float start_y = 36.0f - state->scroll_y;
  float item_h = 34.0f;
  float gap = 2.0f;

  for (int i = 0; i < total_cities; i++) {
    float y = start_y + i * (item_h + gap);
    if (y > 240.0f || y + item_h < 34.0f)
      continue;

    const CityTimezone* tz = world_clock_get_city_info((u8)i);
    bool already_added = world_clock_has_city(save, (u8)i);

    u32 bg_clr;
    if (already_added) {
      bg_clr = C2D_Color32(0x38, 0x38, 0x38, 0xFF);
    } else if (i == state->selected_index) {
      bg_clr = C2D_Color32(0x48, 0x58, 0x6E, 0xFF);
    } else {
      bg_clr = CLR_BTN;
    }

    C2D_DrawRectSolid(10.0f, y, 0.0f, 296.0f, item_h, bg_clr);

    /* Left: City Name */
    C2D_Text txt;
    C2D_TextParse(&txt, buf, tz->city);
    C2D_TextOptimize(&txt);
    C2D_DrawText(&txt, C2D_WithColor, 18.0f, y + 4.0f, 0.0f, 0.58f, 0.58f,
                 already_added ? CLR_TEXT_DIM : CLR_TEXT);

    /* Left subtext: Country */
    C2D_TextParse(&txt, buf, tz->country);
    C2D_TextOptimize(&txt);
    C2D_DrawText(&txt, C2D_WithColor, 18.0f, y + 19.0f, 0.0f, 0.40f, 0.40f, CLR_TEXT_DIM);

    /* Right text: UTC standard offset or [Added] */
    if (already_added) {
      C2D_TextParse(&txt, buf, "[Added]");
      C2D_TextOptimize(&txt);
      C2D_DrawText(&txt, C2D_WithColor, 250.0f, y + 9.0f, 0.0f, 0.48f, 0.48f, CLR_TEXT_DIM);
    } else {
      char utc_str[16];
      if (tz->utc_offset_m != 0) {
        snprintf(utc_str, sizeof(utc_str), "UTC%+d:%02d", tz->utc_offset_h, tz->utc_offset_m);
      } else {
        snprintf(utc_str, sizeof(utc_str), "UTC%+d", tz->utc_offset_h);
      }
      C2D_TextParse(&txt, buf, utc_str);
      C2D_TextOptimize(&txt);
      float tw_utc, th_utc;
      C2D_TextGetDimensions(&txt, 0.48f, 0.48f, &tw_utc, &th_utc);
      C2D_DrawText(&txt, C2D_WithColor, 298.0f - tw_utc, y + 9.0f, 0.0f, 0.48f, 0.48f, CLR_TEXT_DIM);
    }
  }

  /* Scrollbar indicator on the right edge */
  float total_h = total_cities * (item_h + gap);
  float view_h = 204.0f;
  if (total_h > view_h) {
    float max_scroll = total_h - view_h;
    float track_h = 196.0f;
    float thumb_h = (view_h / total_h) * track_h;
    if (thumb_h < 16.0f) thumb_h = 16.0f;
    float thumb_y = 38.0f + (state->scroll_y / max_scroll) * (track_h - thumb_h);
    C2D_DrawRectSolid(312.0f, 38.0f, 0.0f, 3.0f, track_h, C2D_Color32(0x28, 0x28, 0x28, 0xFF));
    C2D_DrawRectSolid(312.0f, thumb_y, 0.0f, 3.0f, thumb_h, C2D_Color32(0x80, 0x80, 0x80, 0xFF));
  }

  ui_set_scissor(GPU_SCISSOR_DISABLE, 0, 0, 0, 0);

  /* Draw header bar over the scrolling content */
  C2D_DrawRectSolid(0.0f, 0.0f, 0.0f, 320.0f, 34.0f, CLR_TAB_INACT);
  C2D_DrawRectSolid(0.0f, 33.0f, 0.0f, 320.0f, 1.0f, CLR_TAB_SEP);

  draw_button_scaled(buf, &BTN_CITY_PICKER_BACK, "Back", 0.55f, CLR_BTN);
  draw_text_centered_x(buf, "Add City", 8.0f, 0.65f, 320.0f);

  char count_str[16];
  snprintf(count_str, sizeof(count_str), "%d/%d", save->world_city_count, MAX_WORLD_CITIES);
  C2D_Text txt;
  C2D_TextParse(&txt, buf, count_str);
  C2D_TextOptimize(&txt);
  float tw_c, th_c;
  C2D_TextGetDimensions(&txt, 0.48f, 0.48f, &tw_c, &th_c);
  C2D_DrawText(&txt, C2D_WithColor, 310.0f - tw_c, 10.0f, 0.0f, 0.48f, 0.48f, CLR_TEXT_DIM);
}

void ui_draw_world_clock_delete_confirm(C2D_TextBuf buf, u8 city_id) {
  draw_modal_bg();

  const CityTimezone* tz = world_clock_get_city_info(city_id);

  draw_text_centered_x(buf, "Remove City", 58.0f, 0.70f, 320.0f);

  char line1[64];
  snprintf(line1, sizeof(line1), "Remove %s", tz->city);
  draw_text_centered_x(buf, line1, 86.0f, 0.58f, 320.0f);
  draw_text_centered_x(buf, "from World Clock?", 108.0f, 0.50f, 320.0f);

  draw_button_scaled(buf, &BTN_MODAL_CITY_CANCEL, "Cancel", 0.55f, CLR_BTN);
  draw_button_scaled(buf, &BTN_MODAL_CITY_DEL, "Remove", 0.55f, C2D_Color32(0x8A, 0x24, 0x24, 0xFF));
}

void ui_draw_world_clock_set_home_confirm(C2D_TextBuf buf, u8 city_id) {
  draw_modal_bg();

  const CityTimezone* tz = world_clock_get_city_info(city_id);

  draw_text_centered_x(buf, "Set Home City", 58.0f, 0.70f, 320.0f);

  char line1[64];
  snprintf(line1, sizeof(line1), "Set %s as your", tz->city);
  draw_text_centered_x(buf, line1, 86.0f, 0.55f, 320.0f);
  draw_text_centered_x(buf, "Home reference city?", 108.0f, 0.50f, 320.0f);

  draw_button_scaled(buf, &BTN_MODAL_HOME_CANCEL, "Cancel", 0.55f, CLR_BTN);
  draw_button_scaled(buf, &BTN_MODAL_HOME_SET, "Set Home", 0.55f, C2D_Color32(0x35, 0x7A, 0x38, 0xFF));
}

void ui_draw_stopwatch_idle(C2D_TextBuf buf) {
  draw_button(buf, &BTN_SW_START, "Start");
}

void ui_draw_stopwatch_running(C2D_TextBuf buf) {
  draw_button(buf, &BTN_SW_LAP, "Lap");
  draw_button(buf, &BTN_SW_PAUSE, "Pause");
  draw_button(buf, &BTN_SW_RESET, "Reset");
}

void ui_draw_stopwatch_paused(C2D_TextBuf buf) {
  draw_button(buf, &BTN_SW_RESUME, "Resume");
  draw_button(buf, &BTN_SW_RESET_PAUSED, "Reset");
}

void ui_draw_timer_adjust(C2D_TextBuf buf, int h, int m, int s) {
  draw_button(buf, &BTN_TMR_START, "Start");
  draw_time_editor(buf, h, m, s);
}

void ui_draw_timer_running(C2D_TextBuf buf) {
  draw_button(buf, &BTN_TMR_PAUSE, "Pause");
  draw_button(buf, &BTN_TMR_RESET, "Reset");
}

void ui_draw_timer_paused(C2D_TextBuf buf) {
  draw_button(buf, &BTN_TMR_RESUME, "Resume");
  draw_button(buf, &BTN_TMR_RESET, "Reset");
}

void ui_draw_timer_ringing_top(C2D_TextBuf buf, int h, int m, int s,
                               u32 frame_counter) {
  /* Dark Blue solid fill */
  C2D_DrawRectSolid(0, 0, 0, 400, 240, C2D_Color32(0x00, 0x14, 0x30, 0xFF));

  /* Header */
  draw_text_centered_x(buf, "TIMER", 30.0f, 1.0f, 400.0f);

  /* Time text with fading / blinking alpha modulation */
  float alpha = 0.6f + 0.4f * sinf(frame_counter * 0.08f);
  if (alpha < 0.2f) alpha = 0.2f;
  if (alpha > 1.0f) alpha = 1.0f;
  u32 blink_clr = C2D_Color32(0xFF, 0xFF, 0xFF, (u8)(alpha * 255.0f));

  char time_str[16];
  snprintf(time_str, sizeof(time_str), "%02d:%02d:%02d", h, m, s);

  C2D_Text t;
  C2D_TextParse(&t, buf, time_str);
  C2D_TextOptimize(&t);

  float tw, th;
  C2D_TextGetDimensions(&t, 1.8f, 1.8f, &tw, &th);
  C2D_DrawText(&t, C2D_WithColor, (400.0f - tw) / 2.0f, 85.0f, 0.0f, 1.8f,
               1.8f, blink_clr);

  /* Subtext */
  draw_text_centered_x(buf, "Time Up!", 150.0f, 0.75f, 400.0f);
}

void ui_draw_timer_ringing_bottom(C2D_TextBuf buf, int h, int m, int s) {
  C2D_DrawRectSolid(0, 0, 0, 320, 240, CLR_OVERLAY);
  C2D_DrawRectSolid(20, 20, 0, 280, 200, CLR_MODAL_BG);

  draw_text_centered_x(buf, "TIMER", 35.0f, 0.9f, 320.0f);

  char time_str[16];
  snprintf(time_str, sizeof(time_str), "%02d:%02d:%02d", h, m, s);
  draw_text_centered_x(buf, time_str, 65.0f, 1.4f, 320.0f);

  draw_text_centered_x(buf, "Time Up!", 105.0f, 0.7f, 320.0f);

  /* Dismiss Button in Dark Blue */
  draw_button_scaled(buf, &BTN_TIMER_DISMISS, "DISMISS", 0.70f,
                     C2D_Color32(0x18, 0x48, 0x8A, 0xFF));
}

/* ------------------------------------------------------------------ */
/*  Bottom Screen Settings Overlay Screens                            */
/* ------------------------------------------------------------------ */

void ui_draw_settings_main(C2D_TextBuf buf) {
  /* Nav bar (y=0..36) */
  C2D_DrawRectSolid(0.0f, 0.0f, 0.0f, 320.0f, 36.0f, CLR_TAB_INACT);
  C2D_DrawRectSolid(0.0f, 35.0f, 0.0f, 320.0f, 1.0f, CLR_TAB_SEP);
  draw_button(buf, &BTN_SET_BACK, "Back");
  draw_text_centered_x(buf, "Settings", 8.0f, 0.7f, 320.0f);

  /* Main option buttons */
  draw_button(buf, &BTN_SET_EDIT, "Edit Time & Date");
  draw_button(buf, &BTN_SET_RESET, "Reset Time & Date");
  draw_button(buf, &BTN_SET_DISPLAY, "Display & Power");
}

void ui_draw_settings_display(C2D_TextBuf buf) {
  /* Nav bar (y=0..36) */
  C2D_DrawRectSolid(0.0f, 0.0f, 0.0f, 320.0f, 36.0f, CLR_TAB_INACT);
  C2D_DrawRectSolid(0.0f, 35.0f, 0.0f, 320.0f, 1.0f, CLR_TAB_SEP);
  draw_button(buf, &BTN_SET_BACK, "Back");
  draw_text_centered_x(buf, "Display & Power", 8.0f, 0.7f, 320.0f);

  /* Display control options */
  draw_button(buf, &BTN_DISP_BOTH_OFF, "Turn Off Both Screens");
  draw_button(buf, &BTN_DISP_BOT_OFF, "Turn Off Bottom Screen");

  /* Helper hint text */
  draw_text_centered_x(buf, "Wake: Tap screen or press D-Pad", 196.0f, 0.45f, 320.0f);
  draw_text_centered_x(buf, "Shortcut: L + R turns off both screens", 212.0f, 0.45f, 320.0f);
}

void ui_draw_settings_edit_time(C2D_TextBuf buf, int h, int m, int s) {
  /* Nav bar (y=0..36) */
  C2D_DrawRectSolid(0.0f, 0.0f, 0.0f, 320.0f, 36.0f, CLR_TAB_INACT);
  C2D_DrawRectSolid(0.0f, 35.0f, 0.0f, 320.0f, 1.0f, CLR_TAB_SEP);
  draw_button(buf, &BTN_SET_BACK, "Back");
  draw_button(buf, &BTN_SET_SAVE, "Save");
  draw_text_centered_x(buf, "Edit Time", 8.0f, 0.7f, 320.0f);

  /* Time editor steppers */
  draw_time_editor(buf, h, m, s);

  /* Navigate to Edit Date */
  draw_button(buf, &BTN_EDIT_DATE, "Edit Date");
}

void ui_draw_settings_edit_date(C2D_TextBuf buf, int y, int m, int d,
                                DateFormat fmt) {
  /* Nav bar (y=0..36) */
  C2D_DrawRectSolid(0.0f, 0.0f, 0.0f, 320.0f, 36.0f, CLR_TAB_INACT);
  C2D_DrawRectSolid(0.0f, 35.0f, 0.0f, 320.0f, 1.0f, CLR_TAB_SEP);
  draw_button(buf, &BTN_SET_BACK, "Back");
  draw_button(buf, &BTN_SET_SAVE, "Save");
  draw_text_centered_x(buf, "Edit Date", 8.0f, 0.7f, 320.0f);

  /* Column strings based on format */
  char c1[8], c2[8], c3[8];
  if (fmt == DATEFMT_ISO) {
    snprintf(c1, sizeof(c1), "%04d", y);
    snprintf(c2, sizeof(c2), "%02d", m);
    snprintf(c3, sizeof(c3), "%02d", d);
  } else if (fmt == DATEFMT_US) {
    snprintf(c1, sizeof(c1), "%02d", m);
    snprintf(c2, sizeof(c2), "%02d", d);
    snprintf(c3, sizeof(c3), "%04d", y);
  } else { /* DATEFMT_EUR */
    snprintf(c1, sizeof(c1), "%02d", d);
    snprintf(c2, sizeof(c2), "%02d", m);
    snprintf(c3, sizeof(c3), "%04d", y);
  }

  float cx[3] = {70.0f, 160.0f, 250.0f};
  const char *c_str[3] = {c1, c2, c3};
  float scale = 1.6f;

  /* Measure digit height once (all columns use the same font/scale) */
  C2D_Text probe;
  C2D_TextParse(&probe, buf, "00");
  C2D_TextOptimize(&probe);
  float pw, ph;
  C2D_TextGetDimensions(&probe, scale, scale, &pw, &ph);

  float digit_y = STEPPER_CY - ph / 2.0f;
  float up_cy = digit_y - ARROW_PAD - ARROW_TRI_H / 2.0f;
  float down_cy = digit_y + ph + ARROW_PAD + ARROW_TRI_H / 2.0f;

  for (int i = 0; i < 3; i++) {
    C2D_Text txt;
    C2D_TextParse(&txt, buf, c_str[i]);
    C2D_TextOptimize(&txt);

    float tw, th;
    C2D_TextGetDimensions(&txt, scale, scale, &tw, &th);
    C2D_DrawText(&txt, C2D_WithColor, cx[i] - tw / 2.0f, digit_y, 0.0f, scale,
                 scale, CLR_TEXT);

    draw_arrow_up(cx[i], up_cy, ARROW_TRI_W, ARROW_TRI_H, CLR_TEXT);
    draw_arrow_down(cx[i], down_cy, ARROW_TRI_W, ARROW_TRI_H, CLR_TEXT);
  }

  /* Date format selector bar — pushed to bottom action row */
  draw_button(buf, &BTN_FMT_LEFT, "<");
  draw_button(buf, &BTN_FMT_RIGHT, ">");

  const char *fmt_str = "DD/MM/YYYY (EUR)";
  if (fmt == DATEFMT_ISO) {
    fmt_str = "YYYY-MM-DD (ISO)";
  } else if (fmt == DATEFMT_US) {
    fmt_str = "MM/DD/YYYY (US)";
  }
  float fmt_label_y = BOTTOM_ACTION_Y + 6.0f;
  draw_text_centered_x(buf, fmt_str, fmt_label_y, 0.6f, 320.0f);
}

void ui_draw_modal_confirm(C2D_TextBuf buf) {
  draw_modal_bg();
  draw_text_centered_x(buf, "Reset clock to", 75.0f, 0.7f, 320.0f);
  draw_text_centered_x(buf, "system time & date?", 100.0f, 0.7f, 320.0f);
  draw_button(buf, &BTN_CANCEL, "Cancel");
  draw_button(buf, &BTN_CONFIRM, "Confirm");
}

void ui_draw_modal_success(C2D_TextBuf buf, const char *msg) {
  draw_modal_bg();
  draw_text_centered_x(buf, msg ? msg : "Saved successfully!", 88.0f, 0.7f,
                       320.0f);
  draw_button(buf, &BTN_OK, "OK");
}

void ui_draw_first_boot(C2D_TextBuf buf, bool show_ok) {
  draw_modal_bg();

  if (!show_ok) {
    draw_text_centered_x(buf, "Creating save data...", 100.0f, 0.7f, 320.0f);
  } else {
    draw_text_centered_x(buf, "Save data created!", 90.0f, 0.7f, 320.0f);
    draw_button(buf, &BTN_OK, "OK");
  }
}
