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
    SCR_SLOTS,        /* 多重自訂名稱存檔管理器 (8 槽自由選取/另存/刪除) */
    SCR_CREATE,       /* 創立公司 + 自訂存檔名稱 */
    SCR_STARTER_AC,
    SCR_MAIN,
    SCR_AP_PICKER,    /* 七大洲 -> 203國 -> 機場 (也可於 MOD 選目標國家) */
    SCR_SHOP_DETAIL,  /* 機型百科 + 批量購買 (x1/x5/x10/x25) + 員工檢查 + 客艙配座 */
    SCR_PLANE_MODAL,
    SCR_CHEAT,        /* 2 頁史詩級 MOD MENU (精準改錢 + 指定機型全國/全球開航!) */
    SCR_PAUSE
} AmScreen;

typedef enum {
    TAB_MAP = 0,
    TAB_FLEET,
    TAB_SHOP,
    TAB_FUEL,
    TAB_COMPANY,
    TAB_FINANCE,
    TAB_BONUS
} MainTab;

typedef enum { DRW_CLOSED = 0, DRW_PLANE_CTRL, DRW_ROUTE_BUILD } MapDrawerMode;

typedef enum {
    PICK_FOR_CREATE_HUB = 0,
    PICK_FOR_ROUTE_DEST,
    PICK_FOR_ROUTE_STOP,
    PICK_FOR_BUY_HUB,
    PICK_FOR_MOD_COUNTRY
} PickerPurpose;

/* 遊戲化航空大亨質感配色 (清晰高對比、無雜亂感) */
#define COL_BG        0xE6EEF6
#define COL_WHITE     0xFFFFFF
#define COL_PANEL     0xF8FAFC
#define COL_BORDER    0xCBD5E1
#define COL_SHADOW    0xB8C7D9
#define COL_HUD       0x0F1E36
#define COL_HUD_BTN   0x1E3354
#define COL_TEXT      0x0F172A
#define COL_MUTED     0x64748B
#define COL_BLUE      0x2563EB
#define COL_CYAN      0x0284C7
#define COL_GREEN     0x16A34A
#define COL_AMBER     0xD97706
#define COL_RED       0xDC2626
#define COL_PURPLE    0x7C3AED

static AmScreen      g_scr = SCR_TITLE;
static MainTab       g_tab = TAB_MAP;
static MapDrawerMode g_drawer = DRW_PLANE_CTRL;
static int g_redraw = 1;
static int g_title_sel = 0, g_pause_sel = 0;

/* 多存檔槽介面狀態 */
static int  g_slot_mode = 0; /* 0=讀取存檔, 1=另存新檔 */
static int  g_slot_sel  = 0;
static char g_save_input[28] = "My Empire";

/* 創公司 */
static char    g_new_save[28] = "Empire 1";
static char    g_new_name[28] = "Starlux Sim";
static char    g_new_iata[4]  = "JX";
static int     g_new_realism  = 0;
static int     g_new_hub_idx  = 0;
static int64_t g_new_cash     = 1800000LL;
static int     g_create_row   = 0; /* 0=SaveName, 1=AirlineName, 2=IATA, 3=Cash/Mode, 4=Hub */
static int     g_starter_sel  = 0;

/* 七大洲 -> 203國 -> 機場 選擇器 */
static PickerPurpose g_pick_purpose = PICK_FOR_CREATE_HUB;
static AmScreen      g_pick_return  = SCR_CREATE;
static int g_pick_step = 0, g_pick_cont = 0;
static int g_pick_c_list[80], g_pick_c_cnt = 0, g_pick_c_page = 0, g_pick_c_sel = 0;
static int g_pick_ap_list[128], g_pick_ap_cnt = 0, g_pick_ap_scroll = 0, g_pick_ap_sel = 0;

/* 地圖縮放與拖拉平移 */
static int g_map_zoom = 1, g_map_pan_x = 0, g_map_pan_y = 0;
static int g_sel_plane = 0;
static int g_rt_orig_hub_slot = 0, g_rt_dest_ap = 5, g_rt_stop_ap = -1;
static int g_rt_py = 500, g_rt_pj = 1200, g_rt_pf = 2200, g_rt_ci = 150, g_rt_auto_ret = 1;

/* 各分頁與批量購買 / 自訂油量狀態 */
static int g_fleet_scroll = 0;
static int g_shop_mfg = 0, g_shop_sel = 0, g_shop_scroll = 0, g_shop_detail_model = 0;
static int g_buy_qty = 1; /* 批量購買數量: 1, 5, 10, 25, 50 */
static int g_cfg_y = 120, g_cfg_j = 18, g_cfg_f = 4;
static int g_fuel_amt_idx = 1; /* 0=100K, 1=500K, 2=1M, 3=5M, 4=補滿 */
static int g_co2_amt_idx  = 1;
static int g_comp_subtab = 0, g_ops_hub_ap = 42;
static int g_fin_subtab = 0, g_stock_sel = 0, g_stock_scroll = 0;
static int g_ach_scroll = 0;
static int g_pl_fleet_idx = 0, g_pl_sel = 0;

/* 2 頁 MOD MENU 狀態 (精準改錢 + 指定機型全國/全球開航) */
static int  g_mod_page = 0; /* 0=資金與上帝模式, 1=指定機型全國/全球一鍵開航 */
static char g_mod_cash_str[16] = "50000000";
static int  g_mod_model_idx = 24; /* 預設 A350-900 */
static int  g_mod_country_idx = 0; /* 預設 TW */

static char g_toast[112] = {0};
static int  g_toast_sec  = 0;

static void set_toast(const char *s) { snprintf(g_toast, sizeof(g_toast), "%.111s", s ? s : ""); g_toast_sec = 5; }

static void fmt_money(int64_t val, char *out, size_t sz) {
    int neg = (val < 0); int64_t a = neg ? -val : val;
    if (a >= 1000000000LL) snprintf(out, sz, "%s$%.2fB", neg ? "-" : "", (double)a / 1e9);
    else if (a >= 1000000LL) snprintf(out, sz, "%s$%.2fM", neg ? "-" : "", (double)a / 1e6);
    else if (a >= 1000LL) snprintf(out, sz, "%s$%.1fK", neg ? "-" : "", (double)a / 1e3);
    else snprintf(out, sz, "%s$%lld", neg ? "-" : "", (long long)a);
}

static int find_free_slot(void) {
    for (int i = 0; i < AM_MAX_SAVE_SLOTS; i++) if (!g_slots.slots[i].used) return i;
    return 0;
}
static int any_slot_used(void) {
    for (int i = 0; i < AM_MAX_SAVE_SLOTS; i++) if (g_slots.slots[i].used) return 1;
    return 0;
}

static int find_model_by_substr(const char *sub, int fallback) {
    int n = am_aircraft_count();
    for (int i = 0; i < n; i++) if (strstr(am_aircraft_get(i)->model, sub)) return i;
    return fallback;
}

static int build_shop_list(int *out_idxs, int max_n) {
    int n = am_aircraft_count(), cnt = 0;
    for (int i = 0; i < n && cnt < max_n; i++) {
        const AircraftModel *m = am_aircraft_get(i);
        if (g_shop_mfg == 0 || (int)m->mfg == (g_shop_mfg - 1)) out_idxs[cnt++] = i;
    }
    return cnt;
}

static void init_default_seats_for_model(int model_idx) {
    const AircraftModel *m = am_aircraft_get(model_idx);
    if (m->is_cargo) { g_cfg_y = m->cap; g_cfg_j = 0; g_cfg_f = 0; }
    else {
        g_cfg_f = (m->cap >= 200) ? (m->cap / 18) : 0;
        g_cfg_j = (m->cap >= 80)  ? (m->cap / 9)  : 0;
        g_cfg_y = m->cap - g_cfg_j * 2 - g_cfg_f * 3;
        if (g_cfg_y < 1) g_cfg_y = m->cap;
    }
    g_buy_qty = 1;
}

static void adjust_seat_config(int cap, int cabin, int delta) {
    int y = g_cfg_y, j = g_cfg_j, f = g_cfg_f;
    if (cabin == 1) { j += delta; if (j < 0) j = 0; }
    else if (cabin == 2) { f += delta; if (f < 0) f = 0; }
    else { y += delta; if (y < 0) y = 0; }
    int used_jf = j * 2 + f * 3;
    if (used_jf > cap) return;
    if (cabin != 0) y = cap - used_jf;
    if (y + used_jf > cap) y = cap - used_jf;
    if (y < 0) return;
    g_cfg_y = y; g_cfg_j = j; g_cfg_f = f;
}

static void clamp_map_pan(void) {
    if (g_map_zoom < 1) g_map_zoom = 1;
    if (g_map_zoom > 3) g_map_zoom = 3;
    int src_w = 360 / g_map_zoom, src_h = 190 / g_map_zoom;
    if (g_map_pan_x < 0) g_map_pan_x = 0;
    if (g_map_pan_y < 0) g_map_pan_y = 0;
    if (g_map_pan_x + src_w > 360) g_map_pan_x = 360 - src_w;
    if (g_map_pan_y + src_h > 190) g_map_pan_y = 190 - src_h;
}

/* 真正的觸控拖拉地圖平移！ */
void am_ui_on_touch_drag(int dx, int dy) {
    if (g_scr != SCR_MAIN || g_tab != TAB_MAP) return;
    if (g_map_zoom == 1) g_map_zoom = 2; /* 1x 拖拉時自動微放大進入平滑瀏覽 */
    g_map_pan_x -= dx / 2;
    g_map_pan_y -= dy / 2;
    clamp_map_pan();
    g_redraw = 1;
}

static void picker_load_continent(int cont_id) {
    if (cont_id < 0 || cont_id >= AM_NUM_CONTINENTS) cont_id = 0;
    g_pick_cont = cont_id;
    g_pick_c_cnt = am_countries_in_continent(cont_id, g_pick_c_list, 80);
    g_pick_c_page = 0; g_pick_c_sel = 0; g_pick_step = 0;
}

static void open_airport_picker(PickerPurpose purpose, AmScreen ret_scr) {
    g_pick_purpose = purpose; g_pick_return = ret_scr;
    picker_load_continent(g_pick_cont);
    g_scr = SCR_AP_PICKER;
}

static void picker_enter_country(int list_pos) {
    if (list_pos < 0 || list_pos >= g_pick_c_cnt) return;
    g_pick_c_sel = list_pos;
    int c_idx = g_pick_c_list[list_pos];
    /* 若是 MOD 選目標國家，選完國家直接返回 MOD MENU！ */
    if (g_pick_purpose == PICK_FOR_MOD_COUNTRY) {
        g_mod_country_idx = c_idx;
        g_scr = g_pick_return;
        return;
    }
    g_pick_ap_cnt = am_airports_in_country(am_country_code(c_idx), g_pick_ap_list, 128);
    g_pick_ap_sel = 0; g_pick_ap_scroll = 0; g_pick_step = 1;
}

static void sync_route_autoprice(void) {
    if (g_am.fleet_count <= 0) return;
    if (g_sel_plane >= g_am.fleet_count) g_sel_plane = 0;
    int orig = g_am.hubs[g_rt_orig_hub_slot % g_am.hub_count];
    int dist = (g_rt_stop_ap >= 0) ? (am_calc_distance_km(orig, g_rt_stop_ap) + am_calc_distance_km(g_rt_stop_ap, g_rt_dest_ap))
                                   : am_calc_distance_km(orig, g_rt_dest_ap);
    const AircraftModel *m = am_aircraft_get(g_am.fleet[g_sel_plane].model_idx);
    g_rt_py = am_calc_auto_ticket(dist, m->is_cargo ? 3 : 0, g_am.realism_mode);
    g_rt_pj = am_calc_auto_ticket(dist, 1, g_am.realism_mode);
    g_rt_pf = am_calc_auto_ticket(dist, 2, g_am.realism_mode);
}

static void picker_commit_airport(int ap_idx) {
    if (g_pick_purpose == PICK_FOR_CREATE_HUB) g_new_hub_idx = ap_idx;
    else if (g_pick_purpose == PICK_FOR_ROUTE_DEST) { g_rt_dest_ap = ap_idx; sync_route_autoprice(); }
    else if (g_pick_purpose == PICK_FOR_ROUTE_STOP) { g_rt_stop_ap = ap_idx; sync_route_autoprice(); }
    else if (g_pick_purpose == PICK_FOR_BUY_HUB) g_ops_hub_ap = ap_idx;
    g_scr = g_pick_return;
}

static void enter_live_route_drawer(int fleet_idx) {
    if (fleet_idx < 0 || fleet_idx >= g_am.fleet_count) return;
    g_sel_plane = fleet_idx; g_rt_orig_hub_slot = 0;
    const FleetPlane *fp = &g_am.fleet[fleet_idx];
    if (fp->has_route && fp->dest_ap >= 0) {
        g_rt_dest_ap = fp->dest_ap; g_rt_stop_ap = fp->stopover_ap;
        g_rt_py = fp->price_y; g_rt_pj = fp->price_j; g_rt_pf = fp->price_f;
        g_rt_ci = fp->cost_index; g_rt_auto_ret = fp->auto_return;
    } else {
        int sin = am_airport_find_iata("SIN");
        g_rt_dest_ap = (sin >= 0 && sin != g_am.hubs[0]) ? sin : ((g_am.hubs[0] + 5) % am_airport_count());
        g_rt_stop_ap = -1; g_rt_ci = 150; g_rt_auto_ret = 1;
        sync_route_autoprice();
    }
    g_tab = TAB_MAP; g_drawer = DRW_ROUTE_BUILD; g_scr = SCR_MAIN;
}

static void open_plane_modal(int fleet_idx) {
    if (fleet_idx < 0 || fleet_idx >= g_am.fleet_count) return;
    g_pl_fleet_idx = fleet_idx; g_pl_sel = 0;
    g_cfg_y = g_am.fleet[fleet_idx].seats_y; g_cfg_j = g_am.fleet[fleet_idx].seats_j; g_cfg_f = g_am.fleet[fleet_idx].seats_f;
    g_scr = SCR_PLANE_MODAL;
}

void am_ui_init(void) {
    am_sim_init();
    g_scr = SCR_TITLE; g_tab = TAB_MAP; g_drawer = DRW_PLANE_CTRL; g_title_sel = 0;
    int tpe = am_airport_find_iata("TPE"); g_new_hub_idx = (tpe >= 0) ? tpe : 0;
    int syd = am_airport_find_iata("SYD"); g_ops_hub_ap  = (syd >= 0) ? syd : 0;
    g_mod_model_idx = find_model_by_substr("A350-900", 23);
    g_redraw = 1;
}

void am_ui_shutdown(void) { am_sim_save(); }
int  am_ui_needs_redraw(void) { return g_redraw; }

void am_ui_tick_second(void) {
    if (g_toast_sec > 0) { g_toast_sec--; if (g_toast_sec == 0) { g_toast[0] = '\0'; g_redraw = 1; } }
    if (g_scr == SCR_MAIN) {
        int major = am_sim_tick(1, 1);
        if (major) {
            if (g_am.last_notice[0]) set_toast(g_am.last_notice);
            g_redraw = 1;
        }
    }
}

static void edit_buf(char *buf, size_t cap, AmKey key, char raw_char, int digits_only) {
    if (key == AKEY_BACKSPACE) { size_t n = strlen(buf); if (n > 0) buf[n - 1] = '\0'; }
    else if (digits_only ? isdigit((unsigned char)raw_char) : (raw_char >= 0x20 && raw_char < 0x7F)) {
        size_t n = strlen(buf);
        if (n + 1 < cap) { buf[n] = raw_char; buf[n + 1] = '\0'; }
    }
}

