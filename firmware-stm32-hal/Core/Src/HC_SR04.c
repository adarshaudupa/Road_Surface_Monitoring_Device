/* HC_SR04.c */

#include "HC_SR04.h"

/* TIM5 Handle */
extern TIM_HandleTypeDef htim5;

/* Driver State Variables */
volatile uint8_t  hcsr04_busy    = 0;
volatile uint8_t  hcsr04_done    = 0;
volatile uint16_t hcsr04_last_cm = 0;

/* Internal Variables */
static volatile uint32_t rising_edge  = 0;
static volatile uint32_t falling_edge = 0;
static volatile uint32_t pulse_width  = 0;

static volatile uint8_t capture_state = 0;

/*
capture_state:

0 -> waiting for rising edge
1 -> waiting for falling edge
*/

void HCSR04_Init(void)
{
    /* Nothing needed currently */
}

void HCSR04_Start(void)
{
    if(hcsr04_busy)
    {
        return;
    }

    hcsr04_busy = 1;
    hcsr04_done = 0;

    pulse_width = 0;

    capture_state = 0;

    /* Set capture polarity to rising edge */

    __HAL_TIM_SET_CAPTUREPOLARITY(&htim5,
                                  TIM_CHANNEL_1,
                                  TIM_INPUTCHANNELPOLARITY_RISING);

    /* Send 10us trigger pulse on PA6 */

    HAL_GPIO_WritePin(GPIOA,
                      GPIO_PIN_6,
                      GPIO_PIN_SET);

    uint32_t start =
        __HAL_TIM_GET_COUNTER(&htim5);

    while((__HAL_TIM_GET_COUNTER(&htim5) - start) < 10);

    HAL_GPIO_WritePin(GPIOA,
                      GPIO_PIN_6,
                      GPIO_PIN_RESET);
}

void HCSR04_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim)
{
    if(htim->Instance == TIM5)
    {
        /* Rising Edge */

        if(capture_state == 0)
        {
            rising_edge =
                HAL_TIM_ReadCapturedValue(htim,
                                          TIM_CHANNEL_1);

            capture_state = 1;

            /* Switch to falling edge */

            __HAL_TIM_SET_CAPTUREPOLARITY(htim,
                                          TIM_CHANNEL_1,
                                          TIM_INPUTCHANNELPOLARITY_FALLING);
        }

        /* Falling Edge */

        else if(capture_state == 1)
        {
            falling_edge =
                HAL_TIM_ReadCapturedValue(htim,
                                          TIM_CHANNEL_1);

            /* Handle timer overflow */

            if(falling_edge >= rising_edge)
            {
                pulse_width =
                    falling_edge - rising_edge;
            }
            else
            {
                pulse_width =
                    (0xFFFFFFFF - rising_edge)
                    + falling_edge;
            }

            /* Distance Calculation */

            hcsr04_last_cm =
                (uint16_t)(pulse_width / 58U);

            hcsr04_done = 1;

            hcsr04_busy = 0;

            capture_state = 0;

            /* Reset to rising edge */

            __HAL_TIM_SET_CAPTUREPOLARITY(htim,
                                          TIM_CHANNEL_1,
                                          TIM_INPUTCHANNELPOLARITY_RISING);
        }
    }
}

uint8_t HCSR04_IsDone(void)
{
    return hcsr04_done;
}

uint16_t HCSR04_GetDistance(void)
{
    return hcsr04_last_cm;
}

void HCSR04_ClearDone(void)
{
    hcsr04_done = 0;
}
