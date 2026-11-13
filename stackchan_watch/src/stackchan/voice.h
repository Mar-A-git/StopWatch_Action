// スタックチャン時計の時刻の読み上げ（ずんだもん）
//
// ・LittleFS の /stackchan/voice/*.wav を「いまは」＋「ごご」＋「さんじ」＋「じゅうごふん」＋「なのだ」
//   の順につなげて1本の音にする（PSRAM）。再生は裏で進むので描画を邪魔しない
// ・口パク用に、20ms ごとの音の大きさを先に計算しておく
#pragma once

#include <stdint.h>

namespace stackchan {
namespace voice {

bool begin();                                // 声のファイルが入っていれば true
bool prepareTime(int hours, int minutes);    // 時刻の読み上げを用意する（hours は 0〜23）
void play();
void stop();
uint32_t durationMs();
float levelAt(uint32_t elapsedMs);           // 0〜1（口の開き具合）

}  // namespace voice
}  // namespace stackchan
