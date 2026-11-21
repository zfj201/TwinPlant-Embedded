#include <Arduino.h>
#include "mqtt_manager.h"
#include "mqtt_handler.h"
#include "../config/mqtt_config.h"
#include <PubSubClient.h>
#include <WiFi.h>
#include "../network/wifi_manager.h"
#include "../util/device.h"

// ==================== 状态定义 ====================
// 不设置连接中状态，因为PubSubClient 的 connect() 会一直阻塞到成功或超时，所以 CONNECTING 只存在于一次 update() 调用内部
enum class MQTTState
{
    DISCONNECTED,
    CONNECTED,
    WAIT_RETRY
};


// ==================== 内部变量 ====================
static WiFiClient wifiClient;
static PubSubClient mqttClient(wifiClient);

static MQTTState current_state = MQTTState::DISCONNECTED;
// 当前状态进入时的时间
static uint32_t state_enter_time = 0;
// 当前退避时间
static uint32_t backoff_time = mqtt_cfg::RECONNECT_BACKOFF_INIT_MS;
// 重试需要等待的时间
static uint32_t retry_wait_time = 0;

// 网关 ID从 device 获取。
static const char* gateway_id;
// 完整 topic，init() 里用 mqtt_cfg 的格式串拼一次，之后一直用。
static char topic_gw_status[mqtt_cfg::TOPIC_MAX_LEN];
static char topic_gw_data[mqtt_cfg::TOPIC_MAX_LEN];
static char topic_dev_ctrl_sub[mqtt_cfg::TOPIC_MAX_LEN];
// status 消息，init() 里拼一次。offline 是连接时交给 broker 的遗嘱，之后改不了，
// 所以不带 timestamp、sequence 这类会随时间变旧的字段。
static char payload_status_online[128];
static char payload_status_offline[128];
// init() 里任何一项检查失败就置 false，此时 update() 不去连接，避免用被截断的 topic 连上 broker。
static bool init_ok = false;

// ==================== 内部函数 ====================
static void set_state(MQTTState new_state)
{
    current_state = new_state;
    state_enter_time = millis();
}

static bool try_connect()
{
    Serial.println("MQTT connecting");
    const uint32_t t0 = millis();
    // 连接的同时设立遗嘱消息。
    bool success = mqttClient.connect(gateway_id,topic_gw_status,1,true,payload_status_offline);
    uint32_t take_time = millis() - t0;
    if (success)
    {
        Serial.printf(
            "MQTT Connected! Take %lu ms\n",
            (unsigned long)take_time);
    }
    else
    {
        Serial.printf(
            "MQTT Connect Failed! State: %d, Take %lu ms\n",
            mqttClient.state(),
            (unsigned long)take_time);
    }
    return success;
}

// 连接失败，进入等待重试
static void schedule_retry()
{
    mqttClient.disconnect();

    retry_wait_time = backoff_time + random(0, mqtt_cfg::RECONNECT_JITTER_MS + 1);

    Serial.printf(
        "Retry connect after %lu ms\n",
        (unsigned long)retry_wait_time);
    backoff_time *= 2;
    if (backoff_time > mqtt_cfg::RECONNECT_BACKOFF_MAX_MS)
    {
        backoff_time = mqtt_cfg::RECONNECT_BACKOFF_MAX_MS;
    }
    set_state(MQTTState::WAIT_RETRY);
}

// 连接成功发布上线消息、订阅控制topic
// 以后要加新动作（订阅别的 topic、发其他消息），在这里往下加一段即可，update() 不用动。
// 前提：current_state 已经是 CONNECTED，因为 mqtt_publish() 会检查它。
static bool on_connected()
{
    // 1. 宣告上线（retain），覆盖掉遗嘱留下的 offline。
    const mqtt_result_t publish_res = mqtt_publish(topic_gw_status, payload_status_online, true);
    if (publish_res != MQTT_RESULT_OK)
    {
        Serial.printf("WARN: online message publish failed, result=%d\n", (int)publish_res);
        return false;
    }

    // 2. 订阅控制请求。clean session 下重连后订阅不会保留，所以每次连上都要订。
    if (!mqttClient.subscribe(topic_dev_ctrl_sub, 1))
    {
        Serial.printf("WARN: subscribe %s failed\n", topic_dev_ctrl_sub);
        return false;
    }

    return true;
}

// 校验是否超限截断
static bool format_ok(int n, size_t buf_size, const char* what)
{
    if (n < 0 || (size_t)n >= buf_size)
    {
        Serial.printf("ERROR: %s truncated or format error (n=%d, buf=%u)\n",
                      what, n, (unsigned)buf_size);
        return false;
    }
    return true;
}

