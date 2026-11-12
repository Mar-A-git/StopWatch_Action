#include "effect.h"

#include <M5Unified.h>
#include <esp_heap_caps.h>

#include "config.h"

namespace effect {
namespace {

M5Canvas spr(&M5.Display);
int W = 466, H = 466, CX = 234, CY = 234;

// 中心からの距離の表（|dx|, |dy| → 距離px）。内部 RAM に置く（約57KB）
constexpr int LUT_N = 240;
uint8_t* distLut = nullptr;
uint16_t* absX = nullptr;   // x → |x - CX|
uint16_t pal[256];          // 距離 → 色（スプライトの並びの RGB565）

inline uint8_t ch(uint32_t c, int shift) { return (c >> shift) & 0xFF; }

uint32_t lerpColor(uint32_t a, uint32_t b, float t) {
  if (t < 0) t = 0;
  if (t > 1) t = 1;
  const uint8_t r = ch(a, 16) + (ch(b, 16) - ch(a, 16)) * t;
  const uint8_t g = ch(a, 8) + (ch(b, 8) - ch(a, 8)) * t;
  const uint8_t bl = ch(a, 0) + (ch(b, 0) - ch(a, 0)) * t;
  return (uint32_t)r << 16 | (uint32_t)g << 8 | bl;
}

uint16_t toPix(uint32_t c, float k) {
  if (k < 0) k = 0;
  auto f = [k](uint8_t v) -> uint8_t {
    const float x = v * k;
    return x > 255 ? 255 : (uint8_t)x;
  };
  return lgfx::v1::swap565(f(ch(c, 16)), f(ch(c, 8)), f(ch(c, 0)));
}

float easeOut(float t) {
  if (t <= 0) return 0;
  if (t >= 1) return 1;
  return 1 - (1 - t) * (1 - t) * (1 - t);
}

// 半径 r の位置の、本来のグラデーションの色（光が広がりきった時の色）
uint32_t gradientAt(int r) {
  if (r < R_KA) return lerpColor(COL_ORANGE_CORE, COL_ORANGE, (float)r / R_KA);
  if (r < R_ME) return lerpColor(COL_ORANGE, COL_YELLOW, (float)(r - R_KA) / (R_ME - R_KA));
  return lerpColor(COL_YELLOW, COL_WHITE, (float)(r - R_ME) / (R_HA - R_ME));
}

// パレットを作る
//   edge      : 光っている一番外の半径（ここまで光る）
//   holeIn/Out: この範囲は消す（④の点滅で外側を消す時に使う）
//   bright    : 全体の明るさ
void buildPalette(int edge, float bright, int holeIn = -1, int holeOut = -1) {
  for (int r = 0; r < 256; r++) {
    uint16_t c = 0;
    if (r < edge) {
      if (!(r >= holeIn && r < holeOut)) c = toPix(gradientAt(r), bright);
    } else if (r < edge + GLOW_PX) {
      // ふちの外側をにじませる
      const float k = 1.0f - (float)(r - edge) / GLOW_PX;
      c = toPix(gradientAt(edge - 1), bright * k * k);
    }
    pal[r] = c;
  }
  // 消した範囲の内側のふちもにじませる
  if (holeIn >= 0) {
    for (int i = 0; i < GLOW_PX && holeIn + i < 256 && holeIn + i < holeOut; i++) {
      const float k = 1.0f - (float)i / GLOW_PX;
      pal[holeIn + i] = toPix(gradientAt(holeIn - 1), bright * k * k);
    }
  }
}

void (*sampleHook)() = nullptr;

void fillFromPalette() {
  auto* buf = (uint16_t*)spr.getBuffer();
  for (int y = 0; y < H; y++) {
    if (sampleHook && y % 120 == 0) sampleHook();
    const uint8_t* row = distLut + abs(y - CY) * LUT_N;
    uint16_t* out = buf + y * W;
    for (int x = 0; x < W; x++) out[x] = pal[row[absX[x]]];
  }
}

uint32_t hash(uint32_t x) {
  x ^= x >> 16;
  x *= 0x7feb352d;
  x ^= x >> 15;
  x *= 0x846ca68b;
  x ^= x >> 16;
  return x;
}

// 光の粒: 画面のふちから中心（inner）へ吸い込まれる
void drawSparks(int count, uint32_t t, int inner) {
  for (int i = 0; i < count; i++) {
    const uint32_t off = hash(i * 7 + 1) % SPARK_TRIP_MS;
    const uint32_t tt = t + off;
    const uint32_t cycle = tt / SPARK_TRIP_MS;
    const float frac = (float)(tt % SPARK_TRIP_MS) / SPARK_TRIP_MS;
    const float ang = (hash(i * 131 + cycle * 977) % 3600) * (PI / 1800.0f);
    const float rr = R_HA - (R_HA - inner) * (frac * frac);  // だんだん速く吸い込まれる
    const int x = CX + (int)(cosf(ang) * rr);
    const int y = CY + (int)(sinf(ang) * rr);
    const int size = frac < 0.7f ? 3 : 2;
    spr.fillCircle(x, y, size, spr.color888(ch(COL_SPARK, 16), ch(COL_SPARK, 8), ch(COL_SPARK, 0)));
  }
}

// ⑤ の青白い炎: 中心は白、外へ行くほど水色。外へ流れる波で明るさをゆらす
void buildFirePalette(float bright, uint32_t t) {
  for (int r = 0; r < 256; r++) {
    uint16_t c = 0;
    if (r < R_HA + GLOW_PX) {
      const uint32_t base = r < R_ME ? lerpColor(COL_FIRE_CORE, COL_FIRE_MID, (float)r / R_ME)
                                     : lerpColor(COL_FIRE_MID, COL_FIRE_EDGE, (float)(r - R_ME) / (R_HA - R_ME));
      const float wave = 0.78f + 0.22f * sinf(r * 0.11f - t * 0.045f);
      float k = bright * wave;
      if (r >= R_HA) k *= 1.0f - (float)(r - R_HA) / GLOW_PX;
      c = toPix(base, k);
    }
    pal[r] = c;
  }
}

// ⑤ の光の粒: 中心から外へ飛び散る
void drawSparksOut(int count, uint32_t t) {
  const uint16_t col = spr.color888(ch(COL_FIRE_SPARK, 16), ch(COL_FIRE_SPARK, 8), ch(COL_FIRE_SPARK, 0));
  for (int i = 0; i < count; i++) {
    const uint32_t off = hash(i * 13 + 5) % FIRE_SPARK_TRIP_MS;
    const uint32_t tt = t + off;
    const uint32_t cycle = tt / FIRE_SPARK_TRIP_MS;
    const float frac = (float)(tt % FIRE_SPARK_TRIP_MS) / FIRE_SPARK_TRIP_MS;
    const float ang = (hash(i * 211 + cycle * 1009) % 3600) * (PI / 1800.0f);
    const float rr = 20 + (R_HA - 20) * sqrtf(frac);  // 最初が速い
    const int x = CX + (int)(cosf(ang) * rr);
    const int y = CY + (int)(sinf(ang) * rr);
    spr.fillCircle(x, y, frac < 0.5f ? 4 : 3, col);
  }
}

void drawLabel(const char* s) {
  if (!SHOW_PHASE_LABEL) return;
  spr.setFont(&fonts::lgfxJapanGothic_24);
  spr.setTextDatum(middle_center);
  const int w = spr.textWidth(s) + 16;
  spr.fillRoundRect(CX - w / 2, 18, w, 32, 8, TFT_BLACK);
  spr.setTextColor(TFT_WHITE);
  spr.drawString(s, CX, 35);
}

// 少しだけ明るさをゆらす（エネルギーっぽく）
float flicker() { return 0.92f + 0.08f * (esp_random() % 1000) / 1000.0f; }

float pulse(uint32_t t, uint32_t period, float depth) {
  return 1.0f - depth * 0.5f * (1.0f + cosf(2 * PI * (float)(t % period) / period));
}

}  // namespace

void setSampleHook(void (*fn)()) { sampleHook = fn; }

void begin() {
  W = M5.Display.width();
  H = M5.Display.height();
  CX = W / 2 + SCREEN_DX;
  CY = H / 2 + SCREEN_DY;

  spr.setPsram(true);
  spr.setColorDepth(16);
  if (!spr.createSprite(W, H)) Serial.println("[FX] createSprite FAILED");

  distLut = (uint8_t*)heap_caps_malloc(LUT_N * LUT_N, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  absX = (uint16_t*)heap_caps_malloc(W * sizeof(uint16_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  if (!distLut || !absX) {
    Serial.println("[FX] LUT alloc FAILED");
    return;
  }
  for (int dy = 0; dy < LUT_N; dy++) {
    for (int dx = 0; dx < LUT_N; dx++) {
      const int d = (int)lroundf(sqrtf((float)(dx * dx + dy * dy)));
      distLut[dy * LUT_N + dx] = d > 255 ? 255 : d;
    }
  }
  for (int x = 0; x < W; x++) {
    const int a = abs(x - CX);
    absX[x] = a < LUT_N ? a : LUT_N - 1;
  }
}

void drawIdle(bool linked, int8_t rssiAvg, int recvPerSec) {
  spr.fillScreen(TFT_BLACK);

  // 外周の輪: 受信レベルの色（開始のしきい値より強いとオレンジ）
  uint32_t ring = COL_DARK;
  if (linked) ring = rssiAvg >= RSSI_START ? COL_ORANGE : (rssiAvg >= RSSI_APART ? COL_YELLOW : COL_GRAY);
  spr.fillArc(CX, CY, 226, 216, 0, 360, spr.color888(ch(ring, 16), ch(ring, 8), ch(ring, 0)));

  spr.setTextDatum(middle_center);
  spr.setFont(&fonts::lgfxJapanGothic_28);
  spr.setTextColor(IS_PARENT ? COL_ORANGE : COL_CYAN);
  spr.drawString(IS_PARENT ? "親機（左手）" : "子機（右手）", CX, 80);

  char line[48];
  if (linked) {
    spr.setFont(&fonts::FreeSansBold24pt7b);
    spr.setTextSize(2);
    spr.setTextColor(TFT_WHITE);
    spr.setTextDatum(middle_right);
    spr.drawNumber(rssiAvg, CX + 60, 200);
    spr.setTextSize(1);
    spr.setFont(&fonts::lgfxJapanGothic_24);
    spr.setTextDatum(middle_left);
    spr.drawString("dBm", CX + 68, 218);
    spr.setTextDatum(middle_center);
    spr.setFont(&fonts::lgfxJapanGothic_20);
    spr.setTextColor(COL_GRAY);
    snprintf(line, sizeof(line), "受信 %d回/秒", recvPerSec);
    spr.drawString(line, CX, 268);
  } else {
    spr.setFont(&fonts::lgfxJapanGothic_40);
    spr.setTextColor(COL_GRAY);
    spr.drawString("未受信", CX, 200);
  }

  spr.setFont(&fonts::lgfxJapanGothic_24);
  spr.setTextDatum(middle_center);
  spr.setTextColor(TFT_WHITE);
  spr.drawString(IS_PARENT ? "ボタンAで開始" : "親機に合わせて光る", CX, 340);

  spr.pushSprite(0, 0);
}

void drawPhase(uint8_t phase, uint32_t t) {
  int sparks = 0;
  bool fireSparks = false;
  int inner = R_KA;
  const char* label = "";

  switch (phase) {
    case PH_KA: {
      // 中心にオレンジの玉がふわっと現れ、脈を打つ
      const int edge = (int)(R_KA * easeOut(t / 400.0f));
      buildPalette(edge, pulse(t, VIB_PERIOD_KA, 0.25f) * flicker());
      sparks = 6;
      inner = R_KA / 2;
      label = "1 か";
      break;
    }
    case PH_ME1: {
      const int edge = R_KA + (int)((R_ME - R_KA) * easeOut((float)t / GROW_MS));
      buildPalette(edge, pulse(t, VIB_PERIOD_ME1, 0.2f) * flicker());
      sparks = 12;
      label = "2 め";
      break;
    }
    case PH_HA: {
      const int edge = R_ME + (int)((R_HA - R_ME) * easeOut((float)t / GROW_MS));
      buildPalette(edge, pulse(t, VIB_PERIOD_HA, 0.15f) * flicker());
      sparks = 20;
      inner = R_ME / 2;
      label = "3 は";
      break;
    }
    case PH_ME2: {
      // 白い部分（③で広がった外側）が点滅する
      const bool on = (t / ME2_BLINK_HALF_MS) % 2 == 0;
      if (on) buildPalette(R_HA, flicker());
      else    buildPalette(R_HA, flicker(), R_ME, R_HA + GLOW_PX);
      sparks = 20;
      inner = R_ME / 2;
      label = t < PHASE_MS ? "4 め" : "4 め（撃てる）";
      break;
    }
    case PH_FIRE: {
      if (t < FIRE_FLASH_MS) {
        // 最初は真っ白
        for (int r = 0; r < 256; r++) pal[r] = toPix(COL_WHITE, 1.0f);
      } else {
        // 点滅: 青白い炎 → 消える → 溜めた色（オレンジ・黄・白）→ 消える → 青白い炎 …
        // 消えている時も真っ黒にせず、青白くうっすら光らせる。最後の 500ms でだんだん暗く
        const uint32_t k = (t - FIRE_FLASH_MS) / FIRE_BLINK_HALF_MS;
        float fade = 1.0f;
        if (t > FIRE_MS - 500) fade = (float)(FIRE_MS - t) / 500.0f;
        if (k % 2 == 1)          buildFirePalette(FIRE_OFF_LEVEL * fade, t);
        else if ((k / 2) % 2 == 0) buildFirePalette(fade, t);
        else                     buildPalette(R_HA, fade);
        fireSparks = t < FIRE_MS - 300;
      }
      label = "5 はっ！";
      break;
    }
    default:
      buildPalette(0, 0);
      break;
  }

  fillFromPalette();
  if (sparks > 0) drawSparks(sparks, t, inner);
  if (fireSparks) drawSparksOut(FIRE_SPARKS, t);
  drawLabel(label);
  spr.pushSprite(0, 0);
}

}  // namespace effect
