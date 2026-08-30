#ifndef SUPERCAP_H
#define SUPERCAP_H

#include "config.h"
#include "stdbool.h"
		
#define MAX_POWER            100.0f

#define MAX_CHARGE_CURR      40.0f
#define MAX_DISCHARGE_CURR  -45.0f

#ifdef USE_BMS_CMD
#define MAX_DISCHARGE_POWER  -450.0f
#endif

#define MAX_CHARGE_POWER	 MAX_POWER - 5.0f

typedef struct 
{
    volatile float inp_vol;
    volatile float cap_vol;
    volatile float inp_cur;
    volatile float oup_cur;
    volatile float cap_cur;
}SuperCap_Ele_t;

typedef struct 
{
    SuperCap_Ele_t ele;
    volatile float iref;
    volatile u8 tick;
    volatile u16 pwm_val;
    volatile bool OpenPwmFlag;
}SuperCap_Handle_t;

extern SuperCap_Handle_t supcap;

bool SuperCap_Init(void);


#endif /* SUPERCAP_H */
