#include <Arduino.h>
#include <ArduinoJson.h>
#include <math.h>
#include <string.h>
#include "message_fmt.h"
#include "../util/device.h"
#include "../util/time_sync.h"
#include "../util/log.h"

static const char* TAG = "msg";

// ==================== 映射表 ====================
// 设备字符串只在这里出现。下标是 tp_dev_t，0 号是网关，不用
static const char* const DEV_NAMES[TP_DEV_COUNT + 1] = {
    nullptr, "tank-01", "fan-01", "hv-01", "incubator-01", "tilt-01",
};

// 设备下标转name，非法编号返回 nullptr
static const char* dev_name(tp_dev_t dev) {
    return (dev > 0 && dev <= TP_DEV_COUNT) ? DEV_NAMES[dev] : nullptr;
}

// name转设备下标，非法字符串返回 0
static tp_dev_t dev_parse(const char* name) {
    for (tp_dev_t dev = 1; dev <= TP_DEV_COUNT; ++dev) {
        if (strcmp(name, DEV_NAMES[dev]) == 0) {
            return dev;
        }
    }
    return 0;
}

static const char* quality_str(tp_quality_t q) {
    switch (q) {
        case TP_QUALITY_GOOD:      return "good";
        case TP_QUALITY_UNCERTAIN: return "uncertain";
        case TP_QUALITY_BAD:       return "bad";
        default:                    return "bad";
    }
}

static const char* msg_src_str(MsgSrc s) {
    return (s == MsgSrc::MOCK) ? "mock" : "sensor";
}

static const char* node_state_str(NodeState s) {
    switch (s) {
        case NodeState::ONLINE:  return "online";
        case NodeState::OFFLINE: return "offline";
        case NodeState::MOCK:    return "mock";
        default:                 return "offline";
    }
}

// 每台设备支持哪些控制项
static bool key_allowed(tp_dev_t dev, const char* key)
{
    return (dev == TP_DEV_FAN && strcmp(key, "setRpm") == 0);
}

// --------------------- JSON 生成辅助函数 --------------------
// 保留1位小数
static float round1(float x)
{
    return roundf(x * 10.0f) / 10.0f;
}
// 保留4位小数
static float round4(float x)
{
    return roundf(x * 10000.0f) / 10000.0f;
}

// 将 topic 安全拷贝到 out->topic，仅当未发生截断时返回 true。
static bool set_topic(MqttMsg* out, const char* topic)
{
    return topic[0] != '\0' &&
           strlcpy(out->topic, topic, sizeof(out->topic)) < sizeof(out->topic);
}

// 填充公共字段：ver、type、gatewayId、timestamp、sequence
static void fill_base(JsonDocument& doc, const char* type, tp_dev_t dev)
{
    doc["ver"]       = mqtt_cfg::PROTOCOL_VERSION;
    doc["type"]      = type;
    doc["gatewayId"] = device_gateway_id();
    doc["timestamp"] = device_now_ms();   // uint64_t，ArduinoJson 在 ESP32 上默认支持 64 位整数
    doc["sequence"]  = device_next_sequence(dev);
}

// 处理设备数据的公共字段：deviceId、quality、src
static JsonObject fill_dev_data(JsonDocument& doc, tp_dev_t dev, tp_quality_t q, MsgSrc s)
{
    fill_base(doc, "data", dev);
    doc["deviceId"] = dev_name(dev);
    doc["quality"]  = quality_str(q);
    doc["src"]      = msg_src_str(s);
    return doc["value"].to<JsonObject>();
}

// 检查 JSON 文档是否溢出、payload 是否足够大，并将 JSON 序列化到 out->payload。成功返回 true。
static bool finish(const JsonDocument& doc, MqttMsg* out)
{
    // 内存不够时 ArduinoJson 不报错，只是悄悄少写字段
    if (doc.overflowed())
    {
        LOG_E(TAG, "json doc out of memory");
        return false;
    }
    const size_t need = measureJson(doc);
    if (need >= sizeof(out->payload))
    {
        LOG_E(TAG, "payload too large, need=%u buf=%u",
              (unsigned)need, (unsigned)sizeof(out->payload));
        return false;
    }
    serializeJson(doc, out->payload, sizeof(out->payload));
    return true;
}

