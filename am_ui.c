#include "am_ui.h"
#include "am_data.h"
#include "am_sim.h"
#include "ui_gfx.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum {
    SCR_TITLE = 0,
    SCR_CREATE,
    SCR_MAIN,
    SCR_ROUTE_MODAL,
    SCR_PLANE_MODAL,
    SCR_PAUSE
} AmScreen;

typedef enum {
    TAB_RADAR = 0,
    TAB_FLEET,
    TAB_SHOP,
    TAB_FUEL,
    TAB_OPS,
    TAB_FINANCE
} MainTab;

static AmScreen g_scr = SCR_TITLE;
static MainTab  g_tab = TAB_RADAR;
static int g_redraw = 1;
static int g_title_sel = 0;
static int g_pause_sel = 0;

/* 創立航空公司精靈狀態 (支援自訂名稱與 2 碼 IATA) */
static char g_new_name[28] = "Starlux Sim";
static char g_new_iata[4]  = "SX";
static int  g_new_realism  = 0;
static int  g_new_hub_idx  = 0;
static int  g_create_row   = 0; /* 0=Name, 1=IATA, 2=Mode, 3=Hub, 4=Launch */

/* 各分頁選取與捲動狀態 */
static int g_fleet_sel = 0, g_fleet_scroll = 0;
static int g_shop_mfg  = 0; /* 0=All, 1..6=mfg+1 */
static int g_shop_sel  = 0, g_shop_scroll = 0;
static int g_ops_hub_ap = 42; /* 預設顯示 SYD 或可切換任意機場 */
static int g_stock_sel = 0, g_stock_scroll = 0;

/* 航線規劃彈窗狀態 */
static int g_rt_fleet_idx = 0;
static int g_rt_orig_hub_slot = 0;
static int g_rt_dest_ap = 5;   /* 預設 HND */
static int g_rt_stop_ap = -1;  /* -1=直飛 */
static int g_rt_row = 0;       /* 0=Orig, 1=Dest, 2=Stop, 3=Confirm */
static char g_rt_search[8] = {0};

/* 飛機詳情與改裝彈窗 */
static int g_pl_fleet_idx = 0;
static int g_pl_sel = 0;

static char g_toast[96] = {0};
static int  g_toast_sec = 0;

static void set_toast(const char *s) {
    snprintf(g_toast, sizeof(g_toast), "%s", s ? s : "");
    g_toast_sec = 5;
}

static void fmt_money(int64_t val, char *out, size_t sz) {
    int neg = (val < 0);
    int64_t a = neg ? -val : val;
    if (a >= 1000000000LL) snprintf(out, sz, "%s$%.2fB", neg ? "-" : "", (double)a / 1e9);
    else if (a >= 1000000LL) snprintf(out, sz, "%s$%.2fM", neg ? "-" : "", (double)a / 1e6);
    else if (a >= 1000LL) snprintf(out, sz, "%s$%.1fK", neg ? "-" : "", (double)a / 1e3);
    else snprintf(out, sz, "%s$%lld", neg ? "-" : "", (long long)a);
}

static int has_save_file(void) {
    FILE *f = fopen(hal_get_save_path(), "rb");
    if (!f) return 0;
    uint32_t m = 0;
    int ok = (fread(&m, sizeof(m), 1, f) == 1 && m == AM_SAVE_MAGIC);
    fclose(f);
    return ok;
}

static int build_shop_list(int *out_idxs, int max_n) {
    int n = am_aircraft_count(), cnt = 0;
    for (int i = 0; i < n && cnt < max_n; i++) {
        const AircraftModel *m = am_aircraft_get(i);
        if (g_shop_mfg == 0 || (int)m->mfg == (g_shop_mfg - 1)) out_idxs[cnt++] = i;
    }
    return cnt;
}

void am_ui_init(void) {
    am_sim_init();
    g_scr = SCR_TITLE;
    g_tab = TAB_RADAR;
    g_title_sel = 0;
    int tpe = am_airport_find_iata("TPE");
    g_new_hub_idx = (tpe >= 0) ? tpe : 0;
    int syd = am_airport_find_iata("SYD");
    g_ops_hub_ap = (syd >= 0) ? syd : 0;
    g_redraw = 1;
}

void am_ui_shutdown(void) { am_sim_save(); }
int  am_ui_needs_redraw(void) { return g_redraw; }

void am_ui_tick_second(void) {
    if (g_toast_sec > 0) {
        g_toast_sec--;
        if (g_toast_sec == 0) { g_toast[0] = '\0'; g_redraw = 1; }
    }
    if (g_scr == SCR_MAIN || g_scr == SCR_ROUTE_MODAL || g_scr == SCR_PLANE_MODAL) {
        am_sim_tick(1, 1);
        g_redraw = 1;
    }
}

static void open_route_modal(int fleet_idx) {
    if (fleet_idx < 0 || fleet_idx >= g_am.fleet_count) return;
    g_rt_fleet_idx = fleet_idx;
    g_rt_orig_hub_slot = 0;
    g_rt_dest_ap = (g_am.fleet[fleet_idx].has_route && g_am.fleet[fleet_idx].dest_ap >= 0) ? g_am.fleet[fleet_idx].dest_ap : ((g_am.hubs[0] + 5) % am_airport_count());
    g_rt_stop_ap = g_am.fleet[fleet_idx].stopover_ap;
    g_rt_row = 1;
    g_rt_search[0] = '\0';
    g_scr = SCR_ROUTE_MODAL;
}

static void open_plane_modal(int fleet_idx) {
    if (fleet_idx < 0 || fleet_idx >= g_am.fleet_count) return;
    g_pl_fleet_idx = fleet_idx;
    g_pl_sel = 0;
    g_scr = SCR_PLANE_MODAL;
}

/* ============================================================
 * 按鍵事件
 * ============================================================ */
