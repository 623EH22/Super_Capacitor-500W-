/*
*************************************************************************************************************************
*                                                        APP_RTT
*                             Application module for the 500W Super-Capacitor Digital Power Supply.
* Filename      : app_rtt.c
* Version       : V1.01.00
* Programmer(s) : RuiYuan Lin
*************************************************************************************************************************
* Note(s)       : User calls only App_RTT_Init() + App_RTT_Write().
*                 Channel/format table is configured in App_RTT_Init().
*************************************************************************************************************************
*                                                  MODIFICATION HISTORY
*************************************************************************************************************************
*   Version   Date          Author        Description
*   V1.00.00  2026-09-29    RuiYuan Lin   Initial release
*   V1.01.00  2026-09-29    RuiYuan Lin   Single write entry
*/

#define DEF_APP_RTT
#include "app_rtt.h"
#include "hal_rtt.h"
#include "drv_rtt.h"

/*
*************************************************************************************************************************
*                                                    PRIVATE DEFINES
*************************************************************************************************************************
*/

/* ---- stream config (edit here) ---- */
#define APP_RTT_SAMPLE_HZ 50000U
#define APP_RTT_ADC_BITS  12U

/*
*************************************************************************************************************************
*                                               GLOBAL FUNCTION PROTOTYPES
*************************************************************************************************************************
*/

void App_RTT_Init(void)
{
    /* per-channel format — sparse is allowed (only register what you use) */
    hal_rtt_register_channel(0, DRV_RTT_FMT_U2);
    /* 通道1暂时停发（只发通道0），恢复时取消下行注释 */
    hal_rtt_register_channel(1, DRV_RTT_FMT_U2);
//		hal_rtt_register_channel(2, DRV_RTT_FMT_U2);
    hal_rtt_start(APP_RTT_SAMPLE_HZ, APP_RTT_ADC_BITS);
}

uint8_t App_RTT_Write(uint8_t channel, const void *p_sample)
{
    return hal_rtt_write_channel(channel, p_sample);
}
