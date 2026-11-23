#include "am_sim.h"
#include "hal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

AmWorld g_am;
SaveSlotIndex g_slots;

static const int SPEED_MULTS[5] = { 1, 10, 60, 360, 1000 };
static uint64_t g_rng_state = 0x853C49E6748FEA9BULL;

static uint32_t rng_u32(void) {
    uint64_t x = g_rng_state;
    x ^= x >> 12; x ^= x << 25; x ^= x >> 27;
    g_rng_state = x;
    return (uint32_t)((x * 2685821657736338717ULL) >> 32);
}
static int rng_range(int lo, int hi) { return (hi <= lo) ? lo : (lo + (int)(rng_u32() % (uint32_t)(hi - lo + 1))); }
static void push_hist(int *arr, int val) { for (int i = 0; i < AM_PRICE_HIST - 1; i++) arr[i] = arr[i + 1]; arr[AM_PRICE_HIST - 1] = val; }

int am_sim_speed_mult(void) {
    int i = g_am.speed_mult_idx;
    return SPEED_MULTS[(i >= 0 && i < 5) ? i : 0];
}

/* 16 種豐富全球航空隨機事件！ */
const char *am_event_name(int id) {
    switch (id) {
        case 1:  return "產油國緊急減產";
        case 2:  return "國際頁岩油大增產";
        case 3:  return "全球奧運旅遊熱潮";
        case 4:  return "綠能航空碳權補貼";
        case 5:  return "跨洲商務峰會旺季";
        case 6:  return "北大西洋極端氣流";
        case 7:  return "大型煉油廠失火停產";
        case 8:  return "全球航空燃油過剩";
        case 9:  return "國際碳稅突然調漲";
        case 10: return "冰島火山灰雲擾動";
        case 11: return "跨國電商貨運爆單";
        case 12: return "全球經濟強勁復甦";
        case 13: return "區域航權開放利多";
        case 14: return "冬季暴風雪來襲";
        case 15: return "頂級富豪包機潮";
        case 16: return "機場起降費減免優惠";
        default: return "全球航空市場平穩";
    }
}

const char *am_event_desc(int id) {
    switch (id) {
        case 1:  return "OPEC 減產導致油價飆升！建議動用低價囤油";
        case 2:  return "頁岩油增產讓油價大幅跳水！絕佳抄底囤油時機";
        case 3:  return "全球旅遊熱潮，全航線載客需求 +18%！";
        case 4:  return "各國推動綠能補貼，CO2 碳權價格大降價！";
        case 5:  return "商務與頭等艙爆滿，J/F 艙收益額外 +15%！";
        case 6:  return "高空強逆風氣流，航班油耗微增 8%";
        case 7:  return "煉油廠事故推升油價暴漲，注意燃油成本";
        case 8:  return "航油庫存過剩，油價持續走低適合擴充囤油";
        case 9:  return "碳稅上調導致 CO2 碳權價格飆漲！";
        case 10: return "火山灰影響空域，繞飛導致油耗增加 10%";
        case 11: return "跨境電商暴增，貨機航班收益額外 +20%！";
        case 12: return "景氣大好，全航線載客率與票價收益 +10%！";
        case 13: return "航權開放，全體航空公司基礎聲譽臨時提升";
        case 14: return "暴風雪襲擊北半球，地面除冰略增營運成本";
        case 15: return "頭等艙需求大爆發，頭等艙票價收益 +20%！";
        case 16: return "主要機場推出減免優惠，航班淨利額外 +8%！";
        default: return "油價、碳權與隨機事件每 5 分鐘刷新一次";
    }
}

typedef struct { const char *name; const char *desc; int64_t reward; } AmAchDef;
static const AmAchDef AM_ACHS[AM_NUM_ACH] = {
    { "首航啟程",     "完成第 1 趟商業航班",            250000LL },
    { "雙向通航",     "累計完成 10 趟往返航班",         500000LL },
    { "航空新星",     "累計完成 50 趟航班",            1500000LL },
    { "區域機隊",     "機隊規模達到 5 架飛機",          600000LL },
    { "大型機隊",     "機隊規模達到 20 架飛機",        2500000LL },
    { "百機霸主",     "機隊規模達到 100 架飛機",      15000000LL },
    { "全球樞紐",     "擁有 3 座全球樞紐機場",         1000000LL },
    { "五大洲網",     "擁有 10 座全球樞紐機場",        5000000LL },
    { "低價囤油王",   "油價低於 $650 時買入燃油",       400000LL },
    { "萬人空巷",     "累計載客突破 5,000 人次",        800000LL },
    { "百萬旅運",     "累計載客突破 50,000 人次",      3000000LL },
    { "貨運大亨",     "累計航空貨運達 500 噸",         1200000LL },
    { "寬體旗艦",     "購入 1 架長程寬體或巨無霸客機", 1500000LL },
    { "超音速傳奇",   "購入協和號或超音速客機",        5000000LL },
    { "品牌巔峰",     "航空公司有效聲譽達 90% 以上",   1000000LL },
    { "億萬帝國",     "現金資產突破 $100,000,000",    10000000LL }
};

const char *am_ach_name(int id)   { return (id >= 0 && id < AM_NUM_ACH) ? AM_ACHS[id].name : ""; }
const char *am_ach_desc(int id)   { return (id >= 0 && id < AM_NUM_ACH) ? AM_ACHS[id].desc : ""; }
int64_t     am_ach_reward(int id) { return (id >= 0 && id < AM_NUM_ACH) ? AM_ACHS[id].reward : 0; }
int am_ach_unlocked_count(void) { int c = 0; for (int i = 0; i < AM_NUM_ACH; i++) if (g_am.ach_unlocked[i]) c++; return c; }

int am_airline_level(void) {
    int lv = 1 + (g_am.total_flights_done * 2 + g_am.fleet_count * 10 + g_am.hub_count * 15 + am_ach_unlocked_count() * 15) / 35;
    return lv > 50 ? 50 : lv;
}
const char *am_airline_rank_name(int lv) {
    if (lv >= 35) return "傳奇全球航空帝國";
    if (lv >= 20) return "全球航空帝國霸主";
    if (lv >= 14) return "五星洲際旗艦航空";
    if (lv >= 10) return "跨國聯盟核心成員";
    if (lv >= 6)  return "國際幹線航空公司";
    if (lv >= 3)  return "區域成長型航空";
    return "新創起飛航空公司";
}

const char *am_alliance_name(int id) {
    switch (id) {
        case 1: return "Star Alliance 星空聯盟";
        case 2: return "SkyTeam 天合聯盟";
        case 3: return "oneworld 寰宇一家";
        default: return "獨立營運 (未加入聯盟)";
    }
}
const char *am_charter_title(int type) {
    switch (type) {
        case 0: return "國際運動代表隊包機";
        case 1: return "跨國領袖高峰會專機";
        case 2: return "國際科技大展商務包機";
        default: return "緊急醫療物資專機";
    }
}

/* 每架飛機必須的機組與地勤人員編制！ */
void am_aircraft_crew_req(int model_idx, int *pilots, int *crew, int *eng, int *tech) {
    const AircraftModel *m = am_aircraft_get(model_idx);
    int p = 2, c = 2, e = 1, t = 1;
    if (m->is_cargo) {
        p = (m->cap >= 80) ? 4 : 2;
        c = 0;
        e = (m->cap >= 80) ? 3 : 2;
        t = (m->cap >= 80) ? 3 : 2;
    } else if (m->cap >= 420) { /* A380 / 747 巨無霸 */
        p = 4; c = 14; e = 4; t = 4;
    } else if (m->cap >= 250) { /* 寬體客機 */
        p = 4; c = 8; e = 3; t = 3;
    } else if (m->cap >= 120) { /* 窄體主力 */
        p = 2; c = 4; e = 2; t = 2;
    } else {                    /* 支線/螺旋槳 */
        p = 2; c = 2; e = 1; t = 1;
    }
    if (pilots) *pilots = p;
    if (crew)   *crew   = c;
    if (eng)    *eng    = e;
    if (tech)   *tech   = t;
}

void am_fleet_crew_used(int *pilots, int *crew, int *eng, int *tech) {
    int sp = 0, sc = 0, se = 0, st = 0;
    for (int i = 0; i < g_am.fleet_count; i++) {
        if (!g_am.fleet[i].active) continue;
        int p, c, e, t;
        am_aircraft_crew_req(g_am.fleet[i].model_idx, &p, &c, &e, &t);
        sp += p; sc += c; se += e; st += t;
    }
    if (pilots) *pilots = sp;
    if (crew)   *crew   = sc;
    if (eng)    *eng    = se;
    if (tech)   *tech   = st;
}

int64_t am_calc_payroll_cost(void) {
    int64_t base = (int64_t)g_am.staff_pilots    * 1200LL +
                   (int64_t)g_am.staff_crew      * 550LL  +
                   (int64_t)g_am.staff_engineers * 900LL  +
                   (int64_t)g_am.staff_techs     * 650LL;
    return (base * (int64_t)g_am.staff_salary_pct) / 100LL;
}

int am_effective_reputation(void) {
    int rep = g_am.base_reputation;
    if (g_am.mkt_airline_secs > 0) rep += g_am.mkt_airline_bonus;
    if (g_am.mkt_eco_secs > 0) rep += 10;
    if (g_am.catering_level == 1) rep += 3;
    else if (g_am.catering_level == 2) rep += 7;
    if (g_am.alliance_id > 0) rep += 6;
    if (g_am.event_id == 13) rep += 5;
    rep += g_am.train_mgmt_lv * 2;
    if (g_am.staff_morale < 60) rep -= (60 - g_am.staff_morale) / 3;
    if (rep < 25) rep = 25;
    if (rep > 99) rep = 99;
    return rep;
}

