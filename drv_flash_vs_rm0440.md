# STM32G4 Flash 驱动 — RM0440 手册 ↔ 代码对照

| 项目 | 内容 |
|------|------|
| 芯片 | STM32G474CBTx（Category 3） |
| 手册 | RM0440 Rev 9，§3 Embedded flash memory |
| 驱动 | `User/Driver/drv_flash.c` / `drv_flash.h` |
| 存储区 | Bank2 末 4 页，`0x0801E000`–`0x0801FFFF` |

---

## 1. 寄存器总览

| 偏移 | 寄存器 | 手册章节 | 本驱动用途 | 代码位置 |
|------|--------|----------|------------|----------|
| 0x00 | **FLASH_ACR** | §3.7.1 | `ICEN`/`DCEN` 开关，`ICRST`/`DCRST` 复位 Cache | `Flash_Program` / `Flash_Page_Erase` |
| 0x08 | **FLASH_KEYR** | §3.7.3 | 解锁：依次写 KEY1、KEY2 | `Flash_Unlock` |
| 0x10 | **FLASH_SR** | §3.7.5 | 等 `BSY`；读/清 `EOP` 与错误位 | `Flash_Wait` 等 |
| 0x14 | **FLASH_CR** | §3.7.6 | `LOCK`/`PG`/`PER`/`PNB`/`BKER`/`STRT` | Unlock/Program/Erase/Lock |
| 主存 | 数据空间 | §3.3.7 | 向 Flash 地址写两个 32-bit 组成双字 | `Flash_Program` |

---

## 2. FLASH_SR 位（手册 ↔ 代码）

| 手册位定义 (RM0440 §3.7.5) | 宏名 | 清除方式 | 代码用法 |
|---------------------------|------|----------|----------|
| Bit16 BSY：操作进行中 | `FLASH_SR_BSY` | 硬件清 0 | `while (SR & BSY)` |
| Bit0 EOP：成功结束 | `FLASH_SR_EOP` | rc_w1 写 1 清 | `Flash_Wait` 读入后清除 |
| Bit3 PROGERR：未擦除地址编程 | `FLASH_SR_PROGERR` | rc_w1 | `Flash_Wait` 捕获后清除 |
| Bit4 WRPERR：写保护 | `FLASH_SR_WRPERR` | rc_w1 | 同上 |
| Bit5 PGAERR：未对齐 | `FLASH_SR_PGAERR` | rc_w1 | 同上 |
| Bit6 SIZERR：非 32-bit 访问 | `FLASH_SR_SIZERR` | rc_w1 | 同上 |
| Bit7 PGSERR：时序错误 | `FLASH_SR_PGSERR` | rc_w1 | 同上 |
| Bit8 MISERR：快写数据 miss | `FLASH_SR_MISERR` | rc_w1 | 同上（CMSIS 名为 MISERR） |
| Bit9 FASTERR：快写失败 | `FLASH_SR_FASTERR` | rc_w1 | 同上 |

**清标志写法（手册 rc_w1）**

```c
/* 正确：只对目标位写 1 */
FLASH->SR = FLASH_SR_EOP | FLASH_SR_PROGERR | ...;

/* 错误：读-改-写可能误清其它为 1 的位 */
/* FLASH->SR |= mask;  — 不推荐 */
```

---

## 3. 解锁 / 上锁

| RM0440 §3.3.5 Unlocking | 实际代码 |
|-------------------------|----------|
| 1. 若已锁，向 FLASH_KEYR 写 KEY1 = `0x45670123` | `WRITE_REG(FLASH->KEYR, FLASH_KEY1);` |
| 2. 再写 KEY2 = `0xCDEF89AB` | `WRITE_REG(FLASH->KEYR, FLASH_KEY2);` |
| 硬件清 CR.LOCK | — |
| 写错序列会锁死直到复位 | 调用方保证顺序正确 |
| 软件可再置 LOCK | `SET_BIT(FLASH->CR, FLASH_CR_LOCK);` |

