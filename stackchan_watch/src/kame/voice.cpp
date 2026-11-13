#include "voice.h"

#include <LittleFS.h>
#include <M5Unified.h>

#include "config.h"

namespace voice {
namespace {

constexpr uint32_t SAMPLE_RATE = 16000;
const char* const FILES[] = {nullptr, "/kame/voice/ka.wav", "/kame/voice/me1.wav", "/kame/voice/ha.wav",
                             "/kame/voice/me2.wav", "/kame/voice/fire.wav"};
constexpr int N = sizeof(FILES) / sizeof(FILES[0]);

int16_t* pcm[N] = {};
size_t   len[N] = {};
uint32_t rate[N] = {};   // ファイルごとのサンプリングレート（16kHz でも 24kHz でもよい）

bool load(int i) {
  File f = LittleFS.open(FILES[i], "r");
  if (!f) {
    Serial.printf("[VOICE] %s がない\n", FILES[i]);
    return false;
  }
  uint8_t hdr[12];
  if (f.read(hdr, 12) != 12 || memcmp(hdr, "RIFF", 4) != 0 || memcmp(hdr + 8, "WAVE", 4) != 0) {
    Serial.printf("[VOICE] %s は WAV ではない\n", FILES[i]);
    return false;
  }
  while (f.available() >= 8) {
    uint8_t ch[8];
    f.read(ch, 8);
    const uint32_t size = ch[4] | (ch[5] << 8) | (ch[6] << 16) | ((uint32_t)ch[7] << 24);
    if (memcmp(ch, "fmt ", 4) == 0 && size >= 16) {
      uint8_t fmt[16];
      f.read(fmt, 16);
      const uint16_t channels = fmt[2] | (fmt[3] << 8);
      rate[i] = fmt[4] | (fmt[5] << 8) | (fmt[6] << 16) | ((uint32_t)fmt[7] << 24);
      const uint16_t bits = fmt[14] | (fmt[15] << 8);
      if (channels != 1 || bits != 16) {
        Serial.printf("[VOICE] %s はモノラル・16bit ではない（%uch / %ubit）\n", FILES[i], channels, bits);
        return false;
      }
      f.seek(f.position() + size - 16 + (size & 1));
      continue;
    }
    if (memcmp(ch, "data", 4) == 0) {
      if (rate[i] == 0) rate[i] = SAMPLE_RATE;
      pcm[i] = (int16_t*)ps_malloc(size);
      if (!pcm[i]) return false;
      len[i] = f.read((uint8_t*)pcm[i], size) / 2;
      return true;
    }
    f.seek(f.position() + size + (size & 1));
  }
  return false;
}

}  // namespace

bool begin() {
  if (!LittleFS.begin(false)) {
    Serial.println("[VOICE] LittleFS を開けない（声のファイルを書き込んでいない？）→ 声なしで動く");
    return false;
  }
  int ok = 0;
  for (int i = 1; i < N; i++) {
    if (load(i)) {
      ok++;
      Serial.printf("[VOICE] %s  %u ms  %u Hz\n", FILES[i], (unsigned)(len[i] * 1000 / rate[i]),
                    (unsigned)rate[i]);
    }
  }
  M5.Speaker.setVolume(SPEAKER_VOLUME);
  Serial.printf("[VOICE] %d / %d files\n", ok, N - 1);
  return ok > 0;
}

void play(uint8_t phase, uint32_t offsetMs) {
  M5.Speaker.stop();
  if (phase >= N || !pcm[phase]) return;
  const size_t skip = (size_t)offsetMs * rate[phase] / 1000;
  if (skip >= len[phase]) return;
  M5.Speaker.playRaw(pcm[phase] + skip, len[phase] - skip, rate[phase], false, 1, 0, true);
}

void stop() { M5.Speaker.stop(); }

}  // namespace voice
