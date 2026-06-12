#include "ESP32.h"

#include <stdio.h>
#include <string.h>

/* =========================================
   PRIVATE VARIABLES
   =========================================*/

static UART_HandleTypeDef *esp32_uart;

static char esp32_msg[128];

/* =========================================
   INITIALIZATION
   =========================================*/

void ESP32_Init(UART_HandleTypeDef *huart)
{
    esp32_uart = huart;
}

/* =========================================
   ROUGH ROAD PACKET

   FORMAT:
   R,shock,time
   =========================================*/

void ESP32_Send_RoughRoad(int16_t shock,
                          char *lat,
                          char *lon,
                          uint32_t timestamp)
{
	sprintf(esp32_msg,
	        "R,%d,%s,%s,%lu\n",
	        shock,
	        lat,
	        lon,
	        timestamp);

    HAL_UART_Transmit(esp32_uart,
                      (uint8_t*)esp32_msg,
                      strlen(esp32_msg),
                      100);
}

/* =========================================
   POTHOLE PACKET

   FORMAT:
   P,shock,distance,lat,lon,time
   =========================================*/

void ESP32_Send_Pothole(int16_t shock,
                        uint32_t distance,
                        char *lat,
                        char *lon,
                        uint32_t timestamp)
{
    sprintf(esp32_msg,
            "P,%d,%lu,%s,%s,%lu\n",
            shock,
            distance,
            lat,
            lon,
            timestamp);

    HAL_UART_Transmit(esp32_uart,
                      (uint8_t*)esp32_msg,
                      strlen(esp32_msg),
                      100);
}
