/*
*************************************************************************************************************************
*                                                         HAL_STORAGE
*                             HAL module for the 500W Super-Capacitor Digital Power Supply.
* Filename      : hal_storage.c
* Version       : V1.00.00
* Programmer(s) : RuiYuan Lin
*************************************************************************************************************************
* Note(s)       : TBD
*************************************************************************************************************************
*                                                  MODIFICATION HISTORY
*************************************************************************************************************************
*   Version   Date          Author        Description
*   V1.00.00  2026-09-16    RuiYuan Lin   function realization
*************************************************************************************************************************
*/

/*
*************************************************************************************************************************
*                                                     INCLUDE FILES
*************************************************************************************************************************
*/
#define DEF_HAL_STORAGE
#include "hal_storage.h"
#include "drv_flash.h"
#include <stddef.h>
#include "string.h"

/*
*************************************************************************************************************************
*                                                    PRIVATE DEFINES
*************************************************************************************************************************
*/
#define DEF_STORAGE_PAGE_MAGIC  0x53544F52

#define DEF_STORAGE_PAGE_ERASED 0xFFFFFFFFU
#define DEF_STORAGE_PAGE_ACTIVE 0xB0B0B0B0U
#define DEF_STORAGE_PAGE_FULL   0x00000000U

#define DEF_STORAGE_ID_LENTH    (sizeof(uint16_t))
#define DEF_STORAGE_INVALID_ID  0x0000U
#define DEF_STORAGE_ERASED_ID   0xFFFFU
#define DEF_STORAGE_EXTENDED_ID 0xFF00U

/*
*************************************************************************************************************************
*                                                     PRIVATE TYPES
*************************************************************************************************************************
*/
typedef enum {
    STORAGE_PAGE_INVALID = 0,
    STORAGE_PAGE_FIRST,
    STORAGE_PAGE_ACTIVE,
    STORAGE_PAGE_RECEIVE,
    STORAGE_PAGE_FULL,
} STORAGE_PAGE_STATUS_E;

typedef struct {
    uint32_t magic;
    uint32_t status;
    uint32_t seq;
    uint32_t crc;
} STORAGE_PAGE_HEADER_T;

typedef struct {
    uint16_t id;
    uint8_t  length;
    void    *data;
} STORAGE_ITEM_T;

typedef struct {
    STORAGE_ITEM_T       storage_item_table[STORAGE_ITEM_MAX_COUNT];
    HAL_STORAGE_STATUS_E status;
    uint8_t              page;
    uint8_t              item_count;
    uint32_t             current;
} HAL_STORAGE_INFO_HANDLE_T;

/*
*************************************************************************************************************************
*                                                   PRIVATE VARIABLES
*************************************************************************************************************************
*/
static HAL_STORAGE_INFO_HANDLE_T storage_page_info;
/*
*************************************************************************************************************************
*                                                    PUBLIC VARIABLES
*************************************************************************************************************************
*/

/*
*************************************************************************************************************************
*                                               PRIVATE (HELPER) FUNCTIONS
*************************************************************************************************************************
*/
uint16_t CRC16_Modbus(const uint8_t *data, uint32_t length)
{
    uint16_t crc = 0xFFFFU;

    for (uint32_t i = 0; i < length; i++) {
        crc ^= data[i];

        for (uint8_t bit = 0; bit < 8; bit++) {
            if (crc & 0x0001U) {
                crc = (crc >> 1) ^ 0xA001U;
            } else {
                crc >>= 1;
            }
        }
    }

    return crc;
}

uint32_t CRC32_Calculate(const uint8_t *data, uint32_t length)
{
    uint32_t crc = 0xFFFFFFFFUL;

    for (uint32_t i = 0; i < length; i++) {
        crc ^= data[i];

        for (uint8_t bit = 0; bit < 8U; bit++) {
            if (crc & 0x00000001UL) {
                crc = (crc >> 1U) ^ 0xEDB88320UL;
            } else {
                crc >>= 1U;
            }
        }
    }

    return crc ^ 0xFFFFFFFFUL;
}

