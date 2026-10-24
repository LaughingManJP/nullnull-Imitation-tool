// NullFloat  -  NullMenu 用モジュール(元のスケッチ NullFloat_v6 を名前空間に入れたもの)
// 0 で始まる変数(配列も)は、ほかの作品と共通の置き場(NM_ARENA)に置いています。
// 一度に動くのは1つだけ(切り替えるときは再起動)なので、重なっていても問題ありません。
#include "nm_common.h"
#include <algorithm>
#pragma GCC optimize("O3")

namespace NullFloat {
static constexpr size_t nmOv0 = 0;

// NullFloat v6  -  浮遊する光。見えない音の波が重なってできる「節」に、光の粒が集まって浮かぶ
//   メディアアートの作品(音で物を浮かせる研究や、空中に光を描く作品)から受けたイメージで作った、非公式の習作です。
//   作品の映像・名前・データは使っていません。
//   M5Stack StopWatch 用(丸い画面)。CoreS3 系でも画面いっぱいに動きます(自動で切り替わります)。
//
// 画面の中:
//   ・まわりに見えない音源がいくつかあり、その波が重なって、強いところと弱いところ(節)の模様ができる(青くうっすら見える)
//   ・光の粒は、波の節へ押し集められて、小さな光のかたまりになって浮かぶ
//   ・音源の位相がゆっくりずれると、節の模様が組み変わり、粒もそれにつれて流れて集まり直す
//
// 傾けると(本体の傾きセンサー):
//   ・光の粒が低い方へ流れていく。水平に戻すと、また波の節に引き戻されて集まる
//   ・傾きの向きが逆のときは、調整用の TILT_FLIP_X / TILT_FLIP_Y / TILT_SWAP を変える。Serial に tilt: の値が出る
//
// 触ると:
//   ・押している所に波が集まり(焦点)、粒が押しのけられて輪になる。なぞると、輪が指についていく
//
// 操作:
//   StopWatch … ボタンA 短押し: 音源の数(4 / 6 / 8)   ボタンA 長押し: 色(青 / 金 / 白)   ボタンB: 明るさ
//   CoreS3系  … 画面の上半分をダブルタップ: 音源の数   下半分をダブルタップ: 色   電源ボタン短押し: 明るさ
//
// 必要なもの:
//   ボードマネージャー M5Stack >= 3.3.7 / ボード: M5StopWatch(PSRAM は OPI)、CoreS3 なら M5CoreS3
//   ライブラリ M5Unified >= 0.2.16, M5GFX >= 0.2.21
//
// しくみ:
//   波の強さは 6 ピクセルおきの格子の上だけで計算し、間は補間します。音源は動かないので、
//   各格子点から各音源までの「距離 × 波数」の cos と sin を起動時に表にしておき(PSRAM)、
//   毎フレームは位相の回転(かけ算と足し算)だけで、重なった波の強さを出します。
//   粒は、波の強さの坂を下るように動きます(強いところから弱いところへ)。
//   画面は8行ずつの帯に分けて、2つのコアで描きます。StopWatch では、帯ごとに丸の中だけを画面へ送ります。



// ---- 調整用 ----------------------------------------------------------------
static constexpr float WAVELEN   = 62.0f;   // 波長(466 の画面でのピクセル。小さいほど節が細かい)
static constexpr int   NPART     = 1200;    // 光の粒の数
static constexpr float PUSH      = 5200.0f; // 粒を節へ押し集める強さ
static constexpr float JITTER    = 14.0f;   // 粒のゆらぎ
static constexpr float DRIFT     = 1.0f;    // 模様が組み変わる速さ
static constexpr float GLOW      = 1.0f;    // 粒の明るさ
static constexpr float TILT_PUSH = 420.0f;  // 傾けたとき、粒が低い方へ流れる強さ
static constexpr bool  DOUBLE_TAP_UI = true;
// ---------------------------------------------------------------------------
static constexpr int NSRC_MAX  = 8;
static constexpr int GS        = 6;          // 波を計算する格子の間隔(ピクセル)
static constexpr int MAXDOT    = NPART * 2;
static constexpr int MAXDOTENT = MAXDOT * 3;

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
struct DotDraw { int16_t x, y; uint8_t r, a; };
struct Scratch { float* ru; };              // 1行ぶんの、格子の列ごとの波の強さ

static const char* SKETCH_NAME = "NullFloat v6";

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

// ---------------------------------------------------------------- 光の点(帯ごとに振り分けて、光を足して描く)
static DotDraw* dots;                 // [MAXDOT]
static constexpr size_t nmOa24 = nmAlignTo(nmOv24, alignof(int)), nmOv25 = nmOa24 + sizeof(int); static int& nDot = *reinterpret_cast<int*>(NM_ARENA + nmOa24);
static int16_t* dotEnt;               // [MAXDOTENT] 帯ごとの、点の番号
static constexpr size_t nmOa25 = nmAlignTo(nmOv25, 16), nmOv26 = nmOa25 + sizeof(int16_t[MAXNB + 1]); static int16_t (&dotStart)[MAXNB + 1] = *reinterpret_cast<int16_t(*)[MAXNB + 1]>(NM_ARENA + nmOa25);
static constexpr size_t nmOa26 = nmAlignTo(nmOv26, 16), nmOv27 = nmOa26 + sizeof(uint16_t[16][8]); static uint16_t (&dotCol)[16][8] = *reinterpret_cast<uint16_t(*)[16][8]>(NM_ARENA + nmOa26);  // [明るさの段][中心からの段] 足す色(565)
static void buildDotColors(Col c, float gain) {
  static const float prof[8] = {1.0f, 0.82f, 0.52f, 0.30f, 0.17f, 0.09f, 0.045f, 0.0f};
  for (int a = 0; a < 16; ++a) for (int q = 0; q < 8; ++q) dotCol[a][q] = to565(mulC(c, gain * a / 15.0f * prof[q]));
}
static inline void addDot(float x, float y, int r, int a) {
  if (nDot >= MAXDOT || a <= 0) return;
  if (r < 1) r = 1;
  if (r > 12) r = 12;
  dots[nDot++] = {(int16_t)x, (int16_t)y, (uint8_t)r, (uint8_t)(a > 15 ? 15 : a)};
}
static void binDots() {
  for (int k = 0; k <= NB; ++k) dotStart[k] = 0;
  int total = 0, na = nDot;
  for (int i = 0; i < na; ++i) {
    const int a = max(0, dots[i].y - dots[i].r) / BAND, b = min(S - 1, max(0, dots[i].y + dots[i].r)) / BAND;
    if (total + (b - a + 1) > MAXDOTENT) { na = i; break; }
    for (int k = a; k <= b; ++k) dotStart[k + 1]++;
    total += b - a + 1;
  }
  for (int k = 0; k < NB; ++k) dotStart[k + 1] += dotStart[k];
  int16_t fill[MAXNB];
  for (int k = 0; k < NB; ++k) fill[k] = dotStart[k];
  for (int i = 0; i < na; ++i) {
    const int a = max(0, dots[i].y - dots[i].r) / BAND, b = min(S - 1, max(0, dots[i].y + dots[i].r)) / BAND;
    for (int k = a; k <= b; ++k) dotEnt[fill[k]++] = (int16_t)i;
  }
}
static void drawDots(int k, uint16_t* buf, int y0, int y1, int bx0, int bx1, int bw) {
  for (int e = dotStart[k]; e < dotStart[k + 1]; ++e) {
    const DotDraw& d = dots[dotEnt[e]];
    const int R = d.r, R1 = (R + 1) * (R + 1);
    const uint16_t* col = dotCol[d.a];
    for (int y = max(y0, d.y - R); y <= min(y1 - 1, d.y + R); ++y) {
      const int dy = y - d.y, xs = max(max((int)spanS[y], bx0), d.x - R), xe = min(min((int)spanE[y], bx1), d.x + R);
      uint16_t* row = buf + (y - y0) * bw - bx0;
      for (int x = xs; x <= xe; ++x) {
        const int dx = x - d.x, d2 = dx * dx + dy * dy;
        if (d2 >= R1) continue;
        const uint16_t c = col[ringQ[d2 * 64 / R1]];
        if (c) row[x] = addPix(row[x], c);
      }
    }
  }
}

// ---------------------------------------------------------------- 波
static constexpr size_t nmOa27 = nmAlignTo(nmOv27, alignof(int)), nmOv28 = nmOa27 + sizeof(int); static int& G = *reinterpret_cast<int*>(NM_ARENA + nmOa27);  // 格子の数(1辺)
static int nSrc = 6;
static constexpr size_t nmOa28 = nmAlignTo(nmOv28, 16), nmOv29 = nmOa28 + sizeof(float[NSRC_MAX]); static float (&srcX)[NSRC_MAX] = *reinterpret_cast<float(*)[NSRC_MAX]>(NM_ARENA + nmOa28);
static constexpr size_t nmOa29 = nmAlignTo(nmOv29, 16), nmOv30 = nmOa29 + sizeof(float[NSRC_MAX]); static float (&srcY)[NSRC_MAX] = *reinterpret_cast<float(*)[NSRC_MAX]>(NM_ARENA + nmOa29);
static constexpr size_t nmOa30 = nmAlignTo(nmOv30, alignof(float)), nmOv31 = nmOa30 + sizeof(float); static float& K = *reinterpret_cast<float*>(NM_ARENA + nmOa30);  // 波数(ラジアン / ピクセル)
static int16_t* tabC;                  // [G*G*NSRC_MAX] cos(k r) × 16384
static int16_t* tabS;                  // [G*G*NSRC_MAX] sin(k r) × 16384
static float* U;                       // [G*G] 波の強さ(0..1)
static constexpr size_t nmOa31 = nmAlignTo(nmOv31, 16), nmOv32 = nmOa31 + sizeof(float[NSRC_MAX]); static float (&basePh)[NSRC_MAX] = *reinterpret_cast<float(*)[NSRC_MAX]>(NM_ARENA + nmOa31);
static constexpr size_t nmOa32 = nmAlignTo(nmOv32, alignof(float)), nmOv33 = nmOa32 + sizeof(float); static float& focusW = *reinterpret_cast<float*>(NM_ARENA + nmOa32);
static constexpr size_t nmOa33 = nmAlignTo(nmOv33, alignof(float)), nmOv34 = nmOa33 + sizeof(float); static float& fX = *reinterpret_cast<float*>(NM_ARENA + nmOa33);
static constexpr size_t nmOa34 = nmAlignTo(nmOv34, alignof(float)), nmOv35 = nmOa34 + sizeof(float); static float& fY = *reinterpret_cast<float*>(NM_ARENA + nmOa34);
static constexpr size_t nmOa35 = nmAlignTo(nmOv35, alignof(int)), nmOv36 = nmOa35 + sizeof(int); static int& theme = *reinterpret_cast<int*>(NM_ARENA + nmOa35);
static constexpr size_t nmOa36 = nmAlignTo(nmOv36, 16), nmOv37 = nmOa36 + sizeof(uint16_t[256]); static uint16_t (&bgLUT)[256] = *reinterpret_cast<uint16_t(*)[256]>(NM_ARENA + nmOa36);  // 波の強さ → 背景の色(565)
static constexpr size_t nmOa37 = nmAlignTo(nmOv37, 16), nmOv38 = nmOa37 + sizeof(uint16_t[32]); static uint16_t (&glowLUT)[32] = *reinterpret_cast<uint16_t(*)[32]>(NM_ARENA + nmOa37);  // 焦点のにじみ(距離の段ごと)

static constexpr size_t nmOa38 = nmAlignTo(nmOv38, alignof(float*)), nmOv39 = nmOa38 + sizeof(float*); static float*& px = *reinterpret_cast<float**>(NM_ARENA + nmOa38);
static constexpr size_t nmOa39 = nmAlignTo(nmOv39, alignof(float*)), nmOv40 = nmOa39 + sizeof(float*); static float*& py = *reinterpret_cast<float**>(NM_ARENA + nmOa39);
static constexpr size_t nmOa40 = nmAlignTo(nmOv40, alignof(float*)), nmOv41 = nmOa40 + sizeof(float*); static float*& pvx = *reinterpret_cast<float**>(NM_ARENA + nmOa40);
static constexpr size_t nmOa41 = nmAlignTo(nmOv41, alignof(float*)), nmOv42 = nmOa41 + sizeof(float*); static float*& pvy = *reinterpret_cast<float**>(NM_ARENA + nmOa41);
static constexpr size_t nmOa42 = nmAlignTo(nmOv42, alignof(float*)), nmOv43 = nmOa42 + sizeof(float*); static float*& ptw = *reinterpret_cast<float**>(NM_ARENA + nmOa42);
static constexpr size_t nmOa43 = nmAlignTo(nmOv43, alignof(float*)), nmOv44 = nmOa43 + sizeof(float*); static float*& psz = *reinterpret_cast<float**>(NM_ARENA + nmOa43);

static bool allocScratch(Scratch& s) { s.ru = (float*)malloc((S / GS + 4) * sizeof(float)); return s.ru != nullptr; }

static bool pieceAlloc() {
  G = S / GS + 3;
  tabC = (int16_t*)ps_malloc((size_t)G * G * NSRC_MAX * 2);
  tabS = (int16_t*)ps_malloc((size_t)G * G * NSRC_MAX * 2);
  U = (float*)allocFast((size_t)G * G * sizeof(float));
  dots = (DotDraw*)allocFast(sizeof(DotDraw) * MAXDOT);
  dotEnt = (int16_t*)allocFast(sizeof(int16_t) * MAXDOTENT);
  px = (float*)allocFast(NPART * 4); py = (float*)allocFast(NPART * 4);
  pvx = (float*)allocFast(NPART * 4); pvy = (float*)allocFast(NPART * 4);
  ptw = (float*)allocFast(NPART * 4); psz = (float*)allocFast(NPART * 4);
  return tabC && tabS && U && dots && dotEnt && px && py && pvx && pvy && ptw && psz;
}

static void buildSources() {
  for (int i = 0; i < nSrc; ++i) {
    const float a = 6.2831853f * i / nSrc + 0.26f;
    srcX[i] = Sc + cosf(a) * VR * 1.08f; srcY[i] = Sc + sinf(a) * VR * 1.08f;
  }
  for (int gy = 0; gy < G; ++gy)
    for (int gx = 0; gx < G; ++gx) {
      int16_t* c = tabC + ((size_t)gy * G + gx) * NSRC_MAX;
      int16_t* s = tabS + ((size_t)gy * G + gx) * NSRC_MAX;
      for (int i = 0; i < nSrc; ++i) {
        const float kr = K * sqrtf((gx * GS - srcX[i]) * (gx * GS - srcX[i]) + (gy * GS - srcY[i]) * (gy * GS - srcY[i]));
        c[i] = (int16_t)lrintf(cosf(kr) * 16383.0f); s[i] = (int16_t)lrintf(sinf(kr) * 16383.0f);
      }
    }
}

static void buildColors() {
  static const Col fg[3] = { C8(150, 205, 255), C8(255, 210, 140), C8(240, 240, 250) };   // 粒の色
  static const Col bgA[3] = { C8(3, 6, 14), C8(10, 6, 3), C8(6, 6, 8) };               // 背景(波が弱い所)
  static const Col bgB[3] = { C8(30, 56, 106), C8(96, 62, 22), C8(60, 62, 70) };       // 背景(波が強い所)
  for (int i = 0; i < 256; ++i) {
    const float u = i / 255.0f, f = sqrtf(u), fr = 0.5f + 0.5f * cosf(u * 18.0f);
    const float k = u * 0.75f + fr * f * 0.45f;
    bgLUT[i] = to565(lerpC(bgA[theme], bgB[theme], clamp01(k)));
  }
  buildDotColors(fg[theme], GLOW);
  for (int i = 0; i < 32; ++i) { const float d = i / 31.0f; glowLUT[i] = to565(mulC(fg[theme], 0.20f * (1 - d) * (1 - d))); }
}

static void resetParticles() {
  for (int i = 0; i < NPART; ++i) {
    float x, y;
    do { x = frand(vx0 + 6, vx1 - 6); y = frand(vy0 + 6, vy1 - 6); } while (isRound && (x - Sc) * (x - Sc) + (y - Sc) * (y - Sc) > (VR - 8) * (VR - 8));
    px[i] = x; py[i] = y; pvx[i] = pvy[i] = 0;
    ptw[i] = frand(0, 6.2832f); psz[i] = frand(0.55f, 1.45f);
  }
}

static void pieceInit() {
  K = 6.2831853f / (WAVELEN * S / 466.0f);
  for (int i = 0; i < NSRC_MAX; ++i) basePh[i] = frand(0, 6.2832f);
  buildSources();
  buildColors();
  resetParticles();
  fX = fY = Sc;
}

static void onA() {                         // 音源の数を変える
  nSrc = nSrc == 4 ? 6 : nSrc == 6 ? 8 : 4;
  buildSources();
  Serial.printf("sources: %d\n", nSrc);
}
static void onAHold() { theme = (theme + 1) % 3; buildColors(); Serial.printf("color: %d\n", theme); }
static void statusText(char* o, int n) { snprintf(o, n, "src %d", nSrc); }

static inline float sampleU(float x, float y) {
  float gx = x / GS, gy = y / GS;
  gx = gx < 0 ? 0 : gx > G - 1.001f ? G - 1.001f : gx;
  gy = gy < 0 ? 0 : gy > G - 1.001f ? G - 1.001f : gy;
  const int ix = (int)gx, iy = (int)gy; const float f = gx - ix, g = gy - iy;
  const float* p = U + iy * G + ix;
  return (p[0] * (1 - f) + p[1] * f) * (1 - g) + (p[G] * (1 - f) + p[G + 1] * f) * g;
}

static void updateFrame(const Input& in) {
  float dt = in.dt > 0.05f ? 0.05f : in.dt;
  if (dt < 0.0005f) dt = 0.0005f;
  simT += dt;

  // 焦点: 押している間、指の所へ波を集める
  focusW += ((in.touching ? 1.0f : 0.0f) - focusW) * (1.0f - expf(-dt * (in.touching ? 6.0f : 1.2f)));
  if (in.touching) { fX = in.tx; fY = in.ty; }
  float cs[NSRC_MAX], sn[NSRC_MAX];
  for (int i = 0; i < nSrc; ++i) {
    const float t = simT * DRIFT;
    const float wander = basePh[i] + 2.2f * sinf(t * 0.09f + i * 1.9f) + 1.1f * sinf(t * 0.041f + i * 0.7f);
    const float focus = -K * sqrtf((fX - srcX[i]) * (fX - srcX[i]) + (fY - srcY[i]) * (fY - srcY[i]));
    float d = focus - wander; d -= roundf(d / 6.2831853f) * 6.2831853f;
    const float ph = wander + d * focusW;
    cs[i] = cosf(ph) * (1.0f / 16383.0f); sn[i] = sinf(ph) * (1.0f / 16383.0f);
  }
  // 重なった波の強さ(位相を回して足すだけ)
  const float inv = 1.0f / (nSrc * nSrc);
  for (int p = 0; p < G * G; ++p) {
    const int16_t* c = tabC + (size_t)p * NSRC_MAX;
    const int16_t* s = tabS + (size_t)p * NSRC_MAX;
    float re = 0, im = 0;
    for (int i = 0; i < nSrc; ++i) { const float C = c[i], Sn = s[i]; re += C * cs[i] - Sn * sn[i]; im += Sn * cs[i] + C * sn[i]; }
    U[p] = (re * re + im * im) * inv;
  }

  // 粒: 波の強いところから弱いところへ(強さの坂を下る)、少しゆらぐ
  const float damp = expf(-dt * 4.0f), h = 2.0f, sc = S / 466.0f;
  nDot = 0;
  for (int i = 0; i < NPART; ++i) {
    const float x = px[i], y = py[i];
    const float gx = (sampleU(x + h, y) - sampleU(x - h, y)) / (2 * h), gy = (sampleU(x, y + h) - sampleU(x, y - h)) / (2 * h);
    pvx[i] = (pvx[i] - gx * PUSH * sc * dt + in.tiltX * TILT_PUSH * sc * dt + (frand() - 0.5f) * JITTER * dt) * damp;
    pvy[i] = (pvy[i] - gy * PUSH * sc * dt + in.tiltY * TILT_PUSH * sc * dt + (frand() - 0.5f) * JITTER * dt) * damp;
    float nx = x + pvx[i] * dt, ny = y + pvy[i] * dt;
    if (isRound) {
      const float dx = nx - Sc, dy = ny - Sc, d = sqrtf(dx * dx + dy * dy), lim = VR - 7;
      if (d > lim) { nx = Sc + dx / d * lim; ny = Sc + dy / d * lim; pvx[i] *= -0.3f; pvy[i] *= -0.3f; }
    } else {
      if (nx < vx0 + 4 || nx > vx1 - 4) { pvx[i] *= -0.3f; nx = fminf(fmaxf(nx, vx0 + 4), vx1 - 4); }
      if (ny < vy0 + 4 || ny > vy1 - 4) { pvy[i] *= -0.3f; ny = fminf(fmaxf(ny, vy0 + 4), vy1 - 4); }
    }
    px[i] = nx; py[i] = ny;
    // 描く点(瞬き + 速いと少し大きく、うしろに淡い残像)
    const float sp = fminf(1.0f, sqrtf(pvx[i] * pvx[i] + pvy[i] * pvy[i]) / 60.0f);
    const float a = (0.45f + 0.4f * sinf(ptw[i] + simT * 2.3f)) * (0.75f + 0.25f * sp);
    const int r = (int)(3.6f * psz[i] * (1 + 0.6f * sp) * sc + 0.5f);
    addDot(nx, ny, r, (int)(a * 15.0f + 0.5f));
    if (sp > 0.25f) addDot(nx - pvx[i] * 0.035f, ny - pvy[i] * 0.035f, r, (int)(a * 7.0f));
  }
  binDots();
}

static void renderBand(int k, uint16_t* buf, Scratch& sc) {
  const int y0 = k * BAND, y1 = min(S, y0 + BAND);
  const int bx0 = bandX0[k], bw = bandW[k], bx1 = bx0 + bw - 1;
  const float gRad = 62.0f * S / 466.0f, gR2 = gRad * gRad;
  for (int y = y0; y < y1; ++y) {
    uint16_t* row = buf + (y - y0) * bw - bx0;
    const int xs = max((int)spanS[y], bx0), xe = min((int)spanE[y], bx1);
    for (int x = bx0; x < xs; ++x) row[x] = 0;
    for (int x = max(xe + 1, bx0); x <= bx1; ++x) row[x] = 0;
    if (xs > xe) continue;
    // 背景: 波の重なりの模様(格子の値を縦に補間し、横は足し算で進める)
    const int gy = y / GS; const float fy = (float)(y - gy * GS) / GS;
    const int gx0 = xs / GS, gx1 = xe / GS + 1;
    const float* u0 = U + gy * G; const float* u1 = u0 + G;
    for (int gx = gx0; gx <= gx1; ++gx) sc.ru[gx - gx0] = (u0[gx] + (u1[gx] - u0[gx]) * fy) * 255.0f;
    int x = xs;
    while (x <= xe) {
      const int gx = x / GS, segEnd = min(xe, gx * GS + GS - 1);
      const float a = sc.ru[gx - gx0], du = (sc.ru[gx - gx0 + 1] - a) * (1.0f / GS);
      float u = a + du * (x - gx * GS);
      for (; x <= segEnd; ++x, u += du) { int i = (int)u; i = i < 0 ? 0 : i > 255 ? 255 : i; row[x] = outPix(bgLUT[i]); }
    }
    // 焦点のにじみ
    if (focusW > 0.02f) {
      const float dy = y - fY;
      if (dy * dy < gR2) {
        const float hw = sqrtf(gR2 - dy * dy);
        const int a0 = max(xs, (int)(fX - hw)), a1 = min(xe, (int)(fX + hw));
        const int fw = (int)(focusW * 16.0f);
        for (int xx = a0; xx <= a1; ++xx) {
          const float dx = xx - fX; const int q = (int)((dx * dx + dy * dy) / gR2 * 31.0f);
          row[xx] = addPix(row[xx], mix565(0, glowLUT[q > 31 ? 31 : q], fw));
        }
      }
    }
  }
  drawDots(k, buf, y0, y1, bx0, bx1, bw);
}

static constexpr size_t nmOa44 = nmAlignTo(nmOv44, 16), nmOv45 = nmOa44 + sizeof(Scratch[2]); static Scratch (&scr)[2] = *reinterpret_cast<Scratch(*)[2]>(NM_ARENA + nmOa44);

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
static_assert(std::is_trivially_default_constructible<int16_t>::value, "NM_ARENA: int16_t");
static_assert(std::is_trivially_default_constructible<uint16_t>::value, "NM_ARENA: uint16_t");
static_assert(std::is_trivially_default_constructible<uint32_t>::value, "NM_ARENA: uint32_t");
static_assert(std::is_trivially_default_constructible<uint8_t>::value, "NM_ARENA: uint8_t");
static constexpr size_t OVL_END = nmAlignTo(nmOv45, 16);
static_assert(OVL_END <= NM_ARENA_SIZE, "NM_ARENA_SIZE (nm_common.h) を大きくしてください");
extern const size_t OVL_SIZE = OVL_END;   // 起動時の確認用
}  // namespace NullFloat
