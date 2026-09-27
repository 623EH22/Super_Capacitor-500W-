/*
*************************************************************************************************************************
*                                                       HAL_SUPERCAP
*                             HAL module for the 500W Super-Capacitor Digital Power Supply.
* Filename      : SuperCap.c
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
#define DEF_HAL_SUPERCAP
#include "hal_supercap.h"
#include "drv_analog.h"
#include "drv_hrtim.h"
#include "PID.h"
#include "hal_usart.h"
#include "stm32g4xx.h"
#include "hal_bms_cmd.h"
#include "hal_fdcan.h"

/*
*************************************************************************************************************************
*                                                    PRIVATE DEFINES
*************************************************************************************************************************
*/

#define MEASURE
#define CHARGE_EFFICIENCY    0.90f
#define DISCHARGE_EFFICIENCY 1.005f
#define CUR_KP               350.0f
#define CUR_KI               12.0f
#define CUR_KD               0.0f
#define MAX_INPUT_VOLTAGE    28.0f
#define MIN_INPUT_VOLTAGE    19.0f
#define MAX_CAP_VOL          15.20f
#define MIN_CAP_VOL          2.0f
#define CAP_CURR_DEADZONE    0.2f
#define CAP_VOL_DEADZONE     0.2f
#define MAX_COMPARE          53760
#define INPUT_VOLTAGE        24.0f
#define MAX_DUTY             MAX_COMPARE *MAX_CAP_VOL / INPUT_VOLTAGE
#define MIN_DUTY             MAX_COMPARE *MIN_CAP_VOL / INPUT_VOLTAGE
#define POWER_RING_REPEAT    10
#define VOL_RING_REPEAT      5
#define RETAINED_POWER       5.0f
#define CAP_VOLTAGE_FACTOR   10.0f
#define INP_VOLTAGE_FACTOR   10.0f
#define OUT_CURR_FACTOR      100.0f
#define INP_CURR_FACTOR      100.0f
#define CAP_CURR_FACTOR      100.0f
#define CAP_VOLTAGE_OFFSET   0.0f
#define INP_VOLTAGE_OFFSET   0.0f
#define OUT_CURR_OFFSET      1.65f
#define INP_CURR_OFFSET      1.65f
#define CAP_CURR_OFFSET      1.65f
/*需要修改的参数*/
#define CAP_VOLTAGE_ERR_COEFF  1.0028f
#define INP_VOLTAGE_ERR_COEFF  0.9950f
#define OUT_CURR_ERR_COEFF     1.23f
#define INP_CURR_ERR_COEFF     1.7321f
#define CAP_CURR_ERR_COEFF     1.10885f
#define CAP_VOLTAGE_ERR_OFFSET 0.0185f
#define INP_VOLTAGE_ERR_OFFSET 0.0299f
#define OUT_CURR_ERR_OFFSET    0.5744f
#define INP_CURR_ERR_OFFSET    0.7599f
#define CAP_CURR_ERR_OFFSET    1.11661f
#if USE_BMS_CMD
hal_bms_handle_t bms_handle;
#endif

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

extern analog_t    user_analog_t;
extern hrtim_t     user_hrtim;
extern uart_data_t uart_data;
Pid_t              cur_pid;
volatile uint32_t  run_time_start;
volatile uint32_t  run_time_end;
volatile uint32_t  run_time_diff;
volatile uint32_t  run_freq;
volatile uint32_t  last_run_freq;
volatile uint32_t  run_freq_diff;
u16                shutdown = 0;
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

hal_supercap_handle_t supcap = { 0 };
void                  hal_supercap_control(void);
void                  hal_supercap_calc(void);

bool                  hal_supercap_init(void)
{
    float cur_kpid[7] = { CUR_KP, CUR_KI, CUR_KD, MAX_DUTY, -MAX_DUTY, MAX_DUTY, MIN_DUTY };
    if (!PID_Handle_Init(&cur_pid, cur_kpid, Inc_Pid))
        return false;
#if USE_BMS_CMD
    hal_bms_cmd_init(&bms_handle);
#endif
    supcap.iref        = 0.0f;
    supcap.tick        = VOL_RING_REPEAT;
    supcap.pwm_val     = 0;

    supcap.OpenPwmFlag = false;
    user_analog_t.init();
    user_analog_t.register_callback(hal_supercap_control);
    user_hrtim.init();
#ifdef MEASURE
    user_hrtim.start();
#endif

    return true;
}

bool hal_supercap_refresh(void)
{
    user_hrtim.stop();

#if USE_BMS_CMD
    bms_handle.Available_Power = 0.0f;
    bms_handle.control_sign    = false;
#endif
    supcap.OpenPwmFlag = false;
    supcap.iref        = 0.0f;
    supcap.tick        = VOL_RING_REPEAT;
    supcap.pwm_val     = 0;

    if (!PID_Refresh(&cur_pid))
        return false;
    return true;
}

