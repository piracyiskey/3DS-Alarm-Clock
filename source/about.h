#pragma once

#include <3ds.h>
#include <citro2d.h>

/* Bottom screen About & Credits renderer */
void about_draw_bottom(C2D_TextBuf buf, float scroll_y);

/* Calculate maximum vertical scroll offset */
float about_get_max_scroll(void);
