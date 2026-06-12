/*
 * esp32_comms.h
 *
 *  Created on: Jun 6, 2026
 *      Author: Adarsha Udupa
 */

#ifndef INC_ESP32_COMMS_H_
#define INC_ESP32_COMMS_H_

#include <stdint.h>

typedef struct {
    uint32_t timestamp_us;
    uint8_t  road_flag;      // 0 = GOOD, 1 = NOT_GOOD
    int16_t  shock_lsb;
    uint16_t dist_cm;
    char     lat_raw[12];
    char     lat_dir;
    char     lon_raw[12];
    char     lon_dir;
    uint8_t  fix_quality;
    uint8_t  satellites;
} ESP32_Packet_t;

void ESP32_SendPacket(const ESP32_Packet_t *p);

#endif /* INC_ESP32_COMMS_H_ */
