/*
*************************************************************************************************************************
*                                                       DRV_FDCAN
*                            Driver module for the 500W Super-Capacitor Digital Power Supply.
* Filename      : drv_fdcan.h
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

#ifndef DRV_FDCAN_H
#define DRV_FDCAN_H

/*
*************************************************************************************************************************
*                                                     INCLUDE FILES
*************************************************************************************************************************
*/

#include "fdcan.h"
#include "config.h"

/*
*************************************************************************************************************************
*                                                     PUBLIC DEFINES
*************************************************************************************************************************
*/

#define DRV_STANDERD_ID          FDCAN_STANDARD_ID
#define DRV_EXTENDED_ID          FDCAN_EXTENDED_ID
#define DRV_FILTER_RANGE         FDCAN_FILTER_RANGE
#define DRV_FILTER_DUAL          FDCAN_FILTER_DUAL
#define DRV_FILTER_MASK          FDCAN_FILTER_MASK
#define DRV_FILTER_RANGE_NO_EIDM FDCAN_FILTER_RANGE_NO_EIDM
#define DRV_FILTER_TO_RXFIFO0    FDCAN_FILTER_TO_RXFIFO0
#define DRV_FILTER_TO_RXFIFO1    FDCAN_FILTER_TO_RXFIFO1
#define DRV_DATA_FRAME           FDCAN_DATA_FRAME
#define DRV_REMOTE_FRAME         FDCAN_REMOTE_FRAME
#define DRV_DLC_BYTES_0          FDCAN_DLC_BYTES_0
#define DRV_DLC_BYTES_1          FDCAN_DLC_BYTES_1
#define DRV_DLC_BYTES_2          FDCAN_DLC_BYTES_2
#define DRV_DLC_BYTES_3          FDCAN_DLC_BYTES_3
#define DRV_DLC_BYTES_4          FDCAN_DLC_BYTES_4
#define DRV_DLC_BYTES_5          FDCAN_DLC_BYTES_5
#define DRV_DLC_BYTES_6          FDCAN_DLC_BYTES_6
#define DRV_DLC_BYTES_7          FDCAN_DLC_BYTES_7
#define DRV_DLC_BYTES_8          FDCAN_DLC_BYTES_8
#define DRV_DLC_BYTES_12         FDCAN_DLC_BYTES_12
#define DRV_DLC_BYTES_16         FDCAN_DLC_BYTES_16
#define DRV_DLC_BYTES_20         FDCAN_DLC_BYTES_20
#define DRV_DLC_BYTES_24         FDCAN_DLC_BYTES_24
#define DRV_DLC_BYTES_32         FDCAN_DLC_BYTES_32
#define DRV_DLC_BYTES_48         FDCAN_DLC_BYTES_48
#define DRV_DLC_BYTES_64         FDCAN_DLC_BYTES_64

/*
*************************************************************************************************************************
*                                                      PUBLIC TYPES
*************************************************************************************************************************
*/

typedef void (*fdcan_rx_callback)(void);

typedef enum {
    fdcan_filter_ok      = 0,
    fdcan_filter_full    = -1,
    fdcan_filter_invalid = -2
} drv_fdcan_filter_status_t;

typedef struct
{
    u32 id_type;
    u32 frame_type;
    u32 id;
    u32 length;
} drv_fdcan_rx_typedef_t;

typedef struct
{
    drv_fdcan_filter_status_t (*add_id_to_filter)(u32 id_type, u32 filter_type, u32 id1, u32 id2, u32 fifos);
    s32 (*start)(void);
    s32 (*send)(u32 id, u32 id_type, u32 txframtype, u32 length, u8 *pdata);
    void (*rx_callback)(fdcan_rx_callback callback);
    volatile drv_fdcan_rx_typedef_t rx_typedef;
    volatile u8                     rx_data[8];
} drv_fdcan_t;

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

/* user_fdcan_t是一个全局变量，包含了FDCAN的相关操作函数和接收数据的结构体定义。
消费者可以通过调用user_fdcan_t中的函数来配置过滤器、启动FDCAN、发送消息以及设置接收回调函数。
同时，消费者也可以通过user_fdcan_t中的rx_typedef和rx_data来获取接收到的消息的相关信息和数据。*/
extern drv_fdcan_t user_fdcan_t;

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

#endif /* DRV_FDCAN_H */