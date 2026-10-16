// NullKaleido  -  NullMenu 用モジュール(元のスケッチ NullKaleido_v12 を名前空間に入れたもの)
// 0 で始まる変数(配列も)は、ほかの作品と共通の置き場(NM_ARENA)に置いています。
// 一度に動くのは1つだけ(切り替えるときは再起動)なので、重なっていても問題ありません。
#include "nm_common.h"
#if defined(ESP_PLATFORM) && __has_include(<lgfx/v1/panel/Panel_AMOLED.hpp>)
#include <lgfx/v1/panel/Panel_AMOLED.hpp>
#endif
#pragma GCC optimize("O3")

namespace NullKaleido {
static constexpr size_t nmOv0 = 0;

// NullKaleido v12  -  うずを巻いて流れる、ぬるぬるの光のマーブル模様。光の粒が、動きまわりながらピカピカまたたく
//   大阪・関西万博のパビリオン「null²」の中を見たイメージから作った、非公式の習作です。作品の映像・名前・データは使っていません。
//   M5Stack StopWatch 用(丸い 466x466 画面)。CoreS3 系でも真ん中に正方形で動きます。
//
// 画面の中:
//   ・マーブル模様の光が、ゆっくり流れながら、いくつものうずに巻きこまれる
//   ・表面はぬれたようにつやがあり、うずに合わせて光の照りかえしがぬるぬる動く
//   ・うずの中心には、黒いつやつやの玉が浮かんでいる。玉からは、うねうねとしたうねりが外へ広がっていく
//     うねりとうずがだんだん小さくなるのと一緒に、玉も小さくなって消え、別の所にまた新しい玉が生まれる
//   ・光の粒が、あちこちでふっと現れて、ふらふら動きまわりながらピカピカ光り、また消える
//   ・色は「にじ / みどり / あお / あか / しろ」の5つ
//
// 触ると:
//   触った所に新しいうずが生まれて、押しているあいだ強く巻きこむ(黒い玉も浮かぶ)。離すと、ゆっくりほどける
//   黒い玉を触ると、うねりと玉が早めに小さくなって、やんわり消えていく
//
// 操作:
//   StopWatch … ボタンA: 色(にじ → みどり → あお → あか → しろ)  ボタンB: うずの並びを変える
//   CoreS3系  … 電源ボタン短押し: 色                        ダブルタップ: うずの並びを変える
//
// 必要なもの:
//   ボードマネージャー M5Stack >= 3.3.7 / ボード: M5StopWatch(CoreS3 なら M5CoreS3)
//   ライブラリ M5Unified >= 0.2.15, M5GFX >= 0.2.21
//
// しくみ:
//   マーブル模様は、最初に1枚(256x256、横にも縦にもつながる)作っておく。色は表(256色 x 明るさ9段)から引く。
//   コマごとには、画面に 8 点おきの格子を置き、格子の点ごとに「流れ」と「うず(回転)」で模様のどこを見るかを計算する。
//   格子の点では、うずで曲がった面の傾きから、つや(光の照りかえし)の明るさも出しておく。
//   1点ずつは、格子のあいだを直線でつないで模様の位置と明るさを出し、表を引くだけ。
//   格子の計算は、上半分と下半分を2つのコアで分ける。画面は16行ずつの帯に分けて、2つのコアで1本おきに描く。

#ifndef IRAM_ATTR
#define IRAM_ATTR
#endif


// Arduino IDE は関数の宣言を自動で先頭近くに足すので、型の名前だけ先に宣言しておく
struct Vortex;
struct Spark;
struct Ball;

// ---- 調整用 ----------------------------------------------------------------
static constexpr float FLOW      = 1.0f;    // 模様が流れる速さ
static constexpr float SWIRL     = 1.0f;    // うずの巻きこみの強さ
static constexpr int   NSPARK    = 60;      // 光の粒の数
// ---------------------------------------------------------------------------

// StopWatch では、画面へ「直接」送る(M5GFX の PSRAM の置き場を通さない)。画面が乱れるときは false に
static constexpr bool DIRECT_SEND = true;
#if defined(ESP_PLATFORM) && defined(LGFX_USE_QSPI) && __has_include(<lgfx/v1/panel/Panel_AMOLED.hpp>)
#define NL_DIRECT_AMOLED 1
struct NlFbPeek : public lgfx::Panel_AMOLED_Framebuffer {
  static lgfx::Panel_AMOLED* inner(lgfx::Panel_Device* p) {
    return static_cast<lgfx::Panel_AMOLED_Framebuffer*>(p)->*(&NlFbPeek::_panel);
  }
};
#endif

static constexpr int BAND = 16, NSLOT = 4;
static constexpr int TEX = 256;                   // マーブル模様の大きさ(TEX x TEX)
static constexpr int CELL = 8;                    // 格子の間隔(点)
static constexpr int MAXG = 64;                   // 格子の点の数の上限(1辺)
static constexpr int NLEV = 17;                   // 明るさの段(0 = 黒 .. 8 = そのまま .. 16 = つやで白く光る)
static constexpr int MAXV = 7;                    // うずの数(指のうずを入れて)
static constexpr int SPR = 18;                    // 光の粒の半径(光の筋まで)

static constexpr size_t nmOa0 = nmAlignTo(nmOv0, alignof(int)), nmOv1 = nmOa0 + sizeof(int); static int& W = *reinterpret_cast<int*>(NM_ARENA + nmOa0);
static constexpr size_t nmOa1 = nmAlignTo(nmOv1, alignof(int)), nmOv2 = nmOa1 + sizeof(int); static int& H = *reinterpret_cast<int*>(NM_ARENA + nmOa1);
static constexpr size_t nmOa2 = nmAlignTo(nmOv2, alignof(int)), nmOv3 = nmOa2 + sizeof(int); static int& S = *reinterpret_cast<int*>(NM_ARENA + nmOa2);
static constexpr size_t nmOa3 = nmAlignTo(nmOv3, alignof(int)), nmOv4 = nmOa3 + sizeof(int); static int& XOFF = *reinterpret_cast<int*>(NM_ARENA + nmOa3);
static constexpr size_t nmOa4 = nmAlignTo(nmOv4, alignof(int)), nmOv5 = nmOa4 + sizeof(int); static int& YOFF = *reinterpret_cast<int*>(NM_ARENA + nmOa4);
static constexpr size_t nmOa5 = nmAlignTo(nmOv5, alignof(int)), nmOv6 = nmOa5 + sizeof(int); static int& NB = *reinterpret_cast<int*>(NM_ARENA + nmOa5);
static constexpr size_t nmOa6 = nmAlignTo(nmOv6, alignof(int)), nmOv7 = nmOa6 + sizeof(int); static int& NG = *reinterpret_cast<int*>(NM_ARENA + nmOa6);
static bool roundScr = true;
static constexpr size_t nmOa7 = nmAlignTo(nmOv7, alignof(bool)), nmOv8 = nmOa7 + sizeof(bool); static bool& fbMode = *reinterpret_cast<bool*>(NM_ARENA + nmOa7);
static constexpr size_t nmOa8 = nmAlignTo(nmOv8, alignof(bool)), nmOv9 = nmOa8 + sizeof(bool); static bool& directSend = *reinterpret_cast<bool*>(NM_ARENA + nmOa8);
static int16_t* spanA = nullptr;
static int16_t* spanB = nullptr;
static int16_t* bandX = nullptr;
static int16_t* bandW = nullptr;
static uint16_t* band[NSLOT] = {nullptr, nullptr, nullptr, nullptr};

static uint8_t*  tex = nullptr;                   // マーブル模様(色の番号) [TEX * TEX]
static uint16_t* pal = nullptr;                   // 色の表 [NLEV * 256](画面へ送る形)
static int32_t*  nodeU = nullptr;                 // 格子の点ごとの、模様の位置(1/256 点) [NG * NG]
static int32_t*  nodeV = nullptr;
static int16_t*  nodeL = nullptr;                 // 格子の点ごとの明るさ(0 .. 16 * 256)
static float*    nodeH = nullptr;                 // 格子の点ごとの、面の高さ(つやの計算用)
struct Ball { int16_t x, y, r; uint8_t a; };       // 黒いつやつやの玉(画面の点で。a = 濃さ 0..32)
static constexpr size_t nmOa9 = nmAlignTo(nmOv9, 16), nmOv10 = nmOa9 + sizeof(Ball[7]); static Ball (&ball)[7] = *reinterpret_cast<Ball(*)[7]>(NM_ARENA + nmOa9);
static constexpr size_t nmOa10 = nmAlignTo(nmOv10, alignof(int)), nmOv11 = nmOa10 + sizeof(int); static int& nBall = *reinterpret_cast<int*>(NM_ARENA + nmOa10);
static uint8_t   sprA[(2 * SPR + 1) * (2 * SPR + 1)];   // 光の粒の形(0..32)
static constexpr size_t nmOa11 = nmAlignTo(nmOv11, 16), nmOv12 = nmOa11 + sizeof(float[1024]); static float (&sinT)[1024] = *reinterpret_cast<float(*)[1024]>(NM_ARENA + nmOa11);

struct Vortex { float x, y, s, k, hole; float bx, by, ph, w, k0; float has, fade, goal, wait, vis; float age, life, spd, str, hr2; };   // str: いまの強さ(0..1)/ age, life: 生まれてからの時間と寿命 / hr2: 玉のまわりの暗さの半径²   // has: 玉を持つか / fade: 玉の濃さ(0..1)
static constexpr size_t nmOa12 = nmAlignTo(nmOv12, 16), nmOv13 = nmOa12 + sizeof(Vortex[MAXV]); static Vortex (&vor)[MAXV] = *reinterpret_cast<Vortex(*)[MAXV]>(NM_ARENA + nmOa12);
static constexpr size_t nmOa13 = nmAlignTo(nmOv13, alignof(int)), nmOv14 = nmOa13 + sizeof(int); static int& nVor = *reinterpret_cast<int*>(NM_ARENA + nmOa13);
struct Spark { float x, y, vx, vy, age, life, tw, sp; };   // age / life: 現れてから消えるまで
static constexpr size_t nmOa14 = nmAlignTo(nmOv14, 16), nmOv15 = nmOa14 + sizeof(Spark[NSPARK]); static Spark (&spark)[NSPARK] = *reinterpret_cast<Spark(*)[NSPARK]>(NM_ARENA + nmOa14);
static constexpr size_t nmOa15 = nmAlignTo(nmOv15, alignof(float)), nmOv16 = nmOa15 + sizeof(float); static float& simT = *reinterpret_cast<float*>(NM_ARENA + nmOa15);
static constexpr size_t nmOa16 = nmAlignTo(nmOv16, alignof(int)), nmOv17 = nmOa16 + sizeof(int); static int& theme = *reinterpret_cast<int*>(NM_ARENA + nmOa16);
static uint32_t rnd = 12345;  // 乱数のたね
static inline float frand() { rnd = rnd * 1664525u + 1013904223u; return (rnd >> 8) * (1.0f / 16777216.0f); }   // 0..1
static constexpr size_t nmOa17 = nmAlignTo(nmOv17, alignof(float)), nmOv18 = nmOa17 + sizeof(float); static float& fingerK = *reinterpret_cast<float*>(NM_ARENA + nmOa17);
static constexpr size_t nmOa18 = nmAlignTo(nmOv18, alignof(float)), nmOv19 = nmOa18 + sizeof(float); static float& fingerX = *reinterpret_cast<float*>(NM_ARENA + nmOa18);
static constexpr size_t nmOa19 = nmAlignTo(nmOv19, alignof(float)), nmOv20 = nmOa19 + sizeof(float); static float& fingerY = *reinterpret_cast<float*>(NM_ARENA + nmOa19);  // 指のうず
static constexpr size_t nmOa20 = nmAlignTo(nmOv20, alignof(bool)), nmOv21 = nmOa20 + sizeof(bool); static bool& fingerBall = *reinterpret_cast<bool*>(NM_ARENA + nmOa20);  // 指のうずに玉を出すか(玉を触って消しているときは出さない)

// 2つのコアで描く
static SemaphoreHandle_t semGo = nullptr, semDone = nullptr;
static volatile int readyW = -1, doneUpTo = -1;
static uint32_t busyUs[2] = {0, 0}, sendUs = 0, simUs = 0;
static volatile int job = 0;                      // コア0 にたのむ仕事: 0 = 帯を描く / 1 = 格子(下半分) / 2 = つや(下半分)
static constexpr size_t nmOa21 = nmAlignTo(nmOv21, alignof(float)), nmOv22 = nmOa21 + sizeof(float); static float& gHalf = *reinterpret_cast<float*>(NM_ARENA + nmOa21);
static constexpr size_t nmOa22 = nmAlignTo(nmOv22, alignof(float)), nmOv23 = nmOa22 + sizeof(float); static float& gInv = *reinterpret_cast<float*>(NM_ARENA + nmOa22);
static constexpr size_t nmOa23 = nmAlignTo(nmOv23, alignof(float)), nmOv24 = nmOa23 + sizeof(float); static float& gTf = *reinterpret_cast<float*>(NM_ARENA + nmOa23);
static constexpr size_t nmOa24 = nmAlignTo(nmOv24, alignof(float)), nmOv25 = nmOa24 + sizeof(float); static float& gDu = *reinterpret_cast<float*>(NM_ARENA + nmOa24);
static constexpr size_t nmOa25 = nmAlignTo(nmOv25, alignof(float)), nmOv26 = nmOa25 + sizeof(float); static float& gDv = *reinterpret_cast<float*>(NM_ARENA + nmOa25);  // 格子の計算で使う値(コマごと)

// ---------------------------------------------------------------- 小さな道具
static inline uint16_t toPix(float r, float g, float b) {
  int R = (int)(r * 255.0f + 0.5f), G = (int)(g * 255.0f + 0.5f), B = (int)(b * 255.0f + 0.5f);
  R = R < 0 ? 0 : R > 255 ? 255 : R; G = G < 0 ? 0 : G > 255 ? 255 : G; B = B < 0 ? 0 : B > 255 ? 255 : B;
  return __builtin_bswap16((uint16_t)(((R & 0xF8) << 8) | ((G & 0xFC) << 3) | (B >> 3)));
}
static inline __attribute__((always_inline)) uint16_t mix565(uint16_t p, uint16_t q, uint32_t a) {   // a: 0..32
  uint32_t x = __builtin_bswap16(p), y = __builtin_bswap16(q);
  x = (x | (x << 16)) & 0x07E0F81Fu;
  y = (y | (y << 16)) & 0x07E0F81Fu;
  const uint32_t r = ((x * (32 - a) + y * a) >> 5) & 0x07E0F81Fu;
  return __builtin_bswap16((uint16_t)(r | (r >> 16)));
}
static inline uint32_t hash32(uint32_t x) { x ^= x >> 16; x *= 0x7feb352d; x ^= x >> 15; x *= 0x846ca68b; x ^= x >> 16; return x; }
static inline float hashf(int x, int y) { return (hash32((uint32_t)x * 73856093u ^ (uint32_t)y * 19349663u) & 0xFFFF) / 65535.0f; }
// つながったノイズ(period ごとにくり返す)
static float vnoise(float x, float y, int period) {
  const int xi = (int)floorf(x), yi = (int)floorf(y);
  float fx = x - xi, fy = y - yi;
  fx = fx * fx * (3 - 2 * fx); fy = fy * fy * (3 - 2 * fy);
  auto hv = [&](int a, int b) { return hashf(((a % period) + period) % period, ((b % period) + period) % period); };
  const float a = hv(xi, yi), b = hv(xi + 1, yi), c = hv(xi, yi + 1), d = hv(xi + 1, yi + 1);
  return (a * (1 - fx) + b * fx) * (1 - fy) + (c * (1 - fx) + d * fx) * fy;
}
static float fbm(float x, float y, int period, int oct) {   // x, y は 0..period
  float s = 0, a = 0.5f, n = 0;
  for (int o = 0; o < oct; ++o) { s += a * vnoise(x, y, period); n += a; x *= 2; y *= 2; period *= 2; a *= 0.5f; }
  return s / n;
}
static inline __attribute__((always_inline)) float fsin(float x) { return sinT[(int)(x * (1024.0f / 6.2831853f)) & 1023]; }
static inline __attribute__((always_inline)) float fcos(float x) { return sinT[((int)(x * (1024.0f / 6.2831853f)) + 256) & 1023]; }
static void* imalloc(size_t n) {                  // 内蔵メモリに(足りなければ、どこでも)
  void* p = heap_caps_malloc(n, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  return p ? p : malloc(n);
}

// ---------------------------------------------------------------- マーブル模様と色
static void buildTexture() {
  // ゆがめたノイズの「等高線」を色の番号にする(ノイズを、もう1つのノイズで押し流してから使う)
  for (int y = 0; y < TEX; ++y)
    for (int x = 0; x < TEX; ++x) {
      const float fx = x * (4.0f / TEX), fy = y * (4.0f / TEX);
      const float wx = fbm(fx + 1.7f, fy + 9.2f, 4, 3), wy = fbm(fx + 8.3f, fy + 2.8f, 4, 3);
      const float n = fbm(fx + 2.2f * wx, fy + 2.2f * wy, 4, 4);
      const float m = fbm(fx * 2.0f + 5.0f, fy * 2.0f + 1.0f, 8, 2);
      tex[y * TEX + x] = (uint8_t)((int)((n * 3.2f + m * 0.35f) * 256.0f) & 255);
    }
}
struct Theme { float c[6][3]; };
static const Theme THEMES[] = {
  {{{0.10f, 0.03f, 0.02f}, {0.88f, 0.08f, 0.05f}, {1.00f, 0.55f, 0.10f}, {1.00f, 0.92f, 0.72f}, {0.30f, 0.85f, 1.00f}, {0.10f, 0.28f, 0.85f}}},   // にじ
  {{{0.00f, 0.05f, 0.02f}, {0.02f, 0.45f, 0.18f}, {0.25f, 0.95f, 0.35f}, {0.95f, 0.85f, 0.25f}, {0.10f, 0.75f, 0.70f}, {0.85f, 1.00f, 0.90f}}},   // みどり
  {{{0.02f, 0.04f, 0.20f}, {0.05f, 0.30f, 0.85f}, {0.15f, 0.80f, 1.00f}, {0.95f, 0.95f, 1.00f}, {0.90f, 0.20f, 0.25f}, {0.20f, 0.85f, 0.45f}}},   // あお
  {{{0.08f, 0.02f, 0.04f}, {0.85f, 0.10f, 0.20f}, {1.00f, 0.45f, 0.65f}, {0.35f, 0.95f, 0.25f}, {0.05f, 0.55f, 0.20f}, {0.20f, 0.90f, 0.90f}}},   // あか
  {{{0.04f, 0.10f, 0.14f}, {0.30f, 0.60f, 0.65f}, {0.85f, 0.95f, 0.95f}, {0.55f, 0.75f, 0.85f}, {0.95f, 0.75f, 0.25f}, {0.15f, 0.45f, 0.40f}}},   // しろ
};
static constexpr int NTHEME = sizeof(THEMES) / sizeof(THEMES[0]);
static const char* const THEME_NAME[NTHEME] = {"rainbow", "green", "blue", "red", "white"};
static void buildPalette(int th) {
  const Theme& T = THEMES[th];
  for (int i = 0; i < 256; ++i) {
    const float t = i / 256.0f;
    // 色: 6色を順に2回まわす
    const float q = t * 2.0f * 6.0f; const int a = (int)q % 6, b = (a + 1) % 6; const float f = q - (int)q;
    const float sf = f * f * (3 - 2 * f);
    float r = T.c[a][0] + (T.c[b][0] - T.c[a][0]) * sf, g = T.c[a][1] + (T.c[b][1] - T.c[a][1]) * sf, bl = T.c[a][2] + (T.c[b][2] - T.c[a][2]) * sf;
    // マーブルのすじ: 細い黒い線と、そのとなりの明るいふち
    const float l = t * 8.0f - floorf(t * 8.0f);
    float k = 1.0f;
    if (l < 0.06f) k = 0.20f + l * 6.0f;
    else if (l < 0.12f) k = 1.35f;
    r *= k; g *= k; bl *= k;
    for (int lv = 0; lv < NLEV; ++lv) {
      if (lv <= 8) { const float d = lv / 8.0f; pal[lv * 256 + i] = toPix(r * d, g * d, bl * d); continue; }
      const float w = (lv - 8) * (0.85f / 8.0f);   // つや: 白に近づける
      pal[lv * 256 + i] = toPix(r + (1.0f - r) * w, g + (1.0f - g) * w, bl + (1.0f - bl) * w);
    }
  }
}
static void buildSprite() {                        // 光の粒: まん中が強く光り、まわりがにじみ、十字の光の筋が出る
  for (int y = -SPR; y <= SPR; ++y)
    for (int x = -SPR; x <= SPR; ++x) {
      const float d = sqrtf((float)(x * x + y * y)) / SPR;
      const float g = d >= 0.5f ? 0 : 1 - d / 0.5f;                            // にじみ
      float a = g * g * 30.0f + (d < 0.09f ? 32.0f : 0.0f);
      const int ax = abs(x), ay = abs(y);
      if (ay <= 1 || ax <= 1) {                                               // 十字の筋(まん中の1本は太く、となりは細く)
        const float t = 1.0f - (float)(ay <= 1 ? ax : ay) / SPR;
        a += t * t * t * ((ay == 0 || ax == 0) ? 34.0f : 10.0f);
      }
      if (d >= 1) a = 0;
      sprA[(y + SPR) * (2 * SPR + 1) + (x + SPR)] = (uint8_t)(a > 32 ? 32 : a);
    }
}

// ---------------------------------------------------------------- 動き
static void respawn(Spark& p) {                    // 光の粒を、どこか別の所にもう一度出す
  p.x = frand() * S; p.y = frand() * S;
  const float a = frand() * 6.2831853f, v = 10.0f + frand() * 30.0f;
  p.vx = v * cosf(a); p.vy = v * sinf(a);
  p.age = 0; p.life = 1.2f + frand() * 3.0f;
  p.tw = frand() * 6.28f; p.sp = 9.0f + frand() * 16.0f;   // またたく速さ
}
static void rebirth(int i, bool first) {          // うず(と玉)を、別の所に新しく生まれさせる
  Vortex& v = vor[i];
  float bx = 0, by = 0;
  for (int t = 0; t < 12; ++t) {                    // ほかのうずから離れた所をさがす
    const float a = frand() * 6.2831853f, r = sqrtf(frand()) * 0.70f;
    bx = r * cosf(a); by = r * sinf(a);
    bool ok = true;
    for (int j = 0; j < 6; ++j) {
      if (j == i || vor[j].str < 0.05f) continue;
      const float dx = bx - vor[j].bx, dy = by - vor[j].by, d = vor[j].s + 0.24f;
      if (dx * dx + dy * dy < d * d) { ok = false; break; }
    }
    if (ok) break;
  }
  v.bx = bx; v.by = by; v.s = 0.24f + frand() * 0.16f;
  v.k0 = (4.5f + frand() * 2.5f) * (frand() < 0.5f ? -1.0f : 1.0f) * SWIRL;
  v.ph = frand() * 6.28f; v.w = 0.10f + frand() * 0.10f;
  v.life = 9.0f + frand() * 8.0f; v.age = first ? (i + frand() * 0.5f) / 6.0f * v.life : 0.0f; v.spd = 1.0f; v.wait = 0; v.str = 0;
}
static void initVortices(bool shuffle) {
  // x, y は画面の中心が 0、ふちが ±1。s = うずの大きさ、k0 = 巻きこみの強さ、hole = 黒い穴
  static const float V[6][6] = {
    {-0.45f, -0.40f, 0.30f,  5.5f, 1.0f, 0.0f},
    { 0.35f,  0.30f, 0.38f, -6.5f, 1.0f, 1.3f},
    {-0.30f,  0.55f, 0.26f,  4.0f, 0.0f, 2.1f},
    { 0.55f, -0.50f, 0.27f, -4.5f, 0.0f, 3.7f},
    { 0.00f,  0.00f, 0.25f,  4.5f, 0.0f, 4.4f},
    {-0.70f,  0.05f, 0.25f, -4.0f, 1.0f, 5.2f},
  };
  nVor = 6;
  for (int i = 0; i < 6; ++i) {
    Vortex& v = vor[i];
    if (shuffle) {                                  // 並びを変える: 場所・大きさ・向き・穴をランダムに
      const float a = i * (6.2831853f / 6.0f) + (frand() - 0.5f) * 0.8f, r = i == 0 ? frand() * 0.2f : 0.38f + frand() * 0.32f;   // 1つはまん中近く、ほかはまわりに散らす
      v.bx = r * cosf(a); v.by = r * sinf(a); v.s = 0.24f + frand() * 0.16f;
      v.k0 = (4.0f + frand() * 3.0f) * (frand() < 0.5f ? -1.0f : 1.0f) * SWIRL; v.has = 1.0f;
    } else {
      v.bx = V[i][0]; v.by = V[i][1]; v.s = V[i][2]; v.k0 = V[i][3] * SWIRL; v.has = 1.0f;
    }
    v.ph = V[i][5]; v.w = 0.11f + 0.03f * i;
    v.x = v.bx; v.y = v.by; v.k = v.k0;
    v.fade = v.has; v.goal = v.has; v.wait = 0; v.vis = 0; v.hole = 0;
    v.life = 9.0f + frand() * 8.0f; v.age = (i + frand() * 0.5f) / 6.0f * v.life; v.spd = 1.0f; v.str = 0;   // 始めから、ばらばらの時に
    if (shuffle) rebirth(i, true);
  }
  if (shuffle) return;
  for (int i = 0; i < NSPARK; ++i) { respawn(spark[i]); spark[i].age = frand() * spark[i].life; }   // 始めから、ばらばらの時に
}
static void simPre(float dt) {
  simT += dt;
  for (int i = 0; i < 6; ++i) {
    Vortex& v = vor[i];
    if (v.wait > 0) { v.wait -= dt; v.str = 0; if (v.wait <= 0) rebirth(i, false); }
    else {
      v.age += dt * v.spd;
      const float u = v.age / v.life;
      if (u >= 1.0f) { v.str = 0; v.wait = 0.8f + frand() * 3.0f; }
      else {
        float g = v.age / 2.0f; g = g > 1 ? 1 : g; g = g * g * (3 - 2 * g);             // 生まれるとき: 約2秒でふくらむ
        float d = (u - 0.30f) / 0.70f; d = d < 0 ? 0 : d; d = 1.0f - d * d * (3 - 2 * d);   // 消えるとき: ゆっくり小さく
        v.str = g * d;
      }
    }
    v.x = v.bx + 0.06f * fsin(simT * v.w + v.ph);
    v.y = v.by + 0.05f * fcos(simT * v.w * 0.8f + v.ph * 1.3f);
    v.k = v.k0 * v.str;
    const float sq = sqrtf(v.str);
    v.hole = v.str > 0.03f ? 1.0f : v.str / 0.03f;
    const float br = v.s * 0.36f * sq;               // 玉の半径(強さの平方根に合わせて小さくなる)
    v.hr2 = br * br * 2.3f;
  }
  // 指のうず(玉は、押しているあいだにゆっくり浮かび、離すとやんわり消える)
  {
    Vortex& v = vor[6];
    v.x = fingerX; v.y = fingerY; v.s = 0.28f; v.k = fingerK; v.has = 1.0f;
    const float goal = (fingerBall && fingerK > 2.0f) ? 1.0f : 0.0f;
    v.fade = goal > v.fade ? min(goal, v.fade + dt / 2.0f) : max(goal, v.fade - dt / 4.0f);
    v.hole = v.fade;
    v.str = max(v.fade, fabsf(fingerK) / 7.0f);     // うずとうねりの強さは、押している強さで(玉が出ていなくても、うずは巻く)
    const float br = v.s * 0.36f * sqrtf(v.fade);
    v.hr2 = br * br * 2.3f;
  }
  nVor = (fabsf(fingerK) > 0.05f || vor[6].fade > 0.01f) ? 7 : 6;
  gHalf = S * 0.5f; gInv = 1.0f / gHalf; gTf = simT * FLOW;
  gDu = fmodf(gTf * 7.0f, (float)TEX); gDv = fmodf(gTf * 3.0f, (float)TEX);   // 模様全体の流れ(点 / 秒。模様はつながっているので、1周で戻す)
}
// 格子の点ごとに、模様のどこを見るか(j0 行から j1 行の手前まで)
static void IRAM_ATTR simRows(int j0, int j1) {
  const float half = gHalf, inv = gInv, tf = gTf, du = gDu, dv = gDv;
  for (int j = j0; j < j1; ++j) {
    const float Y0 = (j * CELL - half) * inv;
    for (int i = 0; i < NG; ++i) {
      float X = (i * CELL - half) * inv, Y = Y0;
      // うず: まわす角度は「画面の上での、うずの中心からの距離」で決める(うずの中心が画面のどこに見えるかが、ぶれない)
      const float PX = X, PY = Y;
      float lit = 1.0f;
      for (int v = 0; v < nVor; ++v) {
        const Vortex& V = vor[v];
        const float ex = PX - V.x, ey = PY - V.y, r2 = ex * ex + ey * ey, s2 = V.s * V.s;
        const float rc2 = s2 * 20.0f;                       // ここより遠くは計算しない(なめらかに 0 へ)
        if (r2 > rc2 || V.str < 0.002f) continue;
        const float q = r2 / rc2, tp = 1.0f - q * q;
        const float a = V.k * s2 / (r2 + s2) * tp;         // まん中ほど大きく回す
        const float c = fcos(a), s = fsin(a), dx = X - V.x, dy = Y - V.y;
        X = V.x + dx * c - dy * s; Y = V.y + dx * s + dy * c;
        {                                                   // うねり: 玉から外へ広がっていく、うねうねした輪の波
          const float r = sqrtf(r2) + 1e-4f, ir = 1.0f / r;
          const float env = V.str * (s2 * 5.0f) / (r2 + s2 * 5.0f) * (r / (r + 0.04f)) * tp;
          const float w = 0.050f * env * fsin(r * 26.0f - simT * 3.6f + V.ph + 0.9f * fsin(ey * 9.0f * ir + simT * 0.7f));
          X += ex * ir * w; Y += ey * ir * w;
        }
        if (V.hole > 0.004f && r2 < V.hr2) {                // 黒い玉のまわりは少し暗く(玉の大きさに合わせて)
          const float q = r2 / V.hr2;
          lit *= 1.0f - V.hole * (1.0f - q * q * (3 - 2 * q));
        }
      }
      // ゆったりした流れ(うずのあとで足すので、うずの中心は動かさない)
      X += 0.22f * fsin(PY * 2.1f + tf * 0.31f) + 0.07f * fsin(PY * 5.3f - PX * 1.9f + tf * 0.53f);
      Y += 0.22f * fsin(PX * 1.7f - tf * 0.27f) + 0.07f * fsin(PX * 4.7f + PY * 2.3f - tf * 0.41f);
      const int n = j * NG + i;
      nodeU[n] = (int32_t)((X * 100.0f + du) * 256.0f);
      nodeV[n] = (int32_t)((Y * 100.0f + dv) * 256.0f);
      nodeL[n] = (int16_t)(lit * 8.0f * 256.0f);
      nodeH[n] = 0.10f * fsin(X * 3.1f + tf * 0.20f) * fcos(Y * 2.7f - tf * 0.15f) + 0.05f * fsin(X * 5.3f + Y * 4.1f + tf * 0.30f);
    }
  }
}
// つや: 面の傾き(となりの格子との高さの差)から、左上の光の照りかえしを出す(j0 行から j1 行の手前まで。全部の行の高さが出てから)
static void IRAM_ATTR simGloss(int j0, int j1) {
  {
    const float k = 1.0f / (2.0f * CELL * gInv);
    const float lx = -0.60f, ly = -0.70f, lz = 0.39f;              // 光の向き(左上から、斜めに)
    const float hx = lx, hy = ly, hz = lz + 1.0f, hl = 1.0f / sqrtf(hx * hx + hy * hy + hz * hz);
    for (int j = j0; j < j1; ++j)
      for (int i = 0; i < NG; ++i) {
        const int n = j * NG + i;
        float gx = (nodeH[j * NG + (i < NG - 1 ? i + 1 : i)] - nodeH[j * NG + (i > 0 ? i - 1 : i)]) * k;
        float gy = (nodeH[(j < NG - 1 ? j + 1 : j) * NG + i] - nodeH[(j > 0 ? j - 1 : j) * NG + i]) * k;
        const float gm = gx * gx + gy * gy;               // うずの真ん中の急な所は、傾きをおさえる(ギザギザ防止)
        if (gm > 1.0f) { const float r = 1.0f / sqrtf(gm); gx *= r; gy *= r; }
        const float nl = 1.0f / sqrtf(gx * gx + gy * gy + 1.0f);
        const float nx = -gx * nl, ny = -gy * nl, nz = nl;
        const float dif = 0.85f + 0.15f * (nx * lx + ny * ly + nz * lz) / 0.39f;
        float sp = (nx * hx + ny * hy + nz * hz) * hl; sp = sp < 0 ? 0 : sp;
        sp *= sp; sp *= sp; sp *= sp; sp *= sp;                          // 16乗: ぬれたような照りかえし
        const float lit = nodeL[n] * (1.0f / (8.0f * 256.0f));
        float L = lit * (8.0f * dif + 7.0f * sp);
        nodeL[n] = (int16_t)((L > 16.0f ? 16.0f : L) * 256.0f);
      }
  }
}
static void simPost(float dt) {
  const float half = gHalf;
  // 黒いつやつやの玉: うずの中心にぴたりと置く
  nBall = 0;
  for (int v = 0; v < nVor; ++v) {
    const Vortex& V = vor[v];
    if (V.str < 0.002f || V.hole < 0.01f) continue;
    const float f = V.hole;                                     // 濃さ(いちばん最後に、すっと消える)
    const float r = V.s * 0.36f * half * sqrtf(v == 6 ? V.fade : V.str);   // うねりと一緒に、小さくなる(指の玉は、玉の濃さで)
    if (r < 1.5f) continue;
    ball[nBall++] = {(int16_t)(V.x * half + half), (int16_t)(V.y * half + half), (int16_t)r, (uint8_t)(f * 32.0f + 0.5f)};
  }
  // 光の粒
  for (int i = 0; i < NSPARK; ++i) {
    Spark& p = spark[i];
    p.age += dt;
    if (p.age >= p.life) { respawn(p); continue; }
    const float a = (frand() - 0.5f) * 6.0f * dt;     // 向きを少しずつ、ランダムに変える
    const float c = cosf(a), sn = sinf(a), vx = p.vx * c - p.vy * sn, vy = p.vx * sn + p.vy * c;
    p.vx = vx; p.vy = vy;
    p.x += p.vx * dt; p.y += p.vy * dt;
  }
}

static void simulate(float dt) {                   // 1つのコアで全部(はじめの1回など)
  simPre(dt); simRows(0, NG); simGloss(0, NG); simPost(dt);
}
static void simulateMT(float dt) {                 // 2つのコアで: 格子とつやを、上半分(コア1)と下半分(コア0)に分ける
  const uint32_t t0 = micros();
  simPre(dt);
  const int jm = NG / 2;
  job = 1; xSemaphoreGive(semGo); simRows(0, jm); xSemaphoreTake(semDone, portMAX_DELAY);
  job = 2; xSemaphoreGive(semGo); simGloss(0, jm); xSemaphoreTake(semDone, portMAX_DELAY);
  job = 0;
  simPost(dt);
  simUs = micros() - t0;
}

// ---------------------------------------------------------------- 描く
// 1行を描く(x は fx0 から fx1 まで)
static inline __attribute__((always_inline)) void drawRow(uint16_t* o, int yf, int fx0, int fx1) {
  const int j = yf / CELL, fy = yf - j * CELL;
  const int32_t* U0 = nodeU + j * NG; const int32_t* U1 = U0 + NG;
  const int32_t* V0 = nodeV + j * NG; const int32_t* V1 = V0 + NG;
  const int16_t* L0 = nodeL + j * NG; const int16_t* L1 = L0 + NG;
  int x = fx0;
  while (x <= fx1) {
    const int i = x / CELL, fx = x - i * CELL;
    const int32_t ua = U0[i] + (((U1[i] - U0[i]) * fy) >> 3), ub = U0[i + 1] + (((U1[i + 1] - U0[i + 1]) * fy) >> 3);
    const int32_t va = V0[i] + (((V1[i] - V0[i]) * fy) >> 3), vb = V0[i + 1] + (((V1[i + 1] - V0[i + 1]) * fy) >> 3);
    const int32_t la = L0[i] + (((L1[i] - L0[i]) * fy) >> 3), lb = L0[i + 1] + (((L1[i + 1] - L0[i + 1]) * fy) >> 3);
    const int32_t dU = (ub - ua) >> 3, dV = (vb - va) >> 3, dL = (lb - la) >> 3;
    int32_t u = ua + dU * fx, v = va + dV * fx, l = la + dL * fx;
    const int xe = min(fx1, i * CELL + CELL - 1);
    for (; x <= xe; ++x) {
      const int lv = l >> 8;
      o[x] = pal[(lv < 0 ? 0 : lv > 16 ? 16 : lv) * 256 + tex[((v >> 8) & (TEX - 1)) * TEX + ((u >> 8) & (TEX - 1))]];
      u += dU; v += dV; l += dL;
    }
  }
}
static void IRAM_ATTR renderBand(int k, uint16_t* out, int bx, int bw) {
  const int y0 = k * BAND, h = min(BAND, S - y0);
  for (int r = 0; r < h; ++r) {
    const int y = y0 + r;
    uint16_t* o = out + r * bw - bx;               // o[x] が画面の x
    const int sa = max((int)spanA[y], bx), sb = min((int)spanB[y], bx + bw - 1);
    for (int x = bx; x < sa; ++x) o[x] = 0;
    for (int x = sb + 1 < bx ? bx : sb + 1; x < bx + bw; ++x) o[x] = 0;
    if (sa > sb) continue;
    drawRow(o, y, sa, sb);
  }
  // 黒いつやつやの玉: ふちにだけ、まわりがうっすら映る。左上に光の照りかえし
  for (int b = 0; b < nBall; ++b) {
    const Ball& B = ball[b];
    if (B.y + B.r < y0 || B.y - B.r >= y0 + h) continue;
    const float ir = 1.0f / B.r;
    const float hx = B.x - B.r * 0.36f, hy = B.y - B.r * 0.40f, hr = B.r * 0.24f, ihr = 1.0f / hr;
    const float gx = B.x + B.r * 0.30f, gy = B.y + B.r * 0.34f, gr = B.r * 0.12f, igr = 1.0f / gr;
    for (int yy = max(y0, B.y - B.r); yy <= min(y0 + h - 1, B.y + B.r); ++yy) {
      uint16_t* o = out + (yy - y0) * bw - bx;
      const float dy = (yy - B.y) * ir;
      const float w2 = 1.0f - dy * dy; if (w2 <= 0) continue;
      const int hw = (int)(sqrtf(w2) * B.r);
      const int xa = max(max(bx, (int)spanA[yy]), B.x - hw), xb = min(min(bx + bw - 1, (int)spanB[yy]), B.x + hw);
      for (int x = xa; x <= xb; ++x) {
        const float dx = (x - B.x) * ir, d2 = dx * dx + dy * dy;
        float e = d2 * d2; e *= e;                             // ふちほど映る(d の 8 乗)
        uint32_t a = (uint32_t)(e * 20.0f);
        if (d2 > 0.90f) a = (uint32_t)(a + (d2 - 0.90f) * 120.0f);   // いちばん外はなめらかに
        uint16_t c = mix565(0, o[x], a > 32 ? 32 : a);
        const float sx = (x - hx) * ihr, sy = (yy - hy) * ihr, s2 = sx * sx + sy * sy;
        if (s2 < 1.0f) { const float f = 1.0f - s2; c = mix565(c, 0xFFFF, (uint32_t)(f * f * 30.0f)); }
        const float tx = (x - gx) * igr, ty = (yy - gy) * igr, t2 = tx * tx + ty * ty;
        if (t2 < 1.0f) { const float f = 1.0f - t2; c = mix565(c, 0xFFFF, (uint32_t)(f * f * 14.0f)); }
        o[x] = B.a >= 32 ? c : mix565(o[x], c, B.a);
      }
    }
  }
  // 光の粒: 現れて、ちらちらまたたきながら動き、消える
  const int D = 2 * SPR + 1;
  for (int i = 0; i < NSPARK; ++i) {
    const Spark& p = spark[i];
    const float q = p.age / p.life, env = fsin(q * 3.1415926f);   // ふっと現れて、ふっと消える
    const float s1 = fsin(p.age * p.sp + p.tw), s2 = fsin(p.age * p.sp * 0.37f + p.tw * 2.0f);
    const float tw = 0.35f + 0.65f * s1 * s1 + (s2 > 0.85f ? 0.6f : 0.0f);   // 速くまたたき、ときどきキラッと
    const float gg = env * tw * 32.0f;
    const uint32_t g = (uint32_t)(gg > 40.0f ? 40.0f : gg);
    if (g < 2) continue;
    {
      const int px = (int)p.x, py = (int)p.y;
      if (py + SPR < y0 || py - SPR >= y0 + h) continue;
      for (int yy = max(y0, py - SPR); yy <= min(y0 + h - 1, py + SPR); ++yy) {
        uint16_t* o = out + (yy - y0) * bw - bx;
        const uint8_t* A = sprA + (yy - py + SPR) * D + SPR;
        const int xa = max(max(bx, (int)spanA[yy]), px - SPR), xb = min(min(bx + bw - 1, (int)spanB[yy]), px + SPR);
        for (int x = xa; x <= xb; ++x) { uint32_t a = (A[x - px] * g) >> 5; if (a) o[x] = mix565(o[x], 0xFFFF, a > 32 ? 32 : a); }
      }
    }
  }
}
static void renderFrame() {
  __atomic_store_n(&readyW, -1, __ATOMIC_RELEASE);
  __atomic_store_n(&doneUpTo, -1, __ATOMIC_RELEASE);
  xSemaphoreGive(semGo);
  uint32_t busy = 0, send = 0;
  if (!fbMode) M5.Display.startWrite();
  for (int k = 0; k < NB; ++k) {
    if ((k & 1) == 0) {
      if (bandW[k] > 0) { const uint32_t t0 = micros(); renderBand(k, band[k % NSLOT], bandX[k], bandW[k]); busy += micros() - t0; }
    } else {
      while (__atomic_load_n(&readyW, __ATOMIC_ACQUIRE) < k) { }
    }
    if (!fbMode) { const uint32_t tw = micros(); M5.Display.waitDMA(); send += micros() - tw; }
    __atomic_store_n(&doneUpTo, k - 1, __ATOMIC_RELEASE);
    if (bandW[k] > 0) {
      const int y0 = k * BAND, h = min(BAND, S - y0);
      if (fbMode) { const uint32_t ts = micros(); M5.Display.pushImage(XOFF + bandX[k], YOFF + y0, bandW[k], h, band[k % NSLOT]); send += micros() - ts; }
      else M5.Display.pushImageDMA(XOFF + bandX[k], YOFF + y0, bandW[k], h, band[k % NSLOT]);
    }
  }
  if (!fbMode) M5.Display.waitDMA();
  __atomic_store_n(&doneUpTo, NB, __ATOMIC_RELEASE);
  if (!fbMode) M5.Display.endWrite();
  xSemaphoreTake(semDone, portMAX_DELAY);
  busyUs[1] = busy; sendUs = send;
}
static void workerTask(void*) {                   // コア0: 奇数番目の帯
  for (;;) {
    xSemaphoreTake(semGo, portMAX_DELAY);
    if (job == 1) { simRows(NG / 2, NG); xSemaphoreGive(semDone); continue; }
    if (job == 2) { simGloss(NG / 2, NG); xSemaphoreGive(semDone); continue; }
    uint32_t busy = 0;
    for (int k = 1; k < NB; k += 2) {
      while (__atomic_load_n(&doneUpTo, __ATOMIC_ACQUIRE) < k - NSLOT) { }
      if (bandW[k] > 0) { const uint32_t t0 = micros(); renderBand(k, band[k % NSLOT], bandX[k], bandW[k]); busy += micros() - t0; }
      __atomic_store_n(&readyW, k, __ATOMIC_RELEASE);
    }
    busyUs[0] = busy;
    xSemaphoreGive(semDone);
  }
}

// ---------------------------------------------------------------- 切り替え
static void shuffleVortices() {
  initVortices(true);
  Serial.println("swirls: shuffled");
}
static void setTheme(int th) {
  theme = th;
  buildPalette(theme);
  Serial.printf("color: %s\n", THEME_NAME[theme]);
}

static void fail(const char* msg) {
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setTextColor(0xF800);
  M5.Display.setTextDatum(middle_center);
  M5.Display.drawString(msg, W / 2, H / 2);
  Serial.println(msg);
  while (true) nmIdle();
}

// ---------------------------------------------------------------- setup / loop
void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);
  Serial.begin(115200);
  Serial.println("NullKaleido v12");
#if defined(ESP_PLATFORM)
  if (getCpuFrequencyMhz() < 240) setCpuFrequencyMhz(240);
#endif
  if (M5.Display.width() < M5.Display.height()) M5.Display.setRotation(1);
  W = M5.Display.width(); H = M5.Display.height();
  roundScr = (W == H);
  S = min(W, H) & ~1;
  XOFF = ((W - S) / 2) & ~1; YOFF = (H - S) / 2;
  NB = (S + BAND - 1) / BAND;
  NG = S / CELL + 2;
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setBrightness(200);
  { auto* pnl = M5.Display.getPanel(); fbMode = pnl && pnl->getAutoDisplay();
#if defined(NL_DIRECT_AMOLED)
    if (fbMode && DIRECT_SEND && M5.getBoard() == m5::board_t::board_M5StopWatch) {
      lgfx::Panel_AMOLED* inner = NlFbPeek::inner(pnl);
      if (inner) { M5.Display.setPanel(inner); fbMode = false; directSend = true; }
    }
#endif
  }
  Serial.printf("display: %s\n", fbMode ? "framebuffer" : directSend ? "direct DMA (StopWatch, no framebuffer)" : "direct DMA");

