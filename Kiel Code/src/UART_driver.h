#ifndef __USART_DRIVER_H
#define __USART_DRIVER_H

#include "stdio.h"
#include "string.h"
#include "Driver_USART.h" // USART API

int16_t UART_Init(void);
void UART_send(char* user_data, uint16_t num);
void UART_receive(char* user_data, uint16_t num);
void UART_receivestring(char* user_data, uint16_t maxnum);

#endif // __USART_DRIVER_H
