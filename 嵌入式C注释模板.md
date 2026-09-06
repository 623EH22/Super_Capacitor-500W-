# 嵌入式 C 语言注释与格式规范（STM32 / FreeRTOS 适用，注释语言：英文）

> 规范风格：文件头与分区采用 Micrium 横幅风格；类型/成员注释保留 Doxygen `/*!< */` 语法（Doxygen、VSCode、Keil 均可识别）。
> 编码统一 UTF-8（Keil 中配置 Edit → Configuration → Editor → Encoding = UTF-8）。
> 约定：**所有代码注释一律使用英文**；**函数一律不加注释**，用分区条分类 + 空行分隔；代码格式统一交给 clang-format（项目根目录 `.clang-format`）。

---

## 1. 文件头注释（Micrium 横幅风格，.c / .h 通用）

> 文件头为**单个** Micrium 横幅注释块：模块标题、描述、`Filename` / `Version` / `Programmer(s)`、编号式 `Note(s)`、修改历史，末尾直接衔接 `INCLUDE FILES` 分区。不包含版权/许可信息；`Programmer(s)` 填作者，改动文件时更新版本与修改历史。

```c
/*
*************************************************************************************************************************
*                                                          LED
*                          LED driver module for the 500W Super-Capacitor Digital Power Supply.
* Filename      : bsp_led.c
* Version       : V1.00.00
* Programmer(s) : RuiYuan Lin
*************************************************************************************************************************
* Note(s)       : TBD
*************************************************************************************************************************
*                                                  MODIFICATION HISTORY
*************************************************************************************************************************
*   Version   Date          Author        Description
*   V1.00.00  2026-08-30    RuiYuan Lin   Initial release
*************************************************************************************************************************
*                                                     INCLUDE FILES
*************************************************************************************************************************
*/
#include "bsp_led.h"
```

头文件防重复包含（`.h` 文件头使用同样的注释块，`Filename` 填 `.h` 文件名）：

```c
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
├── User/App/               Application layer: business logic, state machines, FreeRTOS tasks, control strategy
├── User/Business/          Business layer: biz_xxx modules bridging app and peripherals
├── User/Algorithm/         Algorithm layer (service): PID, filters, calibration
├── User/Driver/            Driver layer: peripheral wrappers (drv_led.c / drv_hrtim.c)
├── Middlewares/            Third-party libraries (FreeRTOS, FatFs)
└── Drivers/ST/             ST official HAL library (must not be modified)
```

分层规则：

| 规则 | 说明 |
|------|------|
| 单向依赖 | App → Business/Algorithm → Driver → HAL，上层可调用下层，下层不得调用上层 |
| 接口隔离 | 每层通过 `.h` 暴露接口，`static` 隐藏内部实现 |
| 禁止裸寄存器 | 业务代码不直接操作寄存器，统一经驱动层封装 |
| 中断最小化 | ISR 内只做标志置位/数据搬运，处理逻辑放任务或主循环 |

### 2.2 文件内代码分区（.c 文件标准顺序）

```c
/*
*************************************************************************************************************************
*                                                     INCLUDE FILES
*************************************************************************************************************************
*/

/*
*************************************************************************************************************************
*                                                    PRIVATE DEFINES
*************************************************************************************************************************
*/

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
*                                                FUNCTION IMPLEMENTATION
*************************************************************************************************************************
*/
```

> `.h` 文件标准顺序：文件头 → 包含保护 → include → 宏导出 → 类型导出 → 函数声明。

---

## 3. 命名规范（全局/局部变量、全局/局部函数）

| 类别 | 命名规则 | 示例 |
|------|----------|------|
| 全局变量（跨文件，.h 中 extern） | `g_` 前缀 + 小写下划线 | `g_vout_ref`、`g_fault_flag` |
| 静态变量（文件内 static 全局） | `s_` 前缀 + 小写下划线 | `s_err_filt`、`s_state` |
| 局部变量（函数内） | 小写下划线，尽量简短 | `err`、`duty_q10`、`loop_cnt` |
| 全局函数（对外接口） | `Module_Action()`：模块前缀 + 大驼峰 | `PID_Calculate()`、`LED_SetState()` |
| 局部函数（static，仅本文件） | 与普通函数同风格命名，`static` 关键字本身标记文件私有 | `LimitMax()`、`Normalization()` |
| 类型 | `Module_Name_t`，枚举 `Module_Name_e` | `PID_t`、`LED_State_e` |
| 枚举值 / 宏 | 全大写 + 模块前缀 | `LED_ON`、`PID_MAX_OUT` |

