#include "ui_gfx.h"
#include <stdlib.h>
#include <string.h>

static const uint8_t FONT5X7[][5] = {
    {0x3E,0x51,0x49,0x45,0x3E}, {0x00,0x42,0x7F,0x40,0x00},
    {0x42,0x61,0x51,0x49,0x46}, {0x21,0x41,0x45,0x4B,0x31},
    {0x18,0x14,0x12,0x7F,0x10}, {0x27,0x45,0x45,0x45,0x39},
    {0x3C,0x4A,0x49,0x49,0x30}, {0x01,0x71,0x09,0x05,0x03},
    {0x36,0x49,0x49,0x49,0x36}, {0x06,0x49,0x49,0x29,0x1E},
    {0x7E,0x11,0x11,0x11,0x7E}, {0x7F,0x49,0x49,0x49,0x36},
    {0x3E,0x41,0x41,0x41,0x22}, {0x7F,0x41,0x41,0x22,0x1C},
    {0x7F,0x49,0x49,0x49,0x41}, {0x7F,0x09,0x09,0x09,0x01},
    {0x3E,0x41,0x49,0x49,0x7A}, {0x7F,0x08,0x08,0x08,0x7F},
    {0x00,0x41,0x7F,0x41,0x00}, {0x20,0x40,0x41,0x3F,0x01},
    {0x7F,0x08,0x14,0x22,0x41}, {0x7F,0x40,0x40,0x40,0x40},
    {0x7F,0x02,0x0C,0x02,0x7F}, {0x7F,0x04,0x08,0x10,0x7F},
    {0x3E,0x41,0x41,0x41,0x3E}, {0x7F,0x09,0x09,0x09,0x06},
    {0x3E,0x41,0x51,0x21,0x5E}, {0x7F,0x09,0x19,0x29,0x46},
    {0x46,0x49,0x49,0x49,0x31}, {0x01,0x01,0x7F,0x01,0x01},
    {0x3F,0x40,0x40,0x40,0x3F}, {0x1F,0x20,0x40,0x20,0x1F},
    {0x3F,0x40,0x38,0x40,0x3F}, {0x63,0x14,0x08,0x14,0x63},
    {0x07,0x08,0x70,0x08,0x07}, {0x61,0x51,0x49,0x45,0x43},
    {0x23,0x13,0x08,0x64,0x62}, {0x20,0x10,0x08,0x04,0x02},
    {0x08,0x08,0x3E,0x08,0x08}, {0x08,0x08,0x08,0x08,0x08},
    {0x00,0x36,0x36,0x00,0x00}, {0x00,0x60,0x60,0x00,0x00},
    {0x24,0x2A,0x7F,0x2A,0x12}, {0x00,0x00,0x5F,0x00,0x00}
};

static int aidx(char ch) {
    if (ch >= '0' && ch <= '9') return ch - '0';
    if (ch >= 'A' && ch <= 'Z') return 10 + (ch - 'A');
    if (ch >= 'a' && ch <= 'z') return 10 + (ch - 'a');
    switch (ch) {
        case '%': return 36; case '/': return 37; case '+': return 38;
        case '-': return 39; case ':': return 40; case '.': return 41;
        case '$': return 42; case '!': return 43; default: return -1;
    }
}

uint32_t ui_blend(uint32_t a, uint32_t b, int pct_b) {
    if (pct_b <= 0) return a;
    if (pct_b >= 100) return b;
    int pa = 100 - pct_b;
    int r  = ((int)((a >> 16) & 0xFF) * pa + (int)((b >> 16) & 0xFF) * pct_b) / 100;
    int g  = ((int)((a >> 8) & 0xFF)  * pa + (int)((b >> 8) & 0xFF)  * pct_b) / 100;
    int bl = ((int)(a & 0xFF)         * pa + (int)(b & 0xFF)         * pct_b) / 100;
    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)bl;
}

void ui_fill_rrect(int x, int y, int w, int h, int r, uint32_t rgb) {
    if (w <= 0 || h <= 0) return;
    if (r <= 1) { hal_fill_rect(x, y, w, h, rgb); return; }
    if (r * 2 > w) r = w / 2;
    if (r * 2 > h) r = h / 2;
    hal_fill_rect(x, y + r, w, h - r * 2, rgb);
    int r2 = r * r;
    for (int dy = 0; dy < r; dy++) {
        int cy = r - 1 - dy, dx = 0;
        while ((dx + 1) * (dx + 1) + cy * cy <= r2) dx++;
        int inset = r - dx; if (inset < 0) inset = 0;
        hal_fill_rect(x + inset, y + dy, w - inset * 2, 1, rgb);
        hal_fill_rect(x + inset, y + h - 1 - dy, w - inset * 2, 1, rgb);
    }
}

