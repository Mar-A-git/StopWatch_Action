#include "stackchan.h"

#include <M5Unified.h>
#include <Preferences.h>
#include <driver/gpio.h>
#include <esp_sleep.h>

#include "../common/net_time.h"
#include "config.h"
#include "face.h"
#include "voice.h"

namespace stackchan {
namespace {

M5Canvas spr(&M5.Display);
int W = 466, H = 466, CX = 234, CY = 234;

FaceState face;
FaceState drawn;           // 最後に描いた状態（変わった時だけ描き直す）
bool      firstDraw = true;

// まばたき
uint32_t nextBlinkMs = 0;
uint32_t blinkEndMs = 0;

// 時刻・回転
bool     demo = false;
uint32_t demoStartMs = 0;
float    demoStartDeg = 0;

// 電池
int      batLevel = -1;
bool     batCharging = false;
uint32_t batReadMs = 0;
int      drawnBat = -2;
bool     drawnCharging = false;

// 消灯
uint32_t lastActiveMs = 0;
bool     screenOn = true;

// ミュート
bool        muted = false;
Preferences prefs;

// 喋っている間
bool     speaking = false;
uint32_t speakStartMs = 0;
uint32_t speakLenMs = 0;
bool     speakHasVoice = false;

// 消灯中にボタンAで起こした時は、離すまでAを無視する（表情が変わらないように）
bool ignoreAUntilRelease = false;

// ---- 消灯・スリープ ----
// ・USB 給電中: 画面だけ消す。CPU は起きたまま（loop の中で、触る・押すと点く）
// ・電池: 画面を消して CPU も眠らせる（ライトスリープ）。ボタンかタッチで起きて、続きから動く
constexpr int PIN_BTN_A = 2;      // 押すと LOW
constexpr int PIN_BTN_B = 1;
constexpr int PIN_TOUCH_INT = 13; // 触ると LOW
constexpr int USB_VBUS_MV = 4000;

bool pinLow(int pin) { return gpio_get_level((gpio_num_t)pin) == 0; }

void readBattery(uint32_t now);  // 下で定義
void wake(uint32_t now);

void sleepUntilWake() {
  const bool usb = M5.Power.getVBUSVoltage() >= USB_VBUS_MV;
  Serial.printf("[SLEEP] screen off (%s)\n", usb ? "USB: display only" : "battery: light sleep");
  Serial.flush();
  M5.Display.sleep();
  screenOn = false;
  if (usb) return;  // 起こすのは loop の中で

  // 電池: 起きた時の「プツッ」対策で、眠る前にスピーカーを止める
  M5.Speaker.end();
  const int pins[] = {PIN_BTN_A, PIN_BTN_B, PIN_TOUCH_INT};
  for (;;) {
    // どれかが LOW のまま眠ると、すぐ起きてしまうので、離れるまで少し待つ
    for (int i = 0; i < 100 && (pinLow(PIN_BTN_A) || pinLow(PIN_BTN_B) || pinLow(PIN_TOUCH_INT)); i++) delay(10);
    for (int p : pins) gpio_wakeup_enable((gpio_num_t)p, GPIO_INTR_LOW_LEVEL);
    esp_sleep_enable_gpio_wakeup();
    esp_light_sleep_start();
    for (int p : pins) gpio_wakeup_disable((gpio_num_t)p);
    if (pinLow(PIN_BTN_A) || pinLow(PIN_BTN_B) || pinLow(PIN_TOUCH_INT)) break;
    // ノイズで起きただけなら、もう一度眠る
  }
  M5.Speaker.begin();
  M5.Speaker.setVolume(SPEAKER_VOLUME);
  ignoreAUntilRelease = pinLow(PIN_BTN_A);  // Aは起こすだけ。Bは離した時にそのまま喋る
  readBattery(millis());                    // 眠っている間に変わったかもしれないので読み直す
  wake(millis());
}

void startSpeaking(uint32_t now) {
  m5::rtc_datetime_t dt;
  const bool timeOk = M5.Rtc.getDateTime(&dt) && dt.date.year >= 2025;
  speakHasVoice = timeOk && voice::prepareTime(dt.time.hours, dt.time.minutes);
  speakLenMs = speakHasVoice ? voice::durationMs() : NO_VOICE_TALK_MS;
  if (speakHasVoice && !muted) voice::play();
  speaking = true;
  speakStartMs = now;
  Serial.printf("[SPEAK] %s%s\n", speakHasVoice ? "voice" : "no voice (mouth only)", muted ? " (muted)" : "");
}

// 声がない時の口パク（ゆっくり開け閉め）
float fakeLevel(uint32_t t) { return 0.5f + 0.5f * sinf(t * 2 * (float)M_PI / 530.0f); }

uint32_t randBetween(uint32_t lo, uint32_t hi) { return lo + esp_random() % (hi - lo + 1); }

// 今の時刻から、短針の角度（12時が 0、1分で 0.5度）
float clockAngle() {
  m5::rtc_datetime_t dt;
  if (!M5.Rtc.getDateTime(&dt) || dt.date.year < 2025) return 0;  // 時刻がわからない時はまっすぐ
  return ((dt.time.hours % 12) * 60 + dt.time.minutes) * 0.5f;
}

void readBattery(uint32_t now) {
  batLevel = M5.Power.getBatteryLevel();
  batCharging = M5.Power.isCharging() == m5::Power_Class::is_charging;
  batReadMs = now;
  Serial.printf("[BAT] %d%% %s\n", batLevel, batCharging ? "(charging)" : "");
}

void drawBattery(int cx, int cy, int level, bool charging) {
  if (level < 0) return;
  if (level > 100) level = 100;
  const uint32_t col = level <= 20 ? BAT_RED : (level <= 50 ? BAT_YELLOW : BAT_GREEN);
  const uint16_t c = spr.color565((col >> 16) & 0xFF, (col >> 8) & 0xFF, col & 0xFF);
  char text[8];
  snprintf(text, sizeof(text), "%d%%", level);
  spr.setFont(&fonts::lgfxJapanGothic_20);
  const int gap = 8, iconW = BAT_ICON_W + 4;
  const int total = iconW + gap + spr.textWidth(text);
  const int x0 = cx - total / 2, y0 = cy - BAT_ICON_H / 2;
  spr.drawRoundRect(x0, y0, BAT_ICON_W, BAT_ICON_H, 3, TFT_WHITE);
  spr.drawRoundRect(x0 + 1, y0 + 1, BAT_ICON_W - 2, BAT_ICON_H - 2, 2, TFT_WHITE);
  spr.fillRect(x0 + BAT_ICON_W, y0 + BAT_ICON_H / 2 - 3, 3, 6, TFT_WHITE);
  const int w = (BAT_ICON_W - 6) * level / 100;
  if (w > 0) spr.fillRect(x0 + 3, y0 + 3, w, BAT_ICON_H - 6, c);
  if (charging) {
    const int bx = x0 + BAT_ICON_W / 2;
    spr.fillTriangle(bx + 2, cy - 9, bx - 5, cy + 1, bx + 1, cy + 1, TFT_WHITE);
    spr.fillTriangle(bx - 2, cy + 9, bx + 5, cy - 1, bx - 1, cy - 1, TFT_WHITE);
  }
  spr.setTextDatum(middle_left);
  spr.setTextColor(c);
  spr.drawString(text, x0 + iconW + gap, cy);
}

bool sameFace(const FaceState& a, const FaceState& b) {
  return fabsf(a.angleDeg - b.angleDeg) < 0.01f && a.exp == b.exp && a.eyesOpen == b.eyesOpen &&
         fabsf(a.mouthOpen - b.mouthOpen) < 0.01f && a.sleeping == b.sleeping && a.zzzMs == b.zzzMs;
}

void render() {
  spr.fillScreen(TFT_BLACK);
  drawFace(spr, CX, CY + FACE_OFFSET_Y, face);
  drawBattery(CX, BAT_Y, batLevel, batCharging);
  spr.pushSprite(0, 0);
  drawn = face;
  drawnBat = batLevel;
  drawnCharging = batCharging;
  firstDraw = false;
}

const char* expName(Expression e) {
  switch (e) {
    case Expression::Neutral: return "neutral";
    case Expression::Happy:   return "happy";
    case Expression::Angry:   return "angry";
    case Expression::Sad:     return "sad";
  }
  return "?";
}

void wake(uint32_t now) {
  lastActiveMs = now;
  if (!screenOn) {
    M5.Display.wakeup();
    screenOn = true;
    firstDraw = true;
    netTimeOnWake();
    Serial.println("[SLEEP] screen on");
  }
}

}  // namespace

void begin() {
  W = M5.Display.width();
  H = M5.Display.height();
  CX = W / 2 + 1;  // 画面の中心の補正（実機で測った値）
  CY = H / 2 + 1;
  spr.setPsram(true);
  spr.setColorDepth(16);
  if (!spr.createSprite(W, H)) Serial.println("[SC] createSprite FAILED");

  prefs.begin("stackchan", false);
  muted = prefs.getBool("mute", false);
  face.sleeping = muted;

  M5.BtnA.setHoldThresh(MUTE_HOLD_MS);
  M5.BtnB.setHoldThresh(1000);

  const uint32_t now = millis();
  nextBlinkMs = now + randBetween(BLINK_INTERVAL_MIN, BLINK_INTERVAL_MAX);
  lastActiveMs = now;
  readBattery(now);
  voice::begin();
  netTimeBegin(now);
  Serial.printf("[SC] start  mute=%d  angle=%.1f\n", muted, clockAngle());
}

void loop() {
  M5.update();
  const uint32_t now = millis();

  netTimeUpdate(now);

  const bool touched = M5.Touch.getCount() > 0;
  const bool anyButton = M5.BtnA.isPressed() || M5.BtnB.isPressed();
  if (touched || anyButton || speaking) {
    // 消えていた時にAで起こしたら、そのAは表情を変えない（Bは、離した時にそのまま喋る）
    if (!screenOn && M5.BtnA.isPressed()) ignoreAUntilRelease = true;
    wake(now);
  }
  if (ignoreAUntilRelease) {
    if (!M5.BtnA.isPressed()) ignoreAUntilRelease = false;
  }

  // ボタン
  if (ignoreAUntilRelease) {
    // 何もしない
  } else if (M5.BtnA.wasHold()) {
    // ミュート ＝ 寝ている（目を閉じて Zzz）。喋っている途中なら止める
    muted = !muted;
    face.sleeping = muted;
    prefs.putBool("mute", muted);
    if (muted && speaking) {
      voice::stop();
      speaking = false;
      face.mouthOpen = 0;
    }
    M5.Power.setVibration(200);
    delay(MUTE_VIB_MS);
    M5.Power.setVibration(0);
    Serial.printf("[MUTE] %s (sleeping)\n", muted ? "ON" : "OFF");
  } else if (M5.BtnA.wasClicked() && !muted) {
    face.exp = (Expression)(((int)face.exp + 1) % EXPRESSION_COUNT);
    Serial.printf("[EXP] %s\n", expName(face.exp));
  }
  if (M5.BtnB.wasHold()) {
    demo = !demo;
    demoStartMs = now;
    demoStartDeg = clockAngle();
    Serial.printf("[DEMO] %s\n", demo ? "ON (fast)" : "OFF");
  } else if (M5.BtnB.wasClicked() && !speaking) {
    if (muted) Serial.println("[SPEAK] sleeping (muted) -> no voice");
    else startSpeaking(now);
  }

  // 寝ている時の Zzz（ZZZ_FRAME_MS ごとに描き直す）
  face.zzzMs = muted ? (now / ZZZ_FRAME_MS) * ZZZ_FRAME_MS : 0;

  // 口パク
  if (speaking) {
    const uint32_t t = now - speakStartMs;
    if (t >= speakLenMs) {
      speaking = false;
      face.mouthOpen = 0;
      lastActiveMs = now;
    } else {
      const float lv = speakHasVoice ? voice::levelAt(t) : fakeLevel(t);
      float open = lv * MOUTH_GAIN;
      face.mouthOpen = open > 1 ? 1 : open;
    }
  }

  // 回転
  if (demo) {
    const float deg = demoStartDeg + (now - demoStartMs) / 60000.0f * 0.5f * DEMO_SPEED;
    face.angleDeg = fmodf(deg, 360.0f);
  } else {
    face.angleDeg = clockAngle();
  }

  // まばたき（居眠りの時も、時々まばたきする）
  if (now >= nextBlinkMs) {
    blinkEndMs = now + BLINK_MS;
    nextBlinkMs = now + randBetween(BLINK_INTERVAL_MIN, BLINK_INTERVAL_MAX);
  }
  face.eyesOpen = now >= blinkEndMs;

  // 電池
  if (now - batReadMs >= BAT_READ_MS) readBattery(now);

  // 消灯（電池の時はライトスリープ。USB 給電中は画面だけ消す）
  if (SCREEN_OFF_ENABLED && screenOn && !speaking && !netTimeBusy() && now - lastActiveMs >= SCREEN_OFF_MS) {
    sleepUntilWake();
  }

  if (screenOn && (firstDraw || !sameFace(face, drawn) || batLevel != drawnBat ||
                   batCharging != drawnCharging)) {
    render();
  }
  delay(5);
}

}  // namespace stackchan
