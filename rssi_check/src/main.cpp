// rssi_check: 受信レベル（RSSI）の実験機
//
// 親機と子機が、ESP-NOW で 1秒に20回ずつ合図を送り合う。
// 相手の合図を受け取った時の受信レベル（dBm。0に近いほど強い＝近い）を画面に出す。
//
//   画面の見方（上から）
//     親機／子機・送信の強さ・1秒に何回受け取れたか
//     大きな数字 … 受信レベルの平均（直近 AVG_N 回）
//     今の値・最強・最弱
//     グラフ … 直近 約8.5秒。灰色＝そのままの値、黄色＝平均
//     相手が見た値 … 相手の画面に出ている数字（2台の差を見るため）
//
//   ボタンA: 最強・最弱とグラフをリセット
//   ボタンB（親機だけ）: 送信の強さを切り替える（子機は自動で親機に合わせる）
//
// シリアル（115200bps）には、受け取るたびに CSV を1行出す:
//   ms,seq,raw,avg,peer,tx_dbm
//
// 受信レベルの取り方:
//   この Arduino（2.0.x）の ESP-NOW 受信コールバックには受信レベルが付いてこないので、
//   無線を「全部の電波を見るモード（プロミスキャス）」にして、ESP-NOW の電波（Action フレーム）
//   の受信レベルを横から読む。

#include <M5Unified.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

#ifndef ROLE_PARENT
#error "platformio.ini の env:oya か env:ko を選んでビルドしてください"
#endif

static constexpr bool IS_PARENT = (ROLE_PARENT != 0);

// ---- 設定 ----
static constexpr uint8_t  WIFI_CHANNEL   = 1;      // 2台とも同じチャンネル
static constexpr uint32_t SEND_PERIOD_MS = 50;     // 1秒に20回送る
static constexpr int      AVG_N          = 5;      // 平均を取る回数（5回＝0.25秒）
static constexpr uint32_t LINK_LOST_MS   = 500;    // これだけ受け取れなければ「未受信」
static constexpr uint32_t DRAW_PERIOD_MS = 100;    // 画面の書き直し（1秒に10回）
static constexpr bool     POWER_LED_OFF  = true;   // 本体の緑のLEDを消す

// 送信の強さ（esp_wifi_set_max_tx_power の値は 0.25dBm 単位）
// 近すぎると受信側が飽和して差が出にくいので、弱い送信も試せるようにしておく
static constexpr int8_t TX_LEVELS[] = {84, 52, 34, 8};  // 21 / 13 / 8.5 / 2 dBm
static constexpr int    TX_LEVEL_N  = sizeof(TX_LEVELS) / sizeof(TX_LEVELS[0]);

// グラフ
static constexpr int GRAPH_N    = 170;   // 点の数（20回/秒で約8.5秒）
static constexpr int GRAPH_X    = 63;
static constexpr int GRAPH_Y    = 262;
static constexpr int GRAPH_W    = GRAPH_N * 2;
static constexpr int GRAPH_H    = 116;
static constexpr int GRAPH_TOP_DBM = -10;  // グラフの上の端
static constexpr int GRAPH_BOT_DBM = -90;  // グラフの下の端
static constexpr int8_t NONE = 127;        // 「値なし」

static constexpr uint32_t COL_ORANGE = 0xFF8C00;
static constexpr uint32_t COL_YELLOW = 0xFFE040;
static constexpr uint32_t COL_CYAN   = 0x40D8FF;
static constexpr uint32_t COL_GRAY   = 0x707070;
static constexpr uint32_t COL_DARK   = 0x303030;

