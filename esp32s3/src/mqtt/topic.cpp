#include <Arduino.h>
#include "topic.h"
#include "../util/log.h"

static const char* TAG = "topic";

// ==================== Topic 定义 ====================
// 前缀用宏，才能和后面的字面量在编译期拼成一个字符串（constexpr const char* 做不到）。
// 它是公共 broker 上的命名空间，要带项目独有的部分，避免和别人撞。
#define TOPIC_PREFIX "twin-plant-zfj"

// 两层结构：
//   gw  = 网关，即这块 ESP32。gatewayId 运行时由 MAC 生成，如 esp32-a1b2c3d4e5f6。
//   dev = 3D 场景里的设备。deviceId 是固定约定（tank-01、fan-01 ...），见 scene-contract.json。
// 每条格式串只有一个 %s，填 gatewayId 或 deviceId。规范以 docs/protocol-v1.md 为准。
static constexpr const char* GW_STATUS_FMT    = TOPIC_PREFIX "/gw/%s/status";
static constexpr const char* GW_HEARTBEAT_FMT = TOPIC_PREFIX "/gw/%s/heartbeat";
static constexpr const char* GW_ERROR_FMT     = TOPIC_PREFIX "/gw/%s/error";
static constexpr const char* DEV_DATA_FMT     = TOPIC_PREFIX "/dev/%s/data";
static constexpr const char* DEV_CTRL_REQ_FMT = TOPIC_PREFIX "/dev/%s/ctrl/req";  // 前端发布，网关订阅
static constexpr const char* DEV_CTRL_ACK_FMT = TOPIC_PREFIX "/dev/%s/ctrl/ack";

// ==================== 内部变量 ====================
static char gw_status[TOPIC_MAX_LEN];
static char gw_heartbeat[TOPIC_MAX_LEN];
static char gw_error[TOPIC_MAX_LEN];
static char ctrl_req_sub[TOPIC_MAX_LEN];

// ==================== 内部函数 ====================
// snprintf 返回完整结果应有的长度，>= size 说明被截断
static bool format(char* buf, size_t size, const char* fmt, const char* id)
{
    const int n = snprintf(buf, size, fmt, id);
    return n >= 0 && (size_t)n < size;
}

static bool init_one(char* buf, const char* fmt, const char* id, const char* what)
{
    if (format(buf, TOPIC_MAX_LEN, fmt, id))
    {
        return true;
    }
    LOG_E(TAG, "%s truncated, buf=%u", what, (unsigned)TOPIC_MAX_LEN);
    return false;
}

// ==================== 对外函数 ====================
bool topic_init(const char* gateway_id)
{
    // 用 &= 而不是 &&，每一项都检查，失败的都会打印出来
    bool ok = true;
    ok &= init_one(gw_status, GW_STATUS_FMT, gateway_id, "gw/status");
    ok &= init_one(gw_heartbeat, GW_HEARTBEAT_FMT, gateway_id, "gw/heartbeat");
    ok &= init_one(gw_error, GW_ERROR_FMT, gateway_id, "gw/error");
    ok &= init_one(ctrl_req_sub, DEV_CTRL_REQ_FMT, "+", "ctrl/req sub");
    return ok;
}

const char* topic_gw_status(void)    { return gw_status; }
const char* topic_gw_heartbeat(void) { return gw_heartbeat; }
const char* topic_gw_error(void)     { return gw_error; }
const char* topic_ctrl_req_sub(void) { return ctrl_req_sub; }

bool topic_dev_data(char* buf, size_t size, const char* device_id)
{
    return format(buf, size, DEV_DATA_FMT, device_id);
}

bool topic_dev_ctrl_req(char* buf, size_t size, const char* device_id)
{
    return format(buf, size, DEV_CTRL_REQ_FMT, device_id);
}

bool topic_dev_ctrl_ack(char* buf, size_t size, const char* device_id)
{
    return format(buf, size, DEV_CTRL_ACK_FMT, device_id);
}
