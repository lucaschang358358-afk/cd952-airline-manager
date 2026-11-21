#include "am_ui.h"
#include "am_assets.h"
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
    SCR_AP_PICKER,    /* 兩段式 國家 -> 城市/機場 選擇器 (通用於起始樞紐/航線/買新樞紐) */
    SCR_SHOP_DETAIL,  /* 完整機型百科介紹 + 藍圖剪影 + 購買確認頁 */
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

typedef enum {
    PICK_FOR_CREATE_HUB = 0,
    PICK_FOR_ROUTE_DEST,
    PICK_FOR_ROUTE_STOP,
    PICK_FOR_BUY_HUB
} PickerPurpose;

static AmScreen g_scr = SCR_TITLE;
static MainTab  g_tab = TAB_RADAR;
static int g_redraw = 1;
static int g_radar_tick_div = 0;
static int g_title_sel = 0;
static int g_pause_sel = 0;

/* 創立航空公司 */
static char g_new_name[28] = "Starlux Sim";
static char g_new_iata[4]  = "JX";
static int  g_new_realism  = 0;
static int  g_new_hub_idx  = 0;
static int  g_create_row   = 0; /* 0=Name, 1=IATA, 2=Mode, 3=Hub, 4=Launch */

/* 通用 國家 -> 城市/機場 選擇器 */
static PickerPurpose g_pick_purpose = PICK_FOR_CREATE_HUB;
static AmScreen      g_pick_return  = SCR_CREATE;
static int g_pick_step      = 0; /* 0=選國家, 1=選該國機場 */
static int g_pick_c_page    = 0;
static int g_pick_c_sel     = 0;
static int g_pick_ap_scroll = 0;
static int g_pick_ap_sel    = 0;
static int g_pick_ap_list[128];
static int g_pick_ap_cnt    = 0;

/* 各分頁選取、子頁籤與捲動狀態 */
static int g_fleet_sel = 0, g_fleet_scroll = 0;
static int g_shop_mfg  = 0, g_shop_sel = 0, g_shop_scroll = 0, g_shop_detail_model = 0;
static int g_ops_subtab = 0; /* 0=員工與樞紐, 1=等級與16項成就 */
static int g_ops_hub_ap = 42;
static int g_ach_scroll = 0;
static int g_fin_subtab = 0; /* 0=財報與貸款, 1=航班獲利紀錄, 2=AI股市 */
static int g_stock_sel  = 0, g_stock_scroll = 0;

/* 航線規劃與機體改裝 */
static int g_rt_fleet_idx = 0, g_rt_orig_hub_slot = 0, g_rt_dest_ap = 5, g_rt_stop_ap = -1, g_rt_row = 1;
static int g_pl_fleet_idx = 0, g_pl_sel = 0;

static char g_toast[96] = {0};
static int  g_toast_sec = 0;

