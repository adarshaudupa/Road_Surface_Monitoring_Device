/*
 * uart3.c
 * ESP32 comms — USART3, PC10 TX, PC11 RX, AF7, APB1
 */

#include "uart3.h"
#include "stm32f4xx.h"

void UART3_Init(void)
{
    RCC->AHB1ENR |= (1 << 2);   // GPIOC
    RCC->APB1ENR |= (1 << 18);  // USART3

    // PC10 TX, PC11 RX → AF7
    GPIOC->MODER &= ~((3 << 20) | (3 << 22));
    GPIOC->MODER |=  (2 << 20)  | (2 << 22);
    GPIOC->AFR[1] &= ~((0xF << 8) | (0xF << 12));
    GPIOC->AFR[1] |=  (7 << 8)   | (7 << 12);
    GPIOC->PUPDR &= ~(3 << 22);
    GPIOC->PUPDR |=  (1 << 22);   // pull-up
    GPIOC->OSPEEDR |= (3 << 20) | (3 << 22);

    // 115200 baud @ HSI 16MHz: BRR = 16000000/115200 = 138 = 0x8A
    USART3->BRR = 0x008A;
    USART3->CR1 = (1 << 13) | (1 << 3) | (1 << 2); // UE, TE, RE
}

void UART3_SendChar(char ch)
{
    while (!(USART3->SR & (1 << 7)));
    USART3->DR = ch;
}

void UART3_SendString(const char *str)
{
    while (*str) {
        UART3_SendChar(*str++);
    }
}


