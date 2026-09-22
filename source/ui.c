#include "ui.h"
#include <stdio.h>

/* ------------------------------------------------------------------ */
/*  Layout constants (definitions for the extern declarations in ui.h) */
/* ------------------------------------------------------------------ */

/* Main screen buttons (320×240 bottom screen) */
const HitRect BTN_RESET = {  30.0f, 95.0f, 120.0f, 50.0f };
const HitRect BTN_EDIT  = { 170.0f, 95.0f, 120.0f, 50.0f };

/* Edit screen navigation */
const HitRect BTN_BACK = {  10.0f, 10.0f, 80.0f, 40.0f };
const HitRect BTN_SAVE = { 230.0f, 10.0f, 80.0f, 40.0f };

/* Edit arrows — digit-group centers at x = 80, 160, 240 */
const HitRect ARROW_H_UP   = { 55.0f,  58.0f, 50.0f, 35.0f };
const HitRect ARROW_H_DOWN = { 55.0f, 168.0f, 50.0f, 35.0f };
const HitRect ARROW_M_UP   = { 135.0f, 58.0f, 50.0f, 35.0f };
const HitRect ARROW_M_DOWN = { 135.0f, 168.0f, 50.0f, 35.0f };
const HitRect ARROW_S_UP   = { 215.0f, 58.0f, 50.0f, 35.0f };
const HitRect ARROW_S_DOWN = { 215.0f, 168.0f, 50.0f, 35.0f };

/* Modal buttons */
const HitRect BTN_OK      = { 110.0f, 145.0f, 100.0f, 40.0f };
const HitRect BTN_CANCEL  = {  40.0f, 145.0f, 100.0f, 40.0f };
const HitRect BTN_CONFIRM = { 180.0f, 145.0f, 100.0f, 40.0f };

/* ------------------------------------------------------------------ */
/*  Internal helpers                                                   */
/* ------------------------------------------------------------------ */

static void draw_button(C2D_TextBuf buf, const HitRect* r, const char* label)
{
    C2D_DrawRectSolid(r->x, r->y, 0.0f, r->w, r->h, CLR_BTN);

    C2D_Text text;
    C2D_TextParse(&text, buf, label);
    C2D_TextOptimize(&text);

    float tw, th;
    C2D_TextGetDimensions(&text, 0.7f, 0.7f, &tw, &th);

    float tx = r->x + (r->w - tw) / 2.0f;
    float ty = r->y + (r->h - th) / 2.0f;
    C2D_DrawText(&text, C2D_WithColor, tx, ty, 0.0f, 0.7f, 0.7f, CLR_TEXT);
}

static void draw_arrow_up(float cx, float cy, float w, float h, u32 clr)
{
    C2D_DrawTriangle(
        cx,       cy - h / 2.0f, clr,   /* tip   */
        cx - w / 2.0f, cy + h / 2.0f, clr,   /* bot-L */
        cx + w / 2.0f, cy + h / 2.0f, clr,   /* bot-R */
        0.0f);
}

static void draw_arrow_down(float cx, float cy, float w, float h, u32 clr)
{
    C2D_DrawTriangle(
        cx - w / 2.0f, cy - h / 2.0f, clr,   /* top-L */
        cx + w / 2.0f, cy - h / 2.0f, clr,   /* top-R */
        cx,       cy + h / 2.0f, clr,   /* tip   */
        0.0f);
}

static void draw_modal_bg(void)
{
    C2D_DrawRectSolid(0, 0, 0.0f, 320, 240, CLR_OVERLAY);
    C2D_DrawRectSolid(20, 45, 0.0f, 280, 150, CLR_MODAL_BG);
}

static void draw_text_centered_x(C2D_TextBuf buf, const char* str,
                                  float y, float scale, float screen_w)
{
    C2D_Text text;
    C2D_TextParse(&text, buf, str);
    C2D_TextOptimize(&text);

    float tw, th;
    C2D_TextGetDimensions(&text, scale, scale, &tw, &th);

    float x = (screen_w - tw) / 2.0f;
    C2D_DrawText(&text, C2D_WithColor, x, y, 0.0f, scale, scale, CLR_TEXT);
}

/* ------------------------------------------------------------------ */
/*  Public drawing functions                                           */
/* ------------------------------------------------------------------ */

void ui_draw_clock(C2D_TextBuf buf, int h, int m, int s)
{
    char str[16];
    snprintf(str, sizeof(str), "%02d:%02d:%02d", h, m, s);

    C2D_Text text;
    C2D_TextParse(&text, buf, str);
    C2D_TextOptimize(&text);

    float scale = 3.0f;
    float tw, th;
    C2D_TextGetDimensions(&text, scale, scale, &tw, &th);

    float x = (400.0f - tw) / 2.0f;
    float y = (240.0f - th) / 2.0f;

    C2D_DrawText(&text, C2D_WithColor, x, y, 0.0f, scale, scale, CLR_TEXT);
}

