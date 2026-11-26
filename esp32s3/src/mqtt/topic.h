#pragma once
#include <stddef.h>

// MQTT topic 的拼接和解析。所有 topic 字符串只在 topic.cpp 里定义。

// topic 缓冲区大小（含结尾 \0）
constexpr size_t TOPIC_MAX_LEN = 96;

// 拼好网关相关的固定 topic。gatewayId 运行时才生成，所以在 mqtt_manager_init() 里调用一次。
// 任何一个被截断都返回 false，此时不应连接 broker。
bool topic_init(const char* gateway_id);

// 以下四个在 topic_init() 成功后才有效
const char* topic_gw_status(void);     // 在线/离线，retain，遗嘱也用它
const char* topic_gw_heartbeat(void);
const char* topic_gw_error(void);
const char* topic_ctrl_req_sub(void);  // 订阅用：.../dev/+/ctrl/req

// 设备 topic 写进调用方的 buf，截断返回 false。
// 不返回内部静态缓冲区：连续拼两个时，后一个会覆盖前一个。
bool topic_dev_data(char* buf, size_t size, const char* device_id);
bool topic_dev_ctrl_ack(char* buf, size_t size, const char* device_id);

// 从收到的 ctrl/req topic 中取出 deviceId：".../dev/fan-01/ctrl/req" → "fan-01"。
// 前缀或结尾不对、deviceId 为空或含 '/'、放不进 device_id，都返回 false。
bool topic_parse_ctrl_req(const char* topic, char* device_id, size_t size);
