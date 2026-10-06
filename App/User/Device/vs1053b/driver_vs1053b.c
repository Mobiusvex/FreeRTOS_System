#include "driver_vs1053b.h"
#include "bsp_gpio.h"
#include "bsp_spi.h"
#include "debug_func.h"
#include "cmsis_os2.h"
#include <string.h>
#include "bsp_delay.h"

/* ============ 引脚定义（按你的硬件修改） ============ */
#define VS1053_CS_LOW() BSP_GPIO_Write(BSP_GPIO_VS1053_XCS, BSP_GPIO_LOW)
#define VS1053_CS_HIGH() BSP_GPIO_Write(BSP_GPIO_VS1053_XCS, BSP_GPIO_HIGH)
#define VS1053_DCS_LOW() BSP_GPIO_Write(BSP_GPIO_VS1053_XDCS, BSP_GPIO_LOW)
#define VS1053_DCS_HIGH() BSP_GPIO_Write(BSP_GPIO_VS1053_XDCS, BSP_GPIO_HIGH)
#define VS1053_RST_LOW() BSP_GPIO_Write(BSP_GPIO_VS1053_RST, BSP_GPIO_LOW)
#define VS1053_RST_HIGH() BSP_GPIO_Write(BSP_GPIO_VS1053_RST, BSP_GPIO_HIGH)
#define VS1053_DREQ_READ() BSP_GPIO_Read(BSP_GPIO_VS1053_DREQ)

#define VS1053_SPI_BUS BSP_SPI_BUS_2
#define VS1053_SPI_CS_CMD BSP_GPIO_VS1053_XCS  /* SCI 用 */
#define VS1053_SPI_CS_DAT BSP_GPIO_VS1053_XDCS /* SDI 用 */

/* ============ 内部延时 ============ */
#define VS1053_DREQ_TIMEOUT_MS 100

/* ============================================================
 *  等待 DREQ 拉高（表示 VS1053 可以接收数据/命令）
 * ============================================================ */
/**
 * @brief  等待 DREQ 拉高（表示 VS1053 可以接收数据/命令）
 * @retval  true: DREQ 拉高，false: 超时
 */
static bool vs1053_wait_dreq(void) {
    uint32_t start = osKernelGetTickCount();
    while (!VS1053_DREQ_READ()) {
        if ((osKernelGetTickCount() - start) > VS1053_DREQ_TIMEOUT_MS) {
            RTT_PRINTF("VS1053: DREQ timeout\n");
            return false;
        }
        BSP_DelayMS_Sleep(1);
    }
    return true;
}

/**
 * @brief  写 SCI 寄存器（命令通道）
 * @param  addr: 寄存器地址
 * @param  data: 寄存器数据
 * @note 格式：0x02 + addr(1B) + data_hi(1B) + data_lo(1B)
 */
void VS1053_WriteSci(uint8_t addr, uint16_t data) {
    uint8_t buf[4];
    buf[0] = 0x02;
    buf[1] = addr;
    buf[2] = (uint8_t)(data >> 8);
    buf[3] = (uint8_t)(data & 0xFF);

    vs1053_wait_dreq();
    VS1053_DCS_HIGH(); // 确保 XDCS 高
    BSP_SPI_Write(VS1053_SPI_BUS, VS1053_SPI_CS_CMD, buf, 4, 100);
}

/**
 * @brief  读 SCI 寄存器（命令通道）
 * @param  addr: 寄存器地址
 * @retval  寄存器数据
 * @note 格式：0x03 + addr(1B) + dummy(1B) + data_hi(1B) + data_lo(1B)
 */
uint16_t VS1053_ReadSci(uint8_t addr) {
    uint8_t tx[4] = {0x03, addr, 0xFF, 0xFF};
    uint8_t rx[4] = {0};

    vs1053_wait_dreq();
    VS1053_DCS_HIGH();
    BSP_SPI_WriteRead(VS1053_SPI_BUS, VS1053_SPI_CS_CMD, tx, rx, 4, 100);

    return ((uint16_t)rx[2] << 8) | rx[3];
}

/**
 * @brief  硬复位 VS1053B
 */
