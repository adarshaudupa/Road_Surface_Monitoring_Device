/*
 * uart1.c
 *
 *  Created on: Jun 4, 2026
 *      Author: Adarsha Udupa
 */

/*
 * uart1.c
 * GPS receive — USART1, PA9 TX, PA10 RX, AF7, APB2
 */

#include "uart1.h"
#include "stm32f4xx.h"


void UART1_Init(void)
{
    RCC->AHB1ENR |= (1 << 0);   // GPIOA
    RCC->APB2ENR |= (1 << 4);   // USART1

    // PA9 TX, PA10 RX → AF7
    GPIOA->MODER &= ~((3 << 18) | (3 << 20));
    GPIOA->MODER |=  (2 << 18)  | (2 << 20);
    GPIOA->AFR[1] &= ~((0xF << 4) | (0xF << 8));
    GPIOA->AFR[1] |=  (7 << 4)   | (7 << 8);

    // 9600 baud @ HSI 16MHz: BRR = 16000000/9600 = 1667 = 0x683
    USART1->BRR = 0x0683;
    USART1->CR1 = (1 << 13) | (1 << 3) | (1 << 2) | (1 << 5); // UE,TE,RE,RXNEIE
    GPIOA->OSPEEDR |= (3<<18) | (3<<20);   // very high speed
    GPIOA->PUPDR   &= ~((3<<18) | (3<<20));
    GPIOA->PUPDR   |=  (1<<20);             // PA10 pull-up (RX)
    NVIC_SetPriority(USART1_IRQn, 1);
    NVIC_EnableIRQ(USART1_IRQn);
}
