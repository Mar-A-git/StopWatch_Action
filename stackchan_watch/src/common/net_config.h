// WiFi / NTP の時刻合わせの定数（どちらも無料の公開サーバー）
#pragma once

#include <stdint.h>

static constexpr const char* NTP_TZ      = "JST-9";
static constexpr const char* NTP_SERVER1 = "ntp.nict.jp";
static constexpr const char* NTP_SERVER2 = "pool.ntp.org";
static constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 15000;
static constexpr uint32_t NTP_SYNC_TIMEOUT_MS     = 15000;
static constexpr uint32_t NTP_RESYNC_MS = 8UL * 60 * 60 * 1000;  // 合わせられたら次は8時間後
static constexpr uint32_t NTP_RETRY_MS  = 30UL * 60 * 1000;      // 失敗したら30分後にもう一度
static constexpr uint32_t NTP_CHECK_MS  = 60UL * 1000;           // 「8時間たったか」を調べる間隔
