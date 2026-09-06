/*
*************************************************************************************************************************
*                                                       BIZ_FDCAN
*                        Business layer module for the 500W Super-Capacitor Digital Power Supply.
* Filename      : biz_fdcan.c
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

#include "biz_fdcan.h"
#include "drv_fdcan.h"
#include "bms_cmd.h"
#include "SuperCap.h"
#include "FreeRTOS.h"
#include "cmsis_os2.h"
#include "queue.h"
#include "string.h"

/*
*************************************************************************************************************************
*                                                    PRIVATE DEFINES
*************************************************************************************************************************
*/

#define MasterControlId   0x300
#define MasterInitId	 	  0x301
#define SuperCapId			  0x2FF
#define MasterDeInit       0x00
#define MasterPowerMeter   0x01
#define MasterSupCapCtr    0x02
#if USE_BMS_CMD
typedef s32 (*SendCanMsgFunc)(void);
void fdcan_rx_callback_func(void);
s32 fdcan_filter_init(void);
#endif

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

extern drv_fdcan_t user_fdcan_t;
extern osMessageQueueId_t FDCANQueueHandle;
extern SuperCap_Handle_t supcap;

/*
*************************************************************************************************************************
*                                               PRIVATE (HELPER) FUNCTIONS
*************************************************************************************************************************
*/


static s32 fdcan_send(uint8_t *data)
{
    #if USE_BMS_CMD
    return user_fdcan_t.send(SuperCapId, DRV_STANDERD_ID, DRV_DATA_FRAME, DRV_DLC_BYTES_5, data);
    #else
    return user_fdcan_t.send(SuperCapId, DRV_STANDERD_ID, DRV_DATA_FRAME, DRV_DLC_BYTES_8, data);
    #endif
}

#if USE_BMS_CMD
extern BMS_Handle_t bms_handle;
s32 fdcan_filter_init(void)
{
    return user_fdcan_t.add_id_to_filter(DRV_STANDERD_ID, DRV_FILTER_DUAL, MasterControlId, MasterInitId, DRV_FILTER_TO_RXFIFO0);
}

static s32 SuperCap_SendMsg_Uninit(void)
{
  uint8_t txdata[5] = {0};
  txdata[4] = bms_handle.status;  
  return fdcan_send((uint8_t *)&txdata);
}

static s32 SuperCap_SendMsg_PowerMeter(void)
{
  uint8_t txdata[5] = {0};
  float Chassis_Power = supcap.ele.inp_vol * supcap.ele.inp_cur;
  memcpy(&txdata[0], &Chassis_Power, sizeof(float));
  txdata[4] = bms_handle.status;  
  return fdcan_send((uint8_t *)&txdata) == HAL_OK;
}

static s32 SuperCap_SendMsg_PowerControl(void)
{
  uint8_t txdata[5] = {0};
  float SuperCap_Power = supcap.ele.cap_vol * supcap.ele.cap_cur;
  memcpy(&txdata[0], &SuperCap_Power, sizeof(float));
  txdata[4] = bms_handle.status;
  return fdcan_send((uint8_t *)&txdata) == HAL_OK;
}

s32 SuperCap_SendCanMsg(void)
{
  SendCanMsgFunc can_msg[3] = {SuperCap_SendMsg_Uninit,SuperCap_SendMsg_PowerMeter,SuperCap_SendMsg_PowerControl};
  return can_msg[bms_handle.status]();
}

void fdcan_rx_callback_func(void)
{
    if (user_fdcan_t.rx_typedef.id_type == DRV_STANDERD_ID && user_fdcan_t.rx_typedef.id == MasterInitId)
    {
      switch (user_fdcan_t.rx_data[0])
		  {
		    case MasterDeInit : bms_handle.status = UNINIT;
		    break;
		    case MasterPowerMeter: bms_handle.status = POWER_METER;
		    break;
		    case MasterSupCapCtr : bms_handle.status = POWER_CONTROL;
		    break;
		  }
    }else if (user_fdcan_t.rx_typedef.id_type == DRV_STANDERD_ID && user_fdcan_t.rx_typedef.id == MasterControlId)
    {
      float available_power = 0.0f;
      memcpy(&available_power, (uint8_t *)&user_fdcan_t.rx_data[0], sizeof(float));
      if(available_power > MAX_CHARGE_POWER)
      {
        bms_handle.Available_Power = 0.0f;
				bms_handle.control_sign    = false;
      }else if((available_power >= 0 && available_power < MAX_CHARGE_POWER) || (available_power < 0 && available_power > MAX_DISCHARGE_POWER))
	    {
	    	bms_handle.Available_Power  = available_power;
	    	bms_handle.control_sign     = true;
	    }else if(available_power <= MAX_DISCHARGE_POWER)
	    {
	    	bms_handle.Available_Power = MAX_DISCHARGE_POWER;
	    	bms_handle.control_sign    = true;
	    }
    }
}
#else

s32 SuperCap_SendCanMsg(void)
{
    // 非BMS模式下只发送不接收
    u8 txdata[8] = {0};
		taskENTER_CRITICAL();
    float battery_power = supcap.ele.inp_vol * supcap.ele.inp_cur;
    float load_power = supcap.ele.inp_vol * supcap.ele.oup_cur;
		taskEXIT_CRITICAL();
    memcpy(&txdata[0], &battery_power, sizeof(float));
    memcpy(&txdata[4], &load_power, sizeof(float));
    return fdcan_send(txdata);
}

#endif
/*
*************************************************************************************************************************
*                                               GLOBAL FUNCTION PROTOTYPES
*************************************************************************************************************************
*/

s32 fdcan_init(void)
{
    #if USE_BMS_CMD
    fdcan_filter_init();
    user_fdcan_t.rx_callback(fdcan_rx_callback_func);
    #endif
    return user_fdcan_t.start();
}

