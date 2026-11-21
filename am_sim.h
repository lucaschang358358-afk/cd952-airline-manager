#ifndef AM_SIM_H
#define AM_SIM_H

#include <stdint.h>
#include "am_data.h"

#define AM_MAX_FLEET     48
#define AM_MAX_HUBS      12
#define AM_PRICE_HIST    32
#define AM_NUM_RIVALS    10
#define AM_SAVE_MAGIC    0x414D3431u /* "AM41" */

typedef struct {
    int active;
    int model_idx;
    char reg[12];           /* 機身註冊編號 (如 JX-001) */
    int seats_y, seats_j, seats_f; /* 客機艙等配置 (貨機則 seats_y = 貨運噸數) */

    int has_route;
    int orig_ap;
    int dest_ap;
    int stopover_ap;        /* -1 表示直飛 */
    int dist_km;
    int price_y, price_j, price_f;

    int status;             /* 0=停機待命, 1=飛行中, 2=A-Check 維修中 */
    int flight_elapsed_sec;
    int flight_total_sec;

    int wear_pct;           /* 0..100 機體磨損率 */
    int acheck_hrs_left;    /* 剩餘可飛時數 */
    int mod_speed;          /* 1=+10% 巡航速度 */
    int mod_fuel;           /* 1=-10% 油耗 */
    int mod_co2;            /* 1=-10% CO2 排放 */

    int total_flights;
    int64_t total_profit;
} FleetPlane;

typedef struct {
    char code[5];
    char name[20];
    int price;              /* 股價 ($/股) */
    int hist[AM_PRICE_HIST];
    int owned_shares;
    int avg_cost;
} RivalAirline;

typedef struct {
    uint32_t magic;
    int initialized;

    /* 玩家航空基本資料 */
    char airline_name[28];
    char iata_code[4];
    int realism_mode;       /* 0=Easy, 1=Realism */
    int speed_mult_idx;     /* 0=1x, 1=10x, 2=60x, 3=360x */

    int64_t cash;
    int64_t total_revenue;
    int64_t total_expenses;
    int64_t last_flight_profit;
    int total_flights_done;
    int total_pax_carried;
    int total_cargo_tons;

    /* 樞紐機場 */
    int hubs[AM_MAX_HUBS];
    int hub_count;

    /* 機隊 */
    FleetPlane fleet[AM_MAX_FLEET];
    int fleet_count;

    /* 油料與 CO2 儲槽與完全無規律動態市場 */
    int64_t fuel_lbs;
    int64_t fuel_cap;
    int64_t co2_quotas;
    int64_t co2_cap;
    int fuel_price;         /* $300..$3000 / 1000 lbs */
    int co2_price;          /* $100..$200 / 1000 quotas */
    int fuel_hist[AM_PRICE_HIST];
    int co2_hist[AM_PRICE_HIST];
    int market_tick_acc;    /* 累積秒數觸發市場隨機跳躍 */

    /* 全球隨機突發事件 */
    int event_id;           /* 0=平靜, 1..6=突發事件 */
    int event_secs_left;

    /* 行銷與聲譽 */
    int base_reputation;    /* 45..99 */
    int mkt_airline_secs;   /* 航空廣告剩餘秒數 */
    int mkt_airline_bonus;  /* +15 / +25 / +35% */
    int mkt_eco_secs;       /* 環保廣告剩餘秒數 */

    /* 員工與士氣 */
    int staff_salary_pct;   /* 80..140% */
    int staff_morale;       /* 30..100% */

    /* 股市與 AI 競爭對手 */
    int own_share_price;
    int own_share_hist[AM_PRICE_HIST];
    RivalAirline rivals[AM_NUM_RIVALS];

    int64_t last_unix_time;
    char last_notice[96];
} AmWorld;

extern AmWorld g_am;

void am_sim_init(void);
void am_sim_save(void);
int  am_sim_load(void);
void am_sim_new_company(const char *name, const char *iata, int hub_ap_idx, int realism_mode);

/* 推進模擬時間 (dt_real_sec 為真實秒數，會自動乘以倍速或處理離線掛機時間) */
void am_sim_tick(int dt_real_sec, int apply_speed_mult);
int  am_sim_speed_mult(void);

/* 核心操作 */
int  am_effective_reputation(void);
int  am_calc_auto_ticket(int dist_km, int cabin, int realism); /* cabin: 0=Y, 1=J, 2=F, 3=Cargo */
int  am_buy_aircraft(int model_idx, int hub_ap_idx, char *out_msg, int msg_sz);
int  am_sell_aircraft(int fleet_idx, char *out_msg, int msg_sz);
int  am_assign_route(int fleet_idx, int orig_ap, int dest_ap, int stopover_ap, char *out_msg, int msg_sz);
int  am_depart_plane(int fleet_idx, char *out_msg, int msg_sz);
int  am_depart_all(char *out_msg, int msg_sz);

int  am_buy_fuel(int64_t lbs, char *out_msg, int msg_sz);
int  am_buy_co2(int64_t quotas, char *out_msg, int msg_sz);
int  am_upgrade_fuel_tank(char *out_msg, int msg_sz);
int  am_upgrade_co2_tank(char *out_msg, int msg_sz);

int  am_start_marketing(int type, char *out_msg, int msg_sz); /* 0=Eco, 1=Std, 2=Mega */
int  am_repair_plane(int fleet_idx, char *out_msg, int msg_sz);
int  am_acheck_plane(int fleet_idx, char *out_msg, int msg_sz);
int  am_buy_mod(int fleet_idx, int mod_type, char *out_msg, int msg_sz); /* 0=Spd, 1=Fuel, 2=CO2 */
int  am_buy_hub(int ap_idx, char *out_msg, int msg_sz);
int  am_trade_stock(int rival_idx, int delta_shares, char *out_msg, int msg_sz);

const char *am_event_name(int event_id);
const char *am_event_desc(int event_id);

#endif
