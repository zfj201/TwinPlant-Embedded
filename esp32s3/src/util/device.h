#pragma once
#include <stdint.h>

const char *device_gateway_id(void); // 返回网关 ID 字符串
uint32_t device_sequence(void);      // 读当前序号，日志用
uint32_t device_next_sequence(void); // 序号加 1 并返回，发数据时用