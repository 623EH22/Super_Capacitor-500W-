/*
*************************************************************************************************************************
*                                                       DRV_RTT
*                    Driver layer: J-Link RTT measurement stream (jscope-style).
* Filename      : drv_rtt.c
* Version       : V1.01.00
* Programmer(s) : RuiYuan Lin
*************************************************************************************************************************
* Note(s)       : Per-channel format frame:
*                 [0]=A5 [1]=5A [2]=n_ch [3..4]=seq LE [5..]=fmt[n_ch] then samples
*                 UP buffer 1 MUST be configured — SEGGER_RTT_Init only wires UP0.
*************************************************************************************************************************
*/

#define DEF_DRV_RTT
#include "drv_rtt.h"
#include "SEGGER_RTT.h"
#include "stm32g4xx.h"
#include <string.h>
#include <stdarg.h>

/*
*************************************************************************************************************************
*                                                    PRIVATE DEFINES
*************************************************************************************************************************
*/

#define DRV_RTT_CH_TERMINAL   0U
#define DRV_RTT_CH_DATA       1U

#define DRV_RTT_DATA_BUF_SIZE 8192U

/* scratch: header + 8 * 8-byte samples */
#define DRV_RTT_FRAME_MAX (4U + DRV_RTT_MAX_CH * 8U)

/*
*************************************************************************************************************************
*                                                    PRIVATE VARIABLES
*************************************************************************************************************************
*/

static drv_rtt_cfg_t drv_rtt_cfg;
static uint16_t      drv_rtt_seq;
static uint8_t       drv_rtt_inited;
static uint16_t      drv_rtt_decim_cnt;

static uint8_t       drv_rtt_up1_buf[DRV_RTT_DATA_BUF_SIZE] __attribute__((section("RTT_RAM")));

/* Batched-mode accumulator: A5 5A | seq u16 | n u8 | n×u16 (1-ch U2 only) */
#define DRV_RTT_BATCH_HDR 5U
static uint8_t  drv_rtt_batch_buf[DRV_RTT_BATCH_HDR + 2U * DRV_RTT_BATCH_N];
static uint16_t drv_rtt_batch_cnt;

/* Config block — also in RTT_RAM so host can find it near the RTT CB */
drv_rtt_cfg_block_t drv_rtt_cfg_block __attribute__((section("RTT_RAM")));
static uint8_t      drv_rtt_frame[DRV_RTT_FRAME_MAX];

/*
*************************************************************************************************************************
*                                               PRIVATE (HELPER) FUNCTIONS
*************************************************************************************************************************
*/

uint8_t drv_rtt_format_size(drv_rtt_fmt_t format)
{
    switch (format) {
        case DRV_RTT_FMT_I1:
        case DRV_RTT_FMT_U1:
            return 1U;
        case DRV_RTT_FMT_I2:
        case DRV_RTT_FMT_U2:
            return 2U;
        case DRV_RTT_FMT_I4:
        case DRV_RTT_FMT_U4:
        case DRV_RTT_FMT_F4:
            return 4U;
        case DRV_RTT_FMT_F8:
            return 8U;
        default:
            return 4U;
    }
}

const char *drv_rtt_format_name(drv_rtt_fmt_t format)
{
    switch (format) {
        case DRV_RTT_FMT_I1:
            return "jscope_i1";
        case DRV_RTT_FMT_U1:
            return "jscope_u1";
        case DRV_RTT_FMT_I2:
            return "jscope_i2";
        case DRV_RTT_FMT_U2:
            return "jscope_u2";
        case DRV_RTT_FMT_I4:
            return "jscope_i4";
        case DRV_RTT_FMT_U4:
            return "jscope_u4";
        case DRV_RTT_FMT_F4:
            return "jscope_f4";
        case DRV_RTT_FMT_F8:
            return "jscope_f8";
        default:
            return "jscope_f4";
    }
}

static void drv_rtt_pack_sample(uint8_t *p_dst, drv_rtt_fmt_t fmt, const void *p_src)
{
    if (p_src == 0) {
        memset(p_dst, 0, drv_rtt_format_size(fmt));
        return;
    }
    memcpy(p_dst, p_src, drv_rtt_format_size(fmt));
}

