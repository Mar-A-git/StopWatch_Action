#include "net_time.h"

#include <M5Unified.h>
#include <WiFi.h>
#include <esp_sntp.h>
#include <time.h>

#include "net_config.h"
// Wi-Fi の設定。include/secrets.h.example を include/secrets.h にコピーして書き換える。
// secrets.h がない時は、時刻合わせをしない（RTC の時刻だけで動く）
#if __has_include("secrets.h")
#include "secrets.h"
#else
#define WIFI_SSID     ""
#define WIFI_PASSWORD ""
#endif

enum class NetState { Disabled, Idle, Connecting, Syncing };

static NetState state = NetState::Idle;
static uint32_t stateStart = 0;

// 最後に時刻を合わせられた日時（RTC の時刻で記録する）。
// 8時間たったかは、本体の経過時間（millis）ではなく RTC の時刻で比べる。
// スリープ中は millis が止まることがあるため。
// RTC_DATA_ATTR はディープスリープ中も消えない。電源OFFでは消えるが、その時は RTC の時刻も
// 消えているので、どのみち起動時に合わせ直す
RTC_DATA_ATTR static time_t lastSyncEpoch = 0;

static uint32_t retryAt = 0;      // 失敗した時、次に試してよい millis（0 なら待たない）
static uint32_t lastCheckAt = 0;  // 最後に「8時間たったか」を調べた millis
static bool checkNow = true;      // 次の netTimeUpdate ですぐに調べる（起動時・起きた時）

// NTP で時刻が届いたら立つ。
// 「システム時刻が2025年以降か」では判定できない（M5Unified が起動時に RTC の時刻を
// システム時刻へ入れるので、NTP が届く前から2025年以降になっている）
static volatile bool ntpReceived = false;

static void onNtpSync(struct timeval* tv) {
  (void)tv;
  ntpReceived = true;
}

static bool hasCredentials() {
  return strlen(WIFI_SSID) > 0 && strcmp(WIFI_SSID, "YOUR_SSID") != 0;
}

const char* netTimeStateName() {
  switch (state) {
    case NetState::Disabled:   return "no-wifi-setting";
    case NetState::Idle:       return "idle";
    case NetState::Connecting: return "connecting";
    case NetState::Syncing:    return "ntp-sync";
  }
  return "?";
}

// WiFi を切る。disconnect(true) は無線もOFFにする
static void stopWifi() {
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
}

// RTC の今の時刻を time_t にする。時刻が消えている時は 0
static time_t rtcEpoch() {
  m5::rtc_datetime_t dt;
  if (!M5.Rtc.getDateTime(&dt) || dt.date.year < 2025) return 0;
  struct tm t = {};
  t.tm_year = dt.date.year - 1900;
  t.tm_mon = dt.date.month - 1;
  t.tm_mday = dt.date.date;
  t.tm_hour = dt.time.hours;
  t.tm_min = dt.time.minutes;
  t.tm_sec = dt.time.seconds;
  return mktime(&t);
}

static void printEpoch(const char* label, time_t e) {
  struct tm t;
  localtime_r(&e, &t);
  Serial.printf("%s %04d-%02d-%02d %02d:%02d:%02d\n", label,
                t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec);
}

static void finish(uint32_t now, bool ok, time_t syncedAt = 0) {
  stopWifi();
  state = NetState::Idle;
  if (ok) {
    lastSyncEpoch = syncedAt;
    retryAt = 0;
    printEpoch("[NTP] done. last sync:", lastSyncEpoch);
    printEpoch("[NTP] next sync after:", lastSyncEpoch + NTP_RESYNC_MS / 1000);
  } else {
    retryAt = now + NTP_RETRY_MS;
    if (retryAt == 0) retryAt = 1;
    Serial.printf("[NTP] failed. retry in %lu min\n", NTP_RETRY_MS / 60000UL);
  }
}