int am_calc_auto_ticket(int dist_km, int cabin, int realism) {
    if (cabin == 0) return ((realism ? 33 : 44) * dist_km) / 100 + (realism ? 150 : 170);
    if (cabin == 1) return ((realism ? 66 : 88) * dist_km) / 100 + (realism ? 500 : 560);
    if (cabin == 2) return ((realism ? 99 : 132) * dist_km) / 100 + (realism ? 1000 : 1200);
    return ((realism ? 95 : 118) * dist_km) / 100 + 420;
}

int am_calc_route_fee(int dist_km) { return 25000 + dist_km * 22; }

void am_calc_leg_preview(int model_idx, int dist_km, int cost_index, int mod_spd, int mod_fuel,
                         int *out_spd_kmh, int *out_secs, int64_t *out_fuel_lbs) {
    const AircraftModel *m = am_aircraft_get(model_idx);
    if (cost_index < 50) cost_index = 50;
    if (cost_index > 200) cost_index = 200;
    int spd = (m->speed_kmh * (80 + ((cost_index - 50) * 30) / 150)) / 100;
    if (mod_spd) spd = (spd * 110) / 100;
    if (!g_am.realism_mode) spd = (spd * 150) / 100;
    if (spd < 180) spd = 180;

    int sec = (dist_km * 3600) / spd;
    if (sec < 600) sec = 600;

    int64_t fuel = (int64_t)dist_km * (int64_t)m->fuel_lbs_km;
    fuel = (fuel * (85 + ((cost_index - 50) * 30) / 150)) / 100;
    if (mod_fuel) fuel = (fuel * 90) / 100;
    if (g_am.train_fuel_lv > 0) fuel = (fuel * (100 - g_am.train_fuel_lv * 2)) / 100;
    if (g_am.event_id == 6) fuel = (fuel * 108) / 100;
    else if (g_am.event_id == 10) fuel = (fuel * 110) / 100;

    if (out_spd_kmh) *out_spd_kmh = spd;
    if (out_secs) *out_secs = sec;
    if (out_fuel_lbs) *out_fuel_lbs = fuel;
}

static int ticket_load_factor(int user_price, int opt_price, int base_load) {
    if (opt_price <= 0) return base_load;
    int ratio = (user_price * 100) / opt_price;
    int lf = base_load;
    if (ratio > 125) lf -= (ratio - 125) * 3;
    else if (ratio > 105) lf -= (ratio - 105);
    else if (ratio < 85) lf += (85 - ratio) / 2;
    if (lf < 5) lf = 5;
    if (lf > 99) lf = 99;
    return lf;
}

int64_t am_estimate_leg_profit(int model_idx, int dist_km, int sy, int sj, int sf,
                               int py, int pj, int pf, int cost_index, int mod_fuel,
                               int64_t *out_rev, int64_t *out_fuel_cost) {
    const AircraftModel *m = am_aircraft_get(model_idx);
    int rep = am_effective_reputation();
    int base_load = 22 + (rep * 78) / 100;
    int64_t rev = 0;
    if (m->is_cargo) {
        int lf = ticket_load_factor(py, am_calc_auto_ticket(dist_km, 3, g_am.realism_mode), base_load);
        rev = (int64_t)((sy * lf) / 100) * (int64_t)py;
    } else {
        int ly = ticket_load_factor(py, am_calc_auto_ticket(dist_km, 0, g_am.realism_mode), base_load);
        int lj = ticket_load_factor(pj, am_calc_auto_ticket(dist_km, 1, g_am.realism_mode), base_load);
        int lf = ticket_load_factor(pf, am_calc_auto_ticket(dist_km, 2, g_am.realism_mode), base_load);
        rev = (int64_t)((sy * ly) / 100) * py + (int64_t)((sj * lj) / 100) * pj + (int64_t)((sf * lf) / 100) * pf;
        if (g_am.catering_level == 1) rev = (rev * 106) / 100;
        else if (g_am.catering_level == 2) rev = (rev * 114) / 100;
    }
    int spd = 0, sec = 0; int64_t fuel_lbs = 0;
    am_calc_leg_preview(model_idx, dist_km, cost_index, 0, mod_fuel, &spd, &sec, &fuel_lbs);
    int64_t fcost = g_am.cheat_god_fuel ? 0 : ((fuel_lbs * (int64_t)g_am.fuel_price) / 1000LL);
    if (out_rev) *out_rev = rev;
    if (out_fuel_cost) *out_fuel_cost = fcost;
    return rev - fcost;
}

static void check_achievements(void) {
    int has_wide = 0, has_sst = 0;
    for (int i = 0; i < g_am.fleet_count; i++) {
        const AircraftModel *m = am_aircraft_get(g_am.fleet[i].model_idx);
        if (m->silhouette == 3 || m->silhouette == 4) has_wide = 1;
        if (m->silhouette == 5) has_sst = 1;
    }
    int cond[AM_NUM_ACH] = {
        g_am.total_flights_done >= 1, g_am.total_flights_done >= 10, g_am.total_flights_done >= 50,
        g_am.fleet_count >= 5, g_am.fleet_count >= 20, g_am.fleet_count >= 100,
        g_am.hub_count >= 3, g_am.hub_count >= 10, g_am.bought_low_fuel > 0,
        g_am.total_pax_carried >= 5000, g_am.total_pax_carried >= 50000, g_am.total_cargo_tons >= 500,
        has_wide, has_sst, am_effective_reputation() >= 90, g_am.cash >= 100000000LL
    };
    for (int i = 0; i < AM_NUM_ACH; i++) {
        if (!g_am.ach_unlocked[i] && cond[i]) {
            g_am.ach_unlocked[i] = 1;
            g_am.cash += AM_ACHS[i].reward;
            g_am.pax_points += 25;
            snprintf(g_am.last_notice, sizeof(g_am.last_notice), "解鎖成就【%s】+$%lld & +25 積分！",
                     AM_ACHS[i].name, (long long)AM_ACHS[i].reward);
        }
    }
}

static void step_stochastic_market(void) {
    if (g_am.custom_fuel_lock > 0) {
        g_am.fuel_price = g_am.custom_fuel_lock;
    } else {
        int drift = (1150 - g_am.fuel_price) / 18;
        int noise = rng_range(-190, 190);
        int jump = 0, r = rng_range(0, 99);
        if (r < 9) jump = rng_range(350, 950);
        else if (r < 18) jump = -rng_range(320, 820);
        if (g_am.event_id == 1 || g_am.event_id == 7) drift += 160;
        else if (g_am.event_id == 2 || g_am.event_id == 8) drift -= 160;
        g_am.fuel_price += drift + noise + jump;
        if (g_am.fuel_price < 300) g_am.fuel_price = 300 + rng_range(0, 45);
        if (g_am.fuel_price > 3000) g_am.fuel_price = 3000 - rng_range(0, 80);
    }
    push_hist(g_am.fuel_hist, g_am.fuel_price);

    int c_noise = rng_range(-18, 18) + ((rng_range(0, 99) < 14) ? rng_range(-35, 35) : 0);
    if (g_am.event_id == 4) c_noise -= 15;
    else if (g_am.event_id == 9) c_noise += 18;
    g_am.co2_price += c_noise;
    if (g_am.co2_price < 100) g_am.co2_price = 100 + rng_range(0, 5);
    if (g_am.co2_price > 200) g_am.co2_price = 200 - rng_range(0, 5);
    push_hist(g_am.co2_hist, g_am.co2_price);

    for (int i = 0; i < AM_NUM_RIVALS; i++) {
        RivalAirline *rv = &g_am.rivals[i];
        int pct = rng_range(-7, 8) + ((rng_range(0, 99) < 6) ? rng_range(-14, 18) : 0);
        int d = (rv->price * pct) / 100; if (d == 0) d = rng_range(-3, 3);
        rv->price += d; if (rv->price < 12) rv->price = 12; if (rv->price > 9999) rv->price = 9999;
        push_hist(rv->hist, rv->price);
    }

    int64_t nw = g_am.cash + (int64_t)g_am.fleet_count * 4500000LL + (int64_t)g_am.hub_count * 3000000LL;
    int tsp = (int)(nw / 450000LL) + am_effective_reputation() / 3; if (tsp < 10) tsp = 10;
    g_am.own_share_price = (g_am.own_share_price * 3 + tsp) / 4 + rng_range(-2, 2);
    if (g_am.own_share_price < 10) g_am.own_share_price = 10;
    push_hist(g_am.own_share_hist, g_am.own_share_price);

    /* 每 5 分鐘刷新時有機會觸發 16 種隨機事件之一 */
    if (rng_range(0, 99) < 48) {
        g_am.event_id = rng_range(1, AM_NUM_EVENTS);
        g_am.event_secs_left = 300; /* 持續至下一次 5 分鐘刷新 */
        snprintf(g_am.last_notice, sizeof(g_am.last_notice), "突發事件：%s！", am_event_name(g_am.event_id));
    } else {
        g_am.event_id = 0;
        g_am.event_secs_left = 0;
    }
}

