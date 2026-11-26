#pragma once
// MQTT 发布结果
enum mqtt_result_t {
    MQTT_RESULT_OK,             // 发布成功
    MQTT_RESULT_NOT_CONNECTED,  // 未连接
    MQTT_RESULT_PAYLOAD_TOO_LARGE, // 报文超过缓冲区
    MQTT_RESULT_PUBLISH_FAILED  // 发布失败
};
void mqtt_manager_init(void);   // setup() 里调用一次
void mqtt_manager_update(void); // loop() 每圈调用一次，不阻塞
bool mqtt_manager_is_connected(void);  // mqtt是否连接
// 发布 MQTT 消息
mqtt_result_t mqtt_publish(
    const char* topic,
    const char* payload,
    bool retained = false
);
const char* mqtt_get_gateway_id(void);
