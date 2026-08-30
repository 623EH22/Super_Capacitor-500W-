#ifndef BIZ_USART_H
#define BIZ_USART_H

#include "config.h"

typedef struct 
{
    u8 print_ready; // 打印准备就绪标志
    float float_data;
    u16  uint16_data;
}uart_data_t;


s32 usart_init(void);
void uart_data_process(char *pdata,u16 size);
void printf_data(void);

#endif /* BIZ_USART_H */