void am_ui_on_key(AmKey key, char raw_char) {
    g_redraw = 1;
    int is_ok = (key == AKEY_OK || key == AKEY_SPACE);

    if (g_scr == SCR_TITLE) {
        int hs = any_slot_used(), n = hs ? 3 : 2;
        if (key == AKEY_UP) g_title_sel = (g_title_sel + n - 1) % n;
        else if (key == AKEY_DOWN) g_title_sel = (g_title_sel + 1) % n;
        else if (key == AKEY_BACK) hal_request_quit();
        else if (is_ok) {
            if (hs && g_title_sel == 0) { am_slots_refresh(); g_slot_mode = 0; g_slot_sel = 0; g_scr = SCR_SLOTS; }
            else if (g_title_sel == (hs ? 1 : 0)) {
                g_slot_sel = find_free_slot();
                snprintf(g_new_save, sizeof(g_new_save), "Save %d", g_slot_sel + 1);
                g_create_row = 0; g_scr = SCR_CREATE;
            } else hal_request_quit();
        }
        return;
    }

    if (g_scr == SCR_SLOTS) {
        if (key == AKEY_BACK) { g_scr = (g_slot_mode == 1) ? SCR_MAIN : SCR_TITLE; return; }
        if (g_slot_mode == 1 && (key == AKEY_BACKSPACE || key == AKEY_CHAR || key == AKEY_SPACE)) {
            edit_buf(g_save_input, sizeof(g_save_input), key, key == AKEY_SPACE ? ' ' : raw_char, 0);
            return;
        }
        if (key == AKEY_UP && g_slot_sel > 0) g_slot_sel--;
        else if (key == AKEY_DOWN && g_slot_sel + 1 < AM_MAX_SAVE_SLOTS) g_slot_sel++;
        else if (key == AKEY_OK) {
            if (g_slot_mode == 0) {
                if (g_slots.slots[g_slot_sel].used && am_sim_load_slot(g_slot_sel)) {
                    g_scr = SCR_MAIN; g_tab = TAB_MAP; g_drawer = DRW_PLANE_CTRL;
                    set_toast(g_am.last_notice);
                }
            } else {
                am_sim_save_slot(g_slot_sel, g_save_input);
                set_toast("已成功儲存至指定存檔槽！");
                g_scr = SCR_MAIN;
            }
        }
        return;
    }

    if (g_scr == SCR_STARTER_AC) {
        if (key == AKEY_BACK) { g_scr = SCR_CREATE; return; }
        if (key == AKEY_LEFT || key == AKEY_RIGHT) g_starter_sel = 1 - g_starter_sel;
        else if (is_ok) {
            int m_id = (g_starter_sel == 0) ? find_model_by_substr("A320-200", 9) : find_model_by_substr("737-800", 36);
            am_sim_new_company(g_slot_sel, g_new_save, g_new_name, g_new_iata, g_new_hub_idx, g_new_realism, g_new_cash, m_id);
            g_sel_plane = 0; g_scr = SCR_MAIN; g_tab = TAB_MAP; g_drawer = DRW_PLANE_CTRL;
            set_toast(g_am.last_notice);
        }
        return;
    }

    if (g_scr == SCR_CHEAT) {
        if (key == AKEY_BACK) { g_scr = SCR_MAIN; return; }
        if (g_mod_page == 0 && (key == AKEY_BACKSPACE || key == AKEY_CHAR)) {
            edit_buf(g_mod_cash_str, sizeof(g_mod_cash_str), key, raw_char, 1);
            return;
        }
        if (g_mod_page == 0 && key == AKEY_OK) {
            char m[112]; int64_t v = atoll(g_mod_cash_str);
            am_cheat_set_cash(v, m, sizeof(m)); set_toast(m);
        }
        return;
    }

    if (g_scr == SCR_AP_PICKER) {
        if (g_pick_step == 0) {
            int per_pg = 12;
            if (key == AKEY_BACK) { g_scr = g_pick_return; return; }
            if (key == AKEY_LEFT && g_pick_c_sel > 0) g_pick_c_sel--;
            else if (key == AKEY_RIGHT && g_pick_c_sel + 1 < g_pick_c_cnt) g_pick_c_sel++;
            else if (key == AKEY_UP && g_pick_c_sel >= 3) g_pick_c_sel -= 3;
            else if (key == AKEY_DOWN && g_pick_c_sel + 3 < g_pick_c_cnt) g_pick_c_sel += 3;
            else if (key == AKEY_PGUP) g_pick_c_sel = (g_pick_c_sel >= per_pg) ? (g_pick_c_sel - per_pg) : 0;
            else if (key == AKEY_PGDN) { g_pick_c_sel += per_pg; if (g_pick_c_sel >= g_pick_c_cnt) g_pick_c_sel = g_pick_c_cnt > 0 ? g_pick_c_cnt - 1 : 0; }
            else if (key >= AKEY_TAB1 && key <= AKEY_TAB6) picker_load_continent(key - AKEY_TAB1);
            else if (is_ok && g_pick_c_sel < g_pick_c_cnt) picker_enter_country(g_pick_c_sel);
            g_pick_c_page = g_pick_c_sel / per_pg;
        } else {
            if (key == AKEY_BACK) { g_pick_step = 0; return; }
            if (key == AKEY_UP && g_pick_ap_sel > 0) g_pick_ap_sel--;
            else if (key == AKEY_DOWN && g_pick_ap_sel + 1 < g_pick_ap_cnt) g_pick_ap_sel++;
            else if (is_ok && g_pick_ap_sel < g_pick_ap_cnt) picker_commit_airport(g_pick_ap_list[g_pick_ap_sel]);
        }
        return;
    }

    if (g_scr == SCR_SHOP_DETAIL) {
        if (key == AKEY_BACK) { g_scr = SCR_MAIN; return; }
        if (is_ok) {
            char m[112];
            if (am_buy_aircraft_bulk(g_shop_detail_model, g_am.hubs[0], g_cfg_y, g_cfg_j, g_cfg_f, g_buy_qty, m, sizeof(m))) {
                g_sel_plane = g_am.fleet_count - 1; set_toast(m); g_scr = SCR_MAIN;
            } else set_toast(m);
        }
        return;
    }

    if (g_scr == SCR_CREATE) {
        if (key == AKEY_BACK) { g_scr = SCR_TITLE; return; }
        if (key == AKEY_UP) { g_create_row = (g_create_row + 4) % 5; return; }
        if (key == AKEY_DOWN) { g_create_row = (g_create_row + 1) % 5; return; }
        if (g_create_row == 0) {
            if (key == AKEY_OK) g_create_row = 1;
            else edit_buf(g_new_save, sizeof(g_new_save), key, key == AKEY_SPACE ? ' ' : raw_char, 0);
            return;
        }
        if (g_create_row == 1) {
            if (key == AKEY_OK) g_create_row = 2;
            else edit_buf(g_new_name, sizeof(g_new_name), key, key == AKEY_SPACE ? ' ' : raw_char, 0);
            return;
        }
        if (g_create_row == 2) {
            if (key == AKEY_BACKSPACE) { size_t n = strlen(g_new_iata); if (n > 0) g_new_iata[n - 1] = '\0'; }
            else if (key == AKEY_OK) g_create_row = 3;
            else if (isalnum((unsigned char)raw_char)) { size_t n = strlen(g_new_iata); if (n < 2) { g_new_iata[n] = (char)toupper((unsigned char)raw_char); g_new_iata[n + 1] = '\0'; } }
            return;
        }
        if (g_create_row == 3 && is_ok) g_new_realism = 1 - g_new_realism;
        if (g_create_row == 4 && is_ok) { g_starter_sel = 0; g_scr = SCR_STARTER_AC; }
        return;
    }

    if (g_scr == SCR_PAUSE) {
        if (key == AKEY_BACK) { g_scr = SCR_MAIN; return; }
        if (key == AKEY_UP) g_pause_sel = (g_pause_sel + 3) % 4;
        else if (key == AKEY_DOWN) g_pause_sel = (g_pause_sel + 1) % 4;
        else if (is_ok) {
            if (g_pause_sel == 0) g_scr = SCR_MAIN;
            else if (g_pause_sel == 1) {
                am_slots_refresh(); g_slot_mode = 1; g_slot_sel = g_am.active_slot;
                snprintf(g_save_input, sizeof(g_save_input), "%s", g_am.save_name);
                g_scr = SCR_SLOTS;
            } else if (g_pause_sel == 2) { am_sim_save(); g_scr = SCR_TITLE; }
            else { am_sim_save(); hal_request_quit(); }
        }
        return;
    }

    if (g_scr == SCR_PLANE_MODAL) {
        if (key == AKEY_BACK) { g_scr = SCR_MAIN; return; }
        if (key == AKEY_UP) g_pl_sel = (g_pl_sel + 7) % 8;
        else if (key == AKEY_DOWN) g_pl_sel = (g_pl_sel + 1) % 8;
        else if (is_ok) {
            char msg[112];
            if (g_pl_sel == 0) { am_repair_plane(g_pl_fleet_idx, msg, sizeof(msg)); set_toast(msg); }
            else if (g_pl_sel == 1) { am_acheck_plane(g_pl_fleet_idx, msg, sizeof(msg)); set_toast(msg); }
            else if (g_pl_sel == 2) { am_buy_mod(g_pl_fleet_idx, 0, msg, sizeof(msg)); set_toast(msg); }
            else if (g_pl_sel == 3) { am_buy_mod(g_pl_fleet_idx, 1, msg, sizeof(msg)); set_toast(msg); }
            else if (g_pl_sel == 4) { am_buy_mod(g_pl_fleet_idx, 2, msg, sizeof(msg)); set_toast(msg); }
            else if (g_pl_sel == 5) { g_am.fleet[g_pl_fleet_idx].auto_return = !g_am.fleet[g_pl_fleet_idx].auto_return; am_sim_save(); }
            else if (g_pl_sel == 6) { am_ground_plane(g_pl_fleet_idx, msg, sizeof(msg)); set_toast(msg); }
            else if (g_pl_sel == 7) {
                if (am_sell_aircraft(g_pl_fleet_idx, msg, sizeof(msg))) {
                    if (g_sel_plane >= g_am.fleet_count && g_am.fleet_count > 0) g_sel_plane = g_am.fleet_count - 1;
                    set_toast(msg); g_scr = SCR_MAIN;
                } else set_toast(msg);
            }
        }
        return;
    }

    if (key == AKEY_TAB1) { g_tab = TAB_MAP; return; }
    if (key == AKEY_TAB2) { g_tab = TAB_FLEET; return; }
    if (key == AKEY_TAB3) { g_tab = TAB_SHOP; return; }
    if (key == AKEY_TAB4) { g_tab = TAB_FUEL; return; }
    if (key == AKEY_TAB5) { g_tab = TAB_COMPANY; return; }
    if (key == AKEY_TAB6) { g_tab = TAB_FINANCE; return; }
    if (key == AKEY_SPEED) { g_am.speed_mult_idx = (g_am.speed_mult_idx + 1) % 5; return; }
    if (key == AKEY_DEPART_ALL) { char m[112]; am_depart_all(m, sizeof(m)); set_toast(m); return; }
    if (key == AKEY_BACK) {
        if (g_tab == TAB_MAP && g_drawer == DRW_ROUTE_BUILD) { g_drawer = DRW_PLANE_CTRL; return; }
        g_pause_sel = 0; g_scr = SCR_PAUSE; return;
    }
    if (g_tab == TAB_MAP && g_map_zoom > 1) {
        if (key == AKEY_LEFT)  { g_map_pan_x -= 24; clamp_map_pan(); }
        else if (key == AKEY_RIGHT) { g_map_pan_x += 24; clamp_map_pan(); }
        else if (key == AKEY_UP)    { g_map_pan_y -= 16; clamp_map_pan(); }
        else if (key == AKEY_DOWN)  { g_map_pan_y += 16; clamp_map_pan(); }
    }
}