static void set_toast(const char *s) {
    snprintf(g_toast, sizeof(g_toast), "%s", s ? s : "");
    g_toast_sec = 6;
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

/* 開啟兩段式 國家 -> 城市/機場 選擇器 */
static void open_airport_picker(PickerPurpose purpose, AmScreen ret_scr) {
    g_pick_purpose = purpose;
    g_pick_return  = ret_scr;
    g_pick_step    = 0;
    g_pick_c_page  = 0;
    g_pick_c_sel   = 0;
    g_pick_ap_sel  = 0;
    g_pick_ap_scroll = 0;
    g_scr = SCR_AP_PICKER;
}

static void picker_enter_country(int c_idx) {
    if (c_idx < 0 || c_idx >= am_country_count()) return;
    g_pick_c_sel = c_idx;
    g_pick_ap_cnt = am_airports_in_country(am_country_code(c_idx), g_pick_ap_list, 128);
    g_pick_ap_sel = 0;
    g_pick_ap_scroll = 0;
    g_pick_step = 1;
}

static void picker_commit_airport(int ap_idx) {
    if (g_pick_purpose == PICK_FOR_CREATE_HUB) g_new_hub_idx = ap_idx;
    else if (g_pick_purpose == PICK_FOR_ROUTE_DEST) g_rt_dest_ap = ap_idx;
    else if (g_pick_purpose == PICK_FOR_ROUTE_STOP) g_rt_stop_ap = ap_idx;
    else if (g_pick_purpose == PICK_FOR_BUY_HUB) g_ops_hub_ap = ap_idx;
    g_scr = g_pick_return;
}

static void open_route_modal(int fleet_idx) {
    if (fleet_idx < 0 || fleet_idx >= g_am.fleet_count) return;
    g_rt_fleet_idx = fleet_idx;
    g_rt_orig_hub_slot = 0;
    g_rt_dest_ap = (g_am.fleet[fleet_idx].has_route && g_am.fleet[fleet_idx].dest_ap >= 0) ? g_am.fleet[fleet_idx].dest_ap : ((g_am.hubs[0] + 5) % am_airport_count());
    g_rt_stop_ap = g_am.fleet[fleet_idx].stopover_ap;
    g_rt_row = 1;
    g_scr = SCR_ROUTE_MODAL;
}

static void open_plane_modal(int fleet_idx) {
    if (fleet_idx < 0 || fleet_idx >= g_am.fleet_count) return;
    g_pl_fleet_idx = fleet_idx; g_pl_sel = 0; g_scr = SCR_PLANE_MODAL;
}

void am_ui_init(void) {
    am_sim_init();
    g_scr = SCR_TITLE; g_tab = TAB_RADAR; g_title_sel = 0;
    int tpe = am_airport_find_iata("TPE"); g_new_hub_idx = (tpe >= 0) ? tpe : 0;
    int syd = am_airport_find_iata("SYD"); g_ops_hub_ap  = (syd >= 0) ? syd : 0;
    g_redraw = 1;
}

void am_ui_shutdown(void) { am_sim_save(); }
int  am_ui_needs_redraw(void) { return g_redraw; }

/* 零閃爍更新策略：絕不每秒亂刷靜態畫面與彈窗！ */
void am_ui_tick_second(void) {
    if (g_toast_sec > 0) {
        g_toast_sec--;
        if (g_toast_sec == 0) { g_toast[0] = '\0'; g_redraw = 1; }
    }
    if (g_scr == SCR_MAIN) {
        int major = am_sim_tick(1, 1);
        if (major) {
            if (g_am.last_notice[0]) set_toast(g_am.last_notice);
            g_redraw = 1;
            return;
        }
        /* 僅於雷達或機隊頁且「真的有飛機在飛」時，每 3 秒平穩刷新一次進度，徹底根除一陣一陣閃爍！ */
        if (g_tab == TAB_RADAR || g_tab == TAB_FLEET) {
            int active_flights = 0;
            for (int i = 0; i < g_am.fleet_count; i++) if (g_am.fleet[i].status > 0) { active_flights = 1; break; }
            if (active_flights) {
                g_radar_tick_div++;
                if (g_radar_tick_div >= 3) { g_radar_tick_div = 0; g_redraw = 1; }
            }
        }
    }
}

/* ============================================================
 * 按鍵事件處理
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

    if (g_scr == SCR_AP_PICKER) {
        if (g_pick_step == 0) {
            int cc = am_country_count();
            int per_pg = 12;
            int tot_pg = (cc + per_pg - 1) / per_pg; if (tot_pg < 1) tot_pg = 1;
            if (key == AKEY_BACK) { g_scr = g_pick_return; return; }
            if (key == AKEY_LEFT && g_pick_c_sel > 0) g_pick_c_sel--;
            else if (key == AKEY_RIGHT && g_pick_c_sel + 1 < cc) g_pick_c_sel++;
            else if (key == AKEY_UP && g_pick_c_sel >= 3) g_pick_c_sel -= 3;
            else if (key == AKEY_DOWN && g_pick_c_sel + 3 < cc) g_pick_c_sel += 3;
            else if (key == AKEY_PGUP) g_pick_c_sel = (g_pick_c_sel >= per_pg) ? (g_pick_c_sel - per_pg) : 0;
            else if (key == AKEY_PGDN) { g_pick_c_sel += per_pg; if (g_pick_c_sel >= cc) g_pick_c_sel = cc - 1; }
            else if (is_ok) picker_enter_country(g_pick_c_sel);
            g_pick_c_page = g_pick_c_sel / per_pg;
            if (g_pick_c_page >= tot_pg) g_pick_c_page = tot_pg - 1;
        } else {
            if (key == AKEY_BACK) { g_pick_step = 0; return; }
            if (key == AKEY_UP && g_pick_ap_sel > 0) g_pick_ap_sel--;
            else if (key == AKEY_DOWN && g_pick_ap_sel + 1 < g_pick_ap_cnt) g_pick_ap_sel++;
            else if (key == AKEY_PGUP) g_pick_ap_sel = (g_pick_ap_sel >= 6) ? (g_pick_ap_sel - 6) : 0;
            else if (key == AKEY_PGDN) { g_pick_ap_sel += 6; if (g_pick_ap_sel >= g_pick_ap_cnt) g_pick_ap_sel = g_pick_ap_cnt - 1; }
            else if (is_ok && g_pick_ap_sel < g_pick_ap_cnt) picker_commit_airport(g_pick_ap_list[g_pick_ap_sel]);
        }
        return;
    }

    if (g_scr == SCR_SHOP_DETAIL) {
        if (key == AKEY_BACK) { g_scr = SCR_MAIN; return; }
        if (is_ok) {
            char m[96];
            if (am_buy_aircraft(g_shop_detail_model, g_am.hubs[0], m, sizeof(m))) { set_toast(m); g_scr = SCR_MAIN; }
            else set_toast(m);
        }
        return;
    }

    if (g_scr == SCR_CREATE) {
        if (key == AKEY_BACK) { g_scr = SCR_TITLE; return; }
        if (key == AKEY_UP) { g_create_row = (g_create_row + 4) % 5; return; }
        if (key == AKEY_DOWN) { g_create_row = (g_create_row + 1) % 5; return; }

        if (g_create_row == 0) {
            if (key == AKEY_BACKSPACE) { size_t n = strlen(g_new_name); if (n > 0) g_new_name[n - 1] = '\0'; }
            else if (key == AKEY_OK) g_create_row = 1;
            else if (raw_char >= 0x20 && raw_char < 0x7F) {
                size_t n = strlen(g_new_name);
                if (n + 1 < sizeof(g_new_name)) { g_new_name[n] = raw_char; g_new_name[n + 1] = '\0'; }
            }
            return;
        }
        if (g_create_row == 1) {
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
            if (is_ok || key == AKEY_LEFT || key == AKEY_RIGHT) open_airport_picker(PICK_FOR_CREATE_HUB, SCR_CREATE);
            return;
        }
        if (g_create_row == 4 && is_ok) {
            am_sim_new_company(g_new_name, g_new_iata, g_new_hub_idx, g_new_realism);
            g_scr = SCR_MAIN; g_tab = TAB_RADAR; set_toast(g_am.last_notice);
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
        if (key == AKEY_BACK) { g_scr = SCR_MAIN; return; }
        if (key == AKEY_UP) { g_rt_row = (g_rt_row + 3) % 4; return; }
        if (key == AKEY_DOWN) { g_rt_row = (g_rt_row + 1) % 4; return; }
        if (g_rt_row == 0 && (key == AKEY_LEFT || key == AKEY_RIGHT) && g_am.hub_count > 0) {
            g_rt_orig_hub_slot = (g_rt_orig_hub_slot + g_am.hub_count + (key == AKEY_RIGHT ? 1 : -1)) % g_am.hub_count;
            return;
        }
        if (g_rt_row == 1 && (is_ok || key == AKEY_LEFT || key == AKEY_RIGHT)) { open_airport_picker(PICK_FOR_ROUTE_DEST, SCR_ROUTE_MODAL); return; }
        if (g_rt_row == 2) {
            if (key == AKEY_BACKSPACE) { g_rt_stop_ap = -1; return; }
            if (is_ok || key == AKEY_LEFT || key == AKEY_RIGHT) { open_airport_picker(PICK_FOR_ROUTE_STOP, SCR_ROUTE_MODAL); return; }
        }
        if (g_rt_row == 3 && is_ok) {
            char msg[96]; int orig = g_am.hubs[g_rt_orig_hub_slot % g_am.hub_count];
            if (am_assign_route(g_rt_fleet_idx, orig, g_rt_dest_ap, g_rt_stop_ap, msg, sizeof(msg))) { set_toast(msg); g_scr = SCR_MAIN; }
            else set_toast(msg);
        }
        return;
    }

    if (g_scr == SCR_PLANE_MODAL) {
        if (key == AKEY_BACK) { g_scr = SCR_MAIN; return; }
        if (key == AKEY_UP) g_pl_sel = (g_pl_sel + 6) % 7;
        else if (key == AKEY_DOWN) g_pl_sel = (g_pl_sel + 1) % 7;
        else if (is_ok) {
            char msg[96];
            if (g_pl_sel == 0) { am_repair_plane(g_pl_fleet_idx, msg, sizeof(msg)); set_toast(msg); }
            else if (g_pl_sel == 1) { am_acheck_plane(g_pl_fleet_idx, msg, sizeof(msg)); set_toast(msg); }
            else if (g_pl_sel == 2) { am_buy_mod(g_pl_fleet_idx, 0, msg, sizeof(msg)); set_toast(msg); }
            else if (g_pl_sel == 3) { am_buy_mod(g_pl_fleet_idx, 1, msg, sizeof(msg)); set_toast(msg); }
            else if (g_pl_sel == 4) { am_buy_mod(g_pl_fleet_idx, 2, msg, sizeof(msg)); set_toast(msg); }
            else if (g_pl_sel == 5) {
                FleetPlane *fp = &g_am.fleet[g_pl_fleet_idx];
                const AircraftModel *m = am_aircraft_get(fp->model_idx);
                if (!m->is_cargo) {
                    if (fp->seats_f == 0 && m->cap >= 120) {
                        fp->seats_f = m->cap / 12; fp->seats_j = m->cap / 6; fp->seats_y = m->cap - fp->seats_j * 2 - fp->seats_f * 3;
                        set_toast("已切換為【豪華頭等商務艙配置】！");
                    } else {
                        fp->seats_f = 0; fp->seats_j = m->cap / 12; fp->seats_y = m->cap - fp->seats_j * 2;
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
        else if (is_ok && g_shop_sel < cnt) { g_shop_detail_model = list[g_shop_sel]; g_scr = SCR_SHOP_DETAIL; }
    } else if (g_tab == TAB_FUEL) {
        char m[96];
        if (key == AKEY_OK) { am_buy_fuel(500000LL, m, sizeof(m)); set_toast(m); }
        else if (key == AKEY_SPACE) { am_buy_co2(500000LL, m, sizeof(m)); set_toast(m); }
    } else if (g_tab == TAB_OPS) {
        if (key == AKEY_SPACE) { g_ops_subtab = 1 - g_ops_subtab; return; }
        if (g_ops_subtab == 0) {
            if (key == AKEY_UP && g_am.staff_salary_pct < 140) g_am.staff_salary_pct += 5;
            else if (key == AKEY_DOWN && g_am.staff_salary_pct > 80) g_am.staff_salary_pct -= 5;
            else if (key == AKEY_LEFT || key == AKEY_RIGHT) open_airport_picker(PICK_FOR_BUY_HUB, SCR_MAIN);
            else if (key == AKEY_OK) { char m[96]; am_buy_hub(g_ops_hub_ap, m, sizeof(m)); set_toast(m); }
        } else {
            if (key == AKEY_UP && g_ach_scroll > 0) g_ach_scroll--;
            else if (key == AKEY_DOWN && g_ach_scroll < 4) g_ach_scroll++;
        }
    } else if (g_tab == TAB_FINANCE) {
        if (key == AKEY_SPACE) { g_fin_subtab = (g_fin_subtab + 1) % 3; return; }
        if (g_fin_subtab == 2) {
            char m[96];
            if (key == AKEY_UP && g_stock_sel > 0) g_stock_sel--;
            else if (key == AKEY_DOWN && g_stock_sel + 1 < AM_NUM_RIVALS) g_stock_sel++;
            else if (key == AKEY_RIGHT || key == AKEY_OK) { am_trade_stock(g_stock_sel, 100, m, sizeof(m)); set_toast(m); }
            else if (key == AKEY_LEFT) { am_trade_stock(g_stock_sel, -100, m, sizeof(m)); set_toast(m); }
        }
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

    if (g_scr == SCR_AP_PICKER) {
        int mw = 620, mh = 416, mx = (sw - mw) / 2, my = (sh - mh) / 2;
        if (tx < mx || tx > mx + mw || ty < my || ty > my + mh) { g_scr = g_pick_return; return; }
        if (ty <= my + 46) {
            if (tx < mx + 120) { if (g_pick_step == 1) g_pick_step = 0; else g_scr = g_pick_return; }
            return;
        }
        if (g_pick_step == 0) {
            int cc = am_country_count();
            if (ty >= my + mh - 46) {
                if (tx < mx + 180 && g_pick_c_page > 0) { g_pick_c_page--; g_pick_c_sel = g_pick_c_page * 12; }
                else if (tx > mx + mw - 180 && (g_pick_c_page + 1) * 12 < cc) { g_pick_c_page++; g_pick_c_sel = g_pick_c_page * 12; }
                return;
            }
            if (ty >= my + 58 && ty < my + mh - 50) {
                int col = (tx - (mx + 20)) / 196;
                int row = (ty - (my + 58)) / 74;
                if (col >= 0 && col < 3 && row >= 0 && row < 4) {
                    int idx = g_pick_c_page * 12 + row * 3 + col;
                    if (idx < cc) picker_enter_country(idx);
                }
            }
        } else {
            if (ty >= my + mh - 46) {
                if (tx < mx + 180 && g_pick_ap_scroll > 0) g_pick_ap_scroll -= 6;
                else if (tx > mx + mw - 180 && g_pick_ap_scroll + 6 < g_pick_ap_cnt) g_pick_ap_scroll += 6;
                if (g_pick_ap_scroll < 0) g_pick_ap_scroll = 0;
                return;
            }
            if (ty >= my + 58 && ty < my + mh - 50) {
                int row = (ty - (my + 58)) / 50;
                int idx = g_pick_ap_scroll + row;
                if (row >= 0 && row < 6 && idx < g_pick_ap_cnt) picker_commit_airport(g_pick_ap_list[idx]);
            }
        }
        return;
    }

    if (g_scr == SCR_SHOP_DETAIL) {
        int mw = 600, mh = 390, mx = (sw - mw) / 2, my = (sh - mh) / 2;
        if (tx < mx || tx > mx + mw || ty < my || ty > my + mh) { g_scr = SCR_MAIN; return; }
        if (ty >= my + mh - 56) {
            if (tx < mx + mw / 2) am_ui_on_key(AKEY_OK, 0);
            else g_scr = SCR_MAIN;
        }
        return;
    }

    if (g_scr == SCR_CREATE) {
        int pw = 580, px = (sw - pw) / 2, py = 24;
        /* Row 0: 公司名稱 + 右側 [退格] [清空] 觸控鈕 */
        if (ty >= py + 48 && ty <= py + 88) {
            g_create_row = 0;
            if (tx >= px + pw - 84) g_new_name[0] = '\0';
            else if (tx >= px + pw - 166) { size_t n = strlen(g_new_name); if (n > 0) g_new_name[n - 1] = '\0'; }
            return;
        }
        /* 快捷名稱 */
        if (ty >= py + 94 && ty <= py + 124) {
            const char *pn[4] = { "Starlux Sim", "Formosa Air", "Sydney Jet", "Pacific Sky" };
            const char *pi[4] = { "JX", "FM", "SY", "PS" };
            int idx = (tx - (px + 22)) / 134;
            if (idx >= 0 && idx < 4) { snprintf(g_new_name, sizeof(g_new_name), "%s", pn[idx]); snprintf(g_new_iata, sizeof(g_new_iata), "%s", pi[idx]); }
            return;
        }
        /* Row 1: IATA + 右側 [退格] [清空] */
        if (ty >= py + 134 && ty <= py + 174) {
            g_create_row = 1;
            if (tx >= px + pw - 84) g_new_iata[0] = '\0';
            else if (tx >= px + pw - 166) { size_t n = strlen(g_new_iata); if (n > 0) g_new_iata[n - 1] = '\0'; }
            return;
        }
        /* Row 2: 模式切換 */
        if (ty >= py + 184 && ty <= py + 224) { g_create_row = 2; g_new_realism = 1 - g_new_realism; return; }
        /* Row 3: 起始樞紐 -> 開啟 國家->城市 選擇器！ */
        if (ty >= py + 234 && ty <= py + 278) { g_create_row = 3; open_airport_picker(PICK_FOR_CREATE_HUB, SCR_CREATE); return; }
        /* 6 大熱門樞紐一鍵點擊 */
        if (ty >= py + 286 && ty <= py + 320) {
            const char *ph[6] = { "TPE", "SYD", "HND", "SIN", "LHR", "JFK" };
            int idx = (tx - (px + 22)) / 90;
            if (idx >= 0 && idx < 6) { int a = am_airport_find_iata(ph[idx]); if (a >= 0) g_new_hub_idx = a; }
            return;
        }
        /* Row 4: 成立航空 */
        if (ty >= py + 344 && ty <= py + 396) {
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
        else if (ty >= my + 106 && ty <= my + 148) { g_rt_row = 1; open_airport_picker(PICK_FOR_ROUTE_DEST, SCR_ROUTE_MODAL); }
        else if (ty >= my + 158 && ty <= my + 198) {
            g_rt_row = 2;
            if (tx >= mx + mw - 110) g_rt_stop_ap = -1;
            else open_airport_picker(PICK_FOR_ROUTE_STOP, SCR_ROUTE_MODAL);
        } else if (ty >= my + 286) {
            if (tx < mx + mw / 2) { g_rt_row = 3; am_ui_on_key(AKEY_OK, 0); }
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

    if (tx < 84) {
        for (int i = 0; i < 6; i++) { int y0 = 58 + i * 64; if (ty >= y0 && ty <= y0 + 54) { g_tab = (MainTab)i; return; } }
        return;
    }
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
        if (ty >= 52 && ty <= 82) { int c = (tx - mx) / 96; if (c >= 0 && c < 7) { g_shop_mfg = c; g_shop_sel = 0; g_shop_scroll = 0; } return; }
        int list[AM_MAX_AIRCRAFT], cnt = build_shop_list(list, AM_MAX_AIRCRAFT);
        int top_y = 92, row_h = 82;
        if (ty >= top_y && ty < sh - 38) {
            int r = (ty - top_y) / row_h, idx = g_shop_scroll + r;
            if (idx >= 0 && idx < cnt && r < 4) {
                g_shop_sel = idx;
                /* 點擊飛機卡片任意處都開啟完整機型介紹與購買確認頁！ */
                g_shop_detail_model = list[idx];
                g_scr = SCR_SHOP_DETAIL;
            }
        } else if (ty >= sh - 38) {
            if (tx < mx + 140) am_ui_on_key(AKEY_PGUP, 0);
            else if (tx >= sw - 150) am_ui_on_key(AKEY_PGDN, 0);
        }
        return;
    }

    if (g_tab == TAB_FUEL) {
        char m[96]; int half = mx + mw / 2;
        if (ty >= 244 && ty <= 284) { if (tx < half) am_buy_fuel(500000LL, m, sizeof(m)); else am_buy_co2(500000LL, m, sizeof(m)); set_toast(m); return; }
        if (ty >= 290 && ty <= 328) { if (tx < half) am_upgrade_fuel_tank(m, sizeof(m)); else am_upgrade_co2_tank(m, sizeof(m)); set_toast(m); return; }
        if (ty >= 354 && ty <= 430) { int c = (tx - mx) / ((mw - 16) / 3); if (c >= 0 && c < 3) { am_start_marketing(c, m, sizeof(m)); set_toast(m); } }
        return;
    }

    if (g_tab == TAB_OPS) {
        if (ty >= 50 && ty <= 82) {
            if (tx < mx + 200) g_ops_subtab = 0;
            else if (tx < mx + 410) g_ops_subtab = 1;
            return;
        }
        if (g_ops_subtab == 0) {
            char m[96];
            if (ty >= 122 && ty <= 164) {
                if (tx >= mx + 260 && tx < mx + 360 && g_am.staff_salary_pct > 80) g_am.staff_salary_pct -= 5;
                else if (tx >= mx + 370 && tx < mx + 470 && g_am.staff_salary_pct < 140) g_am.staff_salary_pct += 5;
                return;
            }
            if (ty >= 344 && ty <= 394) {
                if (tx < mx + 290) open_airport_picker(PICK_FOR_BUY_HUB, SCR_MAIN);
                else { am_buy_hub(g_ops_hub_ap, m, sizeof(m)); set_toast(m); }
            }
        } else if (ty >= sh - 40) {
            if (tx < mx + 180 && g_ach_scroll > 0) g_ach_scroll--;
            else if (tx > sw - 180 && g_ach_scroll < 4) g_ach_scroll++;
        }
        return;
    }

    if (g_tab == TAB_FINANCE) {
        if (ty >= 50 && ty <= 82) {
            int s = (tx - mx) / 160;
            if (s >= 0 && s < 3) g_fin_subtab = s;
            return;
        }
        char m[96];
        if (g_fin_subtab == 0 && ty >= 296 && ty <= 344) {
            int bw = (mw - 20) / 3, c = (tx - mx) / (bw + 10);
            if (c == 0) { am_take_loan(2000000LL, m, sizeof(m)); set_toast(m); }
            else if (c == 1) { am_take_loan(5000000LL, m, sizeof(m)); set_toast(m); }
            else if (c == 2) { am_repay_loan(2000000LL, m, sizeof(m)); set_toast(m); }
        } else if (g_fin_subtab == 2 && ty >= 96 && ty < sh - 28) {
            int r = (ty - 96) / 64, idx = g_stock_scroll + r;
            if (idx >= 0 && idx < AM_NUM_RIVALS && r < 5) {
                g_stock_sel = idx;
                if (tx >= mx + mw - 86) { am_trade_stock(idx, -100, m, sizeof(m)); set_toast(m); }
                else if (tx >= mx + mw - 178) { am_trade_stock(idx, 100, m, sizeof(m)); set_toast(m); }
            }
        }
    }
}

/* ============================================================
 * 介面渲染
 * ============================================================ */
static void render_left_rail(int sh) {
    int rw = 84;
    hal_fill_rect(0, 0, rw, sh, 0x0B1320); hal_fill_rect(rw, 0, 1, sh, 0x1E324F);
    ui_fill_rrect(16, 10, 52, 34, 9, 0x2563EB);
    ui_draw_ascii(24, 21, g_am.iata_code[0] ? g_am.iata_code : "AM", 2, 0xFFFFFF);
    const char *tabs[6] = { "雷達", "機隊", "買機", "油料", "樞紐", "財報" };
    for (int i = 0; i < 6; i++) {
        int ty = 58 + i * 64, act = ((int)g_tab == i);
        uint32_t bg = act ? 0x16263D : 0x0B1320;
        if (act) { ui_fill_rrect(8, ty, rw - 16, 52, 10, bg); hal_fill_rect(0, ty + 10, 4, 32, 0x38BDF8); }
        hal_set_text_bg(bg); ui_draw_str(22, ty + 17, tabs[i], 48, act ? 0x38BDF8 : 0x94A3B8);
        char kb[4]; snprintf(kb, sizeof(kb), "%d", i + 1); ui_draw_ascii(rw - 15, ty + 5, kb, 1, 0x64748B);
    }
}

static void render_top_bar(int sw) {
    int x0 = 85;
    hal_fill_rect(x0, 0, sw - x0, 42, 0x0F1A2B); hal_fill_rect(x0, 42, sw - x0, 1, 0x1E324F);
    char cash_s[24], hdr[128]; fmt_money(g_am.cash, cash_s, sizeof(cash_s));
    snprintf(hdr, sizeof(hdr), "Lv.%d %s  %s  ·  油:%lldK  聲譽:%d%%",
             am_airline_level(), g_am.airline_name, cash_s, (long long)(g_am.fuel_lbs / 1000LL), am_effective_reputation());
    hal_set_text_bg(0x0F1A2B); ui_draw_str(x0 + 12, 12, hdr, sw - x0 - 240, 0xF8FAFC);
    ui_fill_rrect(sw - 220, 7, 94, 28, 8, 0x1E293B); ui_draw_rrect(sw - 220, 7, 94, 28, 8, 0x38BDF8);
    hal_set_text_bg(0x1E293B); char sp[24]; snprintf(sp, sizeof(sp), "速度 %dx", am_sim_speed_mult());
    ui_draw_str(sw - 208, 12, sp, 78, 0x38BDF8);
    ui_fill_rrect(sw - 118, 7, 108, 28, 8, 0x059669); hal_set_text_bg(0x059669);
    ui_draw_str(sw - 108, 12, "全機起飛[D]", 94, 0xFFFFFF);
}

static void render_tab_radar(int mx, int mw, int sh) {
    int my = 52, mh = sh - 134;
    ui_draw_world_map(mx, my, mw, mh);
    (void)am_asset_terrain_sample(mx, my); /* 確保 ROM 符號連結保留 */

    int flying = 0, ready = 0;
    for (int i = 0; i < g_am.fleet_count; i++) {
        const FleetPlane *fp = &g_am.fleet[i]; if (!fp->active) continue;
        if (fp->status == 1) flying++; else if (fp->status == 0 && fp->has_route) ready++;
        if (fp->has_route && fp->orig_ap >= 0 && fp->dest_ap >= 0) {
            const AirportInfo *ao = am_airport_get(fp->orig_ap), *ad = am_airport_get(fp->dest_ap);
            int pct = (fp->status == 1 && fp->flight_total_sec > 0) ? ((fp->flight_elapsed_sec * 100) / fp->flight_total_sec) : -1;
            ui_draw_route_arc(ao->lat10, ao->lon10, ad->lat10, ad->lon10, mx, my, mw, mh, pct, (fp->status == 1) ? 0x38BDF8 : 0x334155, 0xFACC15);
        }
    }
    for (int h = 0; h < g_am.hub_count; h++) {
        const AirportInfo *ap = am_airport_get(g_am.hubs[h]);
        int hx, hy; ui_latlon_to_xy(ap->lat10, ap->lon10, mx, my, mw, mh, &hx, &hy);
        hal_fill_circle(hx, hy, 5, 0xF59E0B); hal_fill_circle(hx, hy, 2, 0xFFFFFF);
        ui_draw_ascii(hx + 6, hy - 4, ap->iata, 1, 0xFEF08A);
    }

    int by = my + mh + 10;
    ui_card(mx, by, mw, 64, 12, 0x111C2E, 0x1E324F, 0x050910, 0, 0); hal_set_text_bg(0x111C2E);
    char l1[128], l2[128];
    snprintf(l1, sizeof(l1), "機隊：%d 架 (飛行 %d · 待命 %d) · 全球 %d 國 / %d 座機場 · 油價 $%d · CO2 $%d",
             g_am.fleet_count, flying, ready, am_country_count(), am_airport_count(), g_am.fuel_price, g_am.co2_price);
    snprintf(l2, sizeof(l2), "%s", g_toast[0] ? g_toast : (g_am.event_id ? am_event_desc(g_am.event_id) : am_asset_tip(g_am.total_flights_done)));
    ui_draw_str(mx + 14, by + 10, l1, mw - 28, 0xE2E8F0);
    ui_draw_str(mx + 14, by + 36, l2, mw - 28, g_am.event_id ? 0xFACC15 : 0x38BDF8);
}

static void render_tab_fleet(int mx, int mw, int sh) {
    if (g_am.fleet_count == 0) { hal_set_text_bg(0x08101C); ui_draw_str(mx + 20, 90, "目前沒有飛機，請至 [3 買機] 購置機隊！", 400, 0x94A3B8); return; }
    if (g_fleet_sel < g_fleet_scroll) g_fleet_scroll = g_fleet_sel;
    if (g_fleet_sel >= g_fleet_scroll + 4) g_fleet_scroll = g_fleet_sel - 3;
    int top_y = 56, row_h = 84;
    for (int r = 0; r < 4; r++) {
        int idx = g_fleet_scroll + r; if (idx >= g_am.fleet_count) break;
        const FleetPlane *fp = &g_am.fleet[idx]; const AircraftModel *m = am_aircraft_get(fp->model_idx);
        int ry = top_y + r * row_h;
        ui_card(mx, ry, mw, row_h - 10, 12, 0x111C2E, 0x1E324F, 0x050910, (idx == g_fleet_sel), 0x38BDF8);
        hal_set_text_bg(0x111C2E);
        char t1[96], t2[96];
        if (fp->has_route) snprintf(t1, sizeof(t1), "%s  %s  (%s -> %s · %dkm)", fp->reg, m->model, am_airport_get(fp->orig_ap)->iata, am_airport_get(fp->dest_ap)->iata, fp->dist_km);
        else snprintf(t1, sizeof(t1), "%s  %s  [尚未指派航線]", fp->reg, m->model);
        ui_draw_str(mx + 14, ry + 10, t1, mw - 236, 0xF8FAFC);
        const char *st = (fp->status == 1) ? "飛行中" : ((fp->status == 2) ? "A-Check中" : "停機待命");
        snprintf(t2, sizeof(t2), "%s · 磨損:%d%% · A檢剩:%dh · 累計飛:%d次", st, fp->wear_pct, fp->acheck_hrs_left, fp->total_flights);
        ui_draw_str(mx + 14, ry + 34, t2, mw - 236, (fp->acheck_hrs_left <= 10) ? 0xF87171 : 0x94A3B8);
        int pct = (fp->status > 0 && fp->flight_total_sec > 0) ? ((fp->flight_elapsed_sec * 100) / fp->flight_total_sec) : 0;
        ui_progress_bar(mx + 14, ry + 58, mw - 242, 6, pct, 0x1E293B, (fp->status == 2) ? 0xF59E0B : 0x38BDF8);
        uint32_t c_dep = (fp->status == 0 && fp->has_route) ? 0x059669 : 0x1E293B;
        ui_fill_rrect(mx + mw - 216, ry + 20, 64, 34, 8, c_dep); hal_set_text_bg(c_dep); ui_draw_str(mx + mw - 202, ry + 28, "起飛", 44, 0xFFFFFF);
        ui_fill_rrect(mx + mw - 144, ry + 20, 64, 34, 8, 0x2563EB); hal_set_text_bg(0x2563EB); ui_draw_str(mx + mw - 130, ry + 28, "航線", 44, 0xFFFFFF);
        ui_fill_rrect(mx + mw - 72,  ry + 20, 62, 34, 8, 0x475569); hal_set_text_bg(0x475569); ui_draw_str(mx + mw - 58,  ry + 28, "機體", 44, 0xFFFFFF);
    }
    hal_set_text_bg(0x08101C);
    ui_draw_str(mx, sh - 28, g_toast[0] ? g_toast : "點按鈕操作 · [Enter]起飛/設航線 · [←]國家城市選航線 · [→]維修改裝", mw, g_toast[0] ? 0xFACC15 : 0x94A3B8);
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
        char pr[24], t1[96], t2[128];
        fmt_money(m->price, pr, sizeof(pr));
        snprintf(t1, sizeof(t1), "%s (%s)  ·  %s", m->model, m->is_cargo ? "貨機" : "客機", pr);
        snprintf(t2, sizeof(t2), "%s", m->desc);
        ui_draw_str(mx + 14, ry + 10, t1, mw - 124, 0xF8FAFC);
        ui_draw_str(mx + 14, ry + 38, t2, mw - 124, 0x94A3B8);

        ui_fill_rrect(mx + mw - 104, ry + 18, 92, 36, 9, 0x2563EB);
        hal_set_text_bg(0x2563EB); ui_draw_str(mx + mw - 94, ry + 27, "介紹/購買", 76, 0xFFFFFF);
    }
    hal_set_text_bg(0x08101C);
    char pg[140];
    if (g_toast[0]) snprintf(pg, sizeof(pg), "%s", g_toast);
    else snprintf(pg, sizeof(pg), "共 %d 款真實機型 (第 %d/%d) · 點擊看藍圖與詳細機型介紹 · [PgUp/PgDn]翻頁", cnt, g_shop_sel + 1, cnt);
    ui_draw_str(mx, sh - 28, pg, mw, g_toast[0] ? 0xFACC15 : 0x94A3B8);
}