static void init_rivals(void) {
    static const char *CODES[AM_NUM_RIVALS] = { "SKX","PAC","EUR","EMX","NIP","QNT","LUF","DLT","SIA","STR" };
    static const char *NAMES[AM_NUM_RIVALS] = {
        "SkyStar Air","Pacifica Jet","EuroWing","EmirateX","AeroNippon",
        "SouthCross","HansaAir","DeltaCore","LionWings","StarluxSim"
    };
    static const int BASE_P[AM_NUM_RIVALS] = { 120, 88, 165, 240, 142, 110, 134, 198, 215, 96 };
    for (int i = 0; i < AM_NUM_RIVALS; i++) {
        RivalAirline *rv = &g_am.rivals[i]; memset(rv, 0, sizeof(*rv));
        snprintf(rv->code, sizeof(rv->code), "%s", CODES[i]);
        snprintf(rv->name, sizeof(rv->name), "%s", NAMES[i]);
        rv->price = BASE_P[i] + rng_range(-15, 20);
        for (int k = 0; k < AM_PRICE_HIST; k++) rv->hist[k] = rv->price + rng_range(-8, 8);
    }
}

void am_roll_new_charter(void) {
    int ac = am_airport_count(); if (ac < 2) return;
    g_am.charter_orig = g_am.hubs[0];
    int dest = rng_range(0, ac - 1); if (dest == g_am.charter_orig) dest = (dest + 3) % ac;
    g_am.charter_dest = dest; g_am.charter_type = rng_range(0, 3);
    int dist = am_calc_distance_km(g_am.charter_orig, g_am.charter_dest);
    g_am.charter_reward = 280000LL + (int64_t)dist * 95LL;
    g_am.charter_pts = 15 + (dist / 1200);
}

/* ============================================================
 * 多重自訂名稱存檔槽管理 (8 個獨立存檔槽!)
 * ============================================================ */
static void save_slots_index(void) {
    g_slots.magic = AM_IDX_MAGIC;
    FILE *f = fopen(hal_get_save_index_path(), "wb");
    if (!f) return;
    fwrite(&g_slots, sizeof(g_slots), 1, f);
    fclose(f);
}

void am_slots_refresh(void) {
    memset(&g_slots, 0, sizeof(g_slots));
    FILE *f = fopen(hal_get_save_index_path(), "rb");
    if (f) {
        SaveSlotIndex tmp;
        if (fread(&tmp, sizeof(tmp), 1, f) == 1 && tmp.magic == AM_IDX_MAGIC) g_slots = tmp;
        fclose(f);
    }
}

int am_sim_save_slot(int slot, const char *save_name) {
    if (!g_am.initialized || slot < 0 || slot >= AM_MAX_SAVE_SLOTS) return 0;
    g_am.magic = AM_SAVE_MAGIC;
    g_am.active_slot = slot;
    if (save_name && save_name[0]) snprintf(g_am.save_name, sizeof(g_am.save_name), "%.27s", save_name);
    else if (!g_am.save_name[0]) snprintf(g_am.save_name, sizeof(g_am.save_name), "Save %d", slot + 1);
    g_am.last_unix_time = hal_get_unix_time();

    char path[128];
    hal_get_save_slot_path(slot, path, sizeof(path));
    FILE *f = fopen(path, "wb");
    if (!f) return 0;
    fwrite(&g_am, sizeof(g_am), 1, f);
    fclose(f);

    SaveSlotMeta *m = &g_slots.slots[slot];
    m->used = 1;
    snprintf(m->save_name, sizeof(m->save_name), "%.27s", g_am.save_name);
    snprintf(m->airline_name, sizeof(m->airline_name), "%.27s", g_am.airline_name);
    m->cash = g_am.cash;
    m->fleet_count = g_am.fleet_count;
    m->hub_count = g_am.hub_count;
    m->level = am_airline_level();
    save_slots_index();
    return 1;
}

void am_sim_save(void) {
    if (!g_am.initialized) return;
    int s = (g_am.active_slot >= 0 && g_am.active_slot < AM_MAX_SAVE_SLOTS) ? g_am.active_slot : 0;
    am_sim_save_slot(s, g_am.save_name);
}

int am_sim_load_slot(int slot) {
    if (slot < 0 || slot >= AM_MAX_SAVE_SLOTS) return 0;
    char path[128];
    hal_get_save_slot_path(slot, path, sizeof(path));
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    AmWorld *tmp = (AmWorld *)malloc(sizeof(AmWorld));
    if (!tmp) { fclose(f); return 0; }
    int ok = (fread(tmp, sizeof(AmWorld), 1, f) == 1 && tmp->magic == AM_SAVE_MAGIC && tmp->initialized);
    fclose(f);
    if (!ok) { free(tmp); return 0; }
    g_am = *tmp;
    free(tmp);
    g_am.active_slot = slot;

    int64_t now = hal_get_unix_time(), offline = now - g_am.last_unix_time;
    if (offline > 5 && offline < 86400 * 30) {
        am_sim_tick((int)offline, 0);
        snprintf(g_am.last_notice, sizeof(g_am.last_notice), "載入【%s】離線掛機 %ld 分鐘已結算！", g_am.save_name, (long)(offline / 60));
    }
    g_am.last_unix_time = now;
    return 1;
}

void am_sim_delete_slot(int slot) {
    if (slot < 0 || slot >= AM_MAX_SAVE_SLOTS) return;
    char path[128];
    hal_get_save_slot_path(slot, path, sizeof(path));
    remove(path);
    memset(&g_slots.slots[slot], 0, sizeof(SaveSlotMeta));
    save_slots_index();
}

void am_sim_init(void) {
    g_rng_state = ((uint64_t)hal_get_seed() << 32) ^ (uint64_t)hal_get_unix_time() ^ 0x9E3779B97F4A7C15ULL;
    if (g_rng_state == 0) g_rng_state = 0x123456789ABCDEFULL;
    am_data_init(hal_get_extra_airports_csv());
    memset(&g_am, 0, sizeof(g_am));
    am_slots_refresh();
}

void am_sim_new_company(int slot, const char *save_name, const char *name, const char *iata,
                        int hub_ap_idx, int realism_mode, int64_t start_cash, int starter_model_idx) {
    memset(&g_am, 0, sizeof(g_am));
    g_am.magic = AM_SAVE_MAGIC; g_am.initialized = 1;
    g_am.active_slot = (slot >= 0 && slot < AM_MAX_SAVE_SLOTS) ? slot : 0;
    snprintf(g_am.save_name, sizeof(g_am.save_name), "%.27s", (save_name && save_name[0]) ? save_name : "My Empire");
    snprintf(g_am.airline_name, sizeof(g_am.airline_name), "%.27s", (name && name[0]) ? name : "SkyEmpire Air");
    snprintf(g_am.iata_code, sizeof(g_am.iata_code), "%.2s", (iata && iata[0]) ? iata : "SE");
    g_am.realism_mode = realism_mode ? 1 : 0;
    g_am.speed_mult_idx = 2;
    g_am.cash = (start_cash > 0) ? start_cash : 1800000LL;
    g_am.pax_points = 110;
    g_am.hubs[0] = (hub_ap_idx >= 0 && hub_ap_idx < am_airport_count()) ? hub_ap_idx : 0;
    g_am.hub_count = 1;

    g_am.fuel_cap = 2000000LL; g_am.fuel_lbs = 1200000LL;
    g_am.co2_cap  = 2000000LL; g_am.co2_quotas = 1000000LL;
    g_am.fuel_price = rng_range(580, 1450); g_am.co2_price = rng_range(115, 165);
    for (int i = 0; i < AM_PRICE_HIST; i++) {
        g_am.fuel_hist[i] = g_am.fuel_price + rng_range(-120, 120); if (g_am.fuel_hist[i] < 300) g_am.fuel_hist[i] = 300;
        g_am.co2_hist[i]  = g_am.co2_price + rng_range(-12, 12); if (g_am.co2_hist[i] < 100) g_am.co2_hist[i] = 100;
    }

    g_am.base_reputation = 62; g_am.staff_salary_pct = 100; g_am.staff_morale = 90;
    /* 起始給予足夠 3~4 架窄體機的機組人員，之後買更多飛機須先招募！ */
    g_am.staff_pilots = 10; g_am.staff_crew = 20; g_am.staff_engineers = 8; g_am.staff_techs = 8;
    g_am.training_points = 3;
    g_am.own_share_price = 25;
    for (int i = 0; i < AM_PRICE_HIST; i++) g_am.own_share_hist[i] = 25;
    init_rivals();

    const AircraftModel *sm = am_aircraft_get(starter_model_idx);
    FleetPlane *p0 = &g_am.fleet[0]; memset(p0, 0, sizeof(*p0));
    p0->active = 1; p0->model_idx = starter_model_idx;
    snprintf(p0->reg, sizeof(p0->reg), "%s-001", g_am.iata_code);
    p0->seats_j = sm->cap / 10; p0->seats_f = 4;
    p0->seats_y = sm->cap - p0->seats_j * 2 - p0->seats_f * 3;
    p0->orig_ap = g_am.hubs[0]; p0->dest_ap = -1; p0->stopover_ap = -1;
    p0->cost_index = 150; p0->auto_return = 1;
    p0->acheck_hrs_left = sm->acheck_hrs;
    g_am.fleet_count = 1;

    am_roll_new_charter();
    g_am.last_unix_time = hal_get_unix_time();
    snprintf(g_am.last_notice, sizeof(g_am.last_notice), "歡迎成立 %s (存檔：%s)！已交付首架 %s", g_am.airline_name, g_am.save_name, sm->model);
    am_sim_save_slot(g_am.active_slot, g_am.save_name);
}

