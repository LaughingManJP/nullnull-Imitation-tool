// null² のあとの花畑: 5つの場面を、本体の上で計算して描く(絵を持たないので軽い)
//   空のグラデーション・夕日・雲(値ノイズ)・海と光の道・鏡の箱(空を映す)・地面・草・花・ルピナス・手前のぼけた花・もや・光の筋。
//   描いた絵は、バイトを入れ替えた 565 で out に書く。鏡のゆらぎや花のそよぎに使う情報は MEADOW_FX[k] に書く
#pragma once
#include <stdint.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

struct MeadowFx { float hz, sunX, sunY; uint8_t sea; float rip; uint8_t nBox; float box[4][7]; uint16_t edge[466]; uint16_t rivA[466], rivB[466]; };   // rivA > rivB なら、その行に川なし
static constexpr int NSCENE = 6;
static MeadowFx* meadowFxAll() {                  // 場面ごとの情報(17KB あるので、内蔵メモリではなく PSRAM に置く)
  static MeadowFx* p = nullptr;
  if (!p) { p = (MeadowFx*)ps_malloc(sizeof(MeadowFx) * NSCENE); if (!p) p = (MeadowFx*)malloc(sizeof(MeadowFx) * NSCENE); if (p) memset(p, 0, sizeof(MeadowFx) * NSCENE); }
  return p;
}
#define MEADOW_FX (meadowFxAll())

