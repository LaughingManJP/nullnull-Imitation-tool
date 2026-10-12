// NullPond  -  NullMenu 用モジュール(元のスケッチ NullPond_v6 を名前空間に入れたもの)
// 0 で始まる変数(配列も)は、ほかの作品と共通の置き場(NM_ARENA)に置いています。
// 一度に動くのは1つだけ(切り替えるときは再起動)なので、重なっていても問題ありません。
#include "nm_common.h"
#include <algorithm>
#pragma GCC optimize("O3")

namespace NullPond {
static constexpr size_t nmOv0 = 0;

// NullPond v6  -  計算機自然の池。空を映す水面が、ところどころピクセルに「デジタル化」して、また自然へ溶けて戻る
//   メディアアートの作品(計算機と自然の境目がなくなっていく、という考え方)から受けたイメージで作った、非公式の習作です。
//   作品の映像・名前・データは使っていません。
//   M5Stack StopWatch 用(丸い画面)。CoreS3 系でも画面いっぱいに動きます(自動で切り替わります)。
//
// 画面の中:
//   ・夕焼け(または夜・夜明け)の空を映す水面。ときどき雨粒が落ちて、波紋が広がる
//   ・水面はところどころ「デジタル化」して、色が段になったピクセルのモザイクになり、またなめらかな景色へ溶けて戻る。
//     その境目が、ゆっくり池の上を移ろう
//
// 傾けると(本体の傾きセンサー):
//   ・水面に映る空が、傾けた方へずれる。低い方のふちから、次々に波が立つ
//   ・傾きの向きが逆のときは、調整用の TILT_FLIP_X / TILT_FLIP_Y / TILT_SWAP を変える。Serial に tilt: の値が出る
//
// 触ると:
//   ・タップ: 波紋が広がり、指の下がピクセルに結晶して、しばらくすると溶けて戻る
//   ・なぞる: 細い波紋の列ができる
//
// 操作:
//   StopWatch … ボタンA 短押し: 空(夕焼け / 夜 / 夜明け)   ボタンA 長押し: デジタル化の多さ(ふつう / 多い / なし)   ボタンB: 明るさ
//   CoreS3系  … 画面の上半分をダブルタップ: 空   下半分をダブルタップ: デジタル化の多さ   電源ボタン短押し: 明るさ
//
// 必要なもの:
//   ボードマネージャー M5Stack >= 3.3.7 / ボード: M5StopWatch(PSRAM は OPI)、CoreS3 なら M5CoreS3
//   ライブラリ M5Unified >= 0.2.16, M5GFX >= 0.2.21
//
// しくみ:
//   水面の高さは 3 ピクセルおきの格子で、波の式(となりの平均 - ひとつ前)で動かします(整数で)。
//   空の絵は起動時に半分の細かさで作って内蔵メモリに置き、水面の傾きの分だけずらして読みます。
//   デジタル化の段階は 8 ピクセルおきの格子で、ゆっくり動くノイズと、触った所の結晶で決めます。
//   画面は8行ずつの帯に分けて、2つのコアで描きます。StopWatch では、帯ごとに丸の中だけを画面へ送ります。



// ---- 調整用 ----------------------------------------------------------------
static constexpr float RAIN      = 1.0f;    // 雨粒の多さ(0 でなし)
static constexpr float RIPPLE    = 1.0f;    // 波で景色がゆがむ強さ
static constexpr float SLOSH     = 1.0f;    // 傾けたとき、低い方のふちから立つ波の多さ
static constexpr float CRYSTAL   = 15.0f;   // 触った所が溶けて戻るまでの時間の目安(秒)
static constexpr bool  DOUBLE_TAP_UI = true;
// ---------------------------------------------------------------------------
static constexpr int HSTEP = 3;              // 水面の格子の間隔(ピクセル)
static constexpr int QSTEP = 8;              // デジタル化の格子の間隔(ピクセル)

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
struct Scratch { int16_t* ox; int16_t* oy; uint16_t* sp; };   // 1行ぶん、水面の格子の列ごとの「ずらす量」と「きらめき」

static const char* SKETCH_NAME = "NullPond v6";

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

// ---------------------------------------------------------------- 水面と空
static constexpr size_t nmOa24 = nmAlignTo(nmOv24, alignof(int)), nmOv25 = nmOa24 + sizeof(int); static int& HW = *reinterpret_cast<int*>(NM_ARENA + nmOa24);  // 水面の格子(1辺)
static int16_t* h0;                    // [HW*HW] ひとつ前
static int16_t* h1;                    // [HW*HW] 今
static constexpr size_t nmOa25 = nmAlignTo(nmOv25, alignof(int)), nmOv26 = nmOa25 + sizeof(int); static int& SW2 = *reinterpret_cast<int*>(NM_ARENA + nmOa25);  // 空の絵(半分の細かさ)の1辺
static uint16_t* sky;                  // [SW2*SW2] 565
static constexpr size_t nmOa26 = nmAlignTo(nmOv26, alignof(int)), nmOv27 = nmOa26 + sizeof(int); static int& QG = *reinterpret_cast<int*>(NM_ARENA + nmOa26);
static uint8_t* level;                 // [QG*QG] 0: 自然 / 1..3: 4・8・16 ピクセルのモザイク
static float*   boost;                 // [QG*QG] 触った所の結晶
static constexpr size_t nmOa27 = nmAlignTo(nmOv27, alignof(float*)), nmOv28 = nmOa27 + sizeof(float*); static float*& nzA = *reinterpret_cast<float**>(NM_ARENA + nmOa27);
static constexpr size_t nmOa28 = nmAlignTo(nmOv28, alignof(float*)), nmOv29 = nmOa28 + sizeof(float*); static float*& nzB = *reinterpret_cast<float**>(NM_ARENA + nmOa28);
static constexpr size_t nmOa29 = nmAlignTo(nmOv29, alignof(float*)), nmOv30 = nmOa29 + sizeof(float*); static float*& nzC = *reinterpret_cast<float**>(NM_ARENA + nmOa29);
static constexpr size_t nmOa30 = nmAlignTo(nmOv30, alignof(float*)), nmOv31 = nmOa30 + sizeof(float*); static float*& nzD = *reinterpret_cast<float**>(NM_ARENA + nmOa30);  // ノイズ
static constexpr size_t nmOa31 = nmAlignTo(nmOv31, alignof(int)), nmOv32 = nmOa31 + sizeof(int); static int& skyTheme = *reinterpret_cast<int*>(NM_ARENA + nmOa31);
static constexpr size_t nmOa32 = nmAlignTo(nmOv32, alignof(int)), nmOv33 = nmOa32 + sizeof(int); static int& digi = *reinterpret_cast<int*>(NM_ARENA + nmOa32);
static float lastX = -1, lastY = -1;
static constexpr size_t nmOa33 = nmAlignTo(nmOv33, alignof(float)), nmOv34 = nmOa33 + sizeof(float); static float& rainT = *reinterpret_cast<float*>(NM_ARENA + nmOa33);
static constexpr size_t nmOa34 = nmAlignTo(nmOv34, alignof(float)), nmOv35 = nmOa34 + sizeof(float); static float& sloshT = *reinterpret_cast<float*>(NM_ARENA + nmOa34);
static constexpr size_t nmOa35 = nmAlignTo(nmOv35, alignof(int)), nmOv36 = nmOa35 + sizeof(int); static int& tiltOX = *reinterpret_cast<int*>(NM_ARENA + nmOa35);
static constexpr size_t nmOa36 = nmAlignTo(nmOv36, alignof(int)), nmOv37 = nmOa36 + sizeof(int); static int& tiltOY = *reinterpret_cast<int*>(NM_ARENA + nmOa36);  // 傾きで、映る空をずらす量(ピクセル)
static constexpr size_t nmOa37 = nmAlignTo(nmOv37, 16), nmOv38 = nmOa37 + sizeof(uint8_t[4][32]); static uint8_t (&post5)[4][32] = *reinterpret_cast<uint8_t(*)[4][32]>(NM_ARENA + nmOa37);
static constexpr size_t nmOa38 = nmAlignTo(nmOv38, 16), nmOv39 = nmOa38 + sizeof(uint8_t[4][64]); static uint8_t (&post6)[4][64] = *reinterpret_cast<uint8_t(*)[4][64]>(NM_ARENA + nmOa38);  // デジタル化の色の段(5 ビット / 6 ビット)
static const int BS[4] = {1, 4, 8, 16};
static int16_t* xdiv3;                 // [S] x / 3(割り算をしないように)

static bool allocScratch(Scratch& s) {
  const int n = S / HSTEP + 4;
  s.ox = (int16_t*)malloc(n * 2); s.oy = (int16_t*)malloc(n * 2); s.sp = (uint16_t*)malloc(n * 2);
  return s.ox && s.oy && s.sp;
}

static bool pieceAlloc() {
  HW = S / HSTEP + 3;
  SW2 = S / 2 + 2;
  QG = S / QSTEP + 2;
  h0 = (int16_t*)allocFast((size_t)HW * HW * 2);
  h1 = (int16_t*)allocFast((size_t)HW * HW * 2);
  sky = (uint16_t*)allocFast((size_t)SW2 * SW2 * 2);
  level = (uint8_t*)malloc(QG * QG);
  xdiv3 = (int16_t*)malloc(S * sizeof(int16_t));
  boost = (float*)malloc(QG * QG * sizeof(float));
  nzA = makeNoise(64, 7); nzB = makeNoise(64, 11); nzC = makeNoise(32, 23); nzD = makeNoise(32, 31);
  return h0 && h1 && sky && level && boost && xdiv3 && nzA && nzB && nzC && nzD;
}

// 空の絵(全体の座標で考えて、半分の細かさで持つ)
static void buildSky() {
  const float k = S / 466.0f;
  for (int j = 0; j < SW2; ++j)
    for (int i = 0; i < SW2; ++i) {
      const float x = i * 2.0f, y = j * 2.0f, v = y / S;
      float r, g, b;
      const float n = noiseAt(nzA, 64, x / (30 * k), y / (7 * k)) * 0.65f + noiseAt(nzB, 64, x / (13 * k), y / (3.5f * k)) * 0.35f;
      if (skyTheme == 0) {                        // 夕焼け
        if (v < 0.45f) { const float t = v / 0.45f; r = 18 + 120 * t; g = 22 + 30 * t; b = 70 + 50 * t; }
        else if (v < 0.62f) { const float t = (v - 0.45f) / 0.17f; r = 138 + 110 * t; g = 52 + 70 * t; b = 120 - 70 * t; }
        else { const float t = fminf(1.0f, (v - 0.62f) / 0.38f); r = 248 - 60 * t; g = 122 - 50 * t; b = 50 + 10 * t; }
        const float cl = fmaxf(0, n - 0.52f) * 2.4f * (0.3f + 0.7f * fminf(1.0f, v * 1.8f));
        r = r * (1 - cl * 0.5f) + 255 * cl * 0.55f; g = g * (1 - cl * 0.5f) + 150 * cl * 0.5f; b = b * (1 - cl * 0.5f) + 140 * cl * 0.45f;
        const float d = sqrtf((x - Sc) * (x - Sc) + (y - S * 0.6f) * (y - S * 0.6f)) / k;
        const float sun = fminf(1.0f, fmaxf(0, 1 - d / 34) * 3), glow = expf(-d / 70);
        r += 255 * sun + 90 * glow; g += 220 * sun + 60 * glow; b += 150 * sun + 20 * glow;
      } else if (skyTheme == 1) {                 // 夜(月と星)
        r = 6 + 14 * v; g = 10 + 22 * v; b = 28 + 40 * v;
        const float cl = fmaxf(0, n - 0.55f) * 2.0f;
        r += 30 * cl; g += 36 * cl; b += 48 * cl;
        const float d = sqrtf((x - S * 0.62f) * (x - S * 0.62f) + (y - S * 0.38f) * (y - S * 0.38f)) / k;
        const float moon = fminf(1.0f, fmaxf(0, 1 - d / 22) * 4), glow = expf(-d / 55);
        r += 225 * moon + 40 * glow; g += 225 * moon + 46 * glow; b += 210 * moon + 60 * glow;
        if ((hash3(i, j, 99) & 1023) < 3) { const float s = 120 + (hash3(i, j, 7) & 127); r += s; g += s; b += s; }
      } else {                                    // 夜明け
        r = 120 + 110 * v; g = 140 + 80 * v; b = 190 - 30 * v;
        const float cl = fmaxf(0, n - 0.5f) * 2.0f;
        r += 60 * cl; g += 50 * cl; b += 40 * cl;
        const float d = sqrtf((x - Sc * 0.7f) * (x - Sc * 0.7f) + (y - S * 0.75f) * (y - S * 0.75f)) / k;
        const float glow = expf(-d / 90);
        r += 110 * glow; g += 80 * glow; b += 40 * glow;
      }
      // 水の深み(わずかに暗く青く)を、先にかけておく
      sky[j * SW2 + i] = rgb565((int)(r * 0.82f), (int)(g * 0.84f), (int)(b * 0.9f + 16));
    }
}

static void pieceInit() {
  memset(h0, 0, (size_t)HW * HW * 2); memset(h1, 0, (size_t)HW * HW * 2);
  for (int i = 0; i < S; ++i) xdiv3[i] = (int16_t)(i / HSTEP);
  for (int i = 0; i < QG * QG; ++i) { boost[i] = 0; level[i] = 0; }
  for (int l = 0; l < 4; ++l) {
    const int lv = l == 0 ? 32 : l == 1 ? 10 : l == 2 ? 6 : 4;
    for (int v = 0; v < 32; ++v) post5[l][v] = (uint8_t)(l == 0 ? v : (int)(roundf(v * (lv - 1) / 31.0f) * 31.0f / (lv - 1)));
    for (int v = 0; v < 64; ++v) post6[l][v] = (uint8_t)(l == 0 ? v : (int)(roundf(v * (lv - 1) / 63.0f) * 63.0f / (lv - 1)));
  }
  buildSky();
}

static void onA() { skyTheme = (skyTheme + 1) % 3; buildSky(); Serial.printf("sky: %d\n", skyTheme); }
static void onAHold() { digi = (digi + 1) % 3; Serial.printf("digital: %d\n", digi); }
static void statusText(char* o, int n) { snprintf(o, n, "sky %d digital %d", skyTheme, digi); }

// 水面に、ふくらみを落とす(x, y: 画面のピクセル / r: 半径 / a: 強さ)
static void drop(float x, float y, float r, float a) {
  const float cx = x / HSTEP, cy = y / HSTEP, rr = r / HSTEP;
  const int j0 = max(1, (int)floorf(cy - rr * 2)), j1 = min(HW - 2, (int)ceilf(cy + rr * 2));
  const int i0 = max(1, (int)floorf(cx - rr * 2)), i1 = min(HW - 2, (int)ceilf(cx + rr * 2));
  for (int j = j0; j <= j1; ++j)
    for (int i = i0; i <= i1; ++i) {
      const float d2 = ((i - cx) * (i - cx) + (j - cy) * (j - cy)) / (rr * rr);
      int v = h1[j * HW + i] + (int)(a * 64.0f * expf(-d2));
      h1[j * HW + i] = (int16_t)(v > 30000 ? 30000 : v < -30000 ? -30000 : v);
    }
}

static void updateFrame(const Input& in) {
  float dt = in.dt > 0.05f ? 0.05f : in.dt;
  if (dt < 0.0005f) dt = 0.0005f;
  simT += dt;
  const float k = S / 466.0f;

  // 波(2回ぶん進める): 新しい高さ = となり4つの平均×2 - ひとつ前、少しずつ弱まる
  for (int it = 0; it < 2; ++it) {
    for (int j = 1; j < HW - 1; ++j) {
      int16_t* a = h0 + j * HW; const int16_t* b = h1 + j * HW;
      for (int i = 1; i < HW - 1; ++i) {
        int v = ((b[i - 1] + b[i + 1] + b[i - HW] + b[i + HW]) >> 1) - a[i];
        v -= v >> 6;
        a[i] = (int16_t)(v > 30000 ? 30000 : v < -30000 ? -30000 : v);
      }
    }
    int16_t* t = h0; h0 = h1; h1 = t;
  }
  // 雨粒
  rainT -= dt;
  if (RAIN > 0 && rainT < 0) {
    rainT = (0.35f + frand() * 0.9f) / RAIN;
    float x, y;
    do { x = frand(vx0 + 10, vx1 - 10); y = frand(vy0 + 10, vy1 - 10); } while (isRound && (x - Sc) * (x - Sc) + (y - Sc) * (y - Sc) > (VR - 20) * (VR - 20));
    drop(x, y, 3 * k, 9);
  }
  // 傾き: 映る空をずらし、低い方のふちから波を立てる
  tiltOX = (int)(in.tiltX * 26 * k); tiltOY = (int)(in.tiltY * 26 * k);
  {
    const float tm = sqrtf(in.tiltX * in.tiltX + in.tiltY * in.tiltY);
    if (tm > 0.08f && SLOSH > 0) {
      sloshT += dt * 8 * tm * SLOSH;
      const float dx = in.tiltX / tm, dy = in.tiltY / tm;
      while (sloshT > 1) {
        sloshT -= 1;
        const float side = frand(-0.75f, 0.75f) * VR;
        float x = Sc + dx * (VR - 18 * k) * 0.95f - dy * side, y = Sc + dy * (VR - 18 * k) * 0.95f + dx * side;
        x = fminf(fmaxf(x, vx0 + 8), vx1 - 8); y = fminf(fmaxf(y, vy0 + 8), vy1 - 8);
        drop(x, y, 5 * k, 6 + 10 * tm);
      }
    } else sloshT = 0;
  }
  // 指: タップで大きな波紋、なぞると細い波紋の列。指の下はピクセルに結晶する
  if (in.tapped) drop(in.tx, in.ty, 7 * k, 70);
  if (in.touching) {
    if (lastX >= 0 && fabsf(in.tx - lastX) + fabsf(in.ty - lastY) > 3) drop(in.tx, in.ty, 4 * k, 14);
    lastX = in.tx; lastY = in.ty;
    const float gx = in.tx / QSTEP, gy = in.ty / QSTEP, rad = 6.0f * k;
    for (int j = max(0, (int)(gy - rad)); j < min(QG, (int)(gy + rad) + 1); ++j)
      for (int i = max(0, (int)(gx - rad)); i < min(QG, (int)(gx + rad) + 1); ++i) {
        const float d2 = ((i - gx) * (i - gx) + (j - gy) * (j - gy)) / (rad * rad * 0.4f);
        boost[j * QG + i] = fminf(1.6f, boost[j * QG + i] + expf(-d2) * dt * 5);
      }
  } else lastX = -1;
  const float fade = expf(-dt * 5.0f / CRYSTAL);
  for (int p = 0; p < QG * QG; ++p) boost[p] *= fade;
  // デジタル化の段階(ゆっくり動く境目 + 触った所)
  const float amt = digi == 0 ? 1.0f : digi == 1 ? 1.8f : 0.0f;
  const float sx = 9.0f * k, sx2 = 4.0f * k;
  for (int j = 0; j < QG; ++j)
    for (int i = 0; i < QG; ++i) {
      const float n = noiseAt(nzC, 32, i / sx + simT * 0.05f, j / sx - simT * 0.03f) * 0.7f + noiseAt(nzD, 32, i / sx2 - simT * 0.08f, j / sx2 + simT * 0.06f) * 0.3f;
      const float q = fmaxf(0, (n - 0.5f) * 3.2f) * amt + boost[j * QG + i];
      level[j * QG + i] = q < 0.35f ? 0 : q < 0.75f ? 1 : q < 1.15f ? 2 : 3;
    }
}

// 水面の1点の色(sx, sy: 読む位置。モザイクではブロックのまん中)
static inline uint32_t pondColor(int sx, int sy, int KF, int hy, int lv) {
  int hx = xdiv3[sx];
  hx = hx < 1 ? 1 : hx > HW - 2 ? HW - 2 : hx;
  const int16_t* hp = h1 + hy * HW + hx;
  const int dx = hp[1] - hp[-1], dy = hp[HW] - hp[-HW];
  int ux = sx + ((dx * KF) >> 8) + tiltOX, uy = sy + ((dy * KF) >> 8) + tiltOY;
  ux = ux < 0 ? 0 : ux > S - 1 ? S - 1 : ux; uy = uy < 0 ? 0 : uy > S - 1 ? S - 1 : uy;
  const uint32_t c = sky[(uy >> 1) * SW2 + (ux >> 1)];
  int r = c >> 11, g = (c >> 5) & 63, bl = c & 31;
  // 波のきらめき
  const int sp = dy < 0 ? (-dy) >> 4 : 0;
  r += sp >> 1; g += sp; bl += sp >> 1;
  r = r > 31 ? 31 : r; g = g > 63 ? 63 : g; bl = bl > 31 ? 31 : bl;
  if (lv) { r = post5[lv][r]; g = post6[lv][g]; bl = post5[lv][bl]; }   // デジタル: 色を段に丸める
  return (uint32_t)((r << 11) | (g << 5) | bl);
}
static inline uint16_t gridDark(uint32_t c) {        // ブロックのすきまの細い線
  int r = c >> 11, g = (c >> 5) & 63, b = c & 31;
  return (uint16_t)((((r * 9) >> 4) << 11) | (((g * 10) >> 4) << 5) | ((b * 11) >> 4));
}

static void renderBand(int k, uint16_t* buf, Scratch& scr) {
  const int y0 = k * BAND, y1 = min(S, y0 + BAND);
  const int bx0 = bandX0[k], bw = bandW[k], bx1 = bx0 + bw - 1;
  const int KF = (int)(6.0f * RIPPLE * 256.0f / 64.0f);       // 高さの差 → 何ピクセルずらすか(×256)
  for (int y = y0; y < y1; ++y) {
    uint16_t* row = buf + (y - y0) * bw - bx0;
    const int xs = max((int)spanS[y], bx0), xe = min((int)spanE[y], bx1);
    for (int x = bx0; x < xs; ++x) row[x] = 0;
    for (int x = max(xe + 1, bx0); x <= bx1; ++x) row[x] = 0;
    if (xs > xe) continue;
    const uint8_t* lrow = level + (y / QSTEP) * QG;
    int hyN = xdiv3[y]; hyN = hyN < 1 ? 1 : hyN > HW - 2 ? HW - 2 : hyN;
    // この行の、水面の格子の列ごとの「ずらす量」と「きらめき」を先に出しておく
    {
      const int16_t* hp = h1 + hyN * HW;
      const int ha = max(1, (int)xdiv3[xs]), hb = min(HW - 2, (int)xdiv3[xe]);
      for (int hx = ha; hx <= hb; ++hx) {
        const int dx = hp[hx + 1] - hp[hx - 1], dy = hp[hx + HW] - hp[hx - HW];
        scr.ox[hx] = (int16_t)((dx * KF) >> 8); scr.oy[hx] = (int16_t)((dy * KF) >> 8);
        int sp = dy < 0 ? (-dy) >> 4 : 0; if (sp > 31) sp = 31;
        scr.sp[hx] = (uint16_t)(((sp >> 1) << 11) | (sp << 5) | (sp >> 1));
      }
      scr.ox[0] = scr.oy[0] = 0; scr.sp[0] = 0;
    }
    int x = xs;
    while (x <= xe) {
      const int cell = x / QSTEP, cEnd = min(xe, cell * QSTEP + QSTEP - 1);
      const int lv = lrow[cell];
      if (lv == 0) {                                  // 自然のまま: 1ピクセルずつ
        for (; x <= cEnd; ++x) {
          int hx = xdiv3[x]; hx = hx < 1 ? 1 : hx > HW - 2 ? HW - 2 : hx;
          int ux = x + scr.ox[hx] + tiltOX, uy = y + scr.oy[hx] + tiltOY;
          ux = ux < 0 ? 0 : ux > S - 1 ? S - 1 : ux; uy = uy < 0 ? 0 : uy > S - 1 ? S - 1 : uy;
          uint32_t c = sky[(uy >> 1) * SW2 + (ux >> 1)];
          if (scr.sp[hx]) c = add565(c, scr.sp[hx]);
          row[x] = outPix((uint16_t)c);
        }
        continue;
      }
      // モザイク: ブロックごとに1回だけ色を出して、並べる(すきまは細い線)
      const int b = BS[lv], sh = lv + 1;               // 4 / 8 / 16 = 1 << (lv + 1)
      const int sy = ((y >> sh) << sh) + (b >> 1);
      int hyB = xdiv3[sy < S ? sy : S - 1]; hyB = hyB < 1 ? 1 : hyB > HW - 2 ? HW - 2 : hyB;
      const bool lineRow = (y & (b - 1)) == 0;
      while (x <= cEnd) {
        const int bs = (x >> sh) << sh, bEnd = min(cEnd, bs + b - 1);
        const int sx = min(S - 1, bs + (b >> 1));
        const uint32_t c = pondColor(sx, sy < S ? sy : S - 1, KF, hyB, lv);
        const uint16_t cOut = outPix((uint16_t)c), dOut = outPix(gridDark(c));
        if (lineRow) { for (; x <= bEnd; ++x) row[x] = dOut; }
        else { if (x == bs) row[x++] = dOut; for (; x <= bEnd; ++x) row[x] = cOut; }
      }
    }
  }
}

static constexpr size_t nmOa39 = nmAlignTo(nmOv39, 16), nmOv40 = nmOa39 + sizeof(Scratch[2]); static Scratch (&scr)[2] = *reinterpret_cast<Scratch(*)[2]>(NM_ARENA + nmOa39);

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
void setup() {
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
static_assert(std::is_trivially_default_constructible<uint32_t>::value, "NM_ARENA: uint32_t");
static_assert(std::is_trivially_default_constructible<uint8_t>::value, "NM_ARENA: uint8_t");
static constexpr size_t OVL_END = nmAlignTo(nmOv40, 16);
static_assert(OVL_END <= NM_ARENA_SIZE, "NM_ARENA_SIZE (nm_common.h) を大きくしてください");
extern const size_t OVL_SIZE = OVL_END;   // 起動時の確認用
}  // namespace NullPond