static void start_leg_flight(FleetPlane *fp) {
    const AircraftModel *m = am_aircraft_get(fp->model_idx);
    int spd = 0, sec = 0; int64_t fuel_need = 0;
    am_calc_leg_preview(fp->model_idx, fp->dist_km, fp->cost_index, fp->mod_speed, fp->mod_fuel, &spd, &sec, &fuel_need);
    int64_t co2_need = ((int64_t)fp->dist_km * (int64_t)m->co2_kg * (int64_t)(m->is_cargo ? 10 : m->cap)) / 100LL;
    if (fp->mod_co2) co2_need = (co2_need * 90) / 100;
    if (!g_am.cheat_god_fuel) {
        if (g_am.fuel_lbs < fuel_need) return;
        g_am.fuel_lbs -= fuel_need;
        g_am.co2_quotas -= co2_need;
        if (g_am.co2_quotas < 0 && g_am.base_reputation > 40) g_am.base_reputation--;
    }
    fp->status = 1; fp->flight_elapsed_sec = 0; fp->flight_total_sec = sec;
}

static void complete_flight(int fleet_idx) {
    FleetPlane *fp = &g_am.fleet[fleet_idx];
    const AircraftModel *m = am_aircraft_get(fp->model_idx);
    int rep = am_effective_reputation();
    int base_load = 22 + (rep * 78) / 100 + rng_range(-3, 6);
    if (g_am.event_id == 3) base_load += 18;
    else if (g_am.event_id == 12) base_load += 10;

    int64_t rev = 0; int carried = 0;
    if (m->is_cargo) {
        int lf = ticket_load_factor(fp->price_y, am_calc_auto_ticket(fp->dist_km, 3, g_am.realism_mode), base_load);
        int tons = (fp->seats_y * lf) / 100; if (tons < 1) tons = 1;
        rev = (int64_t)tons * (int64_t)fp->price_y;
        if (g_am.event_id == 11) rev = (rev * 120) / 100;
        g_am.total_cargo_tons += tons; carried = tons;
    } else {
        int ly = ticket_load_factor(fp->price_y, am_calc_auto_ticket(fp->dist_km, 0, g_am.realism_mode), base_load);
        int lj = ticket_load_factor(fp->price_j, am_calc_auto_ticket(fp->dist_km, 1, g_am.realism_mode), base_load);
        int lf = ticket_load_factor(fp->price_f, am_calc_auto_ticket(fp->dist_km, 2, g_am.realism_mode), base_load);
        int py = (fp->seats_y * ly) / 100, pj = (fp->seats_j * lj) / 100, pf = (fp->seats_f * lf) / 100;
        int64_t ry = (int64_t)py * fp->price_y, rj = (int64_t)pj * fp->price_j, rf = (int64_t)pf * fp->price_f;
        if (g_am.event_id == 5) { rj = (rj * 115) / 100; rf = (rf * 115) / 100; }
        else if (g_am.event_id == 15) { rf = (rf * 120) / 100; }
        rev = ry + rj + rf;
        if (g_am.catering_level == 1) rev = (rev * 106) / 100;
        else if (g_am.catering_level == 2) rev = (rev * 114) / 100;
        if (g_am.event_id == 12) rev = (rev * 110) / 100;
        else if (g_am.event_id == 16) rev = (rev * 108) / 100;
        carried = py + pj + pf;
        g_am.total_pax_carried += carried;
    }

    int64_t fine = (fp->acheck_hrs_left <= 0 && !g_am.cheat_no_wear) ? (rev / 4) : 0;
    if (g_am.event_id == 14) fine += rev / 20; /* 暴風雪除冰費 */
    if (fine > 0 && g_am.base_reputation > 35) g_am.base_reputation -= 2;

    int64_t net = rev - fine;
    g_am.cash += net; g_am.total_revenue += rev; g_am.total_expenses += fine;
    g_am.last_flight_profit = net; g_am.total_flights_done++;
    fp->total_flights++; fp->total_profit += net;
    fp->status = 0; fp->flight_elapsed_sec = 0;

    if (!g_am.cheat_no_wear) {
        int flt_hrs = fp->flight_total_sec / 3600; if (flt_hrs < 1) flt_hrs = 1;
        fp->acheck_hrs_left -= flt_hrs; if (fp->acheck_hrs_left < 0) fp->acheck_hrs_left = 0;
        int dw = 2 + (flt_hrs / 3);
        if (g_am.train_maint_lv > 0) { dw = (dw * (100 - g_am.train_maint_lv * 10)) / 100; if (dw < 1) dw = 1; }
        fp->wear_pct += dw; if (fp->wear_pct > 100) fp->wear_pct = 100;
    } else fp->wear_pct = 0;

    if ((g_am.total_flights_done % 3) == 0) g_am.training_points++;
    if (g_am.base_reputation < 75 && rng_range(0, 99) < 25) g_am.base_reputation++;

    const AirportInfo *a_from = am_airport_get(fp->leg_dir == 0 ? fp->orig_ap : fp->dest_ap);
    const AirportInfo *a_to   = am_airport_get(fp->leg_dir == 0 ? fp->dest_ap : fp->orig_ap);
    for (int i = AM_MAX_FLOG - 1; i > 0; i--) g_am.flight_log[i] = g_am.flight_log[i - 1];
    FlightLogEntry *fl = &g_am.flight_log[0];
    snprintf(fl->reg, sizeof(fl->reg), "%s", fp->reg);
    snprintf(fl->route, sizeof(fl->route), "%s->%s", a_from->iata, a_to->iata);
    fl->is_cargo = m->is_cargo; fl->pax_or_tons = carried; fl->net_profit = net;
    if (g_am.flight_log_count < AM_MAX_FLOG) g_am.flight_log_count++;

    int was_outbound = (fp->leg_dir == 0);
    fp->leg_dir = 1 - fp->leg_dir;
    if (was_outbound && fp->auto_return) {
        start_leg_flight(fp);
        snprintf(g_am.last_notice, sizeof(g_am.last_notice), "%s 抵 %s (+$%lld) 自動返航 %s！", fp->reg, a_to->iata, (long long)net, a_from->iata);
    } else {
        snprintf(g_am.last_notice, sizeof(g_am.last_notice), "%s 返抵 %s 完成往返！淨利 +$%lld", fp->reg, a_to->iata, (long long)net);
    }
    check_achievements();
}

int am_sim_tick(int dt_real_sec, int apply_speed_mult) {
    if (!g_am.initialized || dt_real_sec <= 0) return 0;
    int sim_sec = apply_speed_mult ? (dt_real_sec * am_sim_speed_mult()) : dt_real_sec;
    if (sim_sec < 1) sim_sec = 1;
    int major = 0;

    for (int i = 0; i < g_am.fleet_count; i++) {
        FleetPlane *fp = &g_am.fleet[i]; if (!fp->active) continue;
        if (fp->status == 1) {
            fp->flight_elapsed_sec += sim_sec;
            if (fp->flight_elapsed_sec >= fp->flight_total_sec) { complete_flight(i); major = 1; }
        } else if (fp->status == 2) {
            fp->flight_elapsed_sec += sim_sec;
            if (fp->flight_elapsed_sec >= fp->flight_total_sec) {
                fp->status = 0; fp->flight_elapsed_sec = 0; fp->wear_pct = 0;
                fp->acheck_hrs_left = am_aircraft_get(fp->model_idx)->acheck_hrs;
                major = 1;
            }
        }
    }
    if (g_am.mkt_airline_secs > 0) { g_am.mkt_airline_secs -= sim_sec; if (g_am.mkt_airline_secs < 0) g_am.mkt_airline_secs = 0; }
    if (g_am.mkt_eco_secs > 0) { g_am.mkt_eco_secs -= sim_sec; if (g_am.mkt_eco_secs < 0) g_am.mkt_eco_secs = 0; }

    int tm = (g_am.staff_salary_pct >= 115) ? 98 : (g_am.staff_salary_pct >= 100 ? 88 : (g_am.staff_salary_pct >= 90 ? 72 : 52));
    if (g_am.staff_morale < tm) g_am.staff_morale++; else if (g_am.staff_morale > tm) g_am.staff_morale--;

    /* 每 5 分鐘 (300 真實秒) 才刷新油價/CO2/股市與隨機事件！絕不亂閃快刷 */
    g_am.market_real_sec_acc += dt_real_sec;
    if (g_am.market_real_sec_acc >= 300) {
        int steps = g_am.market_real_sec_acc / 300;
        if (steps > 12) steps = 12;
        g_am.market_real_sec_acc %= 300;
        for (int s = 0; s < steps; s++) step_stochastic_market();
        major = 1;
    }

    /* 固定每 5 分鐘 (300 真實秒) 發放全體員工工資！ */
    g_am.payroll_real_sec_acc += dt_real_sec;
    if (g_am.payroll_real_sec_acc >= 300) {
        int cycles = g_am.payroll_real_sec_acc / 300;
        if (cycles > 12) cycles = 12;
        g_am.payroll_real_sec_acc %= 300;
        int64_t pay = am_calc_payroll_cost() * cycles;
        g_am.last_payroll_cost = pay;
        g_am.cash -= pay;
        g_am.total_expenses += pay;
        snprintf(g_am.last_notice, sizeof(g_am.last_notice), "定期發薪日：已發放員工工資 -$%lld", (long long)pay);
        major = 1;
    }

    return major;
}

