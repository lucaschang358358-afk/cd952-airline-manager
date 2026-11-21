#ifndef AM_DATA_H
#define AM_DATA_H

#include <stdint.h>

#define AM_MAX_AIRPORTS  4200
#define AM_MAX_AIRCRAFT  160
#define AM_MAX_COUNTRIES 160

typedef struct {
    char iata[4];
    char name[28];
    char country[4];
    int16_t lat10;
    int16_t lon10;
    uint16_t runway_ft;
    uint8_t market_pct;
    uint16_t dem_y;
    uint16_t dem_j;
    uint16_t dem_f;
} AirportInfo;

typedef struct {
    uint8_t mfg;         /* 0=Airbus, 1=Boeing, 2=Embraer, 3=Regional, 4=Special, 5=Cargo */
    const char *model;
    int64_t price;
    uint16_t range_km;
    uint16_t speed_kmh;
    uint16_t cap;        /* pax seats, or metric tons if is_cargo==1 */
    uint8_t fuel_lbs_km;
    uint8_t co2_kg;
    uint16_t runway_ft;
    uint16_t acheck_hrs;
    uint8_t is_cargo;
    uint8_t silhouette;  /* 0=螺旋槳, 1=支線噴射, 2=窄體客機, 3=寬體雙發, 4=四發巨無霸, 5=超音速, 6=重型貨機 */
    const char *desc;    /* 完整中文機型介紹與營運定位 */
} AircraftModel;

void am_data_init(const char *extra_csv_path);
int  am_airport_count(void);
const AirportInfo *am_airport_get(int idx);
int  am_airport_find_iata(const char *iata);

/* 國家 -> 城市/機場 兩段式選擇器 API */
int         am_country_count(void);
const char *am_country_code(int c_idx);
const char *am_country_name_zh(const char *code);
int         am_airports_in_country(const char *code, int *out_idxs, int max_n);

int  am_aircraft_count(void);
const AircraftModel *am_aircraft_get(int idx);
const char *am_mfg_name(int mfg);
const char *am_aircraft_engine_name(int model_idx);

int  am_calc_distance_km(int ap1_idx, int ap2_idx);

#endif