static void render_tab_fuel(int mx, int mw, int sh) {
    int cw = (mw - 14) / 2;
    ui_card(mx, 54, cw, 284, 14, 0x111C2E, 0x1E324F, 0x050910, 0, 0); hal_set_text_bg(0x111C2E);
    char f1[64], f2[64]; snprintf(f1, sizeof(f1), "航空燃油：$%d / 1000 lbs", g_am.fuel_price);
    snprintf(f2, sizeof(f2), "油槽：%lldK / %lldK lbs", (long long)(g_am.fuel_lbs / 1000LL), (long long)(g_am.fuel_cap / 1000LL));
    ui_draw_str(mx + 14, 66, f1, cw - 28, (g_am.fuel_price <= 700) ? 0x4ADE80 : ((g_am.fuel_price >= 1700) ? 0xF87171 : 0xF8FAFC));
    ui_draw_str(mx + 14, 92, f2, cw - 28, 0x94A3B8);
    ui_draw_sparkline(mx + 14, 120, cw - 28, 110, g_am.fuel_hist, AM_PRICE_HIST, 0xF59E0B, 0x1E324F);
    ui_fill_rrect(mx + 14, 244, cw - 28, 38, 9, 0x2563EB); hal_set_text_bg(0x2563EB); ui_draw_str(mx + 46, 254, "購買 50萬 lbs [Enter]", cw - 60, 0xFFFFFF);
    ui_fill_rrect(mx + 14, 290, cw - 28, 36, 9, 0x1E293B); ui_draw_rrect(mx + 14, 290, cw - 28, 36, 9, 0x334155);
    hal_set_text_bg(0x1E293B); ui_draw_str(mx + 56, 299, "擴充油槽 +2M ($1.2M)", cw - 70, 0xCBD5E1);

    int rx = mx + cw + 14;
    ui_card(rx, 54, cw, 284, 14, 0x111C2E, 0x1E324F, 0x050910, 0, 0); hal_set_text_bg(0x111C2E);
    char c1[64], c2[64]; snprintf(c1, sizeof(c1), "CO2 碳權：$%d / 1000 額度", g_am.co2_price);
    snprintf(c2, sizeof(c2), "碳槽：%lldK / %lldK", (long long)(g_am.co2_quotas / 1000LL), (long long)(g_am.co2_cap / 1000LL));
    ui_draw_str(rx + 14, 66, c1, cw - 28, (g_am.co2_price <= 125) ? 0x4ADE80 : 0xF8FAFC);
    ui_draw_str(rx + 14, 92, c2, cw - 28, (g_am.co2_quotas < 0) ? 0xF87171 : 0x94A3B8);
    ui_draw_sparkline(rx + 14, 120, cw - 28, 110, g_am.co2_hist, AM_PRICE_HIST, 0x10B981, 0x1E324F);
    ui_fill_rrect(rx + 14, 244, cw - 28, 38, 9, 0x059669); hal_set_text_bg(0x059669); ui_draw_str(rx + 44, 254, "購買 50萬 CO2 [空白]", cw - 60, 0xFFFFFF);
    ui_fill_rrect(rx + 14, 290, cw - 28, 36, 9, 0x1E293B); ui_draw_rrect(rx + 14, 290, cw - 28, 36, 9, 0x334155);
    hal_set_text_bg(0x1E293B); ui_draw_str(rx + 54, 299, "擴充碳槽 +2M ($0.9M)", cw - 70, 0xCBD5E1);

    int bw = (mw - 20) / 3;
    const char *mnames[3] = { "環保形象行銷 (+10%)", "區域廣告 (+18%)", "全球旗艦廣告 (+32%)" };
    const char *mcost[3]  = { "$120K · 12小時", "$280K · 12小時", "$650K · 24小時" };
    for (int i = 0; i < 3; i++) {
        int bx = mx + i * (bw + 10);
        ui_card(bx, 354, bw, 76, 12, 0x111C2E, 0x38BDF8, 0x050910, 0, 0); hal_set_text_bg(0x111C2E);
        ui_draw_str(bx + 14, 368, mnames[i], bw - 28, 0xF8FAFC); ui_draw_str(bx + 14, 396, mcost[i], bw - 28, 0x38BDF8);
    }
    hal_set_text_bg(0x08101C);
    ui_draw_str(mx, sh - 28, g_toast[0] ? g_toast : "油價採無規律隨機跳躍模型，低於 $650 囤油還可解鎖成就獎金！", mw, 0xFACC15);
}

