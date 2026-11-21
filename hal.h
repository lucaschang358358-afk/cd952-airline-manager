#ifndef HAL_H
#define HAL_H

#include <stdint.h>
#include <stddef.h>

typedef enum {
    AKEY_NONE = 0,
    AKEY_UP,
    AKEY_DOWN,
    AKEY_LEFT,
    AKEY_RIGHT,
    AKEY_PGUP,
    AKEY_PGDN,
    AKEY_OK,
    AKEY_SPACE,
    AKEY_BACK,
    AKEY_BACKSPACE,
    AKEY_TAB1,
    AKEY_TAB2,
    AKEY_TAB3,
    AKEY_TAB4,
    AKEY_TAB5,
    AKEY_TAB6,
    AKEY_DEPART_ALL, /* D */
    AKEY_SPEED,      /* S */
    AKEY_CHAR
} AmKey;

int  hal_get_width(void);
int  hal_get_height(void);

void hal_clear(uint32_t rgb);
void hal_put_pixel(int x, int y, uint32_t rgb);
void hal_fill_rect(int x, int y, int w, int h, uint32_t rgb);
void hal_draw_rect(int x, int y, int w, int h, uint32_t rgb);
void hal_fill_circle(int cx, int cy, int r, uint32_t rgb);
void hal_draw_circle(int cx, int cy, int r, uint32_t rgb);
void hal_draw_line(int x0, int y0, int x1, int y1, uint32_t rgb);

void hal_set_text_bg(uint32_t rgb);
void hal_draw_line_utf8(int x, int y, const char *utf8_text, int byte_len, int max_px_w, uint32_t rgb);
void hal_clear_text_in_rect(int rx, int ry, int rw, int rh);
void hal_dim_rect(int rx, int ry, int rw, int rh);
void hal_present(void);

int64_t hal_get_unix_time(void);
uint32_t hal_get_seed(void);
const char *hal_get_save_path(void);
const char *hal_get_extra_airports_csv(void);
void hal_request_quit(void);

#endif