void ui_draw_rrect(int x, int y, int w, int h, int r, uint32_t rgb) {
    if (w <= 2 || h <= 2) return;
    if (r <= 1) { hal_draw_rect(x, y, w, h, rgb); return; }
    if (r * 2 > w) r = w / 2;
    if (r * 2 > h) r = h / 2;
    hal_fill_rect(x + r, y, w - r * 2, 1, rgb);
    hal_fill_rect(x + r, y + h - 1, w - r * 2, 1, rgb);
    hal_fill_rect(x, y + r, 1, h - r * 2, rgb);
    hal_fill_rect(x + w - 1, y + r, 1, h - r * 2, rgb);
    int r2 = r * r;
    for (int dy = 0; dy < r; dy++) {
        int cy = r - 1 - dy, dx = 0;
        while ((dx + 1) * (dx + 1) + cy * cy <= r2) dx++;
        int inset = r - dx;
        hal_put_pixel(x + inset, y + dy, rgb);
        hal_put_pixel(x + w - 1 - inset, y + dy, rgb);
        hal_put_pixel(x + inset, y + h - 1 - dy, rgb);
        hal_put_pixel(x + w - 1 - inset, y + h - 1 - dy, rgb);
    }
}

void ui_card(int x, int y, int w, int h, int r, uint32_t bg, uint32_t border, uint32_t shadow, int selected, uint32_t accent) {
    ui_fill_rrect(x + 2, y + 3, w, h, r, shadow);
    ui_fill_rrect(x, y, w, h, r, bg);
    if (selected) {
        ui_draw_rrect(x, y, w, h, r, accent);
        ui_draw_rrect(x + 1, y + 1, w - 2, h - 2, r > 1 ? r - 1 : 0, accent);
    } else {
        ui_draw_rrect(x, y, w, h, r, border);
    }
}

void ui_progress_bar(int x, int y, int w, int h, int pct, uint32_t track, uint32_t fill) {
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    int r = h / 2;
    ui_fill_rrect(x, y, w, h, r, track);
    int fw = (w * pct) / 100;
    if (fw > 0) {
        if (fw < h) fw = h;
        if (fw > w) fw = w;
        ui_fill_rrect(x, y, fw, h, r, fill);
    }
}

void ui_draw_ascii(int x, int y, const char *str, int scale, uint32_t color) {
    if (!str || scale < 1) return;
    int cx = x;
    for (int i = 0; str[i]; i++) {
        int id = aidx(str[i]);
        if (id >= 0) {
            for (int c = 0; c < 5; c++) {
                uint8_t bits = FONT5X7[id][c];
                for (int r = 0; r < 7; r++)
                    if (bits & (1u << r)) hal_fill_rect(cx + c * scale, y + r * scale, scale, scale, color);
            }
        }
        cx += 6 * scale;
    }
}

int ui_ascii_width(const char *str, int scale) {
    if (!str) return 0;
    int n = (int)strlen(str);
    return n > 0 ? (n * 6 * scale - scale) : 0;
}

void ui_draw_badge(int x, int y, const char *tag, uint32_t bg, uint32_t fg) {
    int tw = ui_ascii_width(tag, 1);
    ui_fill_rrect(x, y, tw + 10, 15, 5, bg);
    ui_draw_ascii(x + 5, y + 4, tag, 1, fg);
}

void ui_draw_str(int x, int y, const char *utf8, int max_w, uint32_t rgb) {
    if (utf8 && utf8[0]) hal_draw_line_utf8(x, y, utf8, (int)strlen(utf8), max_w, rgb);
}

/* 等距圓柱投影 (lon: -1800..1800, lat: 850..-600) */
void ui_latlon_to_xy(int lat10, int lon10, int mx, int my, int mw, int mh, int *ox, int *oy) {
    int x = mx + ((lon10 + 1800) * mw) / 3600;
    int y = my + ((820 - lat10) * mh) / 1420;
    if (x < mx) x = mx;
    if (x >= mx + mw) x = mx + mw - 1;
    if (y < my) y = my;
    if (y >= my + mh) y = my + mh - 1;
    *ox = x; *oy = y;
}

