#include <Arduino.h>
#include <WiFi.h>
#include "network/wifi_manager.h"
#include "mqtt/mqtt_manager.h"
#include "mqtt/message_fmt.h"
#include "config/mqtt_config.h"
#include "util/log.h"
#include "util/device.h"
#include "util/time_sync.h"

uint32_t last_print_time = 0;
static uint32_t last_publish_time = 0;
static const char* TAG = "main";

static void publish_gateway_heart(void)
{
    // 没接 UART 前，下游节点都是 mock
    const HeartbeatValue v{
        millis() / 1000,
        ESP.getFreeHeap(),
        WiFi.RSSI(),
        NodeState::MOCK,
        NodeState::MOCK,
    };

    MqttMsg m;
    if (!msg_heartbeat(&m, v)) {
        return;   // 原因 message_fmt 已经打印
    }

    const mqtt_result_t result = mqtt_publish(m.topic, m.payload, false);
    if (result != MQTT_RESULT_OK) {
        LOG_W(TAG, "publish failed, result=%d", (int)result);
    }
    else {
      LOG_D(TAG, "published, %u bytes", (unsigned)strlen(m.payload));
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

