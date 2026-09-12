/*
*************************************************************************************************************************
*                                                       DRV_USART
*                            Driver module for the 500W Super-Capacitor Digital Power Supply.
* Filename      : drv_usart.c
* Version       : V1.00.00
* Programmer(s) : RuiYuan Lin
*************************************************************************************************************************
* Note(s)       : TBD
*************************************************************************************************************************
*                                                  MODIFICATION HISTORY
*************************************************************************************************************************
*   Version   Date          Author        Description
*   V1.00.00  2026-09-01    RuiYuan Lin   Initial release
*************************************************************************************************************************
*                                                     INCLUDE FILES
*************************************************************************************************************************
*/
#define DEF_DRV_USART
#include "drv_usart.h"
#include "usart.h"
#include <stdio.h>
#include <stdarg.h>

/*
*************************************************************************************************************************
*                                                    PRIVATE DEFINES
*************************************************************************************************************************
*/

#define USE_TX_DMA 1
#define PRINTF_BUF_SIZE 512

/*
*************************************************************************************************************************
*                                                     PRIVATE TYPES
*************************************************************************************************************************
*/

/*
*************************************************************************************************************************
*                                                   PRIVATE VARIABLES
*************************************************************************************************************************
*/

/*
*************************************************************************************************************************
*                                                    PUBLIC VARIABLES
*************************************************************************************************************************
*/

uart_rx_callback user_uart_rx = NULL;
volatile char printf_buf[PRINTF_BUF_SIZE];      // printf使用的缓冲区

/*
*************************************************************************************************************************
*                                               PRIVATE (HELPER) FUNCTIONS
*************************************************************************************************************************
*/

static s32 Uart_Init(void);
static s32 user_printf(const char *format, ...);
static s32 user_transmit(u8 *pdata, u16 size);
static void user_uart_register_callback(uart_rx_callback callback);

static s32 Uart_Init(void)
{
	s32 status = 0;
	status = HAL_UARTEx_ReceiveToIdle_DMA(&DEBUG_SERIAL,(uint8_t *)user_uart_t.rx_buffer,RX_BUFFER_SIZE);
	__HAL_DMA_DISABLE_IT(DEBUG_SERIAL.hdmarx,DMA_IT_HT);
	return status;
}

static s32 user_printf(const char *format, ...)
{
    va_list args;
    u32 len;

    va_start(args, format);
    len = vsnprintf((char*)printf_buf, PRINTF_BUF_SIZE, format, args);
    va_end(args);

    #if USE_TX_DMA
    	HAL_UART_Transmit_DMA(&DEBUG_SERIAL,(u8 *)printf_buf, len);
    #else
    	HAL_UART_Transmit(&DEBUG_SERIAL, (u8 *)printf_buf, len, HAL_MAX_DELAY);
    #endif
    
    return len;
}

static s32 user_transmit(u8 *pdata, u16 size)
{
    #if USE_TX_DMA
        return HAL_UART_Transmit_DMA(&DEBUG_SERIAL, pdata, size);
    #else
        return HAL_UART_Transmit(&DEBUG_SERIAL, pdata, size, HAL_MAX_DELAY);
    #endif
}

static void user_uart_register_callback(uart_rx_callback callback)
{
    user_uart_rx = callback;
}

/*
*************************************************************************************************************************
*                                               GLOBAL FUNCTION PROTOTYPES
*************************************************************************************************************************
*/

uart_t user_uart_t = 
{
    .init = Uart_Init,
    .printf = user_printf,
    .transmit = user_transmit,
    .rx_callback_register = user_uart_register_callback
};

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
	if(huart->Instance == DEBUG_SERIAL.Instance)
	{
        if (user_uart_rx != NULL) {
            user_uart_rx(Size); // 调用用户注册的回调函数
        }
		HAL_UARTEx_ReceiveToIdle_DMA(&DEBUG_SERIAL,(uint8_t *)user_uart_t.rx_buffer,RX_BUFFER_SIZE);
		__HAL_DMA_DISABLE_IT(DEBUG_SERIAL.hdmarx,DMA_IT_HT);
	}
}