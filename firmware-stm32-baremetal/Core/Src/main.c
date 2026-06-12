#include "stm32f4xx.h"
#include "uart2.h"
#include "uart3.h"
#include "NEO_6M.h"
#include "tim5.h"
#include "i2c1.h"
#include "lsm6ds3.h"
#include "hc-sr04.h"
#include "esp32_comms.h"
#include <string.h>

#define SHOCK_DT_US         10000     // 100Hz sample
#define SHOCK_THRESH        3000      // LSM6DS3 shock threshold for pothole
#define EVENT_COOLDOWN_US  300000     // 300ms

static GPS_Data_t gps_data;

// Pothole event state
static uint32_t last_shock_us   = 0;
static uint32_t last_event_us   = 0;
static uint8_t  send_pending    = 0;  // 1 if need to send depth after US read

// Holds last event for ESP32 packet
static ESP32_Packet_t pkt;

// ──────────────────────────────────────────────────────────────────────────────
int main(void)
{
    // --- Clocks ---
    RCC->CR |= (1 << 0); while (!(RCC->CR & (1 << 1)));     // HSI on
    RCC->CFGR &= ~(3 << 0);  while ((RCC->CFGR & (3 << 2))); // SYSCLK = HSI

    // --- Peripheral init ---
    UART2_Init();
    UART3_Init();
    GPS_Init();           // wires up UART1/ISR/buffer
    TIM5_Init();          // for micros/hc-sr04
    I2C1_Init();
    HCSR04_Init();
    LSM6DS3_Init();

    UART2_SendString("==== POTHOLE DETECTION SYSTEM ====\r\n");
    LSM6DS3_WHOAMI();
    imu_settle_gravity(200, 5); // 1s at 5ms/sample

    // --- Main loop ---
    UART2_SendString("Main Loop Start\r\n");
    while (1)
        {
            uint32_t now = TIM5_GetMicros();
            GPS_Process();

            // --- 1. IMU Polling (100Hz) ---
            if ((uint32_t)(now - last_shock_us) >= SHOCK_DT_US)
            {
                last_shock_us = now;
                int16_t shock = imu_vertical_shock();
                int16_t shock_abs = (shock < 0) ? -shock : shock;

                // Make this threshold lower (e.g., 500) if you don't see events
                uint8_t road_flag = (shock_abs > 500) ? 1 : 0;

                if (road_flag == 1 && (uint32_t)(now - last_event_us) >= EVENT_COOLDOWN_US)
                {
                    last_event_us = now;
                    send_pending = 1;

                    GPS_GetDataCopy(&gps_data);
                    memset(&pkt, 0, sizeof(pkt));
                    pkt.timestamp_us = now;
                    pkt.road_flag    = 1;
                    pkt.shock_lsb    = shock;
                    pkt.dist_cm      = 0;

                    // Copy GPS data
                    strncpy(pkt.lat_raw, gps_data.latitude_raw, sizeof(pkt.lat_raw) - 1);
                    pkt.lat_dir      = gps_data.latitude_direction;
                    strncpy(pkt.lon_raw, gps_data.longitude_raw, sizeof(pkt.lon_raw) - 1);
                    pkt.lon_dir      = gps_data.longitude_direction;
                    pkt.fix_quality  = gps_data.fix_quality;
                    pkt.satellites   = gps_data.satellites;

                    UART2_SendString("*** POTHOLE EVENT DETECTED ***\r\n");
                    HCSR04_Start();
                }
            }

            // --- 2. Asynchronous Distance Check ---
            // This MUST be here to finish the cycle and send the packet
            if (send_pending && HCSR04_IsDone())
            {
                pkt.dist_cm = HCSR04_GetLastCm();
                ESP32_SendPacket(&pkt);

                uart_print_uint("EVENT SENT, DISTANCE_CM = ", pkt.dist_cm);
                send_pending = 0;
            }
        } // End while
    } // End main
