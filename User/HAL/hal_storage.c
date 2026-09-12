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
*   V1.00.00  2026-09-07    RuiYuan Lin   Initial release
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

/*
*************************************************************************************************************************
*                                                    PRIVATE DEFINES
*************************************************************************************************************************
*/

#define STORAGE_INVALID_ID  0x0000U
#define STORAGE_ERASED_ID   0xFFFFU
#define STORAGE_EXTENDED_ID 0xFF00U

/*
*************************************************************************************************************************
*                                                     PRIVATE TYPES
*************************************************************************************************************************
*/

typedef struct {
    uint16_t id;
    uint8_t  length;
    void    *data;
    void    *callback;
} STORAGE_ITEM_T;

typedef struct {
    STORAGE_ITEM_T storage_item_table[STORAGE_ITEM_MAX_COUNT];
    uint8_t        page;
    uint8_t        item_count;
} STORAGE_INFO_T;

/*
*************************************************************************************************************************
*                                                   PRIVATE VARIABLES
*************************************************************************************************************************
*/
static STORAGE_INFO_T storage_page_info;
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

/*
*************************************************************************************************************************
*                                               GLOBAL FUNCTION PROTOTYPES
*************************************************************************************************************************
*/
void Hal_StorageRegister(uint16_t id, void *data, uint8_t length, void *callback)
{
    STORAGE_INFO_T *ptr = &storage_page_info;
    if (id == STORAGE_INVALID_ID || id == STORAGE_ERASED_ID || id == STORAGE_EXTENDED_ID || data == NULL || length == 0U) {
        return;
    }

    if (ptr->item_count >= STORAGE_ITEM_MAX_COUNT) {
        return;
    }

    ptr->storage_item_table[ptr->item_count].id       = id;
    ptr->storage_item_table[ptr->item_count].data     = data;
    ptr->storage_item_table[ptr->item_count].length   = length;
    ptr->storage_item_table[ptr->item_count].callback = callback;

    ptr->item_count++;
}

void Hal_StorageWrite(uint16_t id, void *data, uint8_t length)
{
}

void Hal_StorageRead(uint16_t id, void *data, uint8_t length)
{
}

void Hal_StorageRecover(void)
{
    uint16_t        index;
    STORAGE_INFO_T *ptr = &storage_page_info;
    for (index = 0; index < ptr->item_count; index++) {
    }
}