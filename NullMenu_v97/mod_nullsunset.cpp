// NullSunset  -  NullMenu 用モジュール(元のスケッチ NullSunset_v6 を名前空間に入れたもの)
// 0 で始まる変数(配列も)は、ほかの作品と共通の置き場(NM_ARENA)に置いています。
// 一度に動くのは1つだけ(切り替えるときは再起動)なので、重なっていても問題ありません。
#include "nm_common.h"
#if defined(ESP_PLATFORM) && __has_include(<lgfx/v1/panel/Panel_AMOLED.hpp>)
#include <lgfx/v1/panel/Panel_AMOLED.hpp>
#endif
#pragma GCC optimize("O3")

namespace NullSunset {
static constexpr size_t nmOv0 = 0;

// NullSunset v6  -  夕凪の海。雲が流れ、日がゆっくり沈み、波にゆれる水面に空と日の光の道が映る
//   大阪・関西万博のパビリオン「null²」を見た印象をもとにした、非公式の習作です。写真や他の作品の画像は使っていません。
//   M5Stack StopWatch 用(丸い 466x466 画面)。CoreS3 系でも真ん中に正方形で動きます。
//
// 画面の中:
//   ・水平線まで続く静かな海と、左右の小さな島のシルエット。空には、もこもこした小さな雲がびっしり並ぶ「雲の天井」
//     (NullCloud の雲の海を、下から見上げた感じ)。雲は日の当たる側が金色に光り、反対側は紫がかった影。ふちは日の色に光る
//   ・日はゆっくり沈んでいく。空は金色 → 夕焼け → 残照 → 青い時間 → 星空へ。しばらくすると、また明るくなって日がのぼる
//   ・水面は、大きさと向きの違う小さな波がたくさん重なってできていて、波の面ごとに空を映す。雲も夕焼けも細かく砕けて映り、
//     日の光は、波の面がきらきら反射してできる光の道になる。手前ほど空は映りにくく、暗い海の色になる
//   ・夕方には、ときどき鳥が横切る。夜は星が出る
//
// 触ると:
//   海をタップ … そこから、しずくを落としたような波紋が広がる(海の上の丸なので、遠いほど小さく平たい。先頭の輪がいちばん長く、
//                うしろの輪ほど間がつまる。光る輪ではなく、映った空と日の光が輪の形にゆれる)
//   海をなぞる … 指のあとに、波紋が続いていく
//   空をタップ … そこから鳥の群れが、手前を横切って飛んでいく
//   空を左右になぞる … 時間を前後に動かす(右へで進む。画面の幅で 1/4 周。日がのぼったり沈んだりする)
// 傾けると:
//   左右で見る向きが変わり(日と島が横へ動く)、手前・奥で水平線が上下する
//
// 操作:
//   StopWatch … ボタンA: 時間を進める(1/8 周ずつ。3秒かけて、ゆるりと移りかわる)  ボタンB: 時間の速さ(1倍 / 4倍 / 0.25倍)
//   CoreS3系  … 電源ボタン短押し: 時間を進める  ダブルタップ: 時間の速さ
//
// 必要なもの:
//   ボードマネージャー M5Stack >= 3.3.7 / ボード: M5StopWatch(CoreS3 なら M5CoreS3)。StopWatch では PSRAM を「OPI PSRAM」に
//   ライブラリ M5Unified >= 0.2.15, M5GFX >= 0.2.21
//
// しくみ:
//   雲: 起動時に、上下左右がつながった積雲の模様(512x512)を作る。大きなつぶの上に小さなつぶを、丸い山の形で積んだ高さの絵から、
//     点ごとに「濃さ」と「光の当たり方(日の側 = 奥へ向かって薄くなる面が明るい、厚い所は暗い)」を出し、縮めた絵(5段)も作っておく。
//     空の高い所に張った面として遠近をつけて引く。1行の中は面の上で同じ奥行きなので、模様の1行だけを横に読む(PSRAM でも速い)。
//     遠い行ほど縮めた絵を使い、水平線の近くはかすませる。
//   海: 波の傾きの模様を2枚(大きな波 8m 四方 / 細かい波 1m 四方。どちらも上下左右がつながる。256x256 と縮めた絵 4段)作り、
//     別の向き・速さで流す。点ごとに波の傾きから、映る向き(鏡のように反射した向き)と映りやすさ(フレネル)を出す。
//     映る空は、コマのはじめに2つのコアで描いておく「映り込み用の空」(横は3点おき、縦は水平線の近くほど細かく 96 行)から引く。
//     日の光は、映る向きと日の向きの近さから足す(遠くの行ほど、縮めて消えた細かい波の分だけ、光の道を太くする)。
//   波紋: 海の上の丸(大きさと速さは、触った所の遠さに合わせる)として、波の傾きに足す。先頭の輪が長く、うしろほど短い。広がるほど弱まる。
//   空も海も1点おきに計算して、あいだは左右のまん中にする。帯(16行ずつ)は、2つのコアのうち空いた方が、次の帯を描く。

#if defined(ESP_PLATFORM)
#include <esp_memory_utils.h>
#endif
#ifndef IRAM_ATTR
#define IRAM_ATTR
#endif


// Arduino IDE は関数の宣言を自動で先頭近くに足すので、型の名前だけ先に宣言しておく
struct Key;
struct Ripple;
struct Bird;
struct SkyRow;

// ---- 調整用 ----------------------------------------------------------------
static constexpr float CYCLE_SEC   = 160.0f;  // 日が沈んで、またのぼるまで(秒。1倍のとき)
static constexpr float START_PHASE = 0.15f;   // 始めの時間(0 = 金色の時間 / 0.15 = 日の入り / 0.5 = 夜)
static constexpr float CLOUD_SPEED = 5.0f;    // 雲の流れる速さ(1 でごくゆっくり。5 で頭の上を1秒に十数点)
static constexpr float JUMP_SEC    = 3.0f;    // ボタンで時間を進めるとき、移りかわるのにかける秒数
static constexpr float WAVE_SPEED  = 1.0f;    // 波の速さ
static constexpr bool  TILT        = true;    // 傾きで見え方を変えるか
static constexpr bool  TILT_FLIP_X = false;   // 左右が逆に動くときは true
static constexpr bool  TILT_FLIP_Y = false;   // 上下が逆に動くときは true
static constexpr bool  TILT_SWAP   = false;   // 左右と上下が入れかわっているときは true
static constexpr bool  TILT_STOPWATCH_FIX = true;   // StopWatch はセンサーが 90 度回ってついているので、自動で読みかえる
static constexpr float TILT_RELEVEL = 8.0f;   // 同じ角度で持ち続けると、何秒くらいでそこを「水平」にするか(0 = 起動したときの姿勢のまま)
static constexpr float TILT_RELEVEL_MAX = 0.5f;
// ---------------------------------------------------------------------------

static constexpr bool DIRECT_SEND = true;          // StopWatch では、画面へ「直接」送る。画面が乱れるときは false に
#if defined(ESP_PLATFORM) && __has_include(<lgfx/v1/panel/Panel_AMOLED.hpp>)
#define NL_DIRECT_AMOLED 1
struct NlFbPeek : public lgfx::Panel_AMOLED_Framebuffer {
  static lgfx::Panel_AMOLED* inner(lgfx::Panel_Device* p) {
    return static_cast<lgfx::Panel_AMOLED_Framebuffer*>(p)->*(&NlFbPeek::_panel);
  }
};
#endif

static constexpr int BAND = 16, NSLOT = 4;
static const float SPEEDS[3] = {1.0f, 4.0f, 0.25f};
static constexpr int NE = 2048;                   // 高さの表の段(-0.62 .. 0.82)
static constexpr float E0 = -0.62f, E1 = 0.82f;
static constexpr int CTX = 512, NCL = 5;          // 雲の模様の大きさ、縮めた絵の段の数
static constexpr float CLW = 3.2f;               // 雲の模様1枚が、雲の面の上でおおう広さ(目から雲の面までの高さを 1 として)
static constexpr int WT = 256, NWL = 5;           // 波の模様の大きさ、縮めた絵の段の数
static constexpr float WL0 = 8.0f, WL1 = 1.0f;    // 波の模様1枚がおおう広さ(目の高さを 1 として。大きな波 / 細かい波)
static constexpr int RR = 96, RJ = 8, RS = 3;    // 映り込み用の空の行の数、1回の仕事でやる行の数、横の間(画面の点)
static constexpr float EMX = 0.80f;              // 映り込み用の空の、いちばん上の高さ
static constexpr int NSPEC = 384;
static constexpr int NGLOW = 1024;
static constexpr int MAXRIP = 6, MAXBIRD = 14;
static constexpr float RIP_DZ = 0.65f;            // 波紋の奥行きの縮め方(1 = 本物どおり)
static constexpr float SUNX = 0.10f;              // 日の横の位置(真ん中からの向き)

static int W, H, S, XOFF, YOFF, NB;
static bool roundScr = true, fbMode = false, directSend = false;
static int16_t* spanA = nullptr;
static int16_t* spanB = nullptr;
static int16_t* bandX = nullptr;
static int16_t* bandW = nullptr;
static uint16_t* band[NSLOT] = {nullptr, nullptr, nullptr, nullptr};
static float FOC, iFOC = 1, focRS = 1;           // iFOC = 1 / FOC、focRS = FOC / RS(ESP32-S3 は割り算が遅いので、先に計算しておく)

// 表
static uint16_t* ctex[NCL];                       // 雲の模様(下の 8 ビット: 濃さ、上の 8 ビット: 光の当たり方)。段ごとに半分に縮めた絵
static int16_t*  wtex[2][NWL];                    // 波の傾き(下の 8 ビット: 横、上の 8 ビット: 奥)。2枚 × 縮めた絵
static float     wvar[2][NWL + 1];                // 縮めた絵で消えた、細かい傾きの強さ(2乗)。最後は全部
static uint16_t* refl = nullptr;                  // 映り込み用の空 [RR x RC](RGB565)
static uint8_t*  rRow = nullptr;                  // 高さの表の番号 → 映り込み用の空の行 [NE]
static int RC = 0, RM = 0;                        // 映り込み用の空の横の数、左右の余り(画面の点)
static float*   invE = nullptr;                   // 高さ → 雲の面までの遠さ [NE]
static uint8_t* grad = nullptr;                   // 高さ → 空の色(赤緑青)[NE * 3]
static float*   isle = nullptr;                   // 向き → 島の高さ [ISN]
static uint8_t* cLUT = nullptr;                   // 雲の明るさ(64段)→ 色 [64 * 3]
static uint8_t* silT = nullptr;                   // 雲の濃さ → ふちの光 [256]
static uint8_t* fresT = nullptr;                  // 水面と目の向きの近さ → 映りやすさ [256]
static uint8_t* specT = nullptr;                  // 日からのずれ → きらめき [NSPEC]
static constexpr int ISN = 2048;
static constexpr float ISA = 1.6f;                // 島の表の向きの範囲(±)
static constexpr size_t nmOa0 = nmAlignTo(nmOv0, 16), nmOv1 = nmOa0 + sizeof(uint8_t[NGLOW]); static uint8_t (&glR)[NGLOW] = *reinterpret_cast<uint8_t(*)[NGLOW]>(NM_ARENA + nmOa0);
static constexpr size_t nmOa1 = nmAlignTo(nmOv1, 16), nmOv2 = nmOa1 + sizeof(uint8_t[NGLOW]); static uint8_t (&glG)[NGLOW] = *reinterpret_cast<uint8_t(*)[NGLOW]>(NM_ARENA + nmOa1);
static constexpr size_t nmOa2 = nmAlignTo(nmOv2, 16), nmOv3 = nmOa2 + sizeof(uint8_t[NGLOW]); static uint8_t (&glB)[NGLOW] = *reinterpret_cast<uint8_t(*)[NGLOW]>(NM_ARENA + nmOa2);
static constexpr size_t nmOa3 = nmAlignTo(nmOv3, 16), nmOv4 = nmOa3 + sizeof(uint8_t[NGLOW]); static uint8_t (&glW)[NGLOW] = *reinterpret_cast<uint8_t(*)[NGLOW]>(NM_ARENA + nmOa3);   // 日からの距離の2乗 → 光
static float    glowK = 1;                        // 距離の2乗 → 光の表の番号
static int      sunC8[3] = {0, 0, 0};             // 雲のふちと、水面のきらめきの色(0..255)
static float    sunVis = 1;                       // 日が見えているか(0..1)

// 今の空の色(時間でまざる)
struct Key { float e; float zen[3], mid[3], hor[3], sun[3], lit[3], shade[3], water[3], isl[3]; float glow, stars, birds; };
static const Key KEYS[] = {
  { 0.20f, {0.16f, 0.26f, 0.52f}, {0.62f, 0.56f, 0.62f}, {1.00f, 0.78f, 0.52f}, {1.00f, 0.86f, 0.62f}, {1.00f, 0.84f, 0.66f}, {0.44f, 0.38f, 0.50f}, {0.05f, 0.08f, 0.15f}, {0.10f, 0.07f, 0.09f}, 0.65f, 0.0f, 1.0f},   // 金色の時間
  { 0.06f, {0.12f, 0.12f, 0.34f}, {0.62f, 0.34f, 0.44f}, {1.00f, 0.58f, 0.28f}, {1.00f, 0.66f, 0.30f}, {1.00f, 0.58f, 0.38f}, {0.36f, 0.24f, 0.36f}, {0.06f, 0.06f, 0.12f}, {0.07f, 0.04f, 0.07f}, 1.00f, 0.0f, 1.0f},   // 日の入り
  {-0.03f, {0.05f, 0.05f, 0.18f}, {0.36f, 0.20f, 0.36f}, {0.88f, 0.38f, 0.26f}, {0.90f, 0.40f, 0.22f}, {0.90f, 0.42f, 0.42f}, {0.18f, 0.12f, 0.22f}, {0.04f, 0.04f, 0.09f}, {0.04f, 0.02f, 0.05f}, 0.55f, 0.0f, 0.6f},   // 残照
  {-0.12f, {0.02f, 0.03f, 0.10f}, {0.08f, 0.10f, 0.24f}, {0.22f, 0.26f, 0.46f}, {0.30f, 0.30f, 0.50f}, {0.26f, 0.26f, 0.42f}, {0.06f, 0.06f, 0.13f}, {0.02f, 0.03f, 0.07f}, {0.02f, 0.02f, 0.04f}, 0.15f, 0.4f, 0.0f},   // 青い時間
  {-0.26f, {0.008f, 0.012f, 0.035f}, {0.03f, 0.04f, 0.09f}, {0.08f, 0.09f, 0.17f}, {0.20f, 0.22f, 0.30f}, {0.09f, 0.09f, 0.14f}, {0.03f, 0.03f, 0.06f}, {0.015f, 0.02f, 0.045f}, {0.01f, 0.01f, 0.02f}, 0.0f, 1.0f, 0.0f},   // 夜
};
static constexpr int NKEY = sizeof(KEYS) / sizeof(KEYS[0]);
static Key cur;                                   // 今の色
static int watC[3], islC[3];                     // 0..255
static float sunE = 0.2f, starW = 0, birdW = 1;

// 動き
struct Ripple { float u0, v0, sc, t, a; bool on; };   // 海の上の位置(目の高さを 1 として)、大きさ(触った所の遠さ)
static constexpr size_t nmOa4 = nmAlignTo(nmOv4, 16), nmOv5 = nmOa4 + sizeof(Ripple[MAXRIP]); static Ripple (&rip)[MAXRIP] = *reinterpret_cast<Ripple(*)[MAXRIP]>(NM_ARENA + nmOa4);
static int ripNext = 0;
struct Bird { float x, e, vx, ve, ph, sz; bool on; };
static constexpr size_t nmOa5 = nmAlignTo(nmOv5, 16), nmOv6 = nmOa5 + sizeof(Bird[MAXBIRD]); static Bird (&bird)[MAXBIRD] = *reinterpret_cast<Bird(*)[MAXBIRD]>(NM_ARENA + nmOa5);
static float phase = START_PHASE, simT = 0, nextBirds = 6.0f;
static float jumpTot = 0, jumpT = 0, jumpK = 0;   // ボタンで時間を進めるとき(JUMP_SEC かけて、ゆるりと)
static float cu = 0, cv = 0, w1u = 0, w1v = 0, w2u = 0, w2v = 0;   // 雲と波の流れ(模様1枚 = 1)
static float pan = 0, hzRow = 0;                  // 見ている向き(画面の点)、水平線の行
static int   speedIdx = 0;
static float tiltX = 0, tiltY = 0, levelX = 0, levelY = 0;
static int   levelN = 0;
static bool  imuOK = false;
static uint32_t rnd = 12345;
static inline float frand() { rnd = rnd * 1664525u + 1013904223u; return (rnd >> 8) * (1.0f / 16777216.0f); }   // 0..1

// 2つのコアで描く
static SemaphoreHandle_t semGo = nullptr, semDone = nullptr;
static volatile int doneUpTo = -1;
static volatile int nextBand = 0;
static volatile int reflNext = 0, reflDone = 0;   // 映り込み用の空の仕事
static volatile uint8_t bandReady[64];
static uint32_t busyUs[2] = {0, 0}, sendUs = 0;

// ---------------------------------------------------------------- 小さな道具
static inline float clamp01(float v) { return v < 0 ? 0 : (v > 1 ? 1 : v); }
static inline int clampi(int v, int a, int b) { return v < a ? a : (v > b ? b : v); }
static inline float smooth01(float v) { v = clamp01(v); return v * v * (3 - 2 * v); }
static inline uint32_t hash32(uint32_t x) { x ^= x >> 16; x *= 0x7feb352d; x ^= x >> 15; x *= 0x846ca68b; x ^= x >> 16; return x; }
static const uint8_t BAYER[16] = {0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5};
static inline uint16_t pack(int r, int g, int b) {
  r = r > 255 ? 255 : r; g = g > 255 ? 255 : g; b = b > 255 ? 255 : b;
  return __builtin_bswap16((uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)));
}
// 色の段が目立たないよう、4x4 の決まった模様で少し散らしてから丸める
static inline __attribute__((always_inline)) uint16_t packD(int r, int g, int b, int x, int y) {
  const int d = BAYER[((y & 3) << 2) | (x & 3)];
  r += d >> 1; g += d >> 2; b += d >> 1;
  r = r > 255 ? 255 : r; g = g > 255 ? 255 : g; b = b > 255 ? 255 : b;
  return __builtin_bswap16((uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)));
}
static inline float tone(float c) { return 1.0f - expf(-c * 1.35f); }   // 光の強さ → 画面の明るさ(0..1)
static inline int ffloorI(float x) { const int i = (int)x; return (x < (float)i) ? i - 1 : i; }   // floorf の代わり(関数を呼ばない)
static inline float frcpF(float x) {               // 1/x(x > 0)。掛け算だけ(ESP32-S3 は割り算が関数の呼び出しになって遅い)
  union { float f; uint32_t i; } u = {x}; u.i = 0x7EF311C7u - u.i;
  float r = u.f; r = r * (2.0f - x * r); r = r * (2.0f - x * r); return r;
}
static inline float frsqrtF(float x) {             // 1/sqrt(x)(x > 0)。掛け算だけ
  union { float f; uint32_t i; } u = {x}; u.i = 0x5F3759DFu - (u.i >> 1);
  float y = u.f; y = y * (1.5f - 0.5f * x * y * y); y = y * (1.5f - 0.5f * x * y * y); return y;
}
static inline int eIdx(float e) { const int i = (int)((e - E0) * (NE / (E1 - E0))); return i < 0 ? 0 : (i >= NE ? NE - 1 : i); }
static inline float idxE(int i) { return E0 + (i + 0.5f) * ((E1 - E0) / NE); }

