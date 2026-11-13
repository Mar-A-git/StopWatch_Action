// かめはめ波
//
// 流れ（親機が決めて、子機は親機に合わせる）
//   待機   : 受信レベルを表示。手のひらを向かい合わせる（受信レベル RSSI_START 以上が START_HOLD_MS 続く）と ①へ
//   ①〜③  : 3秒ずつ自動で進む。途中で手が離れたら（RSSI_APART より弱い）リセット
//   ④      : 3秒＋撃つのを待つ5秒。2台が同時に（THRUST_PAIR_MS 以内に）突き出したら ⑤
//            手が離れても、FIRE_GRACE_MS のうちに2台の突き出しが来なければリセット
//   ⑤      : はっ！ 約2秒で待機へ
//   待機に戻ったら、一度手を離すまでは次を始めない
//
// ボタン（確認用。親機だけ）
//   A: 待機中なら ①を始める。途中で押すとリセット
//   B: ④の間なら ⑤

#include <M5Unified.h>

#include "config.h"
#include "effect.h"
#include "motion.h"
#include "radio.h"
#include "voice.h"
#include "kame.h"

static uint8_t  phase = PH_IDLE;
static uint32_t phaseStart = 0;
static uint8_t  vibNow = 0;
static uint8_t  baseBrightness = 128;
static uint8_t  brightNow = 0;

// 親機の判定
static bool     armed = true;       // 待機中に手を離したか（次を始めてよいか）
static uint32_t closeSince = 0;     // 向かい合わせが始まった時刻
static uint32_t apartSince = 0;     // 離れたのが始まった時刻
static uint32_t myThrustMs = 0;     // 自分が最後に突き出した時刻

static const char* phaseName(uint8_t p) {
  switch (p) {
    case PH_IDLE: return "IDLE";
    case PH_KA:   return "1 KA";
    case PH_ME1:  return "2 ME";
    case PH_HA:   return "3 HA";
    case PH_ME2:  return "4 ME";
    case PH_FIRE: return "5 HA!";
  }
  return "?";
}

static void setPhase(uint8_t p, uint32_t start, const char* why = "") {
  const bool changed = p != phase;
  if (changed) Serial.printf("[PHASE] %s  (rssi %d) %s\n", phaseName(p), radio::rssiAvg(), why);
  phase = p;
  phaseStart = start;
  radio::publish(phase, phaseStart);
  if (changed) {
    if (p == PH_IDLE || (!IS_PARENT && !VOICE_ON_CHILD)) voice::stop();
    else voice::play(p, millis() - start);  // 親機より遅れて始まった分を飛ばす
  }
  closeSince = apartSince = 0;
}

// 「離れた」が APART_HOLD_MS 続いたか
static bool apartFor(uint32_t now, uint32_t holdMs) {
  const bool apart = !radio::linked() || radio::rssiAvg() < RSSI_APART;
  if (!apart) {
    apartSince = 0;
    return false;
  }
  if (apartSince == 0) apartSince = now;
  return now - apartSince >= holdMs;
}

// 2台がほぼ同時に突き出したか（④が始まってからのものだけ）
static bool bothThrust() {
  const uint32_t peer = radio::peerThrustMs();
  if (myThrustMs == 0 || peer == 0) return false;
  if ((int32_t)(myThrustMs - phaseStart) < 0 || (int32_t)(peer - phaseStart) < 0) return false;
  return abs((int32_t)(myThrustMs - peer)) <= (int32_t)THRUST_PAIR_MS;
}

static void updateParent(uint32_t now, bool thrust) {
  const uint32_t t = now - phaseStart;
  if (thrust) myThrustMs = now;

  // 確認用のボタン
  if (M5.BtnA.wasClicked()) {
    if (phase == PH_IDLE) setPhase(PH_KA, now, "(button)");
    else setPhase(PH_IDLE, now, "(button)");
    armed = false;
    return;
  }
  if (M5.BtnB.wasClicked() && phase == PH_ME2) {
    setPhase(PH_FIRE, now, "(button)");
    return;
  }

  const int8_t rssi = radio::rssiAvg();
  const bool linked = radio::linked();

  switch (phase) {
    case PH_IDLE:
      if (!armed) {
        if (!linked || rssi < RSSI_APART) armed = true;
        break;
      }
      if (linked && rssi >= RSSI_START) {
        if (closeSince == 0) closeSince = now;
        if (now - closeSince >= START_HOLD_MS) {
          armed = false;
          setPhase(PH_KA, now, "(palms together)");
        }
      } else {
        closeSince = 0;
      }
      break;

    case PH_KA:
    case PH_ME1:
    case PH_HA:
      if (apartFor(now, APART_HOLD_MS)) {
        setPhase(PH_IDLE, now, "(apart -> reset)");
      } else if (t >= PHASE_MS) {
        setPhase(phase + 1, phaseStart + PHASE_MS);
      }
      break;

    case PH_ME2:
      if (bothThrust()) {
        setPhase(PH_FIRE, now, "(both thrust)");
      } else if (FIRE_ON_APART && apartFor(now, APART_HOLD_MS)) {
        setPhase(PH_FIRE, now, "(apart)");
      } else if (!FIRE_ON_APART && apartFor(now, FIRE_GRACE_MS)) {
        setPhase(PH_IDLE, now, "(apart without thrust -> reset)");
      } else if (t >= PHASE_MS + ME2_WAIT_MS) {
        setPhase(PH_IDLE, now, "(timeout)");
      }
      break;

    case PH_FIRE:
      if (t >= FIRE_MS) setPhase(PH_IDLE, now);
      break;
  }
}