static void Hal_Storage_PageInit(uint32_t page, uint32_t seq)
{
    DRV_FLASH_FUNC_HANDLE_T *drv_func = DRV_Flash_GetFuncHandle();
    uint32_t                 address  = STORAGE_PAGE_ADDR(page);

    STORAGE_PAGE_HEADER_T    header;
    header.magic  = DEF_STORAGE_PAGE_MAGIC;
    header.status = DEF_STORAGE_PAGE_ACTIVE;
    header.seq    = seq;

    uint32_t crc  = CRC32_Calculate(&header, sizeof(header) - sizeof(uint32_t));
    header.crc    = crc;

    drv_func->flash_write(address, &header, sizeof(header));
}

static inline void Hal_Storage_GetPageHeader(uint8_t page, STORAGE_PAGE_HEADER_T *head)
{
    DRV_FLASH_FUNC_HANDLE_T *drv_func = DRV_Flash_GetFuncHandle();
    drv_func->flash_read(STORAGE_PAGE_ADDR(page), head, sizeof(STORAGE_PAGE_HEADER_T));
}

static inline void Hal_Storage_PageSetFull(uint32_t page)
{
    DRV_FLASH_FUNC_HANDLE_T *drv_func = DRV_Flash_GetFuncHandle();

    uint32_t                 buff[2];

    buff[0] = DEF_STORAGE_PAGE_FULL;
    buff[1] = DEF_STORAGE_PAGE_FULL;

    drv_func->flash_write(STORAGE_PAGE_ADDR(page), &buff, sizeof(buff));
}

static STORAGE_PAGE_STATUS_E Hal_Storage_FindActivePage(uint32_t *page)
{
    uint8_t  i;
    uint32_t fault_page_mask = 0;

    for (i = 0; i < STORAGE_USE_PAGE_NUM; i++) {
        STORAGE_PAGE_HEADER_T head;

        Hal_Storage_GetPageHeader(i, &head);
        *page = i;
        if (head.magic == DEF_STORAGE_PAGE_MAGIC && head.status == DEF_STORAGE_PAGE_ACTIVE) {
            STORAGE_PAGE_HEADER_T last_head;

            uint8_t               j = (i - 1) & (STORAGE_USE_PAGE_NUM - 1);
            Hal_Storage_GetPageHeader(j, &last_head);

            uint32_t crc = CRC32_Calculate(&head, sizeof(STORAGE_PAGE_HEADER_T) - 4);

            if (last_head.magic == DEF_STORAGE_PAGE_ERASED && last_head.status == DEF_STORAGE_PAGE_ERASED) {
                if (crc == head.crc) {
                    *page = i;
                    return STORAGE_PAGE_ACTIVE;
                } else {
                    fault_page_mask |= 1 << i;
                }
            } else if (last_head.magic == DEF_STORAGE_PAGE_FULL && last_head.status == DEF_STORAGE_PAGE_FULL) {
                if (crc == head.crc) {
                    *page = i;
                    return STORAGE_PAGE_RECEIVE;
                } else {
                    fault_page_mask |= 1 << i;
                }
            } else {
                fault_page_mask |= 1 << i;
            }
        } else if (head.magic == 0 && head.status == 0) {
            if (head.seq != 0 && head.crc != 0) {
                *page = i;
                return STORAGE_PAGE_FULL;
            } else {
                fault_page_mask |= 1 << i;
            }
        } else if (head.magic != DEF_STORAGE_PAGE_ERASED || head.status != DEF_STORAGE_PAGE_ERASED) {
            fault_page_mask |= 1 << i;
        }
    }

    if (fault_page_mask == 0) {
        *page = 0;
        return STORAGE_PAGE_FIRST; /* 扫完都是擦除态 */
    } else {
        *page = fault_page_mask;
        return STORAGE_PAGE_INVALID;
    }
}

static uint32_t Hal_Storage_GetCurrentAddr(uint8_t page)
{
    DRV_FLASH_FUNC_HANDLE_T *drv_func   = DRV_Flash_GetFuncHandle();

    uint32_t                 start_addr = STORAGE_PAGE_ADDR(page);
    uint32_t                 end_addr   = STORAGE_PAGE_ADDR(page) + FLASH_PAGE_SIZE;

    while (start_addr + 8 <= end_addr) {
        uint64_t value;
        drv_func->flash_read(start_addr, &value, sizeof(value));

        if (value == 0xFFFFFFFFFFFFFFFFU) {
            break;
        }

        start_addr += 8;
    }
    return start_addr;
}

