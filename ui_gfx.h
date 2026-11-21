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

/* 繪製暗黑航空雷達世界地圖 (經緯度投影至地圖矩形) */
void ui_draw_world_map(int mx, int my, int mw, int mh);
void ui_latlon_to_xy(int lat10, int lon10, int mx, int my, int mw, int mh, int *ox, int *oy);
void ui_draw_route_arc(int lat1, int lon1, int lat2, int lon2, int mx, int my, int mw, int mh,
                       int progress_pct, uint32_t arc_col, uint32_t plane_col);

/* 繪製迷你折線走勢圖 (油價 / CO2 / 股價) */
void ui_draw_sparkline(int x, int y, int w, int h, const int *vals, int count, uint32_t line_col, uint32_t grid_col);

void ui_draw_aircraft_silhouette(int cx, int cy, int w, int h, int sil_type, uint32_t body, uint32_t accent);
#endif
