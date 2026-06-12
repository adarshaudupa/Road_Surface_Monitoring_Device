/*
 * uart3.h
 *
 *  Created on: Jun 4, 2026
 *      Author: Adarsha Udupa
 */

#ifndef INC_UART3_H_
#define INC_UART3_H_

void UART3_Init(void);
void UART3_SendChar(char ch);
void UART3_SendString(const char *str);


#endif /* INC_UART3_H_ */
