#include <Arduino.h>
#include <WiFi.h>
#include "network/wifi_manager.h"
#include "mqtt/mqtt_manager.h"
#include "config/mqtt_config.h"
#include "util/log.h"
#include "util/device.h"

uint32_t last_print_time = 0;
static uint32_t last_publish_time = 0;

static void publish_gateway_data(void)
{
    char payload[256];
    const int n = snprintf(
        payload, sizeof(payload),
        "{\"ver\":%u,\"gatewayId\":\"%s\",\"type\":\"data\","
        "\"timestamp\":%lu,\"sequence\":%lu,\"quality\":\"good\","
        "\"value\":{\"uptime_s\":%lu,\"free_heap\":%lu}}",
        (unsigned)mqtt_cfg::PROTOCOL_VERSION,
        mqtt_get_gateway_id(),
        (unsigned long)millis(),
        (unsigned long)(device_next_sequence()),
        (unsigned long)(millis() / 1000),
        (unsigned long)ESP.getFreeHeap());

    // snprintf 返回的是完整字符串应有的长度，大于等于数组大小说明被截断了
    if (n < 0 || n >= (int)sizeof(payload)) {
        Serial.println("ERROR: payload truncated");
        return;
    }

    const mqtt_result_t result = mqtt_publish(mqtt_get_data_topic(), payload, false);
    if (result != MQTT_RESULT_OK) {
        Serial.printf("WARN: publish failed, result=%d\n", (int)result);
    }
    else {
      Serial.println("Publish success.");
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
  mqtt_manager_update();
  
  if (mqtt_manager_is_connected() &&
      millis() - last_publish_time >= mqtt_cfg::PUBLISH_INTERVAL_MS) {
      last_publish_time = millis();
      publish_gateway_data();
      LOG_I("main", "boot, heap=%u", (unsigned)ESP.getFreeHeap());
      LOG_W("wifi", "boot, heap=%u", (unsigned)ESP.getFreeHeap());
  }
}