void am_ui_on_key(AmKey key, char raw_char) {
    g_redraw = 1;
    int is_ok = (key == AKEY_OK || key == AKEY_SPACE);

    if (g_scr == SCR_TITLE) {
        int hs = has_save_file(), n = hs ? 3 : 2;
        if (key == AKEY_UP) g_title_sel = (g_title_sel + n - 1) % n;
        else if (key == AKEY_DOWN) g_title_sel = (g_title_sel + 1) % n;
        else if (key == AKEY_BACK) hal_request_quit();
        else if (is_ok) {
            if (hs && g_title_sel == 0) { if (am_sim_load()) { g_scr = SCR_MAIN; g_tab = TAB_RADAR; } }
            else if (g_title_sel == (hs ? 1 : 0)) { g_create_row = 0; g_scr = SCR_CREATE; }
            else hal_request_quit();
        }
        return;
    }

    if (g_scr == SCR_CREATE) {
        if (key == AKEY_BACK) { g_scr = SCR_TITLE; return; }
        if (key == AKEY_UP) { g_create_row = (g_create_row + 4) % 5; return; }
        if (key == AKEY_DOWN) { g_create_row = (g_create_row + 1) % 5; return; }

        if (g_create_row == 0) { /* 自訂航空公司名稱打字 */
            if (key == AKEY_BACKSPACE) { size_t n = strlen(g_new_name); if (n > 0) g_new_name[n - 1] = '\0'; }
            else if (key == AKEY_OK) g_create_row = 1;
            else if (raw_char >= 0x20 && raw_char < 0x7F) {
                size_t n = strlen(g_new_name);
                if (n + 1 < sizeof(g_new_name)) { g_new_name[n] = raw_char; g_new_name[n + 1] = '\0'; }
            }
            return;
        }
        if (g_create_row == 1) { /* 自訂 2 碼 IATA 代碼 */
            if (key == AKEY_BACKSPACE) { size_t n = strlen(g_new_iata); if (n > 0) g_new_iata[n - 1] = '\0'; }
            else if (key == AKEY_OK) g_create_row = 2;
            else if (isalnum((unsigned char)raw_char)) {
                size_t n = strlen(g_new_iata);
                if (n < 2) { g_new_iata[n] = (char)toupper((unsigned char)raw_char); g_new_iata[n + 1] = '\0'; }
            }
            return;
        }
        if (g_create_row == 2) {
            if (key == AKEY_LEFT || key == AKEY_RIGHT || is_ok) g_new_realism = 1 - g_new_realism;
            return;
        }
        if (g_create_row == 3) {
            int ac = am_airport_count();
            if (key == AKEY_LEFT) g_new_hub_idx = (g_new_hub_idx + ac - 1) % ac;
            else if (key == AKEY_RIGHT) g_new_hub_idx = (g_new_hub_idx + 1) % ac;
            else if (key == AKEY_PGUP) g_new_hub_idx = (g_new_hub_idx + ac - 15) % ac;
            else if (key == AKEY_PGDN) g_new_hub_idx = (g_new_hub_idx + 15) % ac;
            else if (is_ok) g_create_row = 4;
            return;
        }
        if (g_create_row == 4 && is_ok) {
            am_sim_new_company(g_new_name, g_new_iata, g_new_hub_idx, g_new_realism);
            g_scr = SCR_MAIN; g_tab = TAB_RADAR;
            set_toast(g_am.last_notice);
        }
        return;
    }

    if (g_scr == SCR_PAUSE) {
        if (key == AKEY_BACK) { g_scr = SCR_MAIN; return; }
        if (key == AKEY_UP) g_pause_sel = (g_pause_sel + 2) % 3;
        else if (key == AKEY_DOWN) g_pause_sel = (g_pause_sel + 1) % 3;
        else if (is_ok) {
            if (g_pause_sel == 0) g_scr = SCR_MAIN;
            else if (g_pause_sel == 1) { am_sim_save(); g_scr = SCR_TITLE; }
            else { am_sim_save(); hal_request_quit(); }
        }
        return;
    }

    if (g_scr == SCR_ROUTE_MODAL) {
        int ac = am_airport_count();
        if (key == AKEY_BACK) { g_scr = SCR_MAIN; return; }
        if (key == AKEY_UP) { g_rt_row = (g_rt_row + 3) % 4; return; }
        if (key == AKEY_DOWN) { g_rt_row = (g_rt_row + 1) % 4; return; }

        /* 支援直接鍵盤打 3 碼 IATA 秒速搜尋目的地機場！ */
        if (g_rt_row == 1 && isalpha((unsigned char)raw_char)) {
            size_t n = strlen(g_rt_search);
            if (n >= 3) { g_rt_search[0] = '\0'; n = 0; }
            g_rt_search[n] = (char)toupper((unsigned char)raw_char); g_rt_search[n + 1] = '\0';
            if (n + 1 == 3) {
                int f = am_airport_find_iata(g_rt_search);
                if (f >= 0) { g_rt_dest_ap = f; set_toast("已定位目的地機場！"); }
            }
            return;
        }
        if (g_rt_row == 1 && key == AKEY_BACKSPACE) { g_rt_search[0] = '\0'; return; }

        if (key == AKEY_LEFT || key == AKEY_RIGHT || key == AKEY_PGUP || key == AKEY_PGDN) {
            int step = (key == AKEY_PGUP || key == AKEY_PGDN) ? 15 : 1;
            int d = (key == AKEY_LEFT || key == AKEY_PGUP) ? -step : step;
            if (g_rt_row == 0 && g_am.hub_count > 0) g_rt_orig_hub_slot = (g_rt_orig_hub_slot + g_am.hub_count + (d > 0 ? 1 : -1)) % g_am.hub_count;
            else if (g_rt_row == 1) g_rt_dest_ap = (g_rt_dest_ap + ac + d) % ac;
            else if (g_rt_row == 2) {
                if (g_rt_stop_ap < 0) g_rt_stop_ap = (d > 0) ? 0 : (ac - 1);
                else { int nx = g_rt_stop_ap + d; g_rt_stop_ap = (nx < 0 || nx >= ac) ? -1 : nx; }
            }
            return;
        }
        if (is_ok) {
            char msg[96];
            int orig = g_am.hubs[g_rt_orig_hub_slot % g_am.hub_count];
            if (am_assign_route(g_rt_fleet_idx, orig, g_rt_dest_ap, g_rt_stop_ap, msg, sizeof(msg))) {
                set_toast(msg); g_scr = SCR_MAIN;
            } else set_toast(msg);
        }
        return;
    }

    if (g_scr == SCR_PLANE_MODAL) {
        if (key == AKEY_BACK) { g_scr = SCR_MAIN; return; }
        if (key == AKEY_UP) g_pl_sel = (g_pl_sel + 6) % 7;
        else if (key == AKEY_DOWN) g_pl_sel = (g_pl_sel + 1) % 7;
        else if (is_ok) {
            char msg[96];
            if (g_pl_sel == 0) { if (am_repair_plane(g_pl_fleet_idx, msg, sizeof(msg))) set_toast(msg); else set_toast(msg); }
            else if (g_pl_sel == 1) { if (am_acheck_plane(g_pl_fleet_idx, msg, sizeof(msg))) set_toast(msg); else set_toast(msg); }
            else if (g_pl_sel == 2) { am_buy_mod(g_pl_fleet_idx, 0, msg, sizeof(msg)); set_toast(msg); }
            else if (g_pl_sel == 3) { am_buy_mod(g_pl_fleet_idx, 1, msg, sizeof(msg)); set_toast(msg); }
            else if (g_pl_sel == 4) { am_buy_mod(g_pl_fleet_idx, 2, msg, sizeof(msg)); set_toast(msg); }
            else if (g_pl_sel == 5) {
                /* 一鍵切換高收益商務配置 / 經濟滿載配置 */
                FleetPlane *fp = &g_am.fleet[g_pl_fleet_idx];
                const AircraftModel *m = am_aircraft_get(fp->model_idx);
                if (!m->is_cargo) {
                    if (fp->seats_f == 0 && m->cap >= 120) {
                        fp->seats_f = m->cap / 12; fp->seats_j = m->cap / 6;
                        fp->seats_y = m->cap - fp->seats_j * 2 - fp->seats_f * 3;
                        set_toast("已切換為【豪華頭等商務艙配置】！");
                    } else {
                        fp->seats_f = 0; fp->seats_j = m->cap / 12;
                        fp->seats_y = m->cap - fp->seats_j * 2;
                        set_toast("已切換為【高密度經濟艙配置】！");
                    }
                    am_sim_save();
                }
            } else if (g_pl_sel == 6) {
                if (am_sell_aircraft(g_pl_fleet_idx, msg, sizeof(msg))) { set_toast(msg); g_scr = SCR_MAIN; }
                else set_toast(msg);
            }
        }
        return;
    }

    /* 主介面 6 分頁快捷鍵 */
    if (key == AKEY_TAB1) { g_tab = TAB_RADAR; return; }
    if (key == AKEY_TAB2) { g_tab = TAB_FLEET; return; }
    if (key == AKEY_TAB3) { g_tab = TAB_SHOP; return; }
    if (key == AKEY_TAB4) { g_tab = TAB_FUEL; return; }
    if (key == AKEY_TAB5) { g_tab = TAB_OPS; return; }
    if (key == AKEY_TAB6) { g_tab = TAB_FINANCE; return; }
    if (key == AKEY_SPEED) { g_am.speed_mult_idx = (g_am.speed_mult_idx + 1) % 4; return; }
    if (key == AKEY_DEPART_ALL) { char m[96]; am_depart_all(m, sizeof(m)); set_toast(m); return; }
    if (key == AKEY_BACK) { g_pause_sel = 0; g_scr = SCR_PAUSE; return; }

    if (g_tab == TAB_FLEET && g_am.fleet_count > 0) {
        if (key == AKEY_UP && g_fleet_sel > 0) g_fleet_sel--;
        else if (key == AKEY_DOWN && g_fleet_sel + 1 < g_am.fleet_count) g_fleet_sel++;
        else if (key == AKEY_OK) {
            if (!g_am.fleet[g_fleet_sel].has_route) open_route_modal(g_fleet_sel);
            else { char m[96]; am_depart_plane(g_fleet_sel, m, sizeof(m)); set_toast(m); }
        } else if (key == AKEY_RIGHT) open_plane_modal(g_fleet_sel);
        else if (key == AKEY_LEFT) open_route_modal(g_fleet_sel);
    } else if (g_tab == TAB_SHOP) {
        int list[AM_MAX_AIRCRAFT], cnt = build_shop_list(list, AM_MAX_AIRCRAFT);
        if (key == AKEY_LEFT) { g_shop_mfg = (g_shop_mfg + 6) % 7; g_shop_sel = 0; g_shop_scroll = 0; }
        else if (key == AKEY_RIGHT) { g_shop_mfg = (g_shop_mfg + 1) % 7; g_shop_sel = 0; g_shop_scroll = 0; }
        else if (key == AKEY_UP && g_shop_sel > 0) g_shop_sel--;
        else if (key == AKEY_DOWN && g_shop_sel + 1 < cnt) g_shop_sel++;
        else if (key == AKEY_PGUP) g_shop_sel = (g_shop_sel >= 5) ? (g_shop_sel - 5) : 0;
        else if (key == AKEY_PGDN) { g_shop_sel += 5; if (g_shop_sel >= cnt) g_shop_sel = cnt - 1; }
        else if (is_ok && g_shop_sel < cnt) { char m[96]; am_buy_aircraft(list[g_shop_sel], g_am.hubs[0], m, sizeof(m)); set_toast(m); }
    } else if (g_tab == TAB_FUEL) {
        char m[96];
        if (key == AKEY_OK) { am_buy_fuel(500000LL, m, sizeof(m)); set_toast(m); }
        else if (key == AKEY_SPACE) { am_buy_co2(500000LL, m, sizeof(m)); set_toast(m); }
    } else if (g_tab == TAB_OPS) {
        int ac = am_airport_count();
        if (key == AKEY_LEFT) g_ops_hub_ap = (g_ops_hub_ap + ac - 1) % ac;
        else if (key == AKEY_RIGHT) g_ops_hub_ap = (g_ops_hub_ap + 1) % ac;
        else if (key == AKEY_UP && g_am.staff_salary_pct < 140) g_am.staff_salary_pct += 5;
        else if (key == AKEY_DOWN && g_am.staff_salary_pct > 80) g_am.staff_salary_pct -= 5;
        else if (is_ok) { char m[96]; am_buy_hub(g_ops_hub_ap, m, sizeof(m)); set_toast(m); }
    } else if (g_tab == TAB_FINANCE) {
        char m[96];
        if (key == AKEY_UP && g_stock_sel > 0) g_stock_sel--;
        else if (key == AKEY_DOWN && g_stock_sel + 1 < AM_NUM_RIVALS) g_stock_sel++;
        else if (key == AKEY_RIGHT || key == AKEY_OK) { am_trade_stock(g_stock_sel, 100, m, sizeof(m)); set_toast(m); }
        else if (key == AKEY_LEFT) { am_trade_stock(g_stock_sel, -100, m, sizeof(m)); set_toast(m); }
    }
}

