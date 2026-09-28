#include "about.h"
#include "ui.h"
#include <stdio.h>

/* ------------------------------------------------------------------ */
/*  Line Types & Data Definition                                      */
/* ------------------------------------------------------------------ */

typedef enum {
  AL_APP_TITLE,
  AL_APP_VERSION,
  AL_SECTION_HEADER,
  AL_LINE_BODY,
  AL_LINE_MUTED,
  AL_LINE_BULLET,
  AL_DIVIDER,
  AL_SPACER,
  AL_CARD_THANK_YOU
} AboutLineType;

typedef struct {
  AboutLineType type;
  const char *text;
} AboutLine;

static const AboutLine s_about_lines[] = {
    {AL_APP_TITLE, "3DS Alarm Clock"},
    {AL_APP_VERSION, "Version 1.0.0"},
    {AL_DIVIDER, NULL},

    {AL_SECTION_HEADER, "DEVELOPER"},
    {AL_LINE_BODY, "Developed by Nguyen Manh Dung"},
    {AL_LINE_MUTED, "GitHub: piracyiskey/3DS-Alarm-Clock"},
    {AL_LINE_MUTED, "Contact: cheesemcrib2004@gmail.com"},
    {AL_SPACER, NULL},

    {AL_SECTION_HEADER, "CREDITS & ASSETS"},
    {AL_LINE_BODY, "Audio: Pixabay (Royalty-free)"},
    {AL_LINE_BODY, "Icons: Icons8 (icons8.com)"},
    {AL_LINE_MUTED, "Built with devkitPro & libctru"},
    {AL_SPACER, NULL},

    {AL_SECTION_HEADER, "SPECIAL THANKS"},
    {AL_LINE_BULLET, "Nintendo 3DS Homebrew Community"},
    {AL_LINE_BULLET, "Luma3DS Team"},
    {AL_SPACER, NULL},

    {AL_CARD_THANK_YOU, "Thank you for downloading and using 3DS Clock!"}};

static const int s_about_line_count = sizeof(s_about_lines) / sizeof(AboutLine);

/* ------------------------------------------------------------------ */
/*  Scroll & Dimension Helpers                                        */
/* ------------------------------------------------------------------ */

static float get_about_line_height(AboutLineType type) {
  switch (type) {
  case AL_APP_TITLE:
    return 24.0f;
  case AL_APP_VERSION:
    return 18.0f;
  case AL_SECTION_HEADER:
    return 24.0f;
  case AL_LINE_BODY:
    return 18.0f;
  case AL_LINE_MUTED:
    return 17.0f;
  case AL_LINE_BULLET:
    return 18.0f;
  case AL_DIVIDER:
    return 12.0f;
  case AL_SPACER:
    return 10.0f;
  case AL_CARD_THANK_YOU:
    return 54.0f;
  default:
    return 16.0f;
  }
}

float about_get_max_scroll(void) {
  float total_h = 44.0f;
  for (int i = 0; i < s_about_line_count; i++) {
    total_h += get_about_line_height(s_about_lines[i].type);
  }
  total_h += 16.0f;

  float view_h = 240.0f;
  float max_s = total_h - view_h;
  return max_s > 0.0f ? max_s : 0.0f;
}

/* ------------------------------------------------------------------ */
/*  Drawing Helpers                                                   */
/* ------------------------------------------------------------------ */

static void draw_text_left(C2D_TextBuf buf, const char *str, float x, float y,
                           float scale, u32 clr) {
  C2D_Text text;
  C2D_TextParse(&text, buf, str);
  C2D_TextOptimize(&text);
  C2D_DrawText(&text, C2D_WithColor, x, y, 0.0f, scale, scale, clr);
}

static void draw_box(float x, float y, float w, float h, u32 bg, u32 border) {
  C2D_DrawRectSolid(x, y, 0.0f, w, h, bg);
  C2D_DrawRectSolid(x, y, 0.0f, w, 1.0f, border);
  C2D_DrawRectSolid(x, y + h - 1.0f, 0.0f, w, 1.0f, border);
  C2D_DrawRectSolid(x, y, 0.0f, 1.0f, h, border);
  C2D_DrawRectSolid(x + w - 1.0f, y, 0.0f, 1.0f, h, border);
}