static void render_tab_ops(int mx, int mw, int sh) {
    const char *stabs[2] = { "員工薪資 & 全球樞紐", "航空等級 & 16項成就" };
    for (int i = 0; i < 2; i++) {
        int tx = mx + i * 204; uint32_t bg = (g_ops_subtab == i) ? 0x2563EB : 0x111C2E;
        ui_fill_rrect(tx, 50, 196, 30, 8, bg); if (g_ops_subtab != i) ui_draw_rrect(tx, 50, 196, 30, 8, 0x1E324F);
        hal_set_text_bg(bg); ui_draw_str(tx + 18, 56, stabs[i], 168, (g_ops_subtab == i) ? 0xFFFFFF : 0x94A3B8);
    }

    if (g_ops_subtab == 0) {
        ui_card(mx, 90, mw, 86, 14, 0x111C2E, 0x1E324F, 0x050910, 0, 0); hal_set_text_bg(0x111C2E);
        char st[96]; snprintf(st, sizeof(st), "員工薪資：%d%% · 士氣：%d%% · 有效聲譽：%d%%", g_am.staff_salary_pct, g_am.staff_morale, am_effective_reputation());
        ui_draw_str(mx + 16, 102, st, mw - 32, 0xF8FAFC);
        ui_draw_str(mx + 16, 134, "調整薪資水準：", 180, 0x94A3B8);
        ui_fill_rrect(mx + 260, 126, 96, 34, 8, 0x334155); hal_set_text_bg(0x334155); ui_draw_str(mx + 286, 134, "-5%", 50, 0xFFFFFF);
        ui_fill_rrect(mx + 370, 126, 96, 34, 8, 0x2563EB); hal_set_text_bg(0x2563EB); ui_draw_str(mx + 394, 134, "+5%", 50, 0xFFFFFF);

        ui_card(mx, 188, mw, 92, 14, 0x111C2E, 0x1E324F, 0x050910, 0, 0); hal_set_text_bg(0x111C2E);
        char hhdr[64]; snprintf(hhdr, sizeof(hhdr), "已擁有全球樞紐機場 (%d / %d)：", g_am.hub_count, AM_MAX_HUBS);
        ui_draw_str(mx + 16, 200, hhdr, mw - 32, 0xFACC15);
        char hlist[180] = {0};
        for (int i = 0; i < g_am.hub_count; i++) { char seg[16]; snprintf(seg, sizeof(seg), "%s ", am_airport_get(g_am.hubs[i])->iata); strncat(hlist, seg, sizeof(hlist) - strlen(hlist) - 1); }
        ui_draw_str(mx + 16, 232, hlist, mw - 32, 0x38BDF8);

        const AirportInfo *ap = am_airport_get(g_ops_hub_ap);
        int64_t hcost = 3500000LL + (int64_t)g_am.hub_count * 1500000LL;
        char cs[24], apinfo[128]; fmt_money(hcost, cs, sizeof(cs));
        ui_card(mx, 292, mw, 132, 14, 0x111C2E, 0x38BDF8, 0x050910, 0, 0); hal_set_text_bg(0x111C2E);
        snprintf(apinfo, sizeof(apinfo), "目標樞紐：[%s] %s - %s · 跑道 %uft · 市場 %u%%",
                 am_country_name_zh(ap->country), ap->iata, ap->name, (unsigned)ap->runway_ft, (unsigned)ap->market_pct);
        ui_draw_str(mx + 16, 308, apinfo, mw - 32, 0xF8FAFC);

        ui_fill_rrect(mx + 16, 348, 260, 44, 9, 0x2563EB); hal_set_text_bg(0x2563EB);
        ui_draw_str(mx + 38, 361, "依國家 / 城市挑選機場 >", 220, 0xFFFFFF);

        ui_fill_rrect(mx + mw - 230, 348, 214, 44, 9, 0x059669); hal_set_text_bg(0x059669);
        char bbtn[48]; snprintf(bbtn, sizeof(bbtn), "購置此樞紐 (%s)", cs);
        ui_draw_str(mx + mw - 216, 361, bbtn, 190, 0xFFFFFF);
    } else {
        int lv = am_airline_level();
        ui_card(mx, 90, mw, 64, 12, 0x111C2E, 0xFACC15, 0x050910, 0, 0); hal_set_text_bg(0x111C2E);
        char lvinfo[128]; snprintf(lvinfo, sizeof(lvinfo), "航空等級：Lv.%d %s  ·  已解鎖成就 %d / %d", lv, am_airline_rank_name(lv), am_ach_unlocked_count(), AM_NUM_ACH);
        ui_draw_str(mx + 16, 112, lvinfo, mw - 32, 0xFACC15);

        int col_w = (mw - 12) / 2;
        for (int slot = 0; slot < 8; slot++) {
            int idx = g_ach_scroll * 2 + slot; if (idx >= AM_NUM_ACH) break;
            int r = slot / 2, c = slot % 2, ax = mx + c * (col_w + 12), ay = 164 + r * 68;
            int unl = g_am.ach_unlocked[idx];
            ui_card(ax, ay, col_w, 60, 10, 0x111C2E, unl ? 0x10B981 : 0x1E324F, 0x050910, unl, 0x10B981);
            hal_set_text_bg(0x111C2E);
            char rw[24], nm[64]; fmt_money(am_ach_reward(idx), rw, sizeof(rw));
            snprintf(nm, sizeof(nm), "%s (%s)", am_ach_name(idx), rw);
            ui_draw_str(ax + 12, ay + 8, nm, col_w - 64, unl ? 0x4ADE80 : 0xF8FAFC);
            ui_draw_str(ax + 12, ay + 34, am_ach_desc(idx), col_w - 24, 0x94A3B8);
            if (unl) ui_draw_badge(ax + col_w - 52, ay + 10, "DONE", 0x10B981, 0xFFFFFF);
        }
    }
    hal_set_text_bg(0x08101C);
    ui_draw_str(mx, sh - 28, g_toast[0] ? g_toast : "[空白]切換子分頁 · 解鎖成就自動發放高額獎金！", mw, 0x94A3B8);
}

