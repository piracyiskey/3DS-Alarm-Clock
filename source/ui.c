#include "ui.h"
#include <stdio.h>

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
const HitRect BTN_SW_PAUSE = {30.0f, 75.0f, 120.0f, 50.0f};
const HitRect BTN_SW_RESUME = {30.0f, 75.0f, 120.0f, 50.0f};
const HitRect BTN_SW_RESET = {170.0f, 75.0f, 120.0f, 50.0f};

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
const HitRect BTN_ALARM_DISMISS = {60.0f, 140.0f, 200.0f, 40.0f};
const HitRect BTN_ALARM_MISSED_OK = {60.0f, 140.0f, 200.0f, 40.0f};

/* Dedicated 2-Column Alarm Stepper Arrows — col centers x=105, 215; Y estimated
 * for scale 1.5 dh≈28 */
const HitRect ARROW_ALARM_H_UP = {80.0f, 35.0f, 50.0f, 26.0f};
const HitRect ARROW_ALARM_H_DOWN = {80.0f, 89.0f, 50.0f, 28.0f};
const HitRect ARROW_ALARM_M_UP = {190.0f, 35.0f, 50.0f, 26.0f};
const HitRect ARROW_ALARM_M_DOWN = {190.0f, 89.0f, 50.0f, 28.0f};

/* Settings overlay buttons */
const HitRect BTN_SET_BACK = {10.0f, 4.0f, 70.0f, 28.0f};
const HitRect BTN_SET_SAVE = {240.0f, 4.0f, 70.0f, 28.0f};
const HitRect BTN_SET_EDIT = {50.0f, 68.0f, 220.0f, 44.0f};
const HitRect BTN_SET_RESET = {50.0f, 132.0f, 220.0f, 44.0f};
const HitRect BTN_EDIT_DATE = {80.0f, BOTTOM_ACTION_Y, 160.0f, 30.0f};

/* Modal buttons */
const HitRect BTN_OK = {110.0f, 145.0f, 100.0f, 40.0f};
const HitRect BTN_CANCEL = {40.0f, 145.0f, 100.0f, 40.0f};
const HitRect BTN_CONFIRM = {180.0f, 145.0f, 100.0f, 40.0f};

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

/* Helper to set hardware scissor in landscape user coordinates (320x240) on the
 * tilted 240x320 bottom framebuffer */
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

void ui_draw_top_clock_with_date(C2D_TextBuf buf, int h, int m, int s,
                                 const char *date_str) {
  /* Telemetry header */
  draw_text_centered_x(buf, "CLOCK", 35.0f, 0.55f, 400.0f);

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
  float y = 75.0f;
  C2D_DrawText(&text, C2D_WithColor, x, y, 0.0f, scale, scale, CLR_TEXT);

  /* Date display string below clock */
  if (date_str && date_str[0] != '\0') {
    draw_text_centered_x(buf, date_str, 150.0f, 0.65f, 400.0f);
  }
}

void ui_draw_top_stopwatch(C2D_TextBuf buf, int hh, int mm, int ss, int cs,
                           bool show_hours) {
  draw_text_centered_x(buf, "STOPWATCH", 45.0f, 0.55f, 400.0f);

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
  float y = (240.0f - th) / 2.0f + 10.0f;
  C2D_DrawText(&text, C2D_WithColor, x, y, 0.0f, scale, scale, CLR_TEXT);
}

void ui_draw_top_timer(C2D_TextBuf buf, int hh, int mm, int ss) {
  draw_text_centered_x(buf, "TIMER", 45.0f, 0.55f, 400.0f);

  char str[16];
  snprintf(str, sizeof(str), "%02d:%02d:%02d", hh, mm, ss);

  C2D_Text text;
  C2D_TextParse(&text, buf, str);
  C2D_TextOptimize(&text);

  float scale = 2.0f;
  float tw, th;
  C2D_TextGetDimensions(&text, scale, scale, &tw, &th);

  float x = (400.0f - tw) / 2.0f;
  float y = (240.0f - th) / 2.0f + 10.0f;
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
                        C2D_Image settings_icon) {
  if (save->alarm_count == 0) {
    draw_text_centered_x(buf, "No alarms set", 100.0f, 0.6f, 320.0f);
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

      /* Repeat mode */
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
      C2D_TextParse(&text, buf, rep_str);
      C2D_TextOptimize(&text);
      C2D_DrawText(&text, C2D_WithColor, 20.0f, y + 28.0f, 0.0f, 0.5f, 0.5f,
                   CLR_TEXT_DIM);

      /* Toggle switch / checkbox area (right side) */
      float box_size = 20.0f;
      float box_x = 300.0f - 10.0f - box_size;
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
                        const char *ringtone_name) {
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

  /* 5. Delete Alarm Button (Option A) */
  if (!is_new) {
    draw_button_scaled(buf, &BTN_ALARM_EDIT_DELETE, "Delete Alarm", 0.55f,
                       C2D_Color32(0x8A, 0x24, 0x24, 0xFF));
  }
}

void ui_draw_alarm_delete_confirm(C2D_TextBuf buf) {
  draw_modal_bg();
  draw_text_centered_x(buf, "Delete this alarm?", 90.0f, 0.70f, 320.0f);
  draw_button(buf, &BTN_CANCEL, "Cancel");
  draw_button_scaled(buf, &BTN_CONFIRM, "Confirm", 0.65f,
                     C2D_Color32(0x8A, 0x24, 0x24, 0xFF));
}

void ui_draw_alarm_ringing_top(C2D_TextBuf buf, int h, int m, u8 repeat_mode,
                               u32 frame_counter) {
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
  draw_text_centered_x(buf, rep_str, 175.0f, 0.7f, 400.0f);
}

void ui_draw_alarm_ringing_bottom(C2D_TextBuf buf, int h, int m,
                                  u8 repeat_mode) {
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
  draw_text_centered_x(buf, rep_str, 105.0f, 0.6f, 320.0f);

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
  draw_text_centered_x(buf, "3DS Clock", 85.0f, 0.75f, 320.0f);
  draw_text_centered_x(buf, "Select a tab below to switch modes", 115.0f, 0.5f,
                       320.0f);
}

void ui_draw_stopwatch_idle(C2D_TextBuf buf) {
  draw_button(buf, &BTN_SW_START, "Start");
}

void ui_draw_stopwatch_running(C2D_TextBuf buf) {
  draw_button(buf, &BTN_SW_PAUSE, "Pause");
  draw_button(buf, &BTN_SW_RESET, "Reset");
}

void ui_draw_stopwatch_paused(C2D_TextBuf buf) {
  draw_button(buf, &BTN_SW_RESUME, "Resume");
  draw_button(buf, &BTN_SW_RESET, "Reset");
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

void ui_draw_timer_expired_modal(C2D_TextBuf buf) {
  draw_modal_bg();
  draw_text_centered_x(buf, "Timer Expired!", 85.0f, 0.85f, 320.0f);
  draw_button(buf, &BTN_OK, "OK");
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
