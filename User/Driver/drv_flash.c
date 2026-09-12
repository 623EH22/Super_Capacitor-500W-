/*
*************************************************************************************************************************
*                                                       DRV_FLASH
*                            Driver module for the 500W Super-Capacitor Digital Power Supply.
* Filename      : drv_flash.c
* Version       : V1.00.00
* Programmer(s) : RuiYuan Lin
*************************************************************************************************************************
* Note(s)       : TBD
*                                                  MODIFICATION HISTORY
*   Version   Date          Author        Description
*   V1.00.00  2026-09-01    RuiYuan Lin   Initial release
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
#define FLASH_BANK1_BASE            0x08000000U
#define FLASH_BANK2_BASE            0x08010000U

#define STORAGE_BANK                (STORAGE_START_ADDR >= FLASH_BANK2_BASE ? FLASH_BANK_2 : FLASH_BANK_1)
#define STORAGE_BANK_BASE           (STORAGE_BANK == FLASH_BANK_1 ? FLASH_BANK1_BASE : FLASH_BANK2_BASE)

#define FLASH_BANK_PAGE_AMOUNT      (FLASH_BANK_MEM_SIZE / FLASH_PAGE_SIZE)
#define STORAGE_PAGE_NUM(x)         ((x) + ((STORAGE_START_ADDR - STORAGE_BANK_BASE) / FLASH_PAGE_SIZE))

#define IS_FLASH_ADDRESS(addr)      (((addr) >= FLASH_BASE) && ((addr) < (FLASH_BASE + 2 * FLASH_BANK_MEM_SIZE)))
#define IS_FLASH_PAGE(page)         (((page) >= 0) && ((page) < FLASH_BANK_PAGE_AMOUNT))
#define IS_FLASH_WRITE_SIZE(size)   (((size) > 0U) && ((size) + (FLASH_MIN_WRITE_SIZE - 1U) <= FLASH_PAGE_SIZE))

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
 * Return: 0 = OK / EOP; non-zero = error flags or timeout (FLASH_SR_BSY). */
RAM_FUNC static uint32_t Flash_Wait(void)
{
    uint32_t timeout = 0x100000U;
    uint32_t sr;

    while ((FLASH->SR & FLASH_SR_BSY) != 0U) {
        if (--timeout == 0U) {
            return FLASH_SR_BSY;
        }
    }

    sr = FLASH->SR & (FLASH_SR_EOP | FLASH_SR_PROGERR | FLASH_SR_SIZERR | FLASH_SR_PGAERR | FLASH_SR_WRPERR | FLASH_SR_PGSERR | FLASH_SR_MISERR | FLASH_SR_FASTERR);

    /* rc_w1: write 1 to clear */
    FLASH->SR = FLASH_SR_EOP | FLASH_SR_PROGERR | FLASH_SR_SIZERR | FLASH_SR_PGAERR | FLASH_SR_WRPERR | FLASH_SR_PGSERR | FLASH_SR_MISERR | FLASH_SR_FASTERR;

    return sr & (FLASH_SR_PROGERR | FLASH_SR_SIZERR | FLASH_SR_PGAERR | FLASH_SR_WRPERR | FLASH_SR_PGSERR | FLASH_SR_MISERR | FLASH_SR_FASTERR);
}

/* RM0440 standard programming: program N double-words with PG kept set */
RAM_FUNC static void Flash_Program(uint32_t address, void *data, uint32_t size)
{
    assert(IS_FLASH_ADDRESS(address));
    assert(data != NULL);
    assert(IS_FLASH_WRITE_SIZE(size));

    uint32_t *src_addr  = (uint32_t *)data;
    uint32_t  word_left = size >> 2;
    uint8_t   dcache_on;

    /* Must be idle; leftover errors abort (else PGSERR) */
    if (Flash_Wait() != 0U) {
        return;
    }

    /* PG with erase bits set is illegal */
    if (READ_BIT(FLASH->CR, FLASH_CR_MER1 | FLASH_CR_MER2 | FLASH_CR_PER) != 0U) {
        return;
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

        if (Flash_Wait() != 0U) {
            break;
        }
    }

    CLEAR_BIT(FLASH->CR, FLASH_CR_PG);

    if (dcache_on != 0U) {
        __HAL_FLASH_DATA_CACHE_RESET();
        __HAL_FLASH_DATA_CACHE_ENABLE();
    }
}