/* ============================================================
 * 全畫面觸控互動
 * ============================================================ */
void am_ui_on_touch(int tx, int ty) {
    int sw = hal_get_width(), sh = hal_get_height();
    g_redraw = 1;

    if (g_scr == SCR_TITLE) {
        int hs = has_save_file(), n = hs ? 3 : 2;
        int pw = 460, px = (sw - pw) / 2, py = 60;
        for (int i = 0; i < n; i++) {
            int ry = py + 150 + i * 52;
            if (tx >= px + 56 && tx <= px + pw - 56 && ty >= ry && ty <= ry + 42) { g_title_sel = i; am_ui_on_key(AKEY_OK, 0); return; }
        }
        return;
    }

    if (g_scr == SCR_CREATE) {
        int pw = 560, px = (sw - pw) / 2, py = 26;
        /* 快速航空名稱預設按鈕 (y = py + 94..124) */
        if (ty >= py + 92 && ty <= py + 126) {
            const char *pn[4] = { "Starlux Sim", "Formosa Air", "Sydney Jet", "Pacific Sky" };
            const char *pi[4] = { "JX", "FM", "SY", "PS" };
            int idx = (tx - (px + 24)) / 128;
            if (idx >= 0 && idx < 4) { snprintf(g_new_name, sizeof(g_new_name), "%s", pn[idx]); snprintf(g_new_iata, sizeof(g_new_iata), "%s", pi[idx]); }
            return;
        }
        /* 熱門樞紐快捷按鈕 (y = py + 286..318) */
        if (ty >= py + 284 && ty <= py + 320) {
            const char *ph[6] = { "TPE", "SYD", "HND", "SIN", "LHR", "JFK" };
            int idx = (tx - (px + 24)) / 86;
            if (idx >= 0 && idx < 6) { int a = am_airport_find_iata(ph[idx]); if (a >= 0) g_new_hub_idx = a; }
            return;
        }
        if (ty >= py + 50 && ty < py + 90) g_create_row = 0;
        else if (ty >= py + 134 && ty < py + 174) g_create_row = 1;
        else if (ty >= py + 184 && ty < py + 224) { g_create_row = 2; g_new_realism = 1 - g_new_realism; }
        else if (ty >= py + 234 && ty < py + 276) {
            g_create_row = 3;
            am_ui_on_key(tx < sw / 2 ? AKEY_LEFT : AKEY_RIGHT, 0);
        } else if (ty >= py + 344 && ty <= py + 394) {
            am_sim_new_company(g_new_name, g_new_iata, g_new_hub_idx, g_new_realism);
            g_scr = SCR_MAIN; g_tab = TAB_RADAR; set_toast(g_am.last_notice);
        }
        return;
    }

    if (g_scr == SCR_PAUSE) {
        int pw = 340, ph = 220, px = (sw - pw) / 2, py = (sh - ph) / 2;
        if (tx < px || tx > px + pw || ty < py || ty > py + ph) { g_scr = SCR_MAIN; return; }
        int idx = (ty - (py + 56)) / 48;
        if (idx >= 0 && idx < 3) { g_pause_sel = idx; am_ui_on_key(AKEY_OK, 0); }
        return;
    }

    if (g_scr == SCR_ROUTE_MODAL) {
        int mw = 560, mh = 350, mx = (sw - mw) / 2, my = (sh - mh) / 2;
        if (tx < mx || tx > mx + mw || ty < my || ty > my + mh) { g_scr = SCR_MAIN; return; }
        if (ty >= my + 56 && ty <= my + 96) { g_rt_row = 0; am_ui_on_key(tx < sw / 2 ? AKEY_LEFT : AKEY_RIGHT, 0); }
        else if (ty >= my + 106 && ty <= my + 148) { g_rt_row = 1; am_ui_on_key(tx < sw / 2 ? AKEY_LEFT : AKEY_RIGHT, 0); }
        else if (ty >= my + 158 && ty <= my + 198) { g_rt_row = 2; am_ui_on_key(tx < sw / 2 ? AKEY_LEFT : AKEY_RIGHT, 0); }
        else if (ty >= my + 286) {
            if (tx < mx + mw / 2) am_ui_on_key(AKEY_OK, 0);
            else g_scr = SCR_MAIN;
        }
        return;
    }

    if (g_scr == SCR_PLANE_MODAL) {
        int mw = 480, mh = 380, mx = (sw - mw) / 2, my = (sh - mh) / 2;
        if (tx < mx || tx > mx + mw || ty < my || ty > my + mh) { g_scr = SCR_MAIN; return; }
        int idx = (ty - (my + 66)) / 42;
        if (idx >= 0 && idx < 7) { g_pl_sel = idx; am_ui_on_key(AKEY_OK, 0); }
        return;
    }

    /* 主介面左側 6 分頁導覽列 (x < 84) */
    if (tx < 84) {
        for (int i = 0; i < 6; i++) {
            int y0 = 58 + i * 64;
            if (ty >= y0 && ty <= y0 + 54) { g_tab = (MainTab)i; return; }
        }
        return;
    }

    /* 頂部狀態列：倍速按鈕 & 一鍵全機起飛 Depart All */
    if (ty <= 42) {
        if (tx >= sw - 220 && tx < sw - 122) { g_am.speed_mult_idx = (g_am.speed_mult_idx + 1) % 4; return; }
        if (tx >= sw - 118) { char m[96]; am_depart_all(m, sizeof(m)); set_toast(m); return; }
        return;
    }

    int mx = 96, mw = sw - mx - 14;
    if (g_tab == TAB_FLEET) {
        int top_y = 56, row_h = 84;
        if (ty >= top_y && ty < sh - 38) {
            int r = (ty - top_y) / row_h, idx = g_fleet_scroll + r;
            if (idx >= 0 && idx < g_am.fleet_count && r < 4) {
                g_fleet_sel = idx;
                if (tx >= mx + mw - 72) open_plane_modal(idx);
                else if (tx >= mx + mw - 144) open_route_modal(idx);
                else if (tx >= mx + mw - 216) { char m[96]; am_depart_plane(idx, m, sizeof(m)); set_toast(m); }
            }
        } else if (ty >= sh - 38) {
            if (tx < mx + 140 && g_fleet_scroll > 0) g_fleet_scroll--;
            else if (tx >= sw - 150 && g_fleet_scroll + 4 < g_am.fleet_count) g_fleet_scroll++;
        }
        return;
    }

    if (g_tab == TAB_SHOP) {
        if (ty >= 52 && ty <= 82) {
            int c = (tx - mx) / 96;
            if (c >= 0 && c < 7) { g_shop_mfg = c; g_shop_sel = 0; g_shop_scroll = 0; }
            return;
        }
        int list[AM_MAX_AIRCRAFT], cnt = build_shop_list(list, AM_MAX_AIRCRAFT);
        int top_y = 92, row_h = 82;
        if (ty >= top_y && ty < sh - 38) {
            int r = (ty - top_y) / row_h, idx = g_shop_scroll + r;
            if (idx >= 0 && idx < cnt && r < 4) {
                g_shop_sel = idx;
                if (tx >= mx + mw - 96) { char m[96]; am_buy_aircraft(list[idx], g_am.hubs[0], m, sizeof(m)); set_toast(m); }
            }
        } else if (ty >= sh - 38) {
            if (tx < mx + 140) am_ui_on_key(AKEY_PGUP, 0);
            else if (tx >= sw - 150) am_ui_on_key(AKEY_PGDN, 0);
        }
        return;
    }

    if (g_tab == TAB_FUEL) {
        char m[96];
        int half = mx + mw / 2;
        if (ty >= 244 && ty <= 284) {
            if (tx < half) { am_buy_fuel(500000LL, m, sizeof(m)); set_toast(m); }
            else { am_buy_co2(500000LL, m, sizeof(m)); set_toast(m); }
            return;
        }
        if (ty >= 292 && ty <= 328) {
            if (tx < half) { am_upgrade_fuel_tank(m, sizeof(m)); set_toast(m); }
            else { am_upgrade_co2_tank(m, sizeof(m)); set_toast(m); }
            return;
        }
        if (ty >= 364 && ty <= 416) {
            int c = (tx - mx) / ((mw - 16) / 3);
            if (c >= 0 && c < 3) { am_start_marketing(c, m, sizeof(m)); set_toast(m); }
        }
        return;
    }

    if (g_tab == TAB_OPS) {
        char m[96];
        if (ty >= 92 && ty <= 136) {
            if (tx >= mx + 260 && tx < mx + 360 && g_am.staff_salary_pct > 80) g_am.staff_salary_pct -= 5;
            else if (tx >= mx + 370 && tx < mx + 470 && g_am.staff_salary_pct < 140) g_am.staff_salary_pct += 5;
            return;
        }
        if (ty >= 296 && ty <= 346) {
            int ac = am_airport_count();
            if (tx < mx + 120) g_ops_hub_ap = (g_ops_hub_ap + ac - 1) % ac;
            else if (tx < mx + 480) g_ops_hub_ap = (g_ops_hub_ap + 1) % ac;
            else { am_buy_hub(g_ops_hub_ap, m, sizeof(m)); set_toast(m); }
        }
        return;
    }

    if (g_tab == TAB_FINANCE) {
        char m[96];
        if (ty >= 200 && ty < sh - 28) {
            int r = (ty - 200) / 56, idx = g_stock_scroll + r;
            if (idx >= 0 && idx < AM_NUM_RIVALS && r < 4) {
                g_stock_sel = idx;
                if (tx >= mx + mw - 86) { am_trade_stock(idx, -100, m, sizeof(m)); set_toast(m); }
                else if (tx >= mx + mw - 178) { am_trade_stock(idx, 100, m, sizeof(m)); set_toast(m); }
            }
        }
    }
}

