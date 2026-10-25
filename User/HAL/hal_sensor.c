/*
*************************************************************************************************************************
*                                                         HAL_SENSOR
*                             HAL module for the 500W Super-Capacitor Digital Power Supply.
* Filename      : hal_sensor.c
* Version       : V1.00.00
* Programmer(s) : RuiYuan Lin
*************************************************************************************************************************
* Note(s)       : TBD
*************************************************************************************************************************
*                                                  MODIFICATION HISTORY
*************************************************************************************************************************
*   Version   Date          Author        Description
*   V1.00.00  2026-10-03    RuiYuan Lin   Initial release
*************************************************************************************************************************
*/

/*
*************************************************************************************************************************
*                                                     INCLUDE FILES
*************************************************************************************************************************
*/
#define DEF_HAL_SENSOR
#include "hal_sensor.h"
#include "opamp.h"
#include <stddef.h>

/*
*************************************************************************************************************************
*                                                    PRIVATE DEFINES
*************************************************************************************************************************
*/
#define CURRENT_SAMPLING_REGISTER 0.0002f
#define CURRENT_INA240_GAIN_RATE  50.0f
#define CURRENT_GAIN_RATE         1 / (CURRENT_SAMPLING_REGISTER * CURRENT_INA240_GAIN_RATE)
#define VOLTAGE_REGISTER_TOP      100000
#define VOLTAGE_REGISTER_BOTTOM   10000
#define VOLTAGE_GAIN_RATE         VOLTAGE_REGISTER_TOP / VOLTAGE_REGISTER_BOTTOM
/*
*************************************************************************************************************************
*                                                     PRIVATE TYPES
*************************************************************************************************************************
*/
typedef struct {
    float gain;   /* raw -> physical gain  (TODO: fill with real divider / shunt / op-amp gain) */
    float offset; /* raw -> physical offset */
} SENSOR_CALIB_T;

/*
*************************************************************************************************************************
*                                                   PRIVATE VARIABLES
*************************************************************************************************************************
*/

/*
*************************************************************************************************************************
*                                                  PUBLIC VARIABLES
*************************************************************************************************************************
*/

/*
*************************************************************************************************************************
*                                               PRIVATE (HELPER) FUNCTIONS
*************************************************************************************************************************
*/

/*
*************************************************************************************************************************
*                                                    PUBLIC API
*************************************************************************************************************************
*/
void Hal_Sensor_Init(void)
{
    HAL_OPAMP_SelfCalibrate(&hopamp1);
    HAL_OPAMP_SelfCalibrate(&hopamp4);
    HAL_OPAMP_Start(&hopamp1);
    HAL_OPAMP_Start(&hopamp4);
    Drv_ADC_Init();
}

void Hal_Ele_Value_Update(HAL_SENSOR_ELE_HANDLE_T *ele_ptr)
{
    DRV_ADC_HANDLE_T *drv_ptr = &drv_adc_hdl;

    ele_ptr->cap_vol          = ((float)drv_ptr->adc_value_raw[1] / 4096.0f) * DEF_ADC_REF_VOLTAGE * VOLTAGE_GAIN_RATE;
    ele_ptr->inp_vol          = ((float)drv_ptr->adc_value_raw[2] / 4096.0f) * DEF_ADC_REF_VOLTAGE * VOLTAGE_GAIN_RATE;
    ele_ptr->oup_cur          = -((((float)drv_ptr->adc_value_raw[3] - 2048) / 4096.0f) * DEF_ADC_REF_VOLTAGE * CURRENT_GAIN_RATE);
    ele_ptr->inp_cur          = (((float)drv_ptr->adc_value_raw[4] - 2048) / 4096.0f) * DEF_ADC_REF_VOLTAGE * CURRENT_GAIN_RATE;
    ele_ptr->cap_cur          = (((float)drv_ptr->adc_value_raw[5] - 2048) / 4096.0f) * DEF_ADC_REF_VOLTAGE * CURRENT_GAIN_RATE;
}
