#ifndef AM_DATA_H
#define AM_DATA_H

#include <stdint.h>

#define AM_MAX_AIRPORTS 4200
#define AM_MAX_AIRCRAFT 160

typedef struct {
    char iata[4];
    char name[28];
    char country[4];
    int16_t lat10;       /* latitude * 10 */
    int16_t lon10;       /* longitude * 10 */
    uint16_t runway_ft;
    uint8_t market_pct;
    uint16_t dem_y;
    uint16_t dem_j;
    uint16_t dem_f;
} AirportInfo;

typedef struct {
    uint8_t mfg;         /* 0=Airbus, 1=Boeing, 2=Embraer, 3=Bombardier/ATR, 4=Comac/MD/Spec, 5=Cargo */
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
} AircraftModel;

void am_data_init(const char *extra_csv_path);
int  am_airport_count(void);
const AirportInfo *am_airport_get(int idx);
int  am_airport_find_iata(const char *iata);

int  am_aircraft_count(void);
const AircraftModel *am_aircraft_get(int idx);
const char *am_mfg_name(int mfg);

/* 球面大圓距離 (km, 純整數查表/近似，免 -lm 連結!) */
int  am_calc_distance_km(int ap1_idx, int ap2_idx);

#endif
