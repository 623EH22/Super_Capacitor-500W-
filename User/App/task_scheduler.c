#include "task_scheduler.h"
#include "FreeRTOS.h"
#include "task.h"
#include "cmsis_os.h"
#include "queue.h"
#include "message_buffer.h"
#include "biz_usart.h"
#include "config.h"
#include "biz_fdcan.h"
#include "SuperCap.h"
#include "bms_cmd.h"
#include "bsp_rgb.h"
#include "stm32g4xx.h"

extern osThreadId_t CAN_Send_TaskHandle;
extern osThreadId_t UartTaskHandle;
extern MessageBufferHandle_t uartMessageBuffer;
extern osMessageQueueId_t FDCANQueueHandle;

extern SuperCap_Handle_t supcap;

#ifdef USE_BMS_CMD
extern BMS_Handle_t bms_handle;
#endif


void StartCAN_Send_Task(void *argument)
{
    for(;;)
    {
        SuperCap_SendCanMsg();
        osDelay(1);
    }
}

void StartUartTask(void *argument)
{
	usart_init();
    for(;;)
    {
				u8 rx_data[128];
        size_t received_size = xMessageBufferReceive(uartMessageBuffer, rx_data, sizeof(rx_data), portMAX_DELAY);
        uart_data_process((char*)rx_data, received_size);
    }
}

void StartUartTxTask(void *argument)
{
	for(;;)
	{ 
		printf_data();
		osDelay(100);
	}
}

void StartRGBTask(void *argument)
{
	for(;;)
    {
        #ifdef USE_BMS_CMD
        switch (bms_handle.status)
        {
            case UNINIT: RGB_Set(ON,OFF,OFF);
            break;
            case POWER_METER: RGB_Set(OFF,ON,OFF);
            break;
            case POWER_CONTROL: RGB_Set(OFF,OFF,ON);
            break;
            default: RGB_Set(OFF,OFF,OFF);
            break;
        }
        #else
        if(supcap.OpenPwmFlag == false)
        {
            RGB_Set(ON,OFF,OFF);
        }else if(supcap.OpenPwmFlag == true && supcap.iref > 0.2)
        {
            RGB_Set(OFF,ON,OFF);
        }else if(supcap.OpenPwmFlag == true && supcap.iref <= -0.2)
        {
            RGB_Set(OFF,OFF,ON);
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

unsigned long getRunTimeCounterValue(void){
    return DWT->CYCCNT;
}