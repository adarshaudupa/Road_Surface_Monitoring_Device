/* ========================= Neo_6m.h ========================= */

#ifndef INC_NEO_6M_H_
#define INC_NEO_6M_H_

#include "main.h"

#include <stdint.h>
#include <stdbool.h>

#define GPS_RX_BUFFER_SIZE      512
#define GPS_LINE_BUFFER_SIZE    128

typedef struct
{
    char utc_time[16];

    char latitude_raw[16];

    char longitude_raw[16];

    char altitude_raw[16];

    char latitude_direction;

    char longitude_direction;

    uint8_t fix_quality;

    uint8_t satellites;

} GPS_Data_t;

void GPS_Init(UART_HandleTypeDef *huart);

void GPS_Process(void);

void GPS_UART_RxCpltCallback(UART_HandleTypeDef *huart);

void GPS_GetDataCopy(GPS_Data_t *dest);

uint8_t GPS_IsDataUpdated(void);

void GPS_ClearDataUpdatedFlag(void);

#endif
