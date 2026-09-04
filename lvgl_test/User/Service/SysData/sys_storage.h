// sys_storage.h
#ifndef __SYS_STORAGE_H
#define __SYS_STORAGE_H

#include "sys_defs.h"
#include "sys_data.h"
#include <stdint.h>

SYS_StatusTypeDef SYS_Storage_Load(FlashStorage_t *data, uint16_t size);
SYS_StatusTypeDef SYS_Storage_Save(const FlashStorage_t *data, uint16_t size);

#endif