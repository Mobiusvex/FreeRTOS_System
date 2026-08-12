#ifndef SYS_DEFS_H
#define SYS_DEFS_H
#include "stdint.h"
typedef enum {
    SYS_OK = 0,
    SYS_ERROR = 1,
    SYS_TIMEOUT = 2,
    SYS_BUSY = 3,
    SYS_NO_ACK = 4,
    SYS_INVALID_PARAM = 5,
    SYS_INVALID_DATA = 6
} SYS_StatusTypeDef;

#endif
