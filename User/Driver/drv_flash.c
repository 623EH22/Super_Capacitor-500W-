/*
*************************************************************************************************************************
*                                                       DRV_FLASH
*                            Driver module for the 500W Super-Capacitor Digital Power Supply.
* Filename      : drv_flash.c
* Version       : V1.01.00
* Programmer(s) : RuiYuan Lin
*************************************************************************************************************************
* Note(s)       : TBD
*************************************************************************************************************************
*                                                  MODIFICATION HISTORY
*************************************************************************************************************************
*   Version   Date          Author        Description
*   V1.00.00  2026-09-01    RuiYuan Lin   Initial release
*   V1.01.00  2026-09-28    RuiYuan Lin   页擦除页号按单 bank 线性换算；擦除后回读校验；写/擦返回状态码
*                                                     INCLUDE FILES
*/
#define DEF_DRV_FLASH
#include "drv_flash.h"
#include "stm32g4xx_hal.h"
#include "assert.h"
/*
*************************************************************************************************************************
*                                                    PRIVATE DEFINES
*************************************************************************************************************************
*/
#define FLASH_MEM_SIZE              0x00020000U

#define FLASH_SR_ALL_FLAGS          (FLASH_SR_EOP | FLASH_SR_OPERR | FLASH_SR_PROGERR | FLASH_SR_SIZERR | FLASH_SR_PGAERR | FLASH_SR_WRPERR | FLASH_SR_PGSERR | FLASH_SR_MISERR | FLASH_SR_FASTERR | FLASH_SR_RDERR | FLASH_SR_OPTVERR)

#define FLASH_SR_FATAL_FLAGS        (FLASH_SR_OPERR | FLASH_SR_PROGERR | FLASH_SR_SIZERR | FLASH_SR_PGAERR | FLASH_SR_WRPERR | FLASH_SR_PGSERR | FLASH_SR_MISERR | FLASH_SR_FASTERR | FLASH_SR_RDERR)

#define IS_FLASH_ADDRESS(addr)      (((addr) >= FLASH_BASE) && ((addr) < (FLASH_BASE + FLASH_MEM_SIZE)))
#define IS_FLASH_WRITE_SIZE(size)   (((size) > 0U) && ((size) + (FLASH_MIN_WRITE_SIZE - 1U) <= FLASH_PAGE_SIZE))
#define IS_FLASH_WRITE_ALIGNMENT(a) (((a) & (FLASH_MIN_WRITE_SIZE - 1U)) == 0)

#define IS_STORAGE_ADDRESS(addr)    (((addr) >= STORAGE_START_ADDR) && ((addr) < STORAGE_END_ADDR))
#define IS_STORAGE_WRITE_SIZE(size) (((size) > 0U) && ((size) + (FLASH_MIN_WRITE_SIZE - 1U) <= FLASH_PAGE_SIZE))
#define IS_STORAGE_PAGE_MASK(mask)  (((mask) >> STORAGE_USE_PAGE_NUM) == 0)

/*
*************************************************************************************************************************
*                                                    PUBLIC VARIABLES
*************************************************************************************************************************
*/
DRV_FLASH_FUNC_HANDLE_T DrvFlashFuncHdl = {
    .flash_write      = Drv_Flash_Write,
    .flash_read       = Drv_Flash_Read,
    .flash_page_erase = Drv_Flash_Page_Erase
};
/*
*************************************************************************************************************************
*                                               PRIVATE (HELPER) FUNCTIONS
*************************************************************************************************************************
*/
/* RM0440: write KEY1 then KEY2 to FLASH_KEYR to clear CR.LOCK */
RAM_FUNC static void Flash_Unlock(void)
{
    if (READ_BIT(FLASH->CR, FLASH_CR_LOCK) != 0U) {
        WRITE_REG(FLASH->KEYR, FLASH_KEY1);
        WRITE_REG(FLASH->KEYR, FLASH_KEY2);
    }
}

/* RM0440: set CR.LOCK to lock FLASH_CR against further writes */
RAM_FUNC static void Flash_Lock(void)
{
    SET_BIT(FLASH->CR, FLASH_CR_LOCK);
}

/* Poll FLASH_SR only — no HAL state, safe to run from RAM.
 * Return: 0 = no error / non-zero = error flags or timeout (FLASH_SR_BSY) */
