/*
 * TwinPlant 应用层数据模型（F103 / F407 / ESP32-S3 共用）。
 *
 * 只放与传输层无关的内容：错误码、数据质量、设备编号、控制项取值范围。
 * topic、JSON 字段名属于 MQTT 层，UART 帧格式属于 tp_uart.h，都不放这里。
 * 前端 src/services/mqtt/type.ts 里有同一份定义，改动时两处一起改。
 */
#ifndef TP_MODEL_H
#define TP_MODEL_H

#include <stdint.h>

/* ---------- 错误码 ----------
 * F407 校验失败时返回，ESP32 原样放进 ctrl/ack 的 code，不做映射。 */
typedef uint16_t tp_err_t;
enum {
    TP_OK                 = 0,
    TP_ERR_BAD_JSON       = 1001,   /* 解析失败，拿不到 requestId */
    TP_ERR_BAD_VERSION    = 1002,
    TP_ERR_UNKNOWN_DEVICE = 1003,
    TP_ERR_UNKNOWN_KEY    = 1004,
    TP_ERR_OUT_OF_RANGE   = 1005,
    TP_ERR_INVALID_STATE  = 1006,   /* 当前状态不允许执行，如设备故障中 */
    TP_ERR_DEVICE_OFFLINE = 1007    /* 下游节点离线 */
    /* 2xxx 预留给 UART 链路（CRC、超时） */
};

/* ---------- 数据质量 ---------- */
typedef uint8_t tp_quality_t;
enum {
    TP_QUALITY_GOOD      = 0,
    TP_QUALITY_UNCERTAIN = 1,
    TP_QUALITY_BAD       = 2
};

/* ---------- 设备编号 ----------
 * 数字编号用于 UART 帧；MQTT 里用字符串 ID（tank-01 等），由 ESP32 负责转换。 */
typedef uint8_t tp_dev_t;
enum {
    TP_DEV_TANK = 1,
    TP_DEV_FAN,
    TP_DEV_HV,
    TP_DEV_INCUBATOR,
    TP_DEV_TILT,
    TP_DEV_COUNT = TP_DEV_TILT      /* 有效编号为 1..TP_DEV_COUNT */
};

/* ---------- 控制项取值范围 ---------- */
#define TP_FAN_RPM_MIN   0
#define TP_FAN_RPM_MAX   1800
#define TP_FAN_RPM_STEP  100

/* F407 执行前和 ESP32 mock 应答前都用它，保证两边判定一致。 */
static inline tp_err_t tp_fan_rpm_check(int32_t rpm)
{
    const int32_t ok = rpm >= TP_FAN_RPM_MIN && rpm <= TP_FAN_RPM_MAX &&
                       rpm % TP_FAN_RPM_STEP == 0;
    return ok ? TP_OK : TP_ERR_OUT_OF_RANGE;
}

#endif /* TP_MODEL_H */