```c
/* 对应 HAL_FLASH_Unlock / HAL_FLASH_Lock 的寄存器级实现 */
RAM_FUNC static void Flash_Unlock(void)
{
    if (READ_BIT(FLASH->CR, FLASH_CR_LOCK) != 0U) {
        WRITE_REG(FLASH->KEYR, FLASH_KEY1);
        WRITE_REG(FLASH->KEYR, FLASH_KEY2);
    }
}

RAM_FUNC static void Flash_Lock(void)
{
    SET_BIT(FLASH->CR, FLASH_CR_LOCK);
}
```

---

## 4. 等待与状态：Flash_Wait

| 手册要求 | 实际代码 |
|----------|----------|
| 编程/擦除前确认 BSY=0 | `while (FLASH->SR & FLASH_SR_BSY)` |
| 结束后检查 EOP / 错误 | 先 `sr = FLASH->SR & (EOP\|ERR…)` |
| 错误与 EOP 为 rc_w1，软件清除 | `FLASH->SR = EOP\|ERR…`（直接赋值） |
| 超时保护（手册用超时机制） | 软件循环 `timeout`，超时返回 `FLASH_SR_BSY` |
| 不依赖 HAL 全局状态 | 只访问 `FLASH->SR`，可放 RAM |

```c
RAM_FUNC static uint32_t Flash_Wait(void)
{
    uint32_t timeout = 0x100000U;
    uint32_t sr;

    while ((FLASH->SR & FLASH_SR_BSY) != 0U) {
        if (--timeout == 0U) {
            return FLASH_SR_BSY;          /* 超时 */
        }
    }

    sr = FLASH->SR & (FLASH_SR_EOP | /* 全部错误位 */);

    FLASH->SR = FLASH_SR_EOP | /* 全部错误位 */;   /* rc_w1 清除 */

    return sr & (/* 仅错误位，不含 EOP */);
    /* 0 = 空闲且无错；非 0 = 错误或超时 */
}
```

---

## 5. 标准编程（写）— §3.3.7

### 5.1 手册步骤 ↔ 代码

| # | RM0440 Standard programming | 实际代码 |
|---|----------------------------|----------|
| 1 | 等 BSY=0 | `if (Flash_Wait() != 0U) return;` |
| 2 | 清上次错误标志（否则 PGSERR） | `Flash_Wait` 末尾已清 |
| 3 | 置 PG | `SET_BIT(FLASH->CR, FLASH_CR_PG);` |
| 4a | 写第一个 word（双字对齐地址） | `*(uint32_t *)address = *src_addr;` |
| 4b | 写第二个 word | `__ISB();` 后写 `address+4` |
| 5 | 等 BSY=0 | 循环内 `Flash_Wait()` |
| 6 | 检查并清 EOP | `Flash_Wait` 内处理 |
| 7 | 无更多请求则清 PG | `CLEAR_BIT(FLASH->CR, FLASH_CR_PG);` |

**补充约束（手册）**

| 约束 | 代码如何满足 |
|------|----------------|
| 只能双字（8 字节）编程 | `word_left` 按 `size/4` 成对写入 |
| 地址必须双字对齐 | `Drv_Flash_Write` 先 `addr & ~7` |
| 未对齐 → PGAERR | 上层保证；`Flash_Program` 假定 addr 已对齐 |
| 8/16 位写 → SIZERR | 只用 `uint32_t` 写 |
| 目标须已擦成 0xFF（或写全 0） | 由存储层先页擦 |
| PG 与 PER/MER 互斥 → PGSERR | 入口检查 `MER1\|MER2\|PER` |
| 写后注意 Cache 一致性 | 关 DCEN → 写 → DCRST → 再 DCEN |
| 同 bank 写时取指会 stall | 整段 `RAM_FUNC`；数据在 Bank2、代码可在 Bank1（RWW） |

### 5.2 上层对齐缓冲（手册无此层，工程需要）