void am_ui_on_touch(int tx, int ty) {
    int sw = hal_get_width(), sh = hal_get_height();
    g_redraw = 1;

    if (g_scr == SCR_TITLE) {
        int hs = any_slot_used(), n = hs ? 3 : 2, pw = 480, px = (sw - pw) / 2, py = 62;
        for (int i = 0; i < n; i++) {
            int ry = py + 150 + i * 52;
            if (tx >= px + 56 && tx <= px + pw - 56 && ty >= ry && ty <= ry + 42) { g_title_sel = i; am_ui_on_key(AKEY_OK, 0); return; }
        }
        return;
    }

    if (g_scr == SCR_SLOTS) {
        int mw = 640, mh = 428, mx = (sw - mw) / 2, my = (sh - mh) / 2;
        if (tx < mx || tx > mx + mw || ty < my || ty > my + mh) { g_scr = (g_slot_mode == 1) ? SCR_MAIN : SCR_TITLE; return; }
        if (g_slot_mode == 1 && ty >= my + 48 && ty <= my + 86) {
            if (tx >= mx + mw - 86) g_save_input[0] = '\0';
            else if (tx >= mx + mw - 166) { size_t n = strlen(g_save_input); if (n > 0) g_save_input[n - 1] = '\0'; }
            return;
        }
        int top_y = (g_slot_mode == 1) ? (my + 94) : (my + 52);
        int row_h = (g_slot_mode == 1) ? 36 : 42;
        if (ty >= top_y && ty < my + mh - 46) {
            int idx = (ty - top_y) / row_h;
            if (idx >= 0 && idx < AM_MAX_SAVE_SLOTS) {
                g_slot_sel = idx;
                if (tx >= mx + mw - 84 && g_slots.slots[idx].used) { am_sim_delete_slot(idx); set_toast("已刪除存檔槽"); return; }
                if (g_slot_mode == 0) {
                    if (g_slots.slots[idx].used && am_sim_load_slot(idx)) {
                        g_scr = SCR_MAIN; g_tab = TAB_MAP; g_drawer = DRW_PLANE_CTRL; set_toast(g_am.last_notice);
                    }
                } else {
                    am_sim_save_slot(idx, g_save_input); set_toast("已另存新檔！"); g_scr = SCR_MAIN;
                }
            }
            return;
        }
        if (ty >= my + mh - 46) g_scr = (g_slot_mode == 1) ? SCR_MAIN : SCR_TITLE;
        return;
    }

    if (g_scr == SCR_STARTER_AC) {
        int mh = 386, my = (sh - mh) / 2;
        if (ty >= my + 56 && ty <= my + mh - 14) {
            g_starter_sel = (tx < sw / 2) ? 0 : 1;
            if (ty >= my + mh - 68) am_ui_on_key(AKEY_OK, 0);
        }
        return;
    }

    /* 2 頁史詩級 MOD MENU 觸控 */
    if (g_scr == SCR_CHEAT) {
        int mw = 620, mh = 420, mx = (sw - mw) / 2, my = (sh - mh) / 2;
        if (tx < mx || tx > mx + mw || ty < my || ty > my + mh) { g_scr = SCR_MAIN; return; }
        /* 頂部 2 頁籤 */
        if (ty >= my + 48 && ty <= my + 80) {
            if (tx < mx + mw / 2) g_mod_page = 0; else g_mod_page = 1;
            return;
        }
        char m[112];
        if (g_mod_page == 0) {
            /* 精準金額輸入列 (y = my+88..126) */
            if (ty >= my + 86 && ty <= my + 128) {
                if (tx >= mx + mw - 122) { am_cheat_set_cash(atoll(g_mod_cash_str), m, sizeof(m)); set_toast(m); }
                else if (tx >= mx + mw - 200) { size_t n = strlen(g_mod_cash_str); if (n > 0) g_mod_cash_str[n - 1] = '\0'; }
                return;
            }
            int cw = (mw - 52) / 3;
            if (ty >= my + 136 && ty <= my + 180) {
                int c = (tx - (mx + 18)) / (cw + 8);
                if (c == 0) am_cheat_add_cash(100000000LL, m, sizeof(m));
                else if (c == 1) am_cheat_add_cash(1000000000LL, m, sizeof(m));
                else if (c == 2) am_cheat_add_points(500, m, sizeof(m));
                set_toast(m); return;
            }
            if (ty >= my + 188 && ty <= my + 232) {
                int c = (tx - (mx + 18)) / (cw + 8);
                if (c == 0) { g_am.cheat_god_fuel = !g_am.cheat_god_fuel; set_toast(g_am.cheat_god_fuel ? "[MOD] 無限免費油已開啟" : "[MOD] 已關閉無限油"); am_sim_save(); }
                else if (c == 1) { g_am.cheat_no_wear = !g_am.cheat_no_wear; set_toast(g_am.cheat_no_wear ? "[MOD] 零磨損無敵已開啟" : "[MOD] 已關閉零磨損"); am_sim_save(); }
                else if (c == 2) { g_am.cheat_no_staff_req = !g_am.cheat_no_staff_req; set_toast(g_am.cheat_no_staff_req ? "[MOD] 免員工買機限制已開啟" : "[MOD] 已恢復員工限制"); am_sim_save(); }
                return;
            }
            if (ty >= my + 240 && ty <= my + 284) {
                int c = (tx - (mx + 18)) / (cw + 8);
                if (c == 0) am_cheat_refill_all(m, sizeof(m));
                else if (c == 1) am_cheat_instant_finish_flights(m, sizeof(m));
                else if (c == 2) am_cheat_max_reputation(m, sizeof(m));
                set_toast(m); return;
            }
            if (ty >= my + 292 && ty <= my + 336) {
                int c = (tx - (mx + 18)) / (cw + 8);
                if (c == 0) am_cheat_unlock_all_hubs(m, sizeof(m));
                else if (c == 1) am_cheat_max_staff_and_training(m, sizeof(m));
                else if (c == 2) am_cheat_force_market_refresh(m, sizeof(m));
                set_toast(m); return;
            }
            if (ty >= my + 344 && ty <= my + 388) {
                int c = (tx - (mx + 18)) / (cw + 8);
                if (c == 0) { am_cheat_unlock_all_achs(m, sizeof(m)); set_toast(m); }
                else if (c == 1) { g_am.speed_mult_idx = 4; set_toast("[MOD] 光速時間 1000x！"); }
                else if (c == 2) g_scr = SCR_MAIN;
                return;
            }
        } else {
            /* Page 2: 指定機型一鍵開通「整個國家所有機場」或「全世界所有機場」！ */
            if (ty >= my + 100 && ty <= my + 150) {
                int n = am_aircraft_count();
                if (tx < mx + mw / 2) g_mod_model_idx = (g_mod_model_idx + n - 1) % n;
                else g_mod_model_idx = (g_mod_model_idx + 1) % n;
                return;
            }
            if (ty >= my + 164 && ty <= my + 214) {
                open_airport_picker(PICK_FOR_MOD_COUNTRY, SCR_CHEAT);
                return;
            }
            if (ty >= my + 236 && ty <= my + 292) {
                am_cheat_open_routes_country(g_mod_model_idx, am_country_code(g_mod_country_idx), m, sizeof(m));
                set_toast(m); return;
            }
            if (ty >= my + 306 && ty <= my + 362) {
                am_cheat_open_routes_world(g_mod_model_idx, m, sizeof(m));
                set_toast(m); return;
            }
        }
        return;
    }

    if (g_scr == SCR_AP_PICKER) {
        int mw = 640, mh = 432, mx = (sw - mw) / 2, my = (sh - mh) / 2;
        if (tx < mx || tx > mx + mw || ty < my || ty > my + mh) { g_scr = g_pick_return; return; }
        if (ty <= my + 44) { if (tx < mx + 116) { if (g_pick_step == 1) g_pick_step = 0; else g_scr = g_pick_return; } return; }
        if (g_pick_step == 0) {
            if (ty >= my + 46 && ty <= my + 82) { int c = (tx - (mx + 14)) / 88; if (c >= 0 && c < AM_NUM_CONTINENTS) picker_load_continent(c); return; }
            if (ty >= my + mh - 44) {
                if (tx < mx + 180 && g_pick_c_page > 0) { g_pick_c_page--; g_pick_c_sel = g_pick_c_page * 12; }
                else if (tx > mx + mw - 180 && (g_pick_c_page + 1) * 12 < g_pick_c_cnt) { g_pick_c_page++; g_pick_c_sel = g_pick_c_page * 12; }
                return;
            }
            if (ty >= my + 88 && ty < my + mh - 48) {
                int col = (tx - (mx + 18)) / 202, row = (ty - (my + 88)) / 72;
                if (col >= 0 && col < 3 && row >= 0 && row < 4) { int pos = g_pick_c_page * 12 + row * 3 + col; if (pos < g_pick_c_cnt) picker_enter_country(pos); }
            }
        } else {
            if (ty >= my + mh - 44) {
                if (tx < mx + 180 && g_pick_ap_scroll > 0) g_pick_ap_scroll -= 6;
                else if (tx > mx + mw - 180 && g_pick_ap_scroll + 6 < g_pick_ap_cnt) g_pick_ap_scroll += 6;
                if (g_pick_ap_scroll < 0) g_pick_ap_scroll = 0;
                return;
            }
            if (ty >= my + 56 && ty < my + mh - 48) {
                int row = (ty - (my + 56)) / 52, idx = g_pick_ap_scroll + row;
                if (row >= 0 && row < 6 && idx < g_pick_ap_cnt) picker_commit_airport(g_pick_ap_list[idx]);
            }
        }
        return;
    }

    /* 購機百科詳情 + 批量購買數量 (x1/x5/x10/x25/x50) + 座位配置 */
    if (g_scr == SCR_SHOP_DETAIL) {
        int mw = 640, mh = 430, mx = (sw - mw) / 2, my = (sh - mh) / 2;
        if (tx < mx || tx > mx + mw || ty < my || ty > my + mh) { g_scr = SCR_MAIN; return; }
        const AircraftModel *m = am_aircraft_get(g_shop_detail_model);
        /* 批量購買數量鈕 (y = my+224..258) */
        if (ty >= my + 222 && ty <= my + 260 && tx >= mx + 170) {
            static const int QTYS[5] = { 1, 5, 10, 25, 50 };
            int s = (tx - (mx + 170)) / 86;
            if (s >= 0 && s < 5) g_buy_qty = QTYS[s];
            return;
        }
        if (!m->is_cargo && ty >= my + 286 && ty <= my + 354) {
            if (tx >= mx + 240 && tx < mx + 300) adjust_seat_config(m->cap, 1, -4);
            else if (tx >= mx + 304 && tx < mx + 366) adjust_seat_config(m->cap, 1, 4);
            else if (tx >= mx + 440 && tx < mx + 500) adjust_seat_config(m->cap, 2, -2);
            else if (tx >= mx + 504 && tx < mx + 566) adjust_seat_config(m->cap, 2, 2);
            return;
        }
        if (ty >= my + mh - 54) { if (tx < mx + mw / 2) am_ui_on_key(AKEY_OK, 0); else g_scr = SCR_MAIN; }
        return;
    }

    if (g_scr == SCR_CREATE) {
        int pw = 580, px = (sw - pw) / 2, py = 16;
        if (ty >= py + 44 && ty <= py + 82) {
            g_create_row = 0;
            if (tx >= px + pw - 84) g_new_save[0] = '\0';
            else if (tx >= px + pw - 166) { size_t n = strlen(g_new_save); if (n > 0) g_new_save[n - 1] = '\0'; }
            return;
        }
        if (ty >= py + 90 && ty <= py + 128) {
            g_create_row = 1;
            if (tx >= px + pw - 84) g_new_name[0] = '\0';
            else if (tx >= px + pw - 166) { size_t n = strlen(g_new_name); if (n > 0) g_new_name[n - 1] = '\0'; }
            return;
        }
        if (ty >= py + 136 && ty <= py + 174) {
            g_create_row = 2;
            if (tx >= px + pw - 84) g_new_iata[0] = '\0';
            else if (tx >= px + pw - 166) { size_t n = strlen(g_new_iata); if (n > 0) g_new_iata[n - 1] = '\0'; }
            return;
        }
        if (ty >= py + 184 && ty <= py + 222) {
            g_create_row = 3;
            if (tx < px + 360) {
                static const int64_t CP[4] = { 1800000LL, 4000000LL, 10000000LL, 25000000LL };
                int cur = 0; for (int i = 0; i < 4; i++) if (g_new_cash == CP[i]) cur = i;
                g_new_cash = CP[(cur + 1) % 4];
            } else g_new_realism = 1 - g_new_realism;
            return;
        }
        if (ty >= py + 232 && ty <= py + 276) { g_create_row = 4; open_airport_picker(PICK_FOR_CREATE_HUB, SCR_CREATE); return; }
        if (ty >= py + 286 && ty <= py + 320) {
            const char *ph[6] = { "TPE", "SYD", "HND", "SIN", "LHR", "JFK" };
            int idx = (tx - (px + 22)) / 90;
            if (idx >= 0 && idx < 6) { int a = am_airport_find_iata(ph[idx]); if (a >= 0) g_new_hub_idx = a; }
            return;
        }
        if (ty >= py + 344 && ty <= py + 398) { g_starter_sel = 0; g_scr = SCR_STARTER_AC; }
        return;
    }

    if (g_scr == SCR_PLANE_MODAL) {
        int mw = 540, mh = 420, mx = (sw - mw) / 2, my = (sh - mh) / 2;
        if (tx < mx || tx > mx + mw || ty < my || ty > my + mh) { g_scr = SCR_MAIN; return; }
        FleetPlane *fp = &g_am.fleet[g_pl_fleet_idx]; const AircraftModel *m = am_aircraft_get(fp->model_idx);
        if (!m->is_cargo && ty >= my + 54 && ty <= my + 98) {
            if (tx >= mx + 180 && tx < mx + 230) adjust_seat_config(m->cap, 1, -4);
            else if (tx >= mx + 234 && tx < mx + 284) adjust_seat_config(m->cap, 1, 4);
            else if (tx >= mx + 340 && tx < mx + 390) adjust_seat_config(m->cap, 2, -2);
            else if (tx >= mx + 394 && tx < mx + 444) adjust_seat_config(m->cap, 2, 2);
            fp->seats_y = g_cfg_y; fp->seats_j = g_cfg_j; fp->seats_f = g_cfg_f; am_sim_save(); return;
        }
        int idx = (ty - (my + 104)) / 38;
        if (idx >= 0 && idx < 8) { g_pl_sel = idx; am_ui_on_key(AKEY_OK, 0); }
        return;
    }

    if (g_scr == SCR_PAUSE) {
        int pw = 360, ph = 264, px = (sw - pw) / 2, py = (sh - ph) / 2;
        if (tx < px || tx > px + pw || ty < py || ty > py + ph) { g_scr = SCR_MAIN; return; }
        int idx = (ty - (py + 54)) / 48;
        if (idx >= 0 && idx < 4) { g_pause_sel = idx; am_ui_on_key(AKEY_OK, 0); }
        return;
    }

    /* 頂部遊戲化 HUD 導覽列 (y <= 40) */
    if (ty <= 40) {
        if (tx >= sw - 386 && tx < sw - 314) { g_mod_page = 0; g_scr = SCR_CHEAT; return; }
        if (tx >= sw - 310 && tx < sw - 224) { g_redraw = 1; set_toast("已刷新即時雷達"); return; }
        if (tx >= sw - 220 && tx < sw - 122) { g_am.speed_mult_idx = (g_am.speed_mult_idx + 1) % 5; return; }
        if (tx >= sw - 118) { char m[112]; am_depart_all(m, sizeof(m)); set_toast(m); return; }
        return;
    }

    /* 右側浮動 7 顆遊戲圖示列 */
    if (tx >= sw - 74) {
        int idx = (ty - 50) / 58;
        if (idx >= 0 && idx < 7) { g_tab = (MainTab)idx; return; }
        return;
    }

    if (g_tab == TAB_MAP) {
        int drw_w = (g_drawer != DRW_CLOSED) ? 296 : 0;
        if (tx >= drw_w && tx <= drw_w + 26 && ty >= 170 && ty <= 230) {
            g_drawer = (g_drawer == DRW_CLOSED) ? DRW_PLANE_CTRL : DRW_CLOSED; return;
        }
        if (g_drawer == DRW_PLANE_CTRL && tx < drw_w && g_am.fleet_count > 0) {
            if (g_sel_plane >= g_am.fleet_count) g_sel_plane = 0;
            char m[112];
            if (ty >= 46 && ty <= 82) {
                if (tx < 56 && g_sel_plane > 0) g_sel_plane--;
                else if (tx > drw_w - 56 && g_sel_plane + 1 < g_am.fleet_count) g_sel_plane++;
                return;
            }
            if (ty >= 122 && ty <= 152) { int s = (tx - 12) / 68; if (s >= 0 && s < 4) { g_am.fleet[g_sel_plane].cost_index = 50 + s * 50; am_sim_save(); } return; }
            if (ty >= 316 && ty <= 354) {
                if (!g_am.fleet[g_sel_plane].has_route) enter_live_route_drawer(g_sel_plane);
                else { am_depart_plane(g_sel_plane, m, sizeof(m)); set_toast(m); }
                return;
            }
            if (ty >= 360 && ty <= 394) { enter_live_route_drawer(g_sel_plane); return; }
            if (ty >= 400 && ty <= 432) { open_plane_modal(g_sel_plane); return; }
            if (ty >= 438 && ty <= 470) { am_ground_plane(g_sel_plane, m, sizeof(m)); set_toast(m); return; }
            return;
        }
        if (g_drawer == DRW_ROUTE_BUILD && tx < drw_w && g_am.fleet_count > 0) {
            if (ty >= 46 && ty <= 74 && tx >= drw_w - 44) { g_drawer = DRW_PLANE_CTRL; return; }
            if (ty >= 82 && ty <= 118) {
                if (tx < drw_w / 2 && g_am.hub_count > 1) { g_rt_orig_hub_slot = (g_rt_orig_hub_slot + 1) % g_am.hub_count; sync_route_autoprice(); }
                else open_airport_picker(PICK_FOR_ROUTE_DEST, SCR_MAIN);
                return;
            }
            if (ty >= 124 && ty <= 154) {
                if (tx >= drw_w - 72) { g_rt_stop_ap = -1; sync_route_autoprice(); }
                else open_airport_picker(PICK_FOR_ROUTE_STOP, SCR_MAIN);
                return;
            }
            if (ty >= 184 && ty <= 226) {
                int col = (tx - 10) / 92, plus = ((tx - 10) % 92) > 46;
                if (col == 0) { g_rt_py += plus ? 25 : -25; if (g_rt_py < 50) g_rt_py = 50; }
                else if (col == 1) { g_rt_pj += plus ? 50 : -50; if (g_rt_pj < 100) g_rt_pj = 100; }
                else if (col == 2) { g_rt_pf += plus ? 100 : -100; if (g_rt_pf < 200) g_rt_pf = 200; }
                return;
            }
            if (ty >= 232 && ty <= 268) {
                if (tx < 168) { sync_route_autoprice(); set_toast("已套用 Autoprice 最佳票價！"); }
                else g_rt_auto_ret = 1 - g_rt_auto_ret;
                return;
            }
            if (ty >= 272 && ty <= 306) { int s = (tx - 10) / 70; if (s >= 0 && s < 4) g_rt_ci = 50 + s * 50; return; }
            if (ty >= 424) {
                char m[112]; int orig = g_am.hubs[g_rt_orig_hub_slot % g_am.hub_count];
                if (am_assign_route_full(g_sel_plane, orig, g_rt_dest_ap, g_rt_stop_ap, g_rt_py, g_rt_pj, g_rt_pf, g_rt_ci, g_rt_auto_ret, m, sizeof(m))) {
                    set_toast(m); g_drawer = DRW_PLANE_CTRL;
                } else set_toast(m);
            }
            return;
        }

        /* 地圖右上 + / - 縮放鈕 + 點擊平移 */
        int map_w = sw - 82, map_h = sh - 102;
        if (tx >= map_w - 40 && tx <= map_w - 6 && ty >= 52 && ty <= 122) {
            if (ty < 86 && g_map_zoom < 3) { g_map_zoom++; g_map_pan_x = (360 - 360 / g_map_zoom) / 2; g_map_pan_y = (190 - 190 / g_map_zoom) / 2; }
            else if (ty >= 86 && g_map_zoom > 1) { g_map_zoom--; if (g_map_zoom == 1) { g_map_pan_x = 0; g_map_pan_y = 0; } }
            clamp_map_pan(); return;
        }
        if (g_map_zoom > 1 && ty >= 40 && ty < sh - 62) {
            int src_w = 360 / g_map_zoom, src_h = 190 / g_map_zoom;
            g_map_pan_x = g_map_pan_x + (tx * src_w) / map_w - src_w / 2;
            g_map_pan_y = g_map_pan_y + ((ty - 40) * src_h) / map_h - src_h / 2;
            clamp_map_pan();
        }
        return;
    }

    int mx = 14, mw = sw - 96;
    if (g_tab == TAB_FLEET) {
        int top_y = 54, row_h = 88;
        if (ty >= top_y && ty < sh - 44) {
            int r = (ty - top_y) / row_h, idx = g_fleet_scroll + r;
            if (idx >= 0 && idx < g_am.fleet_count && r < 4) {
                g_sel_plane = idx;
                if (tx >= mx + mw - 72) open_plane_modal(idx);
                else if (tx >= mx + mw - 144) enter_live_route_drawer(idx);
                else if (tx >= mx + mw - 216) { char m[112]; am_depart_plane(idx, m, sizeof(m)); set_toast(m); }
                else { g_tab = TAB_MAP; g_drawer = DRW_PLANE_CTRL; }
            }
        } else if (ty >= sh - 44) {
            char m[112];
            if (tx < mx + 110 && g_fleet_scroll > 0) g_fleet_scroll -= 4;
            else if (tx >= mx + 118 && tx < mx + 248) { am_repair_all(m, sizeof(m)); set_toast(m); }
            else if (tx >= mx + 256 && tx < mx + 386) { am_acheck_all(m, sizeof(m)); set_toast(m); }
            else if (tx >= mx + mw - 110 && g_fleet_scroll + 4 < g_am.fleet_count) g_fleet_scroll += 4;
            if (g_fleet_scroll < 0) g_fleet_scroll = 0;
        }
        return;
    }

    if (g_tab == TAB_SHOP) {
        if (ty >= 50 && ty <= 82) { int c = (tx - mx) / 98; if (c >= 0 && c < 7) { g_shop_mfg = c; g_shop_sel = 0; g_shop_scroll = 0; } return; }
        int list[AM_MAX_AIRCRAFT], cnt = build_shop_list(list, AM_MAX_AIRCRAFT);
        int top_y = 90, row_h = 84;
        if (ty >= top_y && ty < sh - 38) {
            int r = (ty - top_y) / row_h, idx = g_shop_scroll + r;
            if (idx >= 0 && idx < cnt && r < 4) {
                g_shop_sel = idx; g_shop_detail_model = list[idx];
                init_default_seats_for_model(g_shop_detail_model); g_scr = SCR_SHOP_DETAIL;
            }
        } else if (ty >= sh - 38) {
            if (tx < mx + 140) am_ui_on_key(AKEY_PGUP, 0);
            else if (tx >= mx + mw - 140) am_ui_on_key(AKEY_PGDN, 0);
        }
        return;
    }

    /* 油料與 CO2 自由選擇購買數量 (100K / 500K / 1M / 5M / 補滿) */
    if (g_tab == TAB_FUEL) {
        char m[112]; int half = mx + mw / 2;
        static const int64_t AMTS[5] = { 100000LL, 500000LL, 1000000LL, 5000000LL, 999999999LL };
        if (ty >= 202 && ty <= 236) {
            if (tx < half) { int s = (tx - (mx + 12)) / 64; if (s >= 0 && s < 5) g_fuel_amt_idx = s; }
            else { int s = (tx - (half + 18)) / 64; if (s >= 0 && s < 5) g_co2_amt_idx = s; }
            return;
        }
        if (ty >= 244 && ty <= 284) {
            if (tx < half) am_buy_fuel(AMTS[g_fuel_amt_idx], m, sizeof(m));
            else am_buy_co2(AMTS[g_co2_amt_idx], m, sizeof(m));
            set_toast(m); return;
        }
        if (ty >= 290 && ty <= 328) {
            if (tx < half) am_upgrade_fuel_tank(m, sizeof(m));
            else am_upgrade_co2_tank(m, sizeof(m));
            set_toast(m); return;
        }
        if (ty >= 352 && ty <= 428) { int c = (tx - mx) / ((mw - 16) / 3); if (c >= 0 && c < 3) { am_start_marketing(c, m, sizeof(m)); set_toast(m); } }
        return;
    }

    if (g_tab == TAB_COMPANY) {
        char m[112];
        if (ty >= 48 && ty <= 82) { int s = (tx - mx) / 162; if (s >= 0 && s < 3) g_comp_subtab = s; return; }
        if (g_comp_subtab == 0) {
            if (ty >= 92 && ty <= 132) {
                if (tx >= mx + 440 && tx < mx + 520 && g_am.staff_salary_pct > 80) g_am.staff_salary_pct -= 5;
                else if (tx >= mx + 530 && tx < mx + 610 && g_am.staff_salary_pct < 140) g_am.staff_salary_pct += 5;
                return;
            }
            if (ty >= 146 && ty <= 228) { int bw = (mw - 24) / 4, d = (tx - mx) / (bw + 8); if (d >= 0 && d < 4) { am_hire_staff(d, 10, m, sizeof(m)); set_toast(m); } return; }
            if (ty >= 266 && ty <= 350) { int bw = (mw - 20) / 3, t = (tx - mx) / (bw + 10); if (t >= 0 && t < 3) { am_upgrade_training(t, m, sizeof(m)); set_toast(m); } }
        } else if (g_comp_subtab == 1) {
            if (ty >= 336 && ty <= 394) {
                if (tx < mx + 300) open_airport_picker(PICK_FOR_BUY_HUB, SCR_MAIN);
                else { am_buy_hub(g_ops_hub_ap, m, sizeof(m)); set_toast(m); }
            }
        } else {
            int bw = (mw - 20) / 3;
            if (ty >= 124 && ty <= 204) { int lv = (tx - mx) / (bw + 10); if (lv >= 0 && lv < 3) { am_set_catering(lv, m, sizeof(m)); set_toast(m); } return; }
            if (ty >= 256 && ty <= 344) { int al = (tx - mx) / (bw + 10) + 1; if (al >= 1 && al <= 3) { am_join_alliance(al, m, sizeof(m)); set_toast(m); } }
        }
        return;
    }

    if (g_tab == TAB_FINANCE) {
        if (ty >= 50 && ty <= 82) { int s = (tx - mx) / 162; if (s >= 0 && s < 3) g_fin_subtab = s; return; }
        char m[112];
        if (g_fin_subtab == 0 && ty >= 294 && ty <= 344) {
            int bw = (mw - 20) / 3, c = (tx - mx) / (bw + 10);
            if (c == 0) { am_take_loan(2000000LL, m, sizeof(m)); set_toast(m); }
            else if (c == 1) { am_take_loan(5000000LL, m, sizeof(m)); set_toast(m); }
            else if (c == 2) { am_repay_loan(2000000LL, m, sizeof(m)); set_toast(m); }
        } else if (g_fin_subtab == 2 && ty >= 94 && ty < sh - 28) {
            int r = (ty - 94) / 64, idx = g_stock_scroll + r;
            if (idx >= 0 && idx < AM_NUM_RIVALS && r < 5) {
                g_stock_sel = idx;
                if (tx >= mx + mw - 86) { am_trade_stock(idx, -100, m, sizeof(m)); set_toast(m); }
                else if (tx >= mx + mw - 178) { am_trade_stock(idx, 100, m, sizeof(m)); set_toast(m); }
            }
        }
        return;
    }

    if (g_tab == TAB_BONUS) {
        char m[112];
        if (ty >= 50 && ty <= 104) {
            int bw = (mw - 24) / 4, c = (tx - mx) / (bw + 8);
            if (c == 0) { am_claim_daily_bonus(m, sizeof(m)); set_toast(m); }
            else if (c >= 1 && c <= 3) { am_spend_points(c - 1, m, sizeof(m)); set_toast(m); }
            return;
        }
        if (ty >= 114 && ty <= 186) {
            if (tx >= mx + mw - 102) { am_roll_new_charter(); set_toast("已刷新新的 VIP 包機合約！"); }
            else if (tx >= mx + mw - 236) {
                int ri = -1; for (int i = 0; i < g_am.fleet_count; i++) if (g_am.fleet[i].status == 0) { ri = i; break; }
                am_run_charter_with_plane(ri, m, sizeof(m)); set_toast(m);
            }
            return;
        }
        if (ty >= sh - 38) {
            if (tx < mx + 180 && g_ach_scroll > 0) g_ach_scroll--;
            else if (tx > mx + mw - 180 && g_ach_scroll < 4) g_ach_scroll++;
        }
    }
}

