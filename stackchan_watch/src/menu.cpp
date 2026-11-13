#include "menu.h"

#include <M5Unified.h>
#include <Preferences.h>

#include "custom/custom.h"

namespace menu {
namespace {

constexpr int ITEM_N = 4;
// 右上は「自分のモード」の空き。アイコンと名前は src/custom/custom.cpp で決める
const Mode ITEMS[ITEM_N] = {Mode::StackChan, Mode::Custom, Mode::KameParent, Mode::KameChild};
const char* LABELS[ITEM_N] = {"スタックチャン", nullptr, "かめはめ波・左", "かめはめ波・右"};

// 2×2 に並べたアイコンの中心（466×466 の丸い画面）。約10.5ドットで1mm
// タイトル以外は、OFFSET_Y だけ上へずらす（下の案内が見切れていたので約1mm）
constexpr int OFFSET_Y = -10;
// 上の段の名前が、下の段のアイコンに近すぎた → アイコンを小さく（58→50）、名前をアイコンに近づけ、
// 段の間を広げた（上の段の名前と下の段のアイコンの間が約3.5mm）
constexpr int ICON_R = 50;
const int IX[ITEM_N] = {150, 316, 150, 316};
const int IY[ITEM_N] = {170 + OFFSET_Y, 170 + OFFSET_Y, 326 + OFFSET_Y, 326 + OFFSET_Y};
constexpr int LABEL_DY = ICON_R + 18;

M5Canvas spr(&M5.Display);
int CX = 234;

void drawStackChan(int x, int y) {
  // 黒い顔に白い目と口（スタックチャン）
  spr.fillCircle(x, y, ICON_R - 6, spr.color565(0x20, 0x20, 0x24));
  spr.drawCircle(x, y, ICON_R - 6, spr.color565(0x70, 0x70, 0x70));
  spr.fillCircle(x - 18, y - 6, 4, TFT_WHITE);
  spr.fillCircle(x + 18, y - 6, 4, TFT_WHITE);
  spr.fillRect(x - 12, y + 12, 24, 3, TFT_WHITE);
}

void drawKame(int x, int y, const char* side) {
  // オレンジ → 黄 → 白 の光の玉
  const int r = ICON_R - 6;
  for (int i = r; i > 0; i -= 2) {
    const float t = (float)i / r;  // 1 が外側
    uint8_t R = 255, G, B;
    if (t > 0.5f) {  // 外側: オレンジ → 黄
      G = (uint8_t)(0xE0 - (0xE0 - 0x8C) * (t - 0.5f) * 2);
      B = (uint8_t)(0x40 * (1 - (t - 0.5f) * 2));
    } else {  // 内側: 黄 → 白
      G = (uint8_t)(0xE0 + (0xFF - 0xE0) * (1 - t * 2));
      B = (uint8_t)(0x40 + (0xFF - 0x40) * (1 - t * 2));
    }
    spr.fillCircle(x, y, i, spr.color565(R, G, B));
  }
  spr.setFont(&fonts::lgfxJapanGothic_36);
  spr.setTextDatum(middle_center);
  spr.setTextColor(spr.color565(0x60, 0x30, 0x00));
  spr.drawString(side, x, y + 2);
}

void draw(int selected) {
  spr.fillScreen(TFT_BLACK);
  spr.setTextDatum(middle_center);
  spr.setFont(&fonts::lgfxJapanGothic_28);
  spr.setTextColor(TFT_WHITE);
  spr.drawString("モードをえらぶ", CX, 70);

  for (int i = 0; i < ITEM_N; i++) {
    switch (ITEMS[i]) {
      case Mode::Custom:     custom::drawIcon(spr, IX[i], IY[i], ICON_R - 6); break;
      case Mode::StackChan:  drawStackChan(IX[i], IY[i]); break;
      case Mode::KameParent: drawKame(IX[i], IY[i], "左"); break;
      case Mode::KameChild:  drawKame(IX[i], IY[i], "右"); break;
      default: break;
    }
    if (i == selected) {
      for (int k = 0; k < 4; k++) spr.drawCircle(IX[i], IY[i], ICON_R + k, spr.color565(0x40, 0xD8, 0xFF));
    }
    spr.setFont(&fonts::lgfxJapanGothic_20);
    spr.setTextColor(i == selected ? spr.color565(0x40, 0xD8, 0xFF) : TFT_WHITE);
    spr.drawString(ITEMS[i] == Mode::Custom ? custom::label() : LABELS[i], IX[i], IY[i] + LABEL_DY);
  }

  spr.setFont(&fonts::lgfxJapanGothic_16);
  spr.setTextColor(spr.color565(0x90, 0x90, 0x90));
  spr.drawString("タッチで選ぶ（A:次へ B:決定）", CX, 428 + OFFSET_Y);
  spr.pushSprite(0, 0);
}

int hitTest(int x, int y) {
  for (int i = 0; i < ITEM_N; i++) {
    const int dx = x - IX[i], dy = y - IY[i];
    if (dx * dx + dy * dy <= (ICON_R + 16) * (ICON_R + 16)) return i;
  }
  return -1;
}

Mode runMenu(Mode current) {
  CX = M5.Display.width() / 2 + 1;
  spr.setPsram(true);
  spr.setColorDepth(16);
  spr.createSprite(M5.Display.width(), M5.Display.height());

  int sel = 0;
  for (int i = 0; i < ITEM_N; i++) if (ITEMS[i] == current) sel = i;
  draw(sel);

  // 電源を入れる時に押していたボタンを離すまで待つ（そのまま「決定」にならないように）
  while (M5.BtnA.isPressed() || M5.BtnB.isPressed()) {
    M5.update();
    delay(10);
  }

  for (;;) {
    M5.update();
    if (M5.BtnA.wasClicked()) {
      sel = (sel + 1) % ITEM_N;
      draw(sel);
    }
    if (M5.BtnB.wasClicked()) break;
    if (M5.Touch.getCount() > 0) {
      const auto d = M5.Touch.getDetail(0);
      if (d.wasClicked()) {
        const int hit = hitTest(d.x, d.y);
        if (hit >= 0) {
          sel = hit;
          draw(sel);
          delay(250);  // 選んだことが見えるように少しだけ待つ
          break;
        }
      }
    }
    delay(10);
  }

  spr.deleteSprite();
  M5.Display.fillScreen(TFT_BLACK);
  return ITEMS[sel];
}

}  // namespace

const char* modeName(Mode m) {
  switch (m) {
    case Mode::Custom:     return "custom";
    case Mode::StackChan:  return "stackchan";
    case Mode::KameParent: return "kamehameha (parent)";
    case Mode::KameChild:  return "kamehameha (child)";
    default:               return "none";
  }
}

Mode chooseAtBoot() {
  Preferences prefs;
  prefs.begin("master", false);
  Mode saved = (Mode)prefs.getUChar("mode", 0);
  const bool valid = saved == Mode::StackChan || saved == Mode::KameParent ||
                     saved == Mode::KameChild || saved == Mode::Custom;

  // 電源を入れた時にボタンを押していたか（少しの間、何度か見る）
  bool held = false;
  for (int i = 0; i < 10; i++) {
    M5.update();
    if (M5.BtnA.isPressed() || M5.BtnB.isPressed()) held = true;
    delay(20);
  }

  // A＋B 同時押しで再起動した時は、メニューを出す（requestMenuAndRestart が立てた印）
  const bool requested = prefs.getBool("menu_once", false);
  if (requested) prefs.putBool("menu_once", false);

  if (valid && !held && !requested) {
    prefs.end();
    Serial.printf("[MENU] start saved mode: %s\n", modeName(saved));
    return saved;
  }

  Serial.printf("[MENU] show menu (%s)\n",
                requested ? "A+B" : (held ? "button held at boot" : "no saved mode"));
  const Mode m = runMenu(valid ? saved : Mode::StackChan);
  prefs.putUChar("mode", (uint8_t)m);
  prefs.end();
  Serial.printf("[MENU] selected: %s\n", modeName(m));
  return m;
}

void requestMenuAndRestart() {
  Preferences prefs;
  prefs.begin("master", false);
  prefs.putBool("menu_once", true);
  prefs.end();

  // 再起動する前に、画面に一言出す（音と振動は止める）
  M5.Speaker.stop();
  M5.Power.setVibration(0);
  M5.Display.wakeup();
  M5.Display.setBrightness(128);
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setFont(&fonts::FreeSansBold24pt7b);
  M5.Display.setTextDatum(middle_center);
  M5.Display.setTextColor(TFT_WHITE);
  M5.Display.drawString("MENU", M5.Display.width() / 2 + 1, M5.Display.height() / 2);
  Serial.println("[MENU] A+B -> restart to menu (RTC keeps the time)");
  Serial.flush();
  delay(600);
  esp_restart();
}

}  // namespace menu
