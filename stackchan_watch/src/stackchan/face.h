// スタックチャンの顔を、回転させて描く
//
// 顔の形（目・口の位置と、表情ごとの目の形）は M5Stack-Avatar
// （https://github.com/stack-chan/m5stack-avatar  Copyright (c) 2018 Shinya Ishikawa, MIT License）
// の標準の顔をもとにしている。ライセンスの全文はリポジトリの LICENSE の Third-party notice。
// ライブラリは 320×240 の画像を作ってから拡大・回転するので、丸い大きな画面だと
// ギザギザになる。そこで、目・口などを「円・四角・三角・線」として、
// 回転させた座標で直接描く（どの角度でもなめらか）。
#pragma once

#include <M5Unified.h>
#include <stdint.h>

namespace stackchan {

// A短押しで切り替える表情（ふつう → 笑顔 → 怒り → 悲しい）
enum class Expression : uint8_t { Neutral, Happy, Angry, Sad };
constexpr int EXPRESSION_COUNT = 4;

struct FaceState {
  float      angleDeg = 0;       // 回転（時計回り、12時が 0）
  Expression exp = Expression::Neutral;
  bool       eyesOpen = true;    // まばたき
  float      mouthOpen = 0;      // 0〜1（口パク）
  bool       sleeping = false;   // ミュート中は寝ている（目を閉じて Zzz）
  uint32_t   zzzMs = 0;          // Zzz のアニメーションの時刻
};

// 顔を描く（cx, cy は画面上の顔の中心）
void drawFace(M5Canvas& g, int cx, int cy, const FaceState& s);

}  // namespace stackchan
