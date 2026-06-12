/*
 * hc-sr04.h
 *
 *  Created on: Jun 5, 2026
 *      Author: Adarsha Udupa
 */

#ifndef INC_HC_SR04_H_
#define INC_HC_SR04_H_

#include <stdint.h>

void     HCSR04_Init(void);
void     HCSR04_Start(void);
void     HCSR04_ISR_CaptureDone(void);
uint8_t  HCSR04_IsDone(void);
uint16_t HCSR04_GetLastCm(void);
void     HCSR04_ClearDone(void);

#endif /* INC_HC_SR04_H_ */
