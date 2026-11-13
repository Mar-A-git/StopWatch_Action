// スタックチャン時計の定数
#pragma once

#include <stdint.h>

namespace stackchan {

// ---- 顔（M5Stack-Avatar の標準の顔の座標。320×240 の中の位置）----
// 顔の回転の中心を、画面の中心からずらす量（画面のドット。約10.5ドットで1mm）
// 下に電池があるので、顔が少し下に見える → 上へ（約2mm＝-21 は上げすぎだったので、約1mm）
constexpr int   FACE_OFFSET_Y = -10;
constexpr float FACE_SCALE = 2.0f;   // 466px の丸い画面に合わせて2倍（目と口の「位置」の広がり）
constexpr float PART_SCALE = 0.7f;   // 目・口の「大きさ」だけを小さくする（1.0 でライブラリと同じ）
// 画面は約10.5ドットで1mm。顔の座標は2倍で描くので、顔の座標の1 ≒ 0.19mm
constexpr float EYE_SPACING = 0.6f;  // 目と目の間隔（1.0 でライブラリと同じ。小さいほど目が寄る。0.75→0.6 で片目 約2mm 内側）
constexpr float FACE_CENTER_X = 160; // 目を寄せる時の中心（320×240 のまん中）
constexpr float EYE_DOWN = 31;       // 目を下（口の方）へずらす量（顔の座標。15→31 で 約3mm 下）
constexpr float EYE_R      = 8;
// ライブラリの元の値は、右の目が 3 下（y=96）、口が 3 右にずれていて左右非対称。
// 画面の左の目を基準に、160 を中心にした左右対称にそろえた
// （元のスタックチャンには眉がないので、眉は描かない）
constexpr float EYE_R_X = 90,  EYE_R_Y = 93;    // 画面の左の目
constexpr float EYE_L_X = 230, EYE_L_Y = 93;    // 画面の右の目（元は y=96）
constexpr float MOUTH_X = 160, MOUTH_Y = 148;   // （元は x=163）
constexpr float MOUTH_MIN_W = 50, MOUTH_MAX_W = 90;
constexpr float MOUTH_MIN_H = 4,  MOUTH_MAX_H = 36;  // 開いた時の高さ（ライブラリは 60。開きすぎに見えたので 36）

constexpr uint32_t COL_FACE = 0xFFFFFF;  // 顔のパーツの色（背景は黒）

// ---- 怒りマーク（怒りの時、おでこの右上に赤で）----
// 目は (118, 124) と (202, 124)。顔の座標の1 ≒ 0.19mm
constexpr float    ANGER_X = 198, ANGER_Y = 96;   // マークの中心
constexpr float    ANGER_GAP = 2.2f;              // 中心からカギの角まで
constexpr float    ANGER_LEN = 5.0f;              // カギの長さ
constexpr float    ANGER_W   = 2.4f;              // 線の太さ
constexpr uint32_t COL_ANGER = 0xFF3B3B;

// ---- ハート（笑顔の時、おでこの右上にピンクで）----
// 最初は怒りマークと同じ (198, 96)。そこから 約3mm 上・約4mm 外側へ（顔の座標の1 ≒ 0.19mm）
constexpr float    HEART_X = 219, HEART_Y = 80;  // ハートの中心
constexpr float    HEART_R = 3.4f;               // 上の2つの丸の半径（ハートの幅は約4倍）
constexpr uint32_t COL_HEART = 0xFF6FB5;

// ---- 涙（悲しいの時、画面の左の目の下に水色で1つ）----
constexpr float    TEAR_R   = 3.6f;    // 雫の丸の半径
constexpr float    TEAR_GAP = 2.0f;    // 目の下から雫の先まで
constexpr uint32_t COL_TEAR = 0x6EC6FF;

// ---- 寝ている時（ミュート）の Zzz（頭の右上に、小さい Z から順に出る）----
constexpr float    ZZZ_X[3] = {186, 198, 212};
constexpr float    ZZZ_Y[3] = {84, 68, 50};
constexpr float    ZZZ_S[3] = {7, 9, 11};      // Z の大きさ
constexpr float    ZZZ_W = 2.0f;               // 線の太さ
constexpr float    ZZZ_RISE = 6.0f;            // 1周の間にのぼる量
constexpr uint32_t ZZZ_CYCLE_MS = 2400;
constexpr uint32_t ZZZ_FRAME_MS = 100;         // Zzz を描き直す間隔
constexpr uint32_t COL_ZZZ = 0xCFE8FF;

// ---- 回転 ----
// 12時間で1周（1分で0.5度）。確認用に、B長押しで早送り（DEMO_SPEED 倍）にできる
constexpr float DEMO_SPEED = 720.0f;    // 12時間を1分で回す

// ---- まばたき ----
constexpr uint32_t BLINK_MS = 140;
constexpr uint32_t BLINK_INTERVAL_MIN = 2200;
constexpr uint32_t BLINK_INTERVAL_MAX = 4700;

// ---- 消灯 ----
// 黒が多い顔なので画面の電力は小さい。ただ有機EL（AMOLED）は同じ絵を出し続けると焼き付くことがある
// （顔は回るが、下の電池マークは動かない）ので、長めの5分で消す（1分 → 20秒 → 5分）
constexpr bool     SCREEN_OFF_ENABLED = true;          // false にすると消灯しない（焼き付きと電池の減りに注意）
constexpr uint32_t SCREEN_OFF_MS = 5UL * 60 * 1000;    // さわらないと消すまでの時間

// ---- 声 ----
constexpr uint8_t  SPEAKER_VOLUME   = 180;   // 声のファイルは一番大きい所 0.5。これ以上にすると割れやすい
constexpr float    MOUTH_GAIN       = 1.6f;  // 音の大きさ → 口の開き具合
constexpr uint32_t NO_VOICE_TALK_MS = 2600;  // 声のファイルがない時の口パクの長さ

// ---- ミュート ----
constexpr uint32_t MUTE_HOLD_MS = 2000;
constexpr uint32_t MUTE_VIB_MS  = 150;

// ---- 電池（画面の下）----
constexpr int      BAT_Y       = 440;
constexpr int      BAT_ICON_W  = 32;
constexpr int      BAT_ICON_H  = 16;
constexpr uint32_t BAT_READ_MS = 60000;
constexpr uint32_t BAT_GREEN   = 0x34C759;
constexpr uint32_t BAT_YELLOW  = 0xFFCC00;
constexpr uint32_t BAT_RED     = 0xFF3B30;

}  // namespace stackchan