/* ------------------------------------------------------------------ */
/*  Bottom Screen Renderer                                            */
/* ------------------------------------------------------------------ */

void about_draw_bottom(C2D_TextBuf buf, float scroll_y) {
  /* Header Bar (y=0..36) */
  C2D_DrawRectSolid(0.0f, 0.0f, 0.0f, 320.0f, 36.0f, CLR_TAB_INACT);
  C2D_DrawRectSolid(0.0f, 35.0f, 0.0f, 320.0f, 1.0f, CLR_TAB_SEP);

  draw_button(buf, &BTN_SET_BACK, "Back");
  draw_text_centered_x(buf, "About & Credits", 8.0f, 0.65f, 320.0f);

  /* Scissor-clipped scrollable body (y=36..240) */
  ui_set_scissor(GPU_SCISSOR_NORMAL, 0, 36, 320, 204);

  float cur_y = 44.0f - scroll_y;
  const u32 clr_accent_blue = C2D_Color32(0x61, 0xAF, 0xEF, 0xFF);
  const u32 clr_gold = C2D_Color32(0xE5, 0xC0, 0x7B, 0xFF);

  for (int i = 0; i < s_about_line_count; i++) {
    const AboutLine *line = &s_about_lines[i];
    float lh = get_about_line_height(line->type);

    /* View-frustum culling */
    if (cur_y + lh >= 36.0f && cur_y <= 240.0f) {
      switch (line->type) {
      case AL_APP_TITLE:
        /* Accent pill on left */
        C2D_DrawRectSolid(14.0f, cur_y + 2.0f, 0.0f, 3.0f, 18.0f,
                          clr_accent_blue);
        draw_text_left(buf, line->text, 22.0f, cur_y, 0.58f, CLR_TEXT);
        break;

      case AL_APP_VERSION:
        draw_text_left(buf, line->text, 22.0f, cur_y, 0.44f, CLR_TEXT_DIM);
        break;

      case AL_DIVIDER:
        C2D_DrawRectSolid(14.0f, cur_y + 5.0f, 0.0f, 292.0f, 1.0f,
                          C2D_Color32(0x2E, 0x34, 0x40, 0xFF));
        break;

      case AL_SECTION_HEADER:
        /* Header pill and category text */
        C2D_DrawRectSolid(14.0f, cur_y + 3.0f, 0.0f, 2.0f, 14.0f,
                          clr_accent_blue);
        draw_text_left(buf, line->text, 20.0f, cur_y + 1.0f, 0.48f,
                       clr_accent_blue);
        break;

      case AL_LINE_BODY:
        draw_text_left(buf, line->text, 14.0f, cur_y, 0.45f, CLR_TEXT);
        break;

      case AL_LINE_MUTED:
        draw_text_left(buf, line->text, 14.0f, cur_y, 0.42f, CLR_TEXT_DIM);
        break;

      case AL_LINE_BULLET:
        draw_text_left(buf, line->text, 14.0f, cur_y, 0.45f, CLR_TEXT);
        break;

      case AL_SPACER:
        break;

      case AL_CARD_THANK_YOU:
        /* Dedicated thank-you callout card */
        draw_box(14.0f, cur_y, 292.0f, 44.0f,
                 C2D_Color32(0x1E, 0x1C, 0x16, 0xFF),
                 C2D_Color32(0x5A, 0x4A, 0x2A, 0xFF));
        {
          C2D_Text txt;
          C2D_TextParse(&txt, buf, line->text);
          C2D_TextOptimize(&txt);
          float tw, th;
          C2D_TextGetDimensions(&txt, 0.43f, 0.43f, &tw, &th);
          float tx = 14.0f + (292.0f - tw) / 2.0f;
          float ty = cur_y + (44.0f - th) / 2.0f;
          C2D_DrawText(&txt, C2D_WithColor, tx, ty, 0.0f, 0.43f, 0.43f,
                       clr_gold);
        }
        break;
      }
    }
    cur_y += lh;
  }

  ui_set_scissor(GPU_SCISSOR_DISABLE, 0, 0, 0, 0);

  /* Vertical Scrollbar Indicator */
  float max_s = about_get_max_scroll();
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
