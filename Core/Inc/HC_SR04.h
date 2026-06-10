/* HC_SR04.h */

#ifndef INC_HC_SR04_H_
#define INC_HC_SR04_H_

#include "main.h"
#include <stdint.h>

/* Public Flags */
extern volatile uint8_t  hcsr04_busy;
extern volatile uint8_t  hcsr04_done;
extern volatile uint16_t hcsr04_last_cm;

/* API */
void HCSR04_Init(void);

void HCSR04_Start(void);

void HCSR04_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim);

uint8_t HCSR04_IsDone(void);

uint16_t HCSR04_GetDistance(void);

void HCSR04_ClearDone(void);

#endif