/* ============================================================
 * 現代暗黑航空雷達介面渲染
 * ============================================================ */
static void render_left_rail(int sh) {
    int rw = 84;
    hal_fill_rect(0, 0, rw, sh, 0x0B1320);
    hal_fill_rect(rw, 0, 1, sh, 0x1E324F);

    ui_fill_rrect(16, 10, 52, 34, 9, 0x2563EB);
    ui_draw_ascii(24, 21, g_am.iata_code[0] ? g_am.iata_code : "AM", 2, 0xFFFFFF);

    const char *tabs[6] = { "雷達", "機隊", "買機", "油料", "樞紐", "股市" };
    for (int i = 0; i < 6; i++) {
        int ty = 58 + i * 64, act = ((int)g_tab == i);
        uint32_t bg = act ? 0x16263D : 0x0B1320;
        if (act) { ui_fill_rrect(8, ty, rw - 16, 52, 10, bg); hal_fill_rect(0, ty + 10, 4, 32, 0x38BDF8); }
        hal_set_text_bg(bg);
        ui_draw_str(22, ty + 17, tabs[i], 48, act ? 0x38BDF8 : 0x94A3B8);
        char kb[4]; snprintf(kb, sizeof(kb), "%d", i + 1);
        ui_draw_ascii(rw - 15, ty + 5, kb, 1, 0x64748B);
    }
}

static void render_top_bar(int sw) {
    int x0 = 85;
    hal_fill_rect(x0, 0, sw - x0, 42, 0x0F1A2B);
    hal_fill_rect(x0, 42, sw - x0, 1, 0x1E324F);

    char cash_s[24], hdr[128];
    fmt_money(g_am.cash, cash_s, sizeof(cash_s));
    snprintf(hdr, sizeof(hdr), "%s  %s  ·  油:%lldK  聲譽:%d%%",
             g_am.airline_name, cash_s, (long long)(g_am.fuel_lbs / 1000LL), am_effective_reputation());
    hal_set_text_bg(0x0F1A2B);
    ui_draw_str(x0 + 12, 12, hdr, sw - x0 - 240, 0xF8FAFC);

    /* 倍速按鈕 (1x / 10x / 60x / 360x) */
    ui_fill_rrect(sw - 220, 7, 94, 28, 8, 0x1E293B);
    ui_draw_rrect(sw - 220, 7, 94, 28, 8, 0x38BDF8);
    hal_set_text_bg(0x1E293B);
    char sp[24]; snprintf(sp, sizeof(sp), "速度 %dx", am_sim_speed_mult());
    ui_draw_str(sw - 208, 12, sp, 78, 0x38BDF8);

    /* 一鍵全機起飛鈕 */
    ui_fill_rrect(sw - 118, 7, 108, 28, 8, 0x059669);
    hal_set_text_bg(0x059669);
    ui_draw_str(sw - 108, 12, "全機起飛[D]", 94, 0xFFFFFF);
}

static void render_tab_radar(int mx, int mw, int sh) {
    int my = 52, mh = sh - 134;
    ui_draw_world_map(mx, my, mw, mh);

    /* 繪製所有啟用中的航線與即時飛機光點 */
    int flying = 0, ready = 0;
    for (int i = 0; i < g_am.fleet_count; i++) {
        const FleetPlane *fp = &g_am.fleet[i];
        if (!fp->active) continue;
        if (fp->status == 1) flying++;
        else if (fp->status == 0 && fp->has_route) ready++;
        if (fp->has_route && fp->orig_ap >= 0 && fp->dest_ap >= 0) {
            const AirportInfo *ao = am_airport_get(fp->orig_ap);
            const AirportInfo *ad = am_airport_get(fp->dest_ap);
            int pct = (fp->status == 1 && fp->flight_total_sec > 0) ? ((fp->flight_elapsed_sec * 100) / fp->flight_total_sec) : -1;
            ui_draw_route_arc(ao->lat10, ao->lon10, ad->lat10, ad->lon10, mx, my, mw, mh,
                              pct, (fp->status == 1) ? 0x38BDF8 : 0x334155, 0xFACC15);
        }
    }

    /* 繪製玩家擁有的樞紐機場 (發光金點 + IATA) */
    for (int h = 0; h < g_am.hub_count; h++) {
        const AirportInfo *ap = am_airport_get(g_am.hubs[h]);
        int hx, hy; ui_latlon_to_xy(ap->lat10, ap->lon10, mx, my, mw, mh, &hx, &hy);
        hal_fill_circle(hx, hy, 5, 0xF59E0B);
        hal_fill_circle(hx, hy, 2, 0xFFFFFF);
        ui_draw_ascii(hx + 6, hy - 4, ap->iata, 1, 0xFEF08A);
    }

    /* 底部即時航空簡報與突發事件條 */
    int by = my + mh + 10;
    ui_card(mx, by, mw, 64, 12, 0x111C2E, 0x1E324F, 0x050910, 0, 0);
    hal_set_text_bg(0x111C2E);
    char l1[128], l2[128];
    snprintf(l1, sizeof(l1), "機隊：%d 架 (飛行中 %d · 待命 %d)  ·  機場庫：%d 座  ·  油價 $%d · CO2 $%d",
             g_am.fleet_count, flying, ready, am_airport_count(), g_am.fuel_price, g_am.co2_price);
    snprintf(l2, sizeof(l2), "%s", g_toast[0] ? g_toast : am_event_desc(g_am.event_id));
    ui_draw_str(mx + 14, by + 10, l1, mw - 28, 0xE2E8F0);
    ui_draw_str(mx + 14, by + 36, l2, mw - 28, g_am.event_id ? 0xFACC15 : 0x38BDF8);
}

