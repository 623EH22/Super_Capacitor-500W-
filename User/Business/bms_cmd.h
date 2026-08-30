#ifndef BMS_CMD_H
#define BMS_CMD_H

#include "stdbool.h"
#include "config.h"

#define USE_BMS_CMD

#ifdef USE_BMS_CMD

typedef enum
{
    UNINIT = 0,
    POWER_METER,
    POWER_CONTROL
}BMS_Status;

typedef struct
{
    volatile BMS_Status status;
    volatile bool control_sign;
    volatile float Available_Power;
}BMS_Handle_t;

static inline void bms_cmd_init(BMS_Handle_t *bms_handle)
{
    bms_handle->status = UNINIT;
    bms_handle->control_sign = false;
    bms_handle->Available_Power = 0.0f;
}

#endif


#endif