RAM_FUNC static uint32_t Flash_Wait(void)
{
    uint32_t timeout = 0x100000U;
    uint32_t sr;

    while ((FLASH->SR & FLASH_SR_BSY) != 0U) {
        if (--timeout == 0U) {
            return FLASH_SR_BSY;
        }
    }

    sr = FLASH->SR & FLASH_SR_ALL_FLAGS;

    /* rc_w1: write 1 to clear */
    FLASH->SR = FLASH_SR_ALL_FLAGS;

    return sr & FLASH_SR_FATAL_FLAGS;
}

/* RM0440 standard programming: program N double-words with PG kept set */
RAM_FUNC static uint32_t Flash_Program(uint32_t address, void *data, uint32_t size)
{
    assert(IS_FLASH_ADDRESS(address));
    assert(IS_FLASH_WRITE_ALIGNMENT(address));
    assert(data != NULL);
    assert(IS_FLASH_WRITE_SIZE(size));

    uint32_t *src_addr  = (uint32_t *)data;
    uint32_t  word_left = size >> 2;
    uint32_t  err;
    uint8_t   dcache_on;

    err = Flash_Wait();
    if (err != DRV_FLASH_OK) {
        return err;
    }

    if (READ_BIT(FLASH->CR, FLASH_CR_MER1 | FLASH_CR_MER2 | FLASH_CR_PER) != 0U) {
        return FLASH_SR_PGSERR;
    }

    dcache_on = (READ_BIT(FLASH->ACR, FLASH_ACR_DCEN) != 0U);
    if (dcache_on != 0U) {
        __HAL_FLASH_DATA_CACHE_DISABLE();
    }

    SET_BIT(FLASH->CR, FLASH_CR_PG);

    while (word_left >= 2U) {
        *(uint32_t *)address = *src_addr;
        __ISB();
        *(uint32_t *)(address + 4U) = *(src_addr + 1);
        address += 8U;
        src_addr += 2U;
        word_left -= 2U;

        err = Flash_Wait();
        if (err != DRV_FLASH_OK) {
            break;
        }
    }

    CLEAR_BIT(FLASH->CR, FLASH_CR_PG);

    if (dcache_on != 0U) {
        __HAL_FLASH_DATA_CACHE_RESET();
        __HAL_FLASH_DATA_CACHE_ENABLE();
    }

    return err;
}

/* RM0440 page erase: 与 Flash_Program 相同的 Unlock/Wait/Lock 路径 */
RAM_FUNC static uint32_t Flash_Page_Erase(uint32_t addr)
{
    uint32_t page    = (addr - FLASH_BASE) / FLASH_PAGE_SIZE;
    uint32_t timeout = 0x800000U;
    uint32_t sr;
    uint32_t err;
    uint32_t offset;
    uint8_t  icache_on;
    uint8_t  dcache_on;

    assert(IS_FLASH_ADDRESS(addr));
    assert(page < (FLASH_MEM_SIZE / FLASH_PAGE_SIZE));

    /* 与写路径一致：自写 Unlock */
    Flash_Unlock();

    err = Flash_Wait();
    if (err != DRV_FLASH_OK) {
        Flash_Lock();
        return err;
    }

    icache_on = (READ_BIT(FLASH->ACR, FLASH_ACR_ICEN) != 0U);
    dcache_on = (READ_BIT(FLASH->ACR, FLASH_ACR_DCEN) != 0U);
    if (dcache_on != 0U) {
        __HAL_FLASH_DATA_CACHE_DISABLE();
    }
    if (icache_on != 0U) {
        __HAL_FLASH_INSTRUCTION_CACHE_DISABLE();
    }

    /* 只清冲突位，不动 LOCK/OPTLOCK 等 */
    CLEAR_BIT(FLASH->CR, FLASH_CR_PG | FLASH_CR_FSTPG | FLASH_CR_MER1 | FLASH_CR_MER2 | FLASH_CR_PER | FLASH_CR_BKER);

    MODIFY_REG(FLASH->CR, FLASH_CR_PNB, ((page & 0xFFU) << FLASH_CR_PNB_Pos));
    SET_BIT(FLASH->CR, FLASH_CR_PER);
    SET_BIT(FLASH->CR, FLASH_CR_STRT);

    while ((FLASH->SR & FLASH_SR_BSY) != 0U) {
        if (--timeout == 0U) {
            break;
        }
    }

    sr  = FLASH->SR;

    err = sr & FLASH_SR_FATAL_FLAGS;
    if ((sr & FLASH_SR_BSY) != 0U) {
        err |= FLASH_SR_BSY;
    }

    FLASH->SR = FLASH_SR_ALL_FLAGS;

    CLEAR_BIT(FLASH->CR, FLASH_CR_PER | FLASH_CR_PNB | FLASH_CR_BKER);

    Flash_Lock();

    if (err == DRV_FLASH_OK) {
        for (offset = 0U; offset < FLASH_PAGE_SIZE; offset += 4U) {
            if (*(volatile uint32_t *)(addr + offset) != 0xFFFFFFFFU) {
                err = DRV_FLASH_ERR_VERIFY;
                break;
            }
        }
    }

    if (dcache_on != 0U) {
        __HAL_FLASH_DATA_CACHE_RESET();
        __HAL_FLASH_DATA_CACHE_ENABLE();
    }
    if (icache_on != 0U) {
        __HAL_FLASH_INSTRUCTION_CACHE_RESET();
        __HAL_FLASH_INSTRUCTION_CACHE_ENABLE();
    }

    return err;
}