/* ============================================================
 * 遊戲化現代航空大亨 UI 渲染 (乾淨、俐落、高質感!)
 * ============================================================ */
static void render_top_bar(int sw) {
    hal_fill_rect(0, 0, sw, 40, COL_HUD);
    char cash_s[24], pts_s[24];
    fmt_money(g_am.cash, cash_s, sizeof(cash_s));
    snprintf(pts_s, sizeof(pts_s), "%d PTS", g_am.pax_points);

    ui_fill_rrect(8, 6, 118, 28, 8, COL_HUD_BTN); hal_set_text_bg(COL_HUD_BTN);
    ui_draw_str(16, 11, cash_s, 106, 0x4ADE80);

    ui_fill_rrect(132, 6, 78, 28, 8, COL_HUD_BTN);
    ui_draw_str(140, 11, pts_s, 66, 0xFACC15);

    hal_set_text_bg(COL_HUD);
    char mid[96];
    int rem = 300 - g_am.market_real_sec_acc; if (rem < 0) rem = 0;
    snprintf(mid, sizeof(mid), "%s Lv.%d · 刷新%d:%02d", g_am.airline_name, am_airline_level(), rem / 60, rem % 60);
    ui_draw_str(218, 11, mid, sw - 610, 0xE2E8F0);

    ui_fill_rrect(sw - 386, 6, 70, 28, 8, COL_PURPLE); hal_set_text_bg(COL_PURPLE); ui_draw_str(sw - 376, 11, "外掛MOD", 56, COL_WHITE);
    ui_fill_rrect(sw - 310, 6, 84, 28, 8, COL_HUD_BTN); hal_set_text_bg(COL_HUD_BTN); ui_draw_str(sw - 300, 11, "刷新地圖", 68, COL_WHITE);
    ui_fill_rrect(sw - 220, 6, 94, 28, 8, COL_HUD_BTN);
    char sp[24]; snprintf(sp, sizeof(sp), "速度 %dx", am_sim_speed_mult()); ui_draw_str(sw - 208, 11, sp, 78, 0x7DD3FC);
    ui_fill_rrect(sw - 118, 6, 108, 28, 8, COL_GREEN); hal_set_text_bg(COL_GREEN); ui_draw_str(sw - 108, 11, "全機起飛[D]", 94, COL_WHITE);
}

static void render_right_dock(int sw) {
    const char *labels[7] = { "地圖", "機隊", "買機", "油料", "公司", "財報", "獎勵" };
    const uint32_t cols[7] = { COL_CYAN, COL_BLUE, COL_GREEN, COL_AMBER, COL_PURPLE, 0x0F766E, 0xE11D48 };
    int rx = sw - 70;
    for (int i = 0; i < 7; i++) {
        int ty = 50 + i * 58, act = ((int)g_tab == i);
        uint32_t bg = act ? cols[i] : COL_WHITE;
        ui_card(rx, ty, 60, 48, 12, bg, act ? COL_HUD : COL_BORDER, 0x94A3B8, act, COL_HUD);
        hal_set_text_bg(bg); ui_draw_str(rx + 12, ty + 15, labels[i], 40, act ? COL_WHITE : COL_TEXT);
    }
}

static void render_left_map_drawer(int sh) {
    int dw = 296;
    if (g_drawer == DRW_CLOSED) {
        ui_fill_rrect(0, 174, 24, 52, 6, COL_HUD); ui_draw_ascii(8, 195, ">", 2, COL_WHITE); return;
    }
    ui_fill_rrect(dw, 174, 24, 52, 6, COL_HUD); ui_draw_ascii(dw + 7, 195, "<", 2, COL_WHITE);
    if (g_am.fleet_count == 0) {
        hal_fill_rect(0, 40, dw, sh - 40, COL_WHITE); hal_set_text_bg(COL_WHITE);
        ui_draw_str(16, 100, "尚無飛機，請點右側 [買機]", 260, COL_MUTED); return;
    }
    if (g_sel_plane >= g_am.fleet_count) g_sel_plane = 0;
    const FleetPlane *fp = &g_am.fleet[g_sel_plane]; const AircraftModel *m = am_aircraft_get(fp->model_idx);

    if (g_drawer == DRW_PLANE_CTRL) {
        hal_fill_rect(0, 40, dw, sh - 40, 0x1E293B); hal_fill_rect(dw - 1, 40, 1, sh - 40, COL_HUD);
        hal_fill_rect(0, 40, dw, 42, 0x0F172A);
        ui_fill_rrect(8, 48, 34, 26, 6, 0x334155); ui_draw_ascii(21, 56, "<", 1, COL_WHITE);
        ui_fill_rrect(dw - 42, 48, 34, 26, 6, 0x334155); ui_draw_ascii(dw - 29, 56, ">", 1, COL_WHITE);
        hal_set_text_bg(0x0F172A);
        char phdr[64]; snprintf(phdr, sizeof(phdr), "%s (%d/%d) %d%%舊", fp->reg, g_sel_plane + 1, g_am.fleet_count, fp->wear_pct);
        ui_draw_str(48, 52, phdr, dw - 96, COL_WHITE);

        hal_set_text_bg(0x1E293B);
        if (fp->has_route) {
            const AirportInfo *af = am_airport_get(fp->leg_dir == 0 ? fp->orig_ap : fp->dest_ap);
            const AirportInfo *at = am_airport_get(fp->leg_dir == 0 ? fp->dest_ap : fp->orig_ap);
            char rban[64]; snprintf(rban, sizeof(rban), "%s (%s) -> %s (%s)", af->icao, af->iata, at->icao, at->iata);
            ui_draw_str(14, 92, rban, dw - 28, 0xFACC15);
        } else ui_draw_str(14, 92, "尚未開通雙向航線 (請點 Reroute)", dw - 28, 0xF87171);

        for (int s = 0; s < 4; s++) {
            int ci = 50 + s * 50, bx = 12 + s * 68;
            uint32_t bg = (fp->cost_index == ci) ? COL_AMBER : 0x334155;
            ui_fill_rrect(bx, 122, 62, 26, 6, bg); hal_set_text_bg(bg);
            char cs[12]; snprintf(cs, sizeof(cs), "CI%d", ci); ui_draw_str(bx + 12, 127, cs, 44, COL_WHITE);
        }

        hal_set_text_bg(0x1E293B);
        int spd = 0, sec = 0; int64_t fuel_need = 0; int dist = fp->has_route ? fp->dist_km : 0;
        am_calc_leg_preview(fp->model_idx, dist > 0 ? dist : 1000, fp->cost_index, fp->mod_speed, fp->mod_fuel, &spd, &sec, &fuel_need);
        char l1[64], l2[64], l3[64], l4[64], l5[64], l6[64];
        snprintf(l1, sizeof(l1), "機型：%s", m->model);
        snprintf(l2, sizeof(l2), "距離：%d km · 巡航：%d kph", dist, spd);
        snprintf(l3, sizeof(l3), "油耗：%lld lbs · CO2: 0.%02u", (long long)fuel_need, (unsigned)m->co2_kg);
        snprintf(l4, sizeof(l4), "飛行時間：%02d:%02d:00 · A檢剩%dh", sec / 3600, (sec % 3600) / 60, fp->acheck_hrs_left);
        snprintf(l5, sizeof(l5), "座位配置：Y:%d  J:%d  F:%d", fp->seats_y, fp->seats_j, fp->seats_f);
        if (fp->has_route) {
            int64_t er = 0, ef = 0, en = am_estimate_leg_profit(fp->model_idx, dist, fp->seats_y, fp->seats_j, fp->seats_f, fp->price_y, fp->price_j, fp->price_f, fp->cost_index, fp->mod_fuel, &er, &ef);
            char ps[24]; fmt_money(en, ps, sizeof(ps));
            snprintf(l6, sizeof(l6), "單程預估淨利：%s (%s)", ps, fp->auto_return ? "自動往返" : "手動");
        } else snprintf(l6, sizeof(l6), "開通航線後即可起飛賺錢！");
        ui_draw_str(14, 158, l1, dw - 24, 0x7DD3FC); ui_draw_str(14, 182, l2, dw - 24, COL_WHITE);
        ui_draw_str(14, 206, l3, dw - 24, 0xCBD5E1); ui_draw_str(14, 230, l4, dw - 24, 0xCBD5E1);
        ui_draw_str(14, 254, l5, dw - 24, 0xE2E8F0); ui_draw_str(14, 280, l6, dw - 24, 0x4ADE80);

        if (fp->status == 1) {
            int pct = fp->flight_total_sec > 0 ? (fp->flight_elapsed_sec * 100) / fp->flight_total_sec : 0;
            ui_fill_rrect(14, 316, dw - 28, 36, 8, 0x0F172A);
            ui_progress_bar(22, 340, dw - 44, 6, pct, 0x334155, 0x4ADE80);
            hal_set_text_bg(0x0F172A); char fl[48]; snprintf(fl, sizeof(fl), "飛行執勤中... %d%%", pct); ui_draw_str(76, 320, fl, 160, 0x4ADE80);
        } else {
            uint32_t c_dep = (fp->status == 0) ? COL_GREEN : 0x64748B;
            ui_fill_rrect(14, 316, dw - 28, 36, 8, c_dep); hal_set_text_bg(c_dep);
            ui_draw_str(86, 325, fp->has_route ? "Depart 立即起飛" : "設定航線並啟航", 150, COL_WHITE);
        }
        ui_fill_rrect(14, 360, dw - 28, 32, 8, COL_BLUE); hal_set_text_bg(COL_BLUE); ui_draw_str(78, 367, "Reroute 規劃航線", 150, COL_WHITE);
        ui_fill_rrect(14, 400, dw - 28, 32, 8, 0x2B547E); hal_set_text_bg(0x2B547E); ui_draw_str(68, 407, "Details 機體/改裝/座", 170, COL_WHITE);
        ui_fill_rrect(14, 438, dw - 28, 32, 8, 0x475569); hal_set_text_bg(0x475569); ui_draw_str(74, 445, fp->status == 3 ? "解除封存恢復服役" : "Ground A/C 停飛封存", 160, COL_WHITE);
    } else {
        hal_fill_rect(0, 40, dw, sh - 40, COL_WHITE); hal_fill_rect(dw - 1, 40, 1, sh - 40, COL_BORDER);
        hal_fill_rect(0, 40, dw, 34, COL_HUD); hal_set_text_bg(COL_HUD);
        char rh[48]; snprintf(rh, sizeof(rh), "雙向航線規劃 · %s", fp->reg); ui_draw_str(12, 48, rh, dw - 54, COL_WHITE);
        ui_fill_rrect(dw - 36, 45, 28, 24, 5, 0x334155); ui_draw_ascii(dw - 26, 53, "X", 1, COL_WHITE);

        int orig = g_am.hubs[g_rt_orig_hub_slot % g_am.hub_count];
        const AirportInfo *ao = am_airport_get(orig), *ad = am_airport_get(g_rt_dest_ap);
        ui_fill_rrect(10, 82, 128, 34, 7, COL_PANEL); ui_draw_rrect(10, 82, 128, 34, 7, COL_BORDER);
        hal_set_text_bg(COL_PANEL); char o1[32]; snprintf(o1, sizeof(o1), "%s (%s)", ao->icao, ao->iata); ui_draw_str(18, 90, o1, 112, COL_TEXT);
        ui_fill_rrect(144, 82, dw - 154, 34, 7, COL_BLUE); hal_set_text_bg(COL_BLUE);
        char d1[36]; snprintf(d1, sizeof(d1), "%s 換國家>", ad->iata); ui_draw_str(152, 90, d1, dw - 166, COL_WHITE);

        ui_fill_rrect(10, 122, g_rt_stop_ap >= 0 ? (dw - 84) : (dw - 20), 28, 6, COL_PANEL);
        ui_draw_rrect(10, 122, g_rt_stop_ap >= 0 ? (dw - 84) : (dw - 20), 28, 6, COL_BORDER);
        hal_set_text_bg(COL_PANEL);
        char stp[64];
        if (g_rt_stop_ap >= 0) snprintf(stp, sizeof(stp), "中轉：%s (%s)", am_airport_get(g_rt_stop_ap)->iata, am_airport_get(g_rt_stop_ap)->name);
        else snprintf(stp, sizeof(stp), "中轉：直飛 (點設中轉機場)");
        ui_draw_str(16, 127, stp, dw - 96, COL_MUTED);
        if (g_rt_stop_ap >= 0) { ui_fill_rrect(dw - 68, 122, 58, 28, 6, COL_RED); hal_set_text_bg(COL_RED); ui_draw_str(dw - 60, 127, "直飛", 44, COL_WHITE); }

        int rdy = 0, rdj = 0, rdf = 0, rdc = 0; am_calc_route_demand(orig, g_rt_dest_ap, &rdy, &rdj, &rdf, &rdc);
        hal_set_text_bg(COL_WHITE); char dm[64]; snprintf(dm, sizeof(dm), "雙邊日需求：Y%d / J%d / F%d", rdy, rdj, rdf); ui_draw_str(12, 160, dm, dw - 24, COL_MUTED);

        int ps[3] = { g_rt_py, g_rt_pj, g_rt_pf }; const char *pn[3] = { "Y$", "J$", "F$" };
        for (int i = 0; i < 3; i++) {
            int bx = 10 + i * 92; ui_fill_rrect(bx, 184, 88, 38, 7, COL_PANEL); ui_draw_rrect(bx, 184, 88, 38, 7, COL_BORDER);
            hal_set_text_bg(COL_PANEL); char pb[24]; snprintf(pb, sizeof(pb), "%s%d", pn[i], ps[i]); ui_draw_str(bx + 12, 194, pb, 70, COL_TEXT);
        }
        ui_fill_rrect(10, 232, 152, 34, 8, COL_BLUE); hal_set_text_bg(COL_BLUE); ui_draw_str(30, 240, "Autoprice 最佳價", 124, COL_WHITE);
        uint32_t abg = g_rt_auto_ret ? COL_GREEN : COL_PANEL;
        ui_fill_rrect(168, 232, dw - 178, 34, 8, abg); if (!g_rt_auto_ret) ui_draw_rrect(168, 232, dw - 178, 34, 8, COL_BORDER);
        hal_set_text_bg(abg); ui_draw_str(178, 240, g_rt_auto_ret ? "雙向自動往返" : "手動單段", 104, g_rt_auto_ret ? COL_WHITE : COL_TEXT);

        for (int s = 0; s < 4; s++) {
            int ci = 50 + s * 50, bx = 10 + s * 70; uint32_t bg = (g_rt_ci == ci) ? COL_AMBER : COL_PANEL;
            ui_fill_rrect(bx, 274, 64, 28, 6, bg); if (g_rt_ci != ci) ui_draw_rrect(bx, 274, 64, 28, 6, COL_BORDER);
            hal_set_text_bg(bg); char cl[12]; snprintf(cl, sizeof(cl), "CI%d", ci); ui_draw_str(bx + 12, 279, cl, 44, (g_rt_ci == ci) ? COL_WHITE : COL_TEXT);
        }

        int dist = (g_rt_stop_ap >= 0) ? (am_calc_distance_km(orig, g_rt_stop_ap) + am_calc_distance_km(g_rt_stop_ap, g_rt_dest_ap)) : am_calc_distance_km(orig, g_rt_dest_ap);
        int spd = 0, sec = 0; int64_t fuel_need = 0, er = 0, ef = 0;
        am_calc_leg_preview(fp->model_idx, dist, g_rt_ci, fp->mod_speed, fp->mod_fuel, &spd, &sec, &fuel_need);
        int64_t en = am_estimate_leg_profit(fp->model_idx, dist, fp->seats_y, fp->seats_j, fp->seats_f, g_rt_py, g_rt_pj, g_rt_pf, g_rt_ci, fp->mod_fuel, &er, &ef);
        char n1[20], n2[20], p1[64], p2[64], p3[64], p4[64];
        fmt_money(en, n1, sizeof(n1)); fmt_money(en * 2, n2, sizeof(n2));
        snprintf(p1, sizeof(p1), "真實距離：%d km · %d kph", dist, spd);
        snprintf(p2, sizeof(p2), "飛行時間：%02d:%02d · 耗油 %lld", sec / 3600, (sec % 3600) / 60, (long long)fuel_need);
        snprintf(p3, sizeof(p3), "開航費：$%d · CO2: 0.%02u", am_calc_route_fee(dist), (unsigned)m->co2_kg);
        snprintf(p4, sizeof(p4), "預估淨利：%s (來回 %s)", n1, n2);
        hal_set_text_bg(COL_WHITE);
        ui_draw_str(12, 312, p1, dw - 24, (dist > m->range_km && g_rt_stop_ap < 0) ? COL_RED : COL_TEXT);
        ui_draw_str(12, 336, p2, dw - 24, COL_MUTED); ui_draw_str(12, 360, p3, dw - 24, COL_MUTED);
        ui_draw_str(12, 386, p4, dw - 24, (en > 0) ? COL_GREEN : COL_RED);
        ui_fill_rrect(12, 424, dw - 24, 42, 9, COL_GREEN); hal_set_text_bg(COL_GREEN); ui_draw_str(56, 436, "Create Route 開通雙向航線", 200, COL_WHITE);
    }
}

