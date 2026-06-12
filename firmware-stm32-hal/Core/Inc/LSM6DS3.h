/* LSM6DS3.h */

#ifndef INC_LSM6DS3_H_
#define INC_LSM6DS3_H_

#include "main.h"
#include <stdint.h>

/* API */

void LSM6DS3_Init(void);

void LSM6DS3_WHOAMI(void);

void imu_read_accel_xyz(int16_t *ax,
                        int16_t *ay,
                        int16_t *az);

int16_t imu_vertical_shock(void);

#endif
