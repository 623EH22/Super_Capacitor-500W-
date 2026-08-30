# 嵌入式 C 语言注释模板（STM32 / FreeRTOS 适用，注释语言：英文）

> 规范风格：Doxygen，兼容 Keil MDK、STM32CubeIDE、VSCode 插件自动生成。
> 编码统一 UTF-8（Keil 中配置 Edit → Configuration → Editor → Encoding = UTF-8）。
> 约定：**所有代码注释一律使用英文**（Doxygen tag、内容均英文），保证跨平台/跨编译器显示一致。

---

## 1. 文件头注释（.c / .h 通用）

```c
/**
  ******************************************************************************
  * @file           : bsp_led.c
  * @brief          : LED driver module (BSP / driver layer)
  * @author         : Rayylen
  * @version        : V1.0.0
  * @date           : 2026-08-30
  ******************************************************************************
  * @attention
  *
  * Project   : 500W Super-Capacitor Digital Power Supply
  * MCU       : STM32G474RET6
  * Toolchain : Keil MDK 5 / STM32CubeMX
  * Hierarchy : App layer -> Service layer -> Driver layer -> HAL
  *
  * Copyright (c) 2026 XXXXX. All rights reserved.
  *
  ******************************************************************************
  * @verbatim Revision History
  * ----------------------------------------------------------------------------
  *   Version   Date          Author      Description
  *   V1.0.0    2026-08-30    Rayylen     Initial release
  *   V1.0.1    2026-09-15    Rayylen     Fix blink period overflow issue
  * @endverbatim
  ******************************************************************************
  */
```

头文件还需配合防重复包含宏：

```c
/**
  ******************************************************************************
  * @file    bsp_led.h
  * @brief   LED driver module public interface (BSP / driver layer)
  ******************************************************************************
  */
/* Include guard -------------------------------------------------------------*/
#ifndef __BSP_LED_H
#define __BSP_LED_H

#ifdef __cplusplus
extern "C" {
#endif

/* Header content */

#ifdef __cplusplus
}
#endif

#endif /* __BSP_LED_H */
```

---

## 2. 代码分层

### 2.1 项目目录分层（自上而下调用，禁止跨层/反向调用）

```
Project/
├── App/                    Application layer: business logic, state machines, FreeRTOS tasks, control strategy
├── Service/                Service layer (middleware): algorithms (PI/filter/calibration), protocol parsing (CAN/UART), parameter management
├── Driver/                 Driver layer: peripheral wrappers (bsp_led.c / bsp_key.c / drv_hrtim.c)
├── Hardware/ 或 Middlewares/  Hardware abstraction layer / third-party libraries (FreeRTOS, FatFs)
└── Drivers/ST/             ST official HAL library (must not be modified)
```

分层规则：

| 规则 | 说明 |
|------|------|
| 单向依赖 | App → Service → Driver → HAL，上层可调用下层，下层不得调用上层 |
| 接口隔离 | 每层通过 `.h` 暴露接口，`static` 隐藏内部实现 |
| 禁止裸寄存器 | 业务代码不直接操作寄存器，统一经驱动层封装 |
| 中断最小化 | ISR 内只做标志置位/数据搬运，处理逻辑放任务或主循环 |

### 2.2 文件内代码分区（.c 文件标准顺序）

```c
/* ========================================================================== */
/*                             INCLUDES                                       */
/* ========================================================================== */
#include "bsp_led.h"

/* ========================================================================== */
/*                             PRIVATE DEFINES                                */
/* ========================================================================== */

/* ========================================================================== */
/*                             PRIVATE TYPES                                  */
/* ========================================================================== */

/* ========================================================================== */
/*                             PRIVATE VARIABLES                              */
/* ========================================================================== */

/* ========================================================================== */
/*                             PUBLIC VARIABLES                               */
/* ========================================================================== */

/* ========================================================================== */
/*                             PRIVATE FUNCTION PROTOTYPES                    */
/* ========================================================================== */

/* ========================================================================== */
/*                             FUNCTION IMPLEMENTATION                        */
/* -------------------------------------------------------------------------- */
/*  Part 1: Initialization routines                                           */
/*  Part 2: Public API                                                        */
/*  Part 3: Static (internal) functions                                       */
/* ========================================================================== */
```

> `.h` 文件标准顺序：文件头 → 包含保护 → include → 宏导出 → 类型导出 → 函数声明。

---

## 3. 函数注释

### 3.1 普通函数

```c
/**
  * @brief  Set the LED output state
  * @param  state   Target state, one of @ref LED_State_e
  * @retval int8_t  0 on success, -1 on invalid parameter
  * @note   Task context only; use LED_SetStateFromISR() inside ISRs
  */
int8_t LED_SetState(LED_State_e state)
{
    ...
}
```

### 3.2 带多参数的函数（含取值范围）

