#ifndef OTA_LAYOUT_H
#define OTA_LAYOUT_H

/* ============ W25Q64 外部 Flash 布局 ============ */
#define OTA_META_ADDR 0x000000U      /* 元信息扇区地址（4KB） */
#define OTA_DATA_BASE_ADDR 0x001000U /* 数据区起始地址 */

#define OTA_META_SECTOR_SIZE 4096U /* W25Q64 扇区大小 */
#define OTA_PAGE_SIZE 256U         /* W25Q64 页大小 */
#define OTA_DATA_PER_PAGE 240U     /* 每页有效数据字节数 */
#define OTA_RESERVED_SIZE 12U      /* 页内保留区大小 */
#define OTA_META_SIZE 16U          /* 元信息有效长度 */

/* ============ 元信息魔数 ============ */
#define OTA_MAGIC 0x4F544131U /* "OTA1" */

/* ============ STM32 内部 Flash APP 区 ============ */
/* ★ 根据你的芯片型号调整：Bootloader 占多少，APP 从哪开始 */
#define INTERNAL_APP_ADDR 0x08004000U /* F1: 16KB处起； */

#endif /* OTA_LAYOUT_H */