// ---- 合図の中身 ----
struct __attribute__((packed)) Packet {
  uint32_t magic;     // かめはめ波の合図だと見分ける印
  uint8_t  role;      // 1=親機 0=子機
  uint16_t seq;       // 通し番号（取りこぼしを数える）
  int8_t   seenAvg;   // 送り主が見ている受信レベル（平均）
  uint8_t  txIdx;     // 送り主の送信の強さ（子機は親機に合わせる）
};
static constexpr uint32_t MAGIC = 0x4B4D4830;  // "KMH0"
static const uint8_t BROADCAST[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// 無線の処理（別のタスク）から loop へ渡すもの
struct Sample {
  uint32_t ms;
  int8_t   rssi;
  Packet   pkt;
};
static QueueHandle_t sampleQueue;

// プロミスキャスで最後に見た ESP-NOW の電波
static volatile int8_t lastFrameRssi = 0;
static uint8_t         lastFrameMac[6];

// ---- 状態 ----
static M5Canvas canvas(&M5.Display);
static int      txIdx = 0;
static uint16_t sendSeq = 0;
static int8_t   avgBuf[AVG_N];
static int      avgCount = 0, avgHead = 0;
static int8_t   rawNow = NONE, avgNow = NONE, peerSeen = NONE;
static int8_t   strongest = NONE, weakest = NONE;
static int8_t   graphRaw[GRAPH_N], graphAvg[GRAPH_N];
static int      graphHead = 0;
static uint32_t lastRecvMs = 0;
static uint32_t lastGapPushMs = 0;
static int      recvThisSec = 0, recvPerSec = 0, lostThisSec = 0, lostPerSec = 0;
static uint32_t secStartMs = 0;
static bool     haveSeq = false;
static uint16_t lastSeq = 0;

// ---------------------------------------------------------------------------
// 無線

static void onPromiscuous(void* buf, wifi_promiscuous_pkt_type_t type) {
  if (type != WIFI_PKT_MGMT) return;
  const auto* p = (const wifi_promiscuous_pkt_t*)buf;
  const uint8_t* f = p->payload;
  if (p->rx_ctrl.sig_len < 28) return;
  // Action フレーム（0xD0）で、中身が ESP-NOW（カテゴリ127・Espressif の OUI 18:FE:34）のものだけ
  if (f[0] != 0xD0) return;
  if (f[24] != 127 || f[25] != 0x18 || f[26] != 0xFE || f[27] != 0x34) return;
  memcpy(lastFrameMac, f + 10, 6);  // 送り主
  lastFrameRssi = (int8_t)p->rx_ctrl.rssi;
}

static void onRecv(const uint8_t* mac, const uint8_t* data, int len) {
  if (len != sizeof(Packet)) return;
  Packet pkt;
  memcpy(&pkt, data, sizeof(pkt));
  if (pkt.magic != MAGIC) return;
  if (pkt.role == (IS_PARENT ? 1 : 0)) return;  // 同じ役の機械は無視
  if (memcmp(mac, lastFrameMac, 6) != 0) return;  // 受信レベルが別の電波のものなら捨てる
  Sample s{millis(), lastFrameRssi, pkt};
  xQueueSend(sampleQueue, &s, 0);
}

static void applyTxPower() {
  esp_wifi_set_max_tx_power(TX_LEVELS[txIdx]);
}

static void radioBegin() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  esp_wifi_set_ps(WIFI_PS_NONE);  // 省電力で受信が抜けないように

  wifi_promiscuous_filter_t filt{};
  filt.filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT;
  esp_wifi_set_promiscuous_filter(&filt);
  esp_wifi_set_promiscuous_rx_cb(onPromiscuous);
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_channel(WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE);

  if (esp_now_init() != ESP_OK) {
    Serial.println("[NOW] esp_now_init FAILED");
    return;
  }
  esp_now_register_recv_cb(onRecv);

  esp_now_peer_info_t peer{};
  memcpy(peer.peer_addr, BROADCAST, 6);
  peer.channel = 0;  // 今のチャンネル
  peer.ifidx   = WIFI_IF_STA;
  peer.encrypt = false;
  esp_now_add_peer(&peer);

  applyTxPower();
  Serial.printf("[NOW] ready  role=%s  mac=%s  ch=%d\n",
                IS_PARENT ? "PARENT" : "CHILD", WiFi.macAddress().c_str(), WIFI_CHANNEL);
}

static void sendPacket() {
  Packet pkt{MAGIC, (uint8_t)(IS_PARENT ? 1 : 0), sendSeq++, avgNow, (uint8_t)txIdx};
  esp_now_send(BROADCAST, (const uint8_t*)&pkt, sizeof(pkt));
}

// ---------------------------------------------------------------------------
// 値の処理

static void pushGraph(int8_t raw, int8_t avg) {
  graphRaw[graphHead] = raw;
  graphAvg[graphHead] = avg;
  graphHead = (graphHead + 1) % GRAPH_N;
}

static void resetStats() {
  strongest = weakest = NONE;
  for (int i = 0; i < GRAPH_N; i++) graphRaw[i] = graphAvg[i] = NONE;
  graphHead = 0;
}

