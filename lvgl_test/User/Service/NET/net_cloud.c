#include "net_cloud.h"
#include "net_wifi.h"
#include "stddef.h"
#include "stdio.h"
#include <stdlib.h>
#include <string.h>
#include "debug_func.h"
#include "cmsis_os2.h"
#include "sys_data.h"

const char *onenet_devid = "QlVhcDm9e2";
const char *onenet_product = "0001";
const char *token = "version=2018-10-31&res=products%2FQlVhcDm9e2%2Fdevices%2F0001&et=2719970983&method=md5&sign=n3OIqsKc1Tuq%2FZSMFV0HNA%3D%3D";

#define CLOUD_CMD_BUF_LENGTH 300
enum {
    CLOUD_LED,
    CLOUD_CODE_NUM
};

typedef enum {
    NET_CLOUD_CWMODE1_SET,
    NET_CLOUD_CWDHCP1_SET,
    NET_CLOUD_MQTTUSERCFG,
    NET_CLOUD_MQTT_CONNECT,
    NET_CLOUD_MQTT_SUB_SET,
    NET_CLOUD_MQTT_PUB_DATA,
    NET_CLOUD_MQTT_CLEAN,
    NET_CLOUD_STEP_NUM
} net_cloud_step_t;

static const step_config_t net_cloud_step[] = {
    {"AT+CWMODE=1\r\n", "OK", 3, 3, 3000},
    {"AT+CWDHCP=1,1\r\n", "OK", 3, 3, 3000},
    {"--", "OK", 3, 3, 3000},
    {"AT+MQTTCONN=0,\"mqtts.heclouds.com\",1883,1\r\n", "OK", 3, 3, 3000},
    {"--", "OK", 3, 3, 3000},
    {"---", "OK", 3, 3, 3000},
    {"AT+MQTTCLEAN=0\r\n", "OK", 3, 3, 3000}};

const char *net_cloud_fields[CLOUD_CODE_NUM] = {
    "led",
};

static char cloud_mqtt_buf[CLOUD_CMD_BUF_LENGTH];
at_cmd_ctx_t net_cloud_step_ctx[NET_CLOUD_STEP_NUM];

static cloud_setdata_t net_cloud_setdata = {LED_OFF};

static cmd_state_t parseCloudJSON(char *jsonData, cloud_setdata_t *p_setdata);

/**
 * @brief 获取用户配置命令
 * @param buf 发送的缓冲区
 * @param buf_len 缓冲区大小
 * @param devid 设备ID
 * @param product_id 产品ID
 * @param token
 * @retval 无
 */
static void onenet_user_config_cmd_get(char *buf, uint16_t buf_len, const char *devid, const char *product_id, const char *token) {
    snprintf(buf, buf_len,
             "AT+MQTTUSERCFG=0,1,\"%s\",\"%s\",\"%s\",0,0,\"\"\r\n",
             product_id, devid, token);
}

/**
 * @brief 获取订阅命令
 * @param buf 发送的缓冲区
 * @param buf_len 缓冲区大小
 * @param devid 设备ID
 * @param product_id 产品ID
 * @retval 无
 */
static void onenet_subset_cmd_get(char *buf, uint16_t buf_len, const char *devid, const char *product_id) {
    snprintf(buf, buf_len,
             "AT+MQTTSUB=0,\"$sys/%s/%s/thing/property/set\",1\r\n",
             devid, product_id);
}

/**
 * @brief 获取发布数据命令
 * @param buf 发送的缓冲区
 * @param buf_len 缓冲区大小
 * @param devid 设备ID
 * @param product_id 产品ID
 * @param cloud_updata 上报数据
 * @retval 无
 */
