/*
 * tim5.c
 *
 *  Created on: Jun 5, 2026
 *      Author: Adarsha Udupa
 */

/*
 * tim5.c
 * TIM5 — dual role:
 *   1. Free-running 1µs counter for timestamps (TIM5->CNT via TIM5_GetMicros)
 *   2. Input capture on CH1 (PA0, AF2) for HC-SR04 echo measurement
 *
 * Clock: HSI 16MHz, APB1 = 16MHz, TIM5 clock = 16MHz
 * PSC = 15 → 16MHz/(15+1) = 1MHz → 1 tick = 1µs
 */

#include "stm32f4xx.h"
#include "tim5.h"
#include "hc-sr04.h"

volatile uint8_t  edge_count  = 0;
volatile uint32_t echo_start  = 0;
volatile uint32_t echo_width  = 0;
volatile uint8_t  echo_state = 0;   // 0=idle(wait rising), 1=got rising(wait falling), 2=done

void TIM5_Init(void)
{
    // 1. Clocks
    RCC->APB1ENR |= (1 << 3);   // TIM5EN
    RCC->AHB1ENR |= (1 << 0);   // GPIOA

    // 2. PA0 → AF2 (TIM5_CH1)
    GPIOA->MODER  &= ~(3 << 0);
    GPIOA->MODER  |=  (2 << 0);   // AF mode
    GPIOA->AFR[0] &= ~(0xF << 0);
    GPIOA->AFR[0] |=  (2 << 0);   // AF2 = TIM5
    // Pull-down on PA0 (recommended for ECHO idle-low if using divider)
    GPIOA->PUPDR &= ~(3U << 0);
    GPIOA->PUPDR |=  (2U << 0);  // 10 = pull-down

    // 3. Timer base: 1µs per tick
    TIM5->CR1  = 0;
    TIM5->CNT  = 0;
    TIM5->PSC  = 15;              // 16MHz / 16 = 1MHz
    TIM5->ARR  = 0xFFFFFFFF;      // 32-bit free-run
    TIM5->EGR  = 1;              // UG: latch PSC/ARR immediately
    TIM5->SR   = 0;               // clear all flags

    // 4. CH1 input capture
    TIM5->CCMR1 &= ~(3 << 0);
    TIM5->CCMR1 |=  (1 << 0);   // CC1S = 01: IC1 mapped on TI1

    TIM5->CCMR1 &= ~(0xF << 4);
    TIM5->CCMR1 |=  (0x8 << 4); // IC1F = 1000: filter 8 samples

    TIM5->CCER  &= ~(1 << 1);   // CC1P = 0: rising edge first
    TIM5->CCER  |=  (1 << 0);   // CC1E = 1: capture enabled

    // 5. Interrupt on capture
    TIM5->DIER  |=  (1 << 1);   // CC1IE

    NVIC_SetPriority(TIM5_IRQn, 2);

    TIM5->SR = 0U;
    NVIC_ClearPendingIRQ(TIM5_IRQn);
    NVIC_EnableIRQ(TIM5_IRQn);

    // 6. Start counter
    TIM5->CR1 |= (1 << 0);      // CEN
}

uint32_t TIM5_GetMicros(void)
{
    return TIM5->CNT;
}

void TIM5_IRQHandler(void)
{
    if (!(TIM5->SR & (1 << 1))) return;  // CC1IF not set
    TIM5->SR &= ~(1 << 1);               // clear CC1IF

    uint32_t captured = TIM5->CCR1;

    if (edge_count == 0)
    {
        // Rising edge captured
        echo_start = captured;
        echo_width = 0;
        echo_state = 1;
        TIM5->CCER |=  (1 << 1);  // CC1P = 1: switch to falling edge
        edge_count  = 1;
    }
    else if (edge_count == 1)
    {
        // Falling edge captured
        echo_width  = (captured >= echo_start) ? (captured - echo_start) : (0xFFFFFFFF - echo_start + captured + 1); // wrap
        echo_state = 2;
        TIM5->CCER &= ~(1 << 1);  // CC1P = 0: back to rising edge
        edge_count = 0; // optional: reset for next measurement
        HCSR04_ISR_CaptureDone();
    }
    else	{
		// Should not happen, but reset state if it does
		echo_state = 0;
		edge_count = 0;
		TIM5->CCER &= ~(1 << 1);  // CC1P = 0: ensure rising edge
	}
}