static void showMsg(const char* msg, float prog);
static void fail(const char* msg);

// ---------------------------------------------------------------- 起動時に作る表
static float vnoiseP(float x, float y, int P, uint32_t seed) {   // 上下左右が P でつながる、なめらかなノイズ(0..1)
  const int ix = (int)floorf(x), iy = (int)floorf(y);
  const float fx = x - ix, fy = y - iy, ux = fx * fx * (3 - 2 * fx), uy = fy * fy * (3 - 2 * fy);
  auto h = [&](int a, int b) { a = ((a % P) + P) % P; b = ((b % P) + P) % P; return (hash32((uint32_t)a * 73856093u ^ (uint32_t)b * 19349663u ^ seed) >> 8) * (1.0f / 16777216.0f); };
  const float a = h(ix, iy), b = h(ix + 1, iy), c = h(ix, iy + 1), d = h(ix + 1, iy + 1);
  return (a * (1 - ux) + b * ux) * (1 - uy) + (c * (1 - ux) + d * ux) * uy;
}
static float* sinT = nullptr;                     // 波を作るときの sin の表(作り終わったら返す)
static inline float fsin(float a) { const float f = a * (1024.0f / 6.2831853f); int i = (int)floorf(f); const float t = f - i; const float s0 = sinT[i & 1023], s1 = sinT[(i + 1) & 1023]; return s0 + (s1 - s0) * t; }
static void buildCloud() {                        // 積雲のつぶを積んだ高さの絵 → 濃さと光の当たり方
  float* hp = (float*)ps_malloc(sizeof(float) * CTX * CTX);
  if (!hp) fail("Memory error (cloud)");
  for (int i = 0; i < CTX * CTX; ++i) hp[i] = 0;
  struct Lv { float rad; int cnt; float hs; };
  static const Lv LV[4] = {{35.0f, 90, 1.0f}, {19.0f, 420, 0.75f}, {10.0f, 1800, 0.5f}, {5.0f, 6000, 0.3f}};
  uint32_t rs = 777;
  auto rf = [&]() { rs = rs * 1664525u + 1013904223u; return (rs >> 8) * (1.0f / 16777216.0f); };
  for (int l = 0; l < 4; ++l) {
    for (int k = 0; k < LV[l].cnt; ++k) {
      if ((k & 63) == 0) vTaskDelay(1);
      const float cx = rf() * CTX, cy = rf() * CTX;
      const float cov = 0.6f * vnoiseP(cx * (4.0f / CTX), cy * (4.0f / CTX), 4, 5150u) + 0.4f * vnoiseP(cx * (8.0f / CTX), cy * (8.0f / CTX), 8, 5151u);
      if (rf() > cov * 1.9f - 0.55f) continue;    // 雲の多い所と、切れ間
      const float r = LV[l].rad * (0.6f + 0.8f * rf());
      const int ix = (int)cx, iy = (int)cy;
      const float sub = l > 0 ? hp[(iy & (CTX - 1)) * CTX + (ix & (CTX - 1))] * 0.85f : 0.0f;   // 下のつぶの上に積む
      const int x0 = (int)floorf(cx - r - 1), y0 = (int)floorf(cy - r - 1), w = (int)(2 * r + 3);
      const float ir2 = 1.0f / (r * r), hr = r * LV[l].hs;
      for (int y = y0; y < y0 + w; ++y) {
        const float dy = y - cy;
        float* row = hp + (y & (CTX - 1)) * CTX;
        for (int x = x0; x < x0 + w; ++x) {
          const float dx = x - cx, d2 = (dx * dx + dy * dy) * ir2;
          if (d2 >= 1.0f) continue;
          const float v = sqrtf(1.0f - d2) * hr + sub;
          float& o = row[x & (CTX - 1)];
          if (v > o) o = v;
        }
      }
    }
  }
  float mx = 1e-6f;
  for (int i = 0; i < CTX * CTX; ++i) mx = fmaxf(mx, hp[i]);
  const float im = 1.0f / mx;
  for (int y = 0; y < CTX; ++y) {
    if ((y & 31) == 0) vTaskDelay(1);
    for (int x = 0; x < CTX; ++x) {
      const float h = hp[y * CTX + x] * im;
      const float gz = (hp[((y + 1) & (CTX - 1)) * CTX + x] - hp[((y - 1) & (CTX - 1)) * CTX + x]) * im;
      const float a = smooth01((h - 0.16f) / 0.24f);
      const float face = fminf(1.5f, fmaxf(0.0f, 0.45f - gz * 22.0f));   // 奥(日の側)へ向かって薄くなる面が明るい
      const float L = fminf(1.4f, fmaxf(0.0f, face * 0.55f - smooth01((h - 0.15f) / 0.75f) * 0.25f + 0.18f));
      ctex[0][y * CTX + x] = (uint16_t)((int)(a * 255.0f + 0.5f) | ((int)(L * (255.0f / 1.4f) + 0.5f) << 8));
    }
  }
  free(hp);
  for (int l = 1; l < NCL; ++l) {                 // 縮めた絵
    const int n = CTX >> l, m = n * 2;
    for (int y = 0; y < n; ++y)
      for (int x = 0; x < n; ++x) {
        const uint16_t* s0 = ctex[l - 1] + (y * 2) * m + x * 2;
        const int a = (s0[0] & 255) + (s0[1] & 255) + (s0[m] & 255) + (s0[m + 1] & 255);
        const int L = (s0[0] >> 8) + (s0[1] >> 8) + (s0[m] >> 8) + (s0[m + 1] >> 8);
        ctex[l][y * n + x] = (uint16_t)(((a + 2) >> 2) | (((L + 2) >> 2) << 8));
      }
  }
}
static void buildWaves() {                        // 波の傾き(たくさんの波を重ねる。上下左右がつながる)
  sinT = (float*)malloc(sizeof(float) * 1024);
  if (!sinT) fail("Memory error (waves)");
  for (int i = 0; i < 1024; ++i) sinT[i] = sinf(i * (6.2831853f / 1024.0f));
  float* sx = (float*)ps_malloc(sizeof(float) * WT * WT * 2);
  if (!sx) fail("Memory error (waves)");
  float* sz = sx + WT * WT;
  const float SC = 127.0f / 0.40f;                // 傾き → 8 ビット
  for (int k = 0; k < 2; ++k) {
    for (int i = 0; i < WT * WT; ++i) { sx[i] = 0; sz[i] = 0; }
    uint32_t rs = 4242u + k * 999u;
    auto rf = [&]() { rs = rs * 1664525u + 1013904223u; return (rs >> 8) * (1.0f / 16777216.0f); };
    for (int w = 0; w < 14; ++w) {
      const float n = 2.0f * powf(8.0f, w / 13.0f);   // 1枚に入る波の数(2 → 16)
      float th = (rf() + rf() + rf() - 1.5f) * 0.55f; // 向き(ほとんど手前へ。横にも少し散らす)
      int kx = (int)lroundf(sinf(th) * n), kz = (int)lroundf(cosf(th) * n);
      if (kz == 0 && kx == 0) kz = 2;
      const float kk = sqrtf((float)(kx * kx + kz * kz)) * (6.2831853f / (k ? WL1 : WL0));   // 波数(目の高さ 1 あたり)
      const float slope = 0.030f * (w < 3 && k == 0 ? 0.7f : 1.0f);
      const float amp = slope / kk, ph = rf() * 6.2831853f;
      const float gx = kx * (6.2831853f / WT), gz = kz * (6.2831853f / WT);
      for (int y = 0; y < WT; ++y) {
        if ((y & 63) == 0) vTaskDelay(1);
        for (int x = 0; x < WT; ++x) {
          const float a = gx * x + gz * y + ph;
          const float c = (fsin(a + 1.5707963f) + 0.5f * fsin(2 * a + 1.5707963f)) * amp * kk;   // 山が少しとがる
          sx[y * WT + x] += c * kx / sqrtf((float)(kx * kx + kz * kz));
          sz[y * WT + x] += c * kz / sqrtf((float)(kx * kx + kz * kz));
        }
      }
    }
    double v0 = 0;
    for (int i = 0; i < WT * WT; ++i) {
      const int a = clampi((int)lroundf(sx[i] * SC), -127, 127), b = clampi((int)lroundf(sz[i] * SC), -127, 127);
      wtex[k][0][i] = (int16_t)((a & 255) | (b << 8));
      v0 += (double)a * a + (double)b * b;
    }
    v0 /= (double)WT * WT;
    for (int l = 1; l < NWL; ++l) {               // 縮めた絵と、消えた細かい傾きの強さ
      const int n = WT >> l, m = n * 2;
      double vl = 0;
      for (int y = 0; y < n; ++y)
        for (int x = 0; x < n; ++x) {
          const int16_t* q = wtex[k][l - 1] + (y * 2) * m + x * 2;
          const int a = ((int8_t)(q[0] & 255) + (int8_t)(q[1] & 255) + (int8_t)(q[m] & 255) + (int8_t)(q[m + 1] & 255)) / 4;
          const int b = ((q[0] >> 8) + (q[1] >> 8) + (q[m] >> 8) + (q[m + 1] >> 8)) / 4;
          wtex[k][l][y * n + x] = (int16_t)((a & 255) | (b << 8));
          vl += (double)a * a + (double)b * b;
        }
      vl /= (double)n * n;
      wvar[k][l] = (float)((v0 - vl) / (SC * SC));
    }
    wvar[k][0] = 0; wvar[k][NWL] = (float)(v0 / (SC * SC));
  }
  free(sx); free(sinT); sinT = nullptr;
}
static void buildFixed() {
  for (int i = 0; i < NE; ++i) {
    const float e = idxE(i);
    invE[i] = 1.0f / fmaxf(e, 0.003f);
    rRow[i] = (uint8_t)clampi((int)(sqrtf(fmaxf(e, 0.0f) / EMX) * RR), 0, RR - 1);
  }
  for (int i = 0; i < ISN; ++i) {                 // 島のシルエット(向き → 高さ)
    const float a = (i + 0.5f) * (2 * ISA / ISN) - ISA;
    float h = 0.034f * expf(-powf((a + 0.38f) / 0.13f, 2)) * (1.0f + 0.10f * sinf(a * 60.0f) + 0.05f * sinf(a * 170.0f));
    h = fmaxf(h, 0.018f * expf(-powf((a - 0.40f) / 0.06f, 2)) * (1.0f + 0.10f * sinf(a * 120.0f)));
    h = fmaxf(h, 0.008f * expf(-powf((a + 0.70f) / 0.05f, 2)));
    isle[i] = h > 0.0015f ? h : -1.0f;
  }
  for (int a = 0; a < 256; ++a) {                 // 濃さ → ふちの光(うすい所ほど光る)。濃さは h の smooth なので、h にもどして考える
    float lo = 0.16f, hi = 0.40f;
    for (int it = 0; it < 20; ++it) { const float m = (lo + hi) * 0.5f; if (smooth01((m - 0.16f) / 0.24f) * 255.0f < a) lo = m; else hi = m; }
    silT[a] = (uint8_t)(255.0f * (1.0f - smooth01((lo - 0.16f) / 0.14f)));
  }
  for (int i = 0; i < 256; ++i) {                 // フレネル(水は 0.02)。少しだけ底上げして、手前でもかすかに映る
    const float c = i / 255.0f, f = 0.02f + 0.98f * powf(1.0f - c, 5.0f);
    fresT[i] = (uint8_t)clampi((int)(fminf(1.0f, f + 0.03f) * 255.0f + 0.5f), 0, 255);
  }
  for (int i = 0; i < NSPEC; ++i) specT[i] = (uint8_t)(255.0f * expf(-i / 64.0f));
}