static void fill_poly_latlon(const int16_t *pts, int n, int mx, int my, int mw, int mh, uint32_t fill, uint32_t edge) {
    int px[24], py[24];
    if (n > 24) n = 24;
    int min_y = my + mh, max_y = my;
    for (int i = 0; i < n; i++) {
        ui_latlon_to_xy(pts[i * 2], pts[i * 2 + 1], mx, my, mw, mh, &px[i], &py[i]);
        if (py[i] < min_y) min_y = py[i];
        if (py[i] > max_y) max_y = py[i];
    }
    for (int y = min_y; y <= max_y; y++) {
        int xs[24], cnt = 0;
        for (int i = 0; i < n; i++) {
            int j = (i + 1) % n;
            int y0 = py[i], y1 = py[j];
            if ((y0 <= y && y1 > y) || (y1 <= y && y0 > y)) {
                int ix = px[i] + ((y - y0) * (px[j] - px[i])) / (y1 - y0);
                if (cnt < 24) xs[cnt++] = ix;
            }
        }
        for (int a = 0; a < cnt; a++)
            for (int b = a + 1; b < cnt; b++)
                if (xs[b] < xs[a]) { int t = xs[a]; xs[a] = xs[b]; xs[b] = t; }
        for (int a = 0; a + 1 < cnt; a += 2) {
            if (xs[a + 1] > xs[a]) hal_fill_rect(xs[a], y, xs[a + 1] - xs[a], 1, fill);
        }
    }
    for (int i = 0; i < n; i++) {
        int j = (i + 1) % n;
        hal_draw_line(px[i], py[i], px[j], py[j], edge);
    }
}

void ui_draw_world_map(int mx, int my, int mw, int mh) {
    /* 1. 深海雷達底色與經緯網格 */
    hal_fill_rect(mx, my, mw, mh, 0x081424);
    for (int lon = -1500; lon <= 1500; lon += 300) {
        int x, y; ui_latlon_to_xy(0, lon, mx, my, mw, mh, &x, &y);
        hal_fill_rect(x, my, 1, mh, 0x0F233A);
    }
    for (int lat = -300; lat <= 600; lat += 300) {
        int x, y; ui_latlon_to_xy(lat, 0, mx, my, mw, mh, &x, &y);
        hal_fill_rect(mx, y, mw, 1, (lat == 0) ? 0x163554 : 0x0F233A);
    }

    /* 2. 七大洲精緻簡化多邊形輪廓 (lat*10, lon*10) */
    static const int16_t NAMERICA[] = {
        700,-1650, 720,-1300, 680,-900, 580,-620, 450,-600, 300,-810, 250,-800,
        290,-950, 200,-970, 90,-790, 150,-930, 230,-1060, 320,-1170, 490,-1250, 600,-1450
    };
    static const int16_t GREENLAND[] = { 820,-500, 810,-200, 680,-250, 600,-450, 700,-550 };
    static const int16_t SAMERICA[] = {
        110,-750, 100,-620, -50,-350, -230,-430, -350,-560, -540,-680,
        -520,-740, -330,-720, -180,-700, -50,-810, 50,-770
    };
    static const int16_t EUROPE[] = {
        700,250, 680,420, 550,450, 460,380, 400,260, 370,150, 430,100,
        360,-60, 430,-90, 490,-40, 580,50, 650,120
    };
    static const int16_t UK[] = { 580,-60, 580,0, 500,15, 500,-55 };
    static const int16_t AFRICA[] = {
        360,-60, 370,100, 310,320, 120,510, -20,410, -260,330, -350,200,
        -220,140, -50,120, 50,20, 50,-100, 150,-170, 280,-130
    };
    static const int16_t ASIA[] = {
        720,600, 720,1350, 660,1700, 550,1600, 440,1450, 380,1280, 310,1220,
        220,1140, 100,1090, 10,1040, 160,960, 220,880, 80,770, 240,670,
        260,500, 130,440, 310,340, 410,290, 460,380, 550,450, 680,450
    };
    static const int16_t JAPAN[] = { 440,1450, 410,1410, 350,1400, 310,1300, 340,1310, 400,1400 };
    static const int16_t SEASIA[] = { 60,960, -60,1060, -80,1160, -40,1200, 40,1170, 20,1040 };
    static const int16_t AUSTRALIA[] = {
        -120,1310, -110,1420, -200,1490, -280,1535, -380,1480, -350,1360,
        -340,1150, -220,1140, -160,1240
    };
    static const int16_t NZ[] = { -350,1730, -380,1780, -460,1680, -430,1700 };

    uint32_t land = 0x16304B, border = 0x29527A;
    fill_poly_latlon(NAMERICA, 15, mx, my, mw, mh, land, border);
    fill_poly_latlon(GREENLAND, 5, mx, my, mw, mh, land, border);
    fill_poly_latlon(SAMERICA, 11, mx, my, mw, mh, land, border);
    fill_poly_latlon(EUROPE,   12, mx, my, mw, mh, land, border);
    fill_poly_latlon(UK,        4, mx, my, mw, mh, land, border);
    fill_poly_latlon(AFRICA,   13, mx, my, mw, mh, land, border);
    fill_poly_latlon(ASIA,     21, mx, my, mw, mh, land, border);
    fill_poly_latlon(JAPAN,     6, mx, my, mw, mh, land, border);
    fill_poly_latlon(SEASIA,    6, mx, my, mw, mh, land, border);
    fill_poly_latlon(AUSTRALIA, 9, mx, my, mw, mh, land, border);
    fill_poly_latlon(NZ,        4, mx, my, mw, mh, land, border);

    hal_draw_rect(mx, my, mw, mh, 0x1E3A5F);
}