namespace mpaint {
struct C3 { float r, g, b; };
struct Stop { float t; C3 c; };
struct Box { float xa, xc, xb, ta, tc, tb, bot, dark; };
struct Bk { float x, y, r; C3 c; };
struct Scene {
  float hz, sunX, sunY; C3 sun; float sunA, sunR, sunB; bool disc, stars;
  Stop sky[3];
  uint8_t clouds; float cy, cw; C3 cdark, clit; float cA;     // clouds: 0 = 横に流れるうす雲 / 1 = もこもこの雲
  bool sea; C3 sea0, sea1; float seaGlow; bool mount; C3 mountC; bool city;
  uint8_t nBox; Box box[4]; float rip, glassMix; C3 glass, edgeC;
  float ground, e1, a1, w1, e2, a2, w2;                      // 地面のふち: ground + e1 * sm((x - a1) / w1) - e2 * (1 - sm((x - a2) / w2))
  C3 gfar, gnear, grass, bush; uint8_t nPal; C3 pal[6]; C3 haze;
  float fl; int lupine; float mist, rays; bool cubes, pond, water;
  uint8_t nBk; Bk bk[4];
  uint32_t seed;
  bool river, trees; C3 lupC; uint8_t nFar; float far[8][3]; bool real;   // real: 写実的に描く(花の形・逆光・ぼかし・光のにじみ)   // 川 / 両わきの木 / ルピナスの色(0 = むらさき)/ 遠くのガラスの柱(x, 上のはし, 幅)
};
#define CC(r, g, b) C3{r, g, b}
static const Scene SC[NSCENE] = {
  // 1. 夕暮れの海辺と鏡の箱
  {0.585f, 0.74f, 0.573f, CC(1.0f, 0.70f, 0.40f), 0.55f, 0.10f, 0.30f, false, false,
   {{0, CC(0.40f, 0.44f, 0.70f)}, {0.55f, CC(0.78f, 0.62f, 0.80f)}, {1, CC(1.0f, 0.72f, 0.55f)}},
   0, 0, 0, CC(0.50f, 0.40f, 0.62f), CC(1.0f, 0.68f, 0.62f), 0.8f,
   true, CC(0.80f, 0.62f, 0.70f), CC(0.42f, 0.40f, 0.58f), 0.6f, false, CC(0, 0, 0), true,
   1, {{0.10f, 0.36f, 0.50f, 0.22f, 0.17f, 0.235f, 0.66f, 0.95f}}, 1.5f, 0.06f, CC(0.80f, 0.82f, 0.88f), CC(1.0f, 0.96f, 0.95f),
   0.615f, 0.07f, 0.45f, 0.45f, 0.05f, 0.0f, 0.40f,
   CC(0.36f, 0.33f, 0.40f), CC(0.10f, 0.13f, 0.09f), CC(0.25f, 0.37f, 0.22f), CC(0.29f, 0.33f, 0.33f),
   6, {CC(0.95f, 0.71f, 0.82f), CC(1.0f, 1.0f, 0.97f), CC(0.78f, 0.63f, 0.94f), CC(0.63f, 0.55f, 0.90f), CC(1.0f, 0.84f, 0.47f), CC(0.93f, 0.47f, 0.67f)}, CC(0.59f, 0.59f, 0.59f),
   1.0f, 26, 0, 0, false, false, false,
   4, {{0.12f, 0.93f, 0.075f, CC(0.93f, 0.51f, 0.75f)}, {0.93f, 0.86f, 0.056f, CC(1.0f, 0.99f, 0.94f)}, {0.30f, 1.0f, 0.059f, CC(0.82f, 0.59f, 0.90f)}, {0.70f, 0.98f, 0.052f, CC(0.94f, 0.47f, 0.67f)}},
   3},
  // 2. 朝もやの鏡の回廊
  {0.60f, 0.10f, 0.26f, CC(1.0f, 0.92f, 0.75f), 0.35f, 0.08f, 0.06f, false, false,
   {{0, CC(0.50f, 0.58f, 0.78f)}, {0.6f, CC(0.76f, 0.78f, 0.86f)}, {1, CC(0.90f, 0.88f, 0.84f)}},
   1, 0.22f, 0.20f, CC(0.70f, 0.72f, 0.84f), CC(1.0f, 0.97f, 0.92f), 0.7f,
   false, CC(0, 0, 0), CC(0, 0, 0), 0, false, CC(0, 0, 0), false,
   4, {{0.30f, 0.40f, 0.46f, 0.20f, 0.10f, 0.16f, 0.70f, 1}, {0.52f, 0.60f, 0.74f, 0.08f, 0.02f, 0.10f, 0.66f, 1}, {0.80f, 0.86f, 0.98f, 0.04f, 0.0f, 0.07f, 0.72f, 1}, {0.14f, 0.19f, 0.27f, 0.36f, 0.33f, 0.36f, 0.64f, 0.95f}},
   0.93f, 0.15f, CC(0.80f, 0.83f, 0.90f), CC(0.95f, 0.95f, 0.98f),
   0.60f, 0, 0, 1, 0, 0, 1,
   CC(0.70f, 0.72f, 0.74f), CC(0.32f, 0.38f, 0.28f), CC(0.35f, 0.47f, 0.31f), CC(0.59f, 0.63f, 0.63f),
   5, {CC(0.98f, 0.98f, 1.0f), CC(0.63f, 0.71f, 0.94f), CC(0.82f, 0.75f, 0.94f), CC(0.96f, 0.78f, 0.86f), CC(1.0f, 0.92f, 0.63f)}, CC(0.86f, 0.87f, 0.90f),
   1.0f, 20, 0.30f, 0.10f, false, false, false,
   3, {{0.08f, 0.95f, 0.071f, CC(0.59f, 0.67f, 0.94f)}, {0.90f, 0.92f, 0.060f, CC(0.98f, 0.98f, 0.98f)}, {0.62f, 1.02f, 0.054f, CC(0.94f, 0.75f, 0.84f)}},
   102},
  // 3. むらさきの夕凪
  {0.60f, 0.88f, 0.585f, CC(1.0f, 0.70f, 0.45f), 0.8f, 0.06f, 0.30f, true, false,
   {{0, CC(0.36f, 0.30f, 0.68f)}, {0.55f, CC(0.66f, 0.48f, 0.82f)}, {1, CC(1.0f, 0.66f, 0.60f)}},
   1, 0.34f, 0.26f, CC(0.42f, 0.32f, 0.62f), CC(1.0f, 0.70f, 0.70f), 0.85f,
   true, CC(0.80f, 0.62f, 0.80f), CC(0.40f, 0.34f, 0.60f), 0.7f, false, CC(0, 0, 0), false,
   1, {{0.06f, 0.12f, 0.42f, 0.24f, 0.20f, 0.25f, 0.74f, 0.92f}}, 0.93f, 0.05f, CC(0.80f, 0.82f, 0.88f), CC(1.0f, 0.96f, 0.95f),
   0.63f, 0.05f, 0.55f, 0.40f, 0, 0, 1,
   CC(0.30f, 0.24f, 0.36f), CC(0.12f, 0.10f, 0.14f), CC(0.24f, 0.27f, 0.24f), CC(0.24f, 0.20f, 0.31f),
   5, {CC(0.98f, 0.96f, 1.0f), CC(0.78f, 0.59f, 0.94f), CC(0.59f, 0.43f, 0.86f), CC(0.94f, 0.63f, 0.82f), CC(0.55f, 0.16f, 0.27f)}, CC(0.70f, 0.55f, 0.78f),
   1.0f, 60, 0, 0, false, false, false,
   2, {{0.12f, 0.96f, 0.064f, CC(0.82f, 0.55f, 0.90f)}, {0.85f, 0.97f, 0.058f, CC(0.98f, 0.94f, 0.98f)}},
   103},
  // 4. 水彩の花野
  {0.52f, 0.28f, 0.505f, CC(1.0f, 0.86f, 0.60f), 0.6f, 0.04f, 0.12f, true, false,
   {{0, CC(0.66f, 0.66f, 0.82f)}, {0.6f, CC(0.92f, 0.78f, 0.74f)}, {1, CC(0.98f, 0.80f, 0.62f)}},
   0, 0, 0, CC(0.62f, 0.60f, 0.80f), CC(1.0f, 0.80f, 0.72f), 0.75f,
   true, CC(0.80f, 0.74f, 0.80f), CC(0.62f, 0.64f, 0.76f), 0.5f, true, CC(0.55f, 0.52f, 0.70f), false,
   1, {{0.56f, 0.62f, 0.78f, 0.28f, 0.25f, 0.28f, 0.62f, 0.95f}}, 0.93f, 0.2f, CC(0.80f, 0.82f, 0.88f), CC(1.0f, 0.96f, 0.95f),
   0.56f, 0, 0, 1, 0, 0, 1,
   CC(0.72f, 0.68f, 0.56f), CC(0.46f, 0.48f, 0.32f), CC(0.43f, 0.47f, 0.27f), CC(0.59f, 0.55f, 0.43f),
   5, {CC(0.98f, 0.96f, 0.94f), CC(0.94f, 0.67f, 0.75f), CC(0.90f, 0.47f, 0.59f), CC(0.75f, 0.59f, 0.86f), CC(0.98f, 0.86f, 0.47f)}, CC(0.90f, 0.84f, 0.76f),
   1.2f, 12, 0, 0, true, true, false,
   0, {},
   104},
  // 5. 波打つ鏡と青い時間
  {0.56f, 0.10f, 0.55f, CC(1.0f, 0.72f, 0.62f), 0.25f, 0.10f, 0.30f, false, true,
   {{0, CC(0.30f, 0.38f, 0.62f)}, {0.6f, CC(0.58f, 0.60f, 0.80f)}, {1, CC(0.96f, 0.74f, 0.74f)}},
   0, 0, 0, CC(0.30f, 0.32f, 0.50f), CC(0.95f, 0.68f, 0.72f), 0.8f,
   true, CC(0.70f, 0.66f, 0.80f), CC(0.38f, 0.40f, 0.58f), 0.3f, false, CC(0, 0, 0), false,
   2, {{0.38f, 0.40f, 0.86f, 0.26f, 0.24f, 0.10f, 0.70f, 0.9f}, {0.86f, 0.88f, 1.05f, 0.10f, 0.09f, 0.14f, 0.62f, 0.8f}}, 2.8f, 0.04f, CC(0.80f, 0.82f, 0.88f), CC(0.9f, 0.9f, 0.95f),
   0.62f, 0, 0, 1, 0.04f, 0.0f, 0.35f,
   CC(0.24f, 0.26f, 0.32f), CC(0.08f, 0.10f, 0.10f), CC(0.20f, 0.27f, 0.22f), CC(0.20f, 0.24f, 0.27f),
   5, {CC(0.98f, 0.96f, 0.98f), CC(0.90f, 0.47f, 0.75f), CC(0.78f, 0.55f, 0.90f), CC(0.47f, 0.12f, 0.27f), CC(0.59f, 0.47f, 0.86f)}, CC(0.55f, 0.55f, 0.72f),
   1.0f, 10, 0, 0, false, false, false,
   4, {{0.90f, 0.72f, 0.118f, CC(0.98f, 0.98f, 0.92f)}, {0.40f, 0.86f, 0.075f, CC(0.90f, 0.35f, 0.67f)}, {0.08f, 0.80f, 0.064f, CC(0.82f, 0.67f, 0.90f)}, {0.66f, 1.0f, 0.069f, CC(0.86f, 0.43f, 0.71f)}},
   105},
  // 6. 光の川とガラスの柱(朝日が花畑の小川に映り、透きとおったガラスの柱が立ち並ぶ)
  {0.53f, 0.55f, 0.50f, CC(1.0f, 0.80f, 0.48f), 0.55f, 0.05f, 0.16f, true, false,
   {{0, CC(0.42f, 0.60f, 0.84f)}, {0.55f, CC(0.82f, 0.78f, 0.78f)}, {1, CC(0.98f, 0.80f, 0.58f)}},
   1, 0.26f, 0.22f, CC(0.74f, 0.58f, 0.62f), CC(1.0f, 0.82f, 0.70f), 0.85f,
   false, CC(0, 0, 0), CC(0, 0, 0), 0, true, CC(0.80f, 0.70f, 0.72f), false,
   2, {{0.24f, 0.27f, 0.36f, 0.05f, 0.03f, 0.05f, 0.56f, 1.0f}, {0.78f, 0.86f, 0.89f, 0.06f, 0.06f, 0.08f, 0.53f, 1.0f}}, 0.93f, 0.18f, CC(0.80f, 0.84f, 0.90f), CC(1.0f, 0.98f, 0.96f),
   0.55f, 0, 0, 1, 0, 0, 1,
   CC(0.55f, 0.55f, 0.40f), CC(0.10f, 0.20f, 0.08f), CC(0.22f, 0.42f, 0.18f), CC(0.25f, 0.33f, 0.22f),
   6, {CC(1.0f, 0.45f, 0.16f), CC(0.98f, 0.20f, 0.18f), CC(1.0f, 0.62f, 0.75f), CC(1.0f, 1.0f, 0.97f), CC(1.0f, 0.80f, 0.20f), CC(0.40f, 0.52f, 0.95f)}, CC(0.98f, 0.86f, 0.70f),
   0.9f, 40, 0.10f, 0.03f, false, false, false,
   4, {{0.90f, 0.74f, 0.085f, CC(1.0f, 0.38f, 0.20f)}, {0.05f, 0.88f, 0.075f, CC(1.0f, 0.92f, 0.94f)}, {0.12f, 0.60f, 0.055f, CC(0.98f, 0.80f, 0.86f)}, {0.70f, 0.94f, 0.05f, CC(1.0f, 0.98f, 0.96f)}},
   106, true, true, CC(0.30f, 0.42f, 0.92f), 7, {{0.10f, 0.13f, 0.05f}, {0.38f, 0.28f, 0.035f}, {0.42f, 0.31f, 0.03f}, {0.65f, 0.28f, 0.035f}, {0.70f, 0.33f, 0.03f}, {0.74f, 0.30f, 0.05f}, {0.48f, 0.38f, 0.02f}}, true},
};
#undef CC

// ---- 小さな道具
static inline float pfloor(float x) { const int i = (int)x; return (float)(i - (x < (float)i ? 1 : 0)); }
static inline float pexp(float x) {               // exp の速い版(相対誤差 1e-4 くらい)
  if (x < -80.0f) return 0.0f; if (x > 80.0f) x = 80.0f;
  const float t = x * 1.44269504f, fi = pfloor(t), f = t - fi;
  const float p = 1.0f + f * (0.69314718f + f * (0.24022650f + f * (0.05550411f + f * 0.00961813f)));
  union { float v; int32_t i; } u; u.v = p; u.i += (int32_t)fi * (1 << 23); return u.v;
}
static inline float psin(float x) {               // sin の速い版
  x *= 0.15915494f; x -= pfloor(x + 0.5f); x *= 6.2831853f;
  const float y = x * (1.27323954f - 0.40528473f * fabsf(x));
  return y * (0.775f + 0.225f * fabsf(y));
}
static inline float patan2(float y, float x) {    // atan2 の速い版
  const float ax = fabsf(x), ay = fabsf(y), mx = ax > ay ? ax : ay, mn = ax > ay ? ay : ax;
  if (mx <= 0.0f) return 0.0f;
  const float a = mn / mx, s2 = a * a; float r = ((-0.0464964749f * s2 + 0.15931422f) * s2 - 0.327622764f) * s2 * a + a;
  if (ay > ax) r = 1.57079637f - r; if (x < 0) r = 3.14159274f - r; return y < 0 ? -r : r;
}
static void (*onProg)(int) = nullptr;             // 進み具合(0..100)を知らせる先
// 行ごとの計算を2つのコアで分ける(runPar はこの絵を使う作品が入れる。なければ1つのコアで)
typedef void (*RowFn)(void* ctx, int y0, int y1, int st, bool mainCore);
static void (*runPar)(RowFn fn, void* ctx, int a, int b) = nullptr;
template <class F> static void parRows(int a, int b, F& f) {
  RowFn tr = [](void* c, int y0, int y1, int st, bool m) { (*(F*)c)(y0, y1, st, m); };
  if (runPar && b - a >= 4) runPar(tr, &f, a, b); else f(a, b, 1, true);
}
static int progBase = 0, progSpan = 100, progLast = -1;
static inline void prog(float f) {
  if (!onProg) return;
  const int p = progBase + (int)(f * progSpan);
  if (p != progLast) { progLast = p; onProg(p); }
}
static uint32_t rs = 1;
static inline uint32_t rnd() { rs ^= rs << 13; rs ^= rs >> 17; rs ^= rs << 5; return rs; }
static inline float rf() { return (rnd() & 0xFFFFFF) / 16777216.0f; }
static inline float rf(float a, float b) { return a + (b - a) * rf(); }
static inline float sm(float v) { v = v < 0 ? 0 : v > 1 ? 1 : v; return v * v * (3 - 2 * v); }
static inline float cl01(float v) { return v < 0 ? 0 : v > 1 ? 1 : v; }
static inline float hsh(uint32_t s, int x, int y) {
  uint32_t h = s * 0x9E3779B1u ^ (uint32_t)x * 0x85EBCA77u ^ (uint32_t)y * 0xC2B2AE3Du;
  h ^= h >> 15; h *= 0x2C1B3C6Du; h ^= h >> 12; h *= 0x297A2D39u; h ^= h >> 15;
  return (h & 0xFFFF) / 65535.0f;
}
static inline float vn(uint32_t s, float x, float y) {     // 値ノイズ(なめらか)
  const int ix = (int)pfloor(x), iy = (int)pfloor(y); const float fx = x - ix, fy = y - iy;
  const float u = fx * fx * (3 - 2 * fx), v = fy * fy * (3 - 2 * fy);
  const float a = hsh(s, ix, iy), b = hsh(s, ix + 1, iy), c = hsh(s, ix, iy + 1), d = hsh(s, ix + 1, iy + 1);
  return (a + (b - a) * u) + ((c + (d - c) * u) - (a + (b - a) * u)) * v;
}
static float fbm(uint32_t s, float u, float v, float gx, float gy, int oct, float gain) {   // u, v: 0..1
  float out = 0, amp = 1, tot = 0;
  for (int o = 0; o < oct; ++o) { out += vn(s + o * 101, u * gx, v * gy) * amp; tot += amp; amp *= gain; gx *= 2; gy *= 2; }
  return out / tot;
}
static inline C3 mix(C3 a, C3 b, float t) { return {a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t}; }
static inline C3 mul(C3 a, float k) { return {a.r * k, a.g * k, a.b * k}; }
static inline C3 add(C3 a, C3 b) { return {a.r + b.r, a.g + b.g, a.b + b.b}; }

static int N = 0;
static float* P = nullptr;                       // [N * N * 3] 色
static uint16_t* FULL = nullptr;                 // 空と海だけの絵(鏡に映すため)565
static const float* clipEdge = nullptr;           // あれば、地面のふちより下には描かない
static inline void put(int x, int y, C3 c, float a) {
  if ((unsigned)x >= (unsigned)N || (unsigned)y >= (unsigned)N || a <= 0) return;
  if (clipEdge && (y + 0.5f) > clipEdge[x] * N) return;
  float* p = P + ((size_t)y * N + x) * 3;
  p[0] += (c.r - p[0]) * a; p[1] += (c.g - p[1]) * a; p[2] += (c.b - p[2]) * a;
}
static void ellipse(float cx, float cy, float rx, float ry, C3 c, float a, float soft = 0) {
  if (rx < 0.3f) rx = 0.3f; if (ry < 0.3f) ry = 0.3f;
  const int x0 = (int)pfloor(cx - rx - 1), x1 = (int)ceilf(cx + rx + 1), y0 = (int)pfloor(cy - ry - 1), y1 = (int)ceilf(cy + ry + 1);
  for (int y = y0; y <= y1; ++y)
    for (int x = x0; x <= x1; ++x) {
      const float dx = (x + 0.5f - cx) / rx, dy = (y + 0.5f - cy) / ry, d = sqrtf(dx * dx + dy * dy);
      if (soft > 0) { if (d < 1) put(x, y, c, a * sm((1 - d) / soft)); }
      else { const float e = cl01((1 - d) * fminf(rx, ry) + 0.5f); if (e > 0) put(x, y, c, a * e); }
    }
}
static void line(float x0, float y0, float x1, float y1, C3 c, float a, int w) {
  const int n = (int)fmaxf(fabsf(x1 - x0), fabsf(y1 - y0)) + 1;
  for (int i = 0; i <= n; ++i) {
    const float t = i / (float)n; const int x = (int)(x0 + (x1 - x0) * t), y = (int)(y0 + (y1 - y0) * t);
    for (int k = 0; k < w; ++k) put(x + k, y, c, a);
  }
}
static void flower(float cx, float cy, float r, C3 c, float a = 0.92f, int n = 8) {
  for (int k = 0; k < n; ++k) {
    const float an = k * 6.2832f / n + rf() * 6.2832f;
    ellipse(cx + psin(an + 1.5707963f) * r * 0.62f, cy + psin(an) * r * 0.42f, r * 0.42f, r * 0.28f, c, a);
  }
  ellipse(cx, cy, r * 0.2f, r * 0.14f, {0.92f, 0.76f, 0.31f}, a);
}

static inline void riverAt(float gt, float yy, float& cx, float& hw) {
  const float s0 = cl01((yy - gt) / (1 - gt)); cx = 0.53f + 0.03f * psin(yy * 18.0f) - 0.04f * s0; hw = 0.006f + 0.13f * powf(s0, 1.3f);
}
static inline bool inRiver(const Scene& Q, float gt, float xx, float yy, float pad = 0) {
  if (!Q.river || yy < gt) return false;
  float cx, hw; riverAt(gt, yy, cx, hw);
  return fabsf(xx - cx) < hw + pad;
}
// 写実的な花: 花びらは付け根が濃く先が明るい。形は種類で変える(0 コスモス / 1 デイジー / 2 小花)
static void flowerR(float cx, float cy, float r, C3 c, int kind, C3 sun, float lit) {
  const int np = kind == 1 ? 13 : (kind == 0 ? 8 : 5);
  const float tilt = 0.45f + 0.35f * rf(), a0 = rf() * 6.2832f;
  const float pl = kind == 1 ? 0.95f : 0.80f, pw = kind == 1 ? 0.16f : (kind == 0 ? 0.34f : 0.40f);
  for (int k = 0; k < np; ++k) {
    const float an = a0 + k * 6.2832f / np + rf(-0.08f, 0.08f), ca = psin(an + 1.5707963f), sa = psin(an) * tilt;
    for (int j = 0; j < 4; ++j) {                    // 付け根から先へ、4つの楕円で
      const float t = (j + 0.5f) / 4.0f, rr = r * pl * t;
      C3 pc = mix(mul(c, 0.62f), add(c, mul(sun, 0.25f * lit)), t);
      ellipse(cx + ca * rr, cy + sa * rr, r * pw * (0.7f + 0.4f * t), r * pw * (0.55f + 0.3f * t) * (0.5f + 0.5f * tilt), pc, 0.95f);
    }
  }
  const C3 ctr = kind == 1 ? C3{0.95f, 0.78f, 0.18f} : (kind == 0 ? C3{0.85f, 0.62f, 0.15f} : C3{0.95f, 0.85f, 0.4f});
  ellipse(cx, cy, r * 0.22f, r * 0.22f * tilt + 0.5f, mul(ctr, 0.75f), 1.0f);
  ellipse(cx - r * 0.05f, cy - r * 0.05f * tilt, r * 0.12f, r * 0.12f * tilt + 0.4f, ctr, 1.0f);
}
// 花の模様(つなぎ目なし、TT x TT 点)を1枚作る: 地面の色・草・花。地面には、これを遠近をつけて並べる
static constexpr int TT = 128;
static float* TILE = nullptr;
static float* P2 = nullptr;
static void buildTile(const Scene& Q) {
  float* keepP = P; const int keepN = N; P = TILE; N = TT;
  for (int y = 0; y < TT; ++y) for (int x = 0; x < TT; ++x) {
    const float v = 0.75f + 0.5f * hsh(Q.seed + 40, x >> 1, y >> 1) * 0.6f;
    const C3 g = mul(mix(Q.gfar, Q.gnear, 0.7f), v); float* o = TILE + ((size_t)y * TT + x) * 3; o[0] = g.r; o[1] = g.g; o[2] = g.b;
  }
  auto wrap = [&](float cx, float cy, float r, auto&& draw) {   // はしにかかる物は、反対側にも描く(つなぎ目なし)
    for (int oy = -1; oy <= 1; ++oy) for (int ox = -1; ox <= 1; ++ox) {
      const float x = cx + ox * TT, y = cy + oy * TT;
      if (x + r + 2 < 0 || x - r - 2 > TT || y + r + 2 < 0 || y - r - 2 > TT) continue;
      draw(x, y);
    }
  };
  for (int i = 0; i < 700; ++i) {                  // 草
    const float x = rf(0, TT), y = rf(0, TT), h = rf(2.5f, 6.5f), lean = rf(-1.5f, 1.5f);
    const C3 gc = mul(Q.grass, 0.6f + 0.7f * rf());
    wrap(x, y, h, [&](float px, float py) { line(px, py, px + lean, py - h, gc, 0.7f, 1); });
  }
  const int nf = (int)(420 * Q.fl);                // 花
  for (int i = 0; i < nf; ++i) {
    const float x = rf(0, TT), y = rf(0, TT), r = rf(2.2f, 3.8f);
    const C3 c = Q.pal[rnd() % Q.nPal];
    wrap(x, y, r, [&](float px, float py) { flower(px, py, r, c, 1.0f, 6); });
  }
  P = keepP; N = keepN;
}
// 場面 k を描く。outNo = 鏡の箱のない景色、outBox = 場面の絵(バイトを入れ替えた 565 [n * n])
static bool paintBoth(int k, uint16_t* outNo, uint16_t* outBox, int n) {
  const Scene& Q = SC[k];
  const bool RL = true;                            // 実写ふう(すべての場面)
  if (N != n) { free(P); free(P2); free(FULL); P = P2 = nullptr; FULL = nullptr; N = n; }
  if (!P) P = (float*)ps_malloc((size_t)n * n * 3 * sizeof(float));
  if (!P2) P2 = (float*)ps_malloc((size_t)n * n * 3 * sizeof(float));   // 箱を描く前の絵(箱ありの絵は、ここから作る)
  if (!FULL) FULL = (uint16_t*)ps_malloc((size_t)n * n * 2);
  if (!TILE) TILE = (float*)ps_malloc((size_t)TT * TT * 3 * sizeof(float));
  if (!P || !P2 || !FULL || !TILE) return false;
  rs = Q.seed * 2654435761u + 1;
  const float inv = 1.0f / n;
  float* edge = (float*)malloc(n * sizeof(float));
  if (!edge) return false;
  // ---- 空・雲・海(空を先に、海はその映りを読むのであと。どちらも2つのコアで1行おき)
  auto skyRows = [&](int ya, int yb, int st, bool mainCore) {
  for (int jy = ya; jy < yb; jy += st) {
    const int y = 2 * jy; if (y >= n) break;
    const float Y = (y + 0.5f) * inv;
    if (mainCore && (jy & 3) < st) prog(0.35f * y / n);
    for (int x = 0; x < n; x += 2) {
      const float X = (x + 0.5f) * inv;
      C3 c;
      const float t = cl01(Y / Q.hz);
      c = t < Q.sky[1].t ? mix(Q.sky[0].c, Q.sky[1].c, t / Q.sky[1].t) : mix(Q.sky[1].c, Q.sky[2].c, (t - Q.sky[1].t) / (1 - Q.sky[1].t));
      const float dx = X - Q.sunX, dy = (Y - Q.sunY) * 1.3f, d = sqrtf(dx * dx + dy * dy);
      c = add(c, mul(Q.sun, Q.sunA * pexp(-(d / Q.sunR) * (d / Q.sunR)) + Q.sunB * pexp(-(d / 0.38f) * (d / 0.38f))));
      if (Q.disc && d < 0.018f) c = {1.0f, 0.97f, 0.88f};
      if (Y < Q.hz) {                              // 雲
        float cl, shade;
        if (Q.clouds == 0) {
          const float c1 = fbm(Q.seed + 1, X, Y, 3, 12, 5, 0.55f), c2 = fbm(Q.seed + 2, X, Y, 3, 20, 5, 0.55f);
          const float band = cl01(sm(1 - fabsf(Y - 0.24f) / 0.20f) + 0.7f * sm(1 - fabsf(Y - 0.44f) / 0.08f));
          cl = sm((c1 * 0.8f + c2 * 0.5f - 0.66f) / 0.08f) * band; shade = sm((c1 - c2 * 0.3f - 0.35f) / 0.3f);
        } else {
          const float c1 = fbm(Q.seed + 1, X, Y, 5, 4, 6, 0.55f), c2 = fbm(Q.seed + 3, X, Y, 9, 7, 5, 0.5f);
          const float band = sm(1 - fabsf(Y - Q.cy) / Q.cw);
          cl = sm((c1 * 0.75f + c2 * 0.35f - 0.60f) / 0.04f) * band;
          if (Q.real) cl *= 0.25f + 0.9f * sm((X - 0.45f) / 0.25f);   // 雲は右上に寄せる
          const float up = fbm(Q.seed + 1, X, Y - 0.01f, 5, 4, 6, 0.55f) * 0.75f + c2 * 0.35f - 0.60f;
          shade = sm(0.5f + ((c1 * 0.75f + c2 * 0.35f - 0.60f) - up) * 12.0f);   // 下の方ほど明るい
        }
        if (cl > 0.001f) {
          const C3 cc = mul(mix(Q.cdark, Q.clit, sm(1 - d / 0.6f)), 0.8f + 0.4f * shade);
          c = mix(c, cc, cl * Q.cA);
          if (RL) c = add(c, mul(Q.sun, cl * (1 - cl) * 4.0f * pexp(-d / 0.22f) * 0.55f));   // 日の側の雲のふちが光る
        }
        if (Q.stars && Y < Q.hz * 0.6f && hsh(Q.seed + 9, x, y) > 0.9985f) c = add(c, mul({1, 1, 1}, 0.5f * (1 - Y / Q.hz)));
        if (Q.mount) {                             // 遠くの山
          const float mt = Q.hz - 0.03f * fbm(Q.seed + 7, X, 0, 6, 1, 4, 0.5f) * sm((X - 0.45f) / 0.2f);
          if (Y > mt) c = mix(c, Q.mountC, 0.7f);
        }
        if (Q.city && X > 0.48f && X < 0.62f && Y > Q.hz - 0.02f * hsh(Q.seed + 11, (int)(X * 120), 0)) c = mix(c, {0.55f, 0.45f, 0.58f}, 0.25f);
      } else if (Q.sea) {                          // 海と光の道
        c = mix(Q.sea0, Q.sea1, sm((Y - Q.hz) / 0.12f));
        const float w = 0.02f + 0.3f * (Y - Q.hz), p = (X - Q.sunX) / w;
        const float gl = fbm(Q.seed + 5, X, Y, 40, 120, 3, 0.5f);
        c = add(c, mul(Q.sun, pexp(-p * p) * (0.25f + 0.9f * (gl > 0.58f)) * Q.seaGlow));
        if (RL) {                                    // 遠くほど空を強く映す(フレネル)。横に流れる波
          const float st = cl01((Y - Q.hz) / (1 - Q.hz)), fr = 0.25f + 0.75f * pexp(-st * 6.0f);
          const uint16_t v = FULL[(size_t)max(0, min(n - 1, (int)((2 * Q.hz - Y) * n))) * n + x];
          const C3 rc = {(v >> 11) / 31.0f, ((v >> 5) & 63) / 63.0f, (v & 31) / 31.0f};
          const float wv = fbm(Q.seed + 31, X, Y, 3, 60, 3, 0.5f) - 0.5f;
          c = add(mul(mix(mul(Q.sea1, 0.6f), mul(rc, 0.85f), fr), 0.92f + 0.3f * wv), mul(Q.sun, pexp(-p * p) * (0.2f + 1.3f * (gl + wv * 0.6f > 0.62f)) * Q.seaGlow));
        }
      }
      float* o = P + ((size_t)y * n + x) * 3; o[0] = c.r; o[1] = c.g; o[2] = c.b;
      FULL[(size_t)y * n + x] = (uint16_t)(((int)(cl01(c.r) * 31) << 11) | ((int)(cl01(c.g) * 63) << 5) | (int)(cl01(c.b) * 31));
    }
  }
  };
  auto fillSky = [&](int y0, int y1) {              // 1点おきに計算した空の、間をまぜて埋める
    for (int y = y0; y < y1; y += 2) { float* r = P + (size_t)y * n * 3; for (int x = 1; x < n; x += 2) for (int ch = 0; ch < 3; ++ch) r[x * 3 + ch] = x + 1 < n ? (r[(x - 1) * 3 + ch] + r[(x + 1) * 3 + ch]) * 0.5f : r[(x - 1) * 3 + ch]; }
    for (int y = y0 + 1; y < y1; y += 2) { float* r = P + (size_t)y * n * 3; const float* p0 = r - n * 3; const float* p1 = y + 1 < y1 ? r + n * 3 : p0; for (int i = 0; i < n * 3; ++i) r[i] = (p0[i] + p1[i]) * 0.5f; }
    for (int y = y0; y < y1; ++y) for (int x = 0; x < n; ++x) { const float* o = P + ((size_t)y * n + x) * 3; FULL[(size_t)y * n + x] = (uint16_t)(((int)(cl01(o[0]) * 31) << 11) | ((int)(cl01(o[1]) * 63) << 5) | (int)(cl01(o[2]) * 31)); }
  };
  { const int hzr = max(0, min(n, (int)ceilf(Q.hz * n - 0.5f))), ja = (hzr + 1) / 2, yb = min(n, 2 * ja);
    parRows(0, ja, skyRows); fillSky(0, yb); parRows(ja, (n + 1) / 2, skyRows); fillSky(yb, n); }
  // ---- 遠くのガラスの柱(すきとおって、もやにかすむ)と、両わきの木
  for (int i = 0; i < Q.nFar; ++i) {
    const float fx = Q.far[i][0], ft = Q.far[i][1], fw = Q.far[i][2];
    for (int y = (int)(ft * n); y < (int)(Q.hz * n); ++y)
      for (int x = (int)(fx * n); x < (int)((fx + fw) * n); ++x) {
        const float u = (x - fx * n) / (fw * n);
        const C3 g = u < 0.6f ? C3{0.92f, 0.94f, 0.98f} : C3{0.80f, 0.84f, 0.92f};
        put(x, y, g, 0.22f + 0.1f * (u > 0.6f));
      }
    line(fx * n, ft * n, fx * n, Q.hz * n, {1, 1, 1}, 0.55f, 1); line((fx + fw) * n, ft * n, (fx + fw) * n, Q.hz * n, {1, 1, 1}, 0.4f, 1); line((fx + fw * 0.6f) * n, ft * n, (fx + fw * 0.6f) * n, Q.hz * n, {1, 1, 1}, 0.3f, 1);
    line(fx * n, ft * n, (fx + fw) * n, ft * n, {1, 1, 1}, 0.35f, 1);
  }
  if (Q.trees) for (int i = 0; i < 2600; ++i) {    // 両わきの木(もやに沈んだ緑)
    const bool L = i & 1; const float xx = L ? rf(-0.05f, 0.24f) : rf(0.76f, 1.05f);
    const float top = Q.hz - (L ? 0.17f : 0.15f) * (0.4f + 0.6f * sm(L ? (0.24f - xx) / 0.2f : (xx - 0.76f) / 0.2f)) * rf(0.6f, 1.0f);
    const float yy = rf(top, Q.hz + 0.02f);
    ellipse(xx * n + rf(-6, 6), yy * n, rf(1.2f, 3.2f) * n / 466.0f, rf(1.0f, 2.6f) * n / 466.0f, mix(mul({0.12f, 0.20f, 0.15f}, rf(0.7f, 1.4f)), Q.haze, 0.12f + 0.2f * (yy - top) / 0.2f), 0.9f);
  }
  // ---- 地面
  float gt = 0;
  for (int x = 0; x < n; ++x) {
    const float X = (x + 0.5f) * inv;
    edge[x] = Q.ground + Q.e1 * sm((X - Q.a1) / Q.w1) - Q.e2 * (1 - sm((X - Q.a2) / Q.w2)) + 0.015f * (fbm(Q.seed + 11, X, 0, 12, 1, 4, 0.5f) - 0.5f);
    gt += edge[x];
  }
  gt /= n;
  float emin = 1; for (int x = 0; x < n; ++x) emin = fminf(emin, edge[x]);
  buildTile(Q);                                    // 花の模様(1枚)
  const float H0 = emin * 466.0f - 1.0f, TS = TT / 192.0f;   // 遠近の基準(466 の画面で)
  prog(0.40f);
  auto groundRows = [&](int ya, int yb, int st, bool mainCore) {
  for (int y = ya; y < yb; y += st) {
    const float Y = (y + 0.5f) * inv;
    if (mainCore && (y & 7) < st) prog(0.40f + 0.20f * y / n);
    const float z = 60.0f / fmaxf(3.0f, Y * 466.0f - H0);   // 遠さ(上ほど遠い)
    const int ty = (((int)pfloor(z * 140.0f * TS)) % TT + TT) % TT;
    const float hzF = cl01((z - 0.25f) / 2.5f) * 0.75f;      // 遠いほどもや
    for (int x = 0; x < n; ++x) {
      if (Y <= edge[x]) continue;
      const float X = (x + 0.5f) * inv;
      const int tx = (((int)pfloor(((X * 466.0f - 233.0f) * z * 0.9f + 37.0f) * TS)) % TT + TT) % TT;
      const float* tp = TILE + ((size_t)ty * TT + tx) * 3;
      C3 c = mix(C3{tp[0], tp[1], tp[2]}, Q.haze, hzF);
      if (RL) c = mix(c, mul(c, 0.55f), sm((Y - 0.7f) / 0.3f));
      if (Q.river) {                               // 小川: まん中の奥から手前へ広がり、空と朝日を映す
        const float s = cl01((Y - gt) / (1 - gt)), cx = 0.53f + 0.03f * psin(Y * 18.0f) - 0.04f * s, hw = 0.006f + 0.13f * powf(s, 1.3f);
        const float e0 = fabsf(X - cx) / hw, e = e0 < 1.1f ? e0 + 0.15f * (fbm(Q.seed + 14, X, Y, 14, 14, 3, 0.5f) - 0.5f) : e0;
        if (e < 1.0f) {
          // 水: まん中は空(手前ほど高い空 = 青)と朝日を映し、岸に近いほど岸の草木が映って暗い緑になる
          const float sy = cl01(Q.hz - 0.02f - s * 0.45f), sx2 = cl01(0.5f + (X - cx) * 1.2f);
          const uint16_t v = FULL[(size_t)min(n - 1, (int)(sy * n)) * n + min(n - 1, (int)(sx2 * n))];
          C3 sk = {(v >> 11) / 31.0f, ((v >> 5) & 63) / 63.0f, (v & 31) / 31.0f};
          sk = mix(mul(sk, 0.75f), {0.20f, 0.34f, 0.58f}, 0.5f);
          const float eb = sm((e - 0.30f) / 0.55f);                 // 岸に近いほど 1
          const C3 bank = mul({0.07f, 0.13f, 0.10f}, 0.8f + 0.5f * fbm(Q.seed + 19, X, Y, 30, 8, 3, 0.5f));
          c = mix(sk, bank, eb * (0.55f + 0.35f * s));
          c = mul(c, 0.90f + 0.14f * psin(Y * 300.0f + 8.0f * fbm(Q.seed + 17, X, Y, 6, 30, 2, 0.5f)));   // さざ波
          const float core = pexp(-powf((X - cx) / (hw * 0.35f + 0.004f), 2));
          c = add(c, mul(Q.sun, core * (0.18f * (1 - s * 0.6f) + 0.5f * (fbm(Q.seed + 18, X, Y, 20, 90, 2, 0.5f) > 0.58f))));   // 朝日の光の道ときらめき
          c = add(c, mul(Q.sun, 0.5f * pexp(-(Y - Q.hz) / 0.03f) * (1 - eb)));   // 奥ほど朝日を映して明るい
          if (hsh(Q.seed + 16, x / 4, y / 2) > 0.978f && eb > 0.2f) c = {0.18f, 0.30f, 0.16f};   // 浮き葉(岸寄り)
        }
      }
      if (Q.pond) {                                // 池(空を映す)
        const float px = (X - 0.78f) / 0.16f, py = (Y - 0.84f) / 0.07f;
        const float pr = px * px + py * py;
        if (pr < 1.13f && pr < 1 + 0.25f * (fbm(Q.seed + 13, X, Y, 10, 10, 3, 0.5f) - 0.5f)) {
          const uint16_t v = FULL[(size_t)min(n - 1, (int)(cl01(2 * 0.84f - Y - 0.25f) * Q.hz * n)) * n + x];
          c = {(v >> 11) / 31.0f * 0.8f + 0.05f, ((v >> 5) & 63) / 63.0f * 0.8f + 0.05f, (v & 31) / 31.0f * 0.8f + 0.05f};
        }
      }
      float* o = P + ((size_t)y * n + x) * 3; o[0] = c.r; o[1] = c.g; o[2] = c.b;
    }
  }
  };
  parRows(0, n, groundRows);
  const float sc = n / 932.0f;                     // 元の絵(932)に対する大きさ
  if (Q.real && Q.river) for (int i = 0; i < 900; ++i) {   // 川べりの草が、水の上にかぶさる
    const float yy = rf(gt + 0.01f, 1.0f), s = (yy - gt) / (1 - gt); float cx, hw; riverAt(gt, yy, cx, hw);
    const float sd = rf() < 0.5f ? -1.0f : 1.0f, xx = cx + sd * hw * rf(0.85f, 1.1f), h = (0.01f + 0.06f * s) * n;
    line(xx * n, yy * n, xx * n - sd * h * rf(0.2f, 0.6f), yy * n - h, mul(Q.grass, rf(0.5f, 0.9f)), 0.8f, max(1, (int)(1 + 2 * s)));
  }
  for (int i = 0; i < 500; ++i) {                  // 地平のふちの茂み
    const float xx = rf(); const float ey = edge[min(n - 1, (int)(xx * n))];
    const float X = xx * n, Yp = ey * n + rf(-1, 4) * sc * 2, h = rf(3, 14) * sc;
    line(X, Yp, X + rf(-1, 1), Yp - h, Q.bush, 0.75f, 1);
    if (rf() < 0.25f) ellipse(X, Yp - h * 0.6f, h * 0.5f, h * 0.3f, Q.bush, 0.75f);
  }
  if (Q.cubes) {                                   // 草原に散らばる小さなガラスの箱
    static const float CU[10][3] = {{0.20f, 0.64f, 0.025f}, {0.36f, 0.70f, 0.035f}, {0.48f, 0.66f, 0.02f}, {0.66f, 0.74f, 0.04f}, {0.84f, 0.66f, 0.025f}, {0.12f, 0.80f, 0.05f}, {0.56f, 0.86f, 0.06f}, {0.92f, 0.78f, 0.04f}, {0.30f, 0.60f, 0.015f}, {0.72f, 0.60f, 0.015f}};
    for (auto& q : CU) {
      const float w = q[2] * n, x0 = q[0] * n - w / 2, y0 = q[1] * n - w;
      for (int y = (int)(y0 - w * 0.2f); y < (int)(y0 + w); ++y)
        for (int x = (int)x0; x < (int)(x0 + w * 1.3f); ++x) {
          const bool front = x < x0 + w && y >= y0, side = x >= x0 + w && y >= y0 - (x - x0 - w) * 0.67f && y < y0 + w - (x - x0 - w) * 0.67f;
          const bool top = y < y0 && y >= y0 - w * 0.2f && x >= x0 + (y0 - y) * 1.5f && x < x0 + w + (y0 - y) * 1.5f;
          if (front) put(x, y, {0.78f, 0.82f, 0.92f}, 0.55f); else if (side) put(x, y, {0.70f, 0.72f, 0.82f}, 0.5f); else if (top) put(x, y, {0.95f, 0.95f, 0.98f}, 0.5f);
        }
    }
  }
  if (Q.real) for (int i = 0; i < 70; ++i) {      // 背の高い穂の花(金魚草ふう。桃と赤)
    const float yy = gt + 0.03f + (1 - gt - 0.15f) * powf(rf(), 1.4f), s = (yy - gt) / (1 - gt), xx = rf() < 0.5f ? rf(0.0f, 0.42f) : rf(0.62f, 1.0f);
    if (inRiver(Q, gt, xx, yy, 0.02f)) continue;
    const float h = (0.05f + 0.22f * s) * n, w = (1.2f + 4.0f * s) * sc * 2;
    const C3 pc = rf() < 0.6f ? C3{0.98f, 0.45f, 0.62f} : C3{0.95f, 0.22f, 0.30f};
    line(xx * n, yy * n, xx * n, yy * n - h, {0.18f, 0.32f, 0.16f}, 0.9f, max(1, (int)(w * 0.25f)));
    const int m = (int)(8 + 18 * s);
    for (int j = 0; j < m; ++j) {
      const float t = 0.35f + 0.65f * j / (float)m, ww = w * (1.1f - 0.6f * t);
      ellipse(xx * n + (j & 1 ? 1 : -1) * ww * 0.4f, yy * n - h * t, ww * 0.7f, ww * 0.55f, mul(mix(Q.haze, pc, 0.4f + 0.6f * s), 0.75f + 0.35f * t), 0.95f);
    }
  }
  for (int i = 0; i < Q.lupine; ++i) {             // ルピナスの穂
    const float yy = gt + 0.02f + (1 - gt - 0.05f) * powf(rf(), 1.3f), s = (yy - gt) / (1 - gt), xx = rf();
    if (inRiver(Q, gt, xx, yy)) continue;
    const float h = (0.02f + 0.12f * s) * n, w = (1 + 3.5f * s) * sc; const int m = (int)(10 + 20 * s);
    for (int j = 0; j < m; ++j) { const float t = j / (float)m; ellipse(xx * n, yy * n - h * t, w * (1 - 0.6f * t), w * 0.7f, Q.lupC.b > 0 ? mul(Q.lupC, rf(0.8f, 1.15f)) : C3{rf(0.47f, 0.63f), rf(0.35f, 0.47f), 0.78f}, 0.9f); }
  }
  if (Q.pond) for (int i = 0; i < 14; ++i) {       // 睡蓮
    const float a = rf() * 6.28f, rr = sqrtf(rf()), px = (0.78f + psin(a + 1.5707963f) * 0.16f * rr * 0.9f) * n, py = (0.84f + psin(a) * 0.07f * rr * 0.9f) * n;
    ellipse(px, py, 9 * sc, 3 * sc, {0.27f, 0.43f, 0.27f}, 0.86f);
    if (rf() < 0.5f) flower(px, py - 3 * sc, 5 * sc, {0.96f, 0.67f, 0.78f}, 0.9f, 6);
  }
  for (int i = 0; i < Q.nBk; ++i) {                // 手前の、大きくぼけた花
    const Bk& K = Q.bk[i]; const float r = K.r * n;
    if (Q.real) {                                    // 写実: くっきりした大きな花(茎と葉つき)
      line(K.x * n, K.y * n, K.x * n + r * 0.2f, n * 1.05f, {0.16f, 0.30f, 0.14f}, 0.95f, max(2, (int)(r * 0.08f)));
      flowerR(K.x * n, K.y * n, r, K.c, (K.c.r > 0.95f && K.c.g > 0.9f) ? 1 : 0, Q.sun, 0.6f);
      continue;
    }
    for (int j = 0; j < 8; ++j) { const float a = j * 0.785f + 0.3f; ellipse(K.x * n + psin(a + 1.5707963f) * r * 0.6f, K.y * n + psin(a) * r * 0.45f, r * 0.48f, r * 0.32f, K.c, 0.85f, 0.35f); }
    ellipse(K.x * n, K.y * n, r * 0.2f, r * 0.15f, {0.82f, 0.63f, 0.24f}, 0.9f, 0.4f);
  }
  float progA = 0;
  auto postAll = [&]() {
  // ---- もや・光の筋・水彩
  auto mistRows = [&](int ya, int yb, int st, bool mainCore) {
  for (int y = ya; y < yb; y += st) {
    const float Y = (y + 0.5f) * inv;
    if (mainCore && (y & 7) < st) prog(progA + 0.15f * y / n);
    for (int x = 0; x < n; ++x) {
      const float X = (x + 0.5f) * inv;
      float* o = P + ((size_t)y * n + x) * 3; C3 c = {o[0], o[1], o[2]};
      if (Q.mist > 0) { const float dd = (Y - Q.hz) / 0.16f, mm = 0.5f * Q.mist * pexp(-dd * dd) * 0.9f; c = mix(c, Q.haze, mm); }
      if (Q.rays > 0) {
        const float ang = patan2(Y - Q.sunY, X - Q.sunX), rr = sqrtf((X - Q.sunX) * (X - Q.sunX) + (Y - Q.sunY) * (Y - Q.sunY));
        const float s = 0.5f + 0.5f * psin(ang * 23 + 1.0f), s2 = s * s, s4 = s2 * s2;
        c = add(c, mul(Q.sun, s4 * s4 * s2 * pexp(-rr / 0.4f) * Q.rays));
      }
      if (RL) {                                    // 日のにじみ・まわりを少し暗く・コントラスト
        const float dx = X - Q.sunX, dy = (Y - Q.sunY) * 1.2f, d2 = dx * dx + dy * dy;
        c = add(c, mul(Q.sun, 0.22f * pexp(-d2 / 0.012f) + 0.06f * pexp(-d2 / 0.08f)));
        const float vx = X - 0.5f, vy = Y - 0.5f; c = mul(c, 1.0f - 0.35f * (vx * vx + vy * vy));
        c = {sm(c.r * 0.9f + 0.05f) * 0.55f + c.r * 0.45f, sm(c.g * 0.9f + 0.05f) * 0.55f + c.g * 0.45f, sm(c.b * 0.9f + 0.05f) * 0.55f + c.b * 0.45f};
      }
      if (Q.water) {                               // 水彩: 色を少しまとめて、紙の目
        c = {roundf(c.r * 10) * 0.05f + c.r * 0.5f, roundf(c.g * 10) * 0.05f + c.g * 0.5f, roundf(c.b * 10) * 0.05f + c.b * 0.5f};
        const float paper = hsh(Q.seed + 20, x, y) * 0.08f + 0.04f;     // 紙の目
        c = add(mul(add(c, mul({1, 1, 1}, 0.06f - paper * 0.5f)), 0.88f), mul({0.97f, 0.94f, 0.88f}, 0.12f));
      }
      o[0] = c.r; o[1] = c.g; o[2] = c.b;
    }
  }
  };
  parRows(0, n, mistRows);
  if (RL) {                                        // 光のにじみ: 明るい所を 1/4 に縮めてぼかし、足す
    const int m = n / 4; float* bl = (float*)malloc((size_t)m * m * 3 * sizeof(float)); float* tmp = (float*)malloc((size_t)m * m * 3 * sizeof(float));
    if (bl && tmp) {
      for (int y = 0; y < m; ++y) for (int x = 0; x < m; ++x) for (int ch = 0; ch < 3; ++ch) {
        float a = 0; for (int q = 0; q < 16; ++q) a += fmaxf(0.0f, P[((size_t)(y * 4 + (q >> 2)) * n + x * 4 + (q & 3)) * 3 + ch] - 0.88f);
        bl[(y * m + x) * 3 + ch] = a / 16;
      }
      for (int pass = 0; pass < 3; ++pass) {         // 横と縦に 5 点の平均を3回
        for (int y = 0; y < m; ++y) for (int x = 0; x < m; ++x) for (int ch = 0; ch < 3; ++ch) { float a = 0; for (int d = -2; d <= 2; ++d) a += bl[(y * m + max(0, min(m - 1, x + d))) * 3 + ch]; tmp[(y * m + x) * 3 + ch] = a / 5; }
        for (int y = 0; y < m; ++y) for (int x = 0; x < m; ++x) for (int ch = 0; ch < 3; ++ch) { float a = 0; for (int d = -2; d <= 2; ++d) a += tmp[(max(0, min(m - 1, y + d)) * m + x) * 3 + ch]; bl[(y * m + x) * 3 + ch] = a / 5; }
      }
      for (int y = 0; y < n; ++y) for (int x = 0; x < n; ++x) {
        const float fx = fminf(m - 1.001f, fmaxf(0.0f, x / 4.0f - 0.5f)), fy = fminf(m - 1.001f, fmaxf(0.0f, y / 4.0f - 0.5f));
        const int ix = (int)fx, iy = (int)fy; const float ax = fx - ix, ay = fy - iy;
        for (int ch = 0; ch < 3; ++ch) {
          const float v = (bl[(iy * m + ix) * 3 + ch] * (1 - ax) + bl[(iy * m + ix + 1) * 3 + ch] * ax) * (1 - ay) + (bl[((iy + 1) * m + ix) * 3 + ch] * (1 - ax) + bl[((iy + 1) * m + ix + 1) * 3 + ch] * ax) * ay;
          float& o = P[((size_t)y * n + x) * 3 + ch]; o = o + v * 0.9f; o = o / (1 + 0.18f * o) * 1.1f;   // にじみを足して、明るい所をやわらかく
        }
      }
    }
    free(bl); free(tmp);
  }
  };
  auto ditherTo = [&](uint16_t* out) {
  // ---- 565 に(縞が出ないよう、4x4 のディザ)。丸い画面の外は黒
  static const uint8_t BY[16] = {0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5};
  for (int y = 0; y < n; ++y)
    for (int x = 0; x < n; ++x) {
      const float* o = P + ((size_t)y * n + x) * 3; const float dt = (BY[(y & 3) * 4 + (x & 3)] + 0.5f) / 16.0f;
      const int r = min(31, (int)(cl01(o[0]) * 31 + dt)), g = min(63, (int)(cl01(o[1]) * 63 + dt)), b = min(31, (int)(cl01(o[2]) * 31 + dt));
      const uint16_t v = (uint16_t)((r << 11) | (g << 5) | b);
      out[(size_t)y * n + x] = (uint16_t)((v >> 8) | (v << 8));
    }
  };
  for (int i = 0; i < n * n * 3; ++i) P2[i] = P[i];   // 箱を描く前の絵をとっておく
  progA = 0.62f; postAll(); ditherTo(outNo);       // 鏡の箱のない景色
  { float* t = P; P = P2; P2 = t; }
  prog(0.78f);
  // ---- 鏡の箱: 面ごとに、向かいの空を映す
  for (int b = 0; b < Q.nBox; ++b) {
    const Box& B = Q.box[b];
    for (int f = 0; f < 2; ++f) {
      const float u0 = f ? B.xc : B.xa, u1 = f ? B.xb : B.xc, t0 = f ? B.tc : B.ta, t1 = f ? B.tb : B.tc;
      if (u1 <= u0) continue;
      const float dk = B.dark * (f == 0 ? 0.88f : 1.0f);
      for (int x = max(0, (int)(u0 * n)); x < min(n, (int)(u1 * n) + 1); ++x) {
        const float X = (x + 0.5f) * inv, u = (X - u0) / (u1 - u0);
        if (u < 0 || u > 1) continue;
        const float top = t0 + (t1 - t0) * u;
        for (int y = max(0, (int)(top * n)); y < min(n, (int)(B.bot * n) + 1); ++y) {
          const float Y = (y + 0.5f) * inv; if (Y < top || Y > B.bot || Y > edge[x]) continue;
          const float sx = cl01(f == 0 ? 0.92f - u * 0.35f : 0.30f + u * 0.4f), sy = cl01(0.04f + (Y - top) / fmaxf(B.bot - top, 0.001f) * (Q.hz + 0.06f));
          C3 c = {0, 0, 0};
          for (int q = 0; q < (RL ? 4 : 1); ++q) {   // 映りを少しぼかす(4点の平均)
            const int ox = RL ? (q & 1) * 2 - 1 : 0, oy = RL ? (q >> 1) * 2 - 1 : 0;
            const uint16_t v = FULL[(size_t)max(0, min(n - 1, (int)(sy * n) + oy)) * n + max(0, min(n - 1, (int)(sx * n) + ox))];
            c = add(c, {(v >> 11) / 31.0f * dk, ((v >> 5) & 63) / 63.0f * dk, (v & 31) / 31.0f * dk});
          }
          c = mul(c, RL ? 0.25f : 1.0f);
          if (RL) {                                  // 下ほど少し暗く、角は面取りの光
            const float vv = (Y - top) / fmaxf(B.bot - top, 0.001f);
            c = mul(c, 1.0f - 0.15f * vv);
            c = add(c, mul({1.0f, 0.95f, 0.92f}, (pexp(-u * u / 0.0002f) + pexp(-(1 - u) * (1 - u) / 0.0002f)) * (f == 1 ? 0.35f : 0.2f)));
          }
          c = mix(c, Q.glass, Q.glassMix);
          float* o = P + ((size_t)y * n + x) * 3;
          if (Q.real) {                              // すきとおったガラス: うしろの景色が見えて、空の映りが重なる
            const float rf_ = 0.30f + 0.25f * (f == 1) + 0.15f * (1 - (Y - top) / fmaxf(B.bot - top, 0.001f));
            c = {o[0] * (1 - rf_) + c.r * rf_ + 0.04f, o[1] * (1 - rf_) + c.g * rf_ + 0.045f, o[2] * (1 - rf_) + c.b * rf_ + 0.05f};
          }
          o[0] = c.r; o[1] = c.g; o[2] = c.b;
        }
      }
    }
    const float L[5][4] = {{B.xa, B.ta, B.xc, B.tc}, {B.xc, B.tc, B.xb, B.tb}, {B.xc, B.tc, B.xc, B.bot}, {B.xa, B.ta, B.xa, B.bot}, {B.xb, B.tb, B.xb, B.bot}};
    clipEdge = edge;
    for (int i = 0; i < 5; ++i) line(L[i][0] * n, L[i][1] * n, L[i][2] * n, L[i][3] * n, Q.edgeC, Q.real ? (i == 2 ? 0.85f : 0.6f) : 0.55f, Q.real && i == 2 ? 2 : 1);
    clipEdge = nullptr;
  }
  if (RL) for (int b = 0; b < Q.nBox; ++b) {   // 鏡の箱の足もとの影
    const Box& B = Q.box[b];
    for (int y = max(0, (int)((B.bot - 0.01f) * n)); y < min(n, (int)((B.bot + 0.04f) * n)); ++y) {
      const float e = pexp(-powf(((y + 0.5f) * inv - B.bot - 0.008f) / 0.012f, 2));
      for (int x = max(0, (int)((B.xa - 0.02f) * n)); x < min(n, (int)((B.xb + 0.05f) * n)); ++x) {
        if ((y + 0.5f) * inv < edge[x]) continue;
        float* o = P + ((size_t)y * n + x) * 3; o[0] *= 1 - 0.45f * e; o[1] *= 1 - 0.45f * e; o[2] *= 1 - 0.45f * e;
      }
    }
  }
  progA = 0.80f; postAll(); ditherTo(outBox);      // 場面の絵(鏡の箱つき)
  { float* t = P; P = P2; P2 = t; }
  // ---- 動きの情報
  MeadowFx& F = MEADOW_FX[k];
  F.hz = Q.hz; F.sunX = Q.sunX; F.sunY = Q.sunY; F.sea = Q.sea; F.rip = Q.rip; F.nBox = Q.nBox;
  for (int b = 0; b < 4; ++b) { const Box& B = Q.box[b]; const float v[7] = {B.xa, B.xc, B.xb, B.ta, B.tc, B.tb, B.bot}; for (int j = 0; j < 7; ++j) F.box[b][j] = v[j]; }
  for (int i = 0; i < 466; ++i) F.edge[i] = (uint16_t)(cl01(edge[min(n - 1, i * n / 466)]) * 4096);
  for (int i = 0; i < 466; ++i) {                  // 行ごとの川の左右(割合 × 4096)
    const float yy = (i + 0.5f) / 466.0f; F.rivA[i] = 4096; F.rivB[i] = 0;
    if (Q.river && yy > gt) { float cx, hw; riverAt(gt, yy, cx, hw); F.rivA[i] = (uint16_t)(cl01(cx - hw * 0.8f) * 4096); F.rivB[i] = (uint16_t)(cl01(cx + hw * 0.8f) * 4096); }   // 岸の草がかぶさる所は花といっしょに揺れる
  }
  free(edge);
  prog(1.0f);
  return true;
}
// 半分の大きさで描いて、2倍に広げる。描いた半分の大きさの絵は HALFN(箱なし)・HALF(箱あり)に残る(保存用)
static uint16_t* HALF = nullptr;
static uint16_t* HALFN = nullptr;
static void expandHalf(const uint16_t* half, uint16_t* out, int n) {
  const int h = n / 2;
  for (int y = 0; y < n; ++y) {
    const uint16_t* src = half + (size_t)min(h - 1, y >> 1) * h; uint16_t* o = out + (size_t)y * n;
    for (int x = 0; x < n; ++x) o[x] = src[min(h - 1, x >> 1)];
  }
}
static bool paintBothHalf(int k, uint16_t* outNo, uint16_t* outBox, int n) {
  const int h = n / 2;
  if (!HALF) HALF = (uint16_t*)ps_malloc((size_t)h * h * 2);
  if (!HALFN) HALFN = (uint16_t*)ps_malloc((size_t)h * h * 2);
  if (!HALF || !HALFN || !paintBoth(k, HALFN, HALF, h)) return false;
  expandHalf(HALFN, outNo, n); expandHalf(HALF, outBox, n);
  return true;
}
}  // namespace mpaint