static void Hal_Storage_DataWrite(uint16_t id, void *data, uint8_t length)
{
    HAL_STORAGE_INFO_HANDLE_T *ptr      = &storage_page_info;
    DRV_FLASH_FUNC_HANDLE_T   *drv_func = DRV_Flash_GetFuncHandle();

#if FLASH_REPROGRAM_EN
    uint16_t write_length = DEF_STORAGE_ID_LENTH + 2U + length;
    /* [id:2][crc:2][data:length][pad to 4] */
    uint16_t align_length = (write_length + 3U) & ~3U;
    uint8_t  diff_length  = (uint8_t)(align_length - write_length);

    uint8_t  write_data[align_length];
    uint8_t  crc_src[DEF_STORAGE_ID_LENTH + length];
    uint16_t crc;

    memcpy(&write_data[0], &id, DEF_STORAGE_ID_LENTH);
    memcpy(&write_data[DEF_STORAGE_ID_LENTH + 2U], (uint8_t *)data, length);

    memcpy(&crc_src[0], &id, DEF_STORAGE_ID_LENTH);
    memcpy(&crc_src[DEF_STORAGE_ID_LENTH], (uint8_t *)data, length);
    crc = CRC16_Modbus(crc_src, DEF_STORAGE_ID_LENTH + length);
    memcpy(&write_data[DEF_STORAGE_ID_LENTH], &crc, 2U);

    for (uint8_t i = 0; i < diff_length; i++) {
        write_data[write_length + i] = 0xFF;
    }

    drv_func->flash_write(ptr->current, write_data, align_length);

    ptr->current += align_length;
#else
    /* Flash reprogramming is disabled, handle accordingly */
    uint8_t data_length  = (length + 3U) & ~3U;
    uint8_t write_length = DEF_STORAGE_ID_LENTH + 2U + data_length;

    uint8_t write_data[write_length];
    uint8_t crc_src[DEF_STORAGE_ID_LENTH + length];

    memcpy(&crc_src[0], &id, DEF_STORAGE_ID_LENTH);
    memcpy(&crc_src[DEF_STORAGE_ID_LENTH], (uint8_t *)data, length);
    uint16_t crc = CRC16_Modbus(crc_src, DEF_STORAGE_ID_LENTH + length);

    memcpy(&write_data[0], &id, DEF_STORAGE_ID_LENTH);
    memcpy(&write_data[DEF_STORAGE_ID_LENTH], &crc, 2U);
    memcpy(&write_data[DEF_STORAGE_ID_LENTH + 2U], (uint8_t *)data, length);

    drv_func->flash_write(ptr->current, write_data, write_length);

    ptr->current += write_length;
#endif
}

static uint8_t Hal_Storage_VerifDataValid(HAL_STORAGE_INFO_HANDLE_T *ptr)
{
    DRV_FLASH_FUNC_HANDLE_T *drv_func = DRV_Flash_GetFuncHandle();
    uint8_t                  i;
    uint8_t                  valid = 0;
    ptr->current                   = Hal_Storage_GetCurrentAddr(ptr->page);

    if (ptr->item_count != 0) {
        for (i = 0; i < ptr->item_count; i++) {
            uint32_t addr = ptr->current;

            while (addr >= STORAGE_PAGE_ADDR(ptr->page) + sizeof(STORAGE_PAGE_HEADER_T)) {
                uint16_t id = *(uint16_t *)addr;
                if (ptr->storage_item_table[i].id == id) {
                    uint8_t data_lenth = ptr->storage_item_table[i].length;
                    /* layout: [id:2][crc:2][data:length] */
                    uint8_t  crc_src[DEF_STORAGE_ID_LENTH + data_lenth];
                    uint16_t crc16;
                    uint16_t crc_flash;

                    drv_func->flash_read(addr, crc_src, DEF_STORAGE_ID_LENTH);
                    drv_func->flash_read(addr + DEF_STORAGE_ID_LENTH + 2U, &crc_src[DEF_STORAGE_ID_LENTH], data_lenth);
                    crc16 = CRC16_Modbus(crc_src, DEF_STORAGE_ID_LENTH + data_lenth);
                    drv_func->flash_read(addr + DEF_STORAGE_ID_LENTH, &crc_flash, 2U);

                    if (crc16 == crc_flash) {
                        valid++;
                        break;
                    }
                }
                addr -= 4;
            }
            if ((i == ptr->item_count - 1) && (valid == 0)) {
                valid = 0xFF;
                break;
            }
        }
    } else {
        valid = 0xFF;
    }

    return valid;
}