/*
*************************************************************************************************************************
*                                               GLOBAL FUNCTION PROTOTYPES
*************************************************************************************************************************
*/
RAM_FUNC uint32_t Drv_Flash_Write(uint32_t address, void *data, uint32_t length)
{
    uint32_t err;

    assert(IS_STORAGE_ADDRESS(address));
    assert(IS_STORAGE_WRITE_SIZE(length));
    assert(data != NULL);

#if FLASH_REPROGRAM_EN
    {
        uint32_t addr       = address & ~(FLASH_MIN_WRITE_SIZE - 1);
        uint32_t addr_off   = address & (FLASH_MIN_WRITE_SIZE - 1);
        uint32_t write_size = (addr_off + length + FLASH_MIN_WRITE_SIZE - 1) & ~(FLASH_MIN_WRITE_SIZE - 1);
        uint8_t  bytes[write_size];

        for (uint32_t i = 0; i < write_size; i++) {
            uint32_t src_off = i - addr_off;
            bytes[i]         = ((i < addr_off) || (src_off >= length)) ? 0xFFU : ((uint8_t *)data)[src_off];
        }

        Flash_Unlock();

        err = Flash_Program(addr, bytes, write_size);

        Flash_Lock();
    }
#else
    {
        uint32_t addr     = address & ~(FLASH_MIN_WRITE_SIZE - 1);
        uint32_t addr_off = address & (FLASH_MIN_WRITE_SIZE - 1);

        Flash_Unlock();

        if ((addr_off == 0U) && ((length & (FLASH_MIN_WRITE_SIZE - 1U)) == 0U)) {
            err = Flash_Program(address, data, length);
        } else {
            uint32_t write_size = (addr_off + length + FLASH_MIN_WRITE_SIZE - 1) & ~(FLASH_MIN_WRITE_SIZE - 1);
            uint8_t  bytes[write_size];

            for (uint32_t i = 0; i < write_size; i++) {
                uint32_t src_off = i - addr_off;
                bytes[i]         = ((i < addr_off) || (src_off >= length)) ? 0xFFU : ((uint8_t *)data)[src_off];
            }

            err = Flash_Program(addr, bytes, write_size);
        }

        Flash_Lock();
    }
#endif

    return err;
}

RAM_FUNC void Drv_Flash_Read(uint32_t address, void *data, uint32_t length)
{
    assert(IS_STORAGE_ADDRESS(address));
    assert(data != NULL);

    uint8_t *pdata = data;
    for (uint32_t i = 0; i < length; i++) {
        *(uint8_t *)(pdata + i) = *((uint8_t *)(address + i));
    }
}

RAM_FUNC uint32_t Drv_Flash_Page_Erase(uint32_t mask)
{
    assert(IS_STORAGE_PAGE_MASK(mask));

    uint32_t err = DRV_FLASH_OK;

    for (uint8_t i = 0; i < STORAGE_USE_PAGE_NUM; i++) {
        if ((mask & (1U << i)) != 0U) {
            uint32_t page_err = Flash_Page_Erase(STORAGE_PAGE_ADDR(i));

            if (page_err != DRV_FLASH_OK) {
                err = page_err;
            }
        }
    }

    return err;
}

DRV_FLASH_FUNC_HANDLE_T *DRV_Flash_GetFuncHandle(void)
{
    return &DrvFlashFuncHdl;
}
