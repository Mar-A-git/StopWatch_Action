// ずんだもんの声
//   LittleFS の /voice/*.wav（16kHz / 16bit / モノラル）を起動時に PSRAM へ読み込んでおき、
//   フェーズが変わった瞬間に再生する。再生は裏で進むので描画を邪魔しない
#pragma once

#include <stdint.h>

namespace voice {

bool begin();               // 読み込めた数をログに出す。1つもなければ false（声なしで動く）
// そのフェーズの声を、頭から offsetMs だけ飛ばして再生する
// （子機は親機より少し遅れて始まるので、遅れた分を飛ばして2台の声をそろえる）
void play(uint8_t phase, uint32_t offsetMs = 0);
void stop();

}  // namespace voice
