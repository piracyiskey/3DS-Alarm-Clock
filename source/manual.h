#pragma once

#include <3ds.h>
#include <citro2d.h>
#include "ui.h"

#define MANUAL_PAGE_COUNT 2

/* Header navigation buttons */
extern const HitRect BTN_MANUAL_BACK;
extern const HitRect BTN_MANUAL_PREV;
extern const HitRect BTN_MANUAL_NEXT;

/* Bottom screen manual reader renderer */
void manual_draw_bottom(C2D_TextBuf buf, int page_idx, float scroll_y);

/* Calculate maximum vertical scroll offset for the given page */
float manual_get_max_scroll(int page_idx);

/* Page display titles */
const char* manual_get_page_title(int page_idx);