void ui_draw_first_boot(C2D_TextBuf buf, bool show_ok)
{
    draw_modal_bg();

    if (!show_ok) {
        draw_text_centered_x(buf, "Creating save data...", 100.0f, 0.7f, 320.0f);
    } else {
        draw_text_centered_x(buf, "Save data created!", 90.0f, 0.7f, 320.0f);
        draw_button(buf, &BTN_OK, "OK");
    }
}

void ui_draw_main(C2D_TextBuf buf)
{
    draw_button(buf, &BTN_RESET, "Reset");
    draw_button(buf, &BTN_EDIT,  "Edit");
}

void ui_draw_edit(C2D_TextBuf buf, int h, int m, int s)
{
    /* Navigation buttons */
    draw_button(buf, &BTN_BACK, "Back");
    draw_button(buf, &BTN_SAVE, "Save");

    /* Digit-group layout: centers at x = 80, 160, 240 */
    float scale  = 2.0f;
    float time_y = 100.0f;
    float cx_h   = 80.0f;
    float cx_m   = 160.0f;
    float cx_s   = 240.0f;

    /* Format digit strings */
    char hh[4], mm[4], ss[4];
    snprintf(hh, sizeof(hh), "%02d", h);
    snprintf(mm, sizeof(mm), "%02d", m);
    snprintf(ss, sizeof(ss), "%02d", s);

    /* Parse texts */
    C2D_Text t_h, t_m, t_s, t_col;
    C2D_TextParse(&t_h,   buf, hh);
    C2D_TextOptimize(&t_h);
    C2D_TextParse(&t_m,   buf, mm);
    C2D_TextOptimize(&t_m);
    C2D_TextParse(&t_s,   buf, ss);
    C2D_TextOptimize(&t_s);
    C2D_TextParse(&t_col, buf, ":");
    C2D_TextOptimize(&t_col);

    /* Measure for centering */
    float dw, dh, cw, ch;
    C2D_TextGetDimensions(&t_h,   scale, scale, &dw, &dh);
    C2D_TextGetDimensions(&t_col, scale, scale, &cw, &ch);

    /* Draw digit pairs and colons */
    C2D_DrawText(&t_h,   C2D_WithColor, cx_h - dw / 2, time_y, 0.0f, scale, scale, CLR_TEXT);
    C2D_DrawText(&t_col, C2D_WithColor, 120.0f - cw / 2, time_y, 0.0f, scale, scale, CLR_TEXT);
    C2D_DrawText(&t_m,   C2D_WithColor, cx_m - dw / 2, time_y, 0.0f, scale, scale, CLR_TEXT);
    C2D_DrawText(&t_col, C2D_WithColor, 200.0f - cw / 2, time_y, 0.0f, scale, scale, CLR_TEXT);
    C2D_DrawText(&t_s,   C2D_WithColor, cx_s - dw / 2, time_y, 0.0f, scale, scale, CLR_TEXT);

    /* Arrows (triangles, 24×16 px) */
    float aw = 24.0f, ah = 16.0f;
    float up_y   = 75.0f;
    float down_y = time_y + dh + 12.0f;

    draw_arrow_up(cx_h, up_y, aw, ah, CLR_TEXT);
    draw_arrow_up(cx_m, up_y, aw, ah, CLR_TEXT);
    draw_arrow_up(cx_s, up_y, aw, ah, CLR_TEXT);

    draw_arrow_down(cx_h, down_y, aw, ah, CLR_TEXT);
    draw_arrow_down(cx_m, down_y, aw, ah, CLR_TEXT);
    draw_arrow_down(cx_s, down_y, aw, ah, CLR_TEXT);
}

void ui_draw_modal_confirm(C2D_TextBuf buf)
{
    draw_modal_bg();
    draw_text_centered_x(buf, "Reset clock to",  75.0f, 0.7f, 320.0f);
    draw_text_centered_x(buf, "system time?",   100.0f, 0.7f, 320.0f);
    draw_button(buf, &BTN_CANCEL,  "Cancel");
    draw_button(buf, &BTN_CONFIRM, "Confirm");
}

void ui_draw_modal_success(C2D_TextBuf buf)
{
    draw_modal_bg();
    draw_text_centered_x(buf, "Time saved",      75.0f, 0.7f, 320.0f);
    draw_text_centered_x(buf, "successfully!",  100.0f, 0.7f, 320.0f);
    draw_button(buf, &BTN_OK, "OK");
}
