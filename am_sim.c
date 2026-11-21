#include "am_sim.h"
#include "hal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

AmWorld g_am;

static const int SPEED_MULTS[4] = { 1, 10, 60, 360 };
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
    return SPEED_MULTS[(i >= 0 && i < 4) ? i : 0];
}

const char *am_event_name(int id) {
    switch (id) {
        case 1: return "產油國緊急減產";
        case 2: return "國際頁岩油增產";
        case 3: return "全球旅遊旺季爆發";
        case 4: return "碳排放環保補貼";
        case 5: return "跨洲商務會議潮";
        case 6: return "極端氣候氣流擾動";
        default: return "全球航空市場平穩";
    }
}
const char *am_event_desc(int id) {
    switch (id) {
        case 1: return "油價劇烈飆升！建議動用低價囤油";
        case 2: return "油價大幅跳水！絕佳囤油時機";
        case 3: return "全航線載客需求 +15%%！快派機往返";
        case 4: return "CO2 碳權配額降價促銷中！";
        case 5: return "商務艙與頭等艙收益額外 +12%%！";
        case 6: return "油耗微增 8%%，請注意備足燃油";
        default: return "油價與碳權自由隨機波動中";
    }
}

typedef struct { const char *name; const char *desc; int64_t reward; } AmAchDef;
static const AmAchDef AM_ACHS[AM_NUM_ACH] = {
    { "首航啟程",     "完成第 1 趟商業航班",            250000LL },
    { "雙向通航",     "累計完成 10 趟往返航班",         500000LL },
    { "航空新星",     "累計完成 50 趟航班",            1500000LL },
    { "區域機隊",     "機隊規模達到 3 架飛機",          600000LL },
    { "大型機隊",     "機隊規模達到 10 架飛機",        2500000LL },
    { "跨洲霸主",     "機隊規模達到 20 架飛機",        6000000LL },
    { "全球樞紐",     "擁有 2 座全球樞紐機場",         1000000LL },
    { "五大洲網",     "擁有 5 座全球樞紐機場",         3500000LL },
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
    int lv = 1 + (g_am.total_flights_done * 2 + g_am.fleet_count * 12 + g_am.hub_count * 20 + am_ach_unlocked_count() * 15) / 35;
    return lv > 20 ? 20 : lv;
}
const char *am_airline_rank_name(int lv) {
    if (lv >= 18) return "全球航空帝國霸主";
    if (lv >= 14) return "五星洲際旗艦航空";
    if (lv >= 10) return "跨國聯盟核心成員";
    if (lv >= 6)  return "國際幹線航空公司";
    if (lv >= 3)  return "區域成長型航空";
    return "新創起飛航空公司";
}

int am_effective_reputation(void) {
    int rep = g_am.base_reputation;
    if (g_am.mkt_airline_secs > 0) rep += g_am.mkt_airline_bonus;
    if (g_am.mkt_eco_secs > 0) rep += 10;
    if (g_am.staff_morale < 60) rep -= (60 - g_am.staff_morale) / 3;
    if (rep < 20) rep = 20;
    if (rep > 99) rep = 99;
    return rep;
}

int am_calc_auto_ticket(int dist_km, int cabin, int realism) {
    if (cabin == 0) return ((realism ? 30 : 40) * dist_km) / 100 + 150;
    if (cabin == 1) return ((realism ? 60 : 80) * dist_km) / 100 + 500;
    if (cabin == 2) return ((realism ? 90 : 120) * dist_km) / 100 + 1000;
    return ((realism ? 85 : 105) * dist_km) / 100 + 380;
}

int am_calc_route_fee(int dist_km) {
    return 25000 + dist_km * 22;
}