/* Compose banner into drv_rtt_cfg_block.banner (init-time only). */
static void drv_rtt_build_banner(void)
{
    uint16_t    stream_hz = drv_rtt_cfg.sample_hz;
    char        fmt_list[100];
    uint8_t     i;
    uint8_t     pos = 0;
    const char *p;

    if (drv_rtt_cfg.decimation > 1U) {
        stream_hz = (uint16_t)(drv_rtt_cfg.sample_hz / drv_rtt_cfg.decimation);
    }

    fmt_list[0] = '\0';
    for (i = 0U; i < drv_rtt_cfg.n_channels && pos < sizeof(fmt_list) - 12U; i++) {
        const char *name = drv_rtt_format_name(drv_rtt_cfg.format[i]);
        if (i > 0U) {
            fmt_list[pos++] = ',';
        }
        while (*name != '\0' && pos < sizeof(fmt_list) - 1U) {
            fmt_list[pos++] = *name++;
        }
        fmt_list[pos] = '\0';
    }

    /* JSFMT:<fmts>,NCH:n,SAMPLE_HZ:h,ADC_BITS:b,VREF:3.300\n */
    pos = 0U;
    p   = "JSFMT:";
    while (*p && pos < sizeof(drv_rtt_cfg_block.banner) - 1U) {
        drv_rtt_cfg_block.banner[pos++] = *p++;
    }
    p = fmt_list;
    while (*p && pos < sizeof(drv_rtt_cfg_block.banner) - 1U) {
        drv_rtt_cfg_block.banner[pos++] = *p++;
    }

    {
        char     tmp[12];
        unsigned v;
        int      n;

        p = ",NCH:";
        while (*p && pos < sizeof(drv_rtt_cfg_block.banner) - 1U) {
            drv_rtt_cfg_block.banner[pos++] = *p++;
        }
        v = (unsigned)drv_rtt_cfg.n_channels;
        n = 0;
        if (v == 0U) {
            tmp[n++] = '0';
        }
        while (v > 0U && n < 12) {
            tmp[n++] = (char)('0' + (v % 10U));
            v /= 10U;
        }
        while (n > 0 && pos < sizeof(drv_rtt_cfg_block.banner) - 1U) {
            drv_rtt_cfg_block.banner[pos++] = tmp[--n];
        }

        p = ",SAMPLE_HZ:";
        while (*p && pos < sizeof(drv_rtt_cfg_block.banner) - 1U) {
            drv_rtt_cfg_block.banner[pos++] = *p++;
        }
        v = (unsigned)stream_hz;
        n = 0;
        if (v == 0U) {
            tmp[n++] = '0';
        }
        while (v > 0U && n < 12) {
            tmp[n++] = (char)('0' + (v % 10U));
            v /= 10U;
        }
        while (n > 0 && pos < sizeof(drv_rtt_cfg_block.banner) - 1U) {
            drv_rtt_cfg_block.banner[pos++] = tmp[--n];
        }

        p = ",ADC_BITS:";
        while (*p && pos < sizeof(drv_rtt_cfg_block.banner) - 1U) {
            drv_rtt_cfg_block.banner[pos++] = *p++;
        }
        v = (unsigned)drv_rtt_cfg.adc_bits;
        n = 0;
        if (v == 0U) {
            tmp[n++] = '0';
        }
        while (v > 0U && n < 12) {
            tmp[n++] = (char)('0' + (v % 10U));
            v /= 10U;
        }
        while (n > 0 && pos < sizeof(drv_rtt_cfg_block.banner) - 1U) {
            drv_rtt_cfg_block.banner[pos++] = tmp[--n];
        }

        p = ",VREF:3.300";
        while (*p && pos < sizeof(drv_rtt_cfg_block.banner) - 1U) {
            drv_rtt_cfg_block.banner[pos++] = *p++;
        }
        if (drv_rtt_cfg.batch > 1U) {
            p = ",BATCH:";
            while (*p && pos < sizeof(drv_rtt_cfg_block.banner) - 1U) {
                drv_rtt_cfg_block.banner[pos++] = *p++;
            }
            v = (unsigned)drv_rtt_cfg.batch;
            n = 0;
            if (v == 0U) {
                tmp[n++] = '0';
            }
            while (v > 0U && n < 12) {
                tmp[n++] = (char)('0' + (v % 10U));
                v /= 10U;
            }
            while (n > 0 && pos < sizeof(drv_rtt_cfg_block.banner) - 1U) {
                drv_rtt_cfg_block.banner[pos++] = tmp[--n];
            }
        }
        p = "\n";
        while (*p && pos < sizeof(drv_rtt_cfg_block.banner) - 1U) {
            drv_rtt_cfg_block.banner[pos++] = *p++;
        }
    }
    drv_rtt_cfg_block.banner[pos] = '\0';
}

