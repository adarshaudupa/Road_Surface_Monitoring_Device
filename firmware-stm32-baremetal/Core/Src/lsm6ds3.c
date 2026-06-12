/*
 * lsm6ds3.c
 *
 *  Created on: May 14, 2026
 *      Author: Adarsha Udupa
 */

#include "lsm6ds3.h"
#include "i2c1.h"
#include "uart2.h"
#include "stm32f4xx.h"
#include "tim5.h"

#include <string.h>
#include <math.h>
#include <stdint.h>

// -----------------------------------------------------------------------------
// LSM6DS3 constants
// -----------------------------------------------------------------------------
#define LSM6DS3_ADDR        0x6A

#define WHO_AM_I_REG        0x0F
#define CTRL1_XL            0x10
#define CTRL2_G             0x11

#define OUTX_L_XL           0x28   // accel X low, then XH, YL, YH, ZL, ZH

// -----------------------------------------------------------------------------
// Filtering parameters
// -----------------------------------------------------------------------------

// Gravity estimate LPF (slow). At 100Hz, alpha_g=0.02 -> ~0.5s-1s time constant.
// Tune: smaller = slower baseline tracking (better pothole isolation).
static float alpha_g = 0.02f;

// Optional smoothing of dynamic vertical shock (faster)
static float alpha_dyn = 0.25f;

// Gravity estimate state (baseline vector)
static float g_x = 0.0f;
static float g_y = 0.0f;
static float g_z = 0.0f;

// Smoothed vertical dynamic accel (optional)
static float a_vert_filt = 0.0f;

// Simple helper: sign-safe int32 abs
static inline int32_t iabs32(int32_t x) { return (x < 0) ? -x : x; }

// -----------------------------------------------------------------------------
// Init / identification
// -----------------------------------------------------------------------------
void LSM6DS3_Init(void)
{
    I2C1_Init(); // Initialize I2C peripheral for LSM6DS3 communication

    // Accel: 104Hz ODR, ±2g full scale
    I2C1_WriteByte(LSM6DS3_ADDR, CTRL1_XL, 0x40);

    // Gyro: 104Hz ODR, 245dps full scale
    I2C1_WriteByte(LSM6DS3_ADDR, CTRL2_G, 0x40);

    // Reset filter state
    g_x = g_y = g_z = 0.0f;
    a_vert_filt = 0.0f;
}

void LSM6DS3_WHOAMI(void)
{
    uint8_t id = I2C1_ReadRegister(LSM6DS3_ADDR, WHO_AM_I_REG);
    uart_print_uint("LSM6DS3 WHO_AM_I = ", id); // Should be 106 for LSM6DS3
}

// -----------------------------------------------------------------------------
// Raw reads
// -----------------------------------------------------------------------------

// Read 3-axis accel using 6 consecutive registers
void imu_read_accel_xyz(int16_t *ax, int16_t *ay, int16_t *az)
{
    uint8_t buffer[6];

    // Read 6 consecutive bytes starting from OUTX_L_XL (0x28)
    I2C1_ReadMulti(LSM6DS3_ADDR, OUTX_L_XL, buffer, 6);

    // Reconstruct the 16-bit values from the low and high bytes
    *ax = (int16_t)((buffer[1] << 8) | buffer[0]);
    *ay = (int16_t)((buffer[3] << 8) | buffer[2]);
    *az = (int16_t)((buffer[5] << 8) | buffer[4]);
}

// Backwards-compatible helper (if other code calls it)
int16_t imu_read_accel_z(void)
{
    int16_t ax, ay, az;
    imu_read_accel_xyz(&ax, &ay, &az);
    return az;
}

// -----------------------------------------------------------------------------
// Baseline (gravity) tracking
// -----------------------------------------------------------------------------

// Call this at your sample rate (e.g., 100Hz).
// It updates the gravity estimate vector g_x/g_y/g_z.
void imu_update_gravity_estimate(int16_t ax_raw, int16_t ay_raw, int16_t az_raw)
{
    // First-run initialization: set gravity to first sample to avoid long startup transient
    if (g_x == 0.0f && g_y == 0.0f && g_z == 0.0f)
    {
        g_x = (float)ax_raw;
        g_y = (float)ay_raw;
        g_z = (float)az_raw;
        return;
    }

    g_x = g_x + alpha_g * ((float)ax_raw - g_x);
    g_y = g_y + alpha_g * ((float)ay_raw - g_y);
    g_z = g_z + alpha_g * ((float)az_raw - g_z);
}

// Optional: "settle" for N samples at boot so gravity estimate converges
void imu_settle_gravity(uint16_t samples, uint16_t delay_ms)
{
    UART2_SendString("IMU settling gravity...\r\n");

    for (uint16_t i = 0; i < samples; i++)
    {
        int16_t ax, ay, az;
        imu_read_accel_xyz(&ax, &ay, &az);
        imu_update_gravity_estimate(ax, ay, az);
        uint32_t t = TIM5_GetMicros();
        while ((uint32_t)(TIM5_GetMicros() - t) < (uint32_t)(delay_ms * 1000));
    }

    uart_print_int("g_x = ", (int32_t)g_x);
    uart_print_int("g_y = ", (int32_t)g_y);
    uart_print_int("g_z = ", (int32_t)g_z);
}

// -----------------------------------------------------------------------------
// Dynamic acceleration + vertical shock feature
// -----------------------------------------------------------------------------

// Returns vertical dynamic acceleration (LSB) projected along gravity direction.
// Positive/negative depends on your axis orientation; use abs() for thresholding.
int16_t imu_get_vertical_dynamic_lsb(int16_t ax_raw, int16_t ay_raw, int16_t az_raw)
{
    // Dynamic accel = raw - gravity_est
    float dx = (float)ax_raw - g_x;
    float dy = (float)ay_raw - g_y;
    float dz = (float)az_raw - g_z;

    // Gravity magnitude
    float gmag = sqrtf(g_x*g_x + g_y*g_y + g_z*g_z);
    if (gmag < 1.0f)
    {
        // avoid divide-by-zero if not initialized
        return 0;
    }

    // Unit gravity vector
    float ux = g_x / gmag;
    float uy = g_y / gmag;
    float uz = g_z / gmag;

    // Project dynamic accel along gravity direction
    float a_vert = dx*ux + dy*uy + dz*uz;

    // Optional smoothing
    a_vert_filt = a_vert_filt + alpha_dyn * (a_vert - a_vert_filt);

    return (int16_t)a_vert_filt;
}

// Convenience function: read accel, update gravity, return vertical shock
int16_t imu_vertical_shock(void)
{
    int16_t ax, ay, az;
    imu_read_accel_xyz(&ax, &ay, &az);

    imu_update_gravity_estimate(ax, ay, az);

    return imu_get_vertical_dynamic_lsb(ax, ay, az);
}