void VS1053_Reset(void) {
    VS1053_RST_LOW();
    BSP_DelayMS_Sleep(10);
    VS1053_RST_HIGH();
    BSP_DelayMS_Sleep(50);
}

#define VS1053_CLOCKF_MASK 0xF800 /* SC_MULT[15:13] + SC_ADD[12:11] */
/**
 * @brief  软复位 VS1053B
 */
void VS1053_SoftReset(void) {
    uint16_t retry = 0;
    // 等 DREQ 高
    if (!vs1053_wait_dreq()) return;

    // 同时置 SM_RESET 和 SM_SDINEW，复位后 SDINEW 位保留，RESET 自动清
    while (VS1053_ReadSci(VS1053_SCI_MODE) != 0x0800) {
        VS1053_WriteSci(VS1053_SCI_MODE, 0x0804);
        BSP_DelayMS_Sleep(2);
        if (++retry > 100) break;
    }
    if (!vs1053_wait_dreq()) return;

    // CLOCKF 建议用 0x9800（3 倍频 + 1.5x ADD），对 FLAC/OGG 解码余量更足

    retry = 0;
    while ((VS1053_ReadSci(VS1053_SCI_CLOCKF) & VS1053_CLOCKF_MASK) != (0x9800 & VS1053_CLOCKF_MASK)) {
        VS1053_WriteSci(VS1053_SCI_CLOCKF, 0x9800);
        if (++retry > 100) break;
    }
    BSP_DelayMS_Sleep(20);
}

/**
 * @brief  设置音量（0x00 = 最大音量，0xFE = 静音，0xFF = 关闭）
 * @param  left: 左声道音量
 * @param  right: 右声道音量
 */
void VS1053_SetVolume(uint8_t left, uint8_t right) {
    uint16_t vol = ((uint16_t)left << 8) | right;
    VS1053_WriteSci(VS1053_SCI_VOL, vol);
}

/**
 * @brief  设置低音/高音增强
 * @param  bass:   0x00 = 关，0xF0 = 最大低音增强
 * @param  treble: 0x00 = 关，0x70 = 最大高音增强
 */
void VS1053_SetBassTreble(uint8_t bass, uint8_t treble) {
    /* 低音：低4位是频率限制（0x06 = 60Hz），高4位是幅度
       高音：低4位是频率限制（0x0A = 10kHz），高4位是幅度 */
    uint16_t bass_val = ((uint16_t)(bass & 0xF0) << 8) | 0x06;
    uint16_t treble_val = ((uint16_t)(treble & 0xF0) << 8) | 0x0A;
    VS1053_WriteSci(VS1053_SCI_BASS, bass_val | treble_val);
}

/**
 * @brief  初始化 VS1053B（MP3 播放配置）
 * @retval  true: 初始化成功，false: 初始化失败
 */
bool VS1053_Init(void) {
    RTT_PRINTF("VS1053: init...\n");

    /* 1. 硬复位 */
    VS1053_Reset();

    /* 2. 软复位（内部已包含 SM_SDINEW 和 CLOCKF 设置） */
    VS1053_SoftReset();

    /* 3. 读取状态，确认芯片在线且 SDINEW 置位 */
    /* 更可靠：读两次，必须稳定等于 0x0800；且拒绝 0xFFFF/0x0000 这种全 0/全 1 */
    uint16_t mode = VS1053_ReadSci(VS1053_SCI_MODE);
    if (mode == 0xFFFF || mode == 0x0000 || (mode & VS1053_SM_SDINEW) == 0) {
        RTT_PRINTF("VS1053: init failed, mode=0x%04X\n", mode);
        return false;
    }

    /* 4. 设置音量（0x00 = 最大，0xFE = 静音；0x20 约中等音量）
     *    建议不要一上来就最大音量，避免开机 POP 音过响 */
    VS1053_SetVolume(0xFE, 0xFE);

    /* 5. 关闭音调增强（需要低音/高音时再单独调） */
    VS1053_SetBassTreble(0x00, 0x00);

    /* 6. 可选：设置 SM_LAYER12，允许解码 MPEG Layer I/II（有些老 MP2 文件需要） */
    /* VS1053_WriteSci(VS1053_SCI_MODE, mode | 0x0002); */

    RTT_PRINTF("VS1053: init OK, mode=0x%04X\n", mode);
    return true;
}

