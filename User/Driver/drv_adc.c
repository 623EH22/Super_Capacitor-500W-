/*
*************************************************************************************************************************
*                                                       DRV_ADC
*                            Driver module for the 500W Super-Capacitor Digital Power Supply.
* Filename      : drv_adc.c
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
#define DEF_DRV_ADC
#include "drv_adc.h"
#include "adc.h"
#include "app_rtt.h"
#include "app_rtt.h"

/*
*************************************************************************************************************************
*                                                    PRIVATE DEFINES
*************************************************************************************************************************
*/

// #define OVERSAMPLING_RIGHT_SHIFT 0

#define OVERSAMPLING_RATIO 0

#if !OVERSAMPLING_RIGHT_SHIFT

#define RIGHT_SHIFT 0
#else
#define RIGHT_SHIFT 1
#endif

#define ADC_MAX_VALUE     ((4095 * OVERSAMPLING_RATIO) >> RIGHT_SHIFT)
#define DIV_ADC_MAX_VALUE 1 / ADC_MAX_VALUE
// #define ADC1_BUFFER_SIZE 2
// #define ADC2_BUFFER_SIZE 3
// uint16_t adc1_buffer[ADC1_BUFFER_SIZE];
// uint16_t adc2_buffer[ADC2_BUFFER_SIZE];
#define VREF_OFFSET          0.02f
#define ADC_DUAL_BUFFER_SIZE 3

/*
*************************************************************************************************************************
*                                                     PRIVATE TYPES
*************************************************************************************************************************
*/

typedef struct {
    float x[3];  // x[n], x[n-1], x[n-2]
    float y[3];  // y[n], y[n-1], y[n-2]
} LPF2_Filter_t;

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

uint32_t      adc_dual_buffer[ADC_DUAL_BUFFER_SIZE];
uint16_t      vrefint_cal;
LPF2_Filter_t v_filter[6];

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

DRV_ADC_HANDLE_T drv_adc_hdl;
//= {
//    .callback      = NULL,
//    .adc_value_raw = { 0 }
//};

void Drv_ADC_Init(void)
{
    HAL_ADCEx_Calibration_Start(&hadc1, ADC_SINGLE_ENDED);
    HAL_ADCEx_Calibration_Start(&hadc2, ADC_SINGLE_ENDED);

    /* Read factory VREFINT cal BEFORE starting DMA so the first ConvCplt is valid. */
    vrefint_cal = *(__IO uint16_t *)(0x1FFF75AA);

    //    HAL_ADC_Start_DMA(&hadc1, (uint32_t*)adc1_buffer, ADC1_BUFFER_SIZE);
    //    HAL_ADC_Start_DMA(&hadc2, (uint32_t*)adc2_buffer, ADC2_BUFFER_SIZE);
    /* start RTT BEFORE ADC DMA so first ConvCplt cannot race init */
    HAL_ADCEx_MultiModeStart_DMA(&hadc1, adc_dual_buffer, ADC_DUAL_BUFFER_SIZE);
}

float LPF2_Apply(LPF2_Filter_t *f, float input)
{
    //(1kHz cut-off @ 10kHz sample rate)
    //    const float b0 = 0.067455f;
    //    const float b1 = 0.134911f;
    //    const float b2 = 0.067455f;
    //    const float a1 = -1.142980f;
    //    const float a2 = 0.412802f;
    const float b0 = 0.020083f;
    const float b1 = 0.040167f;
    const float b2 = 0.020083f;
    const float a1 = -1.561018f;
    const float a2 = 0.641352f;

    f->x[2]        = f->x[1];
    f->x[1]        = f->x[0];
    f->x[0]        = input;

    float out      = b0 * f->x[0] + b1 * f->x[1] + b2 * f->x[2] - a1 * f->y[1] - a2 * f->y[2];

    f->y[2]        = f->y[1];
    f->y[1]        = out;
    f->y[0]        = out;

    return out;
}

// #define filter_alpha 0.8f
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
    if (hadc->Instance == ADC1) {
        DRV_ADC_HANDLE_T *ptr = &drv_adc_hdl;

        ptr->adc_value_raw[0] = (adc_dual_buffer[0] & 0xFFFF) >> OVERSAMPLING_RATIO;
        ptr->adc_value_raw[1] = (adc_dual_buffer[1] & 0xFFFF) >> OVERSAMPLING_RATIO;
        ptr->adc_value_raw[2] = (adc_dual_buffer[2] & 0xFFFF) >> OVERSAMPLING_RATIO;
        ptr->adc_value_raw[3] = (adc_dual_buffer[0] >> 16 & 0xFFFF) >> OVERSAMPLING_RATIO;
        ptr->adc_value_raw[4] = (adc_dual_buffer[1] >> 16 & 0xFFFF) >> OVERSAMPLING_RATIO;
        ptr->adc_value_raw[5] = (adc_dual_buffer[2] >> 16 & 0xFFFF) >> OVERSAMPLING_RATIO;

        App_RTT_Write(0, &ptr->adc_value_raw[0]);
        App_RTT_Write(1, &ptr->adc_value_raw[2]);

        //		 user_analog_t.filtered_voltage[5] = LPF2_Apply(&v_filter[3],user_analog_t.original_voltage[5]);

        //        user_analog_t.filtered_voltage[0] = user_analog_t.original_voltage[0] * filter_alpha + user_analog_t.last_voltage[0] * (1 - filter_alpha);
        //        user_analog_t.filtered_voltage[1] = user_analog_t.original_voltage[1] * filter_alpha + user_analog_t.last_voltage[1] * (1 - filter_alpha);
        //        user_analog_t.filtered_voltage[2] = user_analog_t.original_voltage[2] * filter_alpha + user_analog_t.last_voltage[2] * (1 - filter_alpha);
        //        user_analog_t.filtered_voltage[3] = user_analog_t.original_voltage[3] * filter_alpha + user_analog_t.last_voltage[3] * (1 - filter_alpha);
        //        user_analog_t.filtered_voltage[4] = user_analog_t.original_voltage[4] * filter_alpha + user_analog_t.last_voltage[4] * (1 - filter_alpha);
        //        user_analog_t.filtered_voltage[5] = user_analog_t.original_voltage[5] * filter_alpha + user_analog_t.last_voltage[5] * (1 - filter_alpha);
        //
        //		user_analog_t.last_voltage[0] = user_analog_t.filtered_voltage[0];
        //        user_analog_t.last_voltage[1] = user_analog_t.filtered_voltage[1];
        //        user_analog_t.last_voltage[2] = user_analog_t.filtered_voltage[2];
        //        user_analog_t.last_voltage[3] = user_analog_t.filtered_voltage[3];
        //        user_analog_t.last_voltage[4] = user_analog_t.filtered_voltage[4];
        //        user_analog_t.last_voltage[5] = user_analog_t.filtered_voltage[5];

        if (ptr->callback != NULL) {
            ptr->callback();
        }
    }
}