// --------------------- MQTT 消息格式化函数 --------------------
bool msg_tank_data(MqttMsg* out, const TankValue& v, tp_quality_t q, MsgSrc s){
    if (!topic_dev_data(out->topic, sizeof(out->topic), dev_name(TP_DEV_TANK)))
    {
        
        return false;
    }
    JsonDocument doc;
    JsonObject value = fill_dev_data(doc, TP_DEV_TANK, q, s);
    value["level"] = round1(v.level);
    return finish(doc, out);
}
bool msg_fan_data(MqttMsg* out, const FanValue& v, tp_quality_t q, MsgSrc s){
    if(!topic_dev_data(out->topic, sizeof(out->topic), dev_name(TP_DEV_FAN)))
    {
        return false;
    }
    JsonDocument doc;
    JsonObject value = fill_dev_data(doc, TP_DEV_FAN, q, s);
    value["rpm"] = v.rpm;
    value["targetRpm"] = v.target_rpm;
    value["stalled"] = v.stalled;
    return finish(doc, out);
}
bool msg_hv_data(MqttMsg* out, const HvValue& v, tp_quality_t q, MsgSrc s){
    if(!topic_dev_data(out->topic, sizeof(out->topic), dev_name(TP_DEV_HV)))
    {
        return false;
    }
    JsonDocument doc;
    JsonObject value = fill_dev_data(doc, TP_DEV_HV, q, s);
    value["distance"] = round1(v.distance);
    return finish(doc, out);
}
bool msg_incubator_data(MqttMsg* out, const IncubatorValue& v, tp_quality_t q, MsgSrc s){
    if(!topic_dev_data(out->topic, sizeof(out->topic), dev_name(TP_DEV_INCUBATOR)))
    {
        return false;
    }
    JsonDocument doc;
    JsonObject value = fill_dev_data(doc, TP_DEV_INCUBATOR, q, s);
    value["touched"] = v.touched;
    return finish(doc, out);
}
bool msg_tilt_data(MqttMsg* out, const TiltValue& v, tp_quality_t q, MsgSrc s){
    if(!topic_dev_data(out->topic, sizeof(out->topic), dev_name(TP_DEV_TILT)))
    {
        return false;
    }
    JsonDocument doc;
    JsonObject value = fill_dev_data(doc, TP_DEV_TILT, q, s);

    JsonArray quat = value["q"].to<JsonArray>();
    for (int i = 0; i < 4; ++i)
    {
        quat.add(round4(v.q[i]));
    }

    value["roll"] = round1(v.roll);
    value["pitch"] = round1(v.pitch);

    return finish(doc, out);
}

bool msg_heartbeat(MqttMsg* out, const HeartbeatValue& v)
{
    if (!set_topic(out, topic_gw_heartbeat()))
    {
        return false;
    }

    JsonDocument doc;
    fill_base(doc, "heartbeat", DEVICE_GATEWAY);

    JsonObject value = doc["value"].to<JsonObject>();
    value["uptimeS"]  = v.uptime_s;
    value["freeHeap"] = v.free_heap;
    value["rssi"]     = v.rssi;

    JsonObject nodes = value["nodes"].to<JsonObject>();
    nodes["f407"] = node_state_str(v.f407);
    nodes["f103"] = node_state_str(v.f103);

    return finish(doc, out);
}
bool msg_status(MqttMsg* out, bool online){
    if(!set_topic(out, topic_gw_status()))
    {
        return false;
    }
    JsonDocument doc;
    doc["ver"] = mqtt_cfg::PROTOCOL_VERSION;
    doc["type"] = "status";
    doc["gatewayId"] = device_gateway_id();
    doc["value"] = online ? "online" : "offline";
    return finish(doc, out);
}
bool msg_gw_error(MqttMsg* out, tp_err_t code, const char* text){
    if(!set_topic(out, topic_gw_error()))
    {
        return false;
    }
    JsonDocument doc;
    fill_base(doc, "error", DEVICE_GATEWAY);
    doc["code"] = code;
    doc["msg"] = text ? text : err_text(code);
    return finish(doc, out);

}
bool msg_ctrl_ack(MqttMsg* out, const CtrlReq& req, tp_err_t code){
    if (dev_name(req.dev) == nullptr || !topic_dev_ctrl_ack(out->topic, sizeof(out->topic), dev_name(req.dev)))
    {
        
        return false;
    }
    JsonDocument doc;
    fill_base(doc, "ctrl_ack", req.dev);
    doc["deviceId"] = dev_name(req.dev);
    doc["requestId"] = req.request_id;

    if(req.key[0] != '\0')
    {
        doc["key"] = req.key;
    }
    else
    {
        doc["key"] = nullptr;
    }

    if (code == TP_OK && req.dev == TP_DEV_FAN)
    {
        doc["value"] = req.value;
    }
    else
    {
        doc["value"] = nullptr;
    }

    doc["code"] = code;
    doc["msg"] = err_text(code);
    return finish(doc, out);
}