static void render_tab_finance(int mx, int mw, int sh) {
    const char *ftabs[3] = { "財報與銀行貸款", "航班獲利紀錄", "10家AI航空股市" };
    for (int i = 0; i < 3; i++) {
        int tx = mx + i * 162; uint32_t bg = (g_fin_subtab == i) ? 0x2563EB : 0x111C2E;
        ui_fill_rrect(tx, 50, 154, 30, 8, bg); if (g_fin_subtab != i) ui_draw_rrect(tx, 50, 154, 30, 8, 0x1E324F);
        hal_set_text_bg(bg); ui_draw_str(tx + 14, 56, ftabs[i], 132, (g_fin_subtab == i) ? 0xFFFFFF : 0x94A3B8);
    }

    if (g_fin_subtab == 0) {
        ui_card(mx, 92, mw, 184, 14, 0x111C2E, 0x1E324F, 0x050910, 0, 0); hal_set_text_bg(0x111C2E);
        char r1[24], r2[24], l0[24], l1[96], l2[96], l3[96], l4[96];
        fmt_money(g_am.total_revenue, r1, sizeof(r1)); fmt_money(g_am.total_expenses, r2, sizeof(r2)); fmt_money(g_am.loan_balance, l0, sizeof(l0));
        snprintf(l1, sizeof(l1), "%s IPO 股價：$%d / 股", g_am.airline_name, g_am.own_share_price);
        snprintf(l2, sizeof(l2), "累計營收：%s   ·   累計支出：%s", r1, r2);
        snprintf(l3, sizeof(l3), "完成航班：%d 次 · 載客：%d 人 · 貨運：%d 噸", g_am.total_flights_done, g_am.total_pax_carried, g_am.total_cargo_tons);
        snprintf(l4, sizeof(l4), "銀行貸款餘額：%s (上限 $30M · 固定息 10%%)", l0);
        ui_draw_str(mx + 16, 108, l1, 380, 0x38BDF8);
        ui_draw_str(mx + 16, 142, l2, 380, 0xF8FAFC);
        ui_draw_str(mx + 16, 176, l3, 380, 0x94A3B8);
        ui_draw_str(mx + 16, 214, l4, 380, (g_am.loan_balance > 0) ? 0xF87171 : 0x4ADE80);
        ui_draw_sparkline(mx + mw - 246, 108, 230, 144, g_am.own_share_hist, AM_PRICE_HIST, 0x38BDF8, 0x1E324F);

        int bw = (mw - 20) / 3;
        ui_card(mx, 296, bw, 52, 10, 0x2563EB, 0x60A5FA, 0x050910, 0, 0); hal_set_text_bg(0x2563EB); ui_draw_str(mx + 28, 313, "銀行貸款 +$2M", bw - 40, 0xFFFFFF);
        ui_card(mx + bw + 10, 296, bw, 52, 10, 0x7C3AED, 0xA78BFA, 0x050910, 0, 0); hal_set_text_bg(0x7C3AED); ui_draw_str(mx + bw + 38, 313, "銀行貸款 +$5M", bw - 40, 0xFFFFFF);
        ui_card(mx + (bw + 10) * 2, 296, bw, 52, 10, 0x059669, 0x34D399, 0x050910, 0, 0); hal_set_text_bg(0x059669); ui_draw_str(mx + (bw + 10) * 2 + 28, 313, "償還貸款 $2M", bw - 40, 0xFFFFFF);
    } else if (g_fin_subtab == 1) {
        ui_card(mx, 92, mw, 344, 14, 0x111C2E, 0x1E324F, 0x050910, 0, 0); hal_set_text_bg(0x111C2E);
        ui_draw_str(mx + 16, 104, "最近 12 趟完成航班獲利結算紀錄：", mw - 32, 0x38BDF8);
        if (g_am.flight_log_count == 0) {
            ui_draw_str(mx + 16, 154, "尚未有落地完成的航班，快去派飛機起飛吧！", mw - 32, 0x94A3B8);
        } else {
            for (int i = 0; i < g_am.flight_log_count && i < 11; i++) {
                const FlightLogEntry *fl = &g_am.flight_log[i];
                char ps[24], ln[120]; fmt_money(fl->net_profit, ps, sizeof(ps));
                snprintf(ln, sizeof(ln), "#%02d  %s  (%s)  %s:%d  淨利 +%s",
                         i + 1, fl->reg, fl->route, fl->is_cargo ? "貨運噸" : "載客人", fl->pax_or_tons, ps);
                ui_draw_str(mx + 16, 136 + i * 26, ln, mw - 32, (i == 0) ? 0x4ADE80 : 0xE2E8F0);
            }
        }
    } else {
        if (g_stock_sel < g_stock_scroll) g_stock_scroll = g_stock_sel;
        if (g_stock_sel >= g_stock_scroll + 5) g_stock_scroll = g_stock_sel - 4;
        for (int r = 0; r < 5; r++) {
            int idx = g_stock_scroll + r; if (idx >= AM_NUM_RIVALS) break;
            const RivalAirline *rv = &g_am.rivals[idx]; int ry = 96 + r * 64;
            ui_card(mx, ry, mw, 56, 10, 0x111C2E, 0x1E324F, 0x050910, (idx == g_stock_sel), 0xFACC15); hal_set_text_bg(0x111C2E);
            char ln[96]; snprintf(ln, sizeof(ln), "[%s] %s  ·  $%d/股  ·  持有:%d股", rv->code, rv->name, rv->price, rv->owned_shares);
            ui_draw_str(mx + 14, ry + 18, ln, mw - 290, 0xF8FAFC);
            ui_draw_sparkline(mx + mw - 270, ry + 10, 84, 36, rv->hist, AM_PRICE_HIST, 0x4ADE80, 0x1E324F);
            ui_fill_rrect(mx + mw - 178, ry + 13, 84, 30, 7, 0x059669); hal_set_text_bg(0x059669); ui_draw_str(mx + mw - 166, ry + 19, "買100股", 68, 0xFFFFFF);
            ui_fill_rrect(mx + mw - 86,  ry + 13, 76, 30, 7, 0xDC2626); hal_set_text_bg(0xDC2626); ui_draw_str(mx + mw - 76,  ry + 19, "賣100", 58, 0xFFFFFF);
        }
    }
    hal_set_text_bg(0x08101C);
    ui_draw_str(mx, sh - 28, g_toast[0] ? g_toast : "[空白]切換 財報貸款 / 航班獲利紀錄 / AI 股市", mw, 0xFACC15);
}

