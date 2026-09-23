/*
*************************************************************************************************************************
*                                                       DRV_FLASH
*                            Driver module for the 500W Super-Capacitor Digital Power Supply.
* Filename      : drv_flash.h
* Version       : V1.00.00
* Programmer(s) : RuiYuan Lin
*************************************************************************************************************************
* Note(s)       : TBD
*************************************************************************************************************************
*                                                  MODIFICATION HISTORY
*************************************************************************************************************************
*   Version   Date          Author        Description
*   V1.00.00  2026-09-01    RuiYuan Lin   Initial release
*/
#ifndef DRV_FLASH_H
#define DRV_FLASH_H

/*
*************************************************************************************************************************
*                                                     INCLUDE FILES
*************************************************************************************************************************
*/
#include "platform_config.h"

/*
*************************************************************************************************************************
*                                                     EXTERN DEFINES
*************************************************************************************************************************
*/

#ifdef DEF_DRV_FLASH
#define EXT_DRV_FLASH
#else
#define EXT_DRV_FLASH extern
#endif

/*
*************************************************************************************************************************
*                                                     PUBLIC DEFINES
*************************************************************************************************************************
*/
#define FLASH_BANK_MEM_SIZE  0x10000U

#define STORAGE_SIZE         (STORAGE_END_ADDR - STORAGE_START_ADDR)
#define STORAGE_USE_PAGE_NUM (STORAGE_SIZE / FLASH_PAGE_SIZE)
#define STORAGE_PAGE_ADDR(x) (STORAGE_START_ADDR + ((x) * FLASH_PAGE_SIZE))

/* Storage page bit mask for selecting specific storage pages */
#define STORAGE_ERASE_PAGE_MASK(x) (1U << (x))

/*
*************************************************************************************************************************
*                                                      PUBLIC TYPES
*************************************************************************************************************************
*/
typedef void (*DEF_FLASH_WRITE)(uint32_t address, void *data, uint32_t length);
typedef void (*DEF_FLASH_READ)(uint32_t address, void *data, uint32_t length);
typedef void (*DEF_FLASH_ERASE_PAGE)(uint32_t mask);

typedef struct drv_flash_func_handle_t {
    DEF_FLASH_WRITE      flash_write;
    DEF_FLASH_READ       flash_read;
    DEF_FLASH_ERASE_PAGE flash_page_erase;
} DRV_FLASH_FUNC_HANDLE_T;
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
*                                               GLOBAL FUNCTION PROTOTYPES
*************************************************************************************************************************
*/
EXT_DRV_FLASH RAM_FUNC void                     Drv_Flash_Write(uint32_t address, void *data, uint32_t length);
EXT_DRV_FLASH RAM_FUNC void                     Drv_Flash_Read(uint32_t address, void *data, uint32_t length);
EXT_DRV_FLASH RAM_FUNC void                     Drv_Flash_Page_Erase(uint32_t mask);
EXT_DRV_FLASH RAM_FUNC DRV_FLASH_FUNC_HANDLE_T *DRV_Flash_GetFuncHandle(void);

#endif /* DRV_FLASH_H */
