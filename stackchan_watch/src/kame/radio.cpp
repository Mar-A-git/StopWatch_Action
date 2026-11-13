#include "radio.h"

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

#include "config.h"

namespace radio {
namespace {

struct __attribute__((packed)) Packet {
  uint32_t magic;
  uint8_t  role;            // 1=親機 0=子機
  uint16_t seq;
  int8_t   seenAvg;         // 送り主が見ている受信レベル
  uint8_t  phase;
  uint32_t phaseElapsedMs;  // そのフェーズが始まってからの時間
  uint8_t  thrustCount;     // 突き出しの回数（増えたら新しい突き出し）
  uint16_t thrustAgeMs;     // 最後の突き出しから何ms たったか
};
constexpr uint32_t MAGIC = 0x4B4D4831;  // "KMH1"（実験機の "KMH0" とは混ざらない）
const uint8_t BROADCAST[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

struct Sample {
  uint32_t ms;
  int8_t   rssi;
  Packet   pkt;
};
QueueHandle_t queue;

// プロミスキャスで最後に見た ESP-NOW の電波（受信コールバックに受信レベルが付いてこないため）
volatile int8_t lastFrameRssi = 0;
uint8_t         lastFrameMac[6];

// 送るタスクと loop で共有するもの
portMUX_TYPE  mux = portMUX_INITIALIZER_UNLOCKED;
uint8_t       pubPhase = 0;
uint32_t      pubStartMs = 0;
volatile int8_t avgNow = -100;
uint8_t       myThrustCount = 0;
uint32_t      myThrustMs = 0;

// 相手の突き出し
bool     peerThrustSeen = false;
uint8_t  peerThrustCount = 0;
uint32_t peerThrust = 0;

// 受信の集計（loop だけが触る）
int8_t   avgBuf[AVG_N];
int      avgCount = 0, avgHead = 0;
uint32_t lastRecvMs = 0;
int      recvThisSec = 0, recvLastSec = 0;
uint32_t secStartMs = 0;
bool     haveParent = false;
uint8_t  parentPh = 0;
uint32_t parentStart = 0;

void onPromiscuous(void* buf, wifi_promiscuous_pkt_type_t type) {
  if (type != WIFI_PKT_MGMT) return;
  const auto* p = (const wifi_promiscuous_pkt_t*)buf;
  const uint8_t* f = p->payload;
  if (p->rx_ctrl.sig_len < 28) return;
  // Action フレームで、中身が ESP-NOW（カテゴリ127・Espressif の OUI 18:FE:34）のもの
  if (f[0] != 0xD0) return;
  if (f[24] != 127 || f[25] != 0x18 || f[26] != 0xFE || f[27] != 0x34) return;
  memcpy(lastFrameMac, f + 10, 6);
  lastFrameRssi = (int8_t)p->rx_ctrl.rssi;
}

void onRecv(const uint8_t* mac, const uint8_t* data, int len) {
  if (len != sizeof(Packet)) return;
  Packet pkt;
  memcpy(&pkt, data, sizeof(pkt));
  if (pkt.magic != MAGIC) return;
  if (pkt.role == (IS_PARENT ? 1 : 0)) return;
  if (memcmp(mac, lastFrameMac, 6) != 0) return;
  Sample s{millis(), lastFrameRssi, pkt};
  xQueueSend(queue, &s, 0);
}

void sendTask(void*) {
  uint16_t seq = 0;
  TickType_t last = xTaskGetTickCount();
  for (;;) {
    Packet pkt{};
    pkt.magic = MAGIC;
    pkt.role = IS_PARENT ? 1 : 0;
    pkt.seq = seq++;
    pkt.seenAvg = avgNow;
    portENTER_CRITICAL(&mux);
    pkt.phase = pubPhase;
    pkt.phaseElapsedMs = millis() - pubStartMs;
    pkt.thrustCount = myThrustCount;
    {
      const uint32_t age = millis() - myThrustMs;
      pkt.thrustAgeMs = age > 65535 ? 65535 : age;
    }
    portEXIT_CRITICAL(&mux);
    esp_now_send(BROADCAST, (const uint8_t*)&pkt, sizeof(pkt));
    vTaskDelayUntil(&last, pdMS_TO_TICKS(SEND_PERIOD_MS));
  }
}

void handle(const Sample& s) {
  avgBuf[avgHead] = s.rssi;
  avgHead = (avgHead + 1) % AVG_N;
  if (avgCount < AVG_N) avgCount++;
  int sum = 0;
  for (int i = 0; i < avgCount; i++) sum += avgBuf[i];
  avgNow = (int8_t)lroundf((float)sum / avgCount);

  lastRecvMs = s.ms;
  recvThisSec++;

  // 相手の突き出し（最初に受け取った時の回数は「前からあったもの」として数えない）
  if (!peerThrustSeen) {
    peerThrustSeen = true;
    peerThrustCount = s.pkt.thrustCount;
  } else if (s.pkt.thrustCount != peerThrustCount) {
    peerThrustCount = s.pkt.thrustCount;
    peerThrust = s.ms - s.pkt.thrustAgeMs;
    Serial.printf("[LINK] peer thrust (%u ms ago)\n", s.pkt.thrustAgeMs);
  }

  if (s.pkt.role == 1) {
    haveParent = true;
    parentPh = s.pkt.phase;
    parentStart = s.ms - s.pkt.phaseElapsedMs;
  }
}

}  // namespace

void begin() {
  queue = xQueueCreate(64, sizeof(Sample));

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  esp_wifi_set_ps(WIFI_PS_NONE);

  wifi_promiscuous_filter_t filt{};
  filt.filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT;
  esp_wifi_set_promiscuous_filter(&filt);
  esp_wifi_set_promiscuous_rx_cb(onPromiscuous);
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_channel(WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE);

  if (esp_now_init() != ESP_OK) {
    Serial.println("[LINK] esp_now_init FAILED");
    return;
  }
  esp_now_register_recv_cb(onRecv);

  esp_now_peer_info_t peer{};
  memcpy(peer.peer_addr, BROADCAST, 6);
  peer.channel = 0;
  peer.ifidx = WIFI_IF_STA;
  peer.encrypt = false;
  esp_now_add_peer(&peer);

  esp_wifi_set_max_tx_power(TX_POWER);

  // 送るのは core 0（loop は core 1）
  xTaskCreatePinnedToCore(sendTask, "send", 4096, nullptr, 2, nullptr, 0);

  Serial.printf("[LINK] ready  role=%s  mac=%s  ch=%d\n",
                IS_PARENT ? "PARENT" : "CHILD", WiFi.macAddress().c_str(), WIFI_CHANNEL);
}

void update() {
  Sample s;
  while (xQueueReceive(queue, &s, 0) == pdTRUE) handle(s);

  const uint32_t now = millis();
  if (now - secStartMs >= 1000) {
    recvLastSec = recvThisSec;
    recvThisSec = 0;
    secStartMs = now;
  }
}

bool linked() {
  return lastRecvMs != 0 && millis() - lastRecvMs < LINK_LOST_MS;
}

int8_t rssiAvg() { return avgNow; }

int recvPerSec() { return recvLastSec; }

void publish(uint8_t phase, uint32_t phaseStartMs) {
  portENTER_CRITICAL(&mux);
  pubPhase = phase;
  pubStartMs = phaseStartMs;
  portEXIT_CRITICAL(&mux);
}

void noteThrust(uint32_t ms) {
  portENTER_CRITICAL(&mux);
  myThrustCount++;
  myThrustMs = ms;
  portEXIT_CRITICAL(&mux);
}

uint32_t peerThrustMs() { return peerThrust; }

bool parentPhase(uint8_t& phase, uint32_t& startMs) {
  if (!haveParent || !linked()) return false;
  phase = parentPh;
  startMs = parentStart;
  return true;
}

}  // namespace radio