// 時刻を合わせる必要があるか（時刻が消えている / まだ一度も合わせていない / 8時間たった）
static bool syncDue() {
  const time_t e = rtcEpoch();
  if (e == 0 || lastSyncEpoch == 0) return true;
  return e - lastSyncEpoch >= (time_t)(NTP_RESYNC_MS / 1000);
}

// NTP で取れた時刻（日本時間）を RTC へ書き込む
static void writeRtc(const struct tm& t) {
  m5::rtc_datetime_t dt;
  dt.date.year = t.tm_year + 1900;
  dt.date.month = t.tm_mon + 1;
  dt.date.date = t.tm_mday;
  dt.date.weekDay = t.tm_wday;
  dt.time.hours = t.tm_hour;
  dt.time.minutes = t.tm_min;
  dt.time.seconds = t.tm_sec;
  M5.Rtc.setDateTime(dt);
  Serial.printf("[NTP] RTC <- %04d-%02d-%02d %02d:%02d:%02d\n",
                dt.date.year, dt.date.month, dt.date.date,
                dt.time.hours, dt.time.minutes, dt.time.seconds);
}

void netTimeBegin(uint32_t now) {
  if (!hasCredentials()) {
    state = NetState::Disabled;
    Serial.println("[WIFI] include/secrets.h が未設定なので、時刻合わせはしない");
    return;
  }
  (void)now;
  // mktime / localtime を日本時間で計算させる。
  // ディープスリープから起きた時も、記録した日時と同じ基準で比べられるように最初に設定する
  setenv("TZ", NTP_TZ, 1);
  tzset();
  state = NetState::Idle;
  checkNow = true;  // 起動直後に調べる
}

void netTimeOnWake() {
  checkNow = true;
}

bool netTimeBusy() {
  return state == NetState::Connecting || state == NetState::Syncing;
}

const char* netTimeLastSyncText() {
  static char buf[24];
  if (lastSyncEpoch == 0) return "never";
  struct tm t;
  localtime_r(&lastSyncEpoch, &t);
  snprintf(buf, sizeof(buf), "%02d/%02d %02d:%02d", t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min);
  return buf;
}

void netTimeUpdate(uint32_t now) {
  switch (state) {
    case NetState::Disabled:
      return;

    case NetState::Idle:
      // 失敗した後は、30分たつまで試さない
      if (retryAt != 0 && (int32_t)(now - retryAt) < 0) return;
      // 調べるのは、起動時・起きた時と、それ以外は1分に1回（RTC の読み取りを減らす）
      if (!checkNow && now - lastCheckAt < NTP_CHECK_MS) return;
      checkNow = false;
      lastCheckAt = now;
      if (!syncDue()) return;

      Serial.printf("[WIFI] connecting to %s\n", WIFI_SSID);
      WiFi.mode(WIFI_STA);
      WiFi.begin(WIFI_SSID, WIFI_PASSWORD);  // チャンネルは指定しない
      state = NetState::Connecting;
      stateStart = now;
      return;

    case NetState::Connecting:
      if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("[WIFI] connected (%s, RSSI %d)\n",
                      WiFi.localIP().toString().c_str(), WiFi.RSSI());
        // 日本時間で NTP を始める。届いたら onNtpSync が呼ばれる
        ntpReceived = false;
        sntp_set_time_sync_notification_cb(onNtpSync);
        configTzTime(NTP_TZ, NTP_SERVER1, NTP_SERVER2);
        state = NetState::Syncing;
        stateStart = now;
      } else if (now - stateStart >= WIFI_CONNECT_TIMEOUT_MS) {
        Serial.printf("[WIFI] timeout (status %d)\n", (int)WiFi.status());
        finish(now, false);
      }
      return;

    case NetState::Syncing: {
      struct tm t;
      if (ntpReceived && getLocalTime(&t, 0) && t.tm_year + 1900 >= 2025) {
        writeRtc(t);
        sntp_stop();  // 次に合わせる時まで止めておく
        finish(now, true, mktime(&t));
      } else if (now - stateStart >= NTP_SYNC_TIMEOUT_MS) {
        Serial.println("[NTP] timeout");
        finish(now, false);
      }
      return;
    }
  }
}
