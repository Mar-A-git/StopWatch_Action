// 自分のモード（メニューの右上の「空き」）
//
// ここを書き換えると、メニューの4つ目に自分のモードを追加できる。
//   1. label()    … メニューに出す名前
//   2. drawIcon() … メニューに出すアイコン（丸の中に描く）
//   3. begin()    … モードを選んで起動した時に1回だけ呼ばれる
//   4. loop()     … そのあと、ずっとくり返し呼ばれる
//
// 決まりごと
//   ・M5.begin() と画面の初期化は main.cpp で済んでいる。begin() では自分のモードの準備だけ
//   ・loop() の中で M5.update() を呼ぶ（ボタンとタッチの状態が更新される）
//   ・A と B を同時に 1.5秒押すと、どのモードからでもメニューへ戻る（main.cpp が見ている）。
//     A と B の同時押しは、自分のモードの操作には使わない
//   ・声や画像のファイルは data/custom/ に置き、LittleFS.begin() してから "/custom/..." で読む
#pragma once

#include <M5Unified.h>

namespace custom {

const char* label();
void drawIcon(M5Canvas& g, int x, int y, int r);
void begin();
void loop();

}  // namespace custom
