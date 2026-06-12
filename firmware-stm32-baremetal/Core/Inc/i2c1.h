/*
 * i2c1.h
 *
 *  Created on: Jun 5, 2026
 *      Author: Adarsha Udupa
 */

#ifndef INC_I2C1_H_
#define INC_I2C1_H_

#include <stdint.h>

void I2C1_Init(void);
uint8_t I2C1_ReadRegister(uint8_t dev_addr, uint8_t reg_addr);
void I2C1_ReadMulti(uint8_t dev_addr, uint8_t reg_addr, uint8_t *data, uint8_t count);
void I2C1_WriteByte(uint8_t dev_addr, uint8_t reg_addr, uint8_t value);

#endif /* INC_I2C1_H_ */
