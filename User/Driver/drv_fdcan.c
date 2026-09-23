/*
*************************************************************************************************************************
*                                                       DRV_FDCAN
*                            Driver module for the 500W Super-Capacitor Digital Power Supply.
* Filename      : drv_fdcan.c
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
#define DEF_DRV_FDCAN
#include "drv_fdcan.h"

#if USE_FREERTOS
#include "FreeRTOS.h"
#include "task.h"
#endif

/*
*************************************************************************************************************************
*                                                    PRIVATE DEFINES
*************************************************************************************************************************
*/

#define MAX_FILTER_NUM 14

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

static volatile u8       filter_cursor = 0;
static fdcan_rx_callback user_fdcan_rx = NULL;

/*
*************************************************************************************************************************
*                                                    PUBLIC VARIABLES
*************************************************************************************************************************
*/

/*
*************************************************************************************************************************
*                                               PRIVATE (HELPER) FUNCTIONS
*************************************************************************************************************************
*/

static inline s32                       user_fdcan_start(void);
static inline drv_fdcan_filter_status_t user_fdcan_add_id_to_filter(u32 id_type, u32 filter_type, u32 id1, u32 id2, u32 fifos);
static inline s32                       user_fdcan_send(u32 id, u32 id_type, u32 txframtype, u32 lenth, u8 *pdata);

static inline s32                       user_fdcan_start(void)
{
    return HAL_FDCAN_Start(&USER_FDCAN);
}

static drv_fdcan_filter_status_t fdcan_add_id_to_filter(FDCAN_HandleTypeDef *hfdcan, u32 id_type, u32 filter_type, u32 id1, u32 id2, u32 fifos)
{
#if USE_FREERTOS
    vTaskSuspendAll();
#endif
    FDCAN_FilterTypeDef sFilterConfig;

    u32                 max_id_num = id_type == FDCAN_STANDARD_ID ? 0x7FF : 0x1FFFFFFF;

    if ((id1 >= 0 && id1 <= max_id_num) || (id2 >= 0 && id2 <= max_id_num) || filter_cursor <= MAX_FILTER_NUM) {
        sFilterConfig.FilterIndex  = filter_cursor;
        sFilterConfig.IdType       = id_type;
        sFilterConfig.FilterType   = filter_type;
        sFilterConfig.FilterID1    = id1;
        sFilterConfig.FilterID2    = id2;
        sFilterConfig.FilterConfig = fifos;
        filter_cursor++;
    } else if (filter_cursor >= MAX_FILTER_NUM) {
        return fdcan_filter_full;
    } else {
        return fdcan_filter_invalid;
    }
    HAL_FDCAN_ConfigFilter(hfdcan, &sFilterConfig);
    HAL_FDCAN_ActivateNotification(hfdcan, FDCAN_IT_RX_FIFO0_NEW_MESSAGE, fifos);
#if USE_FREERTOS
    xTaskResumeAll();
#endif
    return fdcan_filter_ok;
}

static inline drv_fdcan_filter_status_t user_fdcan_add_id_to_filter(u32 id_type, u32 filter_type, u32 id1, u32 id2, u32 fifos)
{
    return fdcan_add_id_to_filter(&USER_FDCAN, id_type, filter_type, id1, id2, fifos);
}

static inline s32 hal_fdcan_send(FDCAN_HandleTypeDef *hfdcan, u32 id, u32 id_type, u32 txframtype, u32 lenth, u8 *pdata)
{
    FDCAN_TxHeaderTypeDef sTxHeader;
    sTxHeader.Identifier          = id;
    sTxHeader.IdType              = id_type;
    sTxHeader.TxFrameType         = txframtype;
    sTxHeader.DataLength          = lenth;
    sTxHeader.ErrorStateIndicator = FDCAN_ESI_PASSIVE;
    sTxHeader.BitRateSwitch       = FDCAN_BRS_OFF;
    sTxHeader.FDFormat            = FDCAN_CLASSIC_CAN;
    sTxHeader.TxEventFifoControl  = FDCAN_NO_TX_EVENTS;
    sTxHeader.MessageMarker       = 0;

    return HAL_FDCAN_AddMessageToTxFifoQ(hfdcan, &sTxHeader, pdata);
}

static inline s32 user_fdcan_send(u32 id, u32 id_type, u32 txframtype, u32 lenth, u8 *pdata)
{
    return hal_fdcan_send(&USER_FDCAN, id, id_type, txframtype, lenth, pdata);
}

/*
*************************************************************************************************************************
*                                               GLOBAL FUNCTION PROTOTYPES
*************************************************************************************************************************
*/

void        user_fdcan_register_callback(fdcan_rx_callback callback);

drv_fdcan_t user_fdcan_t = {
    .add_id_to_filter = user_fdcan_add_id_to_filter,
    .start            = user_fdcan_start,
    .send             = user_fdcan_send,
    .rx_callback      = user_fdcan_register_callback,
    .rx_typedef       = { 0 },
    .rx_data          = { 0 }
};

void HAL_FDCAN_RxFifo0Callback(FDCAN_HandleTypeDef *hfdcan, uint32_t RxFifo0ITs)
{
    if (hfdcan->Instance == USER_FDCAN.Instance) {
        if ((RxFifo0ITs & FDCAN_IT_RX_FIFO0_NEW_MESSAGE) != 0) {
            FDCAN_RxHeaderTypeDef sRxHeader;
            HAL_FDCAN_GetRxMessage(hfdcan, FDCAN_RX_FIFO0, &sRxHeader, (uint8_t *)user_fdcan_t.rx_data);
            user_fdcan_t.rx_typedef.id_type    = sRxHeader.IdType;
            user_fdcan_t.rx_typedef.frame_type = sRxHeader.RxFrameType;
            user_fdcan_t.rx_typedef.id         = sRxHeader.Identifier;
            user_fdcan_t.rx_typedef.length     = sRxHeader.DataLength;
            if (user_fdcan_rx != NULL) {
                user_fdcan_rx();
            }
        }
    }
}

void user_fdcan_register_callback(fdcan_rx_callback callback)
{
    user_fdcan_rx = callback;
}