#ifndef AM_SIM_H
#define AM_SIM_H

#include <stdint.h>
#include "am_data.h"

#define AM_MAX_FLEET      1024  /* 無限制級大型機隊 (最多 1,024 架!) */
#define AM_MAX_HUBS       512   /* 取消樞紐上限 (可買下全球所有機場樞紐!) */
#define AM_PRICE_HIST     32
#define AM_NUM_RIVALS     10
#define AM_MAX_FLOG       12
#define AM_NUM_ACH        16
#define AM_NUM_EVENTS     16    /* 16 種豐富隨機事件 */
#define AM_MAX_SAVE_SLOTS 8     /* 8 個獨立自訂名稱存檔槽 */
#define AM_SAVE_MAGIC     0x414D3436u /* "AM46" */
#define AM_IDX_MAGIC      0x414D4936u /* "AMI6" */

typedef struct {
    int active;
    int model_idx;
    char reg[12];
    int seats_y, seats_j, seats_f;

    int has_route;
    int orig_ap;
    int dest_ap;
    int stopover_ap;
    int dist_km;
    int price_y, price_j, price_f;
    int cost_index;

    int leg_dir;
    int auto_return;

    int status; /* 0=待命, 1=飛行中, 2=A-Check維修中, 3=停飛封存 */
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
    char route[20];
    int pax_or_tons;
    int is_cargo;
    int64_t net_profit;
} FlightLogEntry;

typedef struct {
    int used;
    char save_name[28];
    char airline_name[28];
    int64_t cash;
    int fleet_count;
    int hub_count;
    int level;
} SaveSlotMeta;

typedef struct {
    uint32_t magic;
    SaveSlotMeta slots[AM_MAX_SAVE_SLOTS];
} SaveSlotIndex;

typedef struct {
    uint32_t magic;
    int initialized;
    int active_slot;
    char save_name[28];

    char airline_name[28];
    char iata_code[4];
    int realism_mode;
    int speed_mult_idx; /* 0=1x, 1=10x, 2=60x, 3=360x, 4=1000x */

    int64_t cash;
    int     pax_points;
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
    int custom_fuel_lock;
    int fuel_hist[AM_PRICE_HIST];
    int co2_hist[AM_PRICE_HIST];

    /* 每 5 分鐘 (300 真實秒) 刷新市場、隨機事件與發放員工工資! */
    int market_real_sec_acc;
    int payroll_real_sec_acc;
    int64_t last_payroll_cost;

    int event_id;
    int event_secs_left;

    int base_reputation;
    int mkt_airline_secs;
    int mkt_airline_bonus;
    int mkt_eco_secs;

    int staff_salary_pct;
    int staff_morale;
    int staff_pilots;
    int staff_crew;
    int staff_engineers;
    int staff_techs;
    int training_points;
    int train_mgmt_lv;
    int train_fuel_lv;
    int train_maint_lv;

    int own_share_price;
    int own_share_hist[AM_PRICE_HIST];
    RivalAirline rivals[AM_NUM_RIVALS];

    int64_t loan_balance;
    FlightLogEntry flight_log[AM_MAX_FLOG];
    int flight_log_count;
    uint8_t ach_unlocked[AM_NUM_ACH];
    int bought_low_fuel;

    int catering_level;
    int alliance_id;

    int charter_orig, charter_dest, charter_type;
    int64_t charter_reward;
    int charter_pts, charter_done_cnt;

    /* 外掛 / MOD 開關 */
    int cheat_god_fuel;
    int cheat_no_wear;
    int cheat_no_staff_req;
    int64_t last_daily_bonus_ts;

    int64_t last_unix_time;
    char last_notice[112];
} AmWorld;

extern AmWorld g_am;
extern SaveSlotIndex g_slots;

void am_sim_init(void);
void am_slots_refresh(void);
int  am_sim_save_slot(int slot, const char *save_name);
int  am_sim_load_slot(int slot);
void am_sim_delete_slot(int slot);
void am_sim_save(void);