/* 兩段式 國家 -> 城市/機場 選擇器彈窗 */
static void render_airport_picker(int sw, int sh) {
    int mw = 620, mh = 416, mx = (sw - mw) / 2, my = (sh - mh) / 2;
    hal_clear_text_in_rect(mx - 8, my - 8, mw + 16, mh + 16); hal_dim_rect(0, 0, sw, sh);
    ui_card(mx, my, mw, mh, 14, 0x0F1A2B, 0x38BDF8, 0x050910, 1, 0x38BDF8);
    ui_fill_rrect(mx + 2, my + 2, mw - 4, 42, 12, 0x16263D);

    ui_fill_rrect(mx + 12, my + 8, 96, 28, 7, 0x1E293B); hal_set_text_bg(0x1E293B);
    ui_draw_str(mx + 22, my + 13, g_pick_step == 1 ? "< 換國家" : "< 取消", 80, 0xFFFFFF);

    hal_set_text_bg(0x16263D);
    if (g_pick_step == 0) {
        char hdr[96]; snprintf(hdr, sizeof(hdr), "步驟 1/2：選擇國家/地區 (共 %d 個國家 · 第 %d 頁)", am_country_count(), g_pick_c_page + 1);
        ui_draw_str(mx + 122, my + 13, hdr, mw - 140, 0x38BDF8);
        int cc = am_country_count();
        for (int slot = 0; slot < 12; slot++) {
            int idx = g_pick_c_page * 12 + slot; if (idx >= cc) break;
            int r = slot / 3, c = slot % 3, bx = mx + 20 + c * 196, by = my + 58 + r * 74;
            int sel = (idx == g_pick_c_sel);
            const char *code = am_country_code(idx);
            int tmp[64], n_ap = am_airports_in_country(code, tmp, 64);
            ui_card(bx, by, 184, 62, 10, sel ? 0x1D4ED8 : 0x16263D, 0x334155, 0x050910, sel, 0xFEF08A);
            hal_set_text_bg(sel ? 0x1D4ED8 : 0x16263D);
            char ln1[48], ln2[32];
            snprintf(ln1, sizeof(ln1), "%s (%s)", am_country_name_zh(code), code);
            snprintf(ln2, sizeof(ln2), "%d 座機場", n_ap);
            ui_draw_str(bx + 12, by + 10, ln1, 162, 0xFFFFFF);
            ui_draw_str(bx + 12, by + 36, ln2, 162, sel ? 0xFEF08A : 0x94A3B8);
        }
        ui_fill_rrect(mx + 20, my + mh - 42, 140, 32, 8, 0x1E293B); hal_set_text_bg(0x1E293B); ui_draw_str(mx + 50, my + mh - 35, "< 上一頁", 90, 0xFFFFFF);
        ui_fill_rrect(mx + mw - 160, my + mh - 42, 140, 32, 8, 0x1E293B); ui_draw_str(mx + mw - 130, my + mh - 35, "下一頁 >", 90, 0xFFFFFF);
    } else {
        const char *cc = am_country_code(g_pick_c_sel);
        char hdr[96]; snprintf(hdr, sizeof(hdr), "步驟 2/2：%s (%s) 城市與機場 (%d座)", am_country_name_zh(cc), cc, g_pick_ap_cnt);
        ui_draw_str(mx + 122, my + 13, hdr, mw - 140, 0x38BDF8);
        if (g_pick_ap_sel < g_pick_ap_scroll) g_pick_ap_scroll = g_pick_ap_sel;
        if (g_pick_ap_sel >= g_pick_ap_scroll + 6) g_pick_ap_scroll = g_pick_ap_sel - 5;
        for (int r = 0; r < 6; r++) {
            int idx = g_pick_ap_scroll + r; if (idx >= g_pick_ap_cnt) break;
            const AirportInfo *ap = am_airport_get(g_pick_ap_list[idx]);
            int ry = my + 58 + r * 50, sel = (idx == g_pick_ap_sel);
            uint32_t bg = sel ? 0x1D4ED8 : 0x16263D;
            ui_fill_rrect(mx + 20, ry, mw - 40, 42, 9, bg); hal_set_text_bg(bg);
            char ln[128];
            snprintf(ln, sizeof(ln), "[%s] %s · 跑道:%uft · 市場:%u%% · 需求 Y%u/J%u/F%u",
                     ap->iata, ap->name, (unsigned)ap->runway_ft, (unsigned)ap->market_pct,
                     (unsigned)ap->dem_y, (unsigned)ap->dem_j, (unsigned)ap->dem_f);
            ui_draw_str(mx + 32, ry + 12, ln, mw - 64, 0xFFFFFF);
        }
        ui_fill_rrect(mx + 20, my + mh - 42, 140, 32, 8, 0x1E293B); hal_set_text_bg(0x1E293B); ui_draw_str(mx + 50, my + mh - 35, "< 上捲", 90, 0xFFFFFF);
        ui_fill_rrect(mx + mw - 160, my + mh - 42, 140, 32, 8, 0x1E293B); ui_draw_str(mx + mw - 130, my + mh - 35, "下捲 >", 90, 0xFFFFFF);
    }
}