static void Hal_Storage_PageTransfer(HAL_STORAGE_INFO_HANDLE_T *ptr)
{
    DRV_FLASH_FUNC_HANDLE_T *drv_func = DRV_Flash_GetFuncHandle();
    if (ptr->item_count == 0) {
        return;
    }

    uint8_t  next_page         = (ptr->page + 1) & (STORAGE_USE_PAGE_NUM - 1);
    uint32_t next_page_current = Hal_Storage_GetCurrentAddr(next_page);

    if (next_page_current != STORAGE_PAGE_ADDR(next_page)) {
        drv_func->flash_page_erase(STORAGE_ERASE_PAGE_MASK(next_page));
    }

    Hal_Storage_PageSetFull(ptr->page);

    uint32_t seq;
    drv_func->flash_read(STORAGE_PAGE_ADDR(ptr->page) + 8, &seq, sizeof(uint32_t));

    Hal_Storage_PageInit(next_page, seq + 1);

    ptr->current = STORAGE_PAGE_ADDR(next_page) + sizeof(STORAGE_PAGE_HEADER_T);

    for (uint8_t i = 0; i < ptr->item_count; i++) {
        Hal_Storage_DataWrite(ptr->storage_item_table[i].id, ptr->storage_item_table[i].data, ptr->storage_item_table[i].length);
    }

    drv_func->flash_page_erase(STORAGE_ERASE_PAGE_MASK(ptr->page));

    ptr->page = next_page;
}

static void Hal_Storage_DataRecoverFromFlash(HAL_STORAGE_INFO_HANDLE_T *ptr)
{
    DRV_FLASH_FUNC_HANDLE_T *drv_func = DRV_Flash_GetFuncHandle();

    uint8_t                  i;

    for (i = 0; i < ptr->item_count; i++) {
        uint32_t addr = ptr->current;

        while (addr >= STORAGE_PAGE_ADDR(ptr->page) + sizeof(STORAGE_PAGE_HEADER_T)) {
            uint16_t id;
            drv_func->flash_read(addr, &id, sizeof(uint16_t));

            if (ptr->storage_item_table[i].id == id) {
                uint8_t data_lenth = ptr->storage_item_table[i].length;
                /* layout: [id:2][crc:2][data:length] */
                uint8_t  crc_src[DEF_STORAGE_ID_LENTH + data_lenth];
                uint16_t crc16;
                uint16_t crc_flash;

                drv_func->flash_read(addr, crc_src, DEF_STORAGE_ID_LENTH);
                drv_func->flash_read(addr + DEF_STORAGE_ID_LENTH + 2U, &crc_src[DEF_STORAGE_ID_LENTH], data_lenth);
                crc16 = CRC16_Modbus(crc_src, DEF_STORAGE_ID_LENTH + data_lenth);
                drv_func->flash_read(addr + DEF_STORAGE_ID_LENTH, &crc_flash, 2U);

                if (crc16 == crc_flash) {
                    drv_func->flash_read(addr + DEF_STORAGE_ID_LENTH + 2U, ptr->storage_item_table[i].data, data_lenth);
                    break;
                }
            }
            addr -= 4;
        }
    }
}

