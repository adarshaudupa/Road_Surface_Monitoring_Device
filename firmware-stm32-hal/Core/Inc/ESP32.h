#ifndef ESP32_H_
#define ESP32_H_

#include "main.h"

/* =========================================
   INITIALIZATION
   =========================================*/

void ESP32_Init(UART_HandleTypeDef *huart);

/* =========================================
   EVENT PACKETS
   =========================================*/

void ESP32_Send_RoughRoad(int16_t shock,
                          char *lat,
                          char *lon,
                          uint32_t timestamp);

void ESP32_Send_Pothole(int16_t shock,
                        uint32_t distance,
                        char *lat,
                        char *lon,
                        uint32_t timestamp);

#endif
