#include "voice.h"

#include <LittleFS.h>
#include <M5Unified.h>

#include "config.h"

namespace stackchan {
namespace voice {
namespace {

constexpr const char* DIR = "/stackchan/voice";
constexpr uint32_t GAP_MS = 30;          // 部品と部品の間の無音
constexpr uint32_t LEVEL_MS = 20;        // 口パク用に大きさを計算する区切り
constexpr uint32_t MAX_SEC = 8;          // つなげた音の上限

bool     ok = false;
uint32_t rate = 24000;                   // 最初に読んだファイルのサンプリングレートに合わせる
int16_t* pcm = nullptr;
size_t   pcmLen = 0, pcmMax = 0;
uint8_t* levels = nullptr;
size_t   levelLen = 0, levelMax = 0;

bool appendWav(const char* name) {
  char path[48];
  snprintf(path, sizeof(path), "%s/%s", DIR, name);
  File f = LittleFS.open(path, "r");
  if (!f) {
    Serial.printf("[VOICE] %s がない\n", path);
    return false;
  }
  uint8_t hdr[12];
  if (f.read(hdr, 12) != 12 || memcmp(hdr, "RIFF", 4) != 0 || memcmp(hdr + 8, "WAVE", 4) != 0) return false;
  while (f.available() >= 8) {
    uint8_t ch[8];
    f.read(ch, 8);
    const uint32_t size = ch[4] | (ch[5] << 8) | (ch[6] << 16) | ((uint32_t)ch[7] << 24);
    if (memcmp(ch, "fmt ", 4) == 0 && size >= 16) {
      uint8_t fmt[16];
      f.read(fmt, 16);
      rate = fmt[4] | (fmt[5] << 8) | (fmt[6] << 16) | ((uint32_t)fmt[7] << 24);
      f.seek(f.position() + size - 16 + (size & 1));
      continue;
    }
    if (memcmp(ch, "data", 4) == 0) {
      size_t n = size / 2;
      if (pcmLen + n > pcmMax) n = pcmMax - pcmLen;
      f.read((uint8_t*)(pcm + pcmLen), n * 2);
      pcmLen += n;
      return true;
    }
    f.seek(f.position() + size + (size & 1));
  }
  return false;
}

void appendSilence(uint32_t ms) {
  size_t n = (size_t)rate * ms / 1000;
  if (pcmLen + n > pcmMax) n = pcmMax - pcmLen;
  memset(pcm + pcmLen, 0, n * 2);
  pcmLen += n;
}

void buildLevels() {
  const size_t step = rate * LEVEL_MS / 1000;
  levelLen = (pcmLen + step - 1) / step;
  if (levelLen > levelMax) levelLen = levelMax;
  uint32_t maxAvg = 1;
  static uint32_t avg[MAX_SEC * 1000 / LEVEL_MS + 1];
  for (size_t i = 0; i < levelLen; i++) {
    uint64_t sum = 0;
    const size_t end = min(pcmLen, (i + 1) * step);
    for (size_t s = i * step; s < end; s++) sum += abs(pcm[s]);
    avg[i] = (uint32_t)(sum / step);
    if (avg[i] > maxAvg) maxAvg = avg[i];
  }
  for (size_t i = 0; i < levelLen; i++) levels[i] = (uint8_t)min<uint32_t>(255, avg[i] * 255 / maxAvg);
}

}  // namespace

bool begin() {
  if (!LittleFS.begin(false)) {
    Serial.println("[VOICE] LittleFS を開けない（声を書き込んでいない？）→ 口パクだけで動く");
    return false;
  }
  char path[48];
  snprintf(path, sizeof(path), "%s/intro.wav", DIR);
  ok = LittleFS.exists(path);
  // 24kHz で8秒分（最初に読むファイルが 16kHz でも足りる）
  pcmMax = 24000 * MAX_SEC;
  levelMax = MAX_SEC * 1000 / LEVEL_MS + 1;
  pcm = (int16_t*)ps_malloc(pcmMax * sizeof(int16_t));
  levels = (uint8_t*)ps_malloc(levelMax);
  if (!pcm || !levels) ok = false;
  M5.Speaker.setVolume(SPEAKER_VOLUME);
  Serial.printf("[VOICE] stackchan voice %s\n", ok ? "found" : "NOT FOUND");
  return ok;
}

bool prepareTime(int hours, int minutes) {
  pcmLen = 0;
  levelLen = 0;
  if (!ok) return false;
  int h12 = hours % 12;
  if (h12 == 0) h12 = 12;
  char hName[12], mName[12];
  snprintf(hName, sizeof(hName), "h%02d.wav", h12);
  snprintf(mName, sizeof(mName), "m%02d.wav", minutes);
  const char* parts[] = {"intro.wav", hours < 12 ? "am.wav" : "pm.wav", hName, mName, "outro.wav"};
  const uint32_t t0 = millis();
  for (size_t i = 0; i < sizeof(parts) / sizeof(parts[0]); i++) {
    if (i > 0) appendSilence(GAP_MS);
    if (!appendWav(parts[i])) {
      pcmLen = 0;
      return false;
    }
  }
  buildLevels();
  Serial.printf("[VOICE] %d:%02d -> %u ms (prepare %lu ms)\n", hours, minutes,
                (unsigned)durationMs(), millis() - t0);
  return true;
}

void play() {
  if (pcmLen == 0) return;
  M5.Speaker.setVolume(SPEAKER_VOLUME);
  M5.Speaker.playRaw(pcm, pcmLen, rate, false, 1, 0, true);
}

void stop() { M5.Speaker.stop(); }

uint32_t durationMs() { return (uint32_t)((uint64_t)pcmLen * 1000 / rate); }

float levelAt(uint32_t elapsedMs) {
  const size_t i = elapsedMs / LEVEL_MS;
  if (i >= levelLen) return 0;
  return levels[i] / 255.0f;
}

}  // namespace voice
}  // namespace stackchan
