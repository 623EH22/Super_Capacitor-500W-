/*
*************************************************************************************************************************
*                                                       DRV_RTT
*                    Driver layer: J-Link RTT measurement stream (jscope-style).
* Filename      : drv_rtt.h
* Version       : V1.02.00
* Programmer(s) : RuiYuan Lin
*************************************************************************************************************************
* Note(s)       : Layout is locked at register time — frames carry NO format table.
*                 Legacy frame : A5 5A | seq u16 LE | samples (per-channel packed)
*                 Batched frame: A5 5A | seq u16 LE | n u8 | n×samples (1-ch U2 only)
*                 UP0 = text banner, UP1 = binary frames.
*************************************************************************************************************************
*/

#ifndef DRV_RTT_H
#define DRV_RTT_H

#include "config.h"

#ifdef DEF_DRV_RTT
#define EXT_DRV_RTT
#else
#define EXT_DRV_RTT extern
#endif

/*
*************************************************************************************************************************
*                                                     PUBLIC DEFINES
*************************************************************************************************************************
*/

#define DRV_RTT_FRAME_MAGIC0 0xA5U
#define DRV_RTT_FRAME_MAGIC1 0x5AU

#define DRV_RTT_MAX_CH       8U

/* header: magic(2) + seq(2) — format table is NOT in the frame */
#define DRV_RTT_FRAME_HDR 4U

/* Batched mode: single-channel U2 streams pack DRV_RTT_BATCH_N samples per
 * frame (A5 5A | seq u16 | n u8 | n×u16) to amortize the 4-byte header —
 * 50 kHz × 2 B ≈ 98 KiB/s instead of 293 KiB/s (fits the RTT link). */
#define DRV_RTT_BATCH_N 100U

/* magic of the RAM config block the host scans */
#define DRV_RTT_CFG_MAGIC 0x4A534346UL /* "JSCF" little-endian */

/*
*************************************************************************************************************************
*                                                      PUBLIC TYPES
*************************************************************************************************************************
*/

/* Per-channel wire format (jscope-compatible). */
typedef enum {
    DRV_RTT_FMT_I1 = 0, /* int8   */
    DRV_RTT_FMT_U1,     /* uint8  */
    DRV_RTT_FMT_I2,     /* int16  */
    DRV_RTT_FMT_U2,     /* uint16 */
    DRV_RTT_FMT_I4,     /* int32  */
    DRV_RTT_FMT_U4,     /* uint32 */
    DRV_RTT_FMT_F4,     /* float  */
    DRV_RTT_FMT_F8,     /* double */
} drv_rtt_fmt_t;

typedef struct {
    drv_rtt_fmt_t format[DRV_RTT_MAX_CH]; /* per-channel format */
    uint8_t       n_channels;             /* 1..8 */
    uint16_t      sample_hz;              /* ADC sample rate (banner reports rate / decimation) */
    uint16_t      decimation;             /* 1 = every sample is sent */
    uint8_t       adc_bits;               /* 6..16 */
    uint16_t      batch;                  /* samples per frame (1 = legacy one-sample frames) */
} drv_rtt_cfg_t;

/* RAM-resident config + banner — stays valid until MCU power-down */
typedef struct {
    uint32_t magic;                  /* DRV_RTT_CFG_MAGIC */
    uint8_t  version;                /* 1 */
    uint8_t  n_channels;
    uint8_t  adc_bits;
    uint8_t  reserved;
    uint32_t sample_hz;              /* effective stream rate (after decimation) */
    uint8_t  format[DRV_RTT_MAX_CH]; /* drv_rtt_fmt_t per channel */
    uint32_t crc;                    /* xor of first 20 bytes */
    char     banner[128];            /* ASCII: JSFMT:... (host scans this) */
} drv_rtt_cfg_block_t;

/*
*************************************************************************************************************************
*                                               GLOBAL FUNCTION PROTOTYPES
*************************************************************************************************************************
*/

/**
 * @brief  Init RTT stream with a per-channel format table.
 * @param  p_formats  array of n_channels formats
 * @param  n_channels 1..8
 * @param  sample_hz  ADC sample rate in Hz
 * @param  decimation send every N-th sample
 * @param  adc_bits   ADC resolution 6..16
 */
void drv_rtt_init(const drv_rtt_fmt_t *p_formats, uint8_t n_channels, uint16_t sample_hz, uint16_t decimation, uint8_t adc_bits);

/**
 * @brief  Send one sample set.
 * @param  pp_values  array of n_channels pointers, each pointing to one sample
 *                    in that channel's registered format
 * @return 1 = written, 0 = dropped
 */
uint8_t drv_rtt_send(const void *const *pp_values);

/**
 * @brief  Send n_actual channels; missing tail channels are zero-filled.
 *         Honors decimation (1 of N frames goes out).
 */
uint8_t drv_rtt_send_n(const void *const *pp_values, uint8_t n_actual);

/**
 * @brief  Send immediately, bypassing decimation (for flush / last frame).
 */
uint8_t     drv_rtt_send_now(const void *const *pp_values, uint8_t n_actual);

void        drv_rtt_get_cfg(drv_rtt_cfg_t *p_cfg);

/** @brief  Address of the RAM config block (host can read it directly). */
const drv_rtt_cfg_block_t *drv_rtt_get_cfg_block(void);
const char *drv_rtt_format_name(drv_rtt_fmt_t format);
uint8_t     drv_rtt_format_size(drv_rtt_fmt_t format);

#endif /* DRV_RTT_H */