#include <Arduino.h>
#include <WiFi.h>
#include "network/wifi_manager.h"
#include "mqtt/mqtt_manager.h"
#include "mqtt/topic.h"
#include "config/mqtt_config.h"
#include "util/log.h"
#include "util/device.h"
#include "util/time_sync.h"

uint32_t last_print_time = 0;
static uint32_t last_publish_time = 0;
static const char* TAG = "main";

static void publish_gateway_heart(void)
{
    char payload[256];
    const int n = snprintf(
        payload, sizeof(payload),
        "{\"ver\":%u,\"gatewayId\":\"%s\",\"type\":\"heartbeat\","
        "\"timestamp\":%llu,\"sequence\":%lu,\"quality\":\"good\","
        "\"value\":{\"uptimeS\":%lu,\"freeHeap\":%lu}}",
        (unsigned)mqtt_cfg::PROTOCOL_VERSION,
        mqtt_get_gateway_id(),
        // Unix 毫秒(UTC)，未对时为 0。64 位，必须用 %llu。
        (unsigned long long)device_now_ms(),
        (unsigned long)(device_next_sequence()),
        (unsigned long)(millis() / 1000),
        (unsigned long)ESP.getFreeHeap());

    // snprintf 返回的是完整字符串应有的长度，大于等于数组大小说明被截断了
    if (n < 0 || n >= (int)sizeof(payload)) {
        LOG_E(TAG, "payload truncated, n=%d, buf=%u", n, (unsigned)sizeof(payload));
        return;
    }

    const mqtt_result_t result = mqtt_publish(topic_gw_heartbeat(), payload, false);
    if (result != MQTT_RESULT_OK) {
        LOG_W(TAG, "publish failed, result=%d", (int)result);
    }
    else {
      LOG_D(TAG, "published, %u bytes", (unsigned)strlen(payload));
    }
}

void setup() {
  Serial.begin(115200);
  wifi_manager_init();
  mqtt_manager_init();
  // last_print_time = 0;
}

void loop() {
  // put your main code here, to run repeatedly:
  // Serial.println("Hello, World!");
  // delay(1000);
  wifi_manager_update();
  if (wifi_manager_is_connected()) {
    time_sync_begin();   // 内部有 started 标志，每圈调用也只会执行一次
  }
  time_sync_update();
  mqtt_manager_update();
  
  if (mqtt_manager_is_connected() &&
      millis() - last_publish_time >= mqtt_cfg::PUBLISH_INTERVAL_MS) {
      last_publish_time = millis();
      publish_gateway_heart();
  }
}

