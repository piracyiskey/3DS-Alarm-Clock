#pragma once
#include <citro2d.h>

/* Hit rectangle for touch input */
typedef struct { float x, y, w, h; } HitRect;

/* Color palette — dark grey background, white text */
#define CLR_BG       C2D_Color32(0x30, 0x30, 0x30, 0xFF)
#define CLR_TEXT     C2D_Color32(0xFF, 0xFF, 0xFF, 0xFF)
#define CLR_BTN      C2D_Color32(0x50, 0x50, 0x50, 0xFF)
#define CLR_MODAL_BG C2D_Color32(0x40, 0x40, 0x40, 0xFF)
#define CLR_OVERLAY  C2D_Color32(0x00, 0x00, 0x00, 0xA0)

/* --- Button / Arrow layout (shared between drawing and input) --- */

/* Main screen */
extern const HitRect BTN_RESET;
extern const HitRect BTN_EDIT;

/* Edit screen */
extern const HitRect BTN_BACK;
extern const HitRect BTN_SAVE;

/* Edit arrows (digit-group centers: x=80, 160, 240) */
extern const HitRect ARROW_H_UP,  ARROW_H_DOWN;
extern const HitRect ARROW_M_UP,  ARROW_M_DOWN;
extern const HitRect ARROW_S_UP,  ARROW_S_DOWN;

/* Modal buttons */
extern const HitRect BTN_OK;
extern const HitRect BTN_CANCEL;
extern const HitRect BTN_CONFIRM;

/* --- Drawing functions --- */

void ui_draw_clock(C2D_TextBuf buf, int h, int m, int s);
void ui_draw_first_boot(C2D_TextBuf buf, bool show_ok);
void ui_draw_main(C2D_TextBuf buf);
void ui_draw_edit(C2D_TextBuf buf, int h, int m, int s);
void ui_draw_modal_confirm(C2D_TextBuf buf);
void ui_draw_modal_success(C2D_TextBuf buf);
