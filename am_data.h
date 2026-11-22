#ifndef AM_DATA_H
#define AM_DATA_H

#include <stdint.h>

#define AM_MAX_AIRPORTS  5000
#define AM_MAX_AIRCRAFT  160
#define AM_MAX_COUNTRIES 220
#define AM_NUM_CONTINENTS 7

typedef struct {
    char iata[4];
    char icao[5];
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
    uint8_t mfg;
    const char *model;
    int64_t price;
    uint16_t range_km;
    uint16_t speed_kmh;
    uint16_t cap;
    uint8_t fuel_lbs_km;
    uint8_t co2_kg;
    uint16_t runway_ft;
    uint16_t acheck_hrs;
    uint8_t is_cargo;
    uint8_t silhouette;
    const char *desc;
} AircraftModel;

void am_data_init(const char *extra_csv_path);
int  am_airport_count(void);
const AirportInfo *am_airport_get(int idx);
int  am_airport_find_iata(const char *iata);

/* 七大洲 -> 全世界所有國家(200+) -> 該國所有機場 3 段式選擇器 */
const char *am_continent_name(int cont_id);
int         am_countries_in_continent(int cont_id, int *out_c_idxs, int max_n);
int         am_country_count(void);
const char *am_country_code(int c_idx);
const char *am_country_name_zh(const char *code);
int         am_airports_in_country(const char *code, int *out_idxs, int max_n);

int  am_aircraft_count(void);
const AircraftModel *am_aircraft_get(int idx);
const char *am_mfg_name(int mfg);
const char *am_aircraft_engine_name(int model_idx);

int  am_calc_distance_km(int ap1_idx, int ap2_idx);
void am_calc_route_demand(int ap1_idx, int ap2_idx, int *out_y, int *out_j, int *out_f, int *out_cargo);

#endif