static void updateChild(uint32_t now) {
  uint8_t p;
  uint32_t st;
  if (!radio::parentPhase(p, st)) {
    if (phase != PH_IDLE) setPhase(PH_IDLE, now, "(parent lost)");
    return;
  }
  if (p != phase) {
    setPhase(p, st);
  } else if (abs((int32_t)(st - phaseStart)) > 80) {
    // 少しずつずれてきたら合わせ直す（毎回合わせると、届く時間のばらつきで点滅がガタつく）
    phaseStart = st;
    radio::publish(phase, phaseStart);
  }
}

static uint8_t vibLevel(uint32_t t) {
  switch (phase) {
    case PH_KA:  return (t % VIB_PERIOD_KA) < VIB_ON_MS ? VIB_KA : 0;
    case PH_ME1: return (t % VIB_PERIOD_ME1) < VIB_ON_MS ? VIB_ME1 : 0;
    case PH_HA:  return (t % VIB_PERIOD_HA) < VIB_ON_MS ? VIB_HA : 0;
    case PH_ME2:
      if (t < PHASE_MS) return (t / ME2_BLINK_HALF_MS) % 2 == 0 ? VIB_ME2 : 0;  // 点滅と一緒に
      return (t % 500) < VIB_ON_MS ? VIB_ME2 : 0;                              // 撃つのを待つ間
    case PH_FIRE: return t < VIB_FIRE_MS ? VIB_FIRE : 0;
  }
  return 0;
}

bool IS_PARENT = true;

namespace kame {

void begin(bool parent) {
  IS_PARENT = parent;
  M5.Display.fillScreen(TFT_BLACK);

  baseBrightness = M5.Display.getBrightness();
  Serial.printf("[DISP] brightness %d\n", baseBrightness);

  effect::begin();
  effect::setSampleHook(motion::sample);
  motion::begin();
  voice::begin();
  radio::begin();
  setPhase(PH_IDLE, millis());
}

void loop() {
  M5.update();
  radio::update();
  const uint32_t now = millis();

  const bool thrust = motion::poll(now);
  if (thrust) radio::noteThrust(now);

  if (IS_PARENT) updateParent(now, thrust);
  else updateChild(now);

  const uint32_t t = now - phaseStart;
  const uint8_t v = vibLevel(t);
  if (v != vibNow) {
    M5.Power.setVibration(v);
    vibNow = v;
  }

  // ⑤ の間だけ画面をいちばん明るくする
  const uint8_t br = phase == PH_FIRE ? FIRE_BRIGHTNESS : baseBrightness;
  if (br != brightNow) {
    M5.Display.setBrightness(br);
    brightNow = br;
  }

  // 電池の残量（起動直後と、BAT_READ_MS ごとに読む）
  static int batLevel = -1;
  static bool batCharging = false;
  static uint32_t batReadMs = 0;
  if (batLevel < 0 || now - batReadMs >= BAT_READ_MS) {
    batLevel = M5.Power.getBatteryLevel();
    batCharging = M5.Power.isCharging() == m5::Power_Class::is_charging;
    batReadMs = now;
    Serial.printf("[BAT] %d%% %s\n", batLevel, batCharging ? "(charging)" : "");
  }

  if (phase == PH_IDLE)
    effect::drawIdle(radio::linked(), radio::rssiAvg(), radio::recvPerSec(), batLevel, batCharging);
  else effect::drawPhase(phase, t);

  // 1秒ごとのログ（描画の速さも見る）
  static uint32_t logMs = 0, frames = 0;
  frames++;
  if (now - logMs >= 1000) {
    Serial.printf("[STAT] %s  rssi %d  recv %d/s  %lu fps\n", phaseName(phase), radio::rssiAvg(),
                  radio::recvPerSec(), (unsigned long)frames);
    frames = 0;
    logMs = now;
  }
}

}  // namespace kame
