// WiFi + NTP で RTC の時刻を合わせる
//
// ・最後に合わせた日時を記録し、そこから8時間以上たっていたら WiFi へつないで合わせる
//   （時刻が消えている時・まだ一度も合わせていない時もすぐに合わせる）
// ・8時間は RTC の時刻で比べるので、スリープで本体のタイマーが止まっても正しく判定できる
// ・調べるのは起動時・起きた時（netTimeOnWake）と、それ以外は1分に1回
// ・合わせ終わったら WiFi は切る（電池の節約）。失敗したら30分後にもう一度
// ・すべて非ブロッキング。つながらなくても時計やアニメーションは止まらない
#pragma once

#include <stdint.h>

void netTimeBegin(uint32_t now);
void netTimeUpdate(uint32_t now);

// 消灯・スリープから起きた時に呼ぶ。次の netTimeUpdate ですぐに「8時間たったか」を調べる
void netTimeOnWake();

// WiFi につないでいる最中か（この間はスリープしない）
bool netTimeBusy();

// ログ用
const char* netTimeStateName();
const char* netTimeLastSyncText();
