// かめはめ波の定数
#pragma once

#include <stdint.h>

#ifndef ROLE_PARENT
#error "platformio.ini の env:oya か env:ko を選んでビルドしてください"
#endif

static constexpr bool IS_PARENT = (ROLE_PARENT != 0);

// ---- フェーズ ----
enum Phase : uint8_t {
  PH_IDLE = 0,  // 待機（受信レベルを表示）
  PH_KA   = 1,  // ① か
  PH_ME1  = 2,  // ② め
  PH_HA   = 3,  // ③ は
  PH_ME2  = 4,  // ④ め（終わってから ME2_WAIT_MS 撃たなければリセット）
  PH_FIRE = 5,  // ⑤ はっ！
};

static constexpr uint32_t PHASE_MS    = 3000;  // ①〜④ の長さ
static constexpr uint32_t ME2_WAIT_MS = 5000;  // ④ が終わってから撃つのを待つ時間
static constexpr uint32_t FIRE_MS     = 2000;  // ⑤ の長さ

// 確認用: 画面の上にフェーズの名前を出す（false で消える）
static constexpr bool SHOW_PHASE_LABEL = true;

// ---- 画面 ----
// 画面は直径 約44mm（約10.5px/mm）。「中心10mm」は直径として作る
static constexpr int SCREEN_DX = 1;    // 中心の補正（実機で測った値）
static constexpr int SCREEN_DY = 1;
static constexpr int R_KA  = 52;       // ① 直径10mm
static constexpr int R_ME  = 105;      // ② 直径20mm
static constexpr int R_HA  = 230;      // ③ 画面のふちまで
static constexpr int GLOW_PX = 8;      // 光の外側のにじみ

static constexpr uint32_t GROW_MS          = 700;  // ②③ で光が外へ広がる時間
static constexpr uint32_t ME2_BLINK_HALF_MS = 120; // ④ 白い部分の点滅（点いている時間＝消えている時間）
static constexpr uint32_t FIRE_FLASH_MS     = 150; // ⑤ 最初の真っ白な光
static constexpr uint32_t FIRE_BLINK_HALF_MS = 90; // ⑤ 全体の点滅
static constexpr float    FIRE_OFF_LEVEL    = 0.18f; // ⑤ 点滅の「消えている」時も、青白くうっすら光らせる
static constexpr uint8_t  FIRE_BRIGHTNESS   = 255;   // ⑤ の間だけ画面をいちばん明るくする
static constexpr int      FIRE_SPARKS       = 36;    // ⑤ 中心から外へ飛び散る光の粒
static constexpr uint32_t FIRE_SPARK_TRIP_MS = 380;

// 光の粒（外から中心へ吸い込まれる）
static constexpr int      SPARK_MAX     = 24;
static constexpr uint32_t SPARK_TRIP_MS = 750;   // ふちから中心まで

static constexpr uint32_t COL_ORANGE      = 0xFF8C00;
static constexpr uint32_t COL_ORANGE_CORE = 0xFFC060;  // ① の中心（少し明るい）
static constexpr uint32_t COL_YELLOW      = 0xFFE040;
static constexpr uint32_t COL_WHITE       = 0xFFFFFF;
static constexpr uint32_t COL_SPARK       = 0xFFF4C0;
// ⑤ の青白い炎（中心は白 → 水色っぽい白 → ふちは水色）
static constexpr uint32_t COL_FIRE_CORE   = 0xFFFFFF;
static constexpr uint32_t COL_FIRE_MID    = 0xC8F4FF;
static constexpr uint32_t COL_FIRE_EDGE   = 0x38C8FF;
static constexpr uint32_t COL_FIRE_SPARK  = 0xE0FAFF;
static constexpr uint32_t COL_CYAN        = 0x40D8FF;
static constexpr uint32_t COL_GRAY        = 0x707070;
static constexpr uint32_t COL_DARK        = 0x303030;

