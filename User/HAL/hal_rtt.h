/*
*************************************************************************************************************************
*                                                       HAL_RTT
*              HAL layer: RTT stream registration and sample write.
* Filename      : hal_rtt.h
* Version       : V1.06.00
* Programmer(s) : RuiYuan Lin
*************************************************************************************************************************
* Note(s)       : One write API. p_sample must match that channel's registered
*                 format. Sparse channels are packed compactly.
*                 register_channel() is only valid BEFORE start().
*************************************************************************************************************************
*/

#ifndef HAL_RTT_H
#define HAL_RTT_H

#include "config.h"
#include "drv_rtt.h"

#ifdef DEF_HAL_RTT
#define EXT_HAL_RTT
#else
#define EXT_HAL_RTT extern
#endif

/*
*************************************************************************************************************************
*                                                     PUBLIC DEFINES
*************************************************************************************************************************
*/

#define HAL_RTT_MAX_CH DRV_RTT_MAX_CH

/*
*************************************************************************************************************************
*                                                      PUBLIC TYPES
*************************************************************************************************************************
*/

typedef struct {
    drv_rtt_fmt_t format[HAL_RTT_MAX_CH];
    uint8_t       used[HAL_RTT_MAX_CH];  /* 1 = channel registered */
    uint8_t       ready[HAL_RTT_MAX_CH]; /* 1 = sample written this frame */
    uint8_t       map[HAL_RTT_MAX_CH];   /* frame index -> logical channel */
    uint8_t       n_channels;
    uint16_t      sample_hz;
    uint8_t       adc_bits;
    uint8_t       started;
} hal_rtt_handle_t;

/*
*************************************************************************************************************************
*                                                    PUBLIC VARIABLES
*************************************************************************************************************************
*/

EXT_HAL_RTT hal_rtt_handle_t hal_rtt;

/*
*************************************************************************************************************************
*                                               GLOBAL FUNCTION PROTOTYPES
*************************************************************************************************************************
*/

/**
 * @brief  Register one channel's wire format (sparse allowed).
 * @return 1 = ok, 0 = bad channel / stream already started
 */
uint8_t hal_rtt_register_channel(uint8_t channel, drv_rtt_fmt_t format);

/**
 * @brief  Enable stream. Frame packs only registered channels (ascending).
 *         Format layout is frozen until the next start().
 */
void hal_rtt_start(uint16_t sample_hz, uint8_t adc_bits);

/**
 * @brief  Write ONE sample for a registered channel.
 * @param  channel  logical channel index
 * @param  p_sample pointer to ONE sample in that channel's registered format
 * @return 1 = sample accepted, 0 = rejected (bad arg / not started / not registered)
 * @note   Frame auto-emits when every registered channel has a sample.
 */
uint8_t hal_rtt_write_channel(uint8_t channel, const void *p_sample);

/**
 * @brief  Force-send frame immediately (bypasses decimation).
 *         Unwritten registered channels are zero-filled.
 */
uint8_t hal_rtt_flush(void);

#endif /* HAL_RTT_H */
