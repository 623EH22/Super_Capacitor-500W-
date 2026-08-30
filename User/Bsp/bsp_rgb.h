#ifndef BSP_RGB_H
#define BSP_RGB_H

#include "gpio.h"

#ifdef RED_Pin
	#define Red(State) 		  HAL_GPIO_WritePin(RED_GPIO_Port,RED_Pin,(GPIO_PinState)State);
#endif                
										  
#ifdef GREEN_Pin      
	#define Green(State)	  HAL_GPIO_WritePin(GREEN_GPIO_Port,GREEN_Pin,(GPIO_PinState)State);
#endif

#ifdef BLUE_Pin
	#define Blue(State)	      HAL_GPIO_WritePin(BLUE_GPIO_Port,BLUE_Pin,(GPIO_PinState)State);
#endif

typedef enum
{
	ON = 0U,
	OFF
}RGB_State;

static inline void RGB_Set(RGB_State r,RGB_State g,RGB_State b)
{
	Red(r);
	Green(g);
	Blue(b); 
}

#endif