static void handleSample(const Sample& s) {
  rawNow = s.rssi;

  avgBuf[avgHead] = s.rssi;
  avgHead = (avgHead + 1) % AVG_N;
  if (avgCount < AVG_N) avgCount++;
  int sum = 0;
  for (int i = 0; i < avgCount; i++) sum += avgBuf[i];
  avgNow = (int8_t)lroundf((float)sum / avgCount);

  if (strongest == NONE || s.rssi > strongest) strongest = s.rssi;
  if (weakest == NONE || s.rssi < weakest) weakest = s.rssi;
  peerSeen = s.pkt.seenAvg;

  // 取りこぼし
  if (haveSeq) {
    uint16_t gap = (uint16_t)(s.pkt.seq - lastSeq);
    if (gap > 1 && gap < 1000) lostThisSec += gap - 1;
  }
  lastSeq = s.pkt.seq;
  haveSeq = true;
  recvThisSec++;
  lastRecvMs = s.ms;

  // 子機は親機の送信の強さに合わせる
  if (!IS_PARENT && s.pkt.txIdx < TX_LEVEL_N && s.pkt.txIdx != txIdx) {
    txIdx = s.pkt.txIdx;
    applyTxPower();
    resetStats();
    Serial.printf("[TX] follow parent: %.1f dBm\n", TX_LEVELS[txIdx] * 0.25f);
  }

  pushGraph(s.rssi, avgNow);
  Serial.printf("%lu,%u,%d,%d,%d,%.1f\n", (unsigned long)s.ms, s.pkt.seq, s.rssi, avgNow,
                peerSeen == NONE ? 0 : peerSeen, TX_LEVELS[txIdx] * 0.25f);
}

// ---------------------------------------------------------------------------
// 画面

static int dbmToY(int dbm) {
  if (dbm > GRAPH_TOP_DBM) dbm = GRAPH_TOP_DBM;
  if (dbm < GRAPH_BOT_DBM) dbm = GRAPH_BOT_DBM;
  return GRAPH_Y + (GRAPH_TOP_DBM - dbm) * (GRAPH_H - 1) / (GRAPH_TOP_DBM - GRAPH_BOT_DBM);
}

// 強いほどオレンジ、弱いほど白っぽく（目安の色。しきい値は実験で決める）
static uint32_t levelColor(int8_t dbm) {
  if (dbm == NONE) return COL_GRAY;
  if (dbm >= -35) return COL_ORANGE;
  if (dbm >= -55) return COL_YELLOW;
  return TFT_WHITE;
}

static void drawGraph(bool linked) {
  // 目盛り（10dB ごと）
  canvas.setFont(&fonts::Font0);
  canvas.setTextDatum(middle_right);
  for (int dbm = GRAPH_TOP_DBM; dbm >= GRAPH_BOT_DBM; dbm -= 10) {
    const int y = dbmToY(dbm);
    canvas.drawFastHLine(GRAPH_X, y, GRAPH_W, COL_DARK);
    if (dbm % 20 == 0) {
      canvas.setTextColor(COL_GRAY);
      canvas.drawNumber(dbm, GRAPH_X - 3, y);
    }
  }
  canvas.drawRect(GRAPH_X - 1, GRAPH_Y - 1, GRAPH_W + 2, GRAPH_H + 2, COL_GRAY);

  // 古い順に左から
  int prevYRaw = -1, prevYAvg = -1;
  for (int i = 0; i < GRAPH_N; i++) {
    const int idx = (graphHead + i) % GRAPH_N;
    const int x = GRAPH_X + i * 2;
    if (graphRaw[idx] == NONE) { prevYRaw = prevYAvg = -1; continue; }
    const int yr = dbmToY(graphRaw[idx]);
    const int ya = dbmToY(graphAvg[idx]);
    if (prevYRaw >= 0) canvas.drawLine(x - 2, prevYRaw, x, yr, COL_GRAY);
    if (prevYAvg >= 0) {
      canvas.drawLine(x - 2, prevYAvg, x, ya, COL_YELLOW);
      canvas.drawLine(x - 2, prevYAvg + 1, x, ya + 1, COL_YELLOW);
    }
    prevYRaw = yr;
    prevYAvg = ya;
  }
  (void)linked;
}

