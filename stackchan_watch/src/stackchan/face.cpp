#include "face.h"

#include "config.h"

namespace stackchan {
namespace {

// ライブラリの顔は 320×240。この点を回転の中心にする（目と口のまん中あたり）
constexpr float PIVOT_X = 160.0f;
constexpr float PIVOT_Y = 122.0f;

struct Xf {
  float c, s, scale;
  int cx, cy;
  // 顔の座標 (x, y) → 画面の座標
  void apply(float x, float y, int& ox, int& oy) const {
    const float dx = (x - PIVOT_X) * scale;
    const float dy = (y - PIVOT_Y) * scale;
    ox = cx + (int)lroundf(dx * c - dy * s);
    oy = cy + (int)lroundf(dx * s + dy * c);
  }
};

M5Canvas* G = nullptr;
Xf X;

uint16_t rgb(uint32_t c) { return G->color565((c >> 16) & 0xFF, (c >> 8) & 0xFF, c & 0xFF); }

void tri(float x0, float y0, float x1, float y1, float x2, float y2, uint16_t col) {
  int a, b, c, d, e, f;
  X.apply(x0, y0, a, b);
  X.apply(x1, y1, c, d);
  X.apply(x2, y2, e, f);
  G->fillTriangle(a, b, c, d, e, f, col);
}

// 顔の座標の四角形（回転するので三角形2つで塗る）
void rect(float x, float y, float w, float h, uint16_t col) {
  tri(x, y, x + w, y, x + w, y + h, col);
  tri(x, y, x + w, y + h, x, y + h, col);
}

void circle(float x, float y, float r, uint16_t col) {
  int ox, oy;
  X.apply(x, y, ox, oy);
  G->fillCircle(ox, oy, (int)lroundf(r * X.scale), col);
}

// 太さ w の線（端は丸くする）
void line(float x0, float y0, float x1, float y1, float w, uint16_t col) {
  const float dx = x1 - x0, dy = y1 - y0;
  const float len = sqrtf(dx * dx + dy * dy);
  if (len <= 0) return;
  const float nx = -dy / len * w / 2, ny = dx / len * w / 2;
  tri(x0 + nx, y0 + ny, x1 + nx, y1 + ny, x1 - nx, y1 - ny, col);
  tri(x0 + nx, y0 + ny, x1 - nx, y1 - ny, x0 - nx, y0 - ny, col);
  circle(x0, y0, w / 2, col);
  circle(x1, y1, w / 2, col);
}

// M5Stack-Avatar の Eye::draw と同じ形
void eye(float x, float y, bool isLeft, const FaceState& s, uint16_t fg, uint16_t bg) {
  const float r = EYE_R * PART_SCALE;
  const float k = PART_SCALE;
  if (!s.eyesOpen || s.sleeping) {
    rect(x - r, y - 2 * k, r * 2, 4 * k, fg);
    return;
  }
  circle(x, y, r, fg);
  if (s.exp == Expression::Angry || s.exp == Expression::Sad) {
    const float x0 = x - r, y0 = y - r, x1 = x0 + r * 2;
    const float x2 = (!isLeft != !(s.exp == Expression::Sad)) ? x0 : x1;
    tri(x0, y0, x1, y0, x2, y0 + r, bg);
  }
  if (s.exp == Expression::Happy) {
    const float x0 = x - r, y0 = y;
    circle(x, y, r / 1.5f, bg);
    rect(x0, y0, r * 2 + 4 * k, r + 2 * k, bg);
  }
}

// 怒りマーク（赤）: 4つのカギが中心を向いた形
void angerMark() {
  const uint16_t col = rgb(COL_ANGER);
  const float cx = ANGER_X, cy = ANGER_Y, g = ANGER_GAP, L = ANGER_LEN, w = ANGER_W;
  for (int i = 0; i < 4; i++) {
    const float sx = (i & 1) ? 1 : -1;
    const float sy = (i & 2) ? 1 : -1;
    const float vx = cx + sx * g, vy = cy + sy * g;  // カギの角
    line(vx, vy, vx + sx * L, vy, w, col);
    line(vx, vy, vx, vy + sy * L, w, col);
  }
}

// ハート（ピンク）: 丸2つ＋下向きの三角
void heart() {
  const uint16_t col = rgb(COL_HEART);
  const float r = HEART_R, x = HEART_X, y = HEART_Y;
  const float top = y - r * 0.4f;  // 丸の中心の高さ
  circle(x - r * 0.95f, top, r, col);
  circle(x + r * 0.95f, top, r, col);
  tri(x - r * 1.9f, top + r * 0.35f, x + r * 1.9f, top + r * 0.35f, x, y + r * 2.0f, col);
}

// 涙（水色）: 画面の左の目の下に1つ
void tear(float eyeX, float eyeY) {
  const uint16_t col = rgb(COL_TEAR);
  const float rr = TEAR_R;
  const float tipY = eyeY + EYE_R * PART_SCALE + TEAR_GAP;
  const float cy = tipY + rr * 2.2f;
  circle(eyeX, cy, rr, col);
  tri(eyeX, tipY, eyeX - rr * 0.97f, cy - rr * 0.25f, eyeX + rr * 0.97f, cy - rr * 0.25f, col);
}

// Z を3本の線で描く（左上が (x, y)、大きさ s）
void zLetter(float x, float y, float s, float w, uint16_t col) {
  line(x, y, x + s, y, w, col);
  line(x + s, y, x, y + s, w, col);
  line(x, y + s, x + s, y + s, w, col);
}

// 寝ている時の Zzz。小さい Z から順に出て、少しずつ上へのぼる
void zzz(uint32_t ms) {
  const uint16_t col = rgb(COL_ZZZ);
  const float t = (ms % ZZZ_CYCLE_MS) / (float)ZZZ_CYCLE_MS;  // 0〜1
  for (int i = 0; i < 3; i++) {
    const float appear = i / 3.0f;
    if (t < appear) continue;
    const float rise = (t - appear) * ZZZ_RISE;
    zLetter(ZZZ_X[i], ZZZ_Y[i] - rise, ZZZ_S[i], ZZZ_W, col);
  }
}

}  // namespace

void drawFace(M5Canvas& g, int cx, int cy, const FaceState& s) {
  G = &g;
  const float rad = s.angleDeg * (float)M_PI / 180.0f;
  X = {cosf(rad), sinf(rad), FACE_SCALE, cx, cy};

  const uint16_t fg = rgb(COL_FACE);
  const uint16_t bg = TFT_BLACK;

  // 目（画面の左が isLeft=false。ライブラリと同じ）。横の位置は EYE_SPACING で中心へ寄せる
  auto sx = [](float x) { return FACE_CENTER_X + (x - FACE_CENTER_X) * EYE_SPACING; };
  const float rx = sx(EYE_R_X), ry = EYE_R_Y + EYE_DOWN;
  const float lx = sx(EYE_L_X), ly = EYE_L_Y + EYE_DOWN;
  eye(rx, ry, false, s, fg, bg);
  eye(lx, ly, true, s, fg, bg);

  // 口（M5Stack-Avatar の Mouth::draw と同じ。開くほど細く高くなる）
  const float open = s.sleeping ? 0 : (s.mouthOpen < 0 ? 0 : (s.mouthOpen > 1 ? 1 : s.mouthOpen));
  const float h = (MOUTH_MIN_H + (MOUTH_MAX_H - MOUTH_MIN_H) * open) * PART_SCALE;
  const float w = (MOUTH_MIN_W + (MOUTH_MAX_W - MOUTH_MIN_W) * (1 - open)) * PART_SCALE;
  rect(MOUTH_X - w / 2, MOUTH_Y - h / 2, w, h, fg);

  if (s.sleeping) {
    zzz(s.zzzMs);
    return;
  }
  if (s.exp == Expression::Happy) heart();
  if (s.exp == Expression::Angry) angerMark();
  if (s.exp == Expression::Sad) tear(rx, ry);
}

}  // namespace stackchan