/* RM0440 page erase: PER + PNB + BKER + STRT, then wait BSY */
RAM_FUNC static void Flash_Page_Erase(uint32_t page, uint32_t bank)
{
    assert(IS_FLASH_PAGE(page));
    uint32_t timeout = 0x800000U;
    uint8_t  icache_on;
    uint8_t  dcache_on;

    if (Flash_Wait() != 0U) {
        return;
    }

    /* PG / FSTPG / MER must be clear when PER is set, else PGSERR */
    if (READ_BIT(FLASH->CR, FLASH_CR_PG | FLASH_CR_FSTPG | FLASH_CR_MER1 | FLASH_CR_MER2) != 0U) {
        return;
    }

    icache_on = (READ_BIT(FLASH->ACR, FLASH_ACR_ICEN) != 0U);
    dcache_on = (READ_BIT(FLASH->ACR, FLASH_ACR_DCEN) != 0U);
    if (dcache_on != 0U) {
        __HAL_FLASH_DATA_CACHE_DISABLE();
    }
    if (icache_on != 0U) {
        __HAL_FLASH_INSTRUCTION_CACHE_DISABLE();
    }

    /* Dual bank: BKER selects bank (0 = Bank1, 1 = Bank2) */
    if ((bank & FLASH_BANK_1) != 0U) {
        CLEAR_BIT(FLASH->CR, FLASH_CR_BKER);
    } else {
        SET_BIT(FLASH->CR, FLASH_CR_BKER);
    }

    MODIFY_REG(FLASH->CR, FLASH_CR_PNB, ((page & 0xFFU) << FLASH_CR_PNB_Pos));
    SET_BIT(FLASH->CR, FLASH_CR_PER);
    SET_BIT(FLASH->CR, FLASH_CR_STRT);

    /* Page erase takes tens of ms — longer software timeout than program */
    while ((FLASH->SR & FLASH_SR_BSY) != 0U) {
        if (--timeout == 0U) {
            break;
        }
    }

    FLASH->SR = FLASH_SR_EOP | FLASH_SR_PROGERR | FLASH_SR_SIZERR | FLASH_SR_PGAERR | FLASH_SR_WRPERR | FLASH_SR_PGSERR | FLASH_SR_MISERR | FLASH_SR_FASTERR;

    CLEAR_BIT(FLASH->CR, FLASH_CR_PER | FLASH_CR_PNB | FLASH_CR_BKER);

    if (dcache_on != 0U) {
        __HAL_FLASH_DATA_CACHE_RESET();
        __HAL_FLASH_DATA_CACHE_ENABLE();
    }
    if (icache_on != 0U) {
        __HAL_FLASH_INSTRUCTION_CACHE_RESET();
        __HAL_FLASH_INSTRUCTION_CACHE_ENABLE();
    }
}
/*
*************************************************************************************************************************
*                                               GLOBAL FUNCTION PROTOTYPES
*************************************************************************************************************************
*/
RAM_FUNC void Drv_Flash_Write(uint32_t address, void *data, uint32_t size)
{
    assert(IS_STORAGE_ADDRESS(address));
    assert(IS_STORAGE_WRITE_SIZE(size));
    assert(data != NULL);

    uint32_t addr       = address & ~(FLASH_MIN_WRITE_SIZE - 1);
    uint8_t  addr_off   = address & (FLASH_MIN_WRITE_SIZE - 1);
    uint32_t write_size = (addr_off + size + FLASH_MIN_WRITE_SIZE - 1) & ~(FLASH_MIN_WRITE_SIZE - 1);
    uint8_t  bytes[write_size];

    uint16_t i;
    for (i = 0; i < addr_off; i++) {
        bytes[i] = 0xFF;
    }

    for (i = 0; i < size; i++) {
        bytes[addr_off + i] = ((uint8_t *)data)[i];
    }

    for (i = addr_off + size; i < write_size; i++) {
        bytes[i] = 0xFF;
    }

    Flash_Unlock();

    Flash_Program(addr, bytes, write_size);

    Flash_Lock();
}

RAM_FUNC void Drv_Flash_Read(uint32_t address, void *data, uint32_t size)
{
    assert(IS_STORAGE_ADDRESS(address));
    assert(data != NULL);

    uint32_t readNum = size;
    uint8_t *pdata   = data;
    for (uint32_t i = 0; i < readNum; i++) {
        *(uint8_t *)(pdata + i) = *((uint8_t *)(address + i));
    }
}

RAM_FUNC void Drv_Flash_Page_Erase(uint32_t mask)
{
    assert(IS_STORAGE_PAGE_MASK(mask));

    uint8_t i;
    Flash_Unlock();

    for (i = 0; i < STORAGE_USE_PAGE_NUM; i++) {
        if ((mask & (1U << i)) != 0U) {
            Flash_Page_Erase(STORAGE_PAGE_NUM(i), STORAGE_BANK);
        }
    }

    Flash_Lock();
}