/* 完整機型百科介紹 + 藍圖剪影 + 購買確認頁 */
static void render_shop_detail(int sw, int sh) {
    int mw = 600, mh = 390, mx = (sw - mw) / 2, my = (sh - mh) / 2;
    hal_clear_text_in_rect(mx - 8, my - 8, mw + 16, mh + 16); hal_dim_rect(0, 0, sw, sh);
    ui_card(mx, my, mw, mh, 14, 0x0F1A2B, 0x38BDF8, 0x050910, 1, 0x38BDF8);
    const AircraftModel *m = am_aircraft_get(g_shop_detail_model);

    /* 上半部：左側藍圖側面剪影 + 右側詳細規格表 */
    ui_draw_aircraft_silhouette(mx + 148, my + 86, 256, 132, m->silhouette, 0xE2E8F0, 0x38BDF8);

    hal_set_text_bg(0x0F1A2B);
    char pr[24], s1[64], s2[64], s3[64], s4[64];
    fmt_money(m->price, pr, sizeof(pr));
    ui_draw_str(mx + 292, my + 20, m->model, 290, 0x38BDF8);
    snprintf(s1, sizeof(s1), "售價：%s  (%s)", pr, m->is_cargo ? "全貨機" : "商用客機");
    snprintf(s2, sizeof(s2), "航程：%u km  ·  巡航：%u km/h", (unsigned)m->range_km, (unsigned)m->speed_kmh);
    snprintf(s3, sizeof(s3), "%s：%u  ·  跑道需求：%u ft", m->is_cargo ? "最大載貨(噸)" : "標準載客", (unsigned)m->cap, (unsigned)m->runway_ft);
    snprintf(s4, sizeof(s4), "油耗：%u lbs/km · CO2：%u · A檢：%uh", (unsigned)m->fuel_lbs_km, (unsigned)m->co2_kg, (unsigned)m->acheck_hrs);
    ui_draw_str(mx + 292, my + 50,  s1, 290, 0xFACC15);
    ui_draw_str(mx + 292, my + 78,  s2, 290, 0xF8FAFC);
    ui_draw_str(mx + 292, my + 106, s3, 290, 0xF8FAFC);
    ui_draw_str(mx + 292, my + 134, s4, 290, 0x94A3B8);

    /* 中段：機型中文百科介紹 (自動折行顯示) */
    ui_card(mx + 20, my + 168, mw - 40, 140, 10, 0x16263D, 0x1E3A5F, 0x050910, 0, 0);
    hal_set_text_bg(0x16263D);
    ui_draw_str(mx + 34, my + 180, "【機型百科介紹與營運定位】", 260, 0x38BDF8);
    /* 將中文介紹分兩段優雅印出避免截斷 */
    const char *d = m->desc;
    size_t dlen = strlen(d);
    size_t split = 0, u = 0;
    while (split < dlen && u < 54) {
        unsigned char c = (unsigned char)d[split];
        if ((c & 0x80) == 0) { split += 1; u += 1; }
        else if ((c & 0xF0) == 0xE0) { split += 3; u += 2; }
        else { split += 1; u += 1; }
    }
    hal_draw_line_utf8(mx + 34, my + 212, d, (int)split, mw - 68, 0xF8FAFC);
    if (split < dlen) hal_draw_line_utf8(mx + 34, my + 242, d + split, (int)(dlen - split), mw - 68, 0xF8FAFC);
    if (g_toast[0]) ui_draw_str(mx + 34, my + 276, g_toast, mw - 68, 0xF87171);

    /* 底部購買與返回按鈕 */
    uint32_t bbuy = (g_am.cash >= m->price) ? 0x059669 : 0x334155;
    ui_fill_rrect(mx + 20, my + mh - 56, 270, 42, 10, bbuy); hal_set_text_bg(bbuy);
    ui_draw_str(mx + 82, my + mh - 44, "確認購買加入機隊 [Enter]", 180, 0xFFFFFF);
    ui_fill_rrect(mx + 310, my + mh - 56, 270, 42, 10, 0x334155); hal_set_text_bg(0x334155);
    ui_draw_str(mx + 398, my + mh - 44, "返回機型清單", 120, 0xFFFFFF);
}

static void render_route_modal(int sw, int sh) {
    int mw = 560, mh = 350, mx = (sw - mw) / 2, my = (sh - mh) / 2;
    hal_clear_text_in_rect(mx - 8, my - 8, mw + 16, mh + 16); hal_dim_rect(0, 0, sw, sh);
    ui_card(mx, my, mw, mh, 14, 0x111C2E, 0x38BDF8, 0x050910, 1, 0x38BDF8); hal_set_text_bg(0x111C2E);
    const FleetPlane *fp = &g_am.fleet[g_rt_fleet_idx]; const AircraftModel *m = am_aircraft_get(fp->model_idx);
    const AirportInfo *ao = am_airport_get(g_am.hubs[g_rt_orig_hub_slot % g_am.hub_count]);
    const AirportInfo *ad = am_airport_get(g_rt_dest_ap);
    char hdr[96]; snprintf(hdr, sizeof(hdr), "航線規劃：%s (%s · 航程 %ukm)", fp->reg, m->model, (unsigned)m->range_km);
    ui_draw_str(mx + 20, my + 16, hdr, mw - 40, 0x38BDF8);

    char r0[96], r1[96], r2[96];
    snprintf(r0, sizeof(r0), "出發樞紐：< %s - %s >", ao->iata, ao->name);
    snprintf(r1, sizeof(r1), "目的機場：[%s] %s - %s (點此按國家選 >)", am_country_name_zh(ad->country), ad->iata, ad->name);
    if (g_rt_stop_ap >= 0) snprintf(r2, sizeof(r2), "中轉機場：%s - %s (點重選)", am_airport_get(g_rt_stop_ap)->iata, am_airport_get(g_rt_stop_ap)->name);
    else snprintf(r2, sizeof(r2), "中轉機場：直飛無中轉 (點此依國家選中轉 >)");

    const char *rows[3] = { r0, r1, r2 };
    for (int i = 0; i < 3; i++) {
        int ry = my + 56 + i * 50; uint32_t bg = (g_rt_row == i) ? 0x1D4ED8 : 0x1E293B;
        ui_fill_rrect(mx + 20, ry, (i == 2 && g_rt_stop_ap >= 0) ? (mw - 134) : (mw - 40), 40, 9, bg);
        hal_set_text_bg(bg); ui_draw_str(mx + 32, ry + 11, rows[i], mw - 144, 0xFFFFFF);
    }
    if (g_rt_stop_ap >= 0) {
        ui_fill_rrect(mx + mw - 106, my + 156, 86, 40, 9, 0xDC2626); hal_set_text_bg(0xDC2626);
        ui_draw_str(mx + mw - 94, my + 167, "改直飛", 64, 0xFFFFFF);
    }

    int orig_ap = g_am.hubs[g_rt_orig_hub_slot % g_am.hub_count];
    int dist = (g_rt_stop_ap >= 0) ? (am_calc_distance_km(orig_ap, g_rt_stop_ap) + am_calc_distance_km(g_rt_stop_ap, g_rt_dest_ap)) : am_calc_distance_km(orig_ap, g_rt_dest_ap);
    char info[120]; snprintf(info, sizeof(info), "大圓距離：%d km · 跑道：%uft/%uft · 經濟建議票價：$%d", dist, (unsigned)ao->runway_ft, (unsigned)ad->runway_ft, am_calc_auto_ticket(dist, 0, g_am.realism_mode));
    hal_set_text_bg(0x111C2E); ui_draw_str(mx + 22, my + 220, info, mw - 44, (dist > m->range_km && g_rt_stop_ap < 0) ? 0xF87171 : 0x4ADE80);
    if (g_toast[0]) ui_draw_str(mx + 22, my + 248, g_toast, mw - 44, 0xFACC15);

    ui_fill_rrect(mx + 20, my + 286, 250, 44, 10, 0x059669); hal_set_text_bg(0x059669); ui_draw_str(mx + 88, my + 299, "確認開通航線", 130, 0xFFFFFF);
    ui_fill_rrect(mx + 290, my + 286, 250, 44, 10, 0x334155); hal_set_text_bg(0x334155); ui_draw_str(mx + 388, my + 299, "取消", 60, 0xFFFFFF);
}