// ---------------------------------------------------------------- 時間の色
static void mixKey(float e) {                     // 日の高さ e で、表の色をまぜる
  int k = 0;
  while (k < NKEY - 2 && e < KEYS[k + 1].e) ++k;
  const Key& A = KEYS[k]; const Key& B = KEYS[k + 1];
  const float t = clamp01((A.e - e) / (A.e - B.e));
  auto L = [&](const float* a, const float* b, float* o) { for (int i = 0; i < 3; ++i) o[i] = a[i] + (b[i] - a[i]) * t; };
  L(A.zen, B.zen, cur.zen); L(A.mid, B.mid, cur.mid); L(A.hor, B.hor, cur.hor); L(A.sun, B.sun, cur.sun);
  L(A.lit, B.lit, cur.lit); L(A.shade, B.shade, cur.shade); L(A.water, B.water, cur.water); L(A.isl, B.isl, cur.isl);
  cur.glow = A.glow + (B.glow - A.glow) * t; cur.stars = A.stars + (B.stars - A.stars) * t; cur.birds = A.birds + (B.birds - A.birds) * t;
}
static void buildFrameTables() {
  mixKey(sunE);
  starW = cur.stars; birdW = cur.birds;
  for (int i = 0; i < 3; ++i) { watC[i] = (int)(tone(cur.water[i] * 0.7f) * 255); islC[i] = (int)(tone(cur.isl[i]) * 255); }
  sunVis = smooth01((sunE + 0.02f) / 0.04f);
  for (int i = 0; i < 3; ++i) sunC8[i] = (int)(tone(cur.sun[i] * 0.9f) * 255);
  for (int li = 0; li < 64; ++li) {             // 雲: 影の色 → 光の当たる色(日に近いと、もっと明るく)
    const float lv = li * (1.4f / 63.0f);
    for (int k = 0; k < 3; ++k) cLUT[li * 3 + k] = (uint8_t)(tone(cur.shade[k] + (cur.lit[k] * 1.1f - cur.shade[k]) * lv) * 255);
  }
  // 空のグラデーション(高さごと)。水平線のすぐ上が明るく、日が低いほど水平線の近くが赤い
  const int gi0 = eIdx(-0.02f);
  for (int i = gi0; i < NE; i += 4) {             // 4 段おきに計算して、あいだはあとで直線でつなぐ
    const float e = fmaxf(0.0f, idxE(i));
    float c[3];
    const float t1 = clamp01(e / 0.10f), t2 = clamp01((e - 0.10f) / 0.45f);
    for (int k = 0; k < 3; ++k) {
      const float m = cur.hor[k] + (cur.mid[k] - cur.hor[k]) * smooth01(t1);
      c[k] = m + (cur.zen[k] - m) * smooth01(powf(t2, 0.7f));
    }
    const float hz = expf(-(e / 0.012f) * (e / 0.012f)) * 0.25f;   // 水平線のもや
    grad[i * 3] = (uint8_t)(tone(c[0] + cur.hor[0] * hz) * 255); grad[i * 3 + 1] = (uint8_t)(tone(c[1] + cur.hor[1] * hz) * 255); grad[i * 3 + 2] = (uint8_t)(tone(c[2] + cur.hor[2] * hz) * 255);
  }
  for (int i = gi0; i < NE; ++i) {                // あいだを直線でつなぐ
    const int a = gi0 + ((i - gi0) & ~3), b = a + 4 < NE ? a + 4 : a;
    if (i == a || b == a) { if (i != a) for (int k = 0; k < 3; ++k) grad[i * 3 + k] = grad[a * 3 + k]; continue; }
    const int t = i - a;
    for (int k = 0; k < 3; ++k) grad[i * 3 + k] = (uint8_t)((grad[a * 3 + k] * (4 - t) + grad[b * 3 + k] * t + 2) >> 2);
  }
  for (int i = 0; i < gi0; ++i) { grad[i * 3] = grad[eIdx(0.0f) * 3]; grad[i * 3 + 1] = grad[eIdx(0.0f) * 3 + 1]; grad[i * 3 + 2] = grad[eIdx(0.0f) * 3 + 2]; }
  // 日のまわりの光(距離の2乗 → 足す光)。日が沈んでも、地平線の下からの光が少し残る
  glowK = NGLOW / 0.30f;
  for (int i = 0; i < NGLOW; i = i < 16 ? i + 1 : i + 4) {   // 日の円盤のある真ん中だけ1段ずつ、あとは 4 段おき
    const float d2 = i / glowK, d = sqrtf(d2);
    const float disk = sunE > -0.02f ? (d < 0.011f ? 2.2f : 2.2f * expf(-powf((d - 0.011f) / 0.0025f, 2))) : 0.0f;
    const float g = cur.glow * (0.75f * expf(-d / 0.025f) + 0.30f * expf(-d / 0.09f) + 0.12f * expf(-d / 0.30f)) + disk;
    glR[i] = (uint8_t)(tone(g * cur.sun[0]) * 255); glG[i] = (uint8_t)(tone(g * cur.sun[1]) * 255); glB[i] = (uint8_t)(tone(g * cur.sun[2]) * 255);
    glW[i] = (uint8_t)(255 * clamp01(g * 0.9f));
  }
  for (int i = 17; i < NGLOW; ++i) {              // 4 段おきのあいだを直線でつなぐ
    const int a = 16 + ((i - 16) & ~3), b = a + 4 < NGLOW ? a + 4 : a, t = i - a;
    if (t == 0) continue;
    if (b == a) { glR[i] = glR[a]; glG[i] = glG[a]; glB[i] = glB[a]; glW[i] = glW[a]; continue; }
    glR[i] = (uint8_t)((glR[a] * (4 - t) + glR[b] * t + 2) >> 2); glG[i] = (uint8_t)((glG[a] * (4 - t) + glG[b] * t + 2) >> 2);
    glB[i] = (uint8_t)((glB[a] * (4 - t) + glB[b] * t + 2) >> 2); glW[i] = (uint8_t)((glW[a] * (4 - t) + glW[b] * t + 2) >> 2);
  }
}

