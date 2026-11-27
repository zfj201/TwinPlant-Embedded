#pragma once
#include <stdint.h>
#include "tp_model.h"
#include <config/mqtt_config.h>
#include "topic.h"
#include "device_data.h"

// 一条待发的消息
struct MqttMsg {
    char topic[TOPIC_MAX_LEN];
    char payload[mqtt_cfg::BUFFER_SIZE];
};
// 每个设备是用单独的格式化函数，返回bool表示是否成功生成消息
bool msg_tank_data(MqttMsg* out, const TankValue& v, tp_quality_t q, MsgSrc s);
bool msg_fan_data(MqttMsg* out, const FanValue& v, tp_quality_t q, MsgSrc s);
bool msg_hv_data(MqttMsg* out, const HvValue& v, tp_quality_t q, MsgSrc s);
bool msg_incubator_data(MqttMsg* out, const IncubatorValue& v, tp_quality_t q, MsgSrc s);
bool msg_tilt_data(MqttMsg* out, const TiltValue& v, tp_quality_t q, MsgSrc s);

bool msg_heartbeat(MqttMsg* out, const HeartbeatValue& v);
bool msg_status(MqttMsg* out, bool online);    
bool msg_gw_error(MqttMsg* out, tp_err_t code, const char* text);
bool msg_ctrl_ack(MqttMsg* out, const CtrlReq& req, tp_err_t code);

tp_err_t msg_decode_ctrl_req(const char* topic, const uint8_t* payload, size_t len, CtrlReq* out);