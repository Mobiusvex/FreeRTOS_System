#include "user_ESP8266CommTask.h"
#include "HWDataAccess.h"
#include "net_cloud.h"
#include "net_time.h"
#include "net_weather.h"
#include "net_wifi.h"
#include "FreeRTOS.h"
#include "stream_buffer.h"

extern StreamBufferHandle_t xESP8266StreamBuffer;

void user_ESP8266CommTask(void *pvParameters) {
    // 创建buffer
    uint8_t buffer[512];
    uint32_t buf_len = 0;
    net_wifi_connect();

    while (1) {
        buf_len = xStreamBufferReceiveFromISR(xESP8266StreamBuffer, buffer, 512, 0);

        net_wifi_mode_set(buffer, buf_len);
        osDelay(10);
    }
}