硬件只认整双字。任意 `address + length` 先在 RAM 拼缓冲：

| 手册/硬件 | `Drv_Flash_Write` |
|-----------|-------------------|
| 从 8 字节对齐地址开始 | `addr = address & ~7` |
| 整双字一次编程 | `size` 向上对齐 8 的倍数 |
| 未用到的字节应保持擦除态 | 头/尾填 `0xFF` |
| 有效数据 | `bytes[addr_off + i] = data[i]` |

```
address = 0x0801E003, length = 5
addr    = 0x0801E000
bytes   = [FF FF FF | d0 d1 d2 d3 d4]
```

### 5.3 完整写路径

```
Drv_Flash_Write
  ├─ 对齐 + 拼 bytes[]（0xFF 填充）
  ├─ Flash_Unlock          ← KEYR
  ├─ Flash_Program         ← PG + 双字循环 + SR
  └─ Flash_Lock            ← CR.LOCK
```

---

## 6. 页擦除 — §3.3.6

### 6.1 手册步骤 ↔ 代码

| # | RM0440 Page erase | 实际代码 |
|---|-------------------|----------|
| 1 | 等 BSY=0 | `Flash_Wait()` |
| 2 | 清错误标志 | `Flash_Wait` 内 |
| 3a | 双 bank：选 BKER | Bank1→清 BKER；Bank2→置 BKER |
| 3b | 选页 PNB | `MODIFY_REG(CR, PNB, page << PNB_Pos)` |
| 3c | 置 PER | `SET_BIT(CR, PER)` |
| 4 | 置 STRT | `SET_BIT(CR, STRT)` |
| 5 | 等 BSY=0 | 长超时轮询（页擦约几十 ms） |
| — | 清 EOP/错误 | 直接写 SR |
| — | HAL：结束后清 PER/PNB | `CLEAR_BIT(CR, PER\|PNB\|BKER)` |

### 6.2 位与 bank 选择

| 手册 (DBANK=1) | 代码 |
|----------------|------|
| BKER=0 → Bank1 | `if (bank & FLASH_BANK_1) CLEAR BKER` |
| BKER=1 → Bank2 | `else SET BKER`（存储用 Bank2） |
| PNB = bank 内页号 | `STORAGE_PAGE_NUM(i)` 换算物理页号 |
| PER 与 PG/FSTPG/MER 互斥 | 入口检查四者全 0 |

### 6.3 擦除后硬件结果

| 项目 | 值 |
|------|-----|
| 页大小 | 2KB |
| 擦除后内容 | 全 `0xFF` |
| 典型耗时 | 约 20–40 ms（比双字编程慢约 10³ 倍） |
| 超时 | `Flash_Page_Erase` 用 `0x800000`（长于编程的 `0x100000`） |

### 6.4 完整擦除路径

```
Drv_Flash_Page_Erase(page_mask)
  ├─ Flash_Unlock
  ├─ 对 mask 中每一位置位 i:
  │    Flash_Page_Erase(STORAGE_PAGE_NUM(i), FLASH_BANK_2)
  │      ├─ Flash_Wait
  │      ├─ 检查 PG/FSTPG/MER
  │      ├─ 关 I/D Cache
  │      ├─ BKER + PNB + PER + STRT
  │      ├─ 等 BSY、清 SR、清 PER/PNB/BKER
  │      └─ 恢复 Cache
  └─ Flash_Lock
```

---

## 7. Cache 处理（§3.3.7 Programming and caches）

| 手册说明 | 代码 |
|----------|------|
| 擦写影响 D-Cache 中数据 | 编程前若 `DCEN=1` 则关掉 |
| 擦除后建议 flush I/D Cache | `Flash_Page_Erase` 同时处理 ICEN/DCEN |
| `ICRST`/`DCRST` 仅可在对应 EN=0 时写 | 先 DISABLE，再 RESET，再 ENABLE |
| HAL 擦除会关 Cache 并 Flush | 与 `HAL_FLASHEx_Erase` 行为对齐 |