static void render_tab_fleet(int mx, int mw, int sh) {
    if (g_am.fleet_count == 0) {
        hal_set_text_bg(0x08101C); ui_draw_str(mx + 20, 90, "目前沒有飛機，請至 [3 買機] 購置機隊！", 400, 0x94A3B8); return;
    }
    if (g_fleet_sel < g_fleet_scroll) g_fleet_scroll = g_fleet_sel;
    if (g_fleet_sel >= g_fleet_scroll + 4) g_fleet_scroll = g_fleet_sel - 3;

    int top_y = 56, row_h = 84;
    for (int r = 0; r < 4; r++) {
        int idx = g_fleet_scroll + r; if (idx >= g_am.fleet_count) break;
        const FleetPlane *fp = &g_am.fleet[idx];
        const AircraftModel *m = am_aircraft_get(fp->model_idx);
        int ry = top_y + r * row_h;
        ui_card(mx, ry, mw, row_h - 10, 12, 0x111C2E, 0x1E324F, 0x050910, (idx == g_fleet_sel), 0x38BDF8);
        hal_set_text_bg(0x111C2E);

        char t1[96], t2[96];
        if (fp->has_route) {
            snprintf(t1, sizeof(t1), "%s  %s  (%s -> %s · %dkm)", fp->reg, m->model,
                     am_airport_get(fp->orig_ap)->iata, am_airport_get(fp->dest_ap)->iata, fp->dist_km);
        } else {
            snprintf(t1, sizeof(t1), "%s  %s  [尚未指派航線]", fp->reg, m->model);
        }
        ui_draw_str(mx + 14, ry + 10, t1, mw - 236, 0xF8FAFC);

        const char *st = (fp->status == 1) ? "飛行中" : ((fp->status == 2) ? "A-Check中" : "停機待命");
        snprintf(t2, sizeof(t2), "%s · 磨損:%d%% · A檢剩:%dh · 累計飛:%d次", st, fp->wear_pct, fp->acheck_hrs_left, fp->total_flights);
        ui_draw_str(mx + 14, ry + 34, t2, mw - 236, (fp->acheck_hrs_left <= 10) ? 0xF87171 : 0x94A3B8);

        int pct = (fp->status > 0 && fp->flight_total_sec > 0) ? ((fp->flight_elapsed_sec * 100) / fp->flight_total_sec) : 0;
        ui_progress_bar(mx + 14, ry + 58, mw - 242, 6, pct, 0x1E293B, (fp->status == 2) ? 0xF59E0B : 0x38BDF8);

        /* 右側 3 顆觸控按鈕：[起飛] [航線] [改裝] */
        uint32_t c_dep = (fp->status == 0 && fp->has_route) ? 0x059669 : 0x1E293B;
        ui_fill_rrect(mx + mw - 216, ry + 20, 64, 34, 8, c_dep); hal_set_text_bg(c_dep); ui_draw_str(mx + mw - 202, ry + 28, "起飛", 44, 0xFFFFFF);
        ui_fill_rrect(mx + mw - 144, ry + 20, 64, 34, 8, 0x2563EB); hal_set_text_bg(0x2563EB); ui_draw_str(mx + mw - 130, ry + 28, "航線", 44, 0xFFFFFF);
        ui_fill_rrect(mx + mw - 72,  ry + 20, 62, 34, 8, 0x475569); hal_set_text_bg(0x475569); ui_draw_str(mx + mw - 58,  ry + 28, "機體", 44, 0xFFFFFF);
    }

    hal_set_text_bg(0x08101C);
    char ft[96]; snprintf(ft, sizeof(ft), "%s", g_toast[0] ? g_toast : "點按鈕操作 · [Enter]起飛/設航線 · [←]航線 · [→]機體維修與改裝");
    ui_draw_str(mx, sh - 28, ft, mw, g_toast[0] ? 0xFACC15 : 0x94A3B8);
}

static void render_tab_shop(int mx, int mw, int sh) {
    const char *mfgs[7] = { "全部", "Airbus", "Boeing", "Embraer", "Regional", "Special", "Cargo" };
    for (int i = 0; i < 7; i++) {
        int px = mx + i * 96; uint32_t bg = (g_shop_mfg == i) ? 0x2563EB : 0x111C2E;
        ui_fill_rrect(px, 52, 90, 28, 8, bg); if (g_shop_mfg != i) ui_draw_rrect(px, 52, 90, 28, 8, 0x1E324F);
        hal_set_text_bg(bg); ui_draw_str(px + 12, 57, mfgs[i], 72, (g_shop_mfg == i) ? 0xFFFFFF : 0x94A3B8);
    }

    int list[AM_MAX_AIRCRAFT], cnt = build_shop_list(list, AM_MAX_AIRCRAFT);
    if (g_shop_sel >= cnt && cnt > 0) g_shop_sel = cnt - 1;
    if (g_shop_sel < g_shop_scroll) g_shop_scroll = g_shop_sel;
    if (g_shop_sel >= g_shop_scroll + 4) g_shop_scroll = g_shop_sel - 3;

    int top_y = 92, row_h = 82;
    for (int r = 0; r < 4; r++) {
        int idx = g_shop_scroll + r; if (idx >= cnt) break;
        const AircraftModel *m = am_aircraft_get(list[idx]);
        int ry = top_y + r * row_h;
        ui_card(mx, ry, mw, row_h - 10, 12, 0x111C2E, 0x1E324F, 0x050910, (idx == g_shop_sel), 0x38BDF8);
        hal_set_text_bg(0x111C2E);

        char pr[24], t1[96], t2[96];
        fmt_money(m->price, pr, sizeof(pr));
        snprintf(t1, sizeof(t1), "%s  (%s)  ·  %s", m->model, m->is_cargo ? "貨機" : "客機", pr);
        snprintf(t2, sizeof(t2), "航程:%ukm · 速度:%ukm/h · %s:%u · 油耗:%u · 跑道:%uft",
                 (unsigned)m->range_km, (unsigned)m->speed_kmh, m->is_cargo ? "載貨(噸)" : "座位",
                 (unsigned)m->cap, (unsigned)m->fuel_lbs_km, (unsigned)m->runway_ft);
        ui_draw_str(mx + 14, ry + 12, t1, mw - 120, 0xF8FAFC);
        ui_draw_str(mx + 14, ry + 40, t2, mw - 120, 0x94A3B8);

        uint32_t bg = (g_am.cash >= m->price) ? 0x059669 : 0x334155;
        ui_fill_rrect(mx + mw - 92, ry + 18, 80, 36, 9, bg);
        hal_set_text_bg(bg); ui_draw_str(mx + mw - 72, ry + 27, "購買", 50, 0xFFFFFF);
    }

    hal_set_text_bg(0x08101C);
    char pg[96];
    if (g_toast[0]) snprintf(pg, sizeof(pg), "%s", g_toast);
    else snprintf(pg, sizeof(pg), "共 %d 款真實機型 (第 %d / %d) · [PgUp/PgDn]快速翻頁 · [Enter]購買", cnt, g_shop_sel + 1, cnt);
    ui_draw_str(mx, sh - 28, pg, mw, g_toast[0] ? 0xFACC15 : 0x94A3B8);
}

