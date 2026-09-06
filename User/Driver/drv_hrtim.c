/*
*************************************************************************************************************************
*                                                       DRV_HRTIM
*                            Driver module for the 500W Super-Capacitor Digital Power Supply.
* Filename      : drv_hrtim.c
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

#include "drv_hrtim.h"
#include "hrtim.h"

/*
*************************************************************************************************************************
*                                                    PRIVATE DEFINES
*************************************************************************************************************************
*/

#define HRTIMA_PERIOD 53760
#define MIN_COMPARE 0x80

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

/*
*************************************************************************************************************************
*                                               GLOBAL FUNCTION PROTOTYPES
*************************************************************************************************************************
*/

void HRTIM_Init(void);
void HRTIM_DeInit(void);
void HRTIM_Start(void);
void HRTIM_Stop(void);
void HRTIM_SetCompare(u16 compare);

hrtim_t user_hrtim = 
{
    .init = HRTIM_Init,
    .deinit = HRTIM_DeInit,
    .start = HRTIM_Start,
    .stop = HRTIM_Stop,
    .set_compare = HRTIM_SetCompare
};

void HRTIM_Init(void)
{
    HAL_HRTIM_WaveformCounterStart(&hhrtim1, HRTIM_TIMERID_TIMER_A);
}

void HRTIM_DeInit(void)
{
    HAL_HRTIM_WaveformCounterStop(&hhrtim1, HRTIM_TIMERID_TIMER_A);
}

void HRTIM_Start(void)
{
	HAL_GPIO_WritePin(GPIOA,GPIO_PIN_10,GPIO_PIN_SET);
  HAL_HRTIM_WaveformOutputStart(&hhrtim1,HRTIM_OUTPUT_TA1);
	HAL_HRTIM_WaveformOutputStart(&hhrtim1,HRTIM_OUTPUT_TA2);
}

void HRTIM_Stop(void)
{
	HAL_GPIO_WritePin(GPIOA,GPIO_PIN_10,GPIO_PIN_RESET);
  HAL_HRTIM_WaveformOutputStop(&hhrtim1,HRTIM_OUTPUT_TA1);
  HAL_HRTIM_WaveformOutputStop(&hhrtim1,HRTIM_OUTPUT_TA2);
}

void HRTIM_SetCompare(u16 compare)
{
	if(compare > HRTIMA_PERIOD) compare = HRTIMA_PERIOD - MIN_COMPARE;
	else if(compare < MIN_COMPARE) compare = MIN_COMPARE;
  __HAL_HRTIM_SetCompare(&hhrtim1,HRTIM_TIMERINDEX_TIMER_A,HRTIM_COMPAREUNIT_1,compare/2);
	__HAL_HRTIM_SetCompare(&hhrtim1,HRTIM_TIMERINDEX_TIMER_A,HRTIM_COMPAREUNIT_2,(HRTIMA_PERIOD-compare/2));
}