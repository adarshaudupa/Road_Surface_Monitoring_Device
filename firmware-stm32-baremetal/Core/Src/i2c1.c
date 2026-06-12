/*
 * i2c1.c
 *
 *  Created on: Jun 5, 2026
 *      Author: Adarsha Udupa
 */

/*
 * i2c1.c
 *
 *  Created on: Feb 28, 2026
 *      Author: Adarsha Udupa
 */
#include "i2c1.h"
#include "stm32f4xx.h"
#include "uart2.h"

uint8_t dev_addr = 0x6B; // LSM6DS3 I2C address (SA0=0)

void I2C1_Init(void)
{
 RCC->AHB1ENR |= (1<<1); //Enable GPIOB Clock
 RCC->APB1ENR |= (1<<21); //Enable I2C1 Clock
 RCC->APB1RSTR |= (1<<21); //Reset I2C1
 RCC->APB1RSTR &= ~(1<<21); //Release reset
 GPIOB->MODER &= ~((3<<16) | (3<<18));
 GPIOB->MODER |= (2<<16) | (2<<18); //Setting PB8 and PB9 to AF
 GPIOB->OTYPER &= ~((1<<8) | (1<<9));
 GPIOB->OTYPER |= (1<<8) | (1<<9); //Set to Open drain type output
 GPIOB->OSPEEDR &= ~((3<<16) | (3<<18));
 GPIOB->OSPEEDR |= (3<<16) | (3<<18);
 GPIOB->PUPDR &= ~((3<<16) | (3<<18));
 GPIOB->PUPDR |=  ((1<<16) | (1<<18));

 GPIOB->AFR[1] &= ~((0xF<<0) | (0xF<<4));
 GPIOB->AFR[1] |= (4<<0) | (4<<4); //PB8 and PB9 set to I2C AF(AF4)

 I2C1->CR1 = 0;
 I2C1->CR2 = 0;
 I2C1->OAR1 = 0;
 I2C1->OAR2 = 0;
 I2C1->CCR = 0;
 I2C1->TRISE = 0;

 uint32_t pclk = 16000000; // APB1 clock = 16MHz (HSI))
 uint32_t pclk_mhz = pclk / 1000000;
 I2C1->CR2 = pclk_mhz;
 //CCR =(PClk/(2 or 3 * Target Frequency) 2 for standard mode(100KHz) and 3 for Fast mode(400Khz)
 I2C1->CCR = pclk / (2 * 100000);
 //TRISE = (PClk/1000000) +1 for Standard mode
 I2C1->TRISE = pclk_mhz + 1;
 I2C1->CR1 |= (1<<0); //PE i.e Peripheral Enable Bit Set

 //uart_print_uint("I2C CR2 = ", I2C1->CR2);
 //uart_print_uint("I2C CCR = ", I2C1->CCR);
 //uart_print_uint("I2C TRISE = ", I2C1->TRISE);

}

uint8_t I2C1_ReadRegister(uint8_t dev_addr, uint8_t reg_addr)
{
    uint32_t timeout;

    I2C1->CR1 |= (1<<8);  // START
    timeout = 100000;
    while (!(I2C1->SR1 & (1<<0)))
    {
        if (--timeout == 0)
        {
            I2C1->CR1 |= (1<<9);  // STOP to release bus
            UART2_SendString("I2C timeout: START\r\n");
            return 0xFF;
        }
    }

    I2C1->DR = (dev_addr << 1) | 0x00;
    timeout = 100000;
    while (!(I2C1->SR1 & (1<<1)))
    {
        if (--timeout == 0)
        {
            I2C1->CR1 |= (1<<9);
            UART2_SendString("I2C timeout: ADDR\r\n");
            return 0xFF;
        }
    }
    (void)I2C1->SR2;

    I2C1->DR = reg_addr;
    timeout = 100000;
    while (!(I2C1->SR1 & (1<<7)))
    {
        if (--timeout == 0)
        {
            I2C1->CR1 |= (1<<9);
            UART2_SendString("I2C timeout: TXE\r\n");
            return 0xFF;
        }
    }
    timeout = 100000;
    while (!(I2C1->SR1 & (1<<2)))
    {
        if (--timeout == 0)
        {
            I2C1->CR1 |= (1<<9);
            UART2_SendString("I2C timeout: BTF\r\n");
            return 0xFF;
        }
    }

    I2C1->CR1 |= (1<<8);  // Repeated START
    timeout = 100000;
    while (!(I2C1->SR1 & (1<<0)))
    {
        if (--timeout == 0)
        {
            I2C1->CR1 |= (1<<9);
            UART2_SendString("I2C timeout: RST\r\n");
            return 0xFF;
        }
    }

    I2C1->DR = (dev_addr << 1) | 0x01;
    timeout = 100000;
    while (!(I2C1->SR1 & (1<<1)))
    {
        if (--timeout == 0)
        {
            I2C1->CR1 |= (1<<9);
            UART2_SendString("I2C timeout: ADDR-R\r\n");
            return 0xFF;
        }
    }

    I2C1->CR1 &= ~(1<<10);
    (void)I2C1->SR1;
    (void)I2C1->SR2;
    I2C1->CR1 |= (1<<9);

    timeout = 100000;
    while (!(I2C1->SR1 & (1<<6)))
    {
        if (--timeout == 0)
        {
            UART2_SendString("I2C timeout: RXNE\r\n");
            return 0xFF;
        }
    }

    uint8_t data = I2C1->DR;
    I2C1->CR1 |= (1<<10);
    return data;
}


