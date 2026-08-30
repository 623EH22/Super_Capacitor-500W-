#ifndef BSP_ANALOG_H
#define BSP_ANALOG_H

#include "config.h"

typedef void (*adc_rx_callback)(void);

typedef struct
{
    void (*init)(void);
    void (*register_callback)(adc_rx_callback callback);
    float original_voltage[6]; // 存储原始电压值
//    float filtered_voltage[6]; // 存储滤波后的电压值
}analog_t;

extern analog_t user_analog_t;



#endif /* BSP_ANALOG_H */