void hal_supercap_control(void)
{
    //    run_time_start = DWT->CYCCNT;
    //    run_freq = DWT->CYCCNT;
    //    run_freq_diff = run_freq - last_run_freq;
    //    last_run_freq = run_freq;

    supcap.ele.cap_vol = (user_analog_t.original_voltage[1] - CAP_VOLTAGE_OFFSET) * CAP_VOLTAGE_FACTOR * CAP_VOLTAGE_ERR_COEFF + CAP_VOLTAGE_ERR_OFFSET;
    supcap.ele.inp_vol = (user_analog_t.original_voltage[2] - INP_VOLTAGE_OFFSET) * INP_VOLTAGE_FACTOR * INP_VOLTAGE_ERR_COEFF + INP_VOLTAGE_ERR_OFFSET;
    supcap.ele.oup_cur = -((user_analog_t.original_voltage[3] - OUT_CURR_OFFSET) * OUT_CURR_FACTOR * OUT_CURR_ERR_COEFF + OUT_CURR_ERR_OFFSET);
    supcap.ele.inp_cur = (user_analog_t.original_voltage[4] - INP_CURR_OFFSET) * INP_CURR_FACTOR * INP_CURR_ERR_COEFF + INP_CURR_ERR_OFFSET;
    supcap.ele.cap_cur = (user_analog_t.original_voltage[5] - CAP_CURR_OFFSET) * CAP_CURR_FACTOR * CAP_CURR_ERR_COEFF + CAP_CURR_ERR_OFFSET;
#ifdef MEASURE
    user_hrtim.set_compare(uart_data.uint16_data);  // 设置一个固定的占空比
#else
    hal_supercap_calc();
#endif

    //	run_time_end = DWT->CYCCNT;
    //	run_time_diff = run_time_end - run_time_start;
}

void hal_supercap_calc(void)
{
    float        max_cap_cur     = 0.0f;
    static float available_power = 0.0f;
#if USE_BMS_CMD
    if ((bms_handle.status != BMS_POWER_CONTROL && supcap.OpenPwmFlag == true) || bms_handle.control_sign == false) {
        hal_supercap_refresh();
        return;
    }
    available_power = bms_handle.Available_Power;
#else
    static u8 pwr_ring = POWER_RING_REPEAT;
    if (pwr_ring == POWER_RING_REPEAT) {
        pwr_ring        = 0;
        available_power = MAX_POWER - (supcap.ele.inp_vol * supcap.ele.oup_cur);
    }
    pwr_ring++;
#endif
    if (supcap.tick == VOL_RING_REPEAT) {
        supcap.tick = 0;
        float cap_v = supcap.ele.cap_vol;
        max_cap_cur = available_power / cap_v;
        if (cap_v <= MIN_CAP_VOL && max_cap_cur < -CAP_CURR_DEADZONE) {
            supcap.iref = 0;
        } else {
            if (max_cap_cur > CAP_CURR_DEADZONE) {
                supcap.iref = (max_cap_cur * CHARGE_EFFICIENCY >= MAX_CHARGE_CURR)
                                ? MAX_CHARGE_CURR
                                : (max_cap_cur * CHARGE_EFFICIENCY);
            } else if (max_cap_cur < -CAP_CURR_DEADZONE) {
                supcap.iref = (max_cap_cur * DISCHARGE_EFFICIENCY <= MAX_DISCHARGE_CURR)
                                ? MAX_DISCHARGE_CURR
                                : (max_cap_cur * DISCHARGE_EFFICIENCY);
            } else {
                supcap.iref = 0;
            }
        }
    }
    supcap.tick++;
    supcap.pwm_val = (u16)PID_Calculate(&cur_pid, supcap.ele.cap_cur, supcap.iref);
    user_hrtim.set_compare(supcap.pwm_val);

    if (supcap.pwm_val > ((supcap.ele.cap_vol - 0.3f) * MAX_COMPARE / supcap.ele.inp_vol) && supcap.OpenPwmFlag == false) {
        user_hrtim.start();
        supcap.OpenPwmFlag = true;
    }

    if (supcap.iref > CAP_CURR_DEADZONE && supcap.ele.cap_cur < CAP_CURR_DEADZONE && supcap.OpenPwmFlag == true) {
        shutdown++;
    } else {
        shutdown = 0;
    }

    if ((supcap.ele.inp_vol > MAX_INPUT_VOLTAGE || supcap.ele.inp_vol < MIN_INPUT_VOLTAGE) || shutdown >= 5000) {
        shutdown = 0;
        hal_supercap_refresh();
    }
}