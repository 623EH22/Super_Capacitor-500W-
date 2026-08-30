#include "system_init.h"
#include "biz_usart.h"
#include "biz_fdcan.h"
#include "SuperCap.h"
#include "task_scheduler.h"


void system_init(void)
{
//    usart_init();
    SuperCap_Init();
    fdcan_init();
		configureTimerForRunTimeStats();
}