static void render_tab_fuel(int mx, int mw, int sh) {
    int cw = (mw - 14) / 2;

    /* 左卡：燃油市場 (無規律跳躍走勢圖) */
    ui_card(mx, 54, cw, 284, 14, 0x111C2E, 0x1E324F, 0x050910, 0, 0);
    hal_set_text_bg(0x111C2E);
    char f1[64], f2[64];
    snprintf(f1, sizeof(f1), "航空燃油：$%d / 1000 lbs", g_am.fuel_price);
    snprintf(f2, sizeof(f2), "油槽：%lldK / %lldK lbs", (long long)(g_am.fuel_lbs / 1000LL), (long long)(g_am.fuel_cap / 1000LL));
    ui_draw_str(mx + 14, 66, f1, cw - 28, (g_am.fuel_price <= 700) ? 0x4ADE80 : ((g_am.fuel_price >= 1700) ? 0xF87171 : 0xF8FAFC));
    ui_draw_str(mx + 14, 92, f2, cw - 28, 0x94A3B8);
    ui_draw_sparkline(mx + 14, 120, cw - 28, 110, g_am.fuel_hist, AM_PRICE_HIST, 0xF59E0B, 0x1E324F);
    ui_fill_rrect(mx + 14, 244, cw - 28, 38, 9, 0x2563EB); hal_set_text_bg(0x2563EB); ui_draw_str(mx + 46, 254, "購買 50萬 lbs [Enter]", cw - 60, 0xFFFFFF);
    ui_fill_rrect(mx + 14, 290, cw - 28, 36, 9, 0x1E293B); ui_draw_rrect(mx + 14, 290, cw - 28, 36, 9, 0x334155);
    hal_set_text_bg(0x1E293B); ui_draw_str(mx + 56, 299, "擴充油槽 +2M ($1.2M)", cw - 70, 0xCBD5E1);

    /* 右卡：CO2 碳權市場 */
    int rx = mx + cw + 14;
    ui_card(rx, 54, cw, 284, 14, 0x111C2E, 0x1E324F, 0x050910, 0, 0);
    hal_set_text_bg(0x111C2E);
    char c1[64], c2[64];
    snprintf(c1, sizeof(c1), "CO2 碳權：$%d / 1000 額度", g_am.co2_price);
    snprintf(c2, sizeof(c2), "碳槽：%lldK / %lldK", (long long)(g_am.co2_quotas / 1000LL), (long long)(g_am.co2_cap / 1000LL));
    ui_draw_str(rx + 14, 66, c1, cw - 28, (g_am.co2_price <= 125) ? 0x4ADE80 : 0xF8FAFC);
    ui_draw_str(rx + 14, 92, c2, cw - 28, (g_am.co2_quotas < 0) ? 0xF87171 : 0x94A3B8);
    ui_draw_sparkline(rx + 14, 120, cw - 28, 110, g_am.co2_hist, AM_PRICE_HIST, 0x10B981, 0x1E324F);
    ui_fill_rrect(rx + 14, 244, cw - 28, 38, 9, 0x059669); hal_set_text_bg(0x059669); ui_draw_str(rx + 44, 254, "購買 50萬 CO2 [空白]", cw - 60, 0xFFFFFF);
    ui_fill_rrect(rx + 14, 290, cw - 28, 36, 9, 0x1E293B); ui_draw_rrect(rx + 14, 290, cw - 28, 36, 9, 0x334155);
    hal_set_text_bg(0x1E293B); ui_draw_str(rx + 54, 299, "擴充碳槽 +2M ($0.9M)", cw - 70, 0xCBD5E1);

    /* 下方 3 顆行銷廣告活動卡片 */
    int bw = (mw - 20) / 3;
    const char *mnames[3] = { "環保形象行銷 (+10%)", "區域廣告 (+18%)", "全球旗艦廣告 (+32%)" };
    const char *mcost[3]  = { "$120K · 12小時", "$280K · 12小時", "$650K · 24小時" };
    for (int i = 0; i < 3; i++) {
        int bx = mx + i * (bw + 10);
        ui_card(bx, 354, bw, 76, 12, 0x111C2E, 0x38BDF8, 0x050910, 0, 0);
        hal_set_text_bg(0x111C2E);
        ui_draw_str(bx + 14, 368, mnames[i], bw - 28, 0xF8FAFC);
        ui_draw_str(bx + 14, 396, mcost[i], bw - 28, 0x38BDF8);
    }

    hal_set_text_bg(0x08101C);
    ui_draw_str(mx, sh - 28, g_toast[0] ? g_toast : "油價與 CO2 採隨機跳躍模型，低價時記得多囤貨！", mw, 0xFACC15);
}

static void render_tab_ops(int mx, int mw, int sh) {
    /* 上卡：員工薪資與士氣管理 */
    ui_card(mx, 54, mw, 100, 14, 0x111C2E, 0x1E324F, 0x050910, 0, 0);
    hal_set_text_bg(0x111C2E);
    char st[96];
    snprintf(st, sizeof(st), "員工薪資水準：%d%%   ·   團隊士氣：%d%%   ·   有效聲譽：%d%%",
             g_am.staff_salary_pct, g_am.staff_morale, am_effective_reputation());
    ui_draw_str(mx + 16, 68, st, mw - 32, 0xF8FAFC);
    ui_draw_str(mx + 16, 102, "調整薪資 (高士氣提升載客率)：", 240, 0x94A3B8);
    ui_fill_rrect(mx + 260, 96, 96, 36, 8, 0x334155); hal_set_text_bg(0x334155); ui_draw_str(mx + 286, 105, "-5%", 50, 0xFFFFFF);
    ui_fill_rrect(mx + 370, 96, 96, 36, 8, 0x2563EB); hal_set_text_bg(0x2563EB); ui_draw_str(mx + 394, 105, "+5%", 50, 0xFFFFFF);

    /* 中卡：已擁有樞紐清單 */
    ui_card(mx, 168, mw, 104, 14, 0x111C2E, 0x1E324F, 0x050910, 0, 0);
    hal_set_text_bg(0x111C2E);
    char hhdr[64]; snprintf(hhdr, sizeof(hhdr), "已擁有全球樞紐機場 (%d / %d)：", g_am.hub_count, AM_MAX_HUBS);
    ui_draw_str(mx + 16, 180, hhdr, mw - 32, 0xFACC15);
    char hlist[180] = {0};
    for (int i = 0; i < g_am.hub_count; i++) {
        const AirportInfo *ap = am_airport_get(g_am.hubs[i]);
        char seg[16]; snprintf(seg, sizeof(seg), "%s ", ap->iata);
        strncat(hlist, seg, sizeof(hlist) - strlen(hlist) - 1);
    }
    ui_draw_str(mx + 16, 214, hlist, mw - 32, 0x38BDF8);

    /* 下卡：購買新樞紐機場 */
    const AirportInfo *ap = am_airport_get(g_ops_hub_ap);
    int64_t hcost = 3500000LL + (int64_t)g_am.hub_count * 1500000LL;
    char cs[24], apinfo[128]; fmt_money(hcost, cs, sizeof(cs));
    ui_card(mx, 286, mw, 136, 14, 0x111C2E, 0x38BDF8, 0x050910, 0, 0);
    hal_set_text_bg(0x111C2E);
    snprintf(apinfo, sizeof(apinfo), "擴張新樞紐：%s - %s (%s) · 跑道 %uft · 市場 %u%%",
             ap->iata, ap->name, ap->country, (unsigned)ap->runway_ft, (unsigned)ap->market_pct);
    ui_draw_str(mx + 16, 302, apinfo, mw - 32, 0xF8FAFC);

    ui_fill_rrect(mx + 16, 344, 100, 42, 9, 0x1E293B); hal_set_text_bg(0x1E293B); ui_draw_str(mx + 34, 356, "< 上一座", 76, 0xFFFFFF);
    ui_fill_rrect(mx + 128, 344, 100, 42, 9, 0x1E293B); ui_draw_str(mx + 146, 356, "下一座 >", 76, 0xFFFFFF);
    ui_fill_rrect(mx + mw - 210, 344, 194, 42, 9, 0x059669); hal_set_text_bg(0x059669);
    char bbtn[48]; snprintf(bbtn, sizeof(bbtn), "購置樞紐 (%s)", cs);
    ui_draw_str(mx + mw - 196, 356, bbtn, 170, 0xFFFFFF);

    hal_set_text_bg(0x08101C);
    ui_draw_str(mx, sh - 28, g_toast[0] ? g_toast : "支援放入 airports.csv 解鎖全球 4,000+ 座機場！", mw, 0x94A3B8);
}