规则说明：

- 前缀即作用域：看到 `g_` 就知道跨文件共享、需考虑并发与 volatile；看到 `s_` 就知道仅本文件可见；static 函数的文件私有性由 `static` 关键字保证
- 全局变量与静态变量必须配 `/*!< */` 注释，写明单位、取值范围、访问上下文（任务/ISR，见第 6 节）
- 文件内分区（见 2.2 节）按类别用分区条隔开：`PRIVATE DEFINES` / `PRIVATE TYPES` / `PRIVATE VARIABLES` / `PUBLIC VARIABLES` / `PRIVATE (HELPER) FUNCTIONS` / `GLOBAL FUNCTION PROTOTYPES`；**当前为空的类别也保留空分区**，新增代码有固定去处

---
## 4. 函数组织规则（函数不加注释）

约定：**函数一律不加注释**（包括 Doxygen 函数头）。函数的语义靠**命名 + 分区条分类**表达；参数含义、取值范围、单位等关键信息写在**头文件的类型/宏注释**或 `config.h` 中（见第 5、6 节）。确有风险需要标注时，用第 7 节的 `/* NOTE / WARNING / TODO */` 标记写在函数内部相关代码上方，不做函数级注释。

```c
/*
*************************************************************************************************************************
*                                               PRIVATE (HELPER) FUNCTIONS
*************************************************************************************************************************
*/

static inline float LimitMax(float value, float max)
{
    if(value > max) value = max;
    if(value < -max) value = -max;
    return value;
}

static inline float LimitMaxMin(float value, float max, float min)
{
    if(value > max) value = max;
    if(value < min) value = min;
    return value;
}

/*
*************************************************************************************************************************
*                                                       GLOBAL FUNCTION PROTOTYPES
*************************************************************************************************************************
*/

bool PID_Handle_Init(Pid_t *pid, const float Kpid[7], const Pid_Mode Mode)
{
    ...
}

float PID_Calculate(Pid_t *pid, const float real, const float exp)
{
    ...
}
```

> 空行规则：**宏定义、变量定义、函数声明**的上下行之间**不加空行**（连续书写）；只有**函数实现**（以及 typedef 结构体/枚举定义）之间空一行；分区条与内容之间空一行。由 `SeparateDefinitionBlocks: Leave` 保持手写原样——clang-format 不自动增删定义间的空行（`Always` 在部分版本会给连续 `#define` 也插空行，故不用），空行规则靠书写纪律维持。
> 宽度规则：分区条为 Micrium 横幅注释块（通栏 `*`，宽 121 列），覆盖编辑器可视宽度；`ColumnLimit: 0` 下 clang-format 不会重排任何注释行，分区条绝对安全。

分区标题按需取用：

| 分区标题 | 放什么 |
|----------|--------|
| INITIALIZATION | 模块/外设初始化函数 |
| GLOBAL FUNCTION PROTOTYPES | 对外接口 |
| PRIVATE (HELPER) FUNCTIONS | static 内部辅助函数 |
| FREERTOS TASKS | 任务函数 |
| ISR CALLBACKS | HAL_XXX_Callback 中断回调 |

---

## 5. 数据类型注释

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

> 函数参数含义复杂时（如 `Kpid[7]` 这类魔法数组），把每个下标的含义写在对应结构体成员的 `/*!< */` 注释里，替代函数注释。

---

## 6. 宏定义与全局变量注释

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

## 7. 行内注释与特殊标记

```c
duty += 5;                          /* Duty step 0.5% (trailing comments aligned) */

/* Multi-line logic comments go ABOVE the code and explain WHY, not WHAT:
 * current-loop bandwidth is limited during the capacitor pre-charge stage,
 * so ramping slowly here avoids inrush overshoot */
state = CHARGE_SLOW;

/* TODO(RuiYuan Lin 2026-08-30): add secondary over-voltage protection, only the comparator stage exists now */
/* FIXME: jitter at duty=0 and duty=1000 boundaries, HRTIM comparator behavior under investigation */
/* NOTE: this callback runs in interrupt context, no printf/delay allowed */
/* WARNING: changing DEADTIME_NS requires updating the dead-time verification table in the datasheet appendix */
```

