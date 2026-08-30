#ifndef BIZ_FDCAN_H
#define BIZ_FDCAN_H

#include "stdbool.h"
#include "config.h"

s32 fdcan_init(void);
s32 SuperCap_SendCanMsg(void);

#endif /* BIZ_FDCAN_H */