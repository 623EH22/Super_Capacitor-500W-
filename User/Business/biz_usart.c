#include "biz_usart.h"
#include "bsp_usart.h"
#include "FreeRTOS.h"
#include "cmsis_os.h"
#include "message_buffer.h"
#include "string.h"
#include "stdbool.h"
#include "stdlib.h"
#include "bsp_analog.h"
#include "SuperCap.h"

extern uart_t user_uart_t;
extern SuperCap_Handle_t supcap;

extern MessageBufferHandle_t uartMessageBuffer;

void uart_rx_callback_func(u16 size);

uart_data_t uart_data = {0};

/*初始化串口*/
s32 usart_init(void)
{
    user_uart_t.rx_callback_register(uart_rx_callback_func); // 注册回调函数
    return user_uart_t.init();
}

/*中断回调*/
void uart_rx_callback_func(u16 size)
{
   BaseType_t pxHighterPriorityTaskWoken = pdFALSE;
   xMessageBufferSendFromISR(uartMessageBuffer,user_uart_t.rx_buffer,size,&pxHighterPriorityTaskWoken);
   portYIELD_FROM_ISR(pxHighterPriorityTaskWoken);
}

/*数据处理*/
void uart_data_process(char *pdata,u16 size)
{
    char ch = pdata[size-1]; // 获取最后一个字节
    pdata[size-1] = '\0'; // 将'\r'替换为字符串结束符
    if(ch == '\r')
    {
        if(strcmp(pdata,"start") == 0)
        {
            uart_data.print_ready = true; // 设置打印准备就绪标志
        }else if(strcmp(pdata,"stop") == 0)
        {
            uart_data.print_ready = false; // 清除打印准备就绪标志
        }else
        {
            float temp = atof((char*)pdata); // 将字符串转换为浮点数
            if(temp != 0.0f || pdata[0] == '0' || pdata[1] == '\0') // 判断转换是否成功（非法输入会返回0.0）
            {
                // 处理有效的浮点数数据
                uart_data.float_data = temp; // 存储转换后的浮点数
                user_uart_t.printf("Received float: %.2f\r\n", uart_data.float_data); // 打印接收到的浮点数
            }else
            {
                // 处理非法输入
                user_uart_t.printf("Invalid input: %f\r\n", temp); // 打印非法输入信息
            }
        }
    }else if(ch == '\n')
    {
        if(strcmp(pdata,"start") == 0)
        {
            uart_data.print_ready = true; // 设置打印准备就绪标志
        }else if(strcmp(pdata,"stop") == 0)
        {
            uart_data.print_ready = false; // 清除打印准备就绪标志
        }else
        {
            u32 temp = (u32)atoi((char*)pdata); // 将字符串转换为无符号整数
            if(temp <= 65535)
            {
                // 处理有效的无符号整数数据
                uart_data.uint16_data = (u16)temp; // 存储转换后的无符号整数
                user_uart_t.printf("Received uint16: %u\r\n", uart_data.uint16_data); // 打印接收到的无符号整数
            }else
            {
                // 处理非法输入
                user_uart_t.printf("Invalid input: %u\r\n", temp); // 打印非法输入信息
            }
        }
    }
}

char global_stats_buffer[1024]; 

void PrintCPUStats(void) {
    static char statsBuffer[512]; // 确保缓冲区够大，容纳所有任务信息
    
    memset(global_stats_buffer, 0, sizeof(global_stats_buffer));
    vTaskGetRunTimeStats(global_stats_buffer);
    
    // 用最稳妥的方式打印
    user_uart_t.printf("\r\n--- START ---\r\n");
		user_uart_t.printf("%s", global_stats_buffer);
		user_uart_t.printf("\r\n--- END ---\r\n");
}

extern u16 shutdown;
extern float available_power;

void printf_data(void)
{
//	 PrintCPUStats();
	 if(uart_data.print_ready)
   {
       // 如果打印准备就绪，执行相关操作（例如打印数据） 
//       user_uart_t.printf("%f,%f,%f,%f,%f,%f\n",user_analog_t.original_voltage[0],user_analog_t.original_voltage[1],user_analog_t.original_voltage[2],user_analog_t.original_voltage[3],user_analog_t.original_voltage[4],user_analog_t.original_voltage[5]);
			user_uart_t.printf("%f,%f,%f,%f,%f\n",supcap.ele.cap_vol, supcap.ele.inp_vol, supcap.ele.oup_cur, supcap.ele.inp_cur, supcap.ele.cap_cur);
//		 user_uart_t.printf("%d,%d,%f\n",supcap.OpenPwmFlag,supcap.pwm_val,supcap.iref);
//			user_uart_t.printf("%f,%f,%f,%f,%d,%d,%d\n",supcap.ele.cap_cur,supcap.ele.cap_vol,supcap.ele.inp_vol,supcap.ele.oup_cur,supcap.pwm_val,supcap.OpenPwmFlag,shutdown);
	 }
}