---

## 8. 错误一览：手册原因 ↔ 驱动行为

| 标志 | 手册典型原因 | 本驱动 |
|------|--------------|--------|
| PROGERR | 写已编程地址（非全 0） | `Flash_Wait` 清掉；循环 `break`，不再写后续双字 |
| PGAERR | 地址非 8 对齐 | 上层 `addr & ~7` 预防 |
| SIZERR | 字节/半字访问 | 只用 32 位写 |
| PGSERR | PG 与擦除位同置；未先清错 | 入口检查 + Wait 清错 |
| WRPERR | 写保护区 | 存储区未开 WRP 时不会出现 |
| MISERR/FASTERR | 快写模式 | 标准 PG 模式一般不涉及 |
| BSY 超时 | 硬件异常 | `Flash_Wait` 返回非 0，上层 return/break |

当前对外 API 为 `void`，出错**静默停写**，不向上返回错误码。

---

## 9. 关键宏与地址（工程 ↔ 手册组织）

| 工程宏 | 值/含义 | 对应手册 |
|--------|---------|----------|
| `FLASH_MIN_WRITE_SIZE` | 8 | 双字编程宽度 |
| `FLASH_PAGE_SIZE` | 0x800 (2KB) | DBANK=1 页大小 |
| `STORAGE_START_ADDR` | 0x0801E000 | Bank2 高地址 |
| `STORAGE_END_ADDR` | 0x08020000 | 128KB 结束 |
| `STORAGE_BANK` | `FLASH_BANK_2` | BKER=1 |
| `STORAGE_PAGE_NUM(x)` | 页 28–31（示例 4 页） | Bank2 内物理页号 |
| `FLASH_BANK_MEM_SIZE` | 0x10000 (64KB/bank) | 双 bank 128KB 器件 |

---

## 10. 函数地图

| 函数 | 手册/参考 | 职责 |
|------|-----------|------|
| `Flash_Unlock` | §3.3.5 + HAL_FLASH_Unlock | KEYR 解锁 |
| `Flash_Lock` | HAL_FLASH_Lock | 置 LOCK |
| `Flash_Wait` | §3.7.5 + FLASH_WaitForLastOperation | 等 BSY、清标志 |
| `Flash_Program` | §3.3.7 标准编程 | N 个双字写入 |
| `Flash_Page_Erase` | §3.3.6 页擦 + FLASH_PageErase | 单页擦除 |
| `Drv_Flash_Write` | 工程封装 | 对齐缓冲 + Program |
| `Drv_Flash_Read` | 内存映射读 | 字节拷贝 |
| `Drv_Flash_Page_Erase` | 工程封装 | 位掩码多页擦 |

全部相关实现均标 `RAM_FUNC`，不依赖 HAL `pFlash` 状态机。

---

## 11. 从手册写驱动的检查清单

| 步骤 | 动作 |
|------|------|
| 1 | 选对 Category 章节（G474 → §3） |
| 2 | 抄「操作序列」编号步骤 → 函数骨架 |
| 3 | 查 §3.7 位图 → `SET_BIT`/`CLEAR_BIT`/`MODIFY_REG` |
| 4 | 对照 HAL `stm32g4xx_hal_flash.c` / `_ex.c` |
| 5 | 补 BSY、互斥位、Cache、对齐、RAM/RWW |
| 6 | 出错路径也要清 PG/PER 并恢复 Cache |

---

## 12. 一页速查

| 操作 | 先决 | 核心寄存器动作 | 结束 |
|------|------|----------------|------|
| 解锁 | LOCK=1 | KEYR←KEY1, KEY2 | LOCK=0 |
| 页擦 | 空闲、PG=0 | BKER/PNB, PER=1, STRT=1, 等 BSY | PER=0 |
| 双字写 | 空闲、地址已擦 | PG=1, 写 word+word, 等 BSY（×N） | PG=0 |
| 上锁 | 操作完成 | CR.LOCK=1 | — |