static void onenet_pubdata_get(char *buf, uint16_t buf_len, const char *devid, const char *product_id, const cloud_updata_t cloud_updata) {
    snprintf(buf, buf_len,
             "AT+MQTTPUB=0,\"$sys/%s/%s/thing/property/post\",\"{\\\"id\\\":\\\"123\\\"\\,\\\"params\\\":\
{\\\"temp\\\":{\\\"value\\\":%d\\}\\,\\\"humi\\\":{\\\"value\\\":%d}\\,\\\"yaw\\\":{\\\"value\\\":%d}\\,\
\\\"roll\\\":{\\\"value\\\":%d}\\,\\\"pitch\\\":{\\\"value\\\":%d}\\,\\\"led\\\":{\\\"value\\\":%d}}}\",0,0\r\n",
             devid, product_id, cloud_updata.temp / 10, cloud_updata.humi / 10, cloud_updata.yaw / 10, cloud_updata.roll / 10, cloud_updata.pitch / 10, cloud_updata.led);
}
/**
 * @brief 初始化云平台上下文
 * @retval 无
 */
static void net_cloud_init_ctx(void) {
    for (int i = 0; i < NET_CLOUD_STEP_NUM; i++) {
        at_cmd_ctx_t *ctx = &net_cloud_step_ctx[i];
        memset(ctx, 0, sizeof(at_cmd_ctx_t));
        ctx->cmd = net_cloud_step[i].cmd;
        ctx->expect_key = net_cloud_step[i].expect;
        ctx->timeout_max_retry = net_cloud_step[i].timeout_max_retry;
        ctx->fail_max_retry = net_cloud_step[i].fail_max_retry;
        ctx->timeout_ticks = net_cloud_step[i].timeout_ms;
        ctx->is_care_for_error = true;
        if (i == NET_CLOUD_MQTT_CLEAN) {
            ctx->is_care_for_error = false;
        }
        ctx->state = CMD_STATE_IDLE;
        ctx->tx_timeout = 10;
        ctx->receive_cb = receive_standard_analysis;
    }
}

/**
 * @brief 按步骤上报数据到云平台
 * @param rx_buf 接收到的缓冲区
 * @param rx_len 接收到的缓冲区大小
 * @retval 当前状态
 */
cmd_state_t net_cloud_mode_set(uint8_t *rx_buf, uint32_t rx_buf_size) {
    static cmd_state_t state = CMD_STATE_IDLE;
    // at_cmd_ctx_t *ctx;
    static net_cloud_step_t net_cloud_current_step = NET_CLOUD_CWMODE1_SET;
    static net_cloud_step_t net_cloud_previous_step = NET_CLOUD_CWMODE1_SET;
    cloud_updata_t cloud_updata;

    if (net_cloud_current_step < NET_CLOUD_STEP_NUM) { // 确保没有越界
        at_cmd_ctx_t *ctx = &net_cloud_step_ctx[net_cloud_current_step];
        // 合成命令
        if (net_cloud_previous_step != net_cloud_current_step) {
            if (net_cloud_current_step == NET_CLOUD_MQTTUSERCFG) {
                onenet_user_config_cmd_get(cloud_mqtt_buf, CLOUD_CMD_BUF_LENGTH, onenet_devid, onenet_product, token);
                ctx->cmd = cloud_mqtt_buf;
                ctx->tx_timeout = 50;
            } else if (net_cloud_current_step == NET_CLOUD_MQTT_SUB_SET) {
                onenet_subset_cmd_get(cloud_mqtt_buf, CLOUD_CMD_BUF_LENGTH, onenet_devid, onenet_product);
                ctx->cmd = cloud_mqtt_buf;
            } else if (net_cloud_current_step == NET_CLOUD_MQTT_PUB_DATA) {
                SYS_DATA_GetEnv(&cloud_updata.temp, &cloud_updata.humi);
                SYS_DATA_GetAngle(&cloud_updata.pitch, &cloud_updata.roll, &cloud_updata.yaw);
                onenet_pubdata_get(cloud_mqtt_buf, CLOUD_CMD_BUF_LENGTH, onenet_devid, onenet_product, cloud_updata);
                ctx->cmd = cloud_mqtt_buf;
                ctx->tx_timeout = 50;
            }
        }

        state = cmd_send_and_judge_process(ctx, rx_buf, rx_buf_size);
        net_cloud_previous_step = net_cloud_current_step;

        if (state == CMD_STATE_SUCCESS || (!ctx->is_care_for_error && state == CMD_STATE_FAIL)) {
            net_cloud_current_step++;
            // 上报数据成功，下个步骤依旧是上报数据
            if (net_cloud_current_step == NET_CLOUD_MQTT_CLEAN) {
                net_cloud_current_step = NET_CLOUD_MQTT_PUB_DATA;
                state = CMD_STATE_DONE;
            } // 已经清除状态，重新连接
            else if (net_cloud_current_step == NET_CLOUD_STEP_NUM) {
                net_cloud_current_step = NET_CLOUD_CWMODE1_SET;
                state = CMD_STATE_DONE;
            }
            // 上报数据需要每次更新
            if (net_cloud_current_step == NET_CLOUD_MQTT_PUB_DATA) {
                net_cloud_previous_step = net_cloud_current_step - 1;
            }
        } else if (state == CMD_STATE_TIMEOUT || state == CMD_STATE_FAIL) {
            if (net_cloud_current_step == NET_CLOUD_MQTT_CLEAN) {
                net_cloud_current_step = NET_CLOUD_CWMODE1_SET;
            } else {
                net_cloud_current_step = NET_CLOUD_MQTT_CLEAN;
            }
        }
    }
    return state;
}