/**
 * @brief  写 SDI 数据（音频流）
 * @param  data: 音频数据
 * @param  len: 数据长度（字节）
 * @retval  true: 写入成功，false: DREQ 超时
 */
bool VS1053_WriteSdi(const uint8_t *data, uint16_t len) {
    if (!vs1053_wait_dreq()) return false;

    BSP_SPI_Write(VS1053_SPI_BUS, VS1053_SPI_CS_DAT, data, len, 100);

    return true;
}

/**
 * @brief  阻塞式写 SDI（自动处理 DREQ 和 32 字节限制）
 * @param  data: 音频数据
 * @param  len: 数据长度（字节）
 * @retval  true: 写入成功，false: DREQ 超时
 */
bool VS1053_WriteSdiBlocking(const uint8_t *data, uint16_t len) {
    uint16_t remain = len;
    const uint8_t *p = data;

    while (remain > 0) {
        /* 等待 DREQ 就绪 */
        if (!vs1053_wait_dreq()) return false;

        /* 每次最多写 32 字节（安全值） */
        uint16_t chunk = (remain > 32) ? 32 : remain;

        BSP_SPI_Write(VS1053_SPI_BUS, VS1053_SPI_CS_DAT, p, chunk, 100);

        p += chunk;
        remain -= chunk;
    }

    return true;
}

/**
 * @brief  检查 VS1053 是否准备好接收数据（DREQ 拉高）
 * @retval  true: VS1053 已准备好，false: VS1053 未准备好
 */
bool VS1053_IsReady(void) {
    return VS1053_DREQ_READ();
}

/**
 * @brief  获取解码时间（单位：ms）
 * @retval  解码时间（单位：ms）
 */
uint16_t VS1053_GetDecodeTime(void) {
    return VS1053_ReadSci(VS1053_SCI_DECODE_TIME);
}

/**
 * @brief  获取当前播放的比特率（单位：kbps）
 * @retval  比特率（单位：kbps）
 */
uint16_t VS1053_GetBitRate(void) {
    /* 高字节 = 码率（kbps），低字节 = 0 */
    return VS1053_ReadSci(VS1053_SCI_HDAT0);
}

/**
 * @brief 读取结束填充字节（用于平滑结束播放）
 * @retval  结束填充字节（0x00-0xFF）
 */
uint16_t VS1053_GetEndFillByte(void) {
    VS1053_WriteSci(VS1053_SCI_WRAMADDR, 0x1E06);
    return VS1053_ReadSci(VS1053_SCI_WRAM);
}

/**
 * @brief  停止播放（静音并发送结束填充字节
 */
void VS1053_StopPlay(void) {
    /* 1. 先静音（0xFE = 静音，不会立即切换，等过零） */
    VS1053_SetVolume(0xFE, 0xFE);

    /* 2. 获取填充字节 */
    uint16_t fill = VS1053_GetEndFillByte();
    uint8_t fill_buf[32];
    memset(fill_buf, (uint8_t)(fill & 0xFF), sizeof(fill_buf));

    /* 3. 发送填充字节，让当前音频自然播完 */
    for (int i = 0; i < 64; i++) {
        if (!VS1053_WriteSdi(fill_buf, sizeof(fill_buf))) break;
    }
}

/**
 * @brief  设置音量百分比
 * @param  vol: 音量百分比（0~100）
 */
void VS1053_SetVolumePercent(uint8_t vol) {
    if (vol > 100) vol = 100;

    uint8_t reg;
    if (vol == 0) {
        reg = 0xFE; /* 完全静音 */
    } else {
        /* 1~100 映射到 -60dB ~ 0dB，每 0.5dB 一格
         * -60dB 对应 120 格（0x78）
         */
        reg = (uint8_t)(((100 - vol) * 120) / 100);
    }

    VS1053_SetVolume(reg, reg);
}