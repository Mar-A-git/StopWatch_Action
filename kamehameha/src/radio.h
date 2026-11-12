// ESP-NOW で親機と子機をつなぐ
//   ・1秒に20回、合図を送り合う（画面の処理とは別のタスクで送るので、描画が重くても遅れない）
//   ・相手の合図の受信レベル（RSSI）を平均して持っておく
//   ・親機は合図に「今のフェーズと、そのフェーズが始まってからの時間」を入れる。子機はそれに合わせる
#pragma once

#include <stdint.h>

namespace radio {

void begin();

// loop から呼ぶ。届いた合図をまとめて処理する
void update();

bool    linked();        // 最近 LINK_LOST_MS 以内に相手から届いたか
int8_t  rssiAvg();       // 受信レベルの平均（dBm）
int     recvPerSec();    // 1秒に何回届いたか

// 自分のフェーズを合図に載せる
void publish(uint8_t phase, uint32_t phaseStartMs);

// 自分が突き出したことを相手に知らせる
void noteThrust(uint32_t ms);

// 相手が最後に突き出した時刻（自分の millis()。まだなら 0）
uint32_t peerThrustMs();

// 子機用: 親機のフェーズ（届いていなければ false）
// startMs は「親機でそのフェーズが始まった時刻」を自分の millis() に直したもの
bool parentPhase(uint8_t& phase, uint32_t& startMs);

}  // namespace radio
