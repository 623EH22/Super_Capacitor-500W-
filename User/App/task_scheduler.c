/*
*************************************************************************************************************************
*                                                     TASK_SCHEDULER
*                      Application layer module for the 500W Super-Capacitor Digital Power Supply.
* Filename      : task_scheduler.c
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
#define DEF_TASK_SCHEDULER
#include "task_scheduler.h"
#include "FreeRTOS.h"
#include "task.h"
#include "cmsis_os.h"
#include "queue.h"
#include "message_buffer.h"
#include "hal_usart.h"
#include "config.h"
#include "hal_fdcan.h"
#include "hal_supercap.h"
#include "hal_bms_cmd.h"
#include "drv_rgb.h"
#include "stm32g4xx.h"
#include "app_storage.h"
/*
*************************************************************************************************************************
*                                                    PRIVATE DEFINES
*************************************************************************************************************************
*/

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

#if USE_BMS_CMD
extern hal_bms_handle_t bms_handle;
#endif
extern osThreadId_t          CAN_Send_TaskHandle;
extern osThreadId_t          UartTaskHandle;
extern MessageBufferHandle_t uartMessageBuffer;
extern osMessageQueueId_t    FDCANQueueHandle;
extern hal_supercap_handle_t supcap;

/*
*************************************************************************************************************************
*                                               PRIVATE (HELPER) FUNCTIONS
*************************************************************************************************************************
*/

/*
*************************************************************************************************************************
*                                               GLOBAL FUNCTION PROTOTYPES
*************************************************************************************************************************
*/

uint32_t storage_test;

void     StartCAN_Send_Task(void *argument)
{
    for (;;) {
        hal_supercap_send_can_msg();
        osDelay(1);
    }
}

void StartUartTask(void *argument)
{
    hal_usart_init();
    for (;;) {
        u8     rx_data[128];
        size_t received_size = xMessageBufferReceive(uartMessageBuffer, rx_data, sizeof(rx_data), portMAX_DELAY);
        hal_uart_data_process((char *)rx_data, received_size);
    }
}

void StartUartTxTask(void *argument)
{
    for (;;) {
        // hal_printf_data();
        HAL_STORAGE_FUNC_INFO_HANDLE_T *ptr = App_Storage_GetHandle();

        storage_test++;
        ptr->para_updt_func(SP_CAP_CURR_ERR_OFFSET, &storage_test, sizeof(storage_test));
        osDelay(100);
    }
}

void StartRGBTask(void *argument)
{
    for (;;) {
#if USE_BMS_CMD
        switch (bms_handle.status) {
            case BMS_UNINIT:
                RGB_Set(ON, OFF, OFF);
                break;
            case BMS_POWER_METER:
                RGB_Set(OFF, ON, OFF);
                break;
            case BMS_POWER_CONTROL:
                RGB_Set(OFF, OFF, ON);
                break;
            default:
                RGB_Set(OFF, OFF, OFF);
                break;
        }
#else
        if (supcap.OpenPwmFlag == false) {
            RGB_Set(ON, OFF, OFF);
        } else if (supcap.OpenPwmFlag == true && supcap.iref > 0.2) {
            RGB_Set(OFF, ON, OFF);
        } else if (supcap.OpenPwmFlag == true && supcap.iref <= -0.2) {
            RGB_Set(OFF, OFF, ON);
        }
#endif
        osDelay(100);
    }
}

void configureTimerForRunTimeStats(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

unsigned long getRunTimeCounterValue(void)
{
    return DWT->CYCCNT;
}