  spanA = (int16_t*)malloc(S * 2); spanB = (int16_t*)malloc(S * 2);
  bandX = (int16_t*)malloc(NB * 2); bandW = (int16_t*)malloc(NB * 2);
  for (int i = 0; i < NSLOT; ++i) band[i] = (uint16_t*)heap_caps_malloc((size_t)S * BAND * 2, MALLOC_CAP_DMA);
  tex = (uint8_t*)imalloc(TEX * TEX);
  pal = (uint16_t*)imalloc(NLEV * 256 * 2);
  nodeU = (int32_t*)imalloc(NG * NG * 4); nodeV = (int32_t*)imalloc(NG * NG * 4); nodeL = (int16_t*)imalloc(NG * NG * 2); nodeH = (float*)imalloc(NG * NG * 4);
  if (!spanA || !spanB || !bandX || !bandW || !band[0] || !band[1] || !band[2] || !band[3] || !tex || !pal || !nodeU || !nodeV || !nodeL || !nodeH || NG > MAXG)
    fail("Memory error");
  const float c = (S - 1) * 0.5f, rr = S * 0.5f + 1.0f;
  for (int y = 0; y < S; ++y) {
    if (!roundScr) { spanA[y] = 0; spanB[y] = S - 1; continue; }
    const float dy = y - c, w = rr * rr - dy * dy;
    if (w <= 0) { spanA[y] = 1; spanB[y] = 0; continue; }
    const float s = sqrtf(w);
    spanA[y] = (int16_t)max(0, (int)floorf(c - s)); spanB[y] = (int16_t)min(S - 1, (int)ceilf(c + s));
  }
  for (int b = 0; b < NB; ++b) {
    int x0 = S, x1 = -1;
    for (int y = b * BAND; y < min(S, (b + 1) * BAND); ++y) if (spanA[y] <= spanB[y]) { x0 = min(x0, (int)spanA[y]); x1 = max(x1, (int)spanB[y]); }
    if (x1 < x0) { bandX[b] = 0; bandW[b] = 0; continue; }
    x0 &= ~1; x1 |= 1; if (x1 > S - 1) x1 = S - 1;
    bandX[b] = (int16_t)x0; bandW[b] = (int16_t)((x1 - x0 + 1) & ~1);
  }
  for (int i = 0; i < 1024; ++i) sinT[i] = sinf(i * 6.2831853f / 1024.0f);

