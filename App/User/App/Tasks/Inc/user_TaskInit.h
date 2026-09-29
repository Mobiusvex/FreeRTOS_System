#ifndef USER_TASKINIT_H
#define USER_TASKINIT_H
#include "cmsis_os2.h"
#include "FreeRTOS.h"
#include "stream_buffer.h"

#define EXTRAM_SECTION __attribute__((section(".extram"), aligned(4)))

/* 定义 StreamBuffer 的存储区和结构体 */
#define DEFINE_EXTRAM_STREAM_BUFFER(name, size)         \
    static uint8_t name##_storage[size] EXTRAM_SECTION; \
    static StaticStreamBuffer_t name##_struct

/* 创建 StreamBuffer */
#define CREATE_EXTRAM_STREAM_BUFFER(name, size, trigger) \
    xStreamBufferCreateStatic(size, trigger, name##_storage, &name##_struct)

void userTasksInit(void);
#endif // USER_TASKINIT_H