// Replace your existing I2C1_ReadMulti with this version:
void I2C1_ReadMulti(uint8_t dev_addr, uint8_t reg_addr, uint8_t *data, uint8_t count)
{
    uint32_t timeout = 500000;

    // 1. START
    I2C1->CR1 |= (1 << 8);
    timeout = 500000;
    while (!(I2C1->SR1 & (1 << 0))) { if (--timeout == 0) { UART2_SendString("I2C ERR: START\r\n"); return; } }

    // 2. ADDR (Write)
    I2C1->DR = (dev_addr << 1);
    timeout = 500000;
    while (!(I2C1->SR1 & (1 << 1))) { if (--timeout == 0) { UART2_SendString("I2C ERR: ADDR\r\n"); return; } }
    (void)I2C1->SR1; (void)I2C1->SR2;

    // 3. REG
    I2C1->DR = reg_addr;
    timeout = 500000;
    while (!(I2C1->SR1 & (1 << 7))) { if (--timeout == 0) return; }

    // 4. RESTART
    I2C1->CR1 |= (1 << 8);
    timeout = 500000;
    while (!(I2C1->SR1 & (1 << 0))) { if (--timeout == 0) return; }

    // 5. ADDR (Read)
    I2C1->DR = (dev_addr << 1) | 1;
    timeout = 500000;
    while (!(I2C1->SR1 & (1 << 1))) { if (--timeout == 0) return; }
    (void)I2C1->SR1; (void)I2C1->SR2;

    I2C1->CR1 |= (1 << 10);

    for (uint8_t i = 0; i < count; i++)
    {
        if (i == (count - 1)) { I2C1->CR1 &= ~(1 << 10); I2C1->CR1 |= (1 << 9); }
        timeout = 500000;
        while (!(I2C1->SR1 & (1 << 6))) { if (--timeout == 0) return; }
        data[i] = I2C1->DR;
    }
}

void I2C1_WriteByte(uint8_t dev_addr, uint8_t reg_addr, uint8_t value)
{
    uint32_t timeout;

    // START
    I2C1->CR1 |= (1 << 8);
    timeout = 100000;
    while (!(I2C1->SR1 & (1 << 0)))
    {
        if (--timeout == 0) { UART2_SendString("I2C WR: START timeout\r\n"); return; }
    }

    // Address + W
    I2C1->DR = (dev_addr << 1) | 0x00;
    timeout = 100000;
    while (!(I2C1->SR1 & (1 << 1)))
    {
        if (--timeout == 0) { I2C1->CR1 |= (1<<9); UART2_SendString("I2C WR: ADDR timeout\r\n"); return; }
    }
    (void)I2C1->SR2;

    // Register address
    timeout = 100000;
    while (!(I2C1->SR1 & (1 << 7)))
    {
        if (--timeout == 0) { I2C1->CR1 |= (1<<9); UART2_SendString("I2C WR: TXE timeout\r\n"); return; }
    }
    I2C1->DR = reg_addr;
    timeout = 100000;
    while (!(I2C1->SR1 & (1 << 2)))
    {
        if (--timeout == 0) { I2C1->CR1 |= (1<<9); UART2_SendString("I2C WR: BTF1 timeout\r\n"); return; }
    }

    // Data
    I2C1->DR = value;
    timeout = 100000;
    while (!(I2C1->SR1 & (1 << 2)))
    {
        if (--timeout == 0) { I2C1->CR1 |= (1<<9); UART2_SendString("I2C WR: BTF2 timeout\r\n"); return; }
    }

    // STOP
    I2C1->CR1 |= (1 << 9);
}
