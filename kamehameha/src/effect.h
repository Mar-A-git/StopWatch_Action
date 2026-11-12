// 画面の光（同心円のグラデーション・点滅・光の粒）
//
// 描き方: 画面の中心からの距離（px）を前もって表にしておき、
// フレームごとに「距離 → 色」の対応表（パレット）だけを作って、全画面を塗る。
// 同心円をたくさん描くより速く、グラデーションもなめらかになる。
#pragma once

#include <stdint.h>

namespace effect {

void begin();

// 描画の途中で何回か呼ぶ関数（IMU を細かく読むため）
void setSampleHook(void (*fn)());

// 待機中: 役・受信レベル・案内
void drawIdle(bool linked, int8_t rssiAvg, int recvPerSec);

// ①〜⑤: t はそのフェーズが始まってからの時間（ms）
void drawPhase(uint8_t phase, uint32_t t);

}  // namespace effect
