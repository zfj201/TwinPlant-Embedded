#include <Arduino.h>
#include <WiFi.h>
#include <time.h>
#include "time_sync.h"
#include "log.h"

static const char *TAG = "time";

static bool synced = false;
static bool started = false;

static const char* NTP_SERVER1 = "ntp.aliyun.com";
static const char* NTP_SERVER2 = "cn.pool.ntp.org";
static const char* NTP_SERVER3 = "time.google.com";

// 用一个合理的最早时间判断是否已经对时,2023-11-14
static constexpr time_t SYNCED_MIN_EPOCH = 1700000000;

void time_sync_begin(void)
{
    if (started || WiFi.status() != WL_CONNECTED)
    {
        return;
    }
    started = true;


    // SNTP 启动后由 lwIP 定期自动校准，断网恢复后也会继续，不需要重复调用 configTime()。
    configTime(8 * 3600, 0, NTP_SERVER1, NTP_SERVER2, NTP_SERVER3);
    LOG_I(TAG, "SNTP started");
}

void time_sync_update(void)
{
    if (!started || synced)
    {
        return;
    }

    // 不用 getLocalTime()：它在没对时时会 delay(10) 轮询，会卡住 loop()。
    if (time(nullptr) < SYNCED_MIN_EPOCH)
    {
        return;
    }
    synced = true;

    struct tm t;
    const time_t now = time(nullptr);
    localtime_r(&now, &t);
    LOG_I(TAG, "synced, %04d-%02d-%02d %02d:%02d:%02d (UTC+8)",
          t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
          t.tm_hour, t.tm_min, t.tm_sec);
}

bool time_is_synced(void)
{
    return synced;
}

uint64_t device_now_ms(void)
{
    if (!synced)
    {
        return 0;
    }
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    // 先转 uint64_t 再乘，32 位 time_t 直接乘 1000 会溢出。
    return (uint64_t)tv.tv_sec * 1000ULL + (uint64_t)(tv.tv_usec / 1000);
}
