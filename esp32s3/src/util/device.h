#pragma once
#include <stdint.h>
#include "tp_model.h"

// 网关自己的消息（heartbeat、error）用 0 号序号。设备编号从 1 开始，不会冲突。
constexpr tp_dev_t DEVICE_GATEWAY = 0;

const char* device_gateway_id(void);         // 返回网关 ID 字符串

// 序号按设备各自计数，重启归零。dev 取 DEVICE_GATEWAY 或 TP_DEV_*。
// 正常序号从 1 开始，dev 非法时返回 0。
uint32_t device_next_sequence(tp_dev_t dev);
uint32_t device_sequence(tp_dev_t dev); 
