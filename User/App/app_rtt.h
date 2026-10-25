/*
*************************************************************************************************************************
*                                                        APP_RTT
*                             Application module for the 500W Super-Capacitor Digital Power Supply.
* Filename      : app_rtt.h
* Version       : V1.01.00
* Programmer(s) : RuiYuan Lin
*************************************************************************************************************************
* Note(s)       : User-facing API is just TWO functions:
*                   App_RTT_Init();
*                   App_RTT_Write(ch, p_sample);
*************************************************************************************************************************
*                                                  MODIFICATION HISTORY
*************************************************************************************************************************
*   Version   Date          Author        Description
*   V1.00.00  2026-09-29    RuiYuan Lin   Initial release
*   V1.01.00  2026-09-29    RuiYuan Lin   Single write entry
*/

#ifndef APP_RTT_H
#define APP_RTT_H

/*
*************************************************************************************************************************
*                                                     INCLUDE FILES
*************************************************************************************************************************
*/
#include "config.h"

/*
*************************************************************************************************************************
*                                                     EXTERN DEFINES
*************************************************************************************************************************
*/

#ifdef DEF_APP_RTT
#define EXT_APP_RTT
#else
#define EXT_APP_RTT extern
#endif

/*
*************************************************************************************************************************
*                                               GLOBAL FUNCTION PROTOTYPES
*************************************************************************************************************************
*/

/**
 * @brief  Init RTT stream (channel formats + start). Call once at boot.
 */
void App_RTT_Init(void);

/**
 * @brief  Write one sample for one channel.
 * @param  channel  0 .. (channels configured in App_RTT_Init) - 1
 * @param  p_sample pointer to one sample in that channel's wire format
 *                   (see App_RTT_Init table; F4 => const float*)
 * @return 1 = accepted, 0 = rejected
 */
uint8_t App_RTT_Write(uint8_t channel, const void *p_sample);

#endif /* APP_RTT_H */
