#include "manual.h"
#include <stdio.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/*  HitRect definitions                                               */
/* ------------------------------------------------------------------ */

const HitRect BTN_MANUAL_BACK = {10.0f, 4.0f, 65.0f, 28.0f};
const HitRect BTN_MANUAL_PREV = {85.0f, 4.0f, 30.0f, 28.0f};
const HitRect BTN_MANUAL_NEXT = {280.0f, 4.0f, 30.0f, 28.0f};

/* ------------------------------------------------------------------ */
/*  Page Metadata                                                     */
/* ------------------------------------------------------------------ */

static const char *const s_page_titles[MANUAL_PAGE_COUNT] = {
    "Tips & Setup (1/2)", "Shortcuts (2/2)"};

const char *manual_get_page_title(int page_idx) {
  if (page_idx < 0 || page_idx >= MANUAL_PAGE_COUNT)
    return "";
  return s_page_titles[page_idx];
}

/* ------------------------------------------------------------------ */
/*  Content Engine Data Structures                                    */
/* ------------------------------------------------------------------ */

typedef enum {
  ML_HEADER,
  ML_SUBHEADER,
  ML_BODY,
  ML_BULLET,
  ML_CALLOUT_HEADER,
  ML_CALLOUT_TEXT,
  ML_CALLOUT_END,
  ML_SPACER
} ManualLineType;

typedef struct {
  ManualLineType type;
  const char *text;
} ManualLine;

/* Page 0: Essential Tips & Setup (Practical Bedside Guide) */
static const ManualLine s_lines_page_0[] = {
    {ML_HEADER, "ESSENTIAL TIPS & SETUP"},
    {ML_CALLOUT_HEADER, "OVERNIGHT ALARM SETUP (MUST-READ)"},
    {ML_CALLOUT_TEXT, "* Leave lid open (closing lid cuts internal speakers)"},
    {ML_CALLOUT_TEXT, "  (Or connect headphones/AUX to close lid)"},
    {ML_CALLOUT_TEXT, "* Set volume slider to maximum"},
    {ML_CALLOUT_TEXT, "* Turn off screens with [L + R] to save battery"},
    {ML_CALLOUT_TEXT, "  (Screens wake up automatically on alarm!)"},
    {ML_CALLOUT_TEXT, "* Keep plugged into charger so battery won't die"},
    {ML_CALLOUT_TEXT, "* Keep app open (cannot run in the background)"},
    {ML_CALLOUT_END, NULL},
    {ML_SPACER, NULL},
    {ML_SUBHEADER, "Dimming Front LEDs at Night"},
    {ML_BODY, "If console power or wireless LEDs are too bright:"},
    {ML_BULLET, "Open Rosalina Menu: [L] + [D-Pad Down] + [Select]"},
    {ML_BULLET, "Choose: System Configuration -> Toggle LEDs"},
    {ML_SPACER, NULL},
    {ML_SUBHEADER, "Custom MP3 Ringtones"},
    {ML_BULLET, "Place your .mp3 audio files on your SD card in:"},
    {ML_BULLET, "  sdmc:/3ds/3ds-clock/ringtones/"},
    {ML_BULLET, "They will appear in the ringtone list automatically."},
    {ML_BULLET, "Tap the ringtone name in the editor to preview it."},
    {ML_SPACER, NULL},
    {ML_SUBHEADER, "Safe Timekeeping"},
    {ML_BODY, "Changing time or date inside this app will never alter"},
    {ML_BODY, "your Nintendo 3DS console's system clock."},
    {ML_SPACER, NULL},
    {ML_SUBHEADER, "World Clock Home City"},
    {ML_BODY, "Set your Home City to your current local timezone so"},
    {ML_BODY, "all other cities show accurate relative hour offsets."},
    {ML_SPACER, NULL}};