static void draw() {
  const uint32_t now = millis();
  const bool linked = lastRecvMs != 0 && now - lastRecvMs < LINK_LOST_MS;
  const int cx = M5.Display.width() / 2 + 1;  // 画面の中心の補正（実機で測った値）

  canvas.fillScreen(TFT_BLACK);

  // 外周の輪: 平均の受信レベルの色
  const uint32_t ring = linked ? levelColor(avgNow) : COL_DARK;
  canvas.fillArc(cx, 234, 226, 218, 0, 360, ring);

  // 役
  canvas.setFont(&fonts::lgfxJapanGothic_28);
  canvas.setTextDatum(middle_center);
  canvas.setTextColor(IS_PARENT ? COL_ORANGE : COL_CYAN);
  canvas.drawString(IS_PARENT ? "親機（左手）" : "子機（右手）", cx, 52);

  // 送信の強さ・受信の回数
  canvas.setFont(&fonts::lgfxJapanGothic_20);
  canvas.setTextColor(TFT_WHITE);
  char line[64];
  snprintf(line, sizeof(line), "送信 %.1fdBm  受信 %d回/秒", TX_LEVELS[txIdx] * 0.25f, recvPerSec);
  canvas.drawString(line, cx, 88);
  if (lostPerSec > 0) {
    snprintf(line, sizeof(line), "取りこぼし %d回/秒", lostPerSec);
    canvas.setTextColor(COL_GRAY);
    canvas.drawString(line, cx, 112);
  }

  // 大きな数字（平均）
  if (linked) {
    canvas.setFont(&fonts::FreeSansBold24pt7b);
    canvas.setTextSize(2);
    canvas.setTextColor(levelColor(avgNow));
    canvas.setTextDatum(middle_right);
    canvas.drawNumber(avgNow, cx + 60, 168);
    canvas.setTextSize(1);
    canvas.setFont(&fonts::lgfxJapanGothic_24);
    canvas.setTextDatum(middle_left);
    canvas.drawString("dBm", cx + 68, 186);
  } else {
    canvas.setFont(&fonts::lgfxJapanGothic_40);
    canvas.setTextDatum(middle_center);
    canvas.setTextColor(COL_GRAY);
    canvas.drawString("未受信", cx, 168);
  }

  // 今・最強・最弱
  canvas.setFont(&fonts::lgfxJapanGothic_20);
  canvas.setTextDatum(middle_center);
  canvas.setTextColor(TFT_WHITE);
  if (strongest != NONE) {
    snprintf(line, sizeof(line), "今 %d   最強 %d   最弱 %d", linked ? rawNow : 0, strongest, weakest);
  } else {
    snprintf(line, sizeof(line), "今 --   最強 --   最弱 --");
  }
  canvas.drawString(line, cx, 236);

  drawGraph(linked);

  // 相手が見た値
  canvas.setFont(&fonts::lgfxJapanGothic_24);
  canvas.setTextDatum(middle_center);
  canvas.setTextColor(COL_GRAY);
  if (linked && peerSeen != NONE) {
    snprintf(line, sizeof(line), "相手が見た値 %d", peerSeen);
  } else {
    snprintf(line, sizeof(line), "相手が見た値 --");
  }
  canvas.drawString(line, cx, 404);

  canvas.pushSprite(0, 0);
}

// ---------------------------------------------------------------------------

void setup() {
  auto cfg = M5.config();
  cfg.serial_baudrate = 115200;
  M5.begin(cfg);
  M5.Display.fillScreen(TFT_BLACK);

  if (POWER_LED_OFF && M5.Power.getType() == m5::Power_Class::pmic_m5pm1) {
    M5.Power.M5pm1.setLedEnLevel(false);
  }

  canvas.setPsram(true);
  canvas.setColorDepth(16);
  if (!canvas.createSprite(M5.Display.width(), M5.Display.height())) {
    Serial.println("[DISP] createSprite FAILED");
  }

  resetStats();
  sampleQueue = xQueueCreate(64, sizeof(Sample));
  radioBegin();

  Serial.println("ms,seq,raw,avg,peer,tx_dbm");
}

void loop() {
  M5.update();
  const uint32_t now = millis();

  // 受け取った値をまとめて処理
  Sample s;
  while (xQueueReceive(sampleQueue, &s, 0) == pdTRUE) handleSample(s);

  // 届かない間はグラフに隙間を空ける
  if (lastRecvMs != 0 && now - lastRecvMs >= LINK_LOST_MS && now - lastGapPushMs >= SEND_PERIOD_MS) {
    pushGraph(NONE, NONE);
    lastGapPushMs = now;
  }

  // 送る
  static uint32_t lastSendMs = 0;
  if (now - lastSendMs >= SEND_PERIOD_MS) {
    lastSendMs = now;
    sendPacket();
  }

  // 1秒ごとの回数
  if (now - secStartMs >= 1000) {
    recvPerSec = recvThisSec;
    lostPerSec = lostThisSec;
    recvThisSec = lostThisSec = 0;
    secStartMs = now;
  }

  // ボタン
  if (M5.BtnA.wasClicked()) {
    resetStats();
    Serial.println("[BTN] reset");
  }
  if (IS_PARENT && M5.BtnB.wasClicked()) {
    txIdx = (txIdx + 1) % TX_LEVEL_N;
    applyTxPower();
    resetStats();
    Serial.printf("[TX] %.1f dBm\n", TX_LEVELS[txIdx] * 0.25f);
  }

  static uint32_t lastDrawMs = 0;
  if (now - lastDrawMs >= DRAW_PERIOD_MS) {
    lastDrawMs = now;
    draw();
  }

  delay(1);
}
