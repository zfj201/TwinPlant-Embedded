#pragma once
#include <stdint.h>
#include "tp_model.h"

// 数据源（mock、uart_bridge）和 MQTT 层之间的接口。只放类型，不依赖 MQTT。

enum class MsgSrc : uint8_t { MOCK, SENSOR };
enum class NodeState : uint8_t { ONLINE, OFFLINE, MOCK };

struct HeartbeatValue {
    uint32_t  uptime_s;
    uint32_t  free_heap;
    int32_t   rssi;
    NodeState f407;
    NodeState f103;
};

struct TankValue      { float level; };   // %
struct FanValue       { int32_t rpm; int32_t target_rpm; bool stalled; };
struct HvValue        { float distance; };  // m
struct IncubatorValue { bool touched; };
struct TiltValue      { float q[4]; float roll; float pitch; };  // q 为芯片坐标系 [w,x,y,z]，角度为度

// 解码后的控制请求。字符串都复制进定长数组，不指向 PubSubClient 的缓冲区，可以直接放进队列。
struct CtrlReq {
    tp_dev_t dev;
    char     key[16];         // "setRpm" / "reset"
    int32_t  value;           // setRpm 用；reset 不用
    char     request_id[32];
};