/* Page 1: Controls & Shortcuts Cheatsheet */
static const ManualLine s_lines_page_1[] = {
    {ML_HEADER, "CONTROLS & SHORTCUTS"},
    {ML_SUBHEADER, "General Shortcuts (Anywhere)"},
    {ML_BULLET, "[L] / [R]        : Switch between tabs"},
    {ML_BULLET, "[L + R]        : Turn off both screens (standby)"},
    {ML_BULLET, "Touch / D-Pad  : Wake screens from standby"},
    {ML_BULLET, "[SELECT]       : Open Settings from any tab"},
    {ML_BULLET, "[START]        : Save and exit to Homebrew Launcher"},
    {ML_SPACER, NULL},
    {ML_SUBHEADER, "Alarm Tab"},
    {ML_BULLET, "D-Pad / Circle Pad : Scroll through alarm cards"},
    {ML_BULLET, "[A]            : Edit selected alarm"},
    {ML_BULLET, "[Y]            : Quick toggle alarm On / Off"},
    {ML_BULLET, "[X] / Red [X]  : Delete selected alarm"},
    {ML_BULLET, "[+] Header     : Add a new alarm (up to 32 alarms)"},
    {ML_SPACER, NULL},
    {ML_SUBHEADER, "World Clock Tab"},
    {ML_BULLET, "D-Pad / Circle Pad : Scroll through cities"},
    {ML_BULLET, "[A]            : Set selected city as Home City"},
    {ML_BULLET, "[Y] / [+]      : Add a city (up to 32 world cities)"},
    {ML_BULLET, "[X] / Red [X]  : Remove city from your list"},
    {ML_SPACER, NULL},
    {ML_SUBHEADER, "Stopwatch & Timer Tabs"},
    {ML_BULLET, "[A]            : Start / Pause / Resume"},
    {ML_BULLET, "[B]            : Reset stopwatch or timer"},
    {ML_BULLET, "[Y] / [Lap]    : Record lap split time (Stopwatch)"},
    {ML_SPACER, NULL}};

typedef struct {
  const ManualLine *lines;
  int count;
} PageData;

static const PageData s_pages[MANUAL_PAGE_COUNT] = {
    {s_lines_page_0, sizeof(s_lines_page_0) / sizeof(ManualLine)},
    {s_lines_page_1, sizeof(s_lines_page_1) / sizeof(ManualLine)}};

/* ------------------------------------------------------------------ */
/*  Scroll Calculations                                               */
/* ------------------------------------------------------------------ */

static float get_line_height(ManualLineType type) {
  switch (type) {
  case ML_HEADER:
    return 26.0f;
  case ML_SUBHEADER:
    return 22.0f;
  case ML_CALLOUT_HEADER:
    return 25.0f;
  case ML_CALLOUT_TEXT:
    return 15.0f;
  case ML_CALLOUT_END:
    return 14.0f;
  case ML_BULLET:
    return 16.0f;
  case ML_BODY:
    return 16.0f;
  case ML_SPACER:
    return 8.0f;
  default:
    return 16.0f;
  }
}

float manual_get_max_scroll(int page_idx) {
  if (page_idx < 0 || page_idx >= MANUAL_PAGE_COUNT)
    return 0.0f;
  const PageData *pd = &s_pages[page_idx];
  float total_h = 10.0f;
  for (int i = 0; i < pd->count; i++) {
    total_h += get_line_height(pd->lines[i].type);
  }
  total_h += 16.0f;

  float view_h = 196.0f;
  float max_scroll = total_h - view_h;
  return max_scroll > 0.0f ? max_scroll : 0.0f;
}

/* ------------------------------------------------------------------ */
/*  Text Rendering Helper                                             */
/* ------------------------------------------------------------------ */

static void draw_text_left(C2D_TextBuf buf, const char *str, float x, float y,
                           float scale, u32 clr) {
  C2D_Text text;
  C2D_TextParse(&text, buf, str);
  C2D_TextOptimize(&text);
  C2D_DrawText(&text, C2D_WithColor, x, y, 0.0f, scale, scale, clr);
}

/* ------------------------------------------------------------------ */
/*  Bottom Screen Reader Renderer                                     */
/* ------------------------------------------------------------------ */

