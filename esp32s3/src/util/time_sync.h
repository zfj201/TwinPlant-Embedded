#pragma once
#include <stdint.h>

// Wi-Fi 连上后立即同步
void time_sync_begin(void);

void time_sync_update(void);

bool time_is_synced(void);

// 返回时间戳，单位毫秒，UTC。未对时返回 0。
uint64_t device_now_ms(void);