// 解码控制请求。成功返回 TP_OK，失败返回错误码。topic、payload、len 都是 MQTT 层传进来的原始数据，out 是解码后的结果。

tp_err_t msg_decode_ctrl_req(const char* topic, const uint8_t* payload, size_t len, CtrlReq* out)
{
    // 先清空 out，之后每校验通过一项往里塞一项
    *out = {};

    // 传 const char*，ArduinoJson 会把字符串复制进 doc，不引用 PubSubClient 的缓冲区
    JsonDocument doc;
    // 是否是合法json
    if (deserializeJson(doc, (const char*)payload, len))
    {
        return TP_ERR_BAD_JSON;
    }
    if (doc["ver"].as<int>() != mqtt_cfg::PROTOCOL_VERSION)
    {
        return TP_ERR_BAD_VERSION;
    }
    if (doc["type"] != "ctrl_req")
    {
        return TP_ERR_BAD_JSON;
    }

    // payload 里的 deviceId 拼出的 topic 必须和实际收到的一致
    const char* device_id = doc["deviceId"];   // 不存在或不是字符串时为 nullptr
    char expected[TOPIC_MAX_LEN];
    if (device_id == nullptr ||
        !topic_dev_ctrl_req(expected, sizeof(expected), device_id) ||
        strcmp(expected, topic) != 0)
    {
        return TP_ERR_BAD_JSON;
    }

    const tp_dev_t dev = dev_parse(device_id);
    if (dev == 0)
    {
        return TP_ERR_UNKNOWN_DEVICE;
    }
    // 保存dev
    out->dev = dev;

    // strlcpy 返回源串长度，>= 缓冲区大小说明被截断，截断的 requestId 不能用
    const char* request_id = doc["requestId"];
    if (request_id == nullptr || request_id[0] == '\0' ||
        // 保存request_id
        strlcpy(out->request_id, request_id, sizeof(out->request_id)) >= sizeof(out->request_id))
    {
        out->request_id[0] = '\0';
        return TP_ERR_BAD_JSON;
    }

    // 从这里开始 dev 和 requestId 都有了，失败时调用方会回 ctrl/ack
    const char* key = doc["key"];
    if (key == nullptr || !key_allowed(dev, key))
    {
        return TP_ERR_UNKNOWN_KEY;
    }
    // 保存key
    strlcpy(out->key, key, sizeof(out->key));   // key_allowed 通过说明是已知短串，不会截断

    JsonVariantConst value = doc["value"];
    if (dev == TP_DEV_FAN)
    {
        // is<int32_t>()：是整数且在 int32 范围内。1200.5、"1200" 都不通过
        if (!value.is<int32_t>())
        {
            return TP_ERR_OUT_OF_RANGE;
        }
        // 保存value
        out->value = value.as<int32_t>();
    }
    else if (!value.isNull())
    {
        return TP_ERR_OUT_OF_RANGE;   // reset 只接受 null
    }

    return TP_OK;
}
