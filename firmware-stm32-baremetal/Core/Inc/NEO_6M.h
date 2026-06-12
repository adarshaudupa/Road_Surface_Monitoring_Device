#ifndef NEO_6M_H
#define NEO_6M_H

#include <stdint.h>

#define GPS_RX_BUFFER_SIZE   256
#define GPS_LINE_BUFFER_SIZE  128

typedef struct
{
    char utc_time[16];
    char latitude_raw[16];
    char latitude_direction;
    char longitude_raw[16];
    char longitude_direction;
    uint8_t fix_quality;
    uint8_t satellites;
    char altitude_raw[16];
} GPS_Data_t;

void GPS_Init(void);
void GPS_Process(void);
void GPS_GetDataCopy(GPS_Data_t *dest);
uint8_t GPS_IsDataUpdated(void);
void GPS_ClearDataUpdatedFlag(void);
uint32_t GPS_GetOverflowCount(void);
uint32_t GPS_GetRxByteCount(void);

#endif
