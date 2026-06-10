/* LSM6DS3.c */

#include "LSM6DS3.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

/* =========================
   LSM6DS3 REGISTERS
   ========================= */

#define LSM6DS3_ADDR          (0x6A << 1)

#define WHO_AM_I_REG          0x0F

#define CTRL1_XL              0x10
#define CTRL2_G               0x11

#define OUTX_L_XL             0x28

extern I2C_HandleTypeDef hi2c1;
extern UART_HandleTypeDef huart2;

/* =========================
   FILTER PARAMETERS
   ========================= */

static float alpha_g   = 0.02f;

static float g_x = 0.0f;
static float g_y = 0.0f;
static float g_z = 0.0f;

/* =========================
   HELPERS
   ========================= */

static void LSM6DS3_WriteReg(uint8_t reg,
                             uint8_t data)
{
    HAL_I2C_Mem_Write(&hi2c1,
                      LSM6DS3_ADDR,
                      reg,
                      I2C_MEMADD_SIZE_8BIT,
                      &data,
                      1,
                      100);
}

static uint8_t LSM6DS3_ReadReg(uint8_t reg)
{
    uint8_t data;

    HAL_I2C_Mem_Read(&hi2c1,
                     LSM6DS3_ADDR,
                     reg,
                     I2C_MEMADD_SIZE_8BIT,
                     &data,
                     1,
                     100);

    return data;
}

/* =========================
   INIT
   ========================= */

void LSM6DS3_Init(void)
{
     //Accel:
     //104 Hz
     //±2g

    LSM6DS3_WriteReg(CTRL1_XL,
                     0x40);

     //Gyro:
       //104 Hz
       //245 dps

    LSM6DS3_WriteReg(CTRL2_G,
                     0x40);

    g_x = 0.0f;
    g_y = 0.0f;
    g_z = 0.0f;
}

void LSM6DS3_WHOAMI(void)
{
    uint8_t id;

    char msg[64];

    id = LSM6DS3_ReadReg(WHO_AM_I_REG);

    sprintf(msg,
            "[LSM6DS3] WHO_AM_I: %u\r\n",
            id);

    HAL_UART_Transmit(&huart2,
                      (uint8_t*)msg,
                      strlen(msg),
                      100);
}

/* =========================
   RAW ACCEL READ
   =========================*/

void imu_read_accel_xyz(int16_t *ax,
                        int16_t *ay,
                        int16_t *az)
{
    /* Initialize to 0 to prevent reading stack garbage */
    uint8_t data[6] = {0};

    /* Capture status and drop timeout to 5ms to preserve the 20ms main loop */
    HAL_StatusTypeDef status = HAL_I2C_Mem_Read(&hi2c1,
                                                LSM6DS3_ADDR,
                                                OUTX_L_XL,
                                                I2C_MEMADD_SIZE_8BIT,
                                                data,
                                                6,
                                                5);

    /* Handle I2C lockups cleanly */
    if(status != HAL_OK)
    {
        // The bus is stuck! De-initialize and Re-initialize to unstick it.
        HAL_I2C_DeInit(&hi2c1);
        HAL_I2C_Init(&hi2c1);
        return; // Exit before calculating garbage
    }

    *ax =
        (int16_t)((data[1] << 8) | data[0]);

    *ay =
        (int16_t)((data[3] << 8) | data[2]);

    *az =
        (int16_t)((data[5] << 8) | data[4]);
}

/* =========================
   GRAVITY ESTIMATION
   =========================*/

static void imu_update_gravity(int16_t ax,
                               int16_t ay,
                               int16_t az)
{
    if(g_x == 0.0f &&
       g_y == 0.0f &&
       g_z == 0.0f)
    {
        g_x = ax;
        g_y = ay;
        g_z = az;

        return;
    }

    g_x =
        g_x + alpha_g * ((float)ax - g_x);

    g_y =
        g_y + alpha_g * ((float)ay - g_y);

    g_z =
        g_z + alpha_g * ((float)az - g_z);
}

/* =========================
   VERTICAL SHOCK
   =========================*/

int16_t imu_vertical_shock(void)
{
    /* Initialize to 0 so early-returns from failed I2C reads don't use stack garbage */
    int16_t ax = 0;
    int16_t ay = 0;
    int16_t az = 0;

    float dx;
    float dy;
    float dz;

    float gmag;

    float ux;
    float uy;
    float uz;

    float vertical;

    imu_read_accel_xyz(&ax,
                       &ay,
                       &az);

    imu_update_gravity(ax,
                       ay,
                       az);

    dx = ax - g_x;
    dy = ay - g_y;
    dz = az - g_z;

    gmag =
        sqrtf(g_x*g_x +
              g_y*g_y +
              g_z*g_z);

    if(gmag < 1.0f)
    {
        return 0;
    }

    ux = g_x / gmag;
    uy = g_y / gmag;
    uz = g_z / gmag;

    vertical =
        dx*ux +
        dy*uy +
        dz*uz;

    return (int16_t)vertical;
}