// ==================== 对外函数 ====================
void mqtt_manager_init(void)
{
    Serial.println("MQTT Manager Init");
    init_ok = false;
    // 用 &= 而不是 &&，这样每一项都会检查并打印，不会因为前面失败就漏掉后面的。
    bool ok = true;

    gateway_id = device_gateway_id();

    // 3. 生成 Topic
    ok &= format_ok(snprintf(
        topic_gw_status,
        sizeof(topic_gw_status),
        mqtt_cfg::TOPIC_GW_STATUS_FMT,
        mqtt_cfg::TOPIC_PREFIX,
        gateway_id),
        sizeof(topic_gw_status), "topic_gw_status");

    ok &= format_ok(snprintf(
        topic_gw_data,
        sizeof(topic_gw_data),
        mqtt_cfg::TOPIC_GW_DATA_FMT,
        mqtt_cfg::TOPIC_PREFIX,
        gateway_id),
        sizeof(topic_gw_data), "topic_gw_data");

    ok &= format_ok(snprintf(
        topic_dev_ctrl_sub,
        sizeof(topic_dev_ctrl_sub),
        mqtt_cfg::TOPIC_DEV_CTRL_REQ_FMT,
        mqtt_cfg::TOPIC_PREFIX,
        "+"),
        sizeof(topic_dev_ctrl_sub), "topic_dev_ctrl_sub");

    // 4. 生成 status 消息（online / offline）
    ok &= format_ok(snprintf(
        payload_status_online,
        sizeof(payload_status_online),
        "{\"ver\":%u,\"gatewayId\":\"%s\",\"type\":\"status\",\"value\":\"online\"}",
        (unsigned)mqtt_cfg::PROTOCOL_VERSION,
        gateway_id),
        sizeof(payload_status_online), "payload_status_online");

    ok &= format_ok(snprintf(
        payload_status_offline,
        sizeof(payload_status_offline),
        "{\"ver\":%u,\"gatewayId\":\"%s\",\"type\":\"status\",\"value\":\"offline\"}",
        (unsigned)mqtt_cfg::PROTOCOL_VERSION,
        gateway_id),
        sizeof(payload_status_offline), "payload_status_offline");

    // 初始化 MQTT Client
    mqttClient.setServer(mqtt_cfg::BROKER_HOST, mqtt_cfg::BROKER_PORT);
    mqttClient.setKeepAlive(mqtt_cfg::KEEPALIVE_S);
    mqttClient.setSocketTimeout(mqtt_cfg::SOCKET_TIMEOUT_S);
    // 内存不够时返回 false
    if (!mqttClient.setBufferSize(mqtt_cfg::BUFFER_SIZE))
    {
        Serial.printf("ERROR: setBufferSize(%u) failed, out of memory\n",
                      (unsigned)mqtt_cfg::BUFFER_SIZE);
        ok = false;
    }
    mqttClient.setCallback(mqtt_handler_on_message);

    init_ok = ok;
    if (!init_ok)
    {
        Serial.println("ERROR: MQTT init failed, MQTT is disabled");
    }
}

void mqtt_manager_update(void)
{
    // init 没通过就不连接，原因已经在 init 里打印过。
    if (!init_ok)
    {
        return;
    }
    if (!wifi_manager_is_connected())
    {
        if (current_state != MQTTState::DISCONNECTED)
        {
            mqttClient.disconnect();
            set_state(MQTTState::DISCONNECTED);
            backoff_time = mqtt_cfg::RECONNECT_BACKOFF_INIT_MS;
            retry_wait_time = 0;
        }
        return;
    }
    switch (current_state)
    {
    case MQTTState::DISCONNECTED:
    {
        if (!try_connect())
        {
            schedule_retry();
            break;
        }
        // 先切状态：on_connected() 里的 mqtt_publish() 会检查 current_state == CONNECTED。
        set_state(MQTTState::CONNECTED);
        if (on_connected())
        {
            // 连上并且完成上线宣告和订阅，才算真正成功，这时才清零退避。
            backoff_time = mqtt_cfg::RECONNECT_BACKOFF_INIT_MS;
            retry_wait_time = 0;
        }
        else
        {
            schedule_retry();
        }
        break;
    }

    case MQTTState::CONNECTED:
    {
        bool res = mqttClient.loop();

        if (res)
        {
        }
        else
        {
            set_state(MQTTState::DISCONNECTED);
            Serial.println("MQTT Disconnected");
        }
        break;
    }

    case MQTTState::WAIT_RETRY:
    {
        // 问：等够了吗？        → 够了就重新发起连接
        if (millis() - state_enter_time > retry_wait_time)
        {
            set_state(MQTTState::DISCONNECTED);
        }
    }
    break;
    }
}

bool mqtt_manager_is_connected(void)
{
    // 不能只用current state来判断，因为有可能状态机没来得及切换，但实际已经断开连接了。
    return current_state == MQTTState::CONNECTED && mqttClient.connected();
}

mqtt_result_t mqtt_publish(const char* topic, const char* payload, bool retained)
{
    // 1. 检查连接状态
    if (current_state != MQTTState::CONNECTED ||
        !mqttClient.connected()) {

        return MQTT_RESULT_NOT_CONNECTED;
    }

    // 2. 检查参数
    if (topic == nullptr || payload == nullptr ||
        topic[0] == '\0') {

        return MQTT_RESULT_PUBLISH_FAILED;
    }

    // 3. 计算 Topic 和 Payload 长度
    const size_t topic_len = strlen(topic);
    const size_t payload_len = strlen(payload);

    // MQTT 最大固定报头预留 5 字节
    constexpr size_t HEADER_SIZE = 5;
    constexpr size_t TOPIC_LENGTH_SIZE = 2;

    const size_t required_size =
        HEADER_SIZE +
        TOPIC_LENGTH_SIZE +
        topic_len +
        payload_len;

    // 4. 检查缓冲区容量
    if (required_size > mqtt_cfg::BUFFER_SIZE) {
        return MQTT_RESULT_PAYLOAD_TOO_LARGE;
    }

    // 5. 执行发布
    const bool success = mqttClient.publish(
        topic,
        payload,
        retained
    );

    if (success) {
        return MQTT_RESULT_OK;
    }

    return MQTT_RESULT_PUBLISH_FAILED;
}

const char* mqtt_get_data_topic()
{
    return topic_gw_data;
}
const char* mqtt_get_gateway_id()
{
    return gateway_id;
}