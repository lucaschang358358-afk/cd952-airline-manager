#ifndef AM_UI_H
#define AM_UI_H

#include "hal.h"

void am_ui_init(void);
void am_ui_shutdown(void);
void am_ui_on_key(AmKey key, char raw_char);
void am_ui_on_touch(int tx, int ty);
void am_ui_tick_second(void);
int  am_ui_needs_redraw(void);
void am_ui_render(void);

#endif