void am_sim_new_company(int slot, const char *save_name, const char *name, const char *iata,
                        int hub_ap_idx, int realism_mode, int64_t start_cash, int starter_model_idx);

int  am_sim_tick(int dt_real_sec, int apply_speed_mult);
int  am_sim_speed_mult(void);

int  am_effective_reputation(void);
int  am_calc_auto_ticket(int dist_km, int cabin, int realism);
int  am_calc_route_fee(int dist_km);
void am_calc_leg_preview(int model_idx, int dist_km, int cost_index, int mod_spd, int mod_fuel,
                         int *out_spd_kmh, int *out_secs, int64_t *out_fuel_lbs);
int64_t am_estimate_leg_profit(int model_idx, int dist_km, int sy, int sj, int sf,
                               int py, int pj, int pf, int cost_index, int mod_fuel,
                               int64_t *out_rev, int64_t *out_fuel_cost);

/* 機組人員需求與工資系統 */
void am_aircraft_crew_req(int model_idx, int *pilots, int *crew, int *eng, int *tech);
void am_fleet_crew_used(int *pilots, int *crew, int *eng, int *tech);
int64_t am_calc_payroll_cost(void);

int  am_buy_aircraft_bulk(int model_idx, int hub_ap_idx, int sy, int sj, int sf, int qty, char *out_msg, int msg_sz);
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
int  am_repair_all(char *out_msg, int msg_sz);
int  am_acheck_all(char *out_msg, int msg_sz);
int  am_buy_mod(int fleet_idx, int mod_type, char *out_msg, int msg_sz);
int  am_ground_plane(int fleet_idx, char *out_msg, int msg_sz);

int  am_buy_hub(int ap_idx, char *out_msg, int msg_sz);
int  am_trade_stock(int rival_idx, int delta_shares, char *out_msg, int msg_sz);
int  am_take_loan(int64_t amount, char *out_msg, int msg_sz);
int  am_repay_loan(int64_t amount, char *out_msg, int msg_sz);
int  am_spend_points(int type, char *out_msg, int msg_sz);
int  am_set_catering(int level, char *out_msg, int msg_sz);
int  am_join_alliance(int alliance_id, char *out_msg, int msg_sz);
void am_roll_new_charter(void);
int  am_run_charter_with_plane(int fleet_idx, char *out_msg, int msg_sz);
int  am_hire_staff(int dept, int count, char *out_msg, int msg_sz);
int  am_upgrade_training(int track, char *out_msg, int msg_sz);
int  am_claim_daily_bonus(char *out_msg, int msg_sz);

/* 強化版 MOD MENU 外掛功能 */
void am_cheat_set_cash(int64_t exact_cash, char *out_msg, int msg_sz);
void am_cheat_add_cash(int64_t amount, char *out_msg, int msg_sz);
void am_cheat_add_points(int pts, char *out_msg, int msg_sz);
void am_cheat_refill_all(char *out_msg, int msg_sz);
int  am_cheat_instant_finish_flights(char *out_msg, int msg_sz);
void am_cheat_max_reputation(char *out_msg, int msg_sz);
void am_cheat_unlock_all_achs(char *out_msg, int msg_sz);
void am_cheat_unlock_all_hubs(char *out_msg, int msg_sz);
void am_cheat_max_staff_and_training(char *out_msg, int msg_sz);
void am_cheat_force_market_refresh(char *out_msg, int msg_sz);
int  am_cheat_open_routes_country(int model_idx, const char *country_code, char *out_msg, int msg_sz);
int  am_cheat_open_routes_world(int model_idx, char *out_msg, int msg_sz);

int         am_airline_level(void);
const char *am_airline_rank_name(int lv);
const char *am_ach_name(int id);
const char *am_ach_desc(int id);
int64_t     am_ach_reward(int id);
int         am_ach_unlocked_count(void);
const char *am_event_name(int event_id);
const char *am_event_desc(int event_id);
const char *am_alliance_name(int id);
const char *am_charter_title(int type);

#endif
