/*
*************************************************************************************************************************
*                                                       HAL_RTT
*              HAL layer: RTT stream registration and sample write.
* Filename      : hal_rtt.c
* Version       : V1.06.00
* Programmer(s) : RuiYuan Lin
*************************************************************************************************************************
* Note(s)       : Single write API. register_channel() only before start().
*                 write / flush / emit are IRQ-safe (PRIMASK).
*************************************************************************************************************************
*/

#define DEF_HAL_RTT
#include "hal_rtt.h"
#include "drv_rtt.h"
#include "stm32g4xx.h"
#include <string.h>

/*
*************************************************************************************************************************
*                                                    PRIVATE DEFINES
*************************************************************************************************************************
*/

/* bandwidth guard: send every N-th completed frame (1 = full rate) */
#define HAL_RTT_DECIMATION 1U

/*
*************************************************************************************************************************
*                                                    PRIVATE VARIABLES
*************************************************************************************************************************
*/

hal_rtt_handle_t hal_rtt;

static uint8_t       hal_rtt_slot[HAL_RTT_MAX_CH][8];
static drv_rtt_fmt_t hal_rtt_fmt_compact[HAL_RTT_MAX_CH];

/*
*************************************************************************************************************************
*                                               PRIVATE (HELPER) FUNCTIONS
*************************************************************************************************************************
*/

static uint8_t hal_rtt_all_ready(void)
{
    uint8_t i;

    for (i = 0U; i < hal_rtt.n_channels; i++) {
        if (!hal_rtt.ready[hal_rtt.map[i]]) {
            return 0U;
        }
    }
    return 1U;
}

static void hal_rtt_clear_ready(void)
{
    memset(hal_rtt.ready, 0, sizeof(hal_rtt.ready));
}

/* caller must hold IRQ lock */
static uint8_t hal_rtt_emit_locked(void)
{
    const void *pp[HAL_RTT_MAX_CH];
    uint8_t     i;

    for (i = 0U; i < hal_rtt.n_channels; i++) {
        pp[i] = hal_rtt_slot[hal_rtt.map[i]];
    }
    hal_rtt_clear_ready();
    return drv_rtt_send_n(pp, hal_rtt.n_channels);
}

/*
*************************************************************************************************************************
*                                               GLOBAL FUNCTION PROTOTYPES
*************************************************************************************************************************
*/

uint8_t hal_rtt_register_channel(uint8_t channel, drv_rtt_fmt_t format)
{
    if (channel >= HAL_RTT_MAX_CH) {
        return 0U;
    }
    /* layout is frozen after start — reject to avoid silent map mismatch */
    if (hal_rtt.started) {
        return 0U;
    }

    hal_rtt.format[channel] = format;
    hal_rtt.used[channel]   = 1U;
    hal_rtt.ready[channel]  = 0U;
    return 1U;
}

void hal_rtt_start(uint16_t sample_hz, uint8_t adc_bits)
{
    uint8_t ch;
    uint8_t n;

    if (adc_bits < 6U || adc_bits > 16U) {
        adc_bits = 12U;
    }

    n = 0U;
    for (ch = 0U; ch < HAL_RTT_MAX_CH; ch++) {
        if (hal_rtt.used[ch]) {
            hal_rtt.map[n]         = ch;
            hal_rtt_fmt_compact[n] = hal_rtt.format[ch];
            n++;
        }
    }
    if (n == 0U) {
        hal_rtt.n_channels = 0U;
        hal_rtt.started    = 0U;
        return;
    }

    hal_rtt.n_channels = n;
    hal_rtt.sample_hz  = sample_hz;
    hal_rtt.adc_bits   = adc_bits;
    hal_rtt_clear_ready();

    drv_rtt_init(hal_rtt_fmt_compact, n, sample_hz, HAL_RTT_DECIMATION, adc_bits);
    /* only after drv is inited — otherwise write/emit silently drops */
    hal_rtt.started = 1U;
}

uint8_t hal_rtt_write_channel(uint8_t channel, const void *p_sample)
{
    uint32_t primask;
    uint8_t  size;
    uint8_t  ok;

    if (!hal_rtt.started || p_sample == 0U) {
        return 0U;
    }
    if (channel >= HAL_RTT_MAX_CH || !hal_rtt.used[channel]) {
        return 0U;
    }

    size = drv_rtt_format_size(hal_rtt.format[channel]);

    /* slot + ready + possible emit must not tear vs ISR/task */
    primask = __get_PRIMASK();
    __disable_irq();

    memset(hal_rtt_slot[channel], 0, sizeof(hal_rtt_slot[channel]));
    memcpy(hal_rtt_slot[channel], p_sample, size);
    hal_rtt.ready[channel] = 1U;

    if (hal_rtt_all_ready()) {
        (void)hal_rtt_emit_locked();
    }
    ok = 1U; /* sample accepted */

    __set_PRIMASK(primask);
    return ok;
}

uint8_t hal_rtt_flush(void)
{
    uint32_t primask;
    uint8_t  i;
    uint8_t  ch;
    uint8_t  sent;

    if (!hal_rtt.started) {
        return 0U;
    }

    primask = __get_PRIMASK();
    __disable_irq();

    for (i = 0U; i < hal_rtt.n_channels; i++) {
        ch = hal_rtt.map[i];
        if (!hal_rtt.ready[ch]) {
            memset(hal_rtt_slot[ch], 0, sizeof(hal_rtt_slot[ch]));
        }
    }
    /* flush must bypass decimation — last frame must go out */
    {
        const void *pp[HAL_RTT_MAX_CH];
        for (i = 0U; i < hal_rtt.n_channels; i++) {
            pp[i] = hal_rtt_slot[hal_rtt.map[i]];
        }
        hal_rtt_clear_ready();
        sent = drv_rtt_send_now(pp, hal_rtt.n_channels);
    }

    __set_PRIMASK(primask);
    return sent;
}