static void drv_rtt_send_banner(void)
{
    /* cheap write of prebuilt string — safe from ISR context */
    SEGGER_RTT_WriteString(DRV_RTT_CH_TERMINAL, drv_rtt_cfg_block.banner);
}

static uint8_t drv_rtt_build_and_write(const void *const *pp_values, uint8_t n_actual)
{
    uint8_t  n_ch;
    uint8_t  i;
    uint8_t *p_out;
    uint32_t written;
    uint32_t frame_len;

    n_ch  = drv_rtt_cfg.n_channels;
    p_out = drv_rtt_frame;

    /* Layout is locked at register time — no per-frame format table. */
    p_out[0] = DRV_RTT_FRAME_MAGIC0;
    p_out[1] = DRV_RTT_FRAME_MAGIC1;
    p_out[2] = (uint8_t)(drv_rtt_seq & 0xFFU);
    p_out[3] = (uint8_t)((drv_rtt_seq >> 8) & 0xFFU);
    p_out += 4U;

    /* samples */
    for (i = 0U; i < n_ch; i++) {
        const void *p_src = 0;
        if (pp_values != 0 && i < n_actual) {
            p_src = pp_values[i];
        }
        drv_rtt_pack_sample(p_out, drv_rtt_cfg.format[i], p_src);
        p_out += drv_rtt_format_size(drv_rtt_cfg.format[i]);
    }

    frame_len = (uint32_t)(p_out - drv_rtt_frame);
    written   = SEGGER_RTT_Write(DRV_RTT_CH_DATA, drv_rtt_frame, (unsigned)frame_len);

    drv_rtt_seq++;
    return (written == frame_len) ? 1U : 0U;
}

/* Batched mode: accumulate N single-channel U2 samples, then emit one frame.
 * Amortizes the 4-byte header over N samples so 50 kHz costs ~98 KiB/s
 * instead of 293 KiB/s (fits the J-Link RTT delivery ceiling). */
static uint8_t drv_rtt_batch_push(const void *p_sample)
{
    uint32_t written;
    uint32_t frame_len;

    if (p_sample == 0) {
        return 0U;
    }
    memcpy(&drv_rtt_batch_buf[DRV_RTT_BATCH_HDR + 2U * drv_rtt_batch_cnt], p_sample, 2U);
    drv_rtt_batch_cnt++;
    if (drv_rtt_batch_cnt < DRV_RTT_BATCH_N) {
        return 1U; /* accumulated; frame goes out when full */
    }

    drv_rtt_batch_buf[0] = DRV_RTT_FRAME_MAGIC0;
    drv_rtt_batch_buf[1] = DRV_RTT_FRAME_MAGIC1;
    drv_rtt_batch_buf[2] = (uint8_t)(drv_rtt_seq & 0xFFU);
    drv_rtt_batch_buf[3] = (uint8_t)((drv_rtt_seq >> 8) & 0xFFU);
    drv_rtt_batch_buf[4] = (uint8_t)DRV_RTT_BATCH_N;
    frame_len            = (uint32_t)(DRV_RTT_BATCH_HDR + 2U * DRV_RTT_BATCH_N);
    written              = SEGGER_RTT_Write(DRV_RTT_CH_DATA, drv_rtt_batch_buf, (unsigned)frame_len);
    drv_rtt_seq++;
    drv_rtt_batch_cnt = 0U;
    return (written == frame_len) ? 1U : 0U;
}

/*
*************************************************************************************************************************
*                                               GLOBAL FUNCTION PROTOTYPES
*************************************************************************************************************************
*/

