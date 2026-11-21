#ifndef AM_SIM_H
#define AM_SIM_H

#include <stdint.h>
#include "am_data.h"

#define AM_MAX_FLEET     48
#define AM_MAX_HUBS      12
#define AM_PRICE_HIST    32
#define AM_NUM_RIVALS    10
#define AM_MAX_FLOG      12
#define AM_NUM_ACH       16
#define AM_SAVE_MAGIC    0x414D3433u /* "AM43" */

typedef struct {
    int active;
    int model_idx;
    char reg[12];
    int seats_y, seats_j, seats_f; /* 客機自由配座 (空間權重 Y=1, J=2, F=3); 貨機 seats_y=噸 */

    int has_route;
    int orig_ap;                   /* 基地樞紐機場 */
    int dest_ap;                   /* 目的機場 */
    int stopover_ap;               /* -1=直飛 */
    int dist_km;
    int price_y, price_j, price_f; /* 可手動自由調整或 Autoprice */
    int cost_index;                /* 50..200 (預設 150，影響速度與油耗!) */

    /* 雙向往返航線核心邏輯！ */
    int leg_dir;                   /* 0=去程 (orig->dest), 1=回程 (dest->orig) */
    int auto_return;               /* 1=抵達目的地後自動載客飛回原機場 (雙向往返!) */

    int status;                    /* 0=停機待命, 1=飛行中, 2=A-Check維修中 */
    int flight_elapsed_sec;
    int flight_total_sec;

    int wear_pct;
    int acheck_hrs_left;
    int mod_speed, mod_fuel, mod_co2;

    int total_flights;
    int64_t total_profit;
} FleetPlane;

typedef struct {
    char code[5];
    char name[20];
    int price;
    int hist[AM_PRICE_HIST];
    int owned_shares;
    int avg_cost;
} RivalAirline;

typedef struct {
    char reg[12];
    char route[20];     /* e.g. "TPE->SIN" or "SIN->TPE" */
    int pax_or_tons;
    int is_cargo;
    int64_t net_profit;
} FlightLogEntry;

typedef struct {
    uint32_t magic;
    int initialized;

    char airline_name[28];
    char iata_code[4];
    int realism_mode;
    int speed_mult_idx; /* 0=1x, 1=10x, 2=60x, 3=360x */

    int64_t cash;
    int     pax_points; /* AM4 積分點數 (可換現金/油槽/聲譽) */
    int64_t total_revenue;
    int64_t total_expenses;
    int64_t last_flight_profit;
    int total_flights_done;
    int total_pax_carried;
    int total_cargo_tons;

    int hubs[AM_MAX_HUBS];
    int hub_count;

    FleetPlane fleet[AM_MAX_FLEET];
    int fleet_count;

    int64_t fuel_lbs;
    int64_t fuel_cap;
    int64_t co2_quotas;
    int64_t co2_cap;
    int fuel_price;
    int co2_price;
    int custom_fuel_lock; /* 0=無規律市場波動, >0=玩家自訂鎖定油價 */
    int fuel_hist[AM_PRICE_HIST];
    int co2_hist[AM_PRICE_HIST];
    int market_tick_acc;

    int event_id;
    int event_secs_left;

    int base_reputation;
    int mkt_airline_secs;
    int mkt_airline_bonus;
    int mkt_eco_secs;

    int staff_salary_pct;
    int staff_morale;

    int own_share_price;
    int own_share_hist[AM_PRICE_HIST];
    RivalAirline rivals[AM_NUM_RIVALS];

    int64_t loan_balance;
    FlightLogEntry flight_log[AM_MAX_FLOG];
    int flight_log_count;
    uint8_t ach_unlocked[AM_NUM_ACH];
    int bought_low_fuel;

    int64_t last_unix_time;
    char last_notice[96];
} AmWorld;

extern AmWorld g_am;

void am_sim_init(void);
void am_sim_save(void);
int  am_sim_load(void);
void am_sim_new_company(const char *name, const char *iata, int hub_ap_idx,
                        int realism_mode, int64_t start_cash, int starter_model_idx);

int  am_sim_tick(int dt_real_sec, int apply_speed_mult);
int  am_sim_speed_mult(void);

int  am_effective_reputation(void);
int  am_calc_auto_ticket(int dist_km, int cabin, int realism);
int  am_calc_route_fee(int dist_km);
void am_calc_leg_preview(int model_idx, int dist_km, int cost_index, int mod_spd, int mod_fuel,
                         int *out_spd_kmh, int *out_secs, int64_t *out_fuel_lbs);

int  am_buy_aircraft_custom(int model_idx, int hub_ap_idx, int sy, int sj, int sf, char *out_msg, int msg_sz);
int  am_sell_aircraft(int fleet_idx, char *out_msg, int msg_sz);
int  am_assign_route_full(int fleet_idx, int orig_ap, int dest_ap, int stopover_ap,
                          int py, int pj, int pf, int cost_index, int auto_ret, char *out_msg, int msg_sz);
int  am_depart_plane(int fleet_idx, char *out_msg, int msg_sz);
int  am_depart_all(char *out_msg, int msg_sz);

int  am_buy_fuel(int64_t lbs, char *out_msg, int msg_sz);
int  am_buy_co2(int64_t quotas, char *out_msg, int msg_sz);
int  am_upgrade_fuel_tank(char *out_msg, int msg_sz);
int  am_upgrade_co2_tank(char *out_msg, int msg_sz);
int  am_start_marketing(int type, char *out_msg, int msg_sz);

int  am_repair_plane(int fleet_idx, char *out_msg, int msg_sz);
int  am_acheck_plane(int fleet_idx, char *out_msg, int msg_sz);
int  am_buy_mod(int fleet_idx, int mod_type, char *out_msg, int msg_sz);
int  am_buy_hub(int ap_idx, char *out_msg, int msg_sz);
int  am_trade_stock(int rival_idx, int delta_shares, char *out_msg, int msg_sz);
int  am_take_loan(int64_t amount, char *out_msg, int msg_sz);
int  am_repay_loan(int64_t amount, char *out_msg, int msg_sz);
int  am_spend_points(int type, char *out_msg, int msg_sz); /* 0=換$2M, 1=油槽+2M, 2=聲譽+5% */

int         am_airline_level(void);
const char *am_airline_rank_name(int lv);
const char *am_ach_name(int id);
const char *am_ach_desc(int id);
int64_t     am_ach_reward(int id);
int         am_ach_unlocked_count(void);
const char *am_event_name(int event_id);
const char *am_event_desc(int event_id);

#endif