  M5.Display.setTextDatum(middle_center);
  M5.Display.setFont(&fonts::lgfxJapanGothicP_16);
  M5.Display.setTextColor(0x8410);
  M5.Display.drawString("模様を用意しています…", W / 2, H / 2);
  buildTexture();
  buildSprite();
  rnd = (uint32_t)esp_random() | 1u;
  initVortices(false);
  setTheme(0);
  simulate(0.0f);

  semGo = xSemaphoreCreateBinary();
  semDone = xSemaphoreCreateBinary();
  if (!semGo || !semDone) fail("Memory error (task)");
  xTaskCreatePinnedToCore(workerTask, "kal", 6144, nullptr, 1, nullptr, 0);   // コア0 で帯を描く
}

void loop() {
  static uint32_t last = millis(), fpsT = millis(), lastTapMs = 0;
  static bool wasTouch = false;
  static int touchUps = 0;
  static int frames = 0;
  M5.update();
  auto t = M5.Touch.getDetail();
  { static bool pr = false; if (pr && !t.isPressed()) ++touchUps; pr = t.isPressed(); }   // タッチが離れた(とぎれた)回数
  const uint32_t now = millis();
  const bool corner = !roundScr && t.x < 56 && t.y < 56;   // 左上のすみはメニューに戻る所なので使わない
  if (roundScr) {
    if (M5.BtnA.wasClicked()) setTheme((theme + 1) % NTHEME);
    if (M5.BtnB.wasClicked()) shuffleVortices();
  } else {
    if (M5.BtnPWR.wasClicked()) setTheme((theme + 1) % NTHEME);
    if (t.wasPressed() && !corner) {
      if (now - lastTapMs < 350) { shuffleVortices(); lastTapMs = 0; } else lastTapMs = now;
    }
  }
  float dt = (now - last) / 1000.0f; last = now;
  if (dt > 0.1f) dt = 0.1f;

  // 触る: 触った所にうず。タッチが一瞬とぎれても(0.15秒まで)、さわり続けているものとして扱う
  static uint32_t lastTouchMs = 0;
  static int lastTX = 0, lastTY = 0;
  if (t.isPressed() && !corner) { lastTouchMs = now; lastTX = t.x; lastTY = t.y; }
  const bool touching = lastTouchMs != 0 && now - lastTouchMs < 150;
  const int tx = lastTX - XOFF, ty = lastTY - YOFF;
  if (touching && tx >= 0 && ty >= 0 && tx < S && ty < S) {
    fingerX = (tx - S * 0.5f) / (S * 0.5f); fingerY = (ty - S * 0.5f) / (S * 0.5f);
    bool onBall = false;
    for (int i = 0; i < 6; ++i) {                    // 触った所に玉があれば、その玉をやんわり消す
      Vortex& v = vor[i];
      if (v.wait > 0 || v.str < 0.05f) continue;
      const float dx = fingerX - v.x, dy = fingerY - v.y, rr = v.s * 0.36f * sqrtf(v.str) * 1.5f + 0.03f;   // 少し大きめに、触りやすく
      if (dx * dx + dy * dy < rr * rr) {
        if (v.age < v.life * 0.30f) v.age = v.life * 0.30f;   // まだ大きくなっているときは、小さくなるところから
        v.spd = 2.2f;                                            // いつもの2倍あまりの速さで、やんわり小さく
        onBall = true;
      }
    }
    fingerBall = !onBall;                            // 玉の上にいるあいだは、指の玉は出さない(離れたら、また出す)
    fingerK += (7.0f * SWIRL - fingerK) * min(1.0f, dt * 2.5f);
  } else {
    fingerK -= fingerK * min(1.0f, dt * 0.8f);
    fingerBall = false;
  }
  wasTouch = touching;

  simulateMT(dt);
  renderFrame();
  if (++frames, now - fpsT >= 3000) {
    Serial.printf("fps: %.1f  (%s)  sim %.1f ms  core0 %.1f ms  core1 %.1f ms  send %.1f ms  touch-ups %d\n", frames * 1000.0f / (now - fpsT), THEME_NAME[theme],
                  simUs / 1000.0f, busyUs[0] / 1000.0f, busyUs[1] / 1000.0f, sendUs / 1000.0f, touchUps);
    touchUps = 0;
    frames = 0; fpsT = now;
  }
}


// 共通の置き場に置いたものは、0 で始まる単純な型だけ(コンストラクタが要らないもの)
static_assert(std::is_trivially_default_constructible<Ball>::value, "NM_ARENA: Ball");
static_assert(std::is_trivially_default_constructible<Spark>::value, "NM_ARENA: Spark");
static_assert(std::is_trivially_default_constructible<Vortex>::value, "NM_ARENA: Vortex");
static_assert(std::is_trivially_default_constructible<bool>::value, "NM_ARENA: bool");
static_assert(std::is_trivially_default_constructible<float>::value, "NM_ARENA: float");
static_assert(std::is_trivially_default_constructible<int>::value, "NM_ARENA: int");
static constexpr size_t OVL_END = nmAlignTo(nmOv26, 16);
static_assert(OVL_END <= NM_ARENA_SIZE, "NM_ARENA_SIZE (nm_common.h) を大きくしてください");
extern const size_t OVL_SIZE = OVL_END;   // 起動時の確認用
}  // namespace NullKaleido
