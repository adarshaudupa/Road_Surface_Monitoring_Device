/*
 * lsm6ds3.h
 *
 *  Created on: May 14, 2026
 *      Author: Adarsha Udupa
 */

#ifndef INC_LSM6DS3_H_
#define INC_LSM6DS3_H_

#include <stdint.h>



void LSM6DS3_Init(void);
void LSM6DS3_WHOAMI(void);
void imu_read_accel_xyz(int16_t *ax, int16_t *ay, int16_t *az);
int16_t imu_read_accel_z(void);
void imu_update_gravity_estimate(int16_t ax_raw, int16_t ay_raw, int16_t az_raw);
void imu_settle_gravity(uint16_t samples, uint16_t delay_ms);
int16_t imu_get_vertical_dynamic_lsb(int16_t ax_raw, int16_t ay_raw, int16_t az_raw);
int16_t imu_vertical_shock(void);

#endif