static void render_tab_map(int sw, int sh) {
    int mw = sw - 82, mh = sh - 102;
    ui_draw_world_map_hd(0, 40, mw, mh, g_map_zoom, g_map_pan_x, g_map_pan_y);
    (void)am_asset_terrain_sample(10, 10);

    /* 最多繪製前 80 條航線避免 400+ 條線過密遮擋地圖 */
    int drawn = 0;
    for (int i = 0; i < g_am.fleet_count && drawn < 80; i++) {
        const FleetPlane *fp = &g_am.fleet[i]; if (!fp->active || !fp->has_route) continue;
        const AirportInfo *ao = am_airport_get(fp->orig_ap), *ad = am_airport_get(fp->dest_ap);
        int pct = (fp->status == 1 && fp->flight_total_sec > 0) ? ((fp->flight_elapsed_sec * 100) / fp->flight_total_sec) : -1;
        ui_draw_route_arc_view(ao->lat10, ao->lon10, ad->lat10, ad->lon10, 0, 40, mw, mh,
                               g_map_zoom, g_map_pan_x, g_map_pan_y, pct, fp->leg_dir,
                               (fp->leg_dir == 0) ? COL_RED : COL_BLUE, COL_GREEN);
        drawn++;
    }
    if (g_drawer == DRW_ROUTE_BUILD && g_am.hub_count > 0) {
        const AirportInfo *ao = am_airport_get(g_am.hubs[g_rt_orig_hub_slot % g_am.hub_count]);
        const AirportInfo *ad = am_airport_get(g_rt_dest_ap);
        ui_draw_route_arc_view(ao->lat10, ao->lon10, ad->lat10, ad->lon10, 0, 40, mw, mh, g_map_zoom, g_map_pan_x, g_map_pan_y, -1, 0, 0xFF0000, 0xFF0000);
    }
    for (int h = 0; h < g_am.hub_count && h < 40; h++) {
        const AirportInfo *ap = am_airport_get(g_am.hubs[h]); int hx, hy;
        if (ui_latlon_to_xy_view(ap->lat10, ap->lon10, 0, 40, mw, mh, g_map_zoom, g_map_pan_x, g_map_pan_y, &hx, &hy)) {
            hal_fill_circle(hx, hy, 5, COL_RED); hal_fill_circle(hx, hy, 2, COL_WHITE);
            if (hx + 34 < mw && h < 12) ui_draw_badge(hx + 6, hy - 8, ap->iata, COL_HUD, COL_WHITE);
        }
    }

    ui_card(mw - 38, 52, 30, 30, 6, COL_HUD, COL_WHITE, 0x64748B, 0, 0); ui_draw_ascii(mw - 28, 60, "+", 2, COL_WHITE);
    ui_card(mw - 38, 86, 30, 30, 6, COL_HUD, COL_WHITE, 0x64748B, 0, 0); ui_draw_ascii(mw - 28, 94, "-", 2, COL_WHITE);

    int bx0 = (g_drawer != DRW_CLOSED) ? 306 : 10, bw0 = mw - bx0 - 4, by = 40 + mh + 4;
    ui_card(bx0, by, bw0, 52, 10, COL_WHITE, COL_BORDER, COL_SHADOW, 0, 0); hal_set_text_bg(COL_WHITE);
    char l1[96], l2[120];
    snprintf(l1, sizeof(l1), "機隊 %d 架 · 樞紐 %d 座 · 油 $%d · 拖拉地圖可平移", g_am.fleet_count, g_am.hub_count, g_am.fuel_price);
    snprintf(l2, sizeof(l2), "%s", g_toast[0] ? g_toast : am_event_desc(g_am.event_id));
    ui_draw_str(bx0 + 10, by + 6, l1, bw0 - 20, COL_TEXT);
    ui_draw_str(bx0 + 10, by + 28, l2, bw0 - 20, g_am.event_id ? COL_AMBER : COL_BLUE);
    render_left_map_drawer(sh);
}

static void render_tab_fleet(int mx, int mw, int sh) {
    if (g_am.fleet_count == 0) { hal_set_text_bg(COL_BG); ui_draw_str(mx + 20, 90, "目前沒有飛機，請點右側 [買機] 購置機隊！", 400, COL_MUTED); return; }
    if (g_fleet_scroll >= g_am.fleet_count) g_fleet_scroll = 0;
    int top_y = 54, row_h = 88;
    for (int r = 0; r < 4; r++) {
        int idx = g_fleet_scroll + r; if (idx >= g_am.fleet_count) break;
        const FleetPlane *fp = &g_am.fleet[idx]; const AircraftModel *m = am_aircraft_get(fp->model_idx);
        int ry = top_y + r * row_h;
        ui_card(mx, ry, mw, row_h - 10, 12, COL_WHITE, COL_BORDER, COL_SHADOW, (idx == g_sel_plane), COL_BLUE); hal_set_text_bg(COL_WHITE);
        char t1[112], t2[112];
        if (fp->has_route) {
            const char *fi = am_airport_get(fp->leg_dir == 0 ? fp->orig_ap : fp->dest_ap)->iata;
            const char *ti = am_airport_get(fp->leg_dir == 0 ? fp->dest_ap : fp->orig_ap)->iata;
            snprintf(t1, sizeof(t1), "%s %s [%s %s->%s · %dkm]", fp->reg, m->model, fp->leg_dir == 0 ? "去" : "回", fi, ti, fp->dist_km);
        } else snprintf(t1, sizeof(t1), "%s %s [未設定航線]", fp->reg, m->model);
        ui_draw_str(mx + 14, ry + 10, t1, mw - 236, COL_TEXT);
        const char *st = (fp->status == 1) ? "飛行中" : ((fp->status == 2) ? "A-Check中" : ((fp->status == 3) ? "停飛封存" : "待命"));
        snprintf(t2, sizeof(t2), "%s · Y%d/J%d/F%d · 磨損:%d%% · A檢:%dh", st, fp->seats_y, fp->seats_j, fp->seats_f, fp->wear_pct, fp->acheck_hrs_left);
        ui_draw_str(mx + 14, ry + 34, t2, mw - 236, (fp->acheck_hrs_left <= 10) ? COL_RED : COL_MUTED);
        int pct = (fp->status > 0 && fp->flight_total_sec > 0) ? ((fp->flight_elapsed_sec * 100) / fp->flight_total_sec) : 0;
        ui_progress_bar(mx + 14, ry + 60, mw - 242, 7, pct, 0xE2E8F0, (fp->status == 2) ? COL_AMBER : COL_GREEN);
        uint32_t c_dep = (fp->status == 0 && fp->has_route) ? COL_GREEN : 0x94A3B8;
        ui_fill_rrect(mx + mw - 216, ry + 20, 64, 36, 8, c_dep); hal_set_text_bg(c_dep); ui_draw_str(mx + mw - 202, ry + 29, "起飛", 44, COL_WHITE);
        ui_fill_rrect(mx + mw - 144, ry + 20, 64, 36, 8, COL_BLUE); hal_set_text_bg(COL_BLUE); ui_draw_str(mx + mw - 130, ry + 29, "航線", 44, COL_WHITE);
        ui_fill_rrect(mx + mw - 72,  ry + 20, 62, 36, 8, 0x475569); hal_set_text_bg(0x475569); ui_draw_str(mx + mw - 58,  ry + 29, "機體", 44, COL_WHITE);
    }
    ui_fill_rrect(mx, sh - 40, 106, 30, 7, COL_WHITE); ui_draw_rrect(mx, sh - 40, 106, 30, 7, COL_BORDER); hal_set_text_bg(COL_WHITE); ui_draw_str(mx + 20, sh - 34, "< 上頁", 76, COL_TEXT);
    ui_fill_rrect(mx + 118, sh - 40, 126, 30, 7, COL_AMBER); hal_set_text_bg(COL_AMBER); ui_draw_str(mx + 134, sh - 34, "一鍵全機修復", 96, COL_WHITE);
    ui_fill_rrect(mx + 256, sh - 40, 126, 30, 7, COL_BLUE);  hal_set_text_bg(COL_BLUE);  ui_draw_str(mx + 272, sh - 34, "一鍵全機大檢", 96, COL_WHITE);
    ui_fill_rrect(mx + mw - 106, sh - 40, 106, 30, 7, COL_WHITE); ui_draw_rrect(mx + mw - 106, sh - 40, 106, 30, 7, COL_BORDER); hal_set_text_bg(COL_WHITE); ui_draw_str(mx + mw - 86, sh - 34, "下頁 >", 76, COL_TEXT);
}

static void render_tab_shop(int mx, int mw, int sh) {
    const char *mfgs[7] = { "全部", "Airbus", "Boeing", "Embraer", "Regional", "Special", "Cargo" };
    for (int i = 0; i < 7; i++) {
        int px = mx + i * 98; uint32_t bg = (g_shop_mfg == i) ? COL_BLUE : COL_WHITE;
        ui_fill_rrect(px, 50, 92, 28, 8, bg); if (g_shop_mfg != i) ui_draw_rrect(px, 50, 92, 28, 8, COL_BORDER);
        hal_set_text_bg(bg); ui_draw_str(px + 12, 55, mfgs[i], 72, (g_shop_mfg == i) ? COL_WHITE : COL_TEXT);
    }
    int list[AM_MAX_AIRCRAFT], cnt = build_shop_list(list, AM_MAX_AIRCRAFT);
    if (g_shop_sel >= cnt && cnt > 0) g_shop_sel = cnt - 1;
    if (g_shop_sel < g_shop_scroll) g_shop_scroll = g_shop_sel;
    if (g_shop_sel >= g_shop_scroll + 4) g_shop_scroll = g_shop_sel - 3;
    for (int r = 0; r < 4; r++) {
        int idx = g_shop_scroll + r; if (idx >= cnt) break;
        const AircraftModel *m = am_aircraft_get(list[idx]); int ry = 90 + r * 84;
        ui_card(mx, ry, mw, 74, 12, COL_WHITE, COL_BORDER, COL_SHADOW, (idx == g_shop_sel), COL_BLUE); hal_set_text_bg(COL_WHITE);
        char pr[24], t1[96]; fmt_money(m->price, pr, sizeof(pr));
        snprintf(t1, sizeof(t1), "%s (%s) · %s · 航程%ukm · 油耗%u", m->model, m->is_cargo ? "貨機" : "客機", pr, (unsigned)m->range_km, (unsigned)m->fuel_lbs_km);
        ui_draw_str(mx + 14, ry + 10, t1, mw - 128, COL_TEXT); ui_draw_str(mx + 14, ry + 38, m->desc, mw - 128, COL_MUTED);
        ui_fill_rrect(mx + mw - 106, ry + 18, 94, 36, 9, COL_GREEN); hal_set_text_bg(COL_GREEN); ui_draw_str(mx + mw - 96, ry + 27, "批量購買", 78, COL_WHITE);
    }
    hal_set_text_bg(COL_BG);
    ui_draw_str(mx, sh - 28, g_toast[0] ? g_toast : "點擊任一機型可一次批量購買多架 (x1/x5/x10/x25/x50) 並配置客艙！", mw, COL_BLUE);
}

static void render_tab_fuel(int mx, int mw, int sh) {
    int cw = (mw - 14) / 2;
    const char *albl[5] = { "100K", "500K", "1M", "5M", "補滿" };

    ui_card(mx, 52, cw, 286, 12, COL_WHITE, COL_BORDER, COL_SHADOW, 0, 0); hal_set_text_bg(COL_WHITE);
    char f1[64], f2[64];
    snprintf(f1, sizeof(f1), "燃油：$%d/1K (每5分鐘刷新)", g_am.fuel_price);
    snprintf(f2, sizeof(f2), "油槽：%lldK / %lldK lbs", (long long)(g_am.fuel_lbs / 1000LL), (long long)(g_am.fuel_cap / 1000LL));
    ui_draw_str(mx + 14, 62, f1, cw - 28, (g_am.fuel_price <= 700) ? COL_GREEN : ((g_am.fuel_price >= 1700) ? COL_RED : COL_TEXT));
    ui_draw_str(mx + 14, 86, f2, cw - 28, COL_MUTED);
    ui_draw_sparkline_light(mx + 14, 112, cw - 28, 82, g_am.fuel_hist, AM_PRICE_HIST, COL_AMBER);
    for (int i = 0; i < 5; i++) {
        int bx = mx + 12 + i * 64; uint32_t bg = (g_fuel_amt_idx == i) ? COL_BLUE : COL_PANEL;
        ui_fill_rrect(bx, 204, 58, 28, 6, bg); if (g_fuel_amt_idx != i) ui_draw_rrect(bx, 204, 58, 28, 6, COL_BORDER);
        hal_set_text_bg(bg); ui_draw_str(bx + 10, 209, albl[i], 44, (g_fuel_amt_idx == i) ? COL_WHITE : COL_TEXT);
    }
    ui_fill_rrect(mx + 14, 244, cw - 28, 38, 9, COL_BLUE); hal_set_text_bg(COL_BLUE); ui_draw_str(mx + 76, 254, "依自訂數量購買燃油", cw - 90, COL_WHITE);
    ui_fill_rrect(mx + 14, 290, cw - 28, 36, 9, COL_PANEL); ui_draw_rrect(mx + 14, 290, cw - 28, 36, 9, COL_BORDER); hal_set_text_bg(COL_PANEL); ui_draw_str(mx + 56, 299, "擴充油槽 +5M ($1.2M)", cw - 70, COL_TEXT);

    int rx = mx + cw + 14;
    ui_card(rx, 52, cw, 286, 12, COL_WHITE, COL_BORDER, COL_SHADOW, 0, 0); hal_set_text_bg(COL_WHITE);
    char c1[64], c2[64]; snprintf(c1, sizeof(c1), "CO2 碳權：$%d/1K", g_am.co2_price);
    snprintf(c2, sizeof(c2), "碳槽：%lldK / %lldK", (long long)(g_am.co2_quotas / 1000LL), (long long)(g_am.co2_cap / 1000LL));
    ui_draw_str(rx + 14, 62, c1, cw - 28, (g_am.co2_price <= 125) ? COL_GREEN : COL_TEXT);
    ui_draw_str(rx + 14, 86, c2, cw - 28, (g_am.co2_quotas < 0) ? COL_RED : COL_MUTED);
    ui_draw_sparkline_light(rx + 14, 112, cw - 28, 82, g_am.co2_hist, AM_PRICE_HIST, COL_GREEN);
    for (int i = 0; i < 5; i++) {
        int bx = rx + 12 + i * 64; uint32_t bg = (g_co2_amt_idx == i) ? COL_GREEN : COL_PANEL;
        ui_fill_rrect(bx, 204, 58, 28, 6, bg); if (g_co2_amt_idx != i) ui_draw_rrect(bx, 204, 58, 28, 6, COL_BORDER);
        hal_set_text_bg(bg); ui_draw_str(bx + 10, 209, albl[i], 44, (g_co2_amt_idx == i) ? COL_WHITE : COL_TEXT);
    }
    ui_fill_rrect(rx + 14, 244, cw - 28, 38, 9, COL_GREEN); hal_set_text_bg(COL_GREEN); ui_draw_str(rx + 72, 254, "依自訂數量購買碳權", cw - 90, COL_WHITE);
    ui_fill_rrect(rx + 14, 290, cw - 28, 36, 9, COL_PANEL); ui_draw_rrect(rx + 14, 290, cw - 28, 36, 9, COL_BORDER); hal_set_text_bg(COL_PANEL); ui_draw_str(rx + 54, 299, "擴充碳槽 +5M ($0.9M)", cw - 70, COL_TEXT);

    int bw = (mw - 20) / 3;
    const char *mnames[3] = { "環保形象行銷 (+10%)", "區域廣告 (+18%)", "全球旗艦廣告 (+32%)" };
    const char *mcost[3]  = { "$120K · 12小時", "$280K · 12小時", "$650K · 24小時" };
    for (int i = 0; i < 3; i++) {
        int bx = mx + i * (bw + 10);
        ui_card(bx, 352, bw, 76, 12, COL_WHITE, COL_BLUE, COL_SHADOW, 0, 0); hal_set_text_bg(COL_WHITE);
        ui_draw_str(bx + 14, 366, mnames[i], bw - 28, COL_TEXT); ui_draw_str(bx + 14, 394, mcost[i], bw - 28, COL_BLUE);
    }
    hal_set_text_bg(COL_BG);
    ui_draw_str(mx, sh - 28, g_toast[0] ? g_toast : "油價、碳權與 16 種隨機事件固定每 5 分鐘刷新一次！", mw, COL_BLUE);
}

