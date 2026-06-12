/*
 * tim5.h
 *
 *  Created on: Jun 5, 2026
 *      Author: Adarsha Udupa
 */

#ifndef INC_TIM5_H_
#define INC_TIM5_H_

extern volatile uint8_t  edge_count;
extern volatile uint32_t echo_start;
extern volatile uint32_t echo_width;
extern volatile uint8_t  echo_state;

void TIM5_Init(void);
uint32_t TIM5_GetMicros(void);
void TIM5_IRQHandler(void);

#endif /* INC_TIM5_H_ */