/* 一次批量購買多架飛機 (嚴格檢查資金與機組人員是否足夠!) */
int am_buy_aircraft_bulk(int model_idx, int hub_ap_idx, int sy, int sj, int sf, int qty, char *out_msg, int msg_sz) {
    if (qty < 1) qty = 1;
    if (g_am.fleet_count + qty > AM_MAX_FLEET) {
        snprintf(out_msg, (size_t)msg_sz, "機隊容量上限 %d 架 (目前 %d 架)", AM_MAX_FLEET, g_am.fleet_count); return 0;
    }
    const AircraftModel *m = am_aircraft_get(model_idx);
    int64_t tot_cost = m->price * (int64_t)qty;
    if (g_am.cash < tot_cost) {
        snprintf(out_msg, (size_t)msg_sz, "資金不足！買 %d 架需 $%lld", qty, (long long)tot_cost); return 0;
    }
    if (!m->is_cargo && (sy + sj * 2 + sf * 3 > m->cap || sy < 0 || sj < 0 || sf < 0)) {
        snprintf(out_msg, (size_t)msg_sz, "座位空間超過機型上限 (%u)！", (unsigned)m->cap); return 0;
    }

    /* 檢查機組與地勤人員是否足夠！(除非開啟 MOD 免員工限制) */
    if (!g_am.cheat_no_staff_req) {
        int rp, rc, re, rt, up, uc, ue, ut;
        am_aircraft_crew_req(model_idx, &rp, &rc, &re, &rt);
        am_fleet_crew_used(&up, &uc, &ue, &ut);
        int need_p = up + rp * qty, need_c = uc + rc * qty, need_e = ue + re * qty, need_t = ut + rt * qty;
        if (g_am.staff_pilots < need_p || g_am.staff_crew < need_c || g_am.staff_engineers < need_e || g_am.staff_techs < need_t) {
            snprintf(out_msg, (size_t)msg_sz, "員工不足！需 機長%d 空服%d 工%d 地%d (請至公司頁招募)",
                     need_p - g_am.staff_pilots > 0 ? need_p - g_am.staff_pilots : 0,
                     need_c - g_am.staff_crew > 0 ? need_c - g_am.staff_crew : 0,
                     need_e - g_am.staff_engineers > 0 ? need_e - g_am.staff_engineers : 0,
                     need_t - g_am.staff_techs > 0 ? need_t - g_am.staff_techs : 0);
            return 0;
        }
    }

    g_am.cash -= tot_cost; g_am.total_expenses += tot_cost;
    for (int k = 0; k < qty; k++) {
        FleetPlane *fp = &g_am.fleet[g_am.fleet_count++]; memset(fp, 0, sizeof(*fp));
        fp->active = 1; fp->model_idx = model_idx;
        snprintf(fp->reg, sizeof(fp->reg), "%s-%03d", g_am.iata_code, g_am.fleet_count);
        fp->seats_y = m->is_cargo ? m->cap : sy;
        fp->seats_j = m->is_cargo ? 0 : sj;
        fp->seats_f = m->is_cargo ? 0 : sf;
        fp->orig_ap = (hub_ap_idx >= 0) ? hub_ap_idx : g_am.hubs[0];
        fp->dest_ap = -1; fp->stopover_ap = -1;
        fp->cost_index = 150; fp->auto_return = 1; fp->leg_dir = 0;
        fp->acheck_hrs_left = m->acheck_hrs;
    }
    snprintf(out_msg, (size_t)msg_sz, "批量購入 %d 架 %s 成功！(現有機隊 %d 架)", qty, m->model, g_am.fleet_count);
    check_achievements(); am_sim_save(); return 1;
}

int am_sell_aircraft(int fleet_idx, char *out_msg, int msg_sz) {
    if (fleet_idx < 0 || fleet_idx >= g_am.fleet_count) return 0;
    if (g_am.fleet[fleet_idx].status != 0 && g_am.fleet[fleet_idx].status != 3) { snprintf(out_msg, (size_t)msg_sz, "飛機執勤或維修中無法出售"); return 0; }
    const AircraftModel *m = am_aircraft_get(g_am.fleet[fleet_idx].model_idx);
    int64_t refund = (m->price * (85 - g_am.fleet[fleet_idx].wear_pct / 2)) / 100;
    if (refund < m->price / 4) refund = m->price / 4;
    g_am.cash += refund;
    for (int i = fleet_idx; i + 1 < g_am.fleet_count; i++) g_am.fleet[i] = g_am.fleet[i + 1];
    g_am.fleet_count--;
    snprintf(out_msg, (size_t)msg_sz, "已出售 %s，回收資金 $%lld", m->model, (long long)refund);
    am_sim_save(); return 1;
}

int am_assign_route_full(int fleet_idx, int orig_ap, int dest_ap, int stopover_ap,
                         int py, int pj, int pf, int cost_index, int auto_ret, char *out_msg, int msg_sz) {
    if (fleet_idx < 0 || fleet_idx >= g_am.fleet_count) return 0;
    FleetPlane *fp = &g_am.fleet[fleet_idx];
    if (fp->status != 0) { snprintf(out_msg, (size_t)msg_sz, "請等飛機落地再修改航線"); return 0; }
    if (orig_ap == dest_ap) { snprintf(out_msg, (size_t)msg_sz, "起降機場不能相同"); return 0; }
    const AircraftModel *m = am_aircraft_get(fp->model_idx);
    const AirportInfo *ao = am_airport_get(orig_ap), *ad = am_airport_get(dest_ap);
    if (g_am.realism_mode && (ao->runway_ft < m->runway_ft || ad->runway_ft < m->runway_ft)) {
        snprintf(out_msg, (size_t)msg_sz, "擬真模式：跑道長度不足 (需 %uft)", (unsigned)m->runway_ft); return 0;
    }
    int d_total = 0;
    if (stopover_ap >= 0 && stopover_ap != orig_ap && stopover_ap != dest_ap) {
        int d1 = am_calc_distance_km(orig_ap, stopover_ap), d2 = am_calc_distance_km(stopover_ap, dest_ap);
        if (d1 > m->range_km || d2 > m->range_km) { snprintf(out_msg, (size_t)msg_sz, "中轉單段超過航程 %ukm！", (unsigned)m->range_km); return 0; }
        d_total = d1 + d2;
    } else {
        stopover_ap = -1; d_total = am_calc_distance_km(orig_ap, dest_ap);
        if (d_total > m->range_km) { snprintf(out_msg, (size_t)msg_sz, "距離 %dkm 超過航程 %ukm (請設中轉)", d_total, (unsigned)m->range_km); return 0; }
    }
    int fee = (!fp->has_route || fp->orig_ap != orig_ap || fp->dest_ap != dest_ap) ? am_calc_route_fee(d_total) : 0;
    if (fee > 0 && g_am.cash < fee) { snprintf(out_msg, (size_t)msg_sz, "開航費不足！需 $%d", fee); return 0; }
    if (fee > 0) { g_am.cash -= fee; g_am.total_expenses += fee; }

    fp->has_route = 1; fp->orig_ap = orig_ap; fp->dest_ap = dest_ap; fp->stopover_ap = stopover_ap;
    fp->dist_km = d_total; fp->leg_dir = 0;
    fp->price_y = py > 10 ? py : am_calc_auto_ticket(d_total, m->is_cargo ? 3 : 0, g_am.realism_mode);
    fp->price_j = pj > 10 ? pj : am_calc_auto_ticket(d_total, 1, g_am.realism_mode);
    fp->price_f = pf > 10 ? pf : am_calc_auto_ticket(d_total, 2, g_am.realism_mode);
    fp->cost_index = (cost_index >= 50 && cost_index <= 200) ? cost_index : 150;
    fp->auto_return = auto_ret ? 1 : 0;
    snprintf(out_msg, (size_t)msg_sz, "雙向航線開通：%s <-> %s (%dkm)！", ao->iata, ad->iata, d_total);
    am_sim_save(); return 1;
}

int am_depart_plane(int fleet_idx, char *out_msg, int msg_sz) {
    if (fleet_idx < 0 || fleet_idx >= g_am.fleet_count) return 0;
    FleetPlane *fp = &g_am.fleet[fleet_idx];
    if (!fp->has_route) { snprintf(out_msg, (size_t)msg_sz, "%s 尚未設定航線", fp->reg); return 0; }
    if (fp->status != 0) { snprintf(out_msg, (size_t)msg_sz, "%s 正在執勤或封存中", fp->reg); return 0; }
    int spd = 0, sec = 0; int64_t fuel_need = 0;
    am_calc_leg_preview(fp->model_idx, fp->dist_km, fp->cost_index, fp->mod_speed, fp->mod_fuel, &spd, &sec, &fuel_need);
    if (!g_am.cheat_god_fuel && g_am.fuel_lbs < fuel_need) { snprintf(out_msg, (size_t)msg_sz, "燃油不足！需 %lld lbs", (long long)fuel_need); return 0; }
    start_leg_flight(fp);
    const AirportInfo *af = am_airport_get(fp->leg_dir == 0 ? fp->orig_ap : fp->dest_ap);
    const AirportInfo *at = am_airport_get(fp->leg_dir == 0 ? fp->dest_ap : fp->orig_ap);
    snprintf(out_msg, (size_t)msg_sz, "%s 起飛 (%s -> %s)！", fp->reg, af->iata, at->iata);
    return 1;
}

int am_depart_all(char *out_msg, int msg_sz) {
    int launched = 0, no_fuel = 0; char tmp[96];
    for (int i = 0; i < g_am.fleet_count; i++) {
        if (g_am.fleet[i].active && g_am.fleet[i].has_route && g_am.fleet[i].status == 0) {
            if (am_depart_plane(i, tmp, sizeof(tmp))) launched++; else no_fuel++;
        }
    }
    if (launched > 0) snprintf(out_msg, (size_t)msg_sz, "Depart All：已派遣 %d 架飛機起飛！", launched);
    else if (no_fuel > 0) snprintf(out_msg, (size_t)msg_sz, "燃油不足，無法派遣待命機隊！");
    else snprintf(out_msg, (size_t)msg_sz, "目前無待命機隊");
    am_sim_save(); return launched;
}