void drv_rtt_init(const drv_rtt_fmt_t *p_formats, uint8_t n_channels, uint16_t sample_hz, uint16_t decimation, uint8_t adc_bits)
{
    uint8_t i;

    if (n_channels == 0U) {
        n_channels = 1U;
    }
    if (n_channels > DRV_RTT_MAX_CH) {
        n_channels = DRV_RTT_MAX_CH;
    }
    if (decimation == 0U) {
        decimation = 1U;
    }
    if (adc_bits < 6U || adc_bits > 16U) {
        adc_bits = 12U;
    }

    if (!drv_rtt_inited) {
        SEGGER_RTT_Init();
        SEGGER_RTT_ConfigUpBuffer(DRV_RTT_CH_DATA,
                                  "Data",
                                  drv_rtt_up1_buf,
                                  sizeof(drv_rtt_up1_buf),
                                  SEGGER_RTT_MODE_NO_BLOCK_SKIP);
    }
    /* Always reset seq on (re)start so host banner resync is unambiguous. */
    drv_rtt_seq = 0U;

    for (i = 0U; i < DRV_RTT_MAX_CH; i++) {
        if (p_formats != 0 && i < n_channels) {
            drv_rtt_cfg.format[i] = p_formats[i];
        } else {
            drv_rtt_cfg.format[i] = DRV_RTT_FMT_F4;
        }
    }
    drv_rtt_cfg.n_channels = n_channels;
    drv_rtt_cfg.sample_hz  = sample_hz;
    drv_rtt_cfg.decimation = decimation;
    drv_rtt_cfg.adc_bits   = adc_bits;
    /* batched framing auto-enables for single-channel U2 streams */
    drv_rtt_cfg.batch = (n_channels == 1U && drv_rtt_cfg.format[0] == DRV_RTT_FMT_U2)
                          ? DRV_RTT_BATCH_N
                          : 1U;
    drv_rtt_batch_cnt = 0U;
    drv_rtt_decim_cnt = 0U;
    drv_rtt_inited    = 1U;

    /* publish layout to RAM for late-connecting hosts */
    {
        uint32_t x = 0U;
        uint8_t *pb;

        drv_rtt_cfg_block.magic      = DRV_RTT_CFG_MAGIC;
        drv_rtt_cfg_block.version    = 1U;
        drv_rtt_cfg_block.n_channels = n_channels;
        drv_rtt_cfg_block.adc_bits   = adc_bits;
        drv_rtt_cfg_block.reserved   = 0U;
        drv_rtt_cfg_block.sample_hz =
            (decimation > 1U) ? (uint32_t)(sample_hz / decimation) : (uint32_t)sample_hz;
        for (i = 0U; i < DRV_RTT_MAX_CH; i++) {
            drv_rtt_cfg_block.format[i] = (uint8_t)drv_rtt_cfg.format[i];
        }
        pb = (uint8_t *)&drv_rtt_cfg_block;
        for (i = 0U; i < 20U; i++) { /* CRC covers header only, not banner[] */
            x ^= pb[i];
        }
        drv_rtt_cfg_block.crc = x;
    }

    drv_rtt_build_banner();
    drv_rtt_send_banner();
}

void drv_rtt_get_cfg(drv_rtt_cfg_t *p_cfg)
{
    if (p_cfg) {
        *p_cfg = drv_rtt_cfg;
    }
}

const drv_rtt_cfg_block_t *drv_rtt_get_cfg_block(void)
{
    return &drv_rtt_cfg_block;
}

uint8_t drv_rtt_send_n(const void *const *pp_values, uint8_t n_actual)
{
    uint32_t primask;
    uint8_t  ok;

    if (!drv_rtt_inited) {
        return 0U;
    }

    primask = __get_PRIMASK();
    __disable_irq();

    drv_rtt_decim_cnt++;
    if (drv_rtt_decim_cnt < drv_rtt_cfg.decimation) {
        __set_PRIMASK(primask);
        return 0U;
    }
    drv_rtt_decim_cnt = 0U;

    if (drv_rtt_cfg.batch > 1U && n_actual == 1U) {
        ok = drv_rtt_batch_push(pp_values[0]);
        __set_PRIMASK(primask);
        return ok;
    }

    ok = drv_rtt_build_and_write(pp_values, n_actual);
    __set_PRIMASK(primask);
    return ok;
}

uint8_t drv_rtt_send_now(const void *const *pp_values, uint8_t n_actual)
{
    uint32_t primask;
    uint8_t  ok;

    if (!drv_rtt_inited) {
        return 0U;
    }
    primask = __get_PRIMASK();
    __disable_irq();
    ok = drv_rtt_build_and_write(pp_values, n_actual);
    __set_PRIMASK(primask);
    return ok;
}

uint8_t drv_rtt_send(const void *const *pp_values)
{
    return drv_rtt_send_n(pp_values, drv_rtt_cfg.n_channels);
}