void am_calc_leg_preview(int model_idx, int dist_km, int cost_index, int mod_spd, int mod_fuel,
                         int *out_spd_kmh, int *out_secs, int64_t *out_fuel_lbs) {
    const AircraftModel *m = am_aircraft_get(model_idx);
    if (cost_index < 50) cost_index = 50;
    if (cost_index > 200) cost_index = 200;

    /* Cost Index 影響巡航速度 (80%..110%) 與油耗 (85%..115%) */
    int spd = (m->speed_kmh * (80 + ((cost_index - 50) * 30) / 150)) / 100;
    if (mod_spd) spd = (spd * 110) / 100;
    if (!g_am.realism_mode) spd = (spd * 150) / 100;
    if (spd < 180) spd = 180;

    int sec = (dist_km * 3600) / spd;
    if (sec < 600) sec = 600;

    int64_t fuel = (int64_t)dist_km * (int64_t)m->fuel_lbs_km;
    fuel = (fuel * (85 + ((cost_index - 50) * 30) / 150)) / 100;
    if (mod_fuel) fuel = (fuel * 90) / 100;
    if (g_am.event_id == 6) fuel = (fuel * 108) / 100;

    if (out_spd_kmh) *out_spd_kmh = spd;
    if (out_secs) *out_secs = sec;
    if (out_fuel_lbs) *out_fuel_lbs = fuel;
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
        g_am.fleet_count >= 3, g_am.fleet_count >= 10, g_am.fleet_count >= 20,
        g_am.hub_count >= 2, g_am.hub_count >= 5, g_am.bought_low_fuel > 0,
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
        if (g_am.event_id == 1) drift += 140;
        else if (g_am.event_id == 2) drift -= 150;
        g_am.fuel_price += drift + noise + jump;
        if (g_am.fuel_price < 300) g_am.fuel_price = 300 + rng_range(0, 45);
        if (g_am.fuel_price > 3000) g_am.fuel_price = 3000 - rng_range(0, 80);
    }
    push_hist(g_am.fuel_hist, g_am.fuel_price);

    int c_noise = rng_range(-18, 18) + ((rng_range(0, 99) < 14) ? rng_range(-35, 35) : 0);
    if (g_am.event_id == 4) c_noise -= 12;
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

    if (g_am.event_secs_left <= 0 && rng_range(0, 99) < 22) {
        g_am.event_id = rng_range(1, 6);
        g_am.event_secs_left = rng_range(1800, 7200);
        snprintf(g_am.last_notice, sizeof(g_am.last_notice), "突發新聞：%s！", am_event_name(g_am.event_id));
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

void am_sim_new_company(const char *name, const char *iata, int hub_ap_idx,
                        int realism_mode, int64_t start_cash, int starter_model_idx) {
    memset(&g_am, 0, sizeof(g_am));
    g_am.magic = AM_SAVE_MAGIC; g_am.initialized = 1;
    snprintf(g_am.airline_name, sizeof(g_am.airline_name), "%s", (name && name[0]) ? name : "SkyEmpire Air");
    snprintf(g_am.iata_code, sizeof(g_am.iata_code), "%s", (iata && iata[0]) ? iata : "SE");
    g_am.realism_mode = realism_mode ? 1 : 0;
    g_am.speed_mult_idx = 2; /* 60x */
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

    g_am.base_reputation = 58; g_am.staff_salary_pct = 100; g_am.staff_morale = 88;
    g_am.own_share_price = 25;
    for (int i = 0; i < AM_PRICE_HIST; i++) g_am.own_share_hist[i] = 25;
    init_rivals();

    /* 玩家剛選的起始客機 (A320-200 或 B737-800) */
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

    g_am.last_unix_time = hal_get_unix_time();
    snprintf(g_am.last_notice, sizeof(g_am.last_notice), "歡迎成立 %s！已交付首架 %s，請設定雙向航線", g_am.airline_name, sm->model);
    am_sim_save();
}

void am_sim_save(void) {
    if (!g_am.initialized) return;
    g_am.magic = AM_SAVE_MAGIC; g_am.last_unix_time = hal_get_unix_time();
    FILE *f = fopen(hal_get_save_path(), "wb"); if (!f) return;
    fwrite(&g_am, sizeof(g_am), 1, f); fclose(f);
}

int am_sim_load(void) {
    FILE *f = fopen(hal_get_save_path(), "rb"); if (!f) return 0;
    AmWorld tmp; int ok = (fread(&tmp, sizeof(tmp), 1, f) == 1 && tmp.magic == AM_SAVE_MAGIC && tmp.initialized);
    fclose(f); if (!ok) return 0;
    g_am = tmp;
    int64_t now = hal_get_unix_time(), offline = now - g_am.last_unix_time;
    if (offline > 5 && offline < 86400 * 30) {
        am_sim_tick((int)offline, 0);
        snprintf(g_am.last_notice, sizeof(g_am.last_notice), "離線掛機 %ld 分鐘，已自動完成往返結算！", (long)(offline / 60));
    }
    g_am.last_unix_time = now; return 1;
}

void am_sim_init(void) {
    g_rng_state = ((uint64_t)hal_get_seed() << 32) ^ (uint64_t)hal_get_unix_time() ^ 0x9E3779B97F4A7C15ULL;
    if (g_rng_state == 0) g_rng_state = 0x123456789ABCDEFULL;
    am_data_init(hal_get_extra_airports_csv());
    memset(&g_am, 0, sizeof(g_am));
}

static void start_leg_flight(FleetPlane *fp) {
    const AircraftModel *m = am_aircraft_get(fp->model_idx);
    int spd = 0, sec = 0; int64_t fuel_need = 0;
    am_calc_leg_preview(fp->model_idx, fp->dist_km, fp->cost_index, fp->mod_speed, fp->mod_fuel, &spd, &sec, &fuel_need);
    int64_t co2_need = ((int64_t)fp->dist_km * (int64_t)m->co2_kg * (int64_t)(m->is_cargo ? 12 : m->cap)) / 1000LL;
    if (fp->mod_co2) co2_need = (co2_need * 90) / 100;
    if (g_am.fuel_lbs < fuel_need) return;

    g_am.fuel_lbs -= fuel_need;
    g_am.co2_quotas -= co2_need;
    if (g_am.co2_quotas < 0 && g_am.base_reputation > 40) g_am.base_reputation--;

    fp->status = 1;
    fp->flight_elapsed_sec = 0;
    fp->flight_total_sec = sec;
}

/* 根據玩家自訂票價與最佳票價比例計算載客彈性 (定太貴沒人搭，定合理高獲利!) */
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

static void complete_flight(int fleet_idx) {
    FleetPlane *fp = &g_am.fleet[fleet_idx];
    const AircraftModel *m = am_aircraft_get(fp->model_idx);
    int rep = am_effective_reputation();
    int base_load = rep - 10 + rng_range(-4, 9);
    if (g_am.event_id == 3) base_load += 15;

    int64_t rev = 0; int carried = 0;
    if (m->is_cargo) {
        int lf = ticket_load_factor(fp->price_y, am_calc_auto_ticket(fp->dist_km, 3, g_am.realism_mode), base_load);
        int tons = (fp->seats_y * lf) / 100; if (tons < 1) tons = 1;
        rev = (int64_t)tons * (int64_t)fp->price_y;
        g_am.total_cargo_tons += tons; carried = tons;
    } else {
        int ly = ticket_load_factor(fp->price_y, am_calc_auto_ticket(fp->dist_km, 0, g_am.realism_mode), base_load);
        int lj = ticket_load_factor(fp->price_j, am_calc_auto_ticket(fp->dist_km, 1, g_am.realism_mode), base_load);
        int lf = ticket_load_factor(fp->price_f, am_calc_auto_ticket(fp->dist_km, 2, g_am.realism_mode), base_load);
        int py = (fp->seats_y * ly) / 100, pj = (fp->seats_j * lj) / 100, pf = (fp->seats_f * lf) / 100;
        int64_t ry = (int64_t)py * (int64_t)fp->price_y;
        int64_t rj = (int64_t)pj * (int64_t)fp->price_j;
        int64_t rf = (int64_t)pf * (int64_t)fp->price_f;
        if (g_am.event_id == 5) { rj = (rj * 112) / 100; rf = (rf * 112) / 100; }
        rev = ry + rj + rf; carried = py + pj + pf;
        g_am.total_pax_carried += carried;
    }

    int64_t fine = (fp->acheck_hrs_left <= 0) ? (rev / 4) : 0;
    if (fine > 0 && g_am.base_reputation > 35) g_am.base_reputation -= 2;

    int64_t net = rev - fine;
    g_am.cash += net; g_am.total_revenue += rev; g_am.total_expenses += fine;
    g_am.last_flight_profit = net; g_am.total_flights_done++;
    fp->total_flights++; fp->total_profit += net;
    fp->status = 0; fp->flight_elapsed_sec = 0;

    int flt_hrs = fp->flight_total_sec / 3600; if (flt_hrs < 1) flt_hrs = 1;
    fp->acheck_hrs_left -= flt_hrs; if (fp->acheck_hrs_left < 0) fp->acheck_hrs_left = 0;
    fp->wear_pct += 2 + (flt_hrs / 3); if (fp->wear_pct > 100) fp->wear_pct = 100;
    if (g_am.base_reputation < 75 && rng_range(0, 99) < 25) g_am.base_reputation++;

    /* 記錄真實雙向航段 (去程 TPE->SIN / 回程 SIN->TPE) */
    const AirportInfo *a_from = am_airport_get(fp->leg_dir == 0 ? fp->orig_ap : fp->dest_ap);
    const AirportInfo *a_to   = am_airport_get(fp->leg_dir == 0 ? fp->dest_ap : fp->orig_ap);
    for (int i = AM_MAX_FLOG - 1; i > 0; i--) g_am.flight_log[i] = g_am.flight_log[i - 1];
    FlightLogEntry *fl = &g_am.flight_log[0];
    snprintf(fl->reg, sizeof(fl->reg), "%s", fp->reg);
    snprintf(fl->route, sizeof(fl->route), "%s->%s", a_from->iata, a_to->iata);
    fl->is_cargo = m->is_cargo; fl->pax_or_tons = carried; fl->net_profit = net;
    if (g_am.flight_log_count < AM_MAX_FLOG) g_am.flight_log_count++;

    /* 雙向往返核心：切換航段方向！若剛飛完去程 (leg_dir==0) 且開啟自動往返，自動載客飛回原樞紐！ */
    int was_outbound = (fp->leg_dir == 0);
    fp->leg_dir = 1 - fp->leg_dir;
    if (was_outbound && fp->auto_return) {
        start_leg_flight(fp);
        snprintf(g_am.last_notice, sizeof(g_am.last_notice), "%s 抵達 %s (+$%lld) 並已自動起飛返航 %s！",
                 fp->reg, a_to->iata, (long long)net, a_from->iata);
    } else {
        snprintf(g_am.last_notice, sizeof(g_am.last_notice), "%s 返抵 %s 完成往返！單程淨利 +$%lld",
                 fp->reg, a_to->iata, (long long)net);
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
    if (g_am.event_secs_left > 0) { g_am.event_secs_left -= sim_sec; if (g_am.event_secs_left <= 0) { g_am.event_secs_left = 0; g_am.event_id = 0; major = 1; } }

    int tm = (g_am.staff_salary_pct >= 115) ? 98 : (g_am.staff_salary_pct >= 100 ? 88 : (g_am.staff_salary_pct >= 90 ? 72 : 52));
    if (g_am.staff_morale < tm) g_am.staff_morale++; else if (g_am.staff_morale > tm) g_am.staff_morale--;

    g_am.market_tick_acc += sim_sec;
    int steps = g_am.market_tick_acc / 600; if (steps > 24) steps = 24;
    if (steps > 0) { g_am.market_tick_acc %= 600; for (int s = 0; s < steps; s++) step_stochastic_market(); major = 1; }
    return major;
}

int am_buy_aircraft_custom(int model_idx, int hub_ap_idx, int sy, int sj, int sf, char *out_msg, int msg_sz) {
    if (g_am.fleet_count >= AM_MAX_FLEET) { snprintf(out_msg, (size_t)msg_sz, "機隊已達上限 (%d 架)！", AM_MAX_FLEET); return 0; }
    const AircraftModel *m = am_aircraft_get(model_idx);
    if (g_am.cash < m->price) { snprintf(out_msg, (size_t)msg_sz, "資金不足！需要 $%lld", (long long)m->price); return 0; }
    if (!m->is_cargo && (sy + sj * 2 + sf * 3 > m->cap || sy < 0 || sj < 0 || sf < 0)) {
        snprintf(out_msg, (size_t)msg_sz, "座位空間超過機型上限 (%u)！", (unsigned)m->cap); return 0;
    }

    g_am.cash -= m->price; g_am.total_expenses += m->price;
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

    snprintf(out_msg, (size_t)msg_sz, "購入 %s (%s)！配置 Y:%d J:%d F:%d", m->model, fp->reg, fp->seats_y, fp->seats_j, fp->seats_f);
    check_achievements(); am_sim_save(); return 1;
}

int am_sell_aircraft(int fleet_idx, char *out_msg, int msg_sz) {
    if (fleet_idx < 0 || fleet_idx >= g_am.fleet_count) return 0;
    if (g_am.fleet[fleet_idx].status != 0) { snprintf(out_msg, (size_t)msg_sz, "飛機執勤或維修中無法出售"); return 0; }
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
    if (!fp->has_route) { snprintf(out_msg, (size_t)msg_sz, "%s 尚未設定雙向航線", fp->reg); return 0; }
    if (fp->status != 0) { snprintf(out_msg, (size_t)msg_sz, "%s 正在執勤或維修中", fp->reg); return 0; }

    int spd = 0, sec = 0; int64_t fuel_need = 0;
    am_calc_leg_preview(fp->model_idx, fp->dist_km, fp->cost_index, fp->mod_speed, fp->mod_fuel, &spd, &sec, &fuel_need);
    if (g_am.fuel_lbs < fuel_need) { snprintf(out_msg, (size_t)msg_sz, "燃油不足！需 %lld lbs", (long long)fuel_need); return 0; }

    start_leg_flight(fp);
    const AirportInfo *a_from = am_airport_get(fp->leg_dir == 0 ? fp->orig_ap : fp->dest_ap);
    const AirportInfo *a_to   = am_airport_get(fp->leg_dir == 0 ? fp->dest_ap : fp->orig_ap);
    snprintf(out_msg, (size_t)msg_sz, "%s 起飛 (%s -> %s)！耗油 %lld lbs", fp->reg, a_from->iata, a_to->iata, (long long)fuel_need);
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
    g_am.cash -= 1200000LL; g_am.fuel_cap += 2000000LL;
    snprintf(out_msg, (size_t)msg_sz, "油槽擴充成功！上限 %lld lbs", (long long)g_am.fuel_cap);
    am_sim_save(); return 1;
}
int am_upgrade_co2_tank(char *out_msg, int msg_sz) {
    if (g_am.cash < 900000LL) { snprintf(out_msg, (size_t)msg_sz, "擴充碳槽需 $0.9M"); return 0; }
    g_am.cash -= 900000LL; g_am.co2_cap += 2000000LL;
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
    int64_t cost = (int64_t)fp->wear_pct * 4500LL + 15000LL;
    if (g_am.cash < cost) { snprintf(out_msg, (size_t)msg_sz, "維修費不足 ($%lld)", (long long)cost); return 0; }
    g_am.cash -= cost; g_am.total_expenses += cost; fp->wear_pct = 0;
    snprintf(out_msg, (size_t)msg_sz, "%s 修復完成！磨損歸零", fp->reg);
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
    snprintf(out_msg, (size_t)msg_sz, "%s 進入 A-Check 大檢 (4小時)", fp->reg);
    am_sim_save(); return 1;
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
int am_buy_hub(int ap_idx, char *out_msg, int msg_sz) {
    if (g_am.hub_count >= AM_MAX_HUBS) { snprintf(out_msg, (size_t)msg_sz, "樞紐已達上限 (%d)", AM_MAX_HUBS); return 0; }
    for (int i = 0; i < g_am.hub_count; i++) if (g_am.hubs[i] == ap_idx) { snprintf(out_msg, (size_t)msg_sz, "已擁有此樞紐！"); return 0; }
    int64_t cost = 3500000LL + (int64_t)g_am.hub_count * 1500000LL;
    if (g_am.cash < cost) { snprintf(out_msg, (size_t)msg_sz, "購置樞紐需 $%lld", (long long)cost); return 0; }
    g_am.cash -= cost; g_am.total_expenses += cost; g_am.hubs[g_am.hub_count++] = ap_idx;
    if (g_am.base_reputation < 90) g_am.base_reputation += 2;
    snprintf(out_msg, (size_t)msg_sz, "新樞紐啟用：%s！基礎聲譽 +2%%", am_airport_get(ap_idx)->iata);
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
    if (g_am.loan_balance + amount > 30000000LL) { snprintf(out_msg, (size_t)msg_sz, "貸款上限為 $30M！"); return 0; }
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
    if (g_am.pax_points < cost) { snprintf(out_msg, (size_t)msg_sz, "積分不足！(需 %d Points)", cost); return 0; }
    g_am.pax_points -= cost;
    if (type == 0) { g_am.cash += 2500000LL; snprintf(out_msg, (size_t)msg_sz, "兌換成功！資金 +$2.5M"); }
    else if (type == 1) { g_am.fuel_cap += 2000000LL; g_am.fuel_lbs += 1000000LL; snprintf(out_msg, (size_t)msg_sz, "兌換成功！油槽 +2M 並贈 1M 油"); }
    else { if (g_am.base_reputation < 95) g_am.base_reputation += 5; snprintf(out_msg, (size_t)msg_sz, "兌換成功！基礎聲譽永久 +5%%"); }
    am_sim_save(); return 1;
}
