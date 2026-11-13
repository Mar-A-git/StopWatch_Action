// stackchan_watch
//
// 起動時のメニューで、モードを選ぶ:
//   ・スタックチャン時計（src/stackchan）
//   ・かめはめ波 左手＝親機 / 右手＝子機（src/kame）
//   ・自分のモード（src/custom。最初は空き）
// 前回選んだモードを覚えていて、ふだんはそのまま起動する。
//
// モードの切り替え:
//   ・A と B を同時に 1.5秒押す → 再起動してメニュー（電源を切らないので、時計の時刻が消えない）
//   ・A か B を押しながら電源を入れる → メニュー（電源を切ると時刻が消えるので、Wi-Fi がない所では注意）
//
// モードは起動時に1つだけ動かす（Wi-Fi の時刻合わせと ESP-NOW がぶつからないように）。

#include <M5Unified.h>
#include <driver/gpio.h>

#include "custom/custom.h"
#include "kame/kame.h"
#include "menu.h"
#include "stackchan/stackchan.h"

static constexpr bool POWER_LED_OFF = true;  // 本体の緑の LED を消す

// A＋B 同時押しでメニューへ。各モードの長押し（2秒）より先に反応させる
static constexpr uint32_t MENU_COMBO_MS = 1500;
static constexpr int PIN_BTN_A = 2;  // 押すと LOW
static constexpr int PIN_BTN_B = 1;

static Mode mode = Mode::None;
static uint32_t bothSince = 0;

static void checkMenuCombo() {
  const bool both = gpio_get_level((gpio_num_t)PIN_BTN_A) == 0 && gpio_get_level((gpio_num_t)PIN_BTN_B) == 0;
  if (!both) {
    bothSince = 0;
    return;
  }
  const uint32_t now = millis();
  if (bothSince == 0) bothSince = now;
  if (now - bothSince >= MENU_COMBO_MS) menu::requestMenuAndRestart();
}

void setup() {
  auto cfg = M5.config();
  cfg.serial_baudrate = 115200;
  M5.begin(cfg);
  M5.Display.fillScreen(TFT_BLACK);
  if (POWER_LED_OFF && M5.Power.getType() == m5::Power_Class::pmic_m5pm1) {
    M5.Power.M5pm1.setLedEnLevel(false);
  }

  mode = menu::chooseAtBoot();
  switch (mode) {
    case Mode::StackChan:  stackchan::begin(); break;
    case Mode::KameParent: kame::begin(true); break;
    case Mode::KameChild:  kame::begin(false); break;
    case Mode::Custom:     custom::begin(); break;
    default: break;
  }
}

void loop() {
  checkMenuCombo();
  switch (mode) {
    case Mode::StackChan:  stackchan::loop(); break;
    case Mode::KameParent:
    case Mode::KameChild:  kame::loop(); break;
    case Mode::Custom:     custom::loop(); break;
    default: delay(100); break;
  }
}