static void render_tab_company(int mx, int mw, int sh) {
    const char *stabs[3] = { "員工招募與工資", "無上限樞紐擴張", "餐飲與航空聯盟" };
    for (int i = 0; i < 3; i++) {
        int tx = mx + i * 162; uint32_t bg = (g_comp_subtab == i) ? COL_BLUE : COL_WHITE;
        ui_fill_rrect(tx, 50, 154, 30, 8, bg); if (g_comp_subtab != i) ui_draw_rrect(tx, 50, 154, 30, 8, COL_BORDER);
        hal_set_text_bg(bg); ui_draw_str(tx + 14, 56, stabs[i], 132, (g_comp_subtab == i) ? COL_WHITE : COL_TEXT);
    }
    if (g_comp_subtab == 0) {
        int up, uc, ue, ut; am_fleet_crew_used(&up, &uc, &ue, &ut);
        char ps[24]; fmt_money(am_calc_payroll_cost(), ps, sizeof(ps));
        ui_card(mx, 90, mw, 48, 10, COL_WHITE, COL_BORDER, COL_SHADOW, 0, 0); hal_set_text_bg(COL_WHITE);
        char st[96]; snprintf(st, sizeof(st), "薪資:%d%% (每5分發薪 %s) · 士氣:%d%% · 訓點:%d", g_am.staff_salary_pct, ps, g_am.staff_morale, g_am.training_points);
        ui_draw_str(mx + 14, 104, st, 418, COL_TEXT);
        ui_fill_rrect(mx + 440, 98, 76, 32, 7, COL_PANEL); ui_draw_rrect(mx + 440, 98, 76, 32, 7, COL_BORDER); hal_set_text_bg(COL_PANEL); ui_draw_str(mx + 462, 105, "-5%", 40, COL_TEXT);
        ui_fill_rrect(mx + 528, 98, 76, 32, 7, COL_BLUE); hal_set_text_bg(COL_BLUE); ui_draw_str(mx + 548, 105, "+5%", 40, COL_WHITE);

        int bw4 = (mw - 24) / 4;
        const char *dn[4] = { "機長 Pilots", "空服員 Crew", "工程師 Eng.", "地勤 Tech" };
        int dc[4] = { g_am.staff_pilots, g_am.staff_crew, g_am.staff_engineers, g_am.staff_techs };
        int du[4] = { up, uc, ue, ut };
        for (int i = 0; i < 4; i++) {
            int bx = mx + i * (bw4 + 8);
            ui_card(bx, 148, bw4, 82, 10, COL_WHITE, COL_BORDER, COL_SHADOW, 0, 0); hal_set_text_bg(COL_WHITE);
            char c1[32]; snprintf(c1, sizeof(c1), "編制 %d (用%d)", dc[i], du[i]);
            ui_draw_str(bx + 10, 158, dn[i], bw4 - 20, COL_BLUE);
            ui_draw_str(bx + 10, 182, c1, bw4 - 20, COL_TEXT);
            ui_draw_str(bx + 10, 204, "點此招募 +10人", bw4 - 20, COL_GREEN);
        }
        int bw3 = (mw - 20) / 3;
        const char *tn[3] = { "管理訓練 (+2%聲譽/級)", "節油訓練 (-2%油耗/級)", "維修訓練 (-10%磨損/級)" };
        int tl[3] = { g_am.train_mgmt_lv, g_am.train_fuel_lv, g_am.train_maint_lv };
        for (int i = 0; i < 3; i++) {
            int bx = mx + i * (bw3 + 10);
            ui_card(bx, 268, bw3, 86, 12, COL_WHITE, COL_AMBER, COL_SHADOW, 0, 0); hal_set_text_bg(COL_WHITE);
            char lv[32]; snprintf(lv, sizeof(lv), "等級：Lv.%d / 5", tl[i]);
            ui_draw_str(bx + 12, 282, tn[i], bw3 - 24, COL_TEXT); ui_draw_str(bx + 12, 312, lv, bw3 - 24, COL_AMBER);
        }
    } else if (g_comp_subtab == 1) {
        ui_card(mx, 94, mw, 110, 12, COL_WHITE, COL_BORDER, COL_SHADOW, 0, 0); hal_set_text_bg(COL_WHITE);
        char hhdr[64]; snprintf(hhdr, sizeof(hhdr), "已擁有全球樞紐機場：%d 座 (無數量上限!)", g_am.hub_count);
        ui_draw_str(mx + 16, 108, hhdr, mw - 32, COL_AMBER);
        char hlist[180] = {0};
        for (int i = 0; i < g_am.hub_count && i < 20; i++) { char seg[16]; snprintf(seg, sizeof(seg), "%s ", am_airport_get(g_am.hubs[i])->iata); strncat(hlist, seg, sizeof(hlist) - strlen(hlist) - 1); }
        ui_draw_str(mx + 16, 144, hlist, mw - 32, COL_BLUE);
        const AirportInfo *ap = am_airport_get(g_ops_hub_ap);
        int64_t hcost = 3500000LL + (int64_t)(g_am.hub_count < 10 ? g_am.hub_count : 10) * 1000000LL;
        char cs[24], apinfo[128]; fmt_money(hcost, cs, sizeof(cs));
        ui_card(mx, 224, mw, 178, 12, COL_WHITE, COL_BLUE, COL_SHADOW, 0, 0); hal_set_text_bg(COL_WHITE);
        snprintf(apinfo, sizeof(apinfo), "目標樞紐：[%s] %s (%s) - %s", am_country_name_zh(ap->country), ap->icao, ap->iata, ap->name);
        ui_draw_str(mx + 16, 244, apinfo, mw - 32, COL_TEXT);
        ui_fill_rrect(mx + 16, 336, 270, 46, 9, COL_BLUE); hal_set_text_bg(COL_BLUE); ui_draw_str(mx + 34, 350, "七大洲 -> 203國挑選機場 >", 240, COL_WHITE);
        ui_fill_rrect(mx + mw - 230, 336, 214, 46, 9, COL_GREEN); hal_set_text_bg(COL_GREEN);
        char bbtn[48]; snprintf(bbtn, sizeof(bbtn), "購置樞紐 (%s)", cs); ui_draw_str(mx + mw - 214, 350, bbtn, 190, COL_WHITE);
    } else {
        int bw = (mw - 20) / 3;
        const char *cnm[3] = { "無免費餐飲 (免費)", "經濟輕食 (+6%營收)", "五星主廚 (+14%營收)" };
        const char *cds[3] = { "標準廉航模式", "升級費 $450K · 聲譽+3%", "升級費 $1.5M · 聲譽+7%" };
        for (int i = 0; i < 3; i++) {
            int bx = mx + i * (bw + 10), act = (g_am.catering_level == i);
            ui_card(bx, 126, bw, 76, 12, act ? 0xEFF6FF : COL_WHITE, act ? COL_GREEN : COL_BORDER, COL_SHADOW, act, COL_GREEN);
            hal_set_text_bg(act ? 0xEFF6FF : COL_WHITE); ui_draw_str(bx + 12, 140, cnm[i], bw - 24, act ? COL_GREEN : COL_TEXT); ui_draw_str(bx + 12, 168, cds[i], bw - 24, COL_MUTED);
        }
        const char *anm[3] = { "Star Alliance", "SkyTeam", "oneworld" };
        const char *azh[3] = { "星空聯盟 (全球最大)", "天合聯盟 (歐亞樞紐)", "寰宇一家 (澳英美幹線)" };
        for (int i = 0; i < 3; i++) {
            int bx = mx + i * (bw + 10), act = (g_am.alliance_id == i + 1);
            ui_card(bx, 258, bw, 84, 12, act ? 0xEFF6FF : COL_WHITE, act ? COL_BLUE : COL_BORDER, COL_SHADOW, act, COL_BLUE);
            hal_set_text_bg(act ? 0xEFF6FF : COL_WHITE); ui_draw_str(bx + 14, 274, anm[i], bw - 28, act ? COL_BLUE : COL_TEXT); ui_draw_str(bx + 14, 304, azh[i], bw - 28, COL_MUTED);
        }
    }
    hal_set_text_bg(COL_BG); ui_draw_str(mx, sh - 28, g_toast[0] ? g_toast : "每架飛機皆須足夠機組與地勤人員才能購買！", mw, COL_BLUE);
}

static void render_tab_finance(int mx, int mw, int sh) {
    const char *ftabs[3] = { "財報與銀行貸款", "雙向航班獲利紀錄", "10家AI航空股市" };
    for (int i = 0; i < 3; i++) {
        int tx = mx + i * 162; uint32_t bg = (g_fin_subtab == i) ? COL_BLUE : COL_WHITE;
        ui_fill_rrect(tx, 50, 154, 30, 8, bg); if (g_fin_subtab != i) ui_draw_rrect(tx, 50, 154, 30, 8, COL_BORDER);
        hal_set_text_bg(bg); ui_draw_str(tx + 12, 56, ftabs[i], 134, (g_fin_subtab == i) ? COL_WHITE : COL_TEXT);
    }
    if (g_fin_subtab == 0) {
        ui_card(mx, 90, mw, 190, 12, COL_WHITE, COL_BORDER, COL_SHADOW, 0, 0); hal_set_text_bg(COL_WHITE);
        char r1[24], r2[24], l0[24], l1[96], l2[96], l3[96], l4[96];
        fmt_money(g_am.total_revenue, r1, sizeof(r1)); fmt_money(g_am.total_expenses, r2, sizeof(r2)); fmt_money(g_am.loan_balance, l0, sizeof(l0));
        snprintf(l1, sizeof(l1), "%s IPO 股價：$%d / 股", g_am.airline_name, g_am.own_share_price);
        snprintf(l2, sizeof(l2), "累計營收：%s  ·  累計支出：%s", r1, r2);
        snprintf(l3, sizeof(l3), "完成航班：%d 次 · 載客：%d 人 · 貨運：%d 噸", g_am.total_flights_done, g_am.total_pax_carried, g_am.total_cargo_tons);
        snprintf(l4, sizeof(l4), "銀行貸款餘額：%s (上限 $50M)", l0);
        ui_draw_str(mx + 16, 106, l1, 380, COL_BLUE); ui_draw_str(mx + 16, 140, l2, 380, COL_TEXT);
        ui_draw_str(mx + 16, 174, l3, 380, COL_MUTED); ui_draw_str(mx + 16, 212, l4, 380, (g_am.loan_balance > 0) ? COL_RED : COL_GREEN);
        ui_draw_sparkline_light(mx + mw - 246, 106, 230, 148, g_am.own_share_hist, AM_PRICE_HIST, COL_BLUE);
        int bw = (mw - 20) / 3;
        ui_card(mx, 294, bw, 48, 10, COL_BLUE, COL_BLUE, COL_SHADOW, 0, 0); hal_set_text_bg(COL_BLUE); ui_draw_str(mx + 34, 309, "銀行貸款 +$2M", bw - 40, COL_WHITE);
        ui_card(mx + bw + 10, 294, bw, 48, 10, COL_PURPLE, COL_PURPLE, COL_SHADOW, 0, 0); hal_set_text_bg(COL_PURPLE); ui_draw_str(mx + bw + 44, 309, "銀行貸款 +$5M", bw - 40, COL_WHITE);
        ui_card(mx + (bw + 10) * 2, 294, bw, 48, 10, COL_GREEN, COL_GREEN, COL_SHADOW, 0, 0); hal_set_text_bg(COL_GREEN); ui_draw_str(mx + (bw + 10) * 2 + 34, 309, "償還貸款 $2M", bw - 40, COL_WHITE);
    } else if (g_fin_subtab == 1) {
        ui_card(mx, 90, mw, 344, 12, COL_WHITE, COL_BORDER, COL_SHADOW, 0, 0); hal_set_text_bg(COL_WHITE);
        ui_draw_str(mx + 16, 102, "最近 12 趟雙向往返航班獲利結算：", mw - 32, COL_BLUE);
        for (int i = 0; i < g_am.flight_log_count && i < 11; i++) {
            const FlightLogEntry *fl = &g_am.flight_log[i]; char ps[24], ln[120]; fmt_money(fl->net_profit, ps, sizeof(ps));
            snprintf(ln, sizeof(ln), "#%02d %s (%s) %s:%d 淨利 +%s", i + 1, fl->reg, fl->route, fl->is_cargo ? "貨運噸" : "載客", fl->pax_or_tons, ps);
            ui_draw_str(mx + 16, 134 + i * 26, ln, mw - 32, (i == 0) ? COL_GREEN : COL_TEXT);
        }
    } else {
        if (g_stock_sel < g_stock_scroll) g_stock_scroll = g_stock_sel;
        if (g_stock_sel >= g_stock_scroll + 5) g_stock_scroll = g_stock_sel - 4;
        for (int r = 0; r < 5; r++) {
            int idx = g_stock_scroll + r; if (idx >= AM_NUM_RIVALS) break;
            const RivalAirline *rv = &g_am.rivals[idx]; int ry = 94 + r * 64;
            ui_card(mx, ry, mw, 56, 10, COL_WHITE, COL_BORDER, COL_SHADOW, (idx == g_stock_sel), COL_BLUE); hal_set_text_bg(COL_WHITE);
            char ln[96]; snprintf(ln, sizeof(ln), "[%s] %s · $%d/股 · 持有:%d股", rv->code, rv->name, rv->price, rv->owned_shares);
            ui_draw_str(mx + 14, ry + 18, ln, mw - 290, COL_TEXT);
            ui_draw_sparkline_light(mx + mw - 270, ry + 10, 84, 36, rv->hist, AM_PRICE_HIST, COL_GREEN);
            ui_fill_rrect(mx + mw - 178, ry + 13, 84, 30, 7, COL_GREEN); hal_set_text_bg(COL_GREEN); ui_draw_str(mx + mw - 166, ry + 19, "買100股", 68, COL_WHITE);
            ui_fill_rrect(mx + mw - 86,  ry + 13, 76, 30, 7, COL_RED);   hal_set_text_bg(COL_RED);   ui_draw_str(mx + mw - 76,  ry + 19, "賣100", 58, COL_WHITE);
        }
    }
    hal_set_text_bg(COL_BG); ui_draw_str(mx, sh - 28, g_toast[0] ? g_toast : "如需直接指定精準金額，請點頂部紫色 [外掛MOD]！", mw, COL_BLUE);
}

static void render_tab_bonus(int mx, int mw, int sh) {
    int bw = (mw - 24) / 4;
    const char *pnm[4] = { "領每日免費寶箱!", "50PTS -> $2.5M", "40PTS -> +5M油", "60PTS -> 聲譽+5%" };
    for (int i = 0; i < 4; i++) {
        int bx = mx + i * (bw + 8);
        ui_card(bx, 50, bw, 52, 10, (i == 0) ? COL_GREEN : COL_WHITE, (i == 0) ? COL_GREEN : COL_AMBER, COL_SHADOW, 0, 0);
        hal_set_text_bg((i == 0) ? COL_GREEN : COL_WHITE); ui_draw_str(bx + 10, 66, pnm[i], bw - 16, (i == 0) ? COL_WHITE : COL_AMBER);
    }
    ui_card(mx, 114, mw, 72, 12, COL_WHITE, COL_BLUE, COL_SHADOW, 0, 0); hal_set_text_bg(COL_WHITE);
    const AirportInfo *co = am_airport_get(g_am.charter_orig), *cd = am_airport_get(g_am.charter_dest);
    char crw[24], ct1[112], ct2[112]; fmt_money(g_am.charter_reward, crw, sizeof(crw));
    snprintf(ct1, sizeof(ct1), "VIP包機：%s (%s -> %s)", am_charter_title(g_am.charter_type), co->iata, cd->iata);
    snprintf(ct2, sizeof(ct2), "報酬：%s + %d PTS (完成 %d 次)", crw, g_am.charter_pts, g_am.charter_done_cnt);
    ui_draw_str(mx + 14, 126, ct1, mw - 256, COL_TEXT); ui_draw_str(mx + 14, 154, ct2, mw - 256, COL_GREEN);
    ui_fill_rrect(mx + mw - 236, 130, 126, 38, 8, COL_GREEN); hal_set_text_bg(COL_GREEN); ui_draw_str(mx + mw - 224, 140, "派待命機承接", 104, COL_WHITE);
    ui_fill_rrect(mx + mw - 102, 130, 88, 38, 8, COL_PANEL); ui_draw_rrect(mx + mw - 102, 130, 88, 38, 8, COL_BORDER); hal_set_text_bg(COL_PANEL); ui_draw_str(mx + mw - 90, 140, "換合約", 64, COL_TEXT);

    int col_w = (mw - 12) / 2;
    for (int slot = 0; slot < 6; slot++) {
        int idx = g_ach_scroll * 2 + slot; if (idx >= AM_NUM_ACH) break;
        int r = slot / 2, c = slot % 2, ax = mx + c * (col_w + 12), ay = 198 + r * 68, unl = g_am.ach_unlocked[idx];
        ui_card(ax, ay, col_w, 60, 10, COL_WHITE, unl ? COL_GREEN : COL_BORDER, COL_SHADOW, unl, COL_GREEN); hal_set_text_bg(COL_WHITE);
        char rw[24], nm[64]; fmt_money(am_ach_reward(idx), rw, sizeof(rw)); snprintf(nm, sizeof(nm), "%s (%s)", am_ach_name(idx), rw);
        ui_draw_str(ax + 12, ay + 8, nm, col_w - 64, unl ? COL_GREEN : COL_TEXT); ui_draw_str(ax + 12, ay + 34, am_ach_desc(idx), col_w - 24, COL_MUTED);
        if (unl) ui_draw_badge(ax + col_w - 52, ay + 10, "DONE", COL_GREEN, COL_WHITE);
    }
    hal_set_text_bg(COL_BG); ui_draw_str(mx, sh - 28, g_toast[0] ? g_toast : "承接 VIP 包機與解鎖成就賺取高額獎金！", mw, COL_BLUE);
}

