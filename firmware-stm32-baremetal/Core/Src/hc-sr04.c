/*
 * hc-sr04.c
 * TRIG: PA6 — output push-pull
 * ECHO: PA0 — AF2, TIM5_CH1 input capture
 * No main-loop polling: event-driven, background ultrasonic measurement.
 */

#include "stm32f4xx.h"
#include "hc-sr04.h"
#include "tim5.h"
#include "uart2.h"

// --- Exposed state for ISR and main ---
volatile uint8_t  hcsr04_busy    = 0;
volatile uint8_t  hcsr04_done    = 0;
volatile uint16_t hcsr04_last_cm = 0;

// Also globally available from tim5.c:
extern volatile uint32_t echo_start;
extern volatile uint32_t echo_width;
extern volatile uint8_t  echo_state;

/* --- GPIO/TRIG config --- */
void HCSR04_Init(void)
{
    // --- TRIG: PA6: Output, push-pull, high speed ---
    RCC->AHB1ENR |= (1 << 0);       // Enable GPIOA clock

    // Configure PA6 (TRIG) as output push-pull
    GPIOA->MODER   &= ~(3U << 12);
    GPIOA->MODER   |=  (1U << 12);    // 01: General purpose output
    GPIOA->OTYPER  &= ~(1U << 6);
    GPIOA->OSPEEDR |=  (3U << 12);    // High speed
    GPIOA->PUPDR   &= ~(3U << 12);    // No pull
    GPIOA->ODR     &= ~(1U << 6);     // Start with TRIG low

    // --- ECHO: PA0: Alternate function (AF2 = TIM5_CH1) ---
    // Configure PA0 as AF2 (TIM5_CH1 Input Capture)
    GPIOA->MODER   &= ~(3U << 0);
    GPIOA->MODER   |=  (2U << 0);     // 10: Alternate function
    GPIOA->AFR[0]  &= ~(0xFU << 0);
    GPIOA->AFR[0]  |=  (2U << 0);     // AF2 = TIM5 for PA0
    GPIOA->PUPDR   &= ~(3U << 0);
    GPIOA->PUPDR   |=  (2U << 0);     // Pull-down (recommended for echo idle low)

    // (TIM5 itself must be initialized elsewhere)
}

/* --- Start a measurement, non-blocking --- */
void HCSR04_Start(void)
{
    if (hcsr04_busy)
        return;

    hcsr04_busy = 1;
    hcsr04_done = 0;
    hcsr04_last_cm = 0;

    echo_state = 0;
    echo_width = 0;
    echo_start = 0;

    // TRIG high
    GPIOA->ODR |= (1U << 6);

    // precise 10us pulse using TIM5 micros
    uint32_t t = TIM5_GetMicros();
    while ((uint32_t)(TIM5_GetMicros() - t) < 10U)
        ;

    // TRIG low
    GPIOA->ODR &= ~(1U << 6);
}

/* --- Called from TIM5 IRQ on edge or from timeout logic --- */
void HCSR04_ISR_CaptureDone(void)
{
    if (!hcsr04_busy) return;

    // Edge machine in TIM5 IRQ sets echo_state==2 after falling edge
    if (echo_state == 2 && echo_width > 0)
    {
        hcsr04_last_cm = (uint16_t)(echo_width / 58U);  // standard formula
    }
    else
    {
        hcsr04_last_cm = 0; // timeout or error
    }
    hcsr04_done = 1;
    hcsr04_busy = 0;
}

/* --- Query if result is ready (non-blocking) --- */
uint8_t HCSR04_IsDone(void)
{
    return hcsr04_done;
}

/* --- Get last distance (valid only if IsDone()==1) --- */
uint16_t HCSR04_GetLastCm(void)
{
    return hcsr04_last_cm;
}

/* --- Called from main after reading value to clear done flag --- */
void HCSR04_ClearDone(void)
{
    hcsr04_done = 0;
}
