#pragma once
#include <stdint.h>

// 只放非敏感参数。MQTT 账号密码在 secrets.h。
namespace mqtt_cfg {

// ---------- Broker ----------
constexpr const char* BROKER_HOST = "broker.emqx.io";
constexpr uint16_t BROKER_PORT = 1883;

// ---------- 连接参数 ----------
// broker 在约 1.5 倍 keepalive 内收不到任何包，就判定设备离线并发出遗嘱。
// 它直接决定验收标准 4 的离线判定时间，所以要小于一个心跳周期。
constexpr uint16_t KEEPALIVE_S = 5;

// connect() 是阻塞调用，这个值限制它等待 broker 应答的时间。
// PubSubClient 默认 15 s，会让 loop() 卡太久。
constexpr uint16_t SOCKET_TIMEOUT_S = 3;

// PubSubClient 默认 256 字节。topic 加 payload 超过它时，publish() 只会返回 false。
constexpr uint16_t BUFFER_SIZE = 512;

// ---------- 重连退避 ----------
// 失败后的等待时间从 1 s 起，每次翻倍，最大 15 s。
constexpr uint32_t RECONNECT_BACKOFF_INIT_MS = 1000;
constexpr uint32_t RECONNECT_BACKOFF_MAX_MS = 15000;
// 在退避时间上再加 0 到这个值的随机抖动，避免 broker 重启后多块板同时重连。
constexpr uint32_t RECONNECT_JITTER_MS = 500;

// ---------- 发布 ----------
constexpr uint32_t PUBLISH_INTERVAL_MS = 2000;

// Topic 的定义和拼接在 mqtt/topic.cpp。

// ---------- 协议 ----------
constexpr uint8_t PROTOCOL_VERSION = 1;

}  // namespace mqtt_cfg
