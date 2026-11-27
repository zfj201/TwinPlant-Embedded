#include <Arduino.h>
#include "device.h"
#include <esp_mac.h>

// 网关 ID，由芯片 MAC 生成，如 "esp32-a1b2c3d4e5f6"（18 个字符加结尾符）。同时用作 MQTT 的 clientId。
static char gateway_id[24];
static bool is_generate = false;
// sequences数组，+1 是因为包含网关sequences，下标 0 是网关，1..TP_DEV_COUNT 是设备。
static uint32_t sequences[TP_DEV_COUNT + 1];

const char *device_gateway_id(void)
{
    // 先生成 gateway_id，再拼 topic 和 status 消息，最后设置 MQTT Client。
    // 1. 获取 MAC 地址
    if(is_generate)
    {
        return gateway_id;
    }
    else{
        uint8_t mac[6];
        esp_read_mac(mac, ESP_MAC_WIFI_STA);
        // 2. 生成 Gateway ID
        const int n = snprintf(
            gateway_id,
            sizeof(gateway_id),
            "esp32-%02x%02x%02x%02x%02x%02x",
            mac[0], mac[1], mac[2],
            mac[3], mac[4], mac[5]);
        // 判断是否超出截断。
        if (n < 0 || (size_t)n >= sizeof(gateway_id))
        {
            Serial.printf("ERROR: gateway_id truncated or format error (n=%d, buf=%u)\n",
                          n, (unsigned)sizeof(gateway_id));
        }
        is_generate = true;
        return gateway_id;
    }
    
}

uint32_t device_next_sequence(tp_dev_t dev)
{
    if (dev > TP_DEV_COUNT)
    {
        return 0;
    }
    return ++sequences[dev];
}

uint32_t device_sequence(tp_dev_t dev)
{
    return dev > TP_DEV_COUNT ? 0 : sequences[dev];
}