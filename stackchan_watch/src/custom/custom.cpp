// 自分のモードのひな形（いまは「空き」）
//
// 何もしていない状態では、メニューに灰色の丸と「空き」が出て、選ぶと案内が出るだけ。
// 下の4つの関数を書き換えて、自分のモードにしてください（使い方は custom.h と README）。
//
// 例: ボタンAを押すたびに、画面の色が変わるモード
//   void begin() { M5.Display.fillScreen(TFT_BLACK); }
//   void loop() {
//     M5.update();
//     if (M5.BtnA.wasClicked()) M5.Display.fillScreen(random(0x10000));
//     delay(10);
//   }

#include "custom.h"

namespace custom {

// メニューに出す名前
const char* label() { return "空き"; }

// メニューに出すアイコン。(x, y) が中心、r が半径
void drawIcon(M5Canvas& g, int x, int y, int r) {
  g.fillCircle(x, y, r, g.color565(0x50, 0x50, 0x50));
  g.fillCircle(x, y, r - 4, g.color565(0x30, 0x30, 0x30));
  g.setFont(&fonts::lgfxJapanGothic_36);
  g.setTextDatum(middle_center);
  g.setTextColor(g.color565(0x80, 0x80, 0x80));
  g.drawString("+", x, y + 2);
}

// 選んで起動した時に1回だけ
void begin() {
  auto& d = M5.Display;
  const int cx = d.width() / 2 + 1;
  d.fillScreen(TFT_BLACK);
  d.setTextDatum(middle_center);
  d.setTextColor(TFT_WHITE);
  d.setFont(&fonts::lgfxJapanGothic_28);
  d.drawString("ここは空きです", cx, 150);
  d.setFont(&fonts::lgfxJapanGothic_20);
  d.setTextColor(d.color565(0xB0, 0xB0, 0xB0));
  d.drawString("src/custom/custom.cpp を", cx, 215);
  d.drawString("書き換えると、自分のモードを", cx, 245);
  d.drawString("ここに追加できます", cx, 275);
  d.setTextColor(d.color565(0x40, 0xD8, 0xFF));
  d.drawString("A＋B を1.5秒でメニューへ", cx, 340);
  Serial.println("[CUSTOM] empty slot");
}

// そのあと、ずっとくり返し
void loop() {
  M5.update();
  delay(20);
}

}  // namespace custom