static void render_tab_finance(int mx, int mw, int sh) {
    /* 上卡：公司財報與自家股價走勢 */
    ui_card(mx, 54, mw, 134, 14, 0x111C2E, 0x1E324F, 0x050910, 0, 0);
    hal_set_text_bg(0x111C2E);
    char r1[24], r2[24], l1[96], l2[96], l3[96];
    fmt_money(g_am.total_revenue, r1, sizeof(r1));
    fmt_money(g_am.total_expenses, r2, sizeof(r2));
    snprintf(l1, sizeof(l1), "%s IPO 股價：$%d / 股", g_am.airline_name, g_am.own_share_price);
    snprintf(l2, sizeof(l2), "累計營收：%s  ·  累計支出：%s", r1, r2);
    snprintf(l3, sizeof(l3), "完成航班：%d 次  ·  載客：%d 人  ·  貨運：%d 噸", g_am.total_flights_done, g_am.total_pax_carried, g_am.total_cargo_tons);
    ui_draw_str(mx + 16, 68,  l1, 360, 0x38BDF8);
    ui_draw_str(mx + 16, 100, l2, 360, 0xF8FAFC);
    ui_draw_str(mx + 16, 132, l3, 360, 0x94A3B8);
    ui_draw_sparkline(mx + mw - 246, 68, 230, 104, g_am.own_share_hist, AM_PRICE_HIST, 0x38BDF8, 0x1E324F);

    /* 下方：10 家 AI 競爭航空股市交易列表 */
    if (g_stock_sel < g_stock_scroll) g_stock_scroll = g_stock_sel;
    if (g_stock_sel >= g_stock_scroll + 4) g_stock_scroll = g_stock_sel - 3;

    for (int r = 0; r < 4; r++) {
        int idx = g_stock_scroll + r; if (idx >= AM_NUM_RIVALS) break;
        const RivalAirline *rv = &g_am.rivals[idx];
        int ry = 200 + r * 56;
        ui_card(mx, ry, mw, 48, 10, 0x111C2E, 0x1E324F, 0x050910, (idx == g_stock_sel), 0xFACC15);
        hal_set_text_bg(0x111C2E);
        char ln[96];
        snprintf(ln, sizeof(ln), "[%s] %s  ·  $%d/股  ·  持有:%d股", rv->code, rv->name, rv->price, rv->owned_shares);
        ui_draw_str(mx + 14, ry + 14, ln, mw - 290, 0xF8FAFC);

        ui_draw_sparkline(mx + mw - 270, ry + 6, 84, 36, rv->hist, AM_PRICE_HIST, 0x4ADE80, 0x1E324F);
        ui_fill_rrect(mx + mw - 178, ry + 9, 84, 30, 7, 0x059669); hal_set_text_bg(0x059669); ui_draw_str(mx + mw - 166, ry + 15, "買100股", 68, 0xFFFFFF);
        ui_fill_rrect(mx + mw - 86,  ry + 9, 76, 30, 7, 0xDC2626); hal_set_text_bg(0xDC2626); ui_draw_str(mx + mw - 76,  ry + 15, "賣100", 58, 0xFFFFFF);
    }

    hal_set_text_bg(0x08101C);
    ui_draw_str(mx, sh - 28, g_toast[0] ? g_toast : "點按鈕或用 [←/→] 買賣 AI 航空股票賺價差！", mw, 0xFACC15);
}

static void render_route_modal(int sw, int sh) {
    int mw = 560, mh = 350, mx = (sw - mw) / 2, my = (sh - mh) / 2;
    hal_clear_text_in_rect(mx - 8, my - 8, mw + 16, mh + 16); hal_dim_rect(0, 0, sw, sh);
    ui_card(mx, my, mw, mh, 14, 0x111C2E, 0x38BDF8, 0x050910, 1, 0x38BDF8);
    hal_set_text_bg(0x111C2E);

    const FleetPlane *fp = &g_am.fleet[g_rt_fleet_idx];
    const AircraftModel *m = am_aircraft_get(fp->model_idx);
    const AirportInfo *ao = am_airport_get(g_am.hubs[g_rt_orig_hub_slot % g_am.hub_count]);
    const AirportInfo *ad = am_airport_get(g_rt_dest_ap);
    char hdr[96]; snprintf(hdr, sizeof(hdr), "航線規劃：%s (%s · 航程 %ukm)", fp->reg, m->model, (unsigned)m->range_km);
    ui_draw_str(mx + 20, my + 16, hdr, mw - 40, 0x38BDF8);

    char r0[88], r1[88], r2[88];
    snprintf(r0, sizeof(r0), "出發樞紐：< %s - %s >", ao->iata, ao->name);
    snprintf(r1, sizeof(r1), "目的機場：< %s - %s (%s) > %s", ad->iata, ad->name, ad->country, g_rt_search[0] ? g_rt_search : "[打IATA可快搜]");
    if (g_rt_stop_ap >= 0) snprintf(r2, sizeof(r2), "中轉機場：< %s - %s >", am_airport_get(g_rt_stop_ap)->iata, am_airport_get(g_rt_stop_ap)->name);
    else snprintf(r2, sizeof(r2), "中轉機場：< 直飛無中轉 (左右鍵可設中轉) >");

    const char *rows[3] = { r0, r1, r2 };
    for (int i = 0; i < 3; i++) {
        int ry = my + 56 + i * 50; uint32_t bg = (g_rt_row == i) ? 0x1D4ED8 : 0x1E293B;
        ui_fill_rrect(mx + 20, ry, mw - 40, 40, 9, bg); hal_set_text_bg(bg);
        ui_draw_str(mx + 32, ry + 11, rows[i], mw - 64, 0xFFFFFF);
    }

    int dist = (g_rt_stop_ap >= 0) ? (am_calc_distance_km(g_am.hubs[g_rt_orig_hub_slot % g_am.hub_count], g_rt_stop_ap) + am_calc_distance_km(g_rt_stop_ap, g_rt_dest_ap))
                                   : am_calc_distance_km(g_am.hubs[g_rt_orig_hub_slot % g_am.hub_count], g_rt_dest_ap);
    char info[120];
    snprintf(info, sizeof(info), "大圓距離：%d km · 跑道：%uft/%uft · 經濟建議票價：$%d",
             dist, (unsigned)ao->runway_ft, (unsigned)ad->runway_ft, am_calc_auto_ticket(dist, 0, g_am.realism_mode));
    hal_set_text_bg(0x111C2E); ui_draw_str(mx + 22, my + 220, info, mw - 44, (dist > m->range_km && g_rt_stop_ap < 0) ? 0xF87171 : 0x4ADE80);
    if (g_toast[0]) ui_draw_str(mx + 22, my + 248, g_toast, mw - 44, 0xFACC15);

    ui_fill_rrect(mx + 20, my + 286, 250, 44, 10, 0x059669); hal_set_text_bg(0x059669); ui_draw_str(mx + 88, my + 299, "確認開通航線", 130, 0xFFFFFF);
    ui_fill_rrect(mx + 290, my + 286, 250, 44, 10, 0x334155); hal_set_text_bg(0x334155); ui_draw_str(mx + 388, my + 299, "取消", 60, 0xFFFFFF);
}

static void render_plane_modal(int sw, int sh) {
    int mw = 480, mh = 380, mx = (sw - mw) / 2, my = (sh - mh) / 2;
    hal_clear_text_in_rect(mx - 8, my - 8, mw + 16, mh + 16); hal_dim_rect(0, 0, sw, sh);
    ui_card(mx, my, mw, mh, 14, 0x111C2E, 0xFACC15, 0x050910, 1, 0xFACC15);
    const FleetPlane *fp = &g_am.fleet[g_pl_fleet_idx];
    const AircraftModel *m = am_aircraft_get(fp->model_idx);
    hal_set_text_bg(0x111C2E);
    char hdr[96]; snprintf(hdr, sizeof(hdr), "機體維護與改裝：%s (%s)", fp->reg, m->model);
    ui_draw_str(mx + 20, my + 16, hdr, mw - 40, 0xFACC15);
    char sub[96]; snprintf(sub, sizeof(sub), "艙等 Y:%d J:%d F:%d · 磨損:%d%% · A檢剩:%dh", fp->seats_y, fp->seats_j, fp->seats_f, fp->wear_pct, fp->acheck_hrs_left);
    ui_draw_str(mx + 20, my + 40, sub, mw - 40, 0x94A3B8);

    char opts[7][72];
    snprintf(opts[0], sizeof(opts[0]), "修復機體磨損 (目前 %d%%)", fp->wear_pct);
    snprintf(opts[1], sizeof(opts[1]), "執行 A-Check 大檢 (重置 %uh)", (unsigned)m->acheck_hrs);
    snprintf(opts[2], sizeof(opts[2]), "引擎改裝 +10%% 速度 [%s]", fp->mod_speed ? "已安裝" : "可改裝");
    snprintf(opts[3], sizeof(opts[3]), "翼尖改裝 -10%% 油耗 [%s]", fp->mod_fuel ? "已安裝" : "可改裝");
    snprintf(opts[4], sizeof(opts[4]), "環保改裝 -10%% CO2  [%s]", fp->mod_co2 ? "已安裝" : "可改裝");
    snprintf(opts[5], sizeof(opts[5]), "切換艙等配置 (高密度經濟 <-> 豪華商務)");
    snprintf(opts[6], sizeof(opts[6]), "出售此架飛機回收資金");

    for (int i = 0; i < 7; i++) {
        int ry = my + 66 + i * 42; uint32_t bg = (i == g_pl_sel) ? ((i == 6) ? 0xDC2626 : 0x2563EB) : 0x1E293B;
        ui_fill_rrect(mx + 20, ry, mw - 40, 34, 8, bg); hal_set_text_bg(bg);
        ui_draw_str(mx + 34, ry + 8, opts[i], mw - 68, 0xFFFFFF);
    }
}

