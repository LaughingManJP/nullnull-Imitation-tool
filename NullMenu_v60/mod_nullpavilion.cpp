// NullPavilion  -  NullMenu 用モジュール(元のスケッチ NullPavilion_v6 を名前空間に入れたもの)
// 0 で始まる変数(配列も)は、ほかの作品と共通の置き場(NM_ARENA)に置いています。
// 一度に動くのは1つだけ(切り替えるときは再起動)なので、重なっていても問題ありません。
#include "nm_common.h"
#if defined(ESP_PLATFORM) && __has_include(<lgfx/v1/panel/Panel_AMOLED.hpp>)
#include <lgfx/v1/panel/Panel_AMOLED.hpp>
#endif
#pragma GCC optimize("O3")

namespace NullPavilion {
static constexpr size_t nmOv0 = 0;

// NullPavilion v6  -  鏡のパビリオン。空と広場を映す鏡が、ゆっくり波打つ。見え方を3つから選べる
//   大阪・関西万博のパビリオン「null²」を見たイメージから作った、非公式の習作です。作品の映像・名前・データは使っていません。
//   M5Stack StopWatch 用(丸い 466x466 画面)。CoreS3 系でも真ん中に正方形で動きます。
//
// 画面の中(見え方は3つ):
//   ・鏡の膜   … 画面いっぱいの鏡の壁。ふくらみがゆっくり動いて、映った空と広場がぐにゃっとゆがむ
//   ・鏡の建物 … 鏡の箱を積んだ建物を、斜め下から見上げる。面がそれぞれ空を映しながら、うねる
//   ・鏡の水面 … 水銀のような鏡の水面をのぞき込む。ときどき雫が落ちて、波紋が広がる
//   空は 昼 / 夕暮れ / 夜 / くもり の4つ。雲はゆっくり流れる
//
// 触ると:
//   ・鏡の膜   … 押すと膜がぬるっとへこみ、離すとぷるんと揺れもどる。タップすると波が広がる
//   ・鏡の建物 … 触った箱の面が、大きくうねる
//   ・鏡の水面 … 触った所から波紋が広がる(なぞると、なぞった所から次々に)
//
// 操作:
//   StopWatch … ボタンA: 見え方を変える(膜 → 建物 → 水面)  ボタンB: 空の時間帯(昼 / 夕暮れ / 夜 / くもり)
//   CoreS3系  … ダブルタップ: 見え方を変える                   電源ボタン短押し: 空の時間帯
//
// 必要なもの:
//   ボードマネージャー M5Stack >= 3.3.7 / ボード: M5StopWatch(CoreS3 なら M5CoreS3)
//   ライブラリ M5Unified >= 0.2.15, M5GFX >= 0.2.21
//
// しくみ:
//   画面の1点ごとに「目からの向き」と「鏡のどこに当たるか」を、見え方を変えたときに1回だけ計算しておく(PSRAM)。
//   コマごとには、鏡の面のかたむき(ふくらみ・波)だけを計算し、鏡で跳ね返った向きの景色を表から引く。
//   景色の表は、空と広場を「上半分 / 下半分」の2枚の丸い絵(放物面の写像)にしたもの。雲は別の絵を流して重ねる。
//   色の計算は縦横1つおきの点だけでして、あいだは平均でうめる(鏡の映り込みはなめらかなので、見た目はほぼ変わらない)。
//   画面を16行ずつの帯に分けて、2つのコアで1本おきに描く。

#ifndef IRAM_ATTR
#define IRAM_ATTR
#endif


// Arduino IDE は関数の宣言を自動で先頭近くに足すので、型の名前だけ先に宣言しておく
struct Pix;
struct Face;

// ---- 調整用 ----------------------------------------------------------------
static constexpr float CLOUD_SPEED = 2.0f;    // 雲が流れる速さ
static constexpr float MEMBRANE    = 0.22f;   // 鏡の膜のふくらみの大きさ
static constexpr float PRESS       = 0.55f;   // 押したときのへこみの深さ
static constexpr float BOX_WAVE    = 1.0f;    // 鏡の建物の面のうねりの大きさ
static constexpr float WATER       = 1.0f;    // 水面の波の大きさ
static constexpr float RAIN_SEC    = 2.6f;    // 水面に雫が落ちる間隔(秒、だいたい)
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
static constexpr int EM = 128;                    // 景色の表の大きさ(EM x EM、上半分と下半分で2枚)
static constexpr int CM = 256;                    // 雲の絵の大きさ(CM x CM。空の表より細かく)
static constexpr int GA = 48;                     // 鏡の膜の高さの格子
static constexpr int GC = 72;                     // 水面の高さの格子
static constexpr int MAXRIP = 8;                  // 同時に広がる波紋の数
static constexpr int NBOX = 6;
static constexpr int NFACE = NBOX * 6;

static constexpr size_t nmOa0 = nmAlignTo(nmOv0, alignof(int)), nmOv1 = nmOa0 + sizeof(int); static int& W = *reinterpret_cast<int*>(NM_ARENA + nmOa0);
static constexpr size_t nmOa1 = nmAlignTo(nmOv1, alignof(int)), nmOv2 = nmOa1 + sizeof(int); static int& H = *reinterpret_cast<int*>(NM_ARENA + nmOa1);
static constexpr size_t nmOa2 = nmAlignTo(nmOv2, alignof(int)), nmOv3 = nmOa2 + sizeof(int); static int& S = *reinterpret_cast<int*>(NM_ARENA + nmOa2);
static constexpr size_t nmOa3 = nmAlignTo(nmOv3, alignof(int)), nmOv4 = nmOa3 + sizeof(int); static int& XOFF = *reinterpret_cast<int*>(NM_ARENA + nmOa3);
static constexpr size_t nmOa4 = nmAlignTo(nmOv4, alignof(int)), nmOv5 = nmOa4 + sizeof(int); static int& YOFF = *reinterpret_cast<int*>(NM_ARENA + nmOa4);
static constexpr size_t nmOa5 = nmAlignTo(nmOv5, alignof(int)), nmOv6 = nmOa5 + sizeof(int); static int& NB = *reinterpret_cast<int*>(NM_ARENA + nmOa5);
static bool roundScr = true;
static constexpr size_t nmOa6 = nmAlignTo(nmOv6, alignof(bool)), nmOv7 = nmOa6 + sizeof(bool); static bool& fbMode = *reinterpret_cast<bool*>(NM_ARENA + nmOa6);
static constexpr size_t nmOa7 = nmAlignTo(nmOv7, alignof(bool)), nmOv8 = nmOa7 + sizeof(bool); static bool& directSend = *reinterpret_cast<bool*>(NM_ARENA + nmOa7);
static int16_t* spanA = nullptr;
static int16_t* spanB = nullptr;
static int16_t* bandX = nullptr;
static int16_t* bandW = nullptr;
static uint16_t* band[NSLOT] = {nullptr, nullptr, nullptr, nullptr};

// 画面の1点ぶんの、見え方ごとに決まっている値
struct Pix { int16_t a, b; int8_t dx, dy, dz; uint8_t f; };
//   鏡の膜:   a, b = 高さの格子の上の位置(8.8)   f = 使わない
//   鏡の建物: a, b = 面の上の位置(-1024..1024)  f = 面の番号(255 = 空か広場)
//   鏡の水面: a, b = 高さの格子の上の位置(8.8)   f = 遠くのかすみ(0..32、255 = 空)
static Pix* pix = nullptr;                        // [S * S] PSRAM

// 景色
static uint16_t* skyMap = nullptr;                // 上半分(空) [EM * EM]
static uint16_t* gndMap = nullptr;                // 下半分(広場) [EM * EM]
static uint8_t*  skyFade = nullptr;               // 空の各点で、雲をどれだけ見せるか(地平線の近くは薄く) 0..32
static uint8_t*  cloudT = nullptr;                // 雲の濃さ [CM * CM] 0..32(横にも縦にもつながっている)
static constexpr size_t nmOa8 = nmAlignTo(nmOv8, 16), nmOv9 = nmOa8 + sizeof(uint8_t[33]); static uint8_t (&cloudMap)[33] = *reinterpret_cast<uint8_t(*)[33]>(NM_ARENA + nmOa8);  // 時間帯ごとの雲の濃さの読みかえ
static constexpr size_t nmOa9 = nmAlignTo(nmOv9, alignof(uint16_t)), nmOv10 = nmOa9 + sizeof(uint16_t); static uint16_t& cloudCol = *reinterpret_cast<uint16_t*>(NM_ARENA + nmOa9);
static constexpr size_t nmOa10 = nmAlignTo(nmOv10, alignof(uint16_t)), nmOv11 = nmOa10 + sizeof(uint16_t); static uint16_t& hazeCol = *reinterpret_cast<uint16_t*>(NM_ARENA + nmOa10);
static constexpr size_t nmOa11 = nmAlignTo(nmOv11, alignof(uint16_t)), nmOv12 = nmOa11 + sizeof(uint16_t); static uint16_t& seamCol = *reinterpret_cast<uint16_t*>(NM_ARENA + nmOa11);
static constexpr size_t nmOa12 = nmAlignTo(nmOv12, alignof(int)), nmOv13 = nmOa12 + sizeof(int); static int& cloudU = *reinterpret_cast<int*>(NM_ARENA + nmOa12);
static constexpr size_t nmOa13 = nmAlignTo(nmOv13, alignof(int)), nmOv14 = nmOa13 + sizeof(int); static int& cloudV = *reinterpret_cast<int*>(NM_ARENA + nmOa13);  // 雲の流れた量(点)
static constexpr size_t nmOa14 = nmAlignTo(nmOv14, alignof(float)), nmOv15 = nmOa14 + sizeof(float); static float& cloudPos = *reinterpret_cast<float*>(NM_ARENA + nmOa14);
static constexpr size_t nmOa15 = nmAlignTo(nmOv15, 16), nmOv16 = nmOa15 + sizeof(float[260]); static float (&recipT)[260] = *reinterpret_cast<float(*)[260]>(NM_ARENA + nmOa15);  // 1 / (1 + i/256)
static constexpr size_t nmOa16 = nmAlignTo(nmOv16, 16), nmOv17 = nmOa16 + sizeof(float[1024]); static float (&sinT)[1024] = *reinterpret_cast<float(*)[1024]>(NM_ARENA + nmOa16);
static constexpr size_t nmOa17 = nmAlignTo(nmOv17, 16), nmOv18 = nmOa17 + sizeof(float[513]); static float (&expT)[513] = *reinterpret_cast<float(*)[513]>(NM_ARENA + nmOa17);  // exp(-i / 32)  (0 .. 16)

// 動き
static float* grid = nullptr;                     // 高さ(作業用) [GC * GC]
static float* grad = nullptr;                     // かたむき(x, y の組) [GC * GC * 2]
static int gN = GA;  // いま使っている格子の大きさ
static constexpr size_t nmOa18 = nmAlignTo(nmOv18, alignof(float)), nmOv19 = nmOa18 + sizeof(float); static float& simT = *reinterpret_cast<float*>(NM_ARENA + nmOa18);
static constexpr size_t nmOa19 = nmAlignTo(nmOv19, alignof(int)), nmOv20 = nmOa19 + sizeof(int); static int& view = *reinterpret_cast<int*>(NM_ARENA + nmOa19);
static constexpr size_t nmOa20 = nmAlignTo(nmOv20, alignof(int)), nmOv21 = nmOa20 + sizeof(int); static int& theme = *reinterpret_cast<int*>(NM_ARENA + nmOa20);

// 鏡の膜: 指のへこみ
static float dentX = 24, dentY = 24, dentTX = 24, dentTY = 24;
static constexpr size_t nmOa21 = nmAlignTo(nmOv21, alignof(float)), nmOv22 = nmOa21 + sizeof(float); static float& dentA = *reinterpret_cast<float*>(NM_ARENA + nmOa21);
static constexpr size_t nmOa22 = nmAlignTo(nmOv22, alignof(float)), nmOv23 = nmOa22 + sizeof(float); static float& dentV = *reinterpret_cast<float*>(NM_ARENA + nmOa22);
// 波紋(膜と水面で使う)
struct Rip { float x, y, age, amp; };
static constexpr size_t nmOa23 = nmAlignTo(nmOv23, 16), nmOv24 = nmOa23 + sizeof(Rip[MAXRIP]); static Rip (&rip)[MAXRIP] = *reinterpret_cast<Rip(*)[MAXRIP]>(NM_ARENA + nmOa23);
static constexpr size_t nmOa24 = nmAlignTo(nmOv24, alignof(int)), nmOv25 = nmOa24 + sizeof(int); static int& nRip = *reinterpret_cast<int*>(NM_ARENA + nmOa24);
static float rainT = 1.0f;
// 鏡の建物
struct Face { float N[3], T[3], B[3]; uint8_t box; };
static constexpr size_t nmOa25 = nmAlignTo(nmOv25, 16), nmOv26 = nmOa25 + sizeof(Face[NFACE]); static Face (&faces)[NFACE] = *reinterpret_cast<Face(*)[NFACE]>(NM_ARENA + nmOa25);
static float boxAmp[NBOX] = {0, 0, 0, 0, 0, 0};

// 2つのコアで描く
static SemaphoreHandle_t semGo = nullptr, semDone = nullptr;
static volatile int readyW = -1, doneUpTo = -1;
static uint32_t busyUs[2] = {0, 0}, sendUs = 0;

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
static inline __attribute__((always_inline)) float fsin(float x) { return sinT[(int)(x * (1024.0f / 6.2831853f)) & 1023]; }
static inline __attribute__((always_inline)) float fcos(float x) { return sinT[((int)(x * (1024.0f / 6.2831853f)) + 256) & 1023]; }
static inline float fexp(float x) {               // exp(-x)(x >= 0。表を直線でつなぐ)
  if (x >= 15.99f) return 0.0f;
  const float f = x * 32.0f; const int i = (int)f; const float t = f - i;
  return expT[i] + (expT[i + 1] - expT[i]) * t;
}
static void* imalloc(size_t n) {                  // 内蔵メモリに(足りなければ、どこでも)
  void* p = heap_caps_malloc(n, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  return p ? p : malloc(n);
}

// ---------------------------------------------------------------- 景色(時間帯)
struct Sky {
  float zen[3], hor[3], gnd[3], gndFar[3], sun[3], sunDir[3], cloud[3], skyline[3];
  float cloudAmt, stars, lamps;
};
static const Sky SKIES[] = {
  // 昼
  {{0.16f,0.40f,0.85f},{0.82f,0.89f,0.97f},{0.66f,0.66f,0.68f},{0.52f,0.54f,0.58f},{1.0f,0.96f,0.85f},{0.50f,0.45f,0.75f},{1.0f,1.0f,1.0f},{0.48f,0.55f,0.62f}, 0.80f, 0, 0},
  // 夕暮れ
  {{0.14f,0.11f,0.36f},{1.00f,0.58f,0.38f},{0.42f,0.32f,0.34f},{0.30f,0.22f,0.28f},{1.0f,0.80f,0.50f},{-0.40f,0.12f,0.90f},{1.0f,0.66f,0.58f},{0.30f,0.20f,0.28f}, 0.75f, 0, 0.3f},
  // 夜
  {{0.01f,0.02f,0.08f},{0.08f,0.10f,0.24f},{0.10f,0.10f,0.14f},{0.05f,0.05f,0.09f},{0.85f,0.88f,1.0f},{0.30f,0.50f,0.80f},{0.18f,0.20f,0.32f},{0.04f,0.05f,0.09f}, 0.45f, 1, 1},
  // くもり
  {{0.58f,0.62f,0.68f},{0.88f,0.90f,0.93f},{0.66f,0.66f,0.67f},{0.50f,0.51f,0.53f},{1.0f,1.0f,1.0f},{0.20f,0.80f,0.40f},{0.95f,0.95f,0.96f},{0.52f,0.55f,0.58f}, 1.00f, 0, 0},
};
static constexpr int NSKY = sizeof(SKIES) / sizeof(SKIES[0]);
static const char* const SKY_NAME[NSKY] = {"day", "sunset", "night", "overcast"};

static void buildClouds() {
  for (int y = 0; y < CM; ++y)
    for (int x = 0; x < CM; ++x) {
      float s = 0, a = 0.5f; int per = 6;
      for (int o = 0; o < 5; ++o) {                 // それぞれの細かさで、CM の幅でちょうどくり返す
        s += a * vnoise(x * (float)per / CM, y * (float)per / CM, per); a *= 0.5f; per *= 2;
      }
      float v = (s - 0.42f) * 3.2f; v = v < 0 ? 0 : v > 1 ? 1 : v;
      cloudT[y * CM + x] = (uint8_t)(v * 32.0f + 0.5f);
    }
}
static void buildSky(int th) {
  const Sky& K = SKIES[th];
  float sl = sqrtf(K.sunDir[0] * K.sunDir[0] + K.sunDir[1] * K.sunDir[1] + K.sunDir[2] * K.sunDir[2]);
  const float sx = K.sunDir[0] / sl, sy = K.sunDir[1] / sl, sz = K.sunDir[2] / sl;
  for (int iv = 0; iv < EM; ++iv)
    for (int iu = 0; iu < EM; ++iu) {
      float u = (iu - 63.5f) / 63.5f, v = (iv - 63.5f) / 63.5f, rr = u * u + v * v;
      if (rr > 1.0f) { const float k = 1.0f / sqrtf(rr); u *= k; v *= k; rr = 1.0f; }
      const float inv = 1.0f / (1.0f + rr);
      const float dx = 2 * u * inv, dz = 2 * v * inv, dyU = (1 - rr) * inv;
      // ---- 空(上半分)
      {
        const float y = dyU, t = sqrtf(y);
        float r = K.hor[0] * (1 - t) + K.zen[0] * t, g = K.hor[1] * (1 - t) + K.zen[1] * t, b = K.hor[2] * (1 - t) + K.zen[2] * t;
        const float sd = dx * sx + y * sy + dz * sz;
        if (sd > 0) {
          const float glow = powf(sd, 12.0f) * 0.35f + powf(sd, 900.0f) * 8.0f;
          r += K.sun[0] * glow; g += K.sun[1] * glow; b += K.sun[2] * glow;
        }
        if (K.stars > 0) {
          const uint32_t h = hash32((uint32_t)iu * 7919u + (uint32_t)iv * 104729u);
          if ((h & 1023) < 5 && y > 0.15f) { const float s = 0.12f + ((h >> 10) & 255) / 1100.0f; r += s; g += s; b += s * 1.1f; }
        }
        // 遠くの建物と木(地平線のすぐ上)
        const float ang = atan2f(dx, dz);
        const float ridge = 0.012f + 0.028f * vnoise(ang * 3.0f + 20.0f, 3.0f, 64) + 0.012f * (vnoise(ang * 14.0f, 7.0f, 256) > 0.6f ? 1.0f : 0.0f);
        if (y < ridge + 0.006f) {                    // ふちは少しぼかす
          float m = (ridge + 0.006f - y) / 0.012f; m = m > 1 ? 1 : m;
          const float sr = K.skyline[0] * 0.6f + K.hor[0] * 0.4f, sg = K.skyline[1] * 0.6f + K.hor[1] * 0.4f, sb = K.skyline[2] * 0.6f + K.hor[2] * 0.4f;
          r = r * (1 - m) + sr * m; g = g * (1 - m) + sg * m; b = b * (1 - m) + sb * m;
          if (K.lamps > 0 && (hash32((uint32_t)iu * 31u + (uint32_t)iv * 977u) & 31) == 0) { r += 0.9f * K.lamps; g += 0.65f * K.lamps; b += 0.3f * K.lamps; }
        }
        skyMap[iv * EM + iu] = toPix(r, g, b);
        float fade = (y - 0.02f) * 5.0f; fade = fade < 0 ? 0 : fade > 1 ? 1 : fade;
        if (y < ridge) fade = 0;
        skyFade[iv * EM + iu] = (uint8_t)(fade * 32.0f + 0.5f);
      }
      // ---- 広場(下半分)
      {
        const float y = -dyU;                       // 下向き
        const float k = 1.0f / (dyU > 0.02f ? dyU : 0.02f);
        const float gu = dx * k * 1.2f, gv = dz * k * 1.2f;
        float near = (-y) * 3.0f; near = near > 1 ? 1 : near;
        float r = K.gndFar[0] * (1 - near) + K.gnd[0] * near, g = K.gndFar[1] * (1 - near) + K.gnd[1] * near, b = K.gndFar[2] * (1 - near) + K.gnd[2] * near;
        float lineAmt = (-y - 0.35f) * 3.0f; lineAmt = lineAmt < 0 ? 0 : lineAmt > 1 ? 1 : lineAmt;
        const float fu = gu - floorf(gu), fv = gv - floorf(gv);
        if (fu < 0.03f || fu > 0.97f || fv < 0.03f || fv > 0.97f) { r *= 1 - 0.35f * lineAmt; g *= 1 - 0.35f * lineAmt; b *= 1 - 0.35f * lineAmt; }
        if (K.lamps > 0) {                          // 夜: 広場のあかり
          const float lu = gu * 0.25f, lv = gv * 0.25f;
          const float cu = lu - floorf(lu) - 0.5f, cv = lv - floorf(lv) - 0.5f;
          const float d2 = cu * cu + cv * cv;
          const float l = expf(-d2 * 60.0f) * K.lamps * 0.6f;
          r += l * 1.0f; g += l * 0.75f; b += l * 0.45f;
        }
        float haze = 1.0f - (-y) * 5.0f; haze = haze < 0 ? 0 : haze;
        r = r * (1 - haze) + K.hor[0] * haze * 0.9f; g = g * (1 - haze) + K.hor[1] * haze * 0.9f; b = b * (1 - haze) + K.hor[2] * haze * 0.9f;
        gndMap[iv * EM + iu] = toPix(r, g, b);
      }
    }
  for (int i = 0; i <= 32; ++i) { float a = i * K.cloudAmt; cloudMap[i] = (uint8_t)(a > 32 ? 32 : a); }
  cloudCol = toPix(K.cloud[0], K.cloud[1], K.cloud[2]);
  hazeCol = toPix(K.hor[0] * 0.95f, K.hor[1] * 0.95f, K.hor[2] * 0.95f);
  seamCol = toPix(K.zen[0] * 0.15f, K.zen[1] * 0.15f, K.zen[2] * 0.18f);
}

// 向き (rx, ry, rz)(長さはだいたい 1)の景色
static inline __attribute__((always_inline)) uint16_t envLook(float rx, float ry, float rz) {
  if (ry >= 0.0f) {
    const float inv = recipT[ry < 1.0f ? (int)(ry * 256.0f) : 256];
    const float u = rx * inv, v = rz * inv;
    int iu = (int)(u * 63.5f + 64.0f), iv = (int)(v * 63.5f + 64.0f);
    iu = iu < 0 ? 0 : iu > EM - 1 ? EM - 1 : iu; iv = iv < 0 ? 0 : iv > EM - 1 ? EM - 1 : iv;
    const int i = iv * EM + iu;
    const uint16_t c = skyMap[i];
    const uint32_t fd = skyFade[i];
    if (!fd) return c;
    const int cu = ((int)(u * 127.5f + 128.0f) + cloudU) & (CM - 1), cv = ((int)(v * 127.5f + 128.0f) + cloudV) & (CM - 1);
    const uint32_t d = cloudMap[(cloudT[cv * CM + cu] * fd) >> 5];
    return d ? mix565(c, cloudCol, d) : c;
  } else {
    const float my = -ry;
    const float inv = recipT[(int)((my > 1.0f ? 1.0f : my) * 256.0f)];
    int iu = (int)(rx * inv * 63.5f + 64.0f), iv = (int)(rz * inv * 63.5f + 64.0f);
    iu = iu < 0 ? 0 : iu > EM - 1 ? EM - 1 : iu; iv = iv < 0 ? 0 : iv > EM - 1 ? EM - 1 : iv;
    return gndMap[iv * EM + iu];
  }
}

// ---------------------------------------------------------------- 見え方ごとの、1点ずつの下ごしらえ
static inline void putDir(Pix& p, float dx, float dy, float dz) {
  p.dx = (int8_t)lrintf(dx * 127.0f); p.dy = (int8_t)lrintf(dy * 127.0f); p.dz = (int8_t)lrintf(dz * 127.0f);
}
// 鏡の建物: 箱(中心と、半分の大きさ)
static const float BOXES[NBOX][6] = {
  {0.0f, 1.6f, 0.0f, 1.7f, 1.6f, 1.7f}, {-1.1f, 3.9f, 0.4f, 0.8f, 0.8f, 0.8f}, {1.2f, 3.6f, -0.3f, 0.65f, 0.65f, 0.65f},
  {0.1f, 4.9f, 0.3f, 0.45f, 0.45f, 0.45f}, {2.3f, 0.75f, -1.0f, 0.75f, 0.75f, 0.75f}, {-2.3f, 0.6f, -0.8f, 0.6f, 0.6f, 0.6f},
};
static void buildFaces() {
  for (int b = 0; b < NBOX; ++b)
    for (int a = 0; a < 3; ++a)
      for (int sg = 0; sg < 2; ++sg) {
        Face& F = faces[b * 6 + a * 2 + sg];
        F.box = (uint8_t)b;
        for (int k = 0; k < 3; ++k) F.N[k] = F.T[k] = F.B[k] = 0;
        F.N[a] = sg ? 1.0f : -1.0f;
        const int o0 = (a + 1) % 3, o1 = (a + 2) % 3;
        const int u = a == 1 ? 0 : (o0 < o1 ? o0 : o1), v = a == 1 ? 2 : (o0 < o1 ? o1 : o0);
        F.T[u] = 1.0f; F.B[v] = 1.0f;
      }
}
static void buildView(int vw) {
  const float half = S * 0.5f;
  // 鏡の建物のカメラ(斜めから見上げる)
  const float ca = cosf(0.6f), sa = sinf(0.6f);
  const float camB[3] = {0.0f * ca + (-7.0f) * sa, 0.35f, -0.0f * sa + (-7.0f) * ca};
  for (int y = 0; y < S; ++y) {
    const float py = (half - (y + 0.5f)) / half;
    for (int x = 0; x < S; ++x) {
      Pix& p = pix[y * S + x];
      if (spanA[y] > x || spanB[y] < x) { p.a = p.b = 0; p.dx = p.dy = p.dz = 0; p.f = 255; continue; }
      const float px = (x + 0.5f - half) / half;
      if (vw == 0) {                                  // 鏡の膜: 少し見上げた正面の壁(z = 4)
        float dx = px * 0.9f, dy = py * 0.9f + 0.45f, dz = 1.0f;
        const float l = 1.0f / sqrtf(dx * dx + dy * dy + dz * dz); dx *= l; dy *= l; dz *= l;
        putDir(p, dx, dy, dz);
        const float X = dx / dz * 4.0f, Y = dy / dz * 4.0f;
        float gx = (X + 4.0f) / 8.0f * (GA - 1), gy = (Y + 2.0f) / 8.0f * (GA - 1);
        gx = gx < 0 ? 0 : gx > GA - 1.01f ? GA - 1.01f : gx; gy = gy < 0 ? 0 : gy > GA - 1.01f ? GA - 1.01f : gy;
        p.a = (int16_t)(gx * 256.0f); p.b = (int16_t)(gy * 256.0f);
        p.f = 0;
      } else if (vw == 1) {                           // 鏡の建物
        float dx = px * 0.8f, dy = py * 0.8f + 0.40f, dz = 1.0f;
        const float l = 1.0f / sqrtf(dx * dx + dy * dy + dz * dz); dx *= l; dy *= l; dz *= l;
        const float rx = dx * ca + dz * sa, rz = -dx * sa + dz * ca;
        dx = rx; dz = rz;
        putDir(p, dx, dy, dz);
        float best = 1e9f; int bf = 255; float bu = 0, bv = 0;
        const float dd[3] = {dx, dy, dz};
        const float id[3] = {1.0f / (fabsf(dx) < 1e-6f ? 1e-6f : dx), 1.0f / (fabsf(dy) < 1e-6f ? 1e-6f : dy), 1.0f / (fabsf(dz) < 1e-6f ? 1e-6f : dz)};
        for (int b = 0; b < NBOX; ++b) {
          const float* B = BOXES[b];
          float tn = -1e9f, tf = 1e9f; int ax = 0;
          for (int a = 0; a < 3; ++a) {
            float t1 = (B[a] - B[3 + a] - camB[a]) * id[a], t2 = (B[a] + B[3 + a] - camB[a]) * id[a];
            if (t1 > t2) { const float t = t1; t1 = t2; t2 = t; }
            if (t1 > tn) { tn = t1; ax = a; }
            if (t2 < tf) tf = t2;
          }
          if (tn < tf && tn > 0 && tn < best) {
            best = tn;
            const int sg = dd[ax] < 0 ? 1 : 0;            // 目の方を向いている面
            bf = b * 6 + ax * 2 + sg;
            const float hp[3] = {camB[0] + dx * tn, camB[1] + dy * tn, camB[2] + dz * tn};
            const Face& F = faces[bf];
            int ui = 0, vi = 0;
            for (int k = 0; k < 3; ++k) { if (F.T[k] != 0) ui = k; if (F.B[k] != 0) vi = k; }
            bu = (hp[ui] - B[ui]) / B[3 + ui]; bv = (hp[vi] - B[vi]) / B[3 + vi];
          }
        }
        p.f = (uint8_t)bf;
        p.a = (int16_t)(bu * 1024.0f); p.b = (int16_t)(bv * 1024.0f);
      } else {                                        // 鏡の水面: 上からのぞく(水面 y = 0、目の高さ 2.2)
        float dx = px * 0.8f, dy = py * 0.8f - 0.30f, dz = 1.0f;
        const float l = 1.0f / sqrtf(dx * dx + dy * dy + dz * dz); dx *= l; dy *= l; dz *= l;
        putDir(p, dx, dy, dz);
        if (dy > -0.004f) { p.f = 255; p.a = p.b = 0; continue; }
        const float k = 2.2f / -dy;
        const float X = dx * k, Z = dz * k;
        float gx = (X + 8.0f) / 16.0f * (GC - 1), gz = Z / 16.0f * (GC - 1);
        gx = gx < 0 ? 0 : gx > GC - 1.01f ? GC - 1.01f : gx; gz = gz < 0 ? 0 : gz > GC - 1.01f ? GC - 1.01f : gz;
        p.a = (int16_t)(gx * 256.0f); p.b = (int16_t)(gz * 256.0f);
        float fog = k / 14.0f; fog = fog > 1 ? 1 : fog;
        p.f = (uint8_t)(fog * fog * 30.0f);
      }
    }
  }
  gN = vw == 2 ? GC : GA;
  for (int i = 0; i < gN * gN * 2; ++i) grad[i] = 0;
}

// ---------------------------------------------------------------- 動き(コマごとの高さ)
static void addRipple(float x, float y, float amp) {
  if (nRip >= MAXRIP) { for (int i = 1; i < nRip; ++i) rip[i - 1] = rip[i]; --nRip; }
  rip[nRip++] = {x, y, 0.0f, amp};
}
static void IRAM_ATTR heightsToGrad(float cell) {
  const int n = gN; const float k = 0.5f / cell;
  for (int y = 0; y < n; ++y)
    for (int x = 0; x < n; ++x) {
      const int xa = x > 0 ? x - 1 : x, xb = x < n - 1 ? x + 1 : x, ya = y > 0 ? y - 1 : y, yb = y < n - 1 ? y + 1 : y;
      grad[(y * n + x) * 2]     = (grid[y * n + xb] - grid[y * n + xa]) * k * (2.0f / (xb - xa));
      grad[(y * n + x) * 2 + 1] = (grid[yb * n + x] - grid[ya * n + x]) * k * (2.0f / (yb - ya));
    }
}
static void IRAM_ATTR stepMembrane(float dt) {
  // 指のへこみ: ばねで追いかける(離すと、ぷるんと揺れもどる)
  dentX += (dentTX - dentX) * (1.0f - expf(-dt * 8.0f));
  dentY += (dentTY - dentY) * (1.0f - expf(-dt * 8.0f));
  const float n = GA, cell = 8.0f / (n - 1);
  static const float BL[4][5] = {{-0.8f, 1.9f, 0.9f, 1.0f, 0.7f}, {1.1f, 1.2f, 1.1f, -0.9f, 0.5f}, {0.2f, 3.0f, 0.8f, 0.9f, 0.9f}, {-1.6f, 0.4f, 1.0f, 0.8f, 0.6f}};
  float bx[4], by[4], is2[4], ba[4];
  for (int i = 0; i < 4; ++i) {
    bx[i] = BL[i][0] + 0.6f * sinf(simT * BL[i][4] + BL[i][0]);
    by[i] = BL[i][1] + 0.5f * cosf(simT * BL[i][4] * 0.8f + BL[i][1]);
    is2[i] = 1.0f / (BL[i][2] * BL[i][2]); ba[i] = BL[i][3] * MEMBRANE;
  }
  const float dX = dentX * cell - 4.0f, dY = dentY * cell - 2.0f;
  float ripDecay[MAXRIP];
  for (int r = 0; r < nRip; ++r) ripDecay[r] = fexp(rip[r].age * 1.2f);
  // 横と縦に分けられる形(sin * sin、exp(-(x² + y²)) = exp(-x²) * exp(-y²))は、行と列ごとに先に計算しておく
  float wx[GA], wy[GA], ex[5][GA], ey[5][GA];
  const bool dent = fabsf(dentA) > 0.002f;
  for (int j = 0; j < GA; ++j) {
    const float X = j * cell - 4.0f, Y = j * cell - 2.0f;
    wx[j] = 0.05f * MEMBRANE * 4.0f * fsin(X * 2.1f + simT * 1.3f); wy[j] = fsin(Y * 1.7f - simT);
    for (int i = 0; i < 4; ++i) { const float ddx = X - bx[i], ddy = Y - by[i]; ex[i][j] = ba[i] * fexp(ddx * ddx * is2[i]); ey[i][j] = fexp(ddy * ddy * is2[i]); }
    if (dent) { const float ddx = X - dX, ddy = Y - dY; ex[4][j] = dentA * fexp(ddx * ddx * 1.4f); ey[4][j] = fexp(ddy * ddy * 1.4f); }
  }
  const int nb = dent ? 5 : 4;
  for (int y = 0; y < GA; ++y) {
    const float Y = y * cell - 2.0f;
    for (int x = 0; x < GA; ++x) {
      const float X = x * cell - 4.0f;
      float h = wx[x] * wy[y];
      for (int i = 0; i < nb; ++i) h += ex[i][x] * ey[i][y];
      for (int r = 0; r < nRip; ++r) {
        const Rip& R = rip[r];
        const float ddx = X - R.x, ddy = Y - R.y, d2 = ddx * ddx + ddy * ddy, front = R.age * 2.4f;
        if (d2 < front * front) { const float d = sqrtf(d2); h += R.amp * fsin(d * 6.0f - R.age * 14.0f) * ripDecay[r] * (1.0f - d / (front + 0.01f)); }
      }
      grid[y * GA + x] = h;
    }
  }
  heightsToGrad(cell);
}
static void IRAM_ATTR stepWater(float dt) {
  const float cell = 16.0f / (GC - 1);
  rainT -= dt;
  if (rainT <= 0) { addRipple(-4.0f + (esp_random() % 800) / 100.0f, 2.5f + (esp_random() % 700) / 100.0f, 0.06f * WATER); rainT = RAIN_SEC * (0.5f + (esp_random() % 100) / 100.0f); }
  // 波: 横だけ・縦だけの部分を先に。sin(a + b) = sin a cos b + cos a sin b
  float ax[GC], sx[GC], cx[GC], az[GC], sz[GC], cz[GC];
  for (int j = 0; j < GC; ++j) {
    const float X = j * cell - 8.0f, Z = j * cell;
    ax[j] = 0.020f * WATER * fsin(X * 3.0f + simT); az[j] = 0.015f * WATER * fsin(Z * 4.3f - simT * 1.4f);
    sx[j] = 0.010f * WATER * fsin(X * 2.1f + simT * 0.8f); cx[j] = 0.010f * WATER * fcos(X * 2.1f + simT * 0.8f);
    sz[j] = fsin(Z * 1.7f); cz[j] = fcos(Z * 1.7f);
  }
  for (int z = 0; z < GC; ++z) {
    const float Z = z * cell;
    for (int x = 0; x < GC; ++x) {
      const float X = x * cell - 8.0f;
      float h = ax[x] + az[z] + sx[x] * cz[z] + cx[x] * sz[z];
      for (int r = 0; r < nRip; ++r) {
        const Rip& R = rip[r];
        const float ddx = X - R.x, ddz = Z - R.y, d2 = ddx * ddx + ddz * ddz;
        const float front = R.age * 3.0f / 5.0f + 0.3f;
        if (d2 > front * front) continue;
        const float d = sqrtf(d2);
        h += R.amp * fsin(d * 5.0f - R.age * 3.0f) * fexp(d * 0.35f + R.age * 0.45f) * (1.0f - d / (front + 0.01f));
      }
      grid[z * GC + x] = h;
    }
  }
  heightsToGrad(cell);
}
static void simulate(float dt) {
  simT += dt;
  cloudPos += dt * CLOUD_SPEED;
  cloudU = (int)cloudPos; cloudV = (int)(cloudPos * 0.35f);
  for (int r = 0; r < nRip; ++r) rip[r].age += dt;
  while (nRip > 0 && rip[0].age > 7.0f) { for (int i = 1; i < nRip; ++i) rip[i - 1] = rip[i]; --nRip; }
  for (int b = 0; b < NBOX; ++b) boxAmp[b] *= expf(-dt * 0.9f);
  if (view == 0) stepMembrane(dt);
  else if (view == 2) stepWater(dt);
}

// ---------------------------------------------------------------- 描く
static inline __attribute__((always_inline)) void sampleGrad(const Pix& p, float& gx, float& gy) {
  const int ia = p.a >> 8, ib = p.b >> 8;
  const float fa = (p.a & 255) * (1.0f / 256.0f), fb = (p.b & 255) * (1.0f / 256.0f);
  const float* g0 = grad + (ib * gN + ia) * 2;
  const float* g1 = g0 + gN * 2;
  const float x0 = g0[0] + (g0[2] - g0[0]) * fa, x1 = g1[0] + (g1[2] - g1[0]) * fa;
  const float y0 = g0[1] + (g0[3] - g0[1]) * fa, y1 = g1[1] + (g1[3] - g1[1]) * fa;
  gx = x0 + (x1 - x0) * fb; gy = y0 + (y1 - y0) * fb;
}
static inline __attribute__((always_inline)) float recipQ(float q) {   // 1 / (1 + q)
  return q < 1.0f ? recipT[(int)(q * 256.0f)] : 1.0f / (1.0f + q);
}
// 1点の色(見え方ごと)
static inline __attribute__((always_inline)) uint16_t shadeMembrane(const Pix& p) {
  const float I = 1.0f / 127.0f;
  const float dx = p.dx * I, dy = p.dy * I, dz = p.dz * I;
  float gx, gy; sampleGrad(p, gx, gy);
  const float k2 = 2.0f * (dx * gx + dy * gy - dz) * recipQ(gx * gx + gy * gy);
  const uint16_t c = envLook(dx - k2 * gx, dy - k2 * gy, dz + k2);
  return c;
}
static inline __attribute__((always_inline)) uint16_t shadeBuilding(const Pix& p) {
  const float I = 1.0f / 127.0f;
  const float dx = p.dx * I, dy = p.dy * I, dz = p.dz * I;
  if (p.f == 255) return envLook(dx, dy, dz);
  const Face& F = faces[p.f];
  const float U = p.a * (1.0f / 1024.0f), V = p.b * (1.0f / 1024.0f);
  const float ba = boxAmp[F.box];
  const float amp = (1.0f + ba) * BOX_WAVE, ph = simT * (1.1f + ba * 0.8f) + F.box;
  const float s1 = fsin(U * 2.4f + ph), c1 = fcos(U * 2.4f + ph), s2 = fsin(V * 2.0f - simT * 0.9f), c2 = fcos(V * 2.0f - simT * 0.9f);
  const float bu = 0.108f * amp * c1 * c2, bv = -0.09f * amp * s1 * s2;
  const float nx = F.N[0] + F.T[0] * bu + F.B[0] * bv, ny = F.N[1] + F.T[1] * bu + F.B[1] * bv, nz = F.N[2] + F.T[2] * bu + F.B[2] * bv;
  const float k2 = 2.0f * (dx * nx + dy * ny + dz * nz) * recipQ(bu * bu + bv * bv);
  uint16_t c = envLook(dx - k2 * nx, dy - k2 * ny, dz - k2 * nz);
  // 箱のふち
  return (U > 0.975f || U < -0.975f || V > 0.975f || V < -0.975f) ? mix565(c, seamCol, 16) : c;
}
static inline __attribute__((always_inline)) uint16_t shadeWater(const Pix& p) {
  const float I = 1.0f / 127.0f;
  const float dx = p.dx * I, dy = p.dy * I, dz = p.dz * I;
  if (p.f == 255) return envLook(dx, dy, dz);
  float hx, hz; sampleGrad(p, hx, hz);
  const float k2 = 2.0f * (-dx * hx + dy - dz * hz) * recipQ(hx * hx + hz * hz);
  float ry = dy - k2; if (ry < 0) ry = -ry;
  const uint16_t c = envLook(dx + k2 * hx, ry, dz + k2 * hz);
  return p.f ? mix565(c, hazeCol, p.f) : c;
}
// 2つの色の平均(画面へ送る形のまま)
static inline __attribute__((always_inline)) uint16_t avg565(uint16_t a, uint16_t b) {
  const uint32_t x = __builtin_bswap16(a), y = __builtin_bswap16(b);
  return __builtin_bswap16((uint16_t)((x & y) + (((x ^ y) & 0xF7DEu) >> 1)));
}
// 帯を1本描く。色の計算は 2x2 点に1回だけして(縦横1つおき)、あいだはまわりの色の平均でうめる。
// ただし、まわりの4点が別の面・空と水面などにまたがるところ(ふち)は、1点ずつきちんと計算する
static uint16_t* scratch[2] = {nullptr, nullptr};   // コアごとの、間引いた色の置き場
static uint8_t*  scrId[2] = {nullptr, nullptr};     // その点が、どの面か
static constexpr int SAMP_W = 240;                  // 1行の間引いた点の数の上限(S / 2 + 2 まで)
static inline __attribute__((always_inline)) uint16_t shadePix(const Pix& p) {
  return view == 0 ? shadeMembrane(p) : view == 1 ? shadeBuilding(p) : shadeWater(p);
}
static inline __attribute__((always_inline)) uint8_t pixId(const Pix& p) {
  if (view == 0) return 0;                          // 鏡の膜: なめらかなので、いつも平均でよい
  if (view == 1) return p.f;                        // 鏡の建物: 面ごと(箱のふちと、空との境目だけ1点ずつ)
  return p.f == 255 ? 1 : 0;                        // 鏡の水面: 空か水面か
}
static inline __attribute__((always_inline)) uint16_t shadeAt(int x, int y) {
  if (y > S - 1) y = S - 1;
  const int sa = spanA[y], sb = spanB[y];
  if (sa > sb) return 0;
  x = x < sa ? sa : x > sb ? sb : x;
  return shadePix(pix[y * S + x]);
}
static void IRAM_ATTR renderBand(int k, uint16_t* out, int bx, int bw, uint16_t* smp, uint8_t* ids) {
  const int y0 = k * BAND, h = min(BAND, S - y0);
  const int nj = h / 2 + 1, ni = bw / 2 + 1;
  // ---- 間引いた点の色
  for (int j = 0; j < nj; ++j) {
    int y = y0 + 2 * j; if (y > S - 1) y = S - 1;
    uint16_t* sr = smp + j * SAMP_W;
    uint8_t* ir = ids + j * SAMP_W;
    const int sa = spanA[y], sb = spanB[y];
    if (sa > sb) { for (int i = 0; i < ni; ++i) { sr[i] = 0; ir[i] = 0; } continue; }
    const Pix* P = pix + y * S;
    for (int i = 0; i < ni; ++i) {
      int x = bx + 2 * i; x = x < sa ? sa : x > sb ? sb : x;   // 丸の外は、いちばん近い丸の中の点で
      const Pix& p = P[x];
      sr[i] = shadePix(p); ir[i] = pixId(p);
    }
  }
  // ---- あいだをうめる(2行ずつ)。丸の外は黒に
  for (int jj = 0; jj < h / 2; ++jj) {
    const uint16_t* A = smp + jj * SAMP_W;
    const uint16_t* B = A + SAMP_W;
    const uint8_t* IA = ids + jj * SAMP_W;
    const uint8_t* IB = IA + SAMP_W;
    uint16_t* o0 = out + (2 * jj) * bw;
    uint16_t* o1 = o0 + bw;
    const int y = y0 + 2 * jj;
    for (int i = 0; i < bw / 2; ++i) {
      const uint16_t a = A[i];
      o0[2 * i] = a;
      const uint8_t id = IA[i];
      if (id == IA[i + 1] && id == IB[i] && id == IB[i + 1]) {
        const uint16_t ab = avg565(a, A[i + 1]);
        o0[2 * i + 1] = ab;
        o1[2 * i] = avg565(a, B[i]);
        o1[2 * i + 1] = avg565(ab, avg565(B[i], B[i + 1]));
      } else {
        const int x = bx + 2 * i;
        o0[2 * i + 1] = shadeAt(x + 1, y);
        o1[2 * i] = shadeAt(x, y + 1);
        o1[2 * i + 1] = shadeAt(x + 1, y + 1);
      }
    }
    for (int rr = 0; rr < 2; ++rr) {
      uint16_t* o = rr ? o1 : o0;
      const int yy = y + rr;
      const int sa = spanA[yy] - bx, sb = spanB[yy] - bx;
      if (sa > sb) { for (int x = 0; x < bw; ++x) o[x] = 0; continue; }
      for (int x = 0; x < sa && x < bw; ++x) o[x] = 0;
      for (int x = sb + 1 < 0 ? 0 : sb + 1; x < bw; ++x) o[x] = 0;
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
      if (bandW[k] > 0) { const uint32_t t0 = micros(); renderBand(k, band[k % NSLOT], bandX[k], bandW[k], scratch[1], scrId[1]); busy += micros() - t0; }
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
    uint32_t busy = 0;
    for (int k = 1; k < NB; k += 2) {
      while (__atomic_load_n(&doneUpTo, __ATOMIC_ACQUIRE) < k - NSLOT) { }
      if (bandW[k] > 0) { const uint32_t t0 = micros(); renderBand(k, band[k % NSLOT], bandX[k], bandW[k], scratch[0], scrId[0]); busy += micros() - t0; }
      __atomic_store_n(&readyW, k, __ATOMIC_RELEASE);
    }
    busyUs[0] = busy;
    xSemaphoreGive(semDone);
  }
}

// ---------------------------------------------------------------- 切り替え
static const char* const VIEW_NAME[3] = {"membrane", "building", "water"};
static void setView(int v) {
  view = v;
  nRip = 0; dentA = 0; dentV = 0;
  const uint32_t t0 = millis();
  buildView(view);
  simulate(0.0f);
  Serial.printf("view: %s  (%u ms)\n", VIEW_NAME[view], (unsigned)(millis() - t0));
}
static void setSky(int th) {
  theme = th;
  buildSky(theme);
  Serial.printf("sky: %s\n", SKY_NAME[theme]);
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
  Serial.println("NullPavilion v6");
#if defined(ESP_PLATFORM)
  if (getCpuFrequencyMhz() < 240) setCpuFrequencyMhz(240);
#endif
  if (M5.Display.width() < M5.Display.height()) M5.Display.setRotation(1);
  W = M5.Display.width(); H = M5.Display.height();
  roundScr = (W == H);
  S = min(W, H) & ~1;
  XOFF = ((W - S) / 2) & ~1; YOFF = (H - S) / 2;
  NB = (S + BAND - 1) / BAND;
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
  pix = (Pix*)ps_malloc((size_t)S * S * sizeof(Pix));
  skyMap = (uint16_t*)imalloc(EM * EM * 2); gndMap = (uint16_t*)imalloc(EM * EM * 2);
  skyFade = (uint8_t*)imalloc(EM * EM); cloudT = (uint8_t*)imalloc(CM * CM);
  grid = (float*)imalloc(GC * GC * sizeof(float)); grad = (float*)imalloc(GC * GC * 2 * sizeof(float));
  for (int i = 0; i < 2; ++i) { scratch[i] = (uint16_t*)imalloc((BAND / 2 + 1) * SAMP_W * 2); scrId[i] = (uint8_t*)imalloc((BAND / 2 + 1) * SAMP_W); }
  if (!spanA || !spanB || !bandX || !bandW || !band[0] || !band[1] || !band[2] || !band[3] || !pix || !skyMap || !gndMap || !skyFade || !cloudT || !grid || !grad || !scratch[0] || !scratch[1] || !scrId[0] || !scrId[1])
    fail("Memory error (PSRAM?)");
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
  for (int i = 0; i < 260; ++i) recipT[i] = 1.0f / (1.0f + i / 256.0f);
  for (int i = 0; i < 1024; ++i) sinT[i] = sinf(i * 6.2831853f / 1024.0f);
  for (int i = 0; i <= 512; ++i) expT[i] = expf(-i / 32.0f);

  M5.Display.setTextDatum(middle_center);
  M5.Display.setFont(&fonts::lgfxJapanGothicP_16);
  M5.Display.setTextColor(0x8410);
  M5.Display.drawString("鏡を用意しています…", W / 2, H / 2);
  buildFaces();
  buildClouds();
  setSky(0);
  setView(0);

  semGo = xSemaphoreCreateBinary();
  semDone = xSemaphoreCreateBinary();
  if (!semGo || !semDone) fail("Memory error (task)");
  xTaskCreatePinnedToCore(workerTask, "pav", 6144, nullptr, 1, nullptr, 0);   // コア0 で帯を描く
}

void loop() {
  static uint32_t last = millis(), fpsT = millis(), lastTapMs = 0, lastDropMs = 0;
  static int frames = 0;
  static bool wasTouch = false;
  M5.update();
  auto t = M5.Touch.getDetail();
  const uint32_t now = millis();
  const bool corner = !roundScr && t.x < 56 && t.y < 56;   // 左上のすみはメニューに戻る所なので使わない
  if (roundScr) {
    if (M5.BtnA.wasClicked()) setView((view + 1) % 3);
    if (M5.BtnB.wasClicked()) setSky((theme + 1) % NSKY);
  } else {
    if (M5.BtnPWR.wasClicked()) setSky((theme + 1) % NSKY);
    if (t.wasPressed() && !corner) {
      if (now - lastTapMs < 350) { setView((view + 1) % 3); lastTapMs = 0; } else lastTapMs = now;
    }
  }
  float dt = (now - last) / 1000.0f; last = now;
  if (dt > 0.1f) dt = 0.1f;

  // 触る
  const bool touching = t.isPressed() && !corner;
  const int tx = t.x - XOFF, ty = t.y - YOFF;
  const bool inside = tx >= 0 && ty >= 0 && tx < S && ty < S && spanA[ty] <= tx && tx <= spanB[ty];
  if (touching && inside) {
    const Pix& p = pix[ty * S + tx];
    if (view == 0) {
      dentTX = p.a / 256.0f; dentTY = p.b / 256.0f;
      if (!wasTouch) { dentX = dentTX; dentY = dentTY; addRipple(dentTX * 8.0f / (GA - 1) - 4.0f, dentTY * 8.0f / (GA - 1) - 2.0f, 0.05f); }
    } else if (view == 1) {
      if (p.f != 255) { const int b = faces[p.f].box; boxAmp[b] = min(3.0f, boxAmp[b] + dt * 6.0f + (wasTouch ? 0.0f : 1.0f)); }
    } else if (p.f != 255 && (!wasTouch || now - lastDropMs > 140)) {
      addRipple(p.a / 256.0f * 16.0f / (GC - 1) - 8.0f, p.b / 256.0f * 16.0f / (GC - 1), (wasTouch ? 0.035f : 0.07f) * WATER);
      lastDropMs = now;
    }
  }
  // 膜のへこみ: 押しているあいだは深く、離すとばねで揺れもどる
  {
    const float target = (touching && inside && view == 0) ? -PRESS : 0.0f;
    const float acc = (target - dentA) * 60.0f - dentV * 3.2f;
    dentV += acc * dt; dentA += dentV * dt;
  }
  wasTouch = touching && inside;

  simulate(dt);
  renderFrame();
  if (++frames, now - fpsT >= 3000) {
    Serial.printf("fps: %.1f  (%s / %s)  core0 %.1f ms  core1 %.1f ms  send %.1f ms\n", frames * 1000.0f / (now - fpsT), VIEW_NAME[view], SKY_NAME[theme],
                  busyUs[0] / 1000.0f, busyUs[1] / 1000.0f, sendUs / 1000.0f);
    frames = 0; fpsT = now;
  }
}


// 共通の置き場に置いたものは、0 で始まる単純な型だけ(コンストラクタが要らないもの)
static_assert(std::is_trivially_default_constructible<Face>::value, "NM_ARENA: Face");
static_assert(std::is_trivially_default_constructible<Rip>::value, "NM_ARENA: Rip");
static_assert(std::is_trivially_default_constructible<bool>::value, "NM_ARENA: bool");
static_assert(std::is_trivially_default_constructible<float>::value, "NM_ARENA: float");
static_assert(std::is_trivially_default_constructible<int>::value, "NM_ARENA: int");
static_assert(std::is_trivially_default_constructible<uint16_t>::value, "NM_ARENA: uint16_t");
static_assert(std::is_trivially_default_constructible<uint8_t>::value, "NM_ARENA: uint8_t");
static constexpr size_t OVL_END = nmAlignTo(nmOv26, 16);
static_assert(OVL_END <= NM_ARENA_SIZE, "NM_ARENA_SIZE (nm_common.h) を大きくしてください");
extern const size_t OVL_SIZE = OVL_END;   // 起動時の確認用
}  // namespace NullPavilion