int am_buy_fuel(int64_t lbs, char *out_msg, int msg_sz) {
    if (lbs <= 0) return 0;
    int64_t room = g_am.fuel_cap - g_am.fuel_lbs;
    if (room <= 0) { snprintf(out_msg, (size_t)msg_sz, "儲油槽已滿！"); return 0; }
    if (lbs > room) lbs = room;
    int64_t cost = (lbs * (int64_t)g_am.fuel_price) / 1000LL;
    if (g_am.cash < cost) { snprintf(out_msg, (size_t)msg_sz, "資金不足！需 $%lld", (long long)cost); return 0; }
    g_am.cash -= cost; g_am.total_expenses += cost; g_am.fuel_lbs += lbs;
    if (g_am.fuel_price <= 650) g_am.bought_low_fuel = 1;
    snprintf(out_msg, (size_t)msg_sz, "購入 %lld lbs 燃油 ($%lld)", (long long)lbs, (long long)cost);
    check_achievements(); am_sim_save(); return 1;
}

int am_buy_co2(int64_t quotas, char *out_msg, int msg_sz) {
    if (quotas <= 0) return 0;
    int64_t room = g_am.co2_cap - g_am.co2_quotas;
    if (room <= 0) { snprintf(out_msg, (size_t)msg_sz, "CO2 儲槽已滿！"); return 0; }
    if (quotas > room) quotas = room;
    int64_t cost = (quotas * (int64_t)g_am.co2_price) / 1000LL;
    if (g_am.cash < cost) { snprintf(out_msg, (size_t)msg_sz, "資金不足！需 $%lld", (long long)cost); return 0; }
    g_am.cash -= cost; g_am.total_expenses += cost; g_am.co2_quotas += quotas;
    snprintf(out_msg, (size_t)msg_sz, "購入 %lld CO2 碳權 ($%lld)", (long long)quotas, (long long)cost);
    am_sim_save(); return 1;
}

int am_upgrade_fuel_tank(char *out_msg, int msg_sz) {
    if (g_am.cash < 1200000LL) { snprintf(out_msg, (size_t)msg_sz, "擴充油槽需 $1.2M"); return 0; }
    g_am.cash -= 1200000LL; g_am.fuel_cap += 5000000LL;
    snprintf(out_msg, (size_t)msg_sz, "油槽擴充成功！上限 %lld lbs", (long long)g_am.fuel_cap);
    am_sim_save(); return 1;
}
int am_upgrade_co2_tank(char *out_msg, int msg_sz) {
    if (g_am.cash < 900000LL) { snprintf(out_msg, (size_t)msg_sz, "擴充碳槽需 $0.9M"); return 0; }
    g_am.cash -= 900000LL; g_am.co2_cap += 5000000LL;
    snprintf(out_msg, (size_t)msg_sz, "CO2 槽擴充成功！上限 %lld", (long long)g_am.co2_cap);
    am_sim_save(); return 1;
}
int am_start_marketing(int type, char *out_msg, int msg_sz) {
    int64_t cost = (type == 0) ? 120000LL : ((type == 1) ? 280000LL : 650000LL);
    if (g_am.cash < cost) { snprintf(out_msg, (size_t)msg_sz, "行銷資金不足！"); return 0; }
    g_am.cash -= cost; g_am.total_expenses += cost;
    if (type == 0) { g_am.mkt_eco_secs = 12 * 3600; snprintf(out_msg, (size_t)msg_sz, "啟動環保行銷！聲譽 +10%%"); }
    else if (type == 1) { g_am.mkt_airline_secs = 12 * 3600; g_am.mkt_airline_bonus = 18; snprintf(out_msg, (size_t)msg_sz, "啟動區域廣告！聲譽 +18%%"); }
    else { g_am.mkt_airline_secs = 24 * 3600; g_am.mkt_airline_bonus = 32; snprintf(out_msg, (size_t)msg_sz, "啟動全球廣告！聲譽 +32%%"); }
    check_achievements(); am_sim_save(); return 1;
}

int am_repair_plane(int fleet_idx, char *out_msg, int msg_sz) {
    if (fleet_idx < 0 || fleet_idx >= g_am.fleet_count) return 0;
    FleetPlane *fp = &g_am.fleet[fleet_idx];
    if (fp->status != 0) { snprintf(out_msg, (size_t)msg_sz, "請等飛機落地再維修"); return 0; }
    if (fp->wear_pct <= 0) { snprintf(out_msg, (size_t)msg_sz, "%s 機體狀態完美無需修復", fp->reg); return 0; }
    const AircraftModel *m = am_aircraft_get(fp->model_idx);
    int64_t cost = (m->price * (int64_t)fp->wear_pct) / 2500LL + (int64_t)fp->wear_pct * 5000LL + 20000LL;
    if (g_am.cash < cost) { snprintf(out_msg, (size_t)msg_sz, "維修費不足！需 $%lld", (long long)cost); return 0; }
    g_am.cash -= cost; g_am.total_expenses += cost; fp->wear_pct = 0;
    snprintf(out_msg, (size_t)msg_sz, "%s 修復完成！(花費 $%lld)", fp->reg, (long long)cost);
    am_sim_save(); return 1;
}
int am_acheck_plane(int fleet_idx, char *out_msg, int msg_sz) {
    if (fleet_idx < 0 || fleet_idx >= g_am.fleet_count) return 0;
    FleetPlane *fp = &g_am.fleet[fleet_idx];
    if (fp->status != 0) { snprintf(out_msg, (size_t)msg_sz, "請等飛機落地再執行 A-Check"); return 0; }
    const AircraftModel *m = am_aircraft_get(fp->model_idx);
    int64_t cost = (m->price / 30LL) + 80000LL;
    if (g_am.cash < cost) { snprintf(out_msg, (size_t)msg_sz, "A-Check 費用不足 ($%lld)", (long long)cost); return 0; }
    g_am.cash -= cost; g_am.total_expenses += cost;
    fp->status = 2; fp->flight_elapsed_sec = 0; fp->flight_total_sec = 4 * 3600;
    snprintf(out_msg, (size_t)msg_sz, "%s 進入 A-Check 大檢 ($%lld)", fp->reg, (long long)cost);
    am_sim_save(); return 1;
}
int am_repair_all(char *out_msg, int msg_sz) {
    int cnt = 0; int64_t before = g_am.cash; char tmp[96];
    for (int i = 0; i < g_am.fleet_count; i++) {
        if (g_am.fleet[i].status == 0 && g_am.fleet[i].wear_pct > 0) {
            if (am_repair_plane(i, tmp, sizeof(tmp))) cnt++;
        }
    }
    if (cnt > 0) snprintf(out_msg, (size_t)msg_sz, "一鍵修復 %d 架飛機，共花費 $%lld", cnt, (long long)(before - g_am.cash));
    else snprintf(out_msg, (size_t)msg_sz, "目前無需修復的待命飛機 (或資金不足)");
    return cnt;
}
int am_acheck_all(char *out_msg, int msg_sz) {
    int cnt = 0; char tmp[96];
    for (int i = 0; i < g_am.fleet_count; i++) {
        if (g_am.fleet[i].status == 0 && g_am.fleet[i].acheck_hrs_left <= 60) {
            if (am_acheck_plane(i, tmp, sizeof(tmp))) cnt++;
        }
    }
    snprintf(out_msg, (size_t)msg_sz, cnt > 0 ? "一鍵大檢：已送檢 %d 架飛機" : "所有待命飛機 A-Check 時數皆充足", cnt);
    return cnt;
}
int am_buy_mod(int fleet_idx, int mod_type, char *out_msg, int msg_sz) {
    if (fleet_idx < 0 || fleet_idx >= g_am.fleet_count) return 0;
    FleetPlane *fp = &g_am.fleet[fleet_idx]; const AircraftModel *m = am_aircraft_get(fp->model_idx);
    int64_t cost = (m->price * 7LL) / 100LL + 150000LL;
    if ((mod_type == 0 && fp->mod_speed) || (mod_type == 1 && fp->mod_fuel) || (mod_type == 2 && fp->mod_co2)) {
        snprintf(out_msg, (size_t)msg_sz, "該項目已完成改裝！"); return 0;
    }
    if (g_am.cash < cost) { snprintf(out_msg, (size_t)msg_sz, "改裝費不足 ($%lld)", (long long)cost); return 0; }
    g_am.cash -= cost; g_am.total_expenses += cost;
    if (mod_type == 0) { fp->mod_speed = 1; snprintf(out_msg, (size_t)msg_sz, "%s 改裝成功：速度 +10%%！", fp->reg); }
    else if (mod_type == 1) { fp->mod_fuel = 1; snprintf(out_msg, (size_t)msg_sz, "%s 改裝成功：油耗 -10%%！", fp->reg); }
    else { fp->mod_co2 = 1; snprintf(out_msg, (size_t)msg_sz, "%s 改裝成功：CO2 -10%%！", fp->reg); }
    am_sim_save(); return 1;
}
int am_ground_plane(int fleet_idx, char *out_msg, int msg_sz) {
    if (fleet_idx < 0 || fleet_idx >= g_am.fleet_count) return 0;
    FleetPlane *fp = &g_am.fleet[fleet_idx];
    if (fp->status == 1 || fp->status == 2) { snprintf(out_msg, (size_t)msg_sz, "執勤或維修中無法停飛封存"); return 0; }
    fp->status = (fp->status == 3) ? 0 : 3;
    snprintf(out_msg, (size_t)msg_sz, fp->status == 3 ? "%s 已停飛封存 (Ground A/C)" : "%s 已恢復待命！", fp->reg);
    am_sim_save(); return 1;
}