void am_ui_render(void) {
    int sw = hal_get_width(), sh = hal_get_height();
    g_redraw = 0;

    if (g_scr == SCR_TITLE) {
        ui_draw_world_map(0, 0, sw, sh);
        int pw = 460, ph = 350, px = (sw - pw) / 2, py = 60;
        ui_card(px, py, pw, ph, 16, 0x0F1A2B, 0x38BDF8, 0x050910, 1, 0x38BDF8);
        hal_set_text_bg(0x0F1A2B);
        ui_draw_str(px + 92, py + 28, "AIRLINE MANAGER  CD-952", 300, 0x38BDF8);
        ui_draw_str(px + 54, py + 64, "310+真實機場(支援CSV擴充4000+) · 121款真實客貨機", 380, 0xCBD5E1);
        ui_draw_str(px + 66, py + 94, "無規律油價/CO2 · 離線掛機 · 改裝維修 · AI 股市", 360, 0x94A3B8);

        int hs = has_save_file(), n = hs ? 3 : 2;
        const char *opts[3];
        if (hs) { opts[0] = "繼續經營我的航空帝國"; opts[1] = "創立新航空公司"; opts[2] = "離開遊戲"; }
        else { opts[0] = "創立新航空公司"; opts[1] = "離開遊戲"; }
        for (int i = 0; i < n; i++) {
            uint32_t bg = (i == g_title_sel) ? ((i == 0) ? 0x059669 : 0x2563EB) : 0x1E293B;
            ui_fill_rrect(px + 56, py + 150 + i * 52, pw - 112, 42, 10, bg);
            hal_set_text_bg(bg); ui_draw_str(px + 132, py + 162 + i * 52, opts[i], 220, 0xFFFFFF);
        }
        hal_present(); return;
    }

    if (g_scr == SCR_CREATE) {
        ui_draw_world_map(0, 0, sw, sh);
        int pw = 560, ph = 420, px = (sw - pw) / 2, py = 26;
        ui_card(px, py, pw, ph, 16, 0x0F1A2B, 0x38BDF8, 0x050910, 1, 0x38BDF8);
        hal_set_text_bg(0x0F1A2B); ui_draw_str(px + 178, py + 14, "創立你的航空公司 (完整自訂)", 240, 0x38BDF8);

        /* Row 0: 自訂公司名 + 4快捷名 */
        uint32_t b0 = (g_create_row == 0) ? 0x1D4ED8 : 0x1E293B;
        ui_fill_rrect(px + 24, py + 48, pw - 48, 38, 9, b0); hal_set_text_bg(b0);
        char r0[96]; snprintf(r0, sizeof(r0), "公司名稱 (鍵盤直接打字)：%s_", g_new_name);
        ui_draw_str(px + 36, py + 58, r0, pw - 72, 0xFFFFFF);

        const char *pn[4] = { "Starlux Sim", "Formosa Air", "Sydney Jet", "Pacific Sky" };
        for (int i = 0; i < 4; i++) {
            int bx = px + 24 + i * 128;
            ui_fill_rrect(bx, py + 94, 120, 28, 7, 0x16263D); ui_draw_rrect(bx, py + 94, 120, 28, 7, 0x334155);
            hal_set_text_bg(0x16263D); ui_draw_str(bx + 10, py + 99, pn[i], 104, 0xCBD5E1);
        }

        /* Row 1: 自訂 2 碼 IATA */
        uint32_t b1 = (g_create_row == 1) ? 0x1D4ED8 : 0x1E293B;
        ui_fill_rrect(px + 24, py + 134, pw - 48, 38, 9, b1); hal_set_text_bg(b1);
        char r1[64]; snprintf(r1, sizeof(r1), "2碼 IATA 代碼 (機身編號字首)：%s_", g_new_iata);
        ui_draw_str(px + 36, py + 144, r1, pw - 72, 0xFFFFFF);

        /* Row 2: 難度模式 */
        uint32_t b2 = (g_create_row == 2) ? 0x1D4ED8 : 0x1E293B;
        ui_fill_rrect(px + 24, py + 184, pw - 48, 38, 9, b2); hal_set_text_bg(b2);
        char r2[80]; snprintf(r2, sizeof(r2), "模式：< %s >", g_new_realism ? "Realism 擬真模式 (嚴格檢查跑道)" : "Easy 輕鬆模式 (免跑道限制·高資金)");
        ui_draw_str(px + 36, py + 194, r2, pw - 72, 0xFFFFFF);

        /* Row 3: 起始樞紐機場 + 6熱門快捷 */
        const AirportInfo *ap = am_airport_get(g_new_hub_idx);
        uint32_t b3 = (g_create_row == 3) ? 0x1D4ED8 : 0x1E293B;
        ui_fill_rrect(px + 24, py + 234, pw - 48, 40, 9, b3); hal_set_text_bg(b3);
        char r3[96]; snprintf(r3, sizeof(r3), "起始樞紐：< %s - %s (%s) >", ap->iata, ap->name, ap->country);
        ui_draw_str(px + 36, py + 245, r3, pw - 72, 0xFFFFFF);

        const char *phubs[6] = { "TPE", "SYD", "HND", "SIN", "LHR", "JFK" };
        for (int i = 0; i < 6; i++) {
            int bx = px + 24 + i * 86;
            ui_fill_rrect(bx, py + 286, 78, 30, 7, 0x16263D); ui_draw_rrect(bx, py + 286, 78, 30, 7, 0x38BDF8);
            hal_set_text_bg(0x16263D); ui_draw_str(bx + 22, py + 292, phubs[i], 44, 0x38BDF8);
        }

        /* Row 4: 啟航大按鈕 */
        uint32_t b4 = (g_create_row == 4) ? 0x059669 : 0x10B981;
        ui_fill_rrect(px + 64, py + 346, pw - 128, 46, 12, b4); hal_set_text_bg(b4);
        ui_draw_str(px + 158, py + 360, ">>>  成立航空公司，正式啟航！  <<<", 280, 0xFFFFFF);
        hal_present(); return;
    }

    /* 主介面渲染 */
    hal_clear(0x08101C);
    render_left_rail(sh);
    render_top_bar(sw);

    int mx = 96, mw = sw - mx - 14;
    if (g_tab == TAB_RADAR) render_tab_radar(mx, mw, sh);
    else if (g_tab == TAB_FLEET) render_tab_fleet(mx, mw, sh);
    else if (g_tab == TAB_SHOP) render_tab_shop(mx, mw, sh);
    else if (g_tab == TAB_FUEL) render_tab_fuel(mx, mw, sh);
    else if (g_tab == TAB_OPS) render_tab_ops(mx, mw, sh);
    else render_tab_finance(mx, mw, sh);

    if (g_scr == SCR_ROUTE_MODAL) render_route_modal(sw, sh);
    else if (g_scr == SCR_PLANE_MODAL) render_plane_modal(sw, sh);
    else if (g_scr == SCR_PAUSE) {
        int pw = 340, ph = 220, px = (sw - pw) / 2, py = (sh - ph) / 2;
        hal_clear_text_in_rect(px - 8, py - 8, pw + 16, ph + 16); hal_dim_rect(0, 0, sw, sh);
        ui_card(px, py, pw, ph, 14, 0x0F1A2B, 0x38BDF8, 0x050910, 1, 0x38BDF8);
        hal_set_text_bg(0x0F1A2B); ui_draw_str(px + 116, py + 16, "系統暫停選單", 140, 0xFFFFFF);
        const char *po[3] = { "繼續經營 (ESC)", "存檔並回主畫面", "存檔並離開" };
        for (int i = 0; i < 3; i++) {
            uint32_t bg = (i == g_pause_sel) ? 0x2563EB : 0x1E293B;
            ui_fill_rrect(px + 28, py + 56 + i * 48, pw - 56, 38, 8, bg);
            hal_set_text_bg(bg); ui_draw_str(px + 84, py + 66 + i * 48, po[i], 180, 0xFFFFFF);
        }
    }

    hal_present();
}