---

## 8. clang-format 自动格式化

项目根目录已放置 [.clang-format](.clang-format)，采用团队确认的配置（`Language: Cpp`、`ColumnLimit: 0`、函数大括号独立、赋值/声明/宏自动对齐）。关键选项：

关键配置：

| 选项 | 值 | 作用 |
|------|-----|------|
| `ColumnLimit` | 0 | 不限制行长、不自动换行；任何注释（含 120 列分区条）都不会被重排 |
| `BreakBeforeBraces` | Custom | 函数大括号独占一行；`if/else/enum/struct` 大括号跟随语句（`} else {`） |
| `IndentWidth` / `UseTab` | 4 / Never | 4 空格缩进，消除 Tab 混用 |
| `AlignConsecutiveAssignments` | AcrossEmptyLines | 连续赋值的 `=` 跨空行对齐 |
| `AlignConsecutiveDeclarations` | AcrossEmptyLines | 连续声明的变量名对齐 |
| `AlignConsecutiveMacros` | AcrossEmptyLinesAndComments | 相邻 `#define` 的值对齐 |
| `AlignTrailingComments` | true | 行尾注释对齐 |
| `PointerAlignment` | Right | `uint8_t *p;` |
| `SpaceBeforeParens` | ControlStatements | `if (...)` 控制语句括号前有空格 |
| `IndentCaseLabels` | true | case 标签相对 switch 缩进一级 |
| `SortIncludes` | false | 不打乱 CubeMX 生成的 include 顺序 |
| `SeparateDefinitionBlocks` | Leave | 空行完全按手写保留，不自动增删（避免部分版本给 #define 插空行） |
| `MaxEmptyLinesToKeep` | 1 | 连续空行只保留一个 |

使用方式：

- **VSCode**：安装 "clang-format" 扩展或 "C/C++" 扩展，`"C_Cpp.clang_format_style": "file"`（默认就会读取项目内 `.clang-format`），建议开启 format on save
- **命令行**：单文件 `clang-format -i xxx.c`；全项目 `git ls-files '*.c' '*.h' | xargs clang-format -i`（建议在独立分支先试跑并检查 diff）
- **Keil MDK**：无原生格式化支持，建议 VSCode 里格式化后再回 Keil 编译；也可参考硬汉嵌入式论坛的 MDK clang-format 插件教程
- **注意**：`ColumnLimit: 0` 下 clang-format 不做断行与合并，`if (x) x = 1;` 这类单行写法保持原样（也不会自动加大括号），行长纪律靠 code review 把关
- **版本要求**：clang-format 14+（`AlignArrayOfStructures`、`SeparateDefinitionBlocks` 需 14+，`AcrossEmptyLines` 系列对齐需 13+；已在 clang-format 23.1 实测通过）

---

## 9. 排版约定速查

| 项目 | 约定 |
|------|------|
| 缩进 | 4 空格，禁用 Tab（clang-format `UseTab: Never`） |
| 单行宽度 | 不强制（`ColumnLimit: 0`），分行靠手工保持可读；分区条固定 120 列 |
| 大括号 | 函数大括号独占一行；`if/else/enum/struct` 跟随语句（`if (...) {`、`} else {`） |
| 空行 | 宏/变量定义、函数声明连续书写不空行；函数实现之间空一行（手写维护，格式化不改动） |
| 分区条宽度 | Micrium 横幅块，通栏宽度 121 列（`*` 通栏 + `*/` 收尾），覆盖编辑器可视宽度 |
| 命名 | 见第 3 节：全局变量 `g_`、静态变量 `s_`、全局函数 `Module_Action()`、宏全大写 |
| **函数注释** | **一律不加**；分区条分类 + 命名表达语义，关键约束写在类型/宏注释或行内标记中 |
| 注释语言 | 全工程统一英文（文件头、类型/宏/变量注释、TODO 标记均英文），拼写完整 |
| 格式化 | clang-format 14+，规则以项目根目录 `.clang-format` 为准，禁止手工对调格式 |
| 修改纪律 | 改动文件必须更新文件头版本号与修改记录 |