int am_buy_hub(int ap_idx, char *out_msg, int msg_sz) {
    if (g_am.hub_count >= AM_MAX_HUBS) { snprintf(out_msg, (size_t)msg_sz, "已擁有全部樞紐！"); return 0; }
    for (int i = 0; i < g_am.hub_count; i++) if (g_am.hubs[i] == ap_idx) { snprintf(out_msg, (size_t)msg_sz, "已擁有此樞紐！"); return 0; }
    int64_t cost = 3500000LL + (int64_t)(g_am.hub_count < 10 ? g_am.hub_count : 10) * 1000000LL;
    if (g_am.cash < cost) { snprintf(out_msg, (size_t)msg_sz, "購置樞紐需 $%lld", (long long)cost); return 0; }
    g_am.cash -= cost; g_am.total_expenses += cost; g_am.hubs[g_am.hub_count++] = ap_idx;
    if (g_am.base_reputation < 90) g_am.base_reputation += 1;
    snprintf(out_msg, (size_t)msg_sz, "新樞紐啟用：%s！(現有 %d 座樞紐)", am_airport_get(ap_idx)->iata, g_am.hub_count);
    check_achievements(); am_sim_save(); return 1;
}

int am_trade_stock(int rival_idx, int delta_shares, char *out_msg, int msg_sz) {
    if (rival_idx < 0 || rival_idx >= AM_NUM_RIVALS || delta_shares == 0) return 0;
    RivalAirline *rv = &g_am.rivals[rival_idx];
    if (delta_shares > 0) {
        int64_t cost = (int64_t)delta_shares * (int64_t)rv->price;
        if (g_am.cash < cost) { snprintf(out_msg, (size_t)msg_sz, "現金不足買入 %d 股！", delta_shares); return 0; }
        g_am.cash -= cost; int tot = rv->owned_shares + delta_shares;
        rv->avg_cost = (int)(((int64_t)rv->owned_shares * rv->avg_cost + cost) / tot); rv->owned_shares = tot;
        snprintf(out_msg, (size_t)msg_sz, "買入 %s %d 股 (@$%d)", rv->code, delta_shares, rv->price);
    } else {
        int sell = -delta_shares; if (rv->owned_shares < sell) sell = rv->owned_shares;
        if (sell <= 0) { snprintf(out_msg, (size_t)msg_sz, "未持有 %s 股票", rv->code); return 0; }
        int64_t gain = (int64_t)sell * (int64_t)rv->price; g_am.cash += gain; rv->owned_shares -= sell;
        snprintf(out_msg, (size_t)msg_sz, "賣出 %s %d 股，入帳 $%lld", rv->code, sell, (long long)gain);
    }
    am_sim_save(); return 1;
}
int am_take_loan(int64_t amount, char *out_msg, int msg_sz) {
    if (g_am.loan_balance + amount > 50000000LL) { snprintf(out_msg, (size_t)msg_sz, "貸款上限為 $50M！"); return 0; }
    g_am.cash += amount; g_am.loan_balance += (amount * 110LL) / 100LL;
    snprintf(out_msg, (size_t)msg_sz, "核貸成功 +$%lld (待還 $%lld)", (long long)amount, (long long)g_am.loan_balance);
    am_sim_save(); return 1;
}
int am_repay_loan(int64_t amount, char *out_msg, int msg_sz) {
    if (g_am.loan_balance <= 0) { snprintf(out_msg, (size_t)msg_sz, "目前無負債！"); return 0; }
    if (amount > g_am.loan_balance) amount = g_am.loan_balance;
    if (g_am.cash < amount) { snprintf(out_msg, (size_t)msg_sz, "現金不足還款！"); return 0; }
    g_am.cash -= amount; g_am.loan_balance -= amount;
    snprintf(out_msg, (size_t)msg_sz, "已償還 $%lld (剩餘 $%lld)", (long long)amount, (long long)g_am.loan_balance);
    am_sim_save(); return 1;
}
int am_spend_points(int type, char *out_msg, int msg_sz) {
    int cost = (type == 0) ? 50 : ((type == 1) ? 40 : 60);
    if (g_am.pax_points < cost) { snprintf(out_msg, (size_t)msg_sz, "積分不足！(需 %d PTS)", cost); return 0; }
    g_am.pax_points -= cost;
    if (type == 0) { g_am.cash += 2500000LL; snprintf(out_msg, (size_t)msg_sz, "兌換成功！資金 +$2.5M"); }
    else if (type == 1) { g_am.fuel_cap += 5000000LL; g_am.fuel_lbs += 2000000LL; snprintf(out_msg, (size_t)msg_sz, "兌換成功！油槽 +5M 並贈 2M 油"); }
    else { if (g_am.base_reputation < 95) g_am.base_reputation += 5; snprintf(out_msg, (size_t)msg_sz, "兌換成功！基礎聲譽永久 +5%%"); }
    am_sim_save(); return 1;
}
int am_set_catering(int level, char *out_msg, int msg_sz) {
    if (level < 0 || level > 2) return 0;
    if (g_am.catering_level == level) { snprintf(out_msg, (size_t)msg_sz, "目前已是此餐飲等級"); return 0; }
    int64_t cost = (level == 1) ? 450000LL : ((level == 2) ? 1500000LL : 0LL);
    if (cost > 0 && g_am.cash < cost) { snprintf(out_msg, (size_t)msg_sz, "升級餐飲資金不足 ($%lld)", (long long)cost); return 0; }
    g_am.cash -= cost; g_am.total_expenses += cost; g_am.catering_level = level;
    snprintf(out_msg, (size_t)msg_sz, level == 2 ? "升級五星主廚餐飲！" : (level == 1 ? "啟用經濟輕食！" : "已關閉餐飲"));
    check_achievements(); am_sim_save(); return 1;
}
int am_join_alliance(int alliance_id, char *out_msg, int msg_sz) {
    if (alliance_id < 1 || alliance_id > 3) return 0;
    if (g_am.alliance_id == alliance_id) { snprintf(out_msg, (size_t)msg_sz, "已屬於 %s", am_alliance_name(alliance_id)); return 0; }
    if (g_am.cash < 1000000LL) { snprintf(out_msg, (size_t)msg_sz, "加入航空聯盟需入會費 $1M"); return 0; }
    g_am.cash -= 1000000LL; g_am.total_expenses += 1000000LL; g_am.alliance_id = alliance_id;
    snprintf(out_msg, (size_t)msg_sz, "成功加入【%s】！聲譽永久 +6%%", am_alliance_name(alliance_id));
    check_achievements(); am_sim_save(); return 1;
}
int am_run_charter_with_plane(int fleet_idx, char *out_msg, int msg_sz) {
    if (fleet_idx < 0 || fleet_idx >= g_am.fleet_count) { snprintf(out_msg, (size_t)msg_sz, "無可用待命飛機"); return 0; }
    FleetPlane *fp = &g_am.fleet[fleet_idx];
    if (fp->status != 0) { snprintf(out_msg, (size_t)msg_sz, "請挑選待命飛機執行包機"); return 0; }
    int dist = am_calc_distance_km(g_am.charter_orig, g_am.charter_dest);
    const AircraftModel *m = am_aircraft_get(fp->model_idx);
    int64_t fuel_need = (int64_t)dist * (int64_t)m->fuel_lbs_km;
    if (!g_am.cheat_god_fuel && g_am.fuel_lbs < fuel_need) { snprintf(out_msg, (size_t)msg_sz, "燃油不足承接包機"); return 0; }
    if (!g_am.cheat_god_fuel) g_am.fuel_lbs -= fuel_need;
    g_am.cash += g_am.charter_reward; g_am.total_revenue += g_am.charter_reward;
    g_am.pax_points += g_am.charter_pts; g_am.charter_done_cnt++;
    if (!g_am.cheat_no_wear) { fp->wear_pct += 4; if (fp->wear_pct > 100) fp->wear_pct = 100; }
    snprintf(out_msg, (size_t)msg_sz, "VIP 包機完成！+$%lld & +%d PTS！", (long long)g_am.charter_reward, g_am.charter_pts);
    am_roll_new_charter(); check_achievements(); am_sim_save(); return 1;
}
int am_hire_staff(int dept, int count, char *out_msg, int msg_sz) {
    if (count <= 0) return 0;
    static const int UNIT_COST[4] = { 18000, 9000, 14000, 11000 };
    static const char *DNM[4] = { "機師", "空服員", "工程師", "地勤技師" };
    int64_t cost = (int64_t)UNIT_COST[dept & 3] * (int64_t)count;
    if (g_am.cash < cost) { snprintf(out_msg, (size_t)msg_sz, "招募%s資金不足 ($%lld)", DNM[dept & 3], (long long)cost); return 0; }
    g_am.cash -= cost; g_am.total_expenses += cost;
    if (dept == 0) g_am.staff_pilots += count;
    else if (dept == 1) g_am.staff_crew += count;
    else if (dept == 2) g_am.staff_engineers += count;
    else g_am.staff_techs += count;
    if (g_am.staff_morale < 98) g_am.staff_morale += 2;
    snprintf(out_msg, (size_t)msg_sz, "成功招募 %d 名%s！", count, DNM[dept & 3]);
    am_sim_save(); return 1;
}
int am_upgrade_training(int track, char *out_msg, int msg_sz) {
    if (g_am.training_points <= 0) { snprintf(out_msg, (size_t)msg_sz, "訓練點數不足"); return 0; }
    int *lv = (track == 0) ? &g_am.train_mgmt_lv : ((track == 1) ? &g_am.train_fuel_lv : &g_am.train_maint_lv);
    if (*lv >= 5) { snprintf(out_msg, (size_t)msg_sz, "此訓練已達滿級 Lv.5！"); return 0; }
    (*lv)++; g_am.training_points--;
    snprintf(out_msg, (size_t)msg_sz, "訓練升級至 Lv.%d！", *lv);
    am_sim_save(); return 1;
}
int am_claim_daily_bonus(char *out_msg, int msg_sz) {
    int64_t now = hal_get_unix_time();
    if (g_am.last_daily_bonus_ts > 0 && (now - g_am.last_daily_bonus_ts) < 3600 * 6) {
        snprintf(out_msg, (size_t)msg_sz, "冷卻中！每 6 小時可領一次"); return 0;
    }
    g_am.last_daily_bonus_ts = now;
    int64_t cb = 500000LL + (int64_t)am_airline_level() * 150000LL;
    int pb = 20 + am_airline_level() * 2;
    g_am.cash += cb; g_am.pax_points += pb; g_am.training_points += 1;
    snprintf(out_msg, (size_t)msg_sz, "領取寶箱！+$%lld & +%d PTS！", (long long)cb, pb);
    am_sim_save(); return 1;
}