/* 繪製優美大圓弧線航路 + 即時移動飛機光點 */
void ui_draw_route_arc(int lat1, int lon1, int lat2, int lon2, int mx, int my, int mw, int mh,
                       int progress_pct, uint32_t arc_col, uint32_t plane_col) {
    int x0, y0, x1, y1;
    ui_latlon_to_xy(lat1, lon1, mx, my, mw, mh, &x0, &y0);
    ui_latlon_to_xy(lat2, lon2, mx, my, mw, mh, &x1, &y1);

    int dx = x1 - x0;
    int lift = abs(dx) / 5;
    if (lift > 42) lift = 42;
    /* 貝茲二次曲線近似極地大圓弧線 */
    int cx = (x0 + x1) / 2;
    int cy = (y0 + y1) / 2 - lift;
    if (cy < my + 4) cy = my + 4;

    int prev_x = x0, prev_y = y0;
    int px_plane = x0, py_plane = y0;
    const int STEPS = 12;
    for (int i = 1; i <= STEPS; i++) {
        int t = (i * 100) / STEPS;
        int it = 100 - t;
        int bx = (it * it * x0 + 2 * it * t * cx + t * t * x1) / 10000;
        int by = (it * it * y0 + 2 * it * t * cy + t * t * y1) / 10000;
        hal_draw_line(prev_x, prev_y, bx, by, arc_col);
        prev_x = bx; prev_y = by;
    }

    if (progress_pct >= 0 && progress_pct <= 100) {
        int t = progress_pct, it = 100 - t;
        px_plane = (it * it * x0 + 2 * it * t * cx + t * t * x1) / 10000;
        py_plane = (it * it * y0 + 2 * it * t * cy + t * t * y1) / 10000;
        hal_fill_circle(px_plane, py_plane, 4, plane_col);
        hal_fill_circle(px_plane, py_plane, 2, 0xFFFFFF);
    }
}

void ui_draw_sparkline(int x, int y, int w, int h, const int *vals, int count, uint32_t line_col, uint32_t grid_col) {
    ui_fill_rrect(x, y, w, h, 8, 0x0B1320);
    ui_draw_rrect(x, y, w, h, 8, grid_col);
    if (!vals || count < 2) return;

    int vmin = vals[0], vmax = vals[0];
    for (int i = 1; i < count; i++) {
        if (vals[i] < vmin) vmin = vals[i];
        if (vals[i] > vmax) vmax = vals[i];
    }
    if (vmax == vmin) vmax = vmin + 10;

    hal_fill_rect(x + 8, y + h / 2, w - 16, 1, 0x16263D);

    int px = -1, py = -1;
    for (int i = 0; i < count; i++) {
        int cx = x + 10 + (i * (w - 20)) / (count - 1);
        int cy = y + h - 10 - ((vals[i] - vmin) * (h - 20)) / (vmax - vmin);
        if (px >= 0) {
            hal_draw_line(px, py, cx, cy, line_col);
            hal_draw_line(px, py + 1, cx, cy + 1, line_col);
        }
        px = cx; py = cy;
    }
    if (px >= 0) hal_fill_circle(px, py, 3, 0xFFFFFF);
}