/* 多存檔槽管理器 (8 槽自由載入 / 另存自訂名稱 / 刪除) */
static void render_slots_modal(int sw, int sh) {
    int mw = 640, mh = 428, mx = (sw - mw) / 2, my = (sh - mh) / 2;
    hal_clear_text_in_rect(mx - 8, my - 8, mw + 16, mh + 16); hal_dim_rect(0, 0, sw, sh);
    ui_card(mx, my, mw, mh, 14, COL_WHITE, COL_BLUE, 0x475569, 1, COL_BLUE);
    ui_fill_rrect(mx + 2, my + 2, mw - 4, 40, 12, COL_HUD); hal_set_text_bg(COL_HUD);
    ui_draw_str(mx + 18, my + 11, g_slot_mode == 1 ? "另存新檔 (輸入自訂名稱後點選存檔槽)" : "選擇要載入的航空帝國存檔 (共 8 槽)", mw - 36, COL_WHITE);

    if (g_slot_mode == 1) {
        ui_fill_rrect(mx + 18, my + 48, mw - 196, 36, 8, 0xEFF6FF); ui_draw_rrect(mx + 18, my + 48, mw - 196, 36, 8, COL_BLUE);
        hal_set_text_bg(0xEFF6FF); char sn[64]; snprintf(sn, sizeof(sn), "自訂存檔名：%s_", g_save_input); ui_draw_str(mx + 28, my + 57, sn, mw - 216, COL_TEXT);
        ui_fill_rrect(mx + mw - 166, my + 48, 76, 36, 7, 0x64748B); hal_set_text_bg(0x64748B); ui_draw_str(mx + mw - 156, my + 57, "退格←", 58, COL_WHITE);
        ui_fill_rrect(mx + mw - 84,  my + 48, 66, 36, 7, COL_RED);  hal_set_text_bg(COL_RED);  ui_draw_str(mx + mw - 72,  my + 57, "清空", 46, COL_WHITE);
    }

    int top_y = (g_slot_mode == 1) ? (my + 94) : (my + 52);
    int row_h = (g_slot_mode == 1) ? 36 : 42;
    for (int i = 0; i < AM_MAX_SAVE_SLOTS; i++) {
        int ry = top_y + i * row_h, sel = (i == g_slot_sel);
        const SaveSlotMeta *m = &g_slots.slots[i];
        uint32_t bg = sel ? 0xEFF6FF : COL_PANEL;
        ui_fill_rrect(mx + 18, ry, mw - 108, row_h - 4, 7, bg); ui_draw_rrect(mx + 18, ry, mw - 108, row_h - 4, 7, sel ? COL_BLUE : COL_BORDER);
        hal_set_text_bg(bg);
        char ln[112];
        if (m->used) {
            char cs[24]; fmt_money(m->cash, cs, sizeof(cs));
            snprintf(ln, sizeof(ln), "[槽%d] %s · %s (Lv.%d · %s · %d機)", i + 1, m->save_name, m->airline_name, m->level, cs, m->fleet_count);
        } else snprintf(ln, sizeof(ln), "[槽%d] --- 空白存檔槽 ---", i + 1);
        ui_draw_str(mx + 28, ry + (row_h - 20) / 2, ln, mw - 128, m->used ? COL_TEXT : COL_MUTED);
        if (m->used) {
            ui_fill_rrect(mx + mw - 84, ry, 66, row_h - 4, 7, COL_RED); hal_set_text_bg(COL_RED);
            ui_draw_str(mx + mw - 70, ry + (row_h - 20) / 2, "刪除", 44, COL_WHITE);
        }
    }
    ui_fill_rrect(mx + mw / 2 - 70, my + mh - 40, 140, 32, 8, 0x334155); hal_set_text_bg(0x334155); ui_draw_str(mx + mw / 2 - 22, my + mh - 33, "返回", 50, COL_WHITE);
}

/* 2 頁史詩級 MOD MENU */
static void render_cheat_modal(int sw, int sh) {
    int mw = 620, mh = 420, mx = (sw - mw) / 2, my = (sh - mh) / 2;
    hal_clear_text_in_rect(mx - 8, my - 8, mw + 16, mh + 16); hal_dim_rect(0, 0, sw, sh);
    ui_card(mx, my, mw, mh, 14, COL_WHITE, COL_PURPLE, 0x1E293B, 1, COL_PURPLE);
    ui_fill_rrect(mx + 2, my + 2, mw - 4, 40, 12, COL_PURPLE); hal_set_text_bg(COL_PURPLE);
    ui_draw_str(mx + 150, my + 11, "史詩級外掛 / 上帝模式控制台 (MOD)", 340, COL_WHITE);

    ui_fill_rrect(mx + 18, my + 48, 286, 30, 7, g_mod_page == 0 ? COL_BLUE : COL_PANEL);
    hal_set_text_bg(g_mod_page == 0 ? COL_BLUE : COL_PANEL); ui_draw_str(mx + 70, my + 54, "1. 精準改錢與上帝開關", 200, g_mod_page == 0 ? COL_WHITE : COL_TEXT);
    ui_fill_rrect(mx + 314, my + 48, 288, 30, 7, g_mod_page == 1 ? COL_BLUE : COL_PANEL);
    hal_set_text_bg(g_mod_page == 1 ? COL_BLUE : COL_PANEL); ui_draw_str(mx + 356, my + 54, "2. 指定機型全國/全球開航!", 220, g_mod_page == 1 ? COL_WHITE : COL_TEXT);

    if (g_mod_page == 0) {
        /* 精準輸入任意金額列 */
        ui_fill_rrect(mx + 18, my + 88, mw - 224, 38, 8, 0xEFF6FF); ui_draw_rrect(mx + 18, my + 88, mw - 224, 38, 8, COL_BLUE);
        hal_set_text_bg(0xEFF6FF); char cs[64]; snprintf(cs, sizeof(cs), "直接設定金額 (打數字)：$%s_", g_mod_cash_str);
        ui_draw_str(mx + 28, my + 98, cs, mw - 244, COL_TEXT);
        ui_fill_rrect(mx + mw - 198, my + 88, 70, 38, 8, 0x64748B); hal_set_text_bg(0x64748B); ui_draw_str(mx + mw - 186, my + 98, "退格←", 52, COL_WHITE);
        ui_fill_rrect(mx + mw - 122, my + 88, 104, 38, 8, COL_GREEN); hal_set_text_bg(COL_GREEN); ui_draw_str(mx + mw - 108, my + 98, "套用金額", 80, COL_WHITE);

        int cw = (mw - 52) / 3;
        const char *btns[5][3] = {
            { "+$100,000,000", "+$1,000,000,000", "+500 PTS & 10訓" },
            { g_am.cheat_god_fuel ? "無限免費油 [開]" : "無限免費油 [關]",
              g_am.cheat_no_wear  ? "零磨損無敵 [開]" : "零磨損無敵 [關]",
              g_am.cheat_no_staff_req ? "免員工限制 [開]" : "免員工限制 [關]" },
            { "油/碳擴滿 50M", "瞬移完成全航班!", "聲譽99%&滿廣告" },
            { "解鎖全球所有樞紐", "員工+500&全訓Lv5", "手動刷新市場事件" },
            { "解鎖全 16 成就", "光速時間 1000x!", "返回遊戲 [ESC]" }
        };
        uint32_t cols[5][3] = {
            { COL_GREEN, 0x047857, COL_AMBER },
            { g_am.cheat_god_fuel ? COL_GREEN : 0x475569, g_am.cheat_no_wear ? COL_GREEN : 0x475569, g_am.cheat_no_staff_req ? COL_GREEN : 0x475569 },
            { COL_BLUE, COL_RED, COL_CYAN },
            { COL_PURPLE, COL_BLUE, COL_AMBER },
            { COL_AMBER, COL_PURPLE, 0x334155 }
        };
        for (int r = 0; r < 5; r++)
            for (int c = 0; c < 3; c++) {
                int bx = mx + 18 + c * (cw + 8), by = my + 136 + r * 52;
                ui_fill_rrect(bx, by, cw, 44, 8, cols[r][c]); hal_set_text_bg(cols[r][c]);
                ui_draw_str(bx + 12, by + 13, btns[r][c], cw - 20, COL_WHITE);
            }
    } else {
        const AircraftModel *m = am_aircraft_get(g_mod_model_idx);
        const char *cc = am_country_code(g_mod_country_idx);
        ui_card(mx + 18, my + 100, mw - 36, 50, 9, COL_PANEL, COL_BLUE, 0xE2E8F0, 0, 0); hal_set_text_bg(COL_PANEL);
        char ml[96]; snprintf(ml, sizeof(ml), "<  指定機型：%s (航程 %ukm · 載客 %u)  >", m->model, (unsigned)m->range_km, (unsigned)m->cap);
        ui_draw_str(mx + 34, my + 116, ml, mw - 68, COL_BLUE);

        ui_card(mx + 18, my + 164, mw - 36, 50, 9, COL_PANEL, COL_AMBER, 0xE2E8F0, 0, 0); hal_set_text_bg(COL_PANEL);
        char cl[96]; snprintf(cl, sizeof(cl), "目標國家：%s (%s)  ·  [點擊從全球 203 國挑選 >]", am_country_name_zh(cc), cc);
        ui_draw_str(mx + 34, my + 180, cl, mw - 68, COL_AMBER);

        ui_fill_rrect(mx + 18, my + 236, mw - 36, 56, 10, COL_GREEN); hal_set_text_bg(COL_GREEN);
        ui_draw_str(mx + 108, my + 255, "一鍵用指定機型開通【該國所有機場】航線！", 420, COL_WHITE);

        ui_fill_rrect(mx + 18, my + 306, mw - 36, 56, 10, COL_RED); hal_set_text_bg(COL_RED);
        ui_draw_str(mx + 102, my + 325, "一鍵用指定機型開通【全世界所有機場】航線！", 420, COL_WHITE);
    }
    hal_set_text_bg(COL_WHITE); ui_draw_str(mx + 20, my + mh - 28, g_toast[0] ? g_toast : "點任一功能立即生效並存檔！", mw - 40, COL_PURPLE);
}

static void render_airport_picker(int sw, int sh) {
    int mw = 640, mh = 432, mx = (sw - mw) / 2, my = (sh - mh) / 2;
    hal_clear_text_in_rect(mx - 8, my - 8, mw + 16, mh + 16); hal_dim_rect(0, 0, sw, sh);
    ui_card(mx, my, mw, mh, 14, COL_WHITE, COL_BLUE, 0x64748B, 1, COL_BLUE);
    ui_fill_rrect(mx + 2, my + 2, mw - 4, 40, 12, COL_HUD);
    ui_fill_rrect(mx + 10, my + 7, 94, 26, 7, 0x334155); hal_set_text_bg(0x334155);
    ui_draw_str(mx + 20, my + 11, g_pick_step == 1 ? "< 換國家" : "< 取消", 76, COL_WHITE);
    hal_set_text_bg(COL_HUD);
    if (g_pick_step == 0) {
        char hdr[96]; snprintf(hdr, sizeof(hdr), "全球 %d 國 / %d 座機場 · %s (%d國)", am_country_count(), am_airport_count(), am_continent_name(g_pick_cont), g_pick_c_cnt);
        ui_draw_str(mx + 116, my + 11, hdr, mw - 130, COL_WHITE);
        for (int c = 0; c < AM_NUM_CONTINENTS; c++) {
            int cx = mx + 14 + c * 88; uint32_t bg = (g_pick_cont == c) ? COL_BLUE : COL_PANEL;
            ui_fill_rrect(cx, my + 48, 82, 28, 7, bg); if (g_pick_cont != c) ui_draw_rrect(cx, my + 48, 82, 28, 7, COL_BORDER);
            hal_set_text_bg(bg); ui_draw_str(cx + 16, my + 53, am_continent_name(c), 56, (g_pick_cont == c) ? COL_WHITE : COL_TEXT);
        }
        for (int slot = 0; slot < 12; slot++) {
            int pos = g_pick_c_page * 12 + slot; if (pos >= g_pick_c_cnt) break;
            int c_idx = g_pick_c_list[pos], r = slot / 3, c = slot % 3, bx = mx + 18 + c * 202, by = my + 88 + r * 72, sel = (pos == g_pick_c_sel);
            const char *code = am_country_code(c_idx); int tmp[64], n_ap = am_airports_in_country(code, tmp, 64);
            ui_card(bx, by, 192, 62, 10, sel ? COL_BLUE : COL_PANEL, COL_BORDER, 0xDCE5EE, sel, COL_BLUE);
            hal_set_text_bg(sel ? COL_BLUE : COL_PANEL);
            char ln1[48], ln2[32]; snprintf(ln1, sizeof(ln1), "%s (%s)", am_country_name_zh(code), code); snprintf(ln2, sizeof(ln2), "%d 座機場", n_ap);
            ui_draw_str(bx + 10, by + 10, ln1, 172, sel ? COL_WHITE : COL_TEXT); ui_draw_str(bx + 10, by + 36, ln2, 172, sel ? 0xE0F2FE : COL_MUTED);
        }
        ui_fill_rrect(mx + 18, my + mh - 40, 140, 30, 8, COL_PANEL); ui_draw_rrect(mx + 18, my + mh - 40, 140, 30, 8, COL_BORDER); hal_set_text_bg(COL_PANEL); ui_draw_str(mx + 48, my + mh - 34, "< 上頁", 90, COL_TEXT);
        ui_fill_rrect(mx + mw - 158, my + mh - 40, 140, 30, 8, COL_PANEL); ui_draw_rrect(mx + mw - 158, my + mh - 40, 140, 30, 8, COL_BORDER); ui_draw_str(mx + mw - 128, my + mh - 34, "下頁 >", 90, COL_TEXT);
    } else {
        int c_idx = g_pick_c_list[g_pick_c_sel]; const char *cc = am_country_code(c_idx);
        char hdr[96]; snprintf(hdr, sizeof(hdr), "%s (%s) 全部商業機場 (%d座)", am_country_name_zh(cc), cc, g_pick_ap_cnt);
        ui_draw_str(mx + 116, my + 11, hdr, mw - 130, COL_WHITE);
        for (int r = 0; r < 6; r++) {
            int idx = g_pick_ap_scroll + r; if (idx >= g_pick_ap_cnt) break;
            const AirportInfo *ap = am_airport_get(g_pick_ap_list[idx]); int ry = my + 56 + r * 52, sel = (idx == g_pick_ap_sel);
            uint32_t bg = sel ? COL_BLUE : COL_PANEL; ui_fill_rrect(mx + 18, ry, mw - 36, 44, 9, bg); if (!sel) ui_draw_rrect(mx + 18, ry, mw - 36, 44, 9, COL_BORDER);
            hal_set_text_bg(bg); char ln[128]; snprintf(ln, sizeof(ln), "%s/%s %s · 跑道:%uft · Y%u/J%u/F%u", ap->icao, ap->iata, ap->name, (unsigned)ap->runway_ft, (unsigned)ap->dem_y, (unsigned)ap->dem_j, (unsigned)ap->dem_f);
            ui_draw_str(mx + 28, ry + 13, ln, mw - 56, sel ? COL_WHITE : COL_TEXT);
        }
        ui_fill_rrect(mx + 18, my + mh - 40, 140, 30, 8, COL_PANEL); ui_draw_rrect(mx + 18, my + mh - 40, 140, 30, 8, COL_BORDER); hal_set_text_bg(COL_PANEL); ui_draw_str(mx + 48, my + mh - 34, "< 上捲", 90, COL_TEXT);
        ui_fill_rrect(mx + mw - 158, my + mh - 40, 140, 30, 8, COL_PANEL); ui_draw_rrect(mx + mw - 158, my + mh - 40, 140, 30, 8, COL_BORDER); ui_draw_str(mx + mw - 128, my + mh - 34, "下捲 >", 90, COL_TEXT);
    }
}

static void render_shop_detail(int sw, int sh) {
    int mw = 640, mh = 430, mx = (sw - mw) / 2, my = (sh - mh) / 2;
    hal_clear_text_in_rect(mx - 8, my - 8, mw + 16, mh + 16); hal_dim_rect(0, 0, sw, sh);
    ui_card(mx, my, mw, mh, 14, COL_WHITE, COL_BLUE, 0x64748B, 1, COL_BLUE);
    const AircraftModel *m = am_aircraft_get(g_shop_detail_model);

    ui_draw_aircraft_card_art(mx + 148, my + 78, 256, 118, m->silhouette, COL_BLUE); hal_set_text_bg(COL_WHITE);
    char pr[24], s1[64], s2[64], s3[64], s4[80]; fmt_money(m->price * g_buy_qty, pr, sizeof(pr));
    ui_draw_str(mx + 294, my + 14, m->model, 320, COL_BLUE);
    snprintf(s1, sizeof(s1), "總價 (%d架)：%s", g_buy_qty, pr);
    snprintf(s2, sizeof(s2), "航程：%u km · 巡航：%u kph · 跑道：%uft", (unsigned)m->range_km, (unsigned)m->speed_kmh, (unsigned)m->runway_ft);
    int rp, rc, re, rt; am_aircraft_crew_req(g_shop_detail_model, &rp, &rc, &re, &rt);
    snprintf(s3, sizeof(s3), "單機需編制：機長%d 空服%d 工%d 地%d", rp, rc, re, rt);
    snprintf(s4, sizeof(s4), "油耗：%u · CO2：0.%02u · A檢：%uh", (unsigned)m->fuel_lbs_km, (unsigned)m->co2_kg, (unsigned)m->acheck_hrs);
    ui_draw_str(mx + 294, my + 40, s1, 330, COL_AMBER); ui_draw_str(mx + 294, my + 66, s2, 330, COL_TEXT);
    ui_draw_str(mx + 294, my + 92, s3, 330, COL_GREEN); ui_draw_str(mx + 294, my + 118, s4, 330, COL_MUTED);

    ui_card(mx + 20, my + 148, mw - 40, 62, 10, COL_PANEL, COL_BORDER, 0xE2E8F0, 0, 0); hal_set_text_bg(COL_PANEL);
    ui_draw_str(mx + 30, my + 160, m->desc, mw - 60, COL_TEXT);

    /* 批量購買數量選擇列 (x1 / x5 / x10 / x25 / x50) */
    hal_set_text_bg(COL_WHITE); ui_draw_str(mx + 22, my + 230, "批量購買數量：", 140, COL_TEXT);
    static const int QTYS[5] = { 1, 5, 10, 25, 50 };
    for (int i = 0; i < 5; i++) {
        int bx = mx + 170 + i * 86; uint32_t bg = (g_buy_qty == QTYS[i]) ? COL_BLUE : COL_PANEL;
        ui_fill_rrect(bx, my + 224, 78, 32, 7, bg); if (g_buy_qty != QTYS[i]) ui_draw_rrect(bx, my + 224, 78, 32, 7, COL_BORDER);
        hal_set_text_bg(bg); char ql[12]; snprintf(ql, sizeof(ql), "x%d 架", QTYS[i]); ui_draw_str(bx + 16, my + 231, ql, 54, (g_buy_qty == QTYS[i]) ? COL_WHITE : COL_TEXT);
    }

    ui_card(mx + 20, my + 268, mw - 40, 82, 10, COL_PANEL, COL_BLUE, 0xE2E8F0, 0, 0); hal_set_text_bg(COL_PANEL);
    if (m->is_cargo) {
        char cg[64]; snprintf(cg, sizeof(cg), "全貨機貨艙容量：%u 公噸", (unsigned)m->cap); ui_draw_str(mx + 32, my + 298, cg, mw - 64, COL_TEXT);
    } else {
        char st[96]; snprintf(st, sizeof(st), "客艙配置 -> 經濟 Y:%d  商務 J:%d  頭等 F:%d", g_cfg_y, g_cfg_j, g_cfg_f);
        ui_draw_str(mx + 32, my + 278, st, mw - 64, COL_TEXT);
        ui_fill_rrect(mx + 240, my + 308, 56, 28, 6, COL_WHITE); ui_draw_rrect(mx + 240, my + 308, 56, 28, 6, COL_BORDER); hal_set_text_bg(COL_WHITE); ui_draw_str(mx + 250, my + 313, "J -4", 40, COL_TEXT);
        ui_fill_rrect(mx + 304, my + 308, 56, 28, 6, COL_BLUE); hal_set_text_bg(COL_BLUE); ui_draw_str(mx + 314, my + 313, "J +4", 40, COL_WHITE);
        ui_fill_rrect(mx + 440, my + 308, 56, 28, 6, COL_WHITE); ui_draw_rrect(mx + 440, my + 308, 56, 28, 6, COL_BORDER); hal_set_text_bg(COL_WHITE); ui_draw_str(mx + 450, my + 313, "F -2", 40, COL_TEXT);
        ui_fill_rrect(mx + 504, my + 308, 56, 28, 6, COL_AMBER); hal_set_text_bg(COL_AMBER); ui_draw_str(mx + 514, my + 313, "F +2", 40, COL_WHITE);
    }

    uint32_t bbuy = (g_am.cash >= m->price * g_buy_qty) ? COL_GREEN : 0x94A3B8;
    ui_fill_rrect(mx + 20, my + mh - 52, 290, 40, 10, bbuy); hal_set_text_bg(bbuy);
    char btxt[48]; snprintf(btxt, sizeof(btxt), "確認批量購買 %d 架", g_buy_qty); ui_draw_str(mx + 86, my + mh - 41, btxt, 180, COL_WHITE);
    ui_fill_rrect(mx + 330, my + mh - 52, 290, 40, 10, COL_PANEL); ui_draw_rrect(mx + 330, my + mh - 52, 290, 40, 10, COL_BORDER);
    hal_set_text_bg(COL_PANEL); ui_draw_str(mx + 422, my + mh - 41, "返回機型清單", 120, COL_TEXT);
}