static void Hal_Storage_ErrorHandler(HAL_STORAGE_INFO_HANDLE_T *ptr)
{
    if (ptr->status != STORAGE_OK) {
        while (1) {
            /* Infinite loop to indicate error */
        }
    }
}
/*
*************************************************************************************************************************
*                                               GLOBAL FUNCTION PROTOTYPES
*************************************************************************************************************************
*/
void Hal_StorageRegister(uint16_t id, void *data, uint8_t length)
{
    HAL_STORAGE_INFO_HANDLE_T *ptr = &storage_page_info;

    if (id == DEF_STORAGE_INVALID_ID || id == DEF_STORAGE_ERASED_ID || id == DEF_STORAGE_EXTENDED_ID || data == NULL || length == 0U) {
        return;
    }

    if (ptr->item_count >= STORAGE_ITEM_MAX_COUNT) {
        return;
    }

    /*Finding duplicate ID*/
    uint8_t i;
    for (i = 0; i < ptr->item_count; i++) {
        if (ptr->storage_item_table[i].id == id) {
            return;
        }
    }

    /* Register new storage item */
    ptr->storage_item_table[ptr->item_count].id     = id;
    ptr->storage_item_table[ptr->item_count].data   = data;
    ptr->storage_item_table[ptr->item_count].length = length;

    ptr->item_count++;
}

void Hal_Storage_ParaUpdt(uint16_t id, void *data, uint8_t length)
{
    HAL_STORAGE_INFO_HANDLE_T *ptr = &storage_page_info;

#if FLASH_REPROGRAM_EN
    uint16_t align_length = (DEF_STORAGE_ID_LENTH + length + 2 + 3) & ~3;

#else
    ptr->current          = (ptr->current + FLASH_MIN_WRITE_SIZE - 1) & ~(FLASH_MIN_WRITE_SIZE - 1);

    uint16_t align_length = (DEF_STORAGE_ID_LENTH + length + 2 + FLASH_MIN_WRITE_SIZE - 1) & ~(FLASH_MIN_WRITE_SIZE - 1);
#endif

    // /* Check if the storage item is registered */
    // uint8_t i;
    // for (i = 0; i < ptr->item_count; i++) {
    //     if (ptr->storage_item_table[i].id == id) {
    //         break;
    //     }
    // }
    // if (i == ptr->item_count) {
    //     return;
    // }

    /* Calculate the aligned length for the storage item */
    if ((ptr->current + align_length) <= STORAGE_PAGE_ADDR(ptr->page) + FLASH_PAGE_SIZE) {
        Hal_Storage_DataWrite(id, data, length);
    } else {
        Hal_Storage_PageTransfer(ptr);
    }
}

void Hal_Storage_Init(void)
{
    HAL_STORAGE_INFO_HANDLE_T *ptr      = &storage_page_info;
    DRV_FLASH_FUNC_HANDLE_T   *drv_func = DRV_Flash_GetFuncHandle();
    uint32_t                   page;
    STORAGE_PAGE_STATUS_E      status;

    status = Hal_Storage_FindActivePage(&page);

    switch (status) {
        case STORAGE_PAGE_FIRST: {
            Hal_Storage_PageInit(0, 1);

            ptr->page    = 0;
            ptr->current = STORAGE_PAGE_ADDR(ptr->page) + sizeof(STORAGE_PAGE_HEADER_T);
            break;
        }

        case STORAGE_PAGE_ACTIVE:

            ptr->page    = page;
            ptr->current = Hal_Storage_GetCurrentAddr(ptr->page);
            Hal_Storage_DataRecoverFromFlash(ptr);
            break;

        case STORAGE_PAGE_FULL:

            ptr->page    = page;
            ptr->current = Hal_Storage_GetCurrentAddr(ptr->page);
            Hal_Storage_DataRecoverFromFlash(ptr);
            Hal_Storage_PageTransfer(ptr);
            break;

        case STORAGE_PAGE_RECEIVE:

            ptr->page    = (uint8_t)((page - 1U) & (STORAGE_USE_PAGE_NUM - 1U));
            ptr->current = Hal_Storage_GetCurrentAddr(ptr->page);
            Hal_Storage_DataRecoverFromFlash(ptr);
            Hal_Storage_PageTransfer(ptr);

            break;

        case STORAGE_PAGE_INVALID:

            /* Handle invalid to check and clear a fault*/
            Hal_Storage_ErrorHandler(ptr);
            break;
        default:
            break;
    }
}