// NullTwin  -  NullMenu 用モジュール(元のスケッチ NullTwin_v6 を名前空間に入れたもの)
// 0 で始まる変数(配列も)は、ほかの作品と共通の置き場(NM_ARENA)に置いています。
// 一度に動くのは1つだけ(切り替えるときは再起動)なので、重なっていても問題ありません。
#include "nm_common.h"
#include <algorithm>
#pragma GCC optimize("O3")

namespace NullTwin {
static constexpr size_t nmOv0 = 0;

// NullTwin v6  -  デジタルの分身。光の小さな立方体でできた人の姿が、走査線で組み上がっては崩れていく
//   null²(大阪・関西万博のパビリオン)から受けたイメージで作った、非公式の習作です。作品の映像・名前・データは使っていません。
//   M5Stack StopWatch 用(丸い画面)。CoreS3 系でも動きます(自動で切り替わります)。
//
// 画面の中:
//   ・光の小さな立方体(ボクセル)でできた人の姿が、ゆっくり回りながら、かすかに呼吸している
//   ・走査線が上下に行き来し、上がるときは通ったところから形が組み上がり、下がるときは上から崩れていく
//   ・床には、うっすら鏡像が映る
//
// 傾けると(本体の傾きセンサー):
//   ・左右に傾けると、分身がそちらへ回る(ドラッグしていないとき)
//   ・傾きの向きが逆のときは、調整用の TILT_FLIP_X / TILT_FLIP_Y / TILT_SWAP を変える。Serial に tilt: の値が出る
//
// 触ると:
//   ・ドラッグ: 分身を回す(離すと、その勢いが少し残って、いつもの回り方に戻る)
//   ・タップ: 立方体がほどけて散り、また集まってくる
//
// 操作:
//   StopWatch … ボタンA 短押し: 姿勢(立つ / 両手を広げる)   ボタンA 長押し: 色(青 / 金 / 白)   ボタンB: 明るさ
//   CoreS3系  … 画面の上半分をダブルタップ: 姿勢   下半分をダブルタップ: 色   電源ボタン短押し: 明るさ
//
// 必要なもの:
//   ボードマネージャー M5Stack >= 3.3.7 / ボード: M5StopWatch(PSRAM は OPI)、CoreS3 なら M5CoreS3
//   ライブラリ M5Unified >= 0.2.16, M5GFX >= 0.2.21
//
// しくみ:
//   人の形は、頭・胴・腕・脚を楕円体と円柱で表して、その表面に点を置きます。点は、ばねで自分の場所へ引かれます。
//   毎フレーム、点を回して奥から順に並べ、その順番のまま画面の8行ずつの帯ごとに振り分けて、小さな四角で描きます。
//   2つのコアで帯を描き、StopWatch では帯ごとに丸の中だけを画面へ送ります。



// ---- 調整用 ----------------------------------------------------------------
static constexpr int   NVOX      = 1700;    // 立方体の数
static constexpr float SCAN_SPD  = 0.32f;   // 走査線の速さ
static constexpr float SPIN      = 0.18f;   // いつもの回る速さ(ラジアン/秒)
static constexpr float MIRROR    = 0.18f;   // 床の鏡像の濃さ
static constexpr bool  DOUBLE_TAP_UI = true;
// ---------------------------------------------------------------------------
static constexpr int NPART    = 11;
static constexpr int MAXRECT  = NVOX * 2;
static constexpr int MAXRENT  = MAXRECT * 2;

// ---- 傾き(本体の傾きセンサー)-----------------------------------------------
static constexpr bool  TILT        = true;    // 傾きで動かすか(false にすると、傾きを使わない)
static constexpr float TILT_GAIN   = 1.0f;    // 傾きの効き方(大きいほど、少し傾けただけで大きく動く)
static constexpr bool  TILT_FLIP_X = false;   // 左右が逆に動くときは true
static constexpr bool  TILT_FLIP_Y = false;   // 上下が逆に動くときは true
static constexpr bool  TILT_SWAP   = false;   // 左右と上下が入れかわっているときは true
static constexpr bool  TILT_STOPWATCH_FIX = true;   // StopWatch はセンサーが 90 度回ってついているので、自動で読みかえる
static constexpr float TILT_DEAD   = 0.08f;   // これより小さい傾きは無視する(机に置いたときや、手のふるえで動かないように)
static constexpr float TILT_RELEVEL = 15.0f;   // 同じ角度で持ち続けると、何秒くらいでそこを「水平」にするか(0 にすると、起動したときの姿勢のまま)
static constexpr float TILT_RELEVEL_MAX = 0.25f;   // これより大きく傾けているあいだは、水平を合わせなおさない(大きく傾けて戻すと逆に動くのを防ぐ)
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------- 型(関数より前にまとめて書く)
struct Col { float r, g, b; };
struct Input { bool touching; bool tapped; float tx, ty; float dt; float tiltX, tiltY; };   // tilt: 画面の上で「下」がどちらか(-1..1 くらい)
struct Part { int t; float c[3]; float r3[3]; float a[3]; float b[3]; float r; };   // t=0: 楕円体 / 1: 円柱
struct VRect { int16_t x, y; uint8_t s, blend; uint16_t col; };                      // blend: 0 = 上書き / 1..16 = 混ぜる割合
struct Scratch { int dummy; };

static const char* SKETCH_NAME = "NullTwin v6";

static constexpr int BAND    = 8;
static constexpr int NSLOT   = 4;
static constexpr int MAXNB   = 64;

static constexpr size_t nmOa0 = nmAlignTo(nmOv0, alignof(int)), nmOv1 = nmOa0 + sizeof(int); static int& W = *reinterpret_cast<int*>(NM_ARENA + nmOa0);
static constexpr size_t nmOa1 = nmAlignTo(nmOv1, alignof(int)), nmOv2 = nmOa1 + sizeof(int); static int& H = *reinterpret_cast<int*>(NM_ARENA + nmOa1);
static constexpr size_t nmOa2 = nmAlignTo(nmOv2, alignof(int)), nmOv3 = nmOa2 + sizeof(int); static int& S = *reinterpret_cast<int*>(NM_ARENA + nmOa2);
static constexpr size_t nmOa3 = nmAlignTo(nmOv3, alignof(int)), nmOv4 = nmOa3 + sizeof(int); static int& XOFF = *reinterpret_cast<int*>(NM_ARENA + nmOa3);
static constexpr size_t nmOa4 = nmAlignTo(nmOv4, alignof(int)), nmOv5 = nmOa4 + sizeof(int); static int& YOFF = *reinterpret_cast<int*>(NM_ARENA + nmOa4);
static constexpr size_t nmOa5 = nmAlignTo(nmOv5, alignof(int)), nmOv6 = nmOa5 + sizeof(int); static int& NB = *reinterpret_cast<int*>(NM_ARENA + nmOa5);
static constexpr size_t nmOa6 = nmAlignTo(nmOv6, alignof(float)), nmOv7 = nmOa6 + sizeof(float); static float& Sc = *reinterpret_cast<float*>(NM_ARENA + nmOa6);
static constexpr size_t nmOa7 = nmAlignTo(nmOv7, alignof(float)), nmOv8 = nmOa7 + sizeof(float); static float& VR = *reinterpret_cast<float*>(NM_ARENA + nmOa7);  // 中心 / 見えている半径(ピクセル)
static constexpr size_t nmOa8 = nmAlignTo(nmOv8, alignof(float)), nmOv9 = nmOa8 + sizeof(float); static float& vx0 = *reinterpret_cast<float*>(NM_ARENA + nmOa8);
static constexpr size_t nmOa9 = nmAlignTo(nmOv9, alignof(float)), nmOv10 = nmOa9 + sizeof(float); static float& vx1 = *reinterpret_cast<float*>(NM_ARENA + nmOa9);
static constexpr size_t nmOa10 = nmAlignTo(nmOv10, alignof(float)), nmOv11 = nmOa10 + sizeof(float); static float& vy0 = *reinterpret_cast<float*>(NM_ARENA + nmOa10);
static constexpr size_t nmOa11 = nmAlignTo(nmOv11, alignof(float)), nmOv12 = nmOa11 + sizeof(float); static float& vy1 = *reinterpret_cast<float*>(NM_ARENA + nmOa11);  // 見えている範囲(S×S の中の座標)
static constexpr size_t nmOa12 = nmAlignTo(nmOv12, alignof(bool)), nmOv13 = nmOa12 + sizeof(bool); static bool& isRound = *reinterpret_cast<bool*>(NM_ARENA + nmOa12);
static uint8_t*  bandVis;
static uint16_t* slotBuf[NSLOT];
static int16_t*  spanS;
static int16_t*  spanE;
static int16_t*  bandX0;
static int16_t*  bandW;
static constexpr size_t nmOa13 = nmAlignTo(nmOv13, 16), nmOv14 = nmOa13 + sizeof(uint32_t[2]); static uint32_t (&busyUs)[2] = *reinterpret_cast<uint32_t(*)[2]>(NM_ARENA + nmOa13);

static SemaphoreHandle_t semGo, semDone;
static volatile int readyW = -1, doneUpTo = -1;   // コア0が描き終えた帯 / 画面へ送り終えた帯
static constexpr size_t nmOa14 = nmAlignTo(nmOv14, alignof(bool)), nmOv15 = nmOa14 + sizeof(bool); static bool& fbMode = *reinterpret_cast<bool*>(NM_ARENA + nmOa14);  // StopWatch: 画面の手前に PSRAM の置き場がある(帯ごとに丸の中だけ送る)
static int SPLIT = 2;  // コア1は SPLIT 本に1本の帯を描く(残りはコア0)
static constexpr size_t nmOa15 = nmAlignTo(nmOv15, alignof(uint32_t)), nmOv16 = nmOa15 + sizeof(uint32_t); static uint32_t& sendUs = *reinterpret_cast<uint32_t*>(NM_ARENA + nmOa15);

static constexpr size_t nmOa16 = nmAlignTo(nmOv16, alignof(float)), nmOv17 = nmOa16 + sizeof(float); static float& simT = *reinterpret_cast<float*>(NM_ARENA + nmOa16);
static constexpr size_t nmOa17 = nmAlignTo(nmOv17, alignof(bool)), nmOv18 = nmOa17 + sizeof(bool); static bool& imuOK = *reinterpret_cast<bool*>(NM_ARENA + nmOa17);
static constexpr size_t nmOa18 = nmAlignTo(nmOv18, alignof(float)), nmOv19 = nmOa18 + sizeof(float); static float& tiltX = *reinterpret_cast<float*>(NM_ARENA + nmOa18);
static constexpr size_t nmOa19 = nmAlignTo(nmOv19, alignof(float)), nmOv20 = nmOa19 + sizeof(float); static float& tiltY = *reinterpret_cast<float*>(NM_ARENA + nmOa19);  // 傾き(なめらかにしたもの)
static constexpr size_t nmOa20 = nmAlignTo(nmOv20, alignof(float)), nmOv21 = nmOa20 + sizeof(float); static float& levelX = *reinterpret_cast<float*>(NM_ARENA + nmOa20);
static constexpr size_t nmOa21 = nmAlignTo(nmOv21, alignof(float)), nmOv22 = nmOa21 + sizeof(float); static float& levelY = *reinterpret_cast<float*>(NM_ARENA + nmOa21);  // 「水平」の基準(起動したときの姿勢)
static constexpr size_t nmOa22 = nmAlignTo(nmOv22, alignof(int)), nmOv23 = nmOa22 + sizeof(int); static int& levelN = *reinterpret_cast<int*>(NM_ARENA + nmOa22);  // 基準を取った回数
static int brightIdx = 2;
static const uint8_t brightSteps[] = {60, 120, 190, 255};

// ---------------------------------------------------------------- 道具
static inline float clamp01(float v) { return v < 0 ? 0 : (v > 1 ? 1 : v); }
static inline Col lerpC(Col a, Col b, float t) { return {a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t}; }
static inline Col mulC(Col a, float k) { return {a.r * k, a.g * k, a.b * k}; }
static inline Col C8(int r, int g, int b) { return {r / 255.0f, g / 255.0f, b / 255.0f}; }
// 色 → 565(バイト順はふつう)
static inline uint16_t to565(Col c) {
  const int r = (int)(clamp01(c.r) * 255.0f + 0.5f), g = (int)(clamp01(c.g) * 255.0f + 0.5f), b = (int)(clamp01(c.b) * 255.0f + 0.5f);
  return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}
static inline uint16_t rgb565(int r, int g, int b) {
  r = r < 0 ? 0 : r > 255 ? 255 : r; g = g < 0 ? 0 : g > 255 ? 255 : g; b = b < 0 ? 0 : b > 255 ? 255 : b;
  return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}
// 光を足す(565 のまま、色ごとに頭打ち)
static inline uint16_t add565(uint32_t a, uint32_t b) {
  const uint32_t s = (a & 0xF7DE) + (b & 0xF7DE);
  const uint32_t ov = s & 0x10820;
  return (uint16_t)(s | (ov - (ov >> 5)));
}
// 2つの色を a/16 で混ぜる(a=16 で b)
static inline uint16_t mix565(uint32_t a, uint32_t b, int q) {
  const uint32_t A = (a | (a << 16)) & 0x07E0F81Fu, B = (b | (b << 16)) & 0x07E0F81Fu;
  const uint32_t M = ((B * q + A * (16 - q)) >> 4) & 0x07E0F81Fu;
  return (uint16_t)(M | (M >> 16));
}
// 画面へ出す色(バイト順を入れ替える)/ 画面の色(入れ替え済み)に光を足す
static inline uint16_t outPix(uint16_t c) { return __builtin_bswap16(c); }
static inline uint16_t addPix(uint16_t dst, uint16_t add) { return __builtin_bswap16(add565(__builtin_bswap16(dst), add)); }
static inline float frand() { return (esp_random() & 0xFFFF) / 65535.0f; }
static inline float frand(float a, float b) { return a + (b - a) * frand(); }
static inline uint32_t hash3(uint32_t x, uint32_t y, uint32_t s) {
  uint32_t h = x * 374761393u + y * 668265263u + s * 2246822519u;
  h = (h ^ (h >> 13)) * 1274126177u;
  return h ^ (h >> 16);
}
// 端がつながる値ノイズ(n×n の格子。x, y は格子の単位)
static float* makeNoise(int n, uint32_t seed) {
  float* a = (float*)malloc(n * n * sizeof(float));
  if (a) for (int i = 0; i < n * n; ++i) a[i] = (hash3(i % n, i / n, seed) & 0xFFFF) / 65535.0f;
  return a;
}
static inline float noiseAt(const float* a, int n, float x, float y) {
  const float fx0 = floorf(x), fy0 = floorf(y);
  const float fx = x - fx0, fy = y - fy0;
  const float sx = fx * fx * (3 - 2 * fx), sy = fy * fy * (3 - 2 * fy);
  int x0 = ((int)fx0 % n + n) % n, y0 = ((int)fy0 % n + n) % n;
  const int x1 = (x0 + 1) % n, y1 = (y0 + 1) % n;
  const float v00 = a[y0 * n + x0], v10 = a[y0 * n + x1], v01 = a[y1 * n + x0], v11 = a[y1 * n + x1];
  return v00 + (v10 - v00) * sx + (v01 - v00) * sy + (v00 - v10 - v01 + v11) * sx * sy;
}
// 中心からの離れ具合(距離の2乗 × 64 /(半径+1)^2)→ 段(0..7)
static constexpr size_t nmOa23 = nmAlignTo(nmOv23, 16), nmOv24 = nmOa23 + sizeof(uint8_t[64]); static uint8_t (&ringQ)[64] = *reinterpret_cast<uint8_t(*)[64]>(NM_ARENA + nmOa23);

static void fail(const char* msg) {
  M5.Display.setTextDatum(middle_center);
  M5.Display.drawString(msg, M5.Display.width() / 2, M5.Display.height() / 2);
  Serial.println(msg);
  while (true) nmIdle();
}
// 内蔵メモリを優先、足りなければ PSRAM。内蔵メモリは、画面の転送やタスクのために 48KB は残しておく
static void* allocFast(size_t n) {
  if (heap_caps_get_free_size(MALLOC_CAP_INTERNAL) >= n + 48 * 1024) {
    void* p = heap_caps_malloc(n, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (p) return p;
  }
  return ps_malloc(n);
}

// ---------------------------------------------------------------- 人の形
// 姿勢 0: 立つ / 1: 両手を広げる(腕の4本だけ変える)
static const Part PARTS[2][NPART] = {
  {
    {0, {0, 0.78f, 0}, {0.11f, 0.13f, 0.11f}, {}, {}, 0},
    {0, {0, 0.43f, 0}, {0.2f, 0.25f, 0.12f}, {}, {}, 0},
    {0, {0, 0.12f, 0}, {0.17f, 0.14f, 0.11f}, {}, {}, 0},
    {1, {}, {}, {-0.24f, 0.6f, 0}, {-0.34f, 0.2f, 0.04f}, 0.055f},
    {1, {}, {}, {0.24f, 0.6f, 0}, {0.34f, 0.2f, 0.04f}, 0.055f},
    {1, {}, {}, {-0.34f, 0.2f, 0.04f}, {-0.3f, -0.12f, 0.1f}, 0.045f},
    {1, {}, {}, {0.34f, 0.2f, 0.04f}, {0.3f, -0.12f, 0.1f}, 0.045f},
    {1, {}, {}, {-0.09f, 0.0f, 0}, {-0.11f, -0.45f, 0.02f}, 0.075f},
    {1, {}, {}, {0.09f, 0.0f, 0}, {0.11f, -0.45f, 0.02f}, 0.075f},
    {1, {}, {}, {-0.11f, -0.45f, 0.02f}, {-0.12f, -0.88f, 0}, 0.06f},
    {1, {}, {}, {0.11f, -0.45f, 0.02f}, {0.12f, -0.88f, 0}, 0.06f},
  },
  {
    {0, {0, 0.78f, 0}, {0.11f, 0.13f, 0.11f}, {}, {}, 0},
    {0, {0, 0.43f, 0}, {0.2f, 0.25f, 0.12f}, {}, {}, 0},
    {0, {0, 0.12f, 0}, {0.17f, 0.14f, 0.11f}, {}, {}, 0},
    {1, {}, {}, {-0.24f, 0.6f, 0}, {-0.52f, 0.72f, 0.02f}, 0.055f},
    {1, {}, {}, {0.24f, 0.6f, 0}, {0.52f, 0.72f, 0.02f}, 0.055f},
    {1, {}, {}, {-0.52f, 0.72f, 0.02f}, {-0.72f, 0.95f, 0.06f}, 0.045f},
    {1, {}, {}, {0.52f, 0.72f, 0.02f}, {0.72f, 0.95f, 0.06f}, 0.045f},
    {1, {}, {}, {-0.09f, 0.0f, 0}, {-0.11f, -0.45f, 0.02f}, 0.075f},
    {1, {}, {}, {0.09f, 0.0f, 0}, {0.11f, -0.45f, 0.02f}, 0.075f},
    {1, {}, {}, {-0.11f, -0.45f, 0.02f}, {-0.12f, -0.88f, 0}, 0.06f},
    {1, {}, {}, {0.11f, -0.45f, 0.02f}, {0.12f, -0.88f, 0}, 0.06f},
  },
};
static uint8_t* vPart;                 // [NVOX] どの部分の点か
static float*   vU;                    // [NVOX*2] 部分の上での位置(2つの数)
static constexpr size_t nmOa24 = nmAlignTo(nmOv24, alignof(float*)), nmOv25 = nmOa24 + sizeof(float*); static float*& home = *reinterpret_cast<float**>(NM_ARENA + nmOa24);
static constexpr size_t nmOa25 = nmAlignTo(nmOv25, alignof(float*)), nmOv26 = nmOa25 + sizeof(float*); static float*& P = *reinterpret_cast<float**>(NM_ARENA + nmOa25);
static constexpr size_t nmOa26 = nmAlignTo(nmOv26, alignof(float*)), nmOv27 = nmOa26 + sizeof(float*); static float*& V = *reinterpret_cast<float**>(NM_ARENA + nmOa26);
static constexpr size_t nmOa27 = nmAlignTo(nmOv27, alignof(float*)), nmOv28 = nmOa27 + sizeof(float*); static float*& seed = *reinterpret_cast<float**>(NM_ARENA + nmOa27);
static constexpr size_t nmOa28 = nmAlignTo(nmOv28, alignof(float*)), nmOv29 = nmOa28 + sizeof(float*); static float*& depth = *reinterpret_cast<float**>(NM_ARENA + nmOa28);
static constexpr size_t nmOa29 = nmAlignTo(nmOv29, alignof(int16_t*)), nmOv30 = nmOa29 + sizeof(int16_t*); static int16_t*& order = *reinterpret_cast<int16_t**>(NM_ARENA + nmOa29);
static constexpr size_t nmOa30 = nmAlignTo(nmOv30, alignof(int16_t*)), nmOv31 = nmOa30 + sizeof(int16_t*); static int16_t*& sx = *reinterpret_cast<int16_t**>(NM_ARENA + nmOa30);
static constexpr size_t nmOa31 = nmAlignTo(nmOv31, alignof(int16_t*)), nmOv32 = nmOa31 + sizeof(int16_t*); static int16_t*& sy = *reinterpret_cast<int16_t**>(NM_ARENA + nmOa31);
static constexpr size_t nmOa32 = nmAlignTo(nmOv32, alignof(float*)), nmOv33 = nmOa32 + sizeof(float*); static float*& sz = *reinterpret_cast<float**>(NM_ARENA + nmOa32);
static VRect*   rects;
static constexpr size_t nmOa33 = nmAlignTo(nmOv33, alignof(int)), nmOv34 = nmOa33 + sizeof(int); static int& nRect = *reinterpret_cast<int*>(NM_ARENA + nmOa33);
static int16_t* rEnt;
static constexpr size_t nmOa34 = nmAlignTo(nmOv34, 16), nmOv35 = nmOa34 + sizeof(int16_t[MAXNB + 1]); static int16_t (&rStart)[MAXNB + 1] = *reinterpret_cast<int16_t(*)[MAXNB + 1]>(NM_ARENA + nmOa34);
static constexpr size_t nmOa35 = nmAlignTo(nmOv35, 16), nmOv36 = nmOa35 + sizeof(uint16_t[512]); static uint16_t (&bgRow)[512] = *reinterpret_cast<uint16_t(*)[512]>(NM_ARENA + nmOa35);  // 行ごとの背景の色
static constexpr size_t nmOa36 = nmAlignTo(nmOv36, alignof(int)), nmOv37 = nmOa36 + sizeof(int); static int& pose = *reinterpret_cast<int*>(NM_ARENA + nmOa36);
static constexpr size_t nmOa37 = nmAlignTo(nmOv37, alignof(int)), nmOv38 = nmOa37 + sizeof(int); static int& theme = *reinterpret_cast<int*>(NM_ARENA + nmOa37);
static float yaw = 0.3f, yawV = SPIN, scan = -1.1f, scanDir = 1;
static constexpr size_t nmOa38 = nmAlignTo(nmOv38, alignof(float)), nmOv39 = nmOa38 + sizeof(float); static float& lastX = *reinterpret_cast<float*>(NM_ARENA + nmOa38);
static constexpr size_t nmOa39 = nmAlignTo(nmOv39, alignof(float)), nmOv40 = nmOa39 + sizeof(float); static float& floorY = *reinterpret_cast<float*>(NM_ARENA + nmOa39);
static constexpr size_t nmOa40 = nmAlignTo(nmOv40, alignof(float)), nmOv41 = nmOa40 + sizeof(float); static float& scanY = *reinterpret_cast<float*>(NM_ARENA + nmOa40);
static constexpr size_t nmOa41 = nmAlignTo(nmOv41, alignof(bool)), nmOv42 = nmOa41 + sizeof(bool); static bool& dragging = *reinterpret_cast<bool*>(NM_ARENA + nmOa41);
static constexpr size_t nmOa42 = nmAlignTo(nmOv42, alignof(bool)), nmOv43 = nmOa42 + sizeof(bool); static bool& wasTouch = *reinterpret_cast<bool*>(NM_ARENA + nmOa42);
static constexpr size_t nmOa43 = nmAlignTo(nmOv43, alignof(float)), nmOv44 = nmOa43 + sizeof(float); static float& pressT = *reinterpret_cast<float*>(NM_ARENA + nmOa43);
static constexpr size_t nmOa44 = nmAlignTo(nmOv44, alignof(float)), nmOv45 = nmOa44 + sizeof(float); static float& pressX = *reinterpret_cast<float*>(NM_ARENA + nmOa44);
static constexpr size_t nmOa45 = nmAlignTo(nmOv45, alignof(float)), nmOv46 = nmOa45 + sizeof(float); static float& pressY = *reinterpret_cast<float*>(NM_ARENA + nmOa45);
static constexpr size_t nmOa46 = nmAlignTo(nmOv46, alignof(float)), nmOv47 = nmOa46 + sizeof(float); static float& moved = *reinterpret_cast<float*>(NM_ARENA + nmOa46);

static bool allocScratch(Scratch&) { return true; }
static bool pieceAlloc() {
  vPart = (uint8_t*)allocFast(NVOX); vU = (float*)allocFast(NVOX * 2 * 4);
  home = (float*)allocFast(NVOX * 3 * 4); P = (float*)allocFast(NVOX * 3 * 4); V = (float*)allocFast(NVOX * 3 * 4);
  seed = (float*)allocFast(NVOX * 4); depth = (float*)allocFast(NVOX * 4); sz = (float*)allocFast(NVOX * 4);
  order = (int16_t*)allocFast(NVOX * 2); sx = (int16_t*)allocFast(NVOX * 2); sy = (int16_t*)allocFast(NVOX * 2);
  rects = (VRect*)allocFast(sizeof(VRect) * MAXRECT); rEnt = (int16_t*)allocFast(sizeof(int16_t) * MAXRENT);
  return vPart && vU && home && P && V && seed && depth && sz && order && sx && sy && rects && rEnt;
}

static void homeOf(int i, float* o) {
  const Part& p = PARTS[pose][vPart[i]];
  const float u = vU[i * 2], v = vU[i * 2 + 1];
  if (p.t == 0) { const float s = sqrtf(fmaxf(0, 1 - u * u)); o[0] = p.c[0] + p.r3[0] * s * cosf(v); o[1] = p.c[1] + p.r3[1] * u; o[2] = p.c[2] + p.r3[2] * s * sinf(v); }
  else { o[0] = p.a[0] + (p.b[0] - p.a[0]) * u + p.r * cosf(v); o[1] = p.a[1] + (p.b[1] - p.a[1]) * u; o[2] = p.a[2] + (p.b[2] - p.a[2]) * u + p.r * sinf(v); }
}
static void buildHomes() { for (int i = 0; i < NVOX; ++i) homeOf(i, home + i * 3); }

static void pieceInit() {
  // 部分ごとの面積に合わせて点を配る
  float area[NPART], tot = 0;
  for (int k = 0; k < NPART; ++k) {
    const Part& p = PARTS[0][k];
    area[k] = p.t == 0 ? 4 * 3.14159f * (p.r3[0] * p.r3[1] + p.r3[0] * p.r3[2] + p.r3[1] * p.r3[2]) / 3
                       : 2 * 3.14159f * p.r * sqrtf((p.b[0] - p.a[0]) * (p.b[0] - p.a[0]) + (p.b[1] - p.a[1]) * (p.b[1] - p.a[1]) + (p.b[2] - p.a[2]) * (p.b[2] - p.a[2]));
    tot += area[k];
  }
  int i = 0;
  for (int k = 0; k < NPART; ++k) {
    const int n = (k == NPART - 1) ? NVOX - i : (int)(NVOX * area[k] / tot + 0.5f);
    for (int j = 0; j < n && i < NVOX; ++j, ++i) {
      vPart[i] = (uint8_t)k;
      vU[i * 2] = PARTS[0][k].t == 0 ? frand(-1, 1) : frand();
      vU[i * 2 + 1] = frand(0, 6.2832f);
      seed[i] = frand(0, 100);
    }
  }
  buildHomes();
  for (int j = 0; j < NVOX; ++j) {
    P[j * 3] = home[j * 3] + (frand() - 0.5f) * 0.3f; P[j * 3 + 1] = home[j * 3 + 1] - frand() * 0.3f; P[j * 3 + 2] = home[j * 3 + 2] + (frand() - 0.5f) * 0.3f;
    V[j * 3] = V[j * 3 + 1] = V[j * 3 + 2] = 0;
  }
  floorY = S * 0.82f;
  for (int y = 0; y < S && y < 512; ++y) {                  // 背景: 上は暗い青、床のあたりで少し明るく、床は暗い
    const float v = (float)y / S; Col c;
    if (v < 0.68f) c = lerpC(C8(3, 5, 10), C8(10, 18, 32), v / 0.68f);
    else if (v < 0.70f) c = C8(4, 7, 12);
    else c = lerpC(C8(4, 7, 12), C8(1, 2, 3), (v - 0.7f) / 0.3f);
    bgRow[y] = to565(c);
  }
}
static void onA() { pose ^= 1; buildHomes(); Serial.printf("pose: %d\n", pose); }
static void onAHold() { theme = (theme + 1) % 3; Serial.printf("color: %d\n", theme); }
static void statusText(char* o, int n) { snprintf(o, n, "pose %d", pose); }

static void binRects() {
  for (int k = 0; k <= NB; ++k) rStart[k] = 0;
  int total = 0, na = nRect;
  for (int i = 0; i < na; ++i) {
    const int a = max(0, (int)rects[i].y) / BAND, b = min(S - 1, max(0, rects[i].y + rects[i].s - 1)) / BAND;
    if (total + (b - a + 1) > MAXRENT) { na = i; break; }
    for (int k = a; k <= b; ++k) rStart[k + 1]++;
    total += b - a + 1;
  }
  for (int k = 0; k < NB; ++k) rStart[k + 1] += rStart[k];
  int16_t fill[MAXNB];
  for (int k = 0; k < NB; ++k) fill[k] = rStart[k];
  for (int i = 0; i < na; ++i) {                                // 並べた順番のまま(奥から手前へ)
    const int a = max(0, (int)rects[i].y) / BAND, b = min(S - 1, max(0, rects[i].y + rects[i].s - 1)) / BAND;
    for (int k = a; k <= b; ++k) rEnt[fill[k]++] = (int16_t)i;
  }
}

static void updateFrame(const Input& in) {
  float dt = in.dt > 0.05f ? 0.05f : in.dt;
  if (dt < 0.0005f) dt = 0.0005f;
  simT += dt;
  const float t = simT;
  if (in.touching) {
    if (!dragging) { dragging = true; lastX = in.tx; }
    yawV = (in.tx - lastX) * 0.012f * 466.0f / S / dt; lastX = in.tx;
  } else { dragging = false; yawV += (SPIN + in.tiltX * 1.8f - yawV) * (1 - expf(-dt * 1.5f)); }
  yaw += yawV * dt;
  // タップ(短く押して、ほとんど動かさずに離す)で、ほどけて散る。ドラッグでは散らない
  if (in.tapped) { pressT = 0; pressX = in.tx; pressY = in.ty; moved = 0; }
  if (in.touching) { pressT += dt; moved = fmaxf(moved, fabsf(in.tx - pressX) + fabsf(in.ty - pressY)); }
  const bool tapUp = wasTouch && !in.touching && pressT < 0.3f && moved < 12.0f * S / 466.0f;
  wasTouch = in.touching;
  if (tapUp)
    for (int i = 0; i < NVOX; ++i) { V[i * 3] += (frand() - 0.5f) * 3 + home[i * 3] * 2; V[i * 3 + 1] += (frand() - 0.2f) * 2.5f; V[i * 3 + 2] += (frand() - 0.5f) * 3 + home[i * 3 + 2] * 2; }
  scan += scanDir * dt * SCAN_SPD;
  if (scan > 1.15f) scanDir = -1;
  if (scan < -1.15f) scanDir = 1;
  const float breath = 0.012f * sinf(t * 1.4f);
  for (int i = 0; i < NVOX; ++i) {
    const float hy = home[i * 3 + 1];
    const bool built = scanDir > 0 ? hy < scan : hy > scan;            // 上がるときは下から組み、下がるときは上から崩す
    const float k = built ? 7.0f : 1.6f, damp = expf(-dt * (built ? 4.0f : 1.5f));
    for (int c = 0; c < 3; ++c) {
      const int j = i * 3 + c;
      float target = home[j] + (c == 0 ? home[j] * breath * 3 : 0);
      if (!built) target += (c == 1 ? -0.07f - 0.05f * sinf(seed[i]) : 0) + 0.06f * sinf(seed[i] * 1.7f + c * 2 + t * 0.9f);
      V[j] = (V[j] + (target - P[j]) * k * dt) * damp;
      P[j] += V[j] * dt;
    }
  }
  // 回して遠近をつけ、奥から順に並べる
  const float sc = VR * 0.62f, cy = cosf(yaw), syw = sinf(yaw), D = 4.0f;
  for (int i = 0; i < NVOX; ++i) {
    const float x = P[i * 3], y = P[i * 3 + 1], z = P[i * 3 + 2];
    const float x1 = x * cy + z * syw, z1 = -x * syw + z * cy, k = D / (D - z1);
    sx[i] = (int16_t)(Sc + x1 * sc * k); sy[i] = (int16_t)(floorY - (y + 0.9f) * sc * k); sz[i] = k; depth[i] = z1; order[i] = (int16_t)i;
  }
  std::sort(order, order + NVOX, [](int16_t a, int16_t b) { return depth[a] < depth[b]; });
  static const Col body[3][3] = {   // [色][奥 / 手前 / 走査線]
    { C8(40, 90, 150), C8(150, 200, 255), C8(180, 130, 40) },
    { C8(120, 80, 30), C8(255, 210, 130), C8(120, 120, 120) },
    { C8(80, 85, 95), C8(235, 240, 250), C8(120, 140, 160) },
  };
  nRect = 0;
  const float ss = S / 466.0f;
  // 床の鏡像(うすく、下へ、上下につぶして)
  for (int n = 0; n < NVOX && nRect < MAXRECT; ++n) {
    const int i = order[n];
    const int yy = (int)(floorY + (floorY - sy[i]) * 0.9f);
    if (yy >= S || yy < 0) continue;
    const int s = max(1, (int)(3.2f * sz[i] * ss));
    rects[nRect++] = {(int16_t)(sx[i] - s / 2), (int16_t)yy, (uint8_t)s, (uint8_t)max(1, (int)(MIRROR * 16)), to565(body[theme][1])};
  }
  // 分身(手前ほど明るく。走査線の近くは光る。崩れた所は暗く)
  for (int n = 0; n < NVOX && nRect < MAXRECT; ++n) {
    const int i = order[n];
    const float hy = home[i * 3 + 1], near = fmaxf(0, 1 - fabsf(hy - scan) * 9), d = (depth[i] + 1) * 0.5f;
    const bool built = scanDir > 0 ? hy < scan : hy > scan;
    const float kk = built ? 1.0f : 0.45f;
    Col c = mulC(lerpC(body[theme][0], body[theme][1], clamp01(d)), kk);
    c = {c.r + body[theme][2].r * near, c.g + body[theme][2].g * near, c.b + body[theme][2].b * near};
    const int s = max(1, (int)(3.4f * sz[i] * (built ? 1.0f : 0.8f) * ss));
    rects[nRect++] = {(int16_t)(sx[i] - s / 2), (int16_t)(sy[i] - s / 2), (uint8_t)s, 0, to565(c)};
  }
  binRects();
  scanY = floorY - (scan + 0.9f) * sc;
}

static void renderBand(int k, uint16_t* buf, Scratch&) {
  const int y0 = k * BAND, y1 = min(S, y0 + BAND);
  const int bx0 = bandX0[k], bw = bandW[k], bx1 = bx0 + bw - 1;
  const uint16_t floorLine = rgb565(40, 60, 85);
  for (int y = y0; y < y1; ++y) {
    uint16_t* row = buf + (y - y0) * bw - bx0;
    const int xs = max((int)spanS[y], bx0), xe = min((int)spanE[y], bx1);
    for (int x = bx0; x <= bx1; ++x) row[x] = 0;
    if (xs > xe) continue;
    uint16_t c = (y == (int)floorY + 2) ? floorLine : bgRow[y < 512 ? y : 511];
    const float dl = fabsf(y - scanY);                         // 走査線の光
    if (dl < 10) { const int q = (int)((1 - dl / 10) * 9); c = add565(c, rgb565(q * 9, q * 12, q * 14)); }
    const uint16_t o = outPix(c);
    for (int x = xs; x <= xe; ++x) row[x] = o;
  }
  for (int e = rStart[k]; e < rStart[k + 1]; ++e) {
    const VRect& r = rects[rEnt[e]];
    for (int y = max(y0, (int)r.y); y < min(y1, r.y + r.s); ++y) {
      const int xs = max(max((int)spanS[y], bx0), (int)r.x), xe = min(min((int)spanE[y], bx1), r.x + r.s - 1);
      uint16_t* row = buf + (y - y0) * bw - bx0;
      if (r.blend == 0) { const uint16_t o = outPix(r.col); for (int x = xs; x <= xe; ++x) row[x] = o; }
      else for (int x = xs; x <= xe; ++x) row[x] = outPix(mix565(__builtin_bswap16(row[x]), r.col, r.blend));
    }
  }
  // 走査線の、いちばん明るいまん中の線(分身の上にも)
  const int ly = (int)scanY;
  if (ly >= y0 && ly < y1) {
    uint16_t* row = buf + (ly - y0) * bw - bx0;
    const int xs = max((int)spanS[ly], bx0), xe = min((int)spanE[ly], bx1);
    for (int x = xs; x <= xe; ++x) row[x] = addPix(row[x], rgb565(60, 90, 110));
  }
}

static constexpr size_t nmOa47 = nmAlignTo(nmOv47, 16), nmOv48 = nmOa47 + sizeof(Scratch[2]); static Scratch (&scr)[2] = *reinterpret_cast<Scratch(*)[2]>(NM_ARENA + nmOa47);

static void __attribute__((noinline)) MOD_IRAM renderBandIram(int k, uint16_t* b, Scratch& s) { renderBand(k, b, s); }

// コア0: コア1が描かない帯(ふつうは奇数番目。StopWatch では3本に2本)
static void workerTask(void*) {
  for (;;) {
    xSemaphoreTake(semGo, portMAX_DELAY);
    uint32_t busy = 0;
    for (int k = 0; k < NB; ++k) {
      if (k % SPLIT == 0) continue;
      while (__atomic_load_n(&doneUpTo, __ATOMIC_ACQUIRE) < k - NSLOT) { }
      if (bandVis[k]) {
        uint32_t t0 = micros();
        renderBandIram(k, slotBuf[k % NSLOT], scr[0]);
        busy += micros() - t0;
      }
      __atomic_store_n(&readyW, k, __ATOMIC_RELEASE);
    }
    busyUs[0] = busy;
    xSemaphoreGive(semDone);
  }
}

// コア1: SPLIT 本に1本の帯を描き、すべての帯を順番に画面へ送る
static void renderFrame() {
  __atomic_store_n(&readyW, -1, __ATOMIC_RELEASE);
  __atomic_store_n(&doneUpTo, -1, __ATOMIC_RELEASE);
  xSemaphoreGive(semGo);
  uint32_t busy = 0, send = 0;
  if (!fbMode) M5.Display.startWrite();
  for (int k = 0; k < NB; ++k) {
    if (k % SPLIT == 0) {
      if (bandVis[k]) {
        uint32_t t0 = micros();
        renderBandIram(k, slotBuf[k % NSLOT], scr[1]);
        busy += micros() - t0;
      }
    } else {
      while (__atomic_load_n(&readyW, __ATOMIC_ACQUIRE) < k) { }
    }
    if (!fbMode) M5.Display.waitDMA();
    __atomic_store_n(&doneUpTo, k - 1, __ATOMIC_RELEASE);
    if (bandVis[k]) {
      const int y0 = k * BAND, h = min(BAND, S - y0);
      if (fbMode) {                                   // 置き場に書いて、この帯の丸の中の幅だけをすぐ画面へ送る
        const uint32_t t0 = micros();
        M5.Display.pushImage(XOFF + bandX0[k], YOFF + y0, bandW[k], h, slotBuf[k % NSLOT]);
        send += micros() - t0;
      } else {
        M5.Display.pushImageDMA(XOFF + bandX0[k], YOFF + y0, bandW[k], h, slotBuf[k % NSLOT]);
      }
    }
  }
  if (!fbMode) M5.Display.waitDMA();
  __atomic_store_n(&doneUpTo, NB, __ATOMIC_RELEASE);
  if (!fbMode) M5.Display.endWrite();
  xSemaphoreTake(semDone, portMAX_DELAY);
  busyUs[1] = busy;
  sendUs = send;
}

// ---------------------------------------------------------------- setup / loop
__attribute__((cold)) void setup() {   // cold: 1回だけ動くので、小さく作る
  auto cfg = M5.config();
  M5.begin(cfg);
  Serial.begin(115200);

  if (M5.Display.width() < M5.Display.height()) M5.Display.setRotation(1);
  W = M5.Display.width(); H = M5.Display.height();
  isRound = (W == H);
  S = (isRound ? W : max(W, H)) & ~1;
  Sc = (S - 1) * 0.5f;
  XOFF = (W - S) / 2;
  YOFF = ((H - S) / 2 / BAND) * BAND;
  NB = (S + BAND - 1) / BAND;
  VR = 0.5f * min(W, H);
  vx0 = (float)max(0, -XOFF); vx1 = (float)min(S, W - XOFF) - 1;
  vy0 = (float)max(0, -YOFF); vy1 = (float)min(S, H - YOFF) - 1;
  Serial.println(SKETCH_NAME);
  Serial.printf("screen %dx%d  area %d\n", W, H, S);

  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setBrightness(brightSteps[brightIdx]);

  for (int i = 0; i < NSLOT; ++i) slotBuf[i] = (uint16_t*)heap_caps_malloc(S * BAND * 2, MALLOC_CAP_DMA);
  spanS  = (int16_t*)malloc(S * sizeof(int16_t)); spanE = (int16_t*)malloc(S * sizeof(int16_t));
  bandX0 = (int16_t*)malloc(NB * sizeof(int16_t)); bandW = (int16_t*)malloc(NB * sizeof(int16_t));
  bandVis = (uint8_t*)malloc(NB);
  bool ok = spanS && spanE && bandX0 && bandW && bandVis && NB <= MAXNB;
  for (int i = 0; i < NSLOT; ++i) ok = ok && slotBuf[i];
  for (int c = 0; c < 2; ++c) ok = ok && allocScratch(scr[c]);
  ok = ok && pieceAlloc();
  if (!ok) fail("Memory error");

  // 丸い画面: 行ごとに、丸の中の左右の端。帯ごとに、送る幅(偶数にそろえる)
  const float rr = S * 0.5f + 1.0f;
  for (int y = 0; y < S; ++y) {
    if (!isRound) { spanS[y] = 0; spanE[y] = S - 1; continue; }
    const float dy = y - Sc, w = rr * rr - dy * dy;
    if (w <= 0) { spanS[y] = 1; spanE[y] = 0; continue; }
    const float s = sqrtf(w);
    spanS[y] = max(0, (int)floorf(Sc - s)); spanE[y] = min(S - 1, (int)ceilf(Sc + s));
  }
  for (int k = 0; k < NB; ++k) {
    int a = S, b = -1;
    for (int y = k * BAND; y < min(S, k * BAND + BAND); ++y)
      if (spanS[y] <= spanE[y]) { a = min(a, (int)spanS[y]); b = max(b, (int)spanE[y]); }
    if (b < a) { a = 0; b = 1; }
    a &= ~1; if (((b - a + 1) & 1) && b < S - 1) ++b;
    bandX0[k] = a; bandW[k] = b - a + 1;
    const int sy = YOFF + k * BAND, sh = min(BAND, S - k * BAND);
    bandVis[k] = (sy + sh > 0 && sy < H) ? 1 : 0;
  }
  for (int i = 0; i < 64; ++i) { int q = (int)(sqrtf(i / 64.0f) * 8.0f); ringQ[i] = (uint8_t)(q > 7 ? 7 : q); }

  const uint32_t t0 = millis();
  pieceInit();
  Serial.printf("prepared in %u ms\n", (unsigned)(millis() - t0));

  imuOK = TILT && M5.Imu.isEnabled();
  Serial.printf("tilt sensor: %s\n", imuOK ? "ok" : "not used");
  Input in0 = {false, false, -1000, -1000, 0.016f, 0, 0};
  updateFrame(in0);

  // StopWatch の M5GFX は、画面の手前に PSRAM の置き場を持ち、描き終わるたびに自動で画面へ送る
  { auto* pnl = M5.Display.getPanel(); fbMode = pnl && pnl->getAutoDisplay(); }
  SPLIT = fbMode ? 3 : 2;
  Serial.printf("display: %s\n", fbMode ? "framebuffer (send each band)" : "direct DMA");
  semGo   = xSemaphoreCreateBinary();
  semDone = xSemaphoreCreateBinary();
  xTaskCreatePinnedToCore(workerTask, "band", 8192, nullptr, 1, nullptr, 0);
}

void loop() {
  static uint32_t last = millis();
  static uint32_t fpsTimer = millis();
  static uint32_t lastTapMs = 0;
  static int frames = 0;

  M5.update();
  auto t = M5.Touch.getDetail();
  uint32_t now = millis();
  bool pressed = t.wasPressed();

  if (isRound) {
    if (M5.BtnA.wasHold()) onAHold();
    else if (M5.BtnA.wasClicked()) onA();
    if (M5.BtnB.wasPressed()) {
      brightIdx = (brightIdx + 1) % sizeof(brightSteps);
      M5.Display.setBrightness(brightSteps[brightIdx]);
    }
  } else {
    if (M5.BtnPWR.wasClicked()) {
      brightIdx = (brightIdx + 1) % sizeof(brightSteps);
      M5.Display.setBrightness(brightSteps[brightIdx]);
    }
    if (DOUBLE_TAP_UI && pressed) {
      if (now - lastTapMs < 350) {                             // ダブルタップ: 上半分 = ボタンA、下半分 = ボタンA長押し
        if (t.y < H / 2) onA(); else onAHold();
        lastTapMs = 0;
      } else lastTapMs = now;
    }
  }

  // 傾き: 本体を傾けたとき、画面の上で「下」になる向き。向きが逆なら、調整用の TILT_FLIP_X / Y / SWAP で直す
  if (imuOK) {
    float ax, ay, az;
    M5.Imu.update();
    if (M5.Imu.getAccel(&ax, &ay, &az)) {
      float sx = -ax, sy = ay;
#if defined(ESP_PLATFORM)
      if (TILT_STOPWATCH_FIX && M5.getBoard() == m5::board_t::board_M5StopWatch) { sx = -ay; sy = -ax; }   // 90 度回して読む
#endif
      if (TILT_SWAP) { const float tmp = sx; sx = sy; sy = tmp; }
      if (TILT_FLIP_X) sx = -sx;
      if (TILT_FLIP_Y) sy = -sy;
      const float dts = (float)(now - last) / 1000.0f;           // (符号なしの引き算に、マイナスをつけないように)
      // 水平の基準: 起動してすぐ(はじめの 20 回)の姿勢を水平とみなす。
      // そのあとも同じ角度で持ち続けると、TILT_RELEVEL 秒くらいかけて、ゆっくりそこを水平にする
      if (levelN < 20) { ++levelN; levelX += (sx - levelX) / levelN; levelY += (sy - levelY) / levelN; }
      else if (TILT_RELEVEL > 0 && fabsf(sx - levelX) < TILT_RELEVEL_MAX && fabsf(sy - levelY) < TILT_RELEVEL_MAX) {   // 少しの傾きだけ「水平」に合わせなおす
        const float b = 1.0f - expf(-dts / TILT_RELEVEL); levelX += (sx - levelX) * b; levelY += (sy - levelY) * b; }
      sx -= levelX; sy -= levelY;
      sx *= TILT_GAIN; sy *= TILT_GAIN;
      const float m = sqrtf(sx * sx + sy * sy);                // 少しの傾きは無視(机に置いたときや、手のふるえで動かないように)
      if (m < TILT_DEAD) { sx = sy = 0; } else { const float k = (m - TILT_DEAD) / m; sx *= k; sy *= k; }
      sx = sx < -1 ? -1 : sx > 1 ? 1 : sx; sy = sy < -1 ? -1 : sy > 1 ? 1 : sy;
      const float a = 1.0f - expf(-dts * 8.0f);
      tiltX += (sx - tiltX) * a; tiltY += (sy - tiltY) * a;
    }
  }

  Input in;
  in.tiltX = tiltX; in.tiltY = tiltY;
  in.touching = t.isPressed();
  in.tapped   = pressed;
  in.tx = (float)(t.x - XOFF);
  in.ty = (float)(t.y - YOFF);
  in.dt = (now - last) / 1000.0f;
  last = now;

  updateFrame(in);
  renderFrame();

  if (++frames, millis() - fpsTimer >= 3000) {
    float fps = frames * 1000.0f / (millis() - fpsTimer);
    char st[48]; statusText(st, sizeof(st));
    Serial.printf("fps: %.1f  core0 %.1f ms  core1 %.1f ms  send %.1f ms  %s\n", fps, busyUs[0] / 1000.0f, busyUs[1] / 1000.0f, sendUs / 1000.0f, st);
    if (imuOK) Serial.printf("tilt: x %+.2f  y %+.2f  (水平の基準 x %+.2f  y %+.2f)  右を下げると x が +、手前(画面の下)を下げると y が + なら正しい向き\n", tiltX, tiltY, levelX, levelY);
    frames = 0; fpsTimer = millis();
  }
}


// 共通の置き場に置いたものは、0 で始まる単純な型だけ(コンストラクタが要らないもの)
static_assert(std::is_trivially_default_constructible<Scratch>::value, "NM_ARENA: Scratch");
static_assert(std::is_trivially_default_constructible<bool>::value, "NM_ARENA: bool");
static_assert(std::is_trivially_default_constructible<float>::value, "NM_ARENA: float");
static_assert(std::is_trivially_default_constructible<float*>::value, "NM_ARENA: float*");
static_assert(std::is_trivially_default_constructible<int>::value, "NM_ARENA: int");
static_assert(std::is_trivially_default_constructible<int16_t>::value, "NM_ARENA: int16_t");
static_assert(std::is_trivially_default_constructible<int16_t*>::value, "NM_ARENA: int16_t*");
static_assert(std::is_trivially_default_constructible<uint16_t>::value, "NM_ARENA: uint16_t");
static_assert(std::is_trivially_default_constructible<uint32_t>::value, "NM_ARENA: uint32_t");
static_assert(std::is_trivially_default_constructible<uint8_t>::value, "NM_ARENA: uint8_t");
static constexpr size_t OVL_END = nmAlignTo(nmOv48, 16);
static_assert(OVL_END <= NM_ARENA_SIZE, "NM_ARENA_SIZE (nm_common.h) を大きくしてください");
extern const size_t OVL_SIZE = OVL_END;   // 起動時の確認用
}  // namespace NullTwin
