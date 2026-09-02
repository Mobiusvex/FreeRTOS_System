#ifndef DEBUG_FUNC_H
#define DEBUG_FUNC_H
#include "FreeRTOS.h"
#include "task.h"
#include "SEGGER_RTT.h"

#define DEBUG_FUNC_ENABLE 1

#define RTT_LOG_ENABLE 1 // 默认开启，方便初次编译

#if (RTT_LOG_ENABLE == 1)

// 开启状态：定义为实际的 RTT 函数
// 使用 ##__VA_ARGS__ 处理可变参数，兼容 GCC/ARMCC
#define RTT_PRINTF(fmt, ...) SEGGER_RTT_printf(0, fmt, ##__VA_ARGS__)

// 专门针对字符串的快捷宏（无格式化，极度省栈）
#define RTT_WRITE_STR(str) SEGGER_RTT_WriteString(0, str)
#else
// 关闭状态：定义为空（参数被彻底丢弃，不会求值）
#define RTT_PRINTF(fmt, ...) ((void)0)
#define RTT_WRITE_STR(str) ((void)0)

#endif

#endif // DEBUG_FUNC_H