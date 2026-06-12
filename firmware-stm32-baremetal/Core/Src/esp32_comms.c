/*
 * esp32_comms.c
 *
 *  Created on: Jun 6, 2026
 *      Author: Adarsha Udupa
 */

/*
 * ESP32_comms.c
 * Structured packet transmission to ESP32 over UART3.
 *
 * Packet format (CSV, terminated with \r\n):
 * EVT,<timestamp_us>,<road_flag>,<shock_lsb>,<dist_cm>,<lat>,<lat_dir>,<lon>,<lon_dir>,<fix>,<sats>\r\n
 *
 * road_flag: 0 = GOOD_ROAD, 1 = NOT_GOOD_ROAD (LLM classifies further)
 * timestamp_us: TIM5->CNT at event detection
 * dist_cm: 0 if ultrasonic not triggered
 */

#include "esp32_comms.h"
#include "uart3.h"
#include "tim5.h"

// ── Internal: number to string helpers (no printf, no stdlib) ────────────────

static void u32_to_str(uint32_t val, char *buf, uint8_t *len)
{
    char tmp[12];
    uint8_t i = 0;
    if (val == 0)
    {
     buf[0] = '0'; buf[1] = '\0'; *len = 1; return;
    }
    while (val > 0)
    {
     tmp[i++] = '0' + (val % 10); val /= 10;
    }
    *len = i;
    for (uint8_t j = 0; j < i; j++) buf[j] = tmp[i - 1 - j];
    buf[i] = '\0';
}

static void i16_to_str(int16_t val, char *buf, uint8_t *len)
{
    uint8_t offset = 0;
    uint32_t abs_val;
    if (val < 0)
    {
     buf[offset++] = '-'; abs_val = (uint32_t)(-(int32_t)val);
    }
    else
       {
    	abs_val = (uint32_t)val;
        }
    uint8_t l = 0;
    u32_to_str(abs_val, buf + offset, &l);
    *len = offset + l;
}

// ── Public API ───────────────────────────────────────────────────────────────

void ESP32_SendPacket(const ESP32_Packet_t *p)
{
    char    buf[12];
    uint8_t len;

    UART3_SendString("EVT,");

    u32_to_str(p->timestamp_us, buf, &len);
    UART3_SendString(buf);
    UART3_SendChar(',');

    // road flag
    UART3_SendChar(p->road_flag ? '1' : '0');
    UART3_SendChar(',');

    // shock
    i16_to_str(p->shock_lsb, buf, &len);
    UART3_SendString(buf);
    UART3_SendChar(',');

    // distance
    u32_to_str(p->dist_cm, buf, &len);
    UART3_SendString(buf);
    UART3_SendChar(',');

    // GPS
    UART3_SendString(p->lat_raw);
    UART3_SendChar(',');
    UART3_SendChar(p->lat_dir);
    UART3_SendChar(',');
    UART3_SendString(p->lon_raw);
    UART3_SendChar(',');
    UART3_SendChar(p->lon_dir);
    UART3_SendChar(',');

    // fix quality + satellites
    UART3_SendChar('0' + p->fix_quality);
    UART3_SendChar(',');
    u32_to_str(p->satellites, buf, &len);
    UART3_SendString(buf);

    UART3_SendString("\r\n");
}
