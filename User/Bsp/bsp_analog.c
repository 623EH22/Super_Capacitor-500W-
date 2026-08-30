#include "bsp_analog.h"
#include "opamp.h"
#include "adc.h"

#define OVERSAMPLING
//#define OVERSAMPLING_RIGHT_SHIFT

#ifdef OVERSAMPLING
    #define OVERSAMPLING_RATIO 2
#else
    #define OVERSAMPLING_RATIO 1
#endif

#ifndef OVERSAMPLING_RIGHT_SHIFT
    #define RIGHT_SHIFT 0
#else
    #define RIGHT_SHIFT 1
#endif

#define ADC_REF_VOLTAGE 3.30f
#define ADC_MAX_VALUE ((4095 * OVERSAMPLING_RATIO) >> RIGHT_SHIFT)
#define DIV_ADC_MAX_VALUE 1/ADC_MAX_VALUE

//#define ADC1_BUFFER_SIZE 2
//#define ADC2_BUFFER_SIZE 3
//uint16_t adc1_buffer[ADC1_BUFFER_SIZE];
//uint16_t adc2_buffer[ADC2_BUFFER_SIZE];

#define VREF_OFFSET 0.02f

#define ADC_DUAL_BUFFER_SIZE 3
uint32_t adc_dual_buffer[ADC_DUAL_BUFFER_SIZE];

adc_rx_callback user_adc_rx = NULL;

void analog_init(void);
void user_adc_register_callback(adc_rx_callback callback);

analog_t user_analog_t = 
{
    .init = analog_init,
    .register_callback = user_adc_register_callback,
    .original_voltage = {0},
//    .filtered_voltage = {0}
};

uint16_t vrefint_cal;

void analog_init(void)
{
    HAL_OPAMP_SelfCalibrate(&hopamp1);
    HAL_OPAMP_SelfCalibrate(&hopamp4);
    HAL_OPAMP_Start(&hopamp1);
    HAL_OPAMP_Start(&hopamp4);
    HAL_ADCEx_Calibration_Start(&hadc1, ADC_SINGLE_ENDED);
    HAL_ADCEx_Calibration_Start(&hadc2, ADC_SINGLE_ENDED);
//    HAL_ADC_Start_DMA(&hadc1, (uint32_t*)adc1_buffer, ADC1_BUFFER_SIZE);
//    HAL_ADC_Start_DMA(&hadc2, (uint32_t*)adc2_buffer, ADC2_BUFFER_SIZE);
    HAL_ADCEx_MultiModeStart_DMA(&hadc1,adc_dual_buffer, ADC_DUAL_BUFFER_SIZE);
	  vrefint_cal = *(__IO uint16_t *)(0x1FFF75AA); 
}

typedef struct {
    float x[3]; // x[n], x[n-1], x[n-2]
    float y[3]; // y[n], y[n-1], y[n-2]
} LPF2_Filter_t;


LPF2_Filter_t v_filter[6]; 

float LPF2_Apply(LPF2_Filter_t *f, float input) {
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


    f->x[2] = f->x[1];
    f->x[1] = f->x[0];
    f->x[0] = input;

    float out = b0 * f->x[0] + b1 * f->x[1] + b2 * f->x[2]
                - a1 * f->y[1] - a2 * f->y[2];

    f->y[2] = f->y[1];
    f->y[1] = out;
    f->y[0] = out;

    return out;
}

//#define filter_alpha 0.8f
 
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc)
{
    if(hadc->Instance == ADC1)
    { 
				user_analog_t.original_voltage[0] = 3.0f * vrefint_cal/ ((float)(adc_dual_buffer[0] & 0xFFFF) / ((float)(OVERSAMPLING_RATIO >> RIGHT_SHIFT))) + VREF_OFFSET;
        user_analog_t.original_voltage[1] = (float)(adc_dual_buffer[1] & 0xFFFF)       * 3.30f * DIV_ADC_MAX_VALUE;
        user_analog_t.original_voltage[2] = (float)(adc_dual_buffer[2] & 0xFFFF)       * 3.30f * DIV_ADC_MAX_VALUE;
        user_analog_t.original_voltage[3] = (float)(adc_dual_buffer[0] >> 16 & 0xFFFF) * 3.30f * DIV_ADC_MAX_VALUE;
        user_analog_t.original_voltage[4] = (float)(adc_dual_buffer[1] >> 16 & 0xFFFF) * 3.30f * DIV_ADC_MAX_VALUE;
        user_analog_t.original_voltage[5] = (float)(adc_dual_buffer[2] >> 16 & 0xFFFF) * 3.30f * DIV_ADC_MAX_VALUE;
			
//				user_analog_t.filtered_voltage[5] = LPF2_Apply(&v_filter[3],user_analog_t.original_voltage[5]);

//        user_analog_t.filtered_voltage[0] = user_analog_t.original_voltage[0] * filter_alpha + user_analog_t.last_voltage[0] * (1 - filter_alpha);
//        user_analog_t.filtered_voltage[1] = user_analog_t.original_voltage[1] * filter_alpha + user_analog_t.last_voltage[1] * (1 - filter_alpha);
//        user_analog_t.filtered_voltage[2] = user_analog_t.original_voltage[2] * filter_alpha + user_analog_t.last_voltage[2] * (1 - filter_alpha);
//        user_analog_t.filtered_voltage[3] = user_analog_t.original_voltage[3] * filter_alpha + user_analog_t.last_voltage[3] * (1 - filter_alpha);
//        user_analog_t.filtered_voltage[4] = user_analog_t.original_voltage[4] * filter_alpha + user_analog_t.last_voltage[4] * (1 - filter_alpha);
//        user_analog_t.filtered_voltage[5] = user_analog_t.original_voltage[5] * filter_alpha + user_analog_t.last_voltage[5] * (1 - filter_alpha);
//				
//				user_analog_t.last_voltage[0] = user_analog_t.filtered_voltage[0];
//        user_analog_t.last_voltage[1] = user_analog_t.filtered_voltage[1];
//        user_analog_t.last_voltage[2] = user_analog_t.filtered_voltage[2];
//        user_analog_t.last_voltage[3] = user_analog_t.filtered_voltage[3];
//        user_analog_t.last_voltage[4] = user_analog_t.filtered_voltage[4];
//        user_analog_t.last_voltage[5] = user_analog_t.filtered_voltage[5];
				
        if(user_adc_rx != NULL)
        {
            user_adc_rx();
        }
    }
}

void user_adc_register_callback(adc_rx_callback callback)
{
    user_adc_rx = callback;
}