// ---------------------------------------------------------------- 空の色
// 1行ぶんの準備(雲の面の上では、1行は同じ奥行き)
struct SkyRow { int ei; float e; const uint16_t* crow; int cm; float uK, uOff; int fadeA, haze8; };
static inline void skyRowSetup(SkyRow& R, float e) {
  R.e = e; R.ei = eIdx(e); R.crow = nullptr;
  if (e <= 0.0015f) return;
  const float tt = invE[R.ei];                    // 雲の面までの遠さ
  float f = tt * tt / FOC * (CTX / CLW) * 0.6f;   // 1行で進む模様の点の数 → 縮めた絵の段
  int l = 0;
  while (f > 1.0f && l < NCL - 1) { f *= 0.5f; ++l; }
  const int n = CTX >> l;
  const float vv = tt * (1.0f / CLW) + cv;
  R.crow = ctex[l] + ((int)(vv * n) & (n - 1)) * n; R.cm = n - 1;
  R.uK = tt / (CLW * FOC) * n; R.uOff = (cu + 0.3f) * n + n * 64.0f;
  R.fadeA = (int)(255 * smooth01(e / 0.012f));    // 水平線のすぐ上は、雲が消える
  R.haze8 = (int)(255 * (smooth01(e / 0.09f) * 0.85f + 0.15f));   // 遠い雲は空の色にかすむ
}
// xs: 向き(画面の点。見ている向きを足したもの)。stars: 星も描くか
static inline __attribute__((always_inline)) void skyPix(const SkyRow& R, float xs, bool stars, int& r, int& g, int& b) {
  const int ei = R.ei; const float e = R.e;
  r = grad[ei * 3]; g = grad[ei * 3 + 1]; b = grad[ei * 3 + 2];
  const float a = xs * iFOC;
  const float dx = a - SUNX, de = e - sunE, d2 = dx * dx + de * de;
  int gw = 0;
  const int gi = (int)(d2 * glowK);
  if (gi < NGLOW) { r += glR[gi]; g += glG[gi]; b += glB[gi]; gw = glW[gi]; }
  if (R.crow) {                                   // 雲(横は2点の間をなめらかに)
    const float uf = xs * R.uK + R.uOff;
    const int ui = ffloorI(uf), fu = (int)((uf - (float)ui) * 256.0f);
    const uint16_t c0 = R.crow[ui & R.cm], c1 = R.crow[(ui + 1) & R.cm];
    const int A = (c0 & 255) + ((((c1 & 255) - (c0 & 255)) * fu) >> 8);
    if (A > 2) {
      const int L = (c0 >> 8) + ((((c1 >> 8) - (c0 >> 8)) * fu) >> 8);
      int li = (L >> 2) + (gw >> 3); li = li > 63 ? 63 : li;
      const int sl = (silT[A] * (90 + gw)) >> 8;  // ふちの光(日に近いほど強い)
      int cr = cLUT[li * 3] + ((sunC8[0] * sl) >> 8), cg = cLUT[li * 3 + 1] + ((sunC8[1] * sl) >> 8), cb = cLUT[li * 3 + 2] + ((sunC8[2] * sl) >> 8);
      cr = r + (((cr - r) * R.haze8) >> 8); cg = g + (((cg - g) * R.haze8) >> 8); cb = b + (((cb - b) * R.haze8) >> 8);
      const int aa = (A * R.fadeA) >> 8;
      r += ((cr - r) * aa) >> 8; g += ((cg - g) * aa) >> 8; b += ((cb - b) * aa) >> 8;
    }
  }
  const int ii = (int)((a + ISA) * (ISN / (2 * ISA)));   // 島
  if (ii >= 0 && ii < ISN) {
    const float h = isle[ii];
    if (h > 0 && e < h && e > -0.002f) { r = islC[0] + (gw >> 4); g = islC[1] + (gw >> 5); b = islC[2] + (gw >> 5); }
  }
  if (stars && e > 0.03f) {                       // 星
    const uint32_t hh = hash32((uint32_t)((int)xs >> 1) * 7919u ^ (uint32_t)((int)(e * FOC) >> 1) * 104729u);
    if ((hh & 511) < 2) { const int sb = (int)(((hh >> 12) & 255) * starW) * (int)min(1.0f, e * 12.0f); r += sb; g += sb; b += sb + (sb >> 3); }
  }
}
// 映り込み用の空(横は RS 点おき、縦は水平線の近くほど細かく)。行 i は高さ EMX·((i+0.5)/RR)²
static void reflRows(int j) {
  const float x0 = pan - S * 0.5f - RM;
  for (int i = j * RJ; i < min(RR, (j + 1) * RJ); ++i) {
    const float u = (i + 0.5f) / RR;
    SkyRow R; skyRowSetup(R, EMX * u * u);
    uint16_t* o = refl + i * RC;
    for (int c = 0; c < RC; ++c) {
      int r, g, b; skyPix(R, x0 + c * (float)RS, false, r, g, b);
      r = r > 255 ? 255 : r; g = g > 255 ? 255 : g; b = b > 255 ? 255 : b;
      o[c] = (uint16_t)(((r + 4 > 255 ? 255 : r + 4) >> 3) << 11 | ((g + 2 > 255 ? 255 : g + 2) >> 2) << 5 | ((b + 4 > 255 ? 255 : b + 4) >> 3));
    }
  }
}
static void doRefl() {                            // 映り込み用の空の仕事を、空いている方のコアがとる
  for (;;) {
    int j = __atomic_load_n(&reflNext, __ATOMIC_ACQUIRE);
    if (j >= (RR + RJ - 1) / RJ) break;
    if (!__atomic_compare_exchange_n(&reflNext, &j, j + 1, false, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) continue;
    reflRows(j);
    __atomic_add_fetch(&reflDone, 1, __ATOMIC_ACQ_REL);
  }
  while (__atomic_load_n(&reflDone, __ATOMIC_ACQUIRE) < (RR + RJ - 1) / RJ) {}
}

// ---------------------------------------------------------------- 描く
static void IRAM_ATTR renderBand(int kb, uint16_t* out, int bx, int bw) {
  const int y0 = kb * BAND, h = min(BAND, S - y0);
  const bool stars = starW > 0.02f;
  const float cx = S * 0.5f;
  for (int rr = 0; rr < h; ++rr) {
    const int y = y0 + rr;
    uint16_t* o = out + rr * bw - bx;
    const int sa = max((int)spanA[y], bx), sb = min((int)spanB[y], bx + bw - 1);
    for (int x = bx; x < sa; ++x) o[x] = 0;
    for (int x = sb + 1 < bx ? bx : sb + 1; x < bx + bw; ++x) o[x] = 0;
    if (sa > sb) continue;
    const float e = (hzRow - (y + 0.5f)) / FOC;
    // 1点おきに計算して、あいだは左右のまん中にする(空も海も、なめらかなので)
#define NS_HALF(CALC)                                                                                     \
    { int r0, g0, b0; { const int x = sa; CALC; r0 = r; g0 = g; b0 = b; }                                  \
      int x = sa;                                                                                         \
      while (x < sb) {                                                                                    \
        const int x2 = min(x + 2, sb); int r2, g2, b2;                                                     \
        { const int x = x2; CALC; r2 = r; g2 = g; b2 = b; }                                                \
        o[x] = packD(r0, g0, b0, x, y);                                                                   \
        if (x2 == x + 2) o[x + 1] = packD((r0 + r2) >> 1, (g0 + g2) >> 1, (b0 + b2) >> 1, x + 1, y);       \
        x = x2; r0 = r2; g0 = g2; b0 = b2;                                                                \
      }                                                                                                   \
      o[sb] = packD(r0, g0, b0, sb, y); }
    if (e >= 0) {                                 // 空
      SkyRow SR; skyRowSetup(SR, e);
      NS_HALF(int r; int g; int b; skyPix(SR, x - cx + pan, stars, r, g, b))
      continue;
    }
    // 海: 手前ほど近い。波の模様を、遠近をつけて引く
    const float q = fmaxf(-e, 0.0015f), z = 1.0f / q;   // z: 目の高さを 1 とした遠さ
    const float rowDy = y + 0.5f - hzRow;         // 水平線からの下がり(画面の点)
    const float fu = z / FOC * 2.0f, fv = z * z / FOC;   // 1点(1点おきに計算)、1行で進む海の上の長さ
    const int16_t* wrow[2] = {nullptr, nullptr}; int wm[2] = {0, 0}; float wK[2] = {0, 0}, wO[2] = {0, 0};
    float var = 0;
    for (int k = 0; k < 2; ++k) {                 // 波の模様の段を選ぶ(細かすぎる所は使わず、消えた分は「荒さ」として日の光の道を太くする)
      const float L = k ? WL1 : WL0;
      float f = fmaxf(fu, fv * 0.5f) * (WT / L);
      int l = 0;
      while (f > 1.0f && l < NWL) { f *= 0.5f; ++l; }
      var += wvar[k][l];
      if (l >= NWL) continue;
      const int n = WT >> l;
      const float vv = z / L + (k ? w2v : w1v);
      wrow[k] = wtex[k][l] + ((int)(vv * n + n * 64.0f) & (n - 1)) * n; wm[k] = n - 1;
      wK[k] = z / (FOC * L) * n; wO[k] = (k ? w2u : w1u) * n + n * 64.0f;
    }
    const float s0 = 0.022f * 0.022f, s2 = s0 + 4.0f * var;
    const float specK = 64.0f / s2;               // 日からのずれ(2乗)→ きらめきの表
    const int specG = sunVis > 0.01f ? (int)(256.0f * 2.6f * powf(s0 / s2, 0.35f) * sunVis) : 0;
    const float invL = 255.0f / sqrtf(1.0f + q * q);
    const float rx0 = -(pan - S * 0.5f - RM) * (1.0f / RS) + 0.5f;   // 向き(画面の点)→ 映り込み用の空の列
    const float SLS = 0.40f / 127.0f;
    const float ripK = smooth01(q / 0.06f);       // 水平線のすぐ近くの波紋は弱く
    const float zF = z / FOC;                     // 画面の点 → 海の上の横の位置
    struct RowRip { float u0, dz, rad, lam0, iLead, iSpan, iRad, amp, wo, wi; };   // この行にかかる波紋だけ(横の範囲も先に出しておく)
    RowRip rrp[MAXRIP]; int nRR = 0;
    for (int k = 0; k < MAXRIP; ++k) {
      const Ripple& R = rip[k];
      if (!R.on) continue;
      // 水に落ちたしずくの波紋: 先頭の輪がいちばん長く、うしろへ行くほど輪の間がつまる。広がるほど弱まる
      const float rad = R.t * R.sc * 0.20f + R.sc * 0.01f, lam0 = R.sc * 0.060f;
      const float lead = lam0 * 0.6f, span = rad * 0.55f + lam0;
      const float dz = (z - R.v0) * RIP_DZ, outer = rad + lead;   // 奥行きは少し縮めて、輪を見やすく(平たくなりすぎない)
      if (fabsf(dz) >= outer) continue;
      const float inner = rad - span;
      RowRip& o = rrp[nRR++];
      o.u0 = R.u0; o.dz = dz; o.rad = rad; o.lam0 = lam0; o.iLead = 1.0f / lead; o.iSpan = 1.0f / span; o.iRad = 1.0f / rad;
      o.amp = R.a * ripK * 0.55f / sqrtf(1.0f + rad / (lam0 * 6.0f));
      o.wo = sqrtf(outer * outer - dz * dz);
      o.wi = inner > fabsf(dz) ? sqrtf(inner * inner - dz * dz) : 0.0f;
    }
    auto seaPix = [&](int x, int& r, int& g, int& b) {
      const float xs = x - cx + pan;
      float sx = 0, sz = 0;                       // 波の傾き(横 / 奥)
      for (int k = 0; k < 2; ++k) {
        if (!wrow[k]) continue;
        const int16_t w = wrow[k][(int)(xs * wK[k] + wO[k]) & wm[k]];
        sx += (int8_t)(w & 255); sz += (w >> 8);
      }
      sx *= SLS; sz *= SLS;
      if (nRR) {                                  // 波紋(海の上の丸。遠いほど小さく、平たく見える)
        const float px = xs * zF;
        for (int k = 0; k < nRR; ++k) {
          const RowRip& R = rrp[k];
          const float dx = px - R.u0, adx = fabsf(dx);
          if (adx >= R.wo || adx < R.wi) continue;
          const float d2r = dx * dx + R.dz * R.dz + 1e-6f, id = frsqrtF(d2r), d = d2r * id, sd = d - R.rad;
          float env = sd > 0 ? 1.0f - sd * R.iLead : 1.0f + sd * R.iSpan;   // 外側は急に、内側はゆっくり消える
          if (env <= 0) continue;
          env *= env;
          const float dr = d * R.iRad, lam = R.lam0 * (0.45f + 0.55f * (dr < 1.0f ? dr : 1.0f));   // うしろの輪ほど短い
          float ph = sd * frcpF(lam); ph -= (float)ffloorI(ph + 0.5f);
          const float sw = ph * (8.0f - 16.0f * fabsf(ph)) * env * R.amp;          // 軽い sin
          sx += sw * dx * id; sz += sw * R.dz * id * (1.0f / RIP_DZ);
        }
      }
      const float a = xs * iFOC;
      // 鏡のように反射した向き。向こうへ傾いた面は、ほかの波のかげになって見えにくいので、映る高さは下げすぎない
      const float ra = a - 2.0f * q * sx, re0 = q + 2.0f * sz + 2.0f * a * sx, reMin = q * 0.6f + 0.0005f, re = re0 > reMin ? re0 : reMin;
      int col = (int)(ra * focRS + rx0); col = col < 0 ? 0 : (col >= RC ? RC - 1 : col);
      const uint16_t c = refl[rRow[eIdx(re)] * RC + col];
      int rr = (c >> 11) << 3, gg = ((c >> 5) & 63) << 2, bb = (c & 31) << 3;
      const float qs = q + sz, qm = q * 0.6f;
      int fi = (int)((qs > qm ? qs : qm) * invL); fi = fi < 0 ? 0 : (fi > 255 ? 255 : fi);
      const int F = fresT[fi];
      r = watC[0] + (((rr - watC[0]) * F) >> 8); g = watC[1] + (((gg - watC[1]) * F) >> 8); b = watC[2] + (((bb - watC[2]) * F) >> 8);
      if (specG) {                                // 日のきらめき
        const float da = ra - SUNX, dd = re - sunE;
        const int si = (int)((da * da + dd * dd) * specK);
        if (si < NSPEC) { const int sp = (specT[si] * specG) >> 8; r += (sunC8[0] * sp) >> 8; g += (sunC8[1] * sp) >> 8; b += (sunC8[2] * sp) >> 8; }
      }
    };
    NS_HALF(int r; int g; int b; seaPix(x, r, g, b))
  }
#undef NS_HALF
  // 鳥(小さな「く」の字のシルエットが、はばたきながら横切る)
  if (birdW > 0.02f) {
    for (int i = 0; i < MAXBIRD; ++i) {
      const Bird& B = bird[i];
      if (!B.on) continue;
      const int bxp = (int)(B.x - pan + cx), byp = (int)(hzRow - B.e * FOC);
      const int sz = (int)B.sz;
      if (byp + sz + 2 < y0 || byp - sz - 2 >= y0 + h) continue;
      const float fl = sinf(B.ph);
      for (int k = -sz; k <= sz; ++k) {
        const int yy = byp - (int)(fabsf((float)k) * 0.55f * fl) + (abs(k) > sz / 2 ? 0 : 0);
        const int xx = bxp + k;
        for (int t = 0; t < 2; ++t) {
          const int py = yy + t;
          if (py < y0 || py >= y0 + h || py < 0 || py >= S || xx < spanA[py] || xx > spanB[py] || xx < bx || xx >= bx + bw) continue;
          if (t == 1 && abs(k) > 1) continue;
          uint16_t* o = out + (py - y0) * bw + (xx - bx);
          *o = pack(islC[0] + 6, islC[1] + 4, islC[2] + 6);
        }
      }
    }
  }
}
static int takeBand(int limit) {
  int k = __atomic_load_n(&nextBand, __ATOMIC_ACQUIRE);
  while (k < NB && k <= limit) {
    if (__atomic_compare_exchange_n(&nextBand, &k, k + 1, false, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) {
      if (bandW[k] > 0) renderBand(k, band[k % NSLOT], bandX[k], bandW[k]);
      __atomic_store_n(&bandReady[k], 1, __ATOMIC_RELEASE);
      return k;
    }
  }
  return -1;
}
static void renderFrame() {
  for (int k = 0; k < NB; ++k) bandReady[k] = 0;
  __atomic_store_n(&doneUpTo, -1, __ATOMIC_RELEASE);
  __atomic_store_n(&nextBand, 0, __ATOMIC_RELEASE);
  __atomic_store_n(&reflDone, 0, __ATOMIC_RELEASE);
  __atomic_store_n(&reflNext, 0, __ATOMIC_RELEASE);
  uint32_t busy = 0, send = 0;
  xSemaphoreGive(semGo);
  { const uint32_t t0 = micros(); doRefl(); busy += micros() - t0; }
  if (!fbMode) M5.Display.startWrite();
  for (int k = 0; k < NB; ++k) {
    while (!__atomic_load_n(&bandReady[k], __ATOMIC_ACQUIRE)) {
      const uint32_t t0 = micros();
      if (takeBand(__atomic_load_n(&doneUpTo, __ATOMIC_ACQUIRE) + NSLOT) >= 0) busy += micros() - t0;
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
static void workerTask(void*) {
  for (;;) {
    xSemaphoreTake(semGo, portMAX_DELAY);
    uint32_t busy = 0;
    { const uint32_t t0 = micros(); doRefl(); busy += micros() - t0; }
    for (;;) {
      const int k = __atomic_load_n(&nextBand, __ATOMIC_ACQUIRE);
      if (k >= NB) break;
      if (k - NSLOT > __atomic_load_n(&doneUpTo, __ATOMIC_ACQUIRE)) continue;
      const uint32_t t0 = micros();
      if (takeBand(__atomic_load_n(&doneUpTo, __ATOMIC_ACQUIRE) + NSLOT) >= 0) busy += micros() - t0;
    }
    busyUs[0] = busy;
    xSemaphoreGive(semDone);
  }
}

// ---------------------------------------------------------------- 動き
static void addRipple(float sx, float sy) {       // 画面の点 → 海の上の位置に波紋
  const float q = (sy + 0.5f - hzRow) / FOC;
  if (q < 0.02f) return;                          // 水平線のすぐ近くは、遠すぎて見えない
  const float z = 1.0f / q;
  Ripple& R = rip[ripNext]; ripNext = (ripNext + 1) % MAXRIP;
  R.on = true; R.u0 = (sx - S * 0.5f + pan) / FOC * z; R.v0 = z; R.sc = z; R.t = 0; R.a = 1.0f;   // 大きさは遠さに合わせる(画面の上で同じくらいに広がる)
}
static void addBirds(float sx, float sy, int n, bool big) { // 群れ(画面の点のあたりから。big: タップしたときの、手前の大きな鳥)
  const float e0 = fmaxf(0.03f, (hzRow - sy) / FOC);
  const float dir = frand() < 0.5f ? -1.0f : 1.0f;
  for (int k = 0, i = 0; i < MAXBIRD && k < n; ++i) {
    if (bird[i].on) continue;
    Bird& B = bird[i];
    B.on = true; B.x = sx - S * 0.5f + pan + (frand() - 0.5f) * S * 0.10f - dir * k * S * 0.02f; B.e = e0 + (frand() - 0.5f) * 0.04f;
    B.vx = dir * S * (big ? 0.10f + 0.03f * frand() : 0.06f + 0.02f * frand()); B.ve = (frand() - 0.3f) * 0.01f; B.ph = frand() * 6.28f;
    B.sz = S * (big ? 0.016f + 0.008f * frand() : 0.008f + 0.006f * frand());
    ++k;
  }
}
static void stepTime() {                          // 時間を 1/8 周進める(まだ移りかわっている途中なら、残りに足す)
  jumpTot = jumpTot * (1.0f - jumpK) + 0.125f; jumpT = 0; jumpK = 0;
  Serial.printf("phase: %.2f -> %.2f\n", phase, fmodf(phase + jumpTot, 1.0f));
}
static void simulate(float dt) {
  simT += dt;
  const float spd = SPEEDS[speedIdx];
  phase += dt * spd / CYCLE_SEC;
  if (jumpTot != 0) {                             // ボタンで進めた分を、はじめと終わりはゆっくり足していく
    jumpT += dt;
    const float k = smooth01(jumpT / JUMP_SEC);
    phase += jumpTot * (k - jumpK); jumpK = k;
    if (jumpT >= JUMP_SEC) { jumpTot = 0; jumpK = 0; }
  }
  phase -= floorf(phase);
  // 日の高さ: 0 → 0.5 で沈み、0.5 → 1 でのぼる(夜はしばらく続く)
  const float p = phase < 0.5f ? phase / 0.5f : (1.0f - phase) / 0.5f;
  sunE = 0.20f - 0.46f * smooth01(p * 1.15f);
  static float lastE = 9.0f;                      // 色の表は、日が少し動いたときだけ作り直す
  if (fabsf(sunE - lastE) > 0.0015f) { buildFrameTables(); lastE = sunE; }   // 日が少し動いたときだけ(ESP32-S3 では表づくりが重い)
  cu += dt * 0.0040f * CLOUD_SPEED; cv -= dt * 0.0025f * CLOUD_SPEED; cu -= floorf(cu); cv -= floorf(cv);
  w1u += dt * 0.012f * WAVE_SPEED; w1v -= dt * 0.090f * WAVE_SPEED; w1u -= floorf(w1u); w1v -= floorf(w1v);   // 大きな波: ゆっくり手前へ
  w2u -= dt * 0.060f * WAVE_SPEED; w2v -= dt * 0.320f * WAVE_SPEED; w2u -= floorf(w2u); w2v -= floorf(w2v);   // 細かい波: 少し横へ、速く
  // 見ている向きと水平線(傾き)
  const float tp = tiltX * S * 0.40f, th = S * (0.53f + tiltY * 0.16f);
  const float a = 1.0f - expf(-dt * 3.0f);
  pan += (tp - pan) * a; hzRow += (th - hzRow) * a;
  for (int k = 0; k < MAXRIP; ++k) { Ripple& R = rip[k]; if (!R.on) continue; R.t += dt; R.a = expf(-R.t * 0.30f); if (R.t > 7.5f) R.on = false; }
  for (int i = 0; i < MAXBIRD; ++i) {
    Bird& B = bird[i];
    if (!B.on) continue;
    B.x += B.vx * dt; B.e += B.ve * dt; B.ph += dt * 9.0f;
    if (fabsf(B.x - pan) > S * 0.8f) B.on = false;
  }
  nextBirds -= dt;
  if (nextBirds <= 0) {                           // 明るいうちは、ときどき鳥が横切る
    if (birdW > 0.3f) { const float sx = frand() < 0.5f ? -S * 0.2f : S * 1.2f; addBirds(sx, S * (0.20f + 0.15f * frand()), 3 + (int)(frand() * 4), false); }
    nextBirds = 9.0f + 10.0f * frand();
  }
}

// ---------------------------------------------------------------- 表示
static void showMsg(const char* msg, float prog) {
  static M5Canvas* cvs = nullptr;
  static const char* text = "";
  const int w = 280, h = 64, x0 = ((W - w) / 2) & ~1, y0 = ((H - h) / 2) & ~1;
  if (msg) {
    text = msg;
    M5.Display.fillScreen(TFT_BLACK);
    if (!cvs) { cvs = new M5Canvas(&M5.Display); cvs->setPsram(true); cvs->setColorDepth(16); if (!cvs->createSprite(w, h)) { delete cvs; cvs = nullptr; } }
  }
  if (!cvs) return;
  cvs->fillScreen(TFT_BLACK);
  cvs->setTextDatum(middle_center);
  cvs->setFont(&fonts::lgfxJapanGothicP_16);
  cvs->setTextColor(M5.Display.color565(230, 190, 150));
  cvs->drawString(text, w / 2, 20);
  cvs->fillRect(40, 44, w - 80, 4, M5.Display.color565(54, 44, 50));
  cvs->fillRect(40, 44, (int)((w - 80) * clamp01(prog)), 4, M5.Display.color565(255, 190, 120));
  cvs->pushSprite(x0, y0);
  if (prog >= 1.0f) { delete cvs; cvs = nullptr; }
}
static void fail(const char* msg) {
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setTextColor(0xF800);
  M5.Display.setTextSize(2);
  M5.Display.setTextDatum(middle_center);
  M5.Display.drawString(msg, W / 2, H / 2);
  Serial.println(msg);
  while (true) nmIdle();
}

// ---------------------------------------------------------------- setup / loop
__attribute__((cold)) void setup() {   // cold: 1回だけ動くので、小さく作る
  auto cfg = M5.config();
  M5.begin(cfg);
  Serial.begin(115200);
  Serial.println("NullSunset v6");
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
  FOC = (float)S; iFOC = 1.0f / FOC; focRS = FOC / RS; hzRow = S * 0.53f;

  spanA = (int16_t*)malloc(S * 2); spanB = (int16_t*)malloc(S * 2);
  bandX = (int16_t*)malloc(NB * 2); bandW = (int16_t*)malloc(NB * 2);
  for (int i = 0; i < NSLOT; ++i) band[i] = (uint16_t*)heap_caps_malloc((size_t)S * BAND * 2, MALLOC_CAP_DMA);
  for (int l = 0; l < NCL; ++l) ctex[l] = (uint16_t*)ps_malloc(sizeof(uint16_t) * (CTX >> l) * (CTX >> l));
  for (int k = 0; k < 2; ++k) for (int l = 0; l < NWL; ++l) wtex[k][l] = (int16_t*)ps_malloc(sizeof(int16_t) * (WT >> l) * (WT >> l));
  RM = S / 8; RC = (S + 2 * RM) / RS + 2;
  refl = (uint16_t*)heap_caps_malloc(sizeof(uint16_t) * RR * RC, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);   // よく引くので、内蔵メモリに置けるなら置く
  if (!refl) refl = (uint16_t*)ps_malloc(sizeof(uint16_t) * RR * RC);
  invE = (float*)malloc(NE * sizeof(float)); grad = (uint8_t*)malloc(NE * 3); rRow = (uint8_t*)malloc(NE);
  isle = (float*)malloc(ISN * sizeof(float));
  cLUT = (uint8_t*)malloc(64 * 3); silT = (uint8_t*)malloc(256); fresT = (uint8_t*)malloc(256); specT = (uint8_t*)malloc(NSPEC);
  bool ok = spanA && spanB && bandX && bandW && refl && invE && grad && rRow && isle && cLUT && silT && fresT && specT;
  for (int l = 0; l < NCL; ++l) ok = ok && ctex[l];
  for (int k = 0; k < 2; ++k) for (int l = 0; l < NWL; ++l) ok = ok && wtex[k][l];
  for (int i = 0; i < NSLOT; ++i) ok = ok && band[i];
  if (!ok) fail("Memory error");
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
  imuOK = TILT && M5.Imu.isEnabled();
  semGo = xSemaphoreCreateBinary();
  semDone = xSemaphoreCreateBinary();
  if (!semGo || !semDone) fail("Memory error (task)");
  xTaskCreatePinnedToCore(workerTask, "sunset", 6144, nullptr, 1, nullptr, 0);

  const uint32_t t0 = millis();
  showMsg("海と空を用意しています…", 0.0f);
  buildCloud();
  showMsg(nullptr, 0.5f);
  buildWaves();
  buildFixed();
  showMsg(nullptr, 1.0f);
  Serial.printf("build: %u ms\n", (unsigned)(millis() - t0));
#if defined(ESP_PLATFORM)
  Serial.printf("memory: refl %s (%u B), cloud/waves in PSRAM, internal free %u B (largest %u B)\n", esp_ptr_external_ram(refl) ? "PSRAM" : "internal", (unsigned)(sizeof(uint16_t) * RR * RC),
                (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL), (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL));
#endif
  rnd = (uint32_t)esp_random() | 1u;
  for (int i = 0; i < MAXRIP; ++i) rip[i].on = false;
  for (int i = 0; i < MAXBIRD; ++i) bird[i].on = false;
  simulate(0.0f);
  addBirds(S * 0.15f, S * 0.30f, 5, false);
}

void loop() {
  static uint32_t last = millis(), fpsT = millis(), lastTapMs = 0;
  static int frames = 0;
  static bool was = false;
  static uint32_t tPrev = micros(), sumIn = 0, sumSim = 0, sumDraw = 0, sumAll = 0;
  const uint32_t t0 = micros();
  sumAll += t0 - tPrev; tPrev = t0;
  M5.update();
  auto t = M5.Touch.getDetail();
  sumIn += micros() - t0;
  uint32_t now = millis();
  const bool corner = !roundScr && t.x < 56 && t.y < 56;
  bool dbl = false;
  if (roundScr) {
    if (M5.BtnA.wasClicked()) { stepTime(); }
    if (M5.BtnB.wasClicked()) { speedIdx = (speedIdx + 1) % 3; Serial.printf("speed: x%.2f\n", SPEEDS[speedIdx]); }
  } else {
    if (M5.BtnPWR.wasClicked()) { stepTime(); }
    if (t.wasPressed() && !corner) {
      if (now - lastTapMs < 350) { speedIdx = (speedIdx + 1) % 3; Serial.printf("speed: x%.2f\n", SPEEDS[speedIdx]); lastTapMs = 0; dbl = true; } else lastTapMs = now;
    }
  }
  float dt = (now - last) / 1000.0f; last = now;
  if (dt > 0.1f) dt = 0.1f;

  if (imuOK) {
    float ax, ay, az;
    M5.Imu.update();
    if (M5.Imu.getAccel(&ax, &ay, &az)) {
      float sx = -ax, sy = ay;
#if defined(ESP_PLATFORM)
      if (TILT_STOPWATCH_FIX && M5.getBoard() == m5::board_t::board_M5StopWatch) { sx = -ay; sy = -ax; }
#endif
      if (TILT_SWAP) { const float tmp = sx; sx = sy; sy = tmp; }
      if (TILT_FLIP_X) sx = -sx;
      if (TILT_FLIP_Y) sy = -sy;
      if (levelN < 20) { ++levelN; levelX += (sx - levelX) / levelN; levelY += (sy - levelY) / levelN; }
      else if (TILT_RELEVEL > 0 && fabsf(sx - levelX) < TILT_RELEVEL_MAX && fabsf(sy - levelY) < TILT_RELEVEL_MAX) {
        const float b = 1.0f - expf(-dt / TILT_RELEVEL); levelX += (sx - levelX) * b; levelY += (sy - levelY) * b;
      }
      sx -= levelX; sy -= levelY;
      sx = sx < -1 ? -1 : sx > 1 ? 1 : sx; sy = sy < -1 ? -1 : sy > 1 ? 1 : sy;
      const float a = 1.0f - expf(-dt * 4.0f);
      tiltX += (sx - tiltX) * a; tiltY += (sy - tiltY) * a;
    }
  }

  // 触る: タップ(海 = 波紋 / 空 = 鳥)、なぞる(空 = 時間を前後に動かす / 海 = 指のあとに波紋が続く)
  static int pressX = 0, pressY = 0, lastX = 0, lastY = 0;
  static bool moved = false, onSea = false;
  static float wake = 0;
  const int tx = t.x - XOFF, ty = t.y - YOFF;
  const bool touching = t.isPressed() && !corner && tx >= 0 && ty >= 0 && tx < S && ty < S;
  if (touching && !was) { pressX = lastX = tx; pressY = lastY = ty; moved = false; onSea = ty > hzRow + 2; wake = 0; }
  if (touching && was) {
    if (!moved && ((tx - pressX) * (tx - pressX) + (ty - pressY) * (ty - pressY)) > (int)(S * S * 0.002f)) { moved = true; if (onSea) addRipple((float)pressX, (float)pressY); }
    if (moved) {
      if (!onSea) { phase += (tx - lastX) * (0.25f / S); phase -= floorf(phase); jumpTot = 0; jumpK = 0; }   // 空: 画面の幅で 1/4 周
      else {
        wake += sqrtf((float)((tx - lastX) * (tx - lastX) + (ty - lastY) * (ty - lastY)));
        if (wake > S * 0.07f) { addRipple((float)tx, (float)ty); wake = 0; }
      }
    }
    lastX = tx; lastY = ty;
  }
  if (!touching && was && !moved && !dbl) {
    if (onSea) addRipple((float)pressX, (float)pressY);   // 海: 波紋
    else addBirds((float)pressX, (float)pressY, 4 + (int)(frand() * 4), true);   // 空: 鳥の群れ
  }
  was = touching;

  const uint32_t tSim0 = micros();
  simulate(dt);
  const uint32_t tDraw0 = micros();
  sumSim += tDraw0 - tSim0;
  renderFrame();
  sumDraw += micros() - tDraw0;
  if (++frames, now - fpsT >= 3000) {
    Serial.printf("fps: %.1f  (time %.2f, sun %+.2f, x%.2f)  core0 %.1f ms  core1 %.1f ms  send %.1f ms\n", frames * 1000.0f / (now - fpsT), phase, sunE, SPEEDS[speedIdx],
                  busyUs[0] / 1000.0f, busyUs[1] / 1000.0f, sendUs / 1000.0f);
    const float k = 1.0f / (1000.0f * frames);
    Serial.printf("time: input %.1f  sim %.1f  draw %.1f  (frame %.1f ms)\n", sumIn * k, sumSim * k, sumDraw * k, sumAll * k);
    sumIn = sumSim = sumDraw = sumAll = 0;
    frames = 0; fpsT = now;
  }
}

// 共通の置き場に置いたものは、0 で始まる単純な型だけ(コンストラクタが要らないもの)
static_assert(std::is_trivially_default_constructible<uint8_t>::value, "NM_ARENA: uint8_t");
static_assert(std::is_trivially_default_constructible<Ripple>::value, "NM_ARENA: Ripple");
static_assert(std::is_trivially_default_constructible<Bird>::value, "NM_ARENA: Bird");
static constexpr size_t OVL_END = nmAlignTo(nmOv6, 16);
static_assert(OVL_END <= NM_ARENA_SIZE, "NM_ARENA_SIZE (nm_common.h) を大きくしてください");
extern const size_t OVL_SIZE = OVL_END;   // 起動時の確認用
}  // namespace NullSunset
