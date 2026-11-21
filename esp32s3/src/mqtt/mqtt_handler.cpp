#include <Arduino.h>
#include "mqtt_handler.h"
#include "../util/log.h"

static const char* TAG = "mqtt-rx";

void mqtt_handler_on_message(char* topic, uint8_t* payload, unsigned int length)
{
    // payload 不以 \0 结尾，必须带长度用 %.*s。
    LOG_I(TAG, "topic=%s len=%u payload=%.*s", topic, length, (int)length, (const char*)payload);
}