/**
 * @brief 清空状态，准备开始连接
 */
void net_cloud_init(void) {
    net_cloud_init_ctx();
}

/**
 * @brief 解析云平台下发的设置命令
 * @param buffer 接收到的缓冲区
 * @param rx_len 接收到的缓冲区大小
 * @retval 当前状态
 */
cmd_state_t cloud_setdata_analysis(uint8_t *buffer, uint32_t rx_len) {
    // 检查期望关键词
    cmd_state_t ret = CMD_STATE_FAIL;
    cloud_setdata_t data;
    if (strstr(buffer, "invalid") || strstr(buffer, "fail")) {
        ret = CMD_STATE_FAIL;
    } else {
        ret = parseCloudJSON(buffer, &data);
    }
    if (ret == CMD_STATE_SUCCESS) {
        net_cloud_setdata = data;
        RTT_PRINTF("led: %d\n", net_cloud_setdata.led);
    }
    return ret;
}

/**
 * @brief 解析json命令
 * @param jsonData 接收到的缓冲区
 * @param p_setdata 设置数据
 * @retval 当前状态
 */
static cmd_state_t parseCloudJSON(char *jsonData, cloud_setdata_t *p_setdata) {
    // TODO: 需要保护，防止数据撕裂,或者完成后通过列队发送给显示屏
    static const char cut_str[] = "{},[]/ +\":";
    cmd_state_t ret = CMD_STATE_WAIT_REPLY;
    uint8_t index = 0;
    // NOTE: strok会改变原字符串，但原字符串每次用完就扔，所以这里不拷贝一份
    char *token = strtok(jsonData, cut_str);
    while (token != NULL) {
        if (strcmp(token, net_cloud_fields[CLOUD_LED]) == 0) {
            token = strtok(NULL, cut_str);
            if (atoi(token) == 0) {
                p_setdata->led = false;
            } else {
                p_setdata->led = true;
            }
            index++;
        }
        token = strtok(NULL, cut_str);
    }
    if (index == CLOUD_CODE_NUM) {
        ret = CMD_STATE_SUCCESS;
    }
    return ret;
}

/**
 * @brief 获取设置数据
 * @retval 设置数据
 */
cloud_setdata_t net_cloud_get_setdata(void) {
    return net_cloud_setdata;
}