```c
/**
  * @brief  Configure the HRTIM output duty cycle
  * @param  ch       Output channel: @arg PWM_CH_BUCK buck stage, @arg PWM_CH_BOOST boost stage
  * @param  duty_q10 Duty cycle in 0.1% units, range [0, 1000] = 0.0~100.0%
  * @param  freq_hz  Switching frequency, range [100000, 500000] Hz
  * @retval 0 success; -1 invalid channel; -2 duty out of range
  * @see    PWM_EnterFault() to force PWM shut-down on fault
  */
int8_t PWM_SetDuty(PWM_Ch_e ch, uint16_t duty_q10, uint32_t freq_hz)
{
    ...
}
```

### 3.3 初始化函数模板

```c
/**
  * @brief  Module initialization: configure GPIO and reset internal state
  * @param  None
  * @retval None
  * @note   Must be called after SystemClock_Config() and before the RTOS scheduler starts
  */
void LED_Init(void)
```

### 3.4 FreeRTOS 任务模板

```c
/**
  * @brief  Control-loop task: runs the voltage-loop PI at a 200 us period
  * @param  argument Unused
  * @retval None
  * @note   Priority osPriorityHigh, stack 256 words; single iteration must complete within 200 us
  */
static void ControlLoopTask(void *argument)
```

### 3.5 中断回调模板

```c
/**
  * @brief  ADC injected-channel conversion complete callback
  * @param  hadc ADC handle
  * @retval None
  * @warning Runs in interrupt context; blocking calls and printf are forbidden, must finish within 20 us
  */
void HAL_ADCEx_InjectedConvCpltCallback(ADC_HandleTypeDef *hadc)
```

---

## 4. 数据类型注释

```c
/**
  * @brief  LED working state
  */
typedef enum {
    LED_OFF = 0,        /*!< Off */
    LED_ON  = 1,        /*!< Steady on */
    LED_BLINK = 2,      /*!< Blinking, period set by LED_SetPeriod() */
} LED_State_e;

/**
  * @brief  Power stage runtime data
  */
typedef struct {
    float    vout;          /*!< Output voltage in V, 10 kHz sampled average */
    float    iout;          /*!< Output current in A */
    float    temperature;   /*!< Power stage temperature in degC, after NTC conversion */
    uint8_t  fault_code;    /*!< Fault code, encoding defined in @ref FaultCode_e */
    uint32_t run_ms;        /*!< Cumulative run time in ms, wraps around after 49.7 days */
} Power_Info_t;
```

---

## 5. 宏定义与全局变量注释

```c
/* Macros with physical meaning: state the unit, range and origin of the value */
#define VOUT_SCALE      0.0121f     /*!< Output voltage scaling = 3.3V/4096 * divider ratio */
#define DEADTIME_NS     150         /*!< HRTIM dead time in ns, chosen from MOSFET datasheet t_off */
#define STACK_SIZE_CTRL 256         /*!< Control task stack in words, measured peak 178 */

/* Register/macro wrappers: state the side effects */
#define PWM_ALL_DISABLE() \
    do { __HAL_HRTIM_WAKEUP(&hhrtim1); } while (0)  /*!< Disable all outputs; used only on fault/shutdown paths */

/* Global variables: must state the unit and access context */
float g_vout_ref = 12.0f;           /*!< Output voltage setpoint in V; task-context access, do not write from ISR */
volatile uint8_t g_fault_flag = 0;  /*!< Fault flag 1=fault 0=normal; written in ISR / cleared in task, must be volatile */
```

---

## 6. 行内注释与特殊标记

```c
duty += 5;                          /* Duty step 0.5% (trailing comments aligned) */

/* Multi-line logic comments go ABOVE the code and explain WHY, not WHAT:
 * current-loop bandwidth is limited during the capacitor pre-charge stage,
 * so ramping slowly here avoids inrush overshoot */
state = CHARGE_SLOW;

/* TODO(Rayylen 2026-08-30): add secondary over-voltage protection, only the comparator stage exists now */
/* FIXME: jitter at duty=0 and duty=1000 boundaries, HRTIM comparator behavior under investigation */
/* NOTE: this callback runs in interrupt context, no printf/delay allowed */
/* WARNING: changing DEADTIME_NS requires updating the dead-time verification table in the datasheet appendix */
```

---

## 7. 排版约定速查

| 项目 | 约定 |
|------|------|
| 缩进 | 4 空格，禁用 Tab 混用 |
| 单行宽度 | ≤ 100 列，超长参数换行对齐 |
| 括号 | Allman 或 K&R 全项目统一，`do{}while(0)` 包裹多语句宏 |
| 命名 | 函数 `Module_Action()`，变量小写下划线，宏全大写，类型 `_t`/`_e` 结尾 |
| **注释语言** | **全工程统一英文**（拼写完整，不用拼音/中式缩写） |
| 注释内容 | 讲清单位、取值范围、调用上下文（任务/ISR）、并发约束 |
| 修改纪律 | 改动文件必须更新文件头版本号与修改记录 |