/* ============================================================
 * 史詩級 MOD MENU 外掛引擎
 * ============================================================ */
void am_cheat_set_cash(int64_t exact_cash, char *out_msg, int msg_sz) {
    if (exact_cash < 0) exact_cash = 0;
    g_am.cash = exact_cash;
    snprintf(out_msg, (size_t)msg_sz, "[MOD] 資金已精準修改為 $%lld！", (long long)exact_cash);
    check_achievements(); am_sim_save();
}
void am_cheat_add_cash(int64_t amount, char *out_msg, int msg_sz) {
    g_am.cash += amount;
    snprintf(out_msg, (size_t)msg_sz, "[MOD] 資金注入 +$%lld！", (long long)amount);
    check_achievements(); am_sim_save();
}
void am_cheat_add_points(int pts, char *out_msg, int msg_sz) {
    g_am.pax_points += pts; g_am.training_points += 10;
    snprintf(out_msg, (size_t)msg_sz, "[MOD] +%d PTS & +10 訓練點！", pts);
    am_sim_save();
}
void am_cheat_refill_all(char *out_msg, int msg_sz) {
    if (g_am.fuel_cap < 50000000LL) g_am.fuel_cap = 50000000LL;
    if (g_am.co2_cap  < 50000000LL) g_am.co2_cap  = 50000000LL;
    g_am.fuel_lbs = g_am.fuel_cap; g_am.co2_quotas = g_am.co2_cap;
    snprintf(out_msg, (size_t)msg_sz, "[MOD] 油槽與 CO2 已免費擴滿至 50M！");
    am_sim_save();
}
int am_cheat_instant_finish_flights(char *out_msg, int msg_sz) {
    int done = 0;
    for (int i = 0; i < g_am.fleet_count; i++) {
        FleetPlane *fp = &g_am.fleet[i];
        if (fp->active && fp->status == 1) { complete_flight(i); done++; }
        else if (fp->active && fp->status == 2) {
            fp->status = 0; fp->flight_elapsed_sec = 0; fp->wear_pct = 0;
            fp->acheck_hrs_left = am_aircraft_get(fp->model_idx)->acheck_hrs; done++;
        }
    }
    snprintf(out_msg, (size_t)msg_sz, done > 0 ? "[MOD] 已瞬移完成 %d 架航班/維修！" : "[MOD] 目前無飛行中班機", done);
    am_sim_save(); return done;
}
void am_cheat_max_reputation(char *out_msg, int msg_sz) {
    g_am.base_reputation = 99; g_am.staff_morale = 100;
    g_am.mkt_airline_secs = 24 * 3600; g_am.mkt_airline_bonus = 32; g_am.mkt_eco_secs = 24 * 3600;
    snprintf(out_msg, (size_t)msg_sz, "[MOD] 聲譽 99%% & 士氣 100%% & 全廣告滿檔！");
    check_achievements(); am_sim_save();
}
void am_cheat_unlock_all_achs(char *out_msg, int msg_sz) {
    int cnt = 0;
    for (int i = 0; i < AM_NUM_ACH; i++) if (!g_am.ach_unlocked[i]) { g_am.ach_unlocked[i] = 1; g_am.cash += AM_ACHS[i].reward; g_am.pax_points += 25; cnt++; }
    snprintf(out_msg, (size_t)msg_sz, "[MOD] 已解鎖全部 %d 項成就！", cnt);
    am_sim_save();
}
void am_cheat_unlock_all_hubs(char *out_msg, int msg_sz) {
    int ac = am_airport_count(), added = 0;
    for (int i = 0; i < ac && g_am.hub_count < AM_MAX_HUBS; i++) {
        int has = 0;
        for (int h = 0; h < g_am.hub_count; h++) if (g_am.hubs[h] == i) { has = 1; break; }
        if (!has) { g_am.hubs[g_am.hub_count++] = i; added++; }
    }
    snprintf(out_msg, (size_t)msg_sz, "[MOD] 已解鎖全球 %d 座樞紐機場！(共 %d 座)", added, g_am.hub_count);
    check_achievements(); am_sim_save();
}
void am_cheat_max_staff_and_training(char *out_msg, int msg_sz) {
    g_am.staff_pilots += 500; g_am.staff_crew += 1500; g_am.staff_engineers += 500; g_am.staff_techs += 500;
    g_am.train_mgmt_lv = 5; g_am.train_fuel_lv = 5; g_am.train_maint_lv = 5;
    g_am.staff_morale = 100;
    snprintf(out_msg, (size_t)msg_sz, "[MOD] 員工大幅擴編 + 全專業訓練 Lv.5 滿級！");
    am_sim_save();
}
void am_cheat_force_market_refresh(char *out_msg, int msg_sz) {
    g_am.market_real_sec_acc = 0;
    step_stochastic_market();
    snprintf(out_msg, (size_t)msg_sz, "[MOD] 已手動刷新市場！油價 $%d · %s", g_am.fuel_price, am_event_name(g_am.event_id));
}

/* 核心 MOD：用指定機型一鍵開通「整個國家所有機場」或「全世界所有機場」的航線！ */
static int spawn_mod_plane_with_route(int model_idx, int orig_ap, int dest_ap) {
    if (orig_ap == dest_ap || g_am.fleet_count >= AM_MAX_FLEET) return 0;
    /* 避免重複開通已存在的完全相同航線 */
    for (int i = 0; i < g_am.fleet_count; i++) {
        if (g_am.fleet[i].active && g_am.fleet[i].has_route &&
            g_am.fleet[i].orig_ap == orig_ap && g_am.fleet[i].dest_ap == dest_ap) return 0;
    }
    const AircraftModel *m = am_aircraft_get(model_idx);
    int dist = am_calc_distance_km(orig_ap, dest_ap);
    FleetPlane *fp = &g_am.fleet[g_am.fleet_count++];
    memset(fp, 0, sizeof(*fp));
    fp->active = 1; fp->model_idx = model_idx;
    snprintf(fp->reg, sizeof(fp->reg), "%s-%03d", g_am.iata_code, g_am.fleet_count);
    if (m->is_cargo) { fp->seats_y = m->cap; }
    else {
        fp->seats_f = (m->cap >= 200) ? (m->cap / 18) : 0;
        fp->seats_j = (m->cap >= 80)  ? (m->cap / 9)  : 0;
        fp->seats_y = m->cap - fp->seats_j * 2 - fp->seats_f * 3;
    }
    fp->has_route = 1; fp->orig_ap = orig_ap; fp->dest_ap = dest_ap; fp->stopover_ap = -1;
    fp->dist_km = dist; fp->leg_dir = 0; fp->auto_return = 1; fp->cost_index = 150;
    fp->price_y = am_calc_auto_ticket(dist, m->is_cargo ? 3 : 0, g_am.realism_mode);
    fp->price_j = am_calc_auto_ticket(dist, 1, g_am.realism_mode);
    fp->price_f = am_calc_auto_ticket(dist, 2, g_am.realism_mode);
    fp->acheck_hrs_left = m->acheck_hrs;
    return 1;
}

int am_cheat_open_routes_country(int model_idx, const char *country_code, char *out_msg, int msg_sz) {
    int aps[128], n = am_airports_in_country(country_code, aps, 128);
    int orig = g_am.hubs[0], opened = 0;
    for (int i = 0; i < n; i++) {
        if (spawn_mod_plane_with_route(model_idx, orig, aps[i])) opened++;
    }
    snprintf(out_msg, (size_t)msg_sz, "[MOD] 已用 %s 開通【%s】%d 條全國航線！",
             am_aircraft_get(model_idx)->model, am_country_name_zh(country_code), opened);
    check_achievements(); am_sim_save(); return opened;
}

int am_cheat_open_routes_world(int model_idx, char *out_msg, int msg_sz) {
    int ac = am_airport_count(), orig = g_am.hubs[0], opened = 0;
    for (int i = 0; i < ac && g_am.fleet_count < AM_MAX_FLEET; i++) {
        if (spawn_mod_plane_with_route(model_idx, orig, i)) opened++;
    }
    snprintf(out_msg, (size_t)msg_sz, "[MOD] 全球制霸！已用 %s 開通全世界 %d 條航線！",
             am_aircraft_get(model_idx)->model, opened);
    check_achievements(); am_sim_save(); return opened;
}
