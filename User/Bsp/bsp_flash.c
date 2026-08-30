#include "bsp_flash.h"
#include "stm32g4xx_hal.h"

void Flash_Write(u32 address, u8 *data, u16 length)
{
    HAL_FLASH_Unlock();
}

