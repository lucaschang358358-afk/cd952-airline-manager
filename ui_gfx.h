#ifndef UI_GFX_H
#define UI_GFX_H

#include <stdint.h>
#include "hal.h"

uint32_t ui_blend(uint32_t a, uint32_t b, int pct_b);
void ui_fill_rrect(int x, int y, int w, int h, int r, uint32_t rgb);
void ui_draw_rrect(int x, int y, int w, int h, int r, uint32_t rgb);
void ui_card(int x, int y, int w, int h, int r, uint32_t bg, uint32_t border, uint32_t shadow, int selected, uint32_t accent);
void ui_progress_bar(int x, int y, int w, int h, int pct, uint32_t track, uint32_t fill);
void ui_draw_ascii(int x, int y, const char *str, int scale, uint32_t color);
int  ui_ascii_width(const char *str, int scale);
void ui_draw_badge(int x, int y, const char *tag, uint32_t bg, uint32_t fg);
void ui_draw_str(int x, int y, const char *utf8, int max_w, uint32_t rgb);

/* 明亮日間航空世界地圖 (天藍海洋 + 自然綠/沙黃陸地，完全對標 AM4 官網!) */
void ui_draw_daylight_sky_clouds(int w, int h);
void ui_draw_world_map_daylight(int mx, int my, int mw, int mh);
void ui_draw_world_map_hd(int mx, int my, int mw, int mh, int zoom, int pan_x, int pan_y);
int  ui_latlon_to_xy_view(int lat10, int lon10, int mx, int my, int mw, int mh, int zoom, int pan_x, int pan_y, int *ox, int *oy);
void ui_draw_route_arc_view(int lat1, int lon1, int lat2, int lon2, int mx, int my, int mw, int mh, int zoom, int pan_x, int pan_y, int progress_pct, int is_return_leg, uint32_t arc_col, uint32_t plane_col);
void ui_latlon_to_xy(int lat10, int lon10, int mx, int my, int mw, int mh, int *ox, int *oy);
void ui_draw_route_arc(int lat1, int lon1, int lat2, int lon2, int mx, int my, int mw, int mh,
                       int progress_pct, int is_return_leg, uint32_t arc_col, uint32_t plane_col);

/* 亮白底折線走勢圖 (油價 / CO2 / 股價) */
void ui_draw_sparkline_light(int x, int y, int w, int h, const int *vals, int count, uint32_t line_col);

/* 明亮展示卡機型剪影 */
void ui_draw_aircraft_card_art(int cx, int cy, int w, int h, int sil_type, uint32_t tail_col);

#endif