static void render_starter_aircraft_screen(int sw, int sh) {
    ui_draw_daylight_sky_clouds(sw, sh);
    int mw = 620, mh = 386, mx = (sw - mw) / 2, my = (sh - mh) / 2;
    ui_card(mx, my, mw, mh, 14, COL_WHITE, COL_BLUE, 0x94A3B8, 1, COL_BLUE);
    ui_fill_rrect(mx + 2, my + 2, mw - 4, 42, 12, 0x1E88E5); hal_set_text_bg(0x1E88E5);
    ui_draw_str(mx + 168, my + 13, "選擇你的首架起始客機 (免費交付)", 300, COL_WHITE);
    int m_ids[2] = { find_model_by_substr("A320-200", 9), find_model_by_substr("737-800", 36) };
    uint32_t tcols[2] = { 0x1D4ED8, 0x0EA5E9 }; int cw = (mw - 42) / 2;
    for (int i = 0; i < 2; i++) {
        const AircraftModel *m = am_aircraft_get(m_ids[i]); int cx = mx + 14 + i * (cw + 14), cy = my + 54, sel = (g_starter_sel == i);
        ui_card(cx, cy, cw, mh - 68, 10, COL_WHITE, sel ? COL_GREEN : COL_BORDER, 0xDCE5EE, sel, COL_GREEN);
        hal_set_text_bg(COL_WHITE); ui_draw_str(cx + 68, cy + 10, m->model, cw - 80, COL_CYAN);
        ui_draw_aircraft_card_art(cx + cw / 2, cy + 68, cw - 24, 72, m->silhouette, tcols[i]);
        char rows[7][64];
        snprintf(rows[0], sizeof(rows[0]), "載客量 Capacity : %u pax", (unsigned)m->cap);
        snprintf(rows[1], sizeof(rows[1]), "油耗 Fuel       : %u Lbs/km", (unsigned)m->fuel_lbs_km);
        snprintf(rows[2], sizeof(rows[2]), "航程 Range      : %u km", (unsigned)m->range_km);
        snprintf(rows[3], sizeof(rows[3]), "速度 Speed      : %u kph", (unsigned)m->speed_kmh);
        snprintf(rows[4], sizeof(rows[4]), "最短跑道 Runway : %u ft", (unsigned)m->runway_ft);
        snprintf(rows[5], sizeof(rows[5]), "CO2 排放        : 0.%02u kg/pax/km", (unsigned)m->co2_kg);
        snprintf(rows[6], sizeof(rows[6]), "發動機 Engine   : %s", am_aircraft_engine_name(m_ids[i]));
        for (int r = 0; r < 7; r++) ui_draw_str(cx + 14, cy + 112 + r * 22, rows[r], cw - 24, (r == 1 || r == 2) ? COL_TEXT : COL_MUTED);
        ui_fill_rrect(cx + 14, cy + mh - 116, cw - 28, 36, 9, COL_GREEN); hal_set_text_bg(COL_GREEN);
        ui_draw_str(cx + 76, cy + mh - 107, "+ 選擇此客機啟航", 160, COL_WHITE);
    }
}

static void render_plane_modal(int sw, int sh) {
    int mw = 540, mh = 420, mx = (sw - mw) / 2, my = (sh - mh) / 2;
    hal_clear_text_in_rect(mx - 8, my - 8, mw + 16, mh + 16); hal_dim_rect(0, 0, sw, sh);
    ui_card(mx, my, mw, mh, 14, COL_WHITE, COL_BLUE, 0x64748B, 1, COL_BLUE);
    const FleetPlane *fp = &g_am.fleet[g_pl_fleet_idx]; const AircraftModel *m = am_aircraft_get(fp->model_idx);
    hal_set_text_bg(COL_WHITE); char hdr[96]; snprintf(hdr, sizeof(hdr), "機體維護、改裝與客艙：%s (%s)", fp->reg, m->model);
    ui_draw_str(mx + 20, my + 16, hdr, mw - 40, COL_BLUE);
    if (!m->is_cargo) {
        char st[64]; snprintf(st, sizeof(st), "座位 Y:%d J:%d F:%d", fp->seats_y, fp->seats_j, fp->seats_f);
        ui_draw_str(mx + 20, my + 62, st, 155, COL_TEXT);
        ui_fill_rrect(mx + 180, my + 56, 48, 30, 6, COL_PANEL); ui_draw_rrect(mx + 180, my + 56, 48, 30, 6, COL_BORDER); hal_set_text_bg(COL_PANEL); ui_draw_str(mx + 188, my + 62, "J-", 32, COL_TEXT);
        ui_fill_rrect(mx + 234, my + 56, 48, 30, 6, COL_BLUE); hal_set_text_bg(COL_BLUE); ui_draw_str(mx + 244, my + 62, "J+", 32, COL_WHITE);
        ui_fill_rrect(mx + 340, my + 56, 48, 30, 6, COL_PANEL); ui_draw_rrect(mx + 340, my + 56, 48, 30, 6, COL_BORDER); hal_set_text_bg(COL_PANEL); ui_draw_str(mx + 348, my + 62, "F-", 32, COL_TEXT);
        ui_fill_rrect(mx + 394, my + 56, 48, 30, 6, COL_AMBER); hal_set_text_bg(COL_AMBER); ui_draw_str(mx + 404, my + 62, "F+", 32, COL_WHITE);
    }
    char opts[8][72];
    snprintf(opts[0], sizeof(opts[0]), "修復機體磨損 (目前 %d%% · 需付維修費)", fp->wear_pct);
    snprintf(opts[1], sizeof(opts[1]), "執行 A-Check 大檢 (剩 %dh / 重置 %uh)", fp->acheck_hrs_left, (unsigned)m->acheck_hrs);
    snprintf(opts[2], sizeof(opts[2]), "引擎改裝 +10%% 速度 [%s]", fp->mod_speed ? "已安裝" : "可改裝");
    snprintf(opts[3], sizeof(opts[3]), "翼尖改裝 -10%% 油耗 [%s]", fp->mod_fuel ? "已安裝" : "可改裝");
    snprintf(opts[4], sizeof(opts[4]), "環保改裝 -10%% CO2  [%s]", fp->mod_co2 ? "已安裝" : "可改裝");
    snprintf(opts[5], sizeof(opts[5]), "雙向往返模式：[%s]", fp->auto_return ? "自動往返" : "手動單段");
    snprintf(opts[6], sizeof(opts[6]), "停飛封存 Ground A/C：[%s]", fp->status == 3 ? "封存中(點恢復)" : "服役中(點封存)");
    snprintf(opts[7], sizeof(opts[7]), "出售此架飛機回收資金");
    for (int i = 0; i < 8; i++) {
        int ry = my + 104 + i * 38; uint32_t bg = (i == g_pl_sel) ? ((i == 7) ? COL_RED : COL_BLUE) : COL_PANEL;
        ui_fill_rrect(mx + 20, ry, mw - 40, 32, 8, bg); if (i != g_pl_sel) ui_draw_rrect(mx + 20, ry, mw - 40, 32, 8, COL_BORDER);
        hal_set_text_bg(bg); ui_draw_str(mx + 34, ry + 7, opts[i], mw - 68, (i == g_pl_sel) ? COL_WHITE : COL_TEXT);
    }
}

void am_ui_render(void) {
    int sw = hal_get_width(), sh = hal_get_height();
    g_redraw = 0;

    if (g_scr == SCR_TITLE || (g_scr == SCR_SLOTS && g_slot_mode == 0)) {
        ui_draw_daylight_sky_clouds(sw, sh);
        int pw = 480, ph = 344, px = (sw - pw) / 2, py = 62;
        ui_card(px, py, pw, ph, 16, COL_WHITE, COL_BLUE, 0x94A3B8, 1, COL_BLUE); hal_set_text_bg(COL_WHITE);
        ui_draw_str(px + 54, py + 28, "AIRLINE MANAGER v6 終極遊戲版", 380, COL_BLUE);
        ui_draw_str(px + 38, py + 64, "無上限機隊/樞紐 · 8組自訂存檔 · 拖拉地圖 · 16隨機事件", 410, COL_TEXT);
        ui_draw_str(px + 52, py + 94, "批量購機 · 機組人員與固定發薪 · 全國/全球MOD一鍵開航", 390, COL_MUTED);
        int hs = any_slot_used(), n = hs ? 3 : 2;
        const char *opts[3];
        if (hs) { opts[0] = "載入 / 管理存檔 (8 個存檔槽)"; opts[1] = "創立新航空帝國 (自訂存檔名)"; opts[2] = "離開遊戲"; }
        else { opts[0] = "創立新航空帝國 (自訂存檔名)"; opts[1] = "離開遊戲"; }
        for (int i = 0; i < n; i++) {
            uint32_t bg = (i == g_title_sel) ? ((i == 0) ? COL_GREEN : COL_BLUE) : COL_PANEL;
            ui_fill_rrect(px + 56, py + 150 + i * 52, pw - 112, 42, 10, bg);
            if (i != g_title_sel) ui_draw_rrect(px + 56, py + 150 + i * 52, pw - 112, 42, 10, COL_BORDER);
            hal_set_text_bg(bg); ui_draw_str(px + 108, py + 162 + i * 52, opts[i], 280, (i == g_title_sel) ? COL_WHITE : COL_TEXT);
        }
        if (g_scr == SCR_SLOTS) render_slots_modal(sw, sh);
        hal_present(); return;
    }

    if (g_scr == SCR_STARTER_AC) { render_starter_aircraft_screen(sw, sh); hal_present(); return; }

    if (g_scr == SCR_CREATE || (g_scr == SCR_AP_PICKER && g_pick_return == SCR_CREATE)) {
        ui_draw_daylight_sky_clouds(sw, sh);
        int pw = 580, ph = 440, px = (sw - pw) / 2, py = 16;
        ui_card(px, py, pw, ph, 16, COL_WHITE, COL_BLUE, 0x94A3B8, 1, COL_BLUE);
        ui_fill_rrect(px + 2, py + 2, pw - 4, 36, 14, COL_HUD); hal_set_text_bg(COL_HUD);
        ui_draw_str(px + 162, py + 10, "創立新航空帝國 (自訂存檔名與公司)", 290, COL_WHITE);

        const char *flbl[3] = { "存檔名：", "公司名：", "2碼IATA：" };
        char *fbuf[3] = { g_new_save, g_new_name, g_new_iata };
        for (int i = 0; i < 3; i++) {
            int ry = py + 44 + i * 46; uint32_t bg = (g_create_row == i) ? 0xEFF6FF : COL_PANEL;
            ui_fill_rrect(px + 22, ry, pw - 196, 38, 9, bg); ui_draw_rrect(px + 22, ry, pw - 196, 38, 9, g_create_row == i ? COL_BLUE : COL_BORDER);
            hal_set_text_bg(bg); char ln[96]; snprintf(ln, sizeof(ln), "%s%s%s", flbl[i], fbuf[i], g_create_row == i ? "_" : "");
            ui_draw_str(px + 34, ry + 10, ln, pw - 216, COL_TEXT);
            ui_fill_rrect(px + pw - 166, ry, 76, 38, 8, 0x64748B); hal_set_text_bg(0x64748B); ui_draw_str(px + pw - 156, ry + 10, "退格←", 60, COL_WHITE);
            ui_fill_rrect(px + pw - 84,  ry, 62, 38, 8, COL_RED);  hal_set_text_bg(COL_RED);  ui_draw_str(px + pw - 74,  ry + 10, "清空", 44, COL_WHITE);
        }
        char cs[24], r2[64]; fmt_money(g_new_cash, cs, sizeof(cs)); snprintf(r2, sizeof(r2), "起始資金：%s (點切換)", cs);
        ui_fill_rrect(px + 22, py + 184, 330, 38, 9, COL_PANEL); ui_draw_rrect(px + 22, py + 184, 330, 38, 9, COL_AMBER);
        hal_set_text_bg(COL_PANEL); ui_draw_str(px + 34, py + 194, r2, 306, COL_AMBER);
        ui_fill_rrect(px + 362, py + 184, 196, 38, 9, COL_BLUE); hal_set_text_bg(COL_BLUE);
        ui_draw_str(px + 376, py + 194, g_new_realism ? "模式：Realism 擬真" : "模式：Easy 輕鬆", 172, COL_WHITE);

        const AirportInfo *ap = am_airport_get(g_new_hub_idx);
        ui_fill_rrect(px + 22, py + 232, pw - 44, 42, 9, 0xEFF6FF); ui_draw_rrect(px + 22, py + 232, pw - 44, 42, 9, COL_BLUE);
        hal_set_text_bg(0xEFF6FF); char r3[96]; snprintf(r3, sizeof(r3), "起始樞紐：[%s] %s/%s - %s (點選203國 >)", am_country_name_zh(ap->country), ap->icao, ap->iata, ap->name);
        ui_draw_str(px + 34, py + 244, r3, pw - 68, COL_BLUE);

        const char *phubs[6] = { "TPE", "SYD", "HND", "SIN", "LHR", "JFK" };
        for (int i = 0; i < 6; i++) {
            int bx = px + 22 + i * 90; ui_fill_rrect(bx, py + 286, 82, 30, 7, COL_PANEL); ui_draw_rrect(bx, py + 286, 82, 30, 7, COL_BLUE);
            hal_set_text_bg(COL_PANEL); ui_draw_str(bx + 24, py + 292, phubs[i], 44, COL_BLUE);
        }
        ui_fill_rrect(px + 64, py + 344, pw - 128, 48, 12, COL_GREEN); hal_set_text_bg(COL_GREEN);
        ui_draw_str(px + 152, py + 359, "下一步：挑選首架起始客機 >", 280, COL_WHITE);
        if (g_scr == SCR_AP_PICKER) render_airport_picker(sw, sh);
        hal_present(); return;
    }

    hal_clear(COL_BG);
    render_top_bar(sw);
    int mx = 14, mw = sw - 96;
    if (g_tab == TAB_MAP) render_tab_map(sw, sh);
    else if (g_tab == TAB_FLEET) render_tab_fleet(mx, mw, sh);
    else if (g_tab == TAB_SHOP) render_tab_shop(mx, mw, sh);
    else if (g_tab == TAB_FUEL) render_tab_fuel(mx, mw, sh);
    else if (g_tab == TAB_COMPANY) render_tab_company(mx, mw, sh);
    else if (g_tab == TAB_FINANCE) render_tab_finance(mx, mw, sh);
    else render_tab_bonus(mx, mw, sh);

    render_right_dock(sw);

    if (g_scr == SCR_PLANE_MODAL) render_plane_modal(sw, sh);
    else if (g_scr == SCR_SHOP_DETAIL) render_shop_detail(sw, sh);
    else if (g_scr == SCR_AP_PICKER) render_airport_picker(sw, sh);
    else if (g_scr == SCR_CHEAT) render_cheat_modal(sw, sh);
    else if (g_scr == SCR_SLOTS) render_slots_modal(sw, sh);
    else if (g_scr == SCR_PAUSE) {
        int pw = 360, ph = 264, px = (sw - pw) / 2, py = (sh - ph) / 2;
        hal_clear_text_in_rect(px - 8, py - 8, pw + 16, ph + 16); hal_dim_rect(0, 0, sw, sh);
        ui_card(px, py, pw, ph, 14, COL_WHITE, COL_BLUE, 0x64748B, 1, COL_BLUE); hal_set_text_bg(COL_WHITE);
        ui_draw_str(px + 126, py + 16, "系統暫停選單", 140, COL_TEXT);
        const char *po[4] = { "繼續經營 (ESC)", "另存新檔 / 切換存檔槽", "存檔並回主畫面", "存檔並離開" };
        for (int i = 0; i < 4; i++) {
            uint32_t bg = (i == g_pause_sel) ? COL_BLUE : COL_PANEL;
            ui_fill_rrect(px + 28, py + 54 + i * 48, pw - 56, 38, 8, bg);
            if (i != g_pause_sel) ui_draw_rrect(px + 28, py + 54 + i * 48, pw - 56, 38, 8, COL_BORDER);
            hal_set_text_bg(bg); ui_draw_str(px + 84, py + 64 + i * 48, po[i], 200, (i == g_pause_sel) ? COL_WHITE : COL_TEXT);
        }
    }
    hal_present();
}
