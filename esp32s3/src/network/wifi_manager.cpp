#include "../config/wifi_config.h"
#include "../config/secrets.h"
#include <WiFi.h>
#include "network/wifi_manager.h"
#include <Arduino.h>
#include "../util/log.h"

// ==================== 状态定义 ====================

enum class WiFiState
{
  IDLE,
  CONNECTING,
  CONNECTED,
  WAIT_RETRY
};

// ==================== 内部变量 ====================

static WiFiState current_state = WiFiState::IDLE;
// 当前状态进入时的时间
static uint32_t state_enter_time = 0;
// 当前退避时间
static uint32_t backoff_time = wifi_cfg::BACKOFF_INIT_MS;
static const char* TAG = "wifi";

// ==================== 内部函数 ====================
// 切换状态
static void set_state(WiFiState new_state)
{
  current_state = new_state;
  state_enter_time = millis();
}

// 发起一次连接
static void start_connect()
{
  LOG_I(TAG, "connecting to \"%s\"", WIFI_SSID);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  // 企业网 xxxx（WPA2-Enterprise，EAP-PEAP + MSCHAPv2）的写法。
  // C3 只支持 2.4 GHz，扫描时没看到该 SSID，暂时不用。
  // identity 先和账号一样，库会对它调用 strlen，不能传 NULL。
  // WiFi.begin(WIFI_SSID, WPA2_AUTH_PEAP, WIFI_USERNAME, WIFI_USERNAME, WIFI_PASSWORD);

  set_state(WiFiState::CONNECTING);
}

// 连接失败，进入等待重试
static void schedule_retry()
{
  WiFi.disconnect();
  LOG_W(TAG, "retry in %lu ms", (unsigned long)backoff_time);

  set_state(WiFiState::WAIT_RETRY);
}

// ==================== 对外函数 ====================
void wifi_manager_init(void)
{
  WiFi.persistent(false);
  WiFi.setHostname(wifi_cfg::HOSTNAME);
  WiFi.mode(WIFI_STA);
  // 避免与软件重连功能冲突
  WiFi.setAutoReconnect(false);

  current_state = WiFiState::IDLE;
  state_enter_time = 0;
  backoff_time = wifi_cfg::BACKOFF_INIT_MS;
}

void wifi_manager_update(void)
{
  const uint32_t now = millis();

  switch (current_state)
  {
  case WiFiState::IDLE:
    // 问：该发起连接了吗？  → 直接发起
    start_connect();
    break;

  case WiFiState::CONNECTING:
    // 问1：连上了吗？       → 进 CONNECTED，打印 IP
    if (WiFi.status() == WL_CONNECTED)
    {
      set_state(WiFiState::CONNECTED);
      // 连接成功后重置退避时间
      backoff_time = wifi_cfg::BACKOFF_INIT_MS;
      LOG_I(TAG, "connected, ip=%s rssi=%d", WiFi.localIP().toString().c_str(), (int)WiFi.RSSI());
    }
    // 问2：超时了吗？       → 进重试
    else if (now - state_enter_time > wifi_cfg::CONNECT_TIMEOUT_MS)
    {
      LOG_W(TAG, "connect timeout, status=%d", (int)WiFi.status());
      schedule_retry();
    }
    // 顺序：先问连上，再问超时
    break;

  case WiFiState::CONNECTED:
    // 问：还在线吗？        → 不在线就进重试
    if (WiFi.status() != WL_CONNECTED)
    {
      LOG_W(TAG, "link lost, status=%d", (int)WiFi.status());
      schedule_retry();
    }
    break;

  case WiFiState::WAIT_RETRY:
    // 问：等够了吗？        → 够了就重新发起连接
    if (now - state_enter_time > backoff_time)
    {
      start_connect();
      backoff_time *= 2;
      if (backoff_time > wifi_cfg::BACKOFF_MAX_MS)
      {
        backoff_time = wifi_cfg::BACKOFF_MAX_MS;
      }
    }
    break;
  }
}

bool wifi_manager_is_connected(void)
{
  return WiFi.status() == WL_CONNECTED && current_state == WiFiState::CONNECTED;
}