// ---- 電池の残量（待機画面の下に出す。①が始まったら消える）----
static constexpr int      BAT_Y       = 400;       // 行の中心のY（外周の輪の内側に収まる高さ）
static constexpr int      BAT_ICON_W  = 32;        // 電池マークの大きさ
static constexpr int      BAT_ICON_H  = 16;
static constexpr uint32_t BAT_READ_MS = 60000;     // 残量を読む間隔（起動直後にも1回読む）
static constexpr uint32_t BAT_GREEN   = 0x34C759;  // 50% より多い
static constexpr uint32_t BAT_YELLOW  = 0xFFCC00;  // 50% 以下
static constexpr uint32_t BAT_RED     = 0xFF3B30;  // 20% 以下

// ---- 振動（0〜255）----
// 弱すぎるとモーターが回らない。170〜230 くらいが目安
static constexpr uint8_t  VIB_KA   = 120;
static constexpr uint8_t  VIB_ME1  = 145;
static constexpr uint8_t  VIB_HA   = 170;
static constexpr uint8_t  VIB_ME2  = 190;
static constexpr uint8_t  VIB_FIRE = 255;
static constexpr uint32_t VIB_FIRE_MS = 700;   // ⑤ の最初に強く震える時間
// ①〜③ は「トン…トン…」と脈を打つ。だんだん速くなる
static constexpr uint32_t VIB_PERIOD_KA  = 700;
static constexpr uint32_t VIB_PERIOD_ME1 = 450;
static constexpr uint32_t VIB_PERIOD_HA  = 280;
static constexpr uint32_t VIB_ON_MS      = 90;

// ---- 無線 ----
static constexpr uint8_t  WIFI_CHANNEL   = 1;
static constexpr uint32_t SEND_PERIOD_MS = 50;    // 1秒に20回送る（画面とは別のタスクで送る）
static constexpr int      AVG_N          = 5;     // 受信レベルの平均を取る回数（0.25秒）
static constexpr uint32_t LINK_LOST_MS   = 500;   // これだけ受け取れなければ「未受信」
static constexpr int8_t   TX_POWER       = 84;    // 21dBm（0.25dBm 単位。rssi_check で 21dBm で十分だった）

// ---- 判定 ----
// 受信レベル（rssi_check での実測: 向かい合わせ -5〜-10 / 開く -20 より弱い / 30cm -36）
static constexpr int8_t   RSSI_START = -12;  // これより強いと「向かい合わせ」
static constexpr int8_t   RSSI_APART = -18;  // これより弱いと「離れた」
static constexpr uint32_t START_HOLD_MS = 300;  // 向かい合わせがこれだけ続いたら ① を始める
static constexpr uint32_t APART_HOLD_MS = 150;  // 離れたのがこれだけ続いたらリセット（一瞬の揺れは無視）
// 待機に戻ったあと、一度手を離すまでは次を始めない（手を合わせたままだと、すぐまた始まってしまうため）

// 突き出し（はっ！）
static constexpr float    THRUST_ACCEL_G     = 1.8f;  // 加速度の大きさ（g）がこれ以上で「突き出した」（止まっている時は 1g）
static constexpr float    THRUST_LOG_G       = 1.4f;  // これ以上の時はログに出す（しきい値を決めるため）
static constexpr uint32_t THRUST_COOLDOWN_MS = 600;
static constexpr uint32_t THRUST_PAIR_MS     = 400;   // 2台の突き出しがこの時間の中なら「同時」
// ④ の間に離れたら、2台の突き出しが来るのをこれだけ待つ。来なければリセット
static constexpr uint32_t FIRE_GRACE_MS      = 500;
// true にすると、④ の間に離れただけで（突き出しがなくても）撃つ
static constexpr bool     FIRE_ON_APART      = false;

// ---- 声 ----
// false にすると、声は親機だけから出す
// 2026-11-11: ジジジのノイズは2台から出したせいではなく、声のファイル（VOICEVOX で伸ばしすぎ）が原因だったので、2台に戻した
static constexpr bool     VOICE_ON_CHILD = true;
// 本体の音量（0〜255）。音量の差は声のファイル側で付けている
// 180 は聞こえにくかったので 240（2026-11-11）。声のファイルは一番大きい「はっ」でも 0.21 なので割れにくい
static constexpr uint8_t  SPEAKER_VOLUME = 240;

static constexpr bool POWER_LED_OFF = true;  // 本体の緑の LED を消す