static void render_plane_modal(int sw, int sh) {
    int mw = 480, mh = 380, mx = (sw - mw) / 2, my = (sh - mh) / 2;
    hal_clear_text_in_rect(mx - 8, my - 8, mw + 16, mh + 16); hal_dim_rect(0, 0, sw, sh);
    ui_card(mx, my, mw, mh, 14, 0x111C2E, 0xFACC15, 0x050910, 1, 0xFACC15);
    const FleetPlane *fp = &g_am.fleet[g_pl_fleet_idx]; const AircraftModel *m = am_aircraft_get(fp->model_idx);
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
        ui_card(px, py, pw, ph, 16, 0x0F1A2B, 0x38BDF8, 0x050910, 1, 0x38BDF8); hal_set_text_bg(0x0F1A2B);
        ui_draw_str(px + 80, py + 28, "AIRLINE MANAGER v2 旗艦版", 320, 0x38BDF8);
        ui_draw_str(px + 42, py + 64, "國家->城市選機場 · 121機型百科藍圖 · 零閃爍雷達", 400, 0xCBD5E1);
        ui_draw_str(px + 54, py + 94, "無規律油價/CO2 · 銀行貸款 · 16成就 · 離線掛機", 380, 0x94A3B8);

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

    if (g_scr == SCR_CREATE || (g_scr == SCR_AP_PICKER && g_pick_return == SCR_CREATE)) {
        ui_draw_world_map(0, 0, sw, sh);
        int pw = 580, ph = 424, px = (sw - pw) / 2, py = 24;
        ui_card(px, py, pw, ph, 16, 0x0F1A2B, 0x38BDF8, 0x050910, 1, 0x38BDF8); hal_set_text_bg(0x0F1A2B);
        ui_draw_str(px + 174, py + 14, "創立你的航空公司 (完整自訂)", 260, 0x38BDF8);

        /* Row 0: 公司名稱 + 獨立 [退格] [清空] 觸控鈕 */
        uint32_t b0 = (g_create_row == 0) ? 0x1D4ED8 : 0x1E293B;
        ui_fill_rrect(px + 22, py + 48, pw - 196, 38, 9, b0); hal_set_text_bg(b0);
        char r0[96]; snprintf(r0, sizeof(r0), "公司名：%s_", g_new_name);
        ui_draw_str(px + 34, py + 58, r0, pw - 216, 0xFFFFFF);
        ui_fill_rrect(px + pw - 166, py + 48, 76, 38, 8, 0x475569); hal_set_text_bg(0x475569); ui_draw_str(px + pw - 156, py + 58, "退格←", 60, 0xFFFFFF);
        ui_fill_rrect(px + pw - 84,  py + 48, 62, 38, 8, 0xDC2626); hal_set_text_bg(0xDC2626); ui_draw_str(px + pw - 74,  py + 58, "清空", 44, 0xFFFFFF);

        const char *pn[4] = { "Starlux Sim", "Formosa Air", "Sydney Jet", "Pacific Sky" };
        for (int i = 0; i < 4; i++) {
            int bx = px + 22 + i * 134;
            ui_fill_rrect(bx, py + 94, 126, 28, 7, 0x16263D); ui_draw_rrect(bx, py + 94, 126, 28, 7, 0x334155);
            hal_set_text_bg(0x16263D); ui_draw_str(bx + 10, py + 99, pn[i], 110, 0xCBD5E1);
        }

        /* Row 1: 2碼 IATA + [退格] [清空] */
        uint32_t b1 = (g_create_row == 1) ? 0x1D4ED8 : 0x1E293B;
        ui_fill_rrect(px + 22, py + 134, pw - 196, 38, 9, b1); hal_set_text_bg(b1);
        char r1[64]; snprintf(r1, sizeof(r1), "2碼 IATA 代碼：%s_", g_new_iata);
        ui_draw_str(px + 34, py + 144, r1, pw - 216, 0xFFFFFF);
        ui_fill_rrect(px + pw - 166, py + 134, 76, 38, 8, 0x475569); hal_set_text_bg(0x475569); ui_draw_str(px + pw - 156, py + 144, "退格←", 60, 0xFFFFFF);
        ui_fill_rrect(px + pw - 84,  py + 134, 62, 38, 8, 0xDC2626); hal_set_text_bg(0xDC2626); ui_draw_str(px + pw - 74,  py + 144, "清空", 44, 0xFFFFFF);

        /* Row 2: 難度 */
        uint32_t b2 = (g_create_row == 2) ? 0x1D4ED8 : 0x1E293B;
        ui_fill_rrect(px + 22, py + 184, pw - 44, 38, 9, b2); hal_set_text_bg(b2);
        char r2[80]; snprintf(r2, sizeof(r2), "模式：< %s >", g_new_realism ? "Realism 擬真模式 (嚴格跑道)" : "Easy 輕鬆模式 (免跑道限制·高資金)");
        ui_draw_str(px + 34, py + 194, r2, pw - 68, 0xFFFFFF);

        /* Row 3: 起始樞紐 (點擊直接進入 國家->城市 選擇器！) */
        const AirportInfo *ap = am_airport_get(g_new_hub_idx);
        uint32_t b3 = (g_create_row == 3) ? 0x1D4ED8 : 0x1E293B;
        ui_fill_rrect(px + 22, py + 234, pw - 44, 42, 9, b3); hal_set_text_bg(b3);
        char r3[96]; snprintf(r3, sizeof(r3), "起始樞紐：[%s] %s - %s (點此按國家選 >)", am_country_name_zh(ap->country), ap->iata, ap->name);
        ui_draw_str(px + 34, py + 246, r3, pw - 68, 0xFFFFFF);

        const char *phubs[6] = { "TPE", "SYD", "HND", "SIN", "LHR", "JFK" };
        for (int i = 0; i < 6; i++) {
            int bx = px + 22 + i * 90;
            ui_fill_rrect(bx, py + 286, 82, 30, 7, 0x16263D); ui_draw_rrect(bx, py + 286, 82, 30, 7, 0x38BDF8);
            hal_set_text_bg(0x16263D); ui_draw_str(bx + 24, py + 292, phubs[i], 44, 0x38BDF8);
        }

        uint32_t b4 = (g_create_row == 4) ? 0x059669 : 0x10B981;
        ui_fill_rrect(px + 64, py + 346, pw - 128, 48, 12, b4); hal_set_text_bg(b4);
        ui_draw_str(px + 168, py + 361, ">>>  成立航空公司，正式啟航！  <<<", 280, 0xFFFFFF);

        if (g_scr == SCR_AP_PICKER) render_airport_picker(sw, sh);
        hal_present(); return;
    }

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
    else if (g_scr == SCR_SHOP_DETAIL) render_shop_detail(sw, sh);
    else if (g_scr == SCR_AP_PICKER) render_airport_picker(sw, sh);
    else if (g_scr == SCR_PAUSE) {
        int pw = 340, ph = 220, px = (sw - pw) / 2, py = (sh - ph) / 2;
        hal_clear_text_in_rect(px - 8, py - 8, pw + 16, ph + 16); hal_dim_rect(0, 0, sw, sh);
        ui_card(px, py, pw, ph, 14, 0x0F1A2B, 0x38BDF8, 0x050910, 1, 0x38BDF8); hal_set_text_bg(0x0F1A2B);
        ui_draw_str(px + 116, py + 16, "系統暫停選單", 140, 0xFFFFFF);
        const char *po[3] = { "繼續經營 (ESC)", "存檔並回主畫面", "存檔並離開" };
        for (int i = 0; i < 3; i++) {
            uint32_t bg = (i == g_pause_sel) ? 0x2563EB : 0x1E293B;
            ui_fill_rrect(px + 28, py + 56 + i * 48, pw - 56, 38, 8, bg);
            hal_set_text_bg(bg); ui_draw_str(px + 84, py + 66 + i * 48, po[i], 180, 0xFFFFFF);
        }
    }

    hal_present();
}