void manual_draw_bottom(C2D_TextBuf buf, int page_idx, float scroll_y) {
  /* Header Bar (y=0..36) */
  C2D_DrawRectSolid(0.0f, 0.0f, 0.0f, 320.0f, 36.0f, CLR_TAB_INACT);
  C2D_DrawRectSolid(0.0f, 35.0f, 0.0f, 320.0f, 1.0f, CLR_TAB_SEP);

  draw_button(buf, &BTN_MANUAL_BACK, "Back");

  /* Stepper Controls */
  draw_stepper_arrow_button_horizontal(&BTN_MANUAL_PREV, true);

  float label_cx = (115.0f + 280.0f) / 2.0f;
  draw_text_centered_x(buf, manual_get_page_title(page_idx), 9.0f, 0.52f,
                       label_cx * 2.0f);

  draw_stepper_arrow_button_horizontal(&BTN_MANUAL_NEXT, false);

  /* Scissor-clipped scrollable body (y=36..236) */
  ui_set_scissor(GPU_SCISSOR_NORMAL, 0, 36, 320, 204);

  if (page_idx >= 0 && page_idx < MANUAL_PAGE_COUNT) {
    const PageData *pd = &s_pages[page_idx];
    float cur_y = 44.0f - scroll_y;

    for (int i = 0; i < pd->count; i++) {
      const ManualLine *line = &pd->lines[i];
      float lh = get_line_height(line->type);
      float render_h = (line->type == ML_CALLOUT_HEADER) ? 138.0f : lh;

      /* View-frustum culling */
      if (cur_y + render_h >= 36.0f && cur_y <= 240.0f) {
        switch (line->type) {
        case ML_HEADER:
          C2D_DrawRectSolid(10.0f, cur_y + 2.0f, 0.0f, 3.0f, 18.0f,
                            C2D_Color32(0x40, 0x80, 0xD0, 0xFF));
          draw_text_left(buf, line->text, 18.0f, cur_y + 1.0f, 0.58f, CLR_TEXT);
          break;

        case ML_SUBHEADER:
          draw_text_left(buf, line->text, 12.0f, cur_y + 1.0f, 0.52f,
                         C2D_Color32(0xE0, 0xE0, 0xE0, 0xFF));
          break;

        case ML_CALLOUT_HEADER:
          /* Amber/Red Callout box with symmetric margins and balanced padding
           */
          C2D_DrawRectSolid(10.0f, cur_y, 0.0f, 300.0f, 138.0f,
                            C2D_Color32(0x28, 0x1A, 0x16, 0xFF));
          C2D_DrawRectSolid(10.0f, cur_y, 0.0f, 300.0f, 1.0f,
                            C2D_Color32(0x9E, 0x30, 0x24, 0xFF));
          C2D_DrawRectSolid(10.0f, cur_y + 137.0f, 0.0f, 300.0f, 1.0f,
                            C2D_Color32(0x9E, 0x30, 0x24, 0xFF));
          C2D_DrawRectSolid(10.0f, cur_y, 0.0f, 1.0f, 138.0f,
                            C2D_Color32(0x9E, 0x30, 0x24, 0xFF));
          C2D_DrawRectSolid(309.0f, cur_y, 0.0f, 1.0f, 138.0f,
                            C2D_Color32(0x9E, 0x30, 0x24, 0xFF));

          draw_text_left(buf, line->text, 18.0f, cur_y + 7.0f, 0.48f,
                         C2D_Color32(0xFF, 0x90, 0x70, 0xFF));
          break;

        case ML_CALLOUT_TEXT:
          draw_text_left(buf, line->text, 18.0f, cur_y + 1.0f, 0.42f,
                         C2D_Color32(0xF0, 0xD4, 0xCC, 0xFF));
          break;

        case ML_CALLOUT_END:
          break;

        case ML_BULLET:
          draw_text_left(buf, line->text, 14.0f, cur_y, 0.45f,
                         C2D_Color32(0xD0, 0xD0, 0xD0, 0xFF));
          break;

        case ML_BODY:
          draw_text_left(buf, line->text, 14.0f, cur_y, 0.45f,
                         C2D_Color32(0xD0, 0xD0, 0xD0, 0xFF));
          break;

        case ML_SPACER:
          break;
        }
      }
      cur_y += lh;
    }
  }

  ui_set_scissor(GPU_SCISSOR_DISABLE, 0, 0, 0, 0);

  /* Vertical Scrollbar Indicator */
  float max_s = manual_get_max_scroll(page_idx);
  if (max_s > 0.0f) {
    float track_x = 314.0f;
    float track_y = 40.0f;
    float track_w = 3.0f;
    float track_h = 194.0f;

    C2D_DrawRectSolid(track_x, track_y, 0.0f, track_w, track_h,
                      C2D_Color32(0x22, 0x22, 0x22, 0xFF));

    float thumb_h = track_h * (track_h / (track_h + max_s));
    if (thumb_h < 24.0f)
      thumb_h = 24.0f;
    float thumb_y = track_y + (scroll_y / max_s) * (track_h - thumb_h);

    C2D_DrawRectSolid(track_x, thumb_y, 0.0f, track_w, thumb_h,
                      C2D_Color32(0x60, 0x64, 0x70, 0xFF));
  }
}
