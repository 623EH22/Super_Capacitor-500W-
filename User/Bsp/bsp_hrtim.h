#ifndef BSP_HRTIM_H
#define BSP_HRTIM_H
#include "config.h"

typedef struct 
{
    void (*init)(void);
    void (*deinit)(void);
    void (*start)(void);
    void (*stop)(void);
    void (*set_compare)(u16 compare);
}hrtim_t;


extern hrtim_t user_hrtim;



#endif /* BSP_HRTIM_H */