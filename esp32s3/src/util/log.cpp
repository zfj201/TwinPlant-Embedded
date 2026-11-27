#include <Arduino.h>
#include <stdarg.h>
#include "log.h"
#include "device.h"

void log_print(char level, const char* tag, const char* fmt, ...)
{
    char msg[192];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);
    // 格式：[毫秒][级别][网关ID][seq=序号] 模块: 内容（AGENTS 5.4）
    // 和某台设备相关的日志，由调用处在内容里写上那台设备的序号。如device_sequence(TP_DEV_FAN)
    Serial.printf("[%lu][%c][%s][seq=%lu] %s: %s\n",
                  (unsigned long)millis(), level, device_gateway_id(),
                  (unsigned long)device_sequence(DEVICE_GATEWAY), tag, msg);
}