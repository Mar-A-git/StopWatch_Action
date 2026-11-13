// 起動メニュー
//
//   ・前回選んだモードを覚えていて、ふだんはそのまま起動する
//   ・ボタン（A か B）を押しながら電源を入れると、メニューが出る（まだ一度も選んでいない時も出る）
//   ・動いている時に A と B を同時に 1.5秒押すと、再起動してメニューが出る。
//     電源を切らないので、時計の時刻（RTC）が消えない。モードを切り替える時はこちらを使う
//   ・メニューはタッチで選ぶ。ボタンでも選べる（A: 次へ / B: 決定）
#pragma once

#include <stdint.h>

enum class Mode : uint8_t {
  None = 0,
  // 1 は使わない（予約）
  StackChan = 2,
  KameParent = 3,
  KameChild = 4,
  Custom = 5,   // 自分のモード（src/custom）
};

namespace menu {

// 起動時に呼ぶ。動かすモードを返す（メニューを出した時は、選ばれるまで戻らない）
Mode chooseAtBoot();

const char* modeName(Mode m);

// 次の起動でメニューを出すように印を付けて、再起動する（電源は切らないので、時計の時刻は消えない）
void requestMenuAndRestart();

}  // namespace menu
