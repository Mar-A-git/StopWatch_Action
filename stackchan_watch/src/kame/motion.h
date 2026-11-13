// 「はっ！」で手を前に突き出す動きを見つける（IMU の加速度）
//
// 描画に時間がかかる（1フレーム約55ms）ので、1フレームに1回読むだけだと
// 一瞬の加速を見逃す。そこで描画の途中からも sample() を呼んで、一番大きい値を覚えておく。
#pragma once

#include <stdint.h>

namespace motion {

void begin();
void sample();               // 加速度を読んで、一番大きい値を覚える（描画の途中からも呼ぶ）
bool poll(uint32_t now);     // 前に呼んでから「突き出し」があったら true
float lastPeakG();           // 直近の一番大きい加速度（ログ用）

}  // namespace motion
