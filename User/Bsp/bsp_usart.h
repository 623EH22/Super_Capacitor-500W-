#ifndef BSP_USART_H
#define BSP_USART_H

#include "config.h"

#define RX_BUFFER_SIZE  128

typedef void (*uart_rx_callback)(u16 size);

typedef struct 
{
    s32 (*init)(void);
    s32 (*printf)(const char *format, ...);
    s32 (*transmit)(u8 *pdata, u16 size);
    u8 rx_buffer[RX_BUFFER_SIZE]; // 接收缓冲区
    void (*rx_callback_register)(uart_rx_callback callback);
}uart_t;

extern uart_t user_uart_t;

#endif /* BSP_USART_H */
