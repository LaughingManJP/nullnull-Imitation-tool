// NullZen  -  NullMenu 用モジュール(元のスケッチ NullZen_v6 を名前空間に入れたもの)
// 0 で始まる変数(配列も)は、ほかの作品と共通の置き場(NM_ARENA)に置いています。
// 一度に動くのは1つだけ(切り替えるときは再起動)なので、重なっていても問題ありません。
#include "nm_common.h"
#include <algorithm>
#pragma GCC optimize("O3")

namespace NullZen {
static constexpr size_t nmOv0 = 0;

// NullZen v6  -  計算機の枯山水。白砂に砂紋、石のまわりには同心円。タップで石を置くと、熊手が砂紋を引き直す
//   null²(大阪・関西万博のパビリオン)の静かな空間から受けたイメージで作った、非公式の習作です。作品の映像・名前・データは使っていません。
//   M5Stack StopWatch 用(丸い画面)。CoreS3 系でも画面いっぱいに動きます(自動で切り替わります)。
//
// 画面の中:
//   ・白砂に、石のまわりは同心円の輪(4本)、その外はまっすぐの砂紋。石どうしが近いと、輪がなめらかにつながる
//   ・砂紋の溝には光と影がつき、木漏れ日の影がゆっくり揺れる
//
// 傾けると(本体の傾きセンサー):
//   ・白砂がさらさらと低い方へ流れて、砂紋が少しずつ崩れていく(ボタンA や石を置くと、熊手が引き直す)
//   ・傾きの向きが逆のときは、調整用の TILT_FLIP_X / TILT_FLIP_Y / TILT_SWAP を変える。Serial に tilt: の値が出る
//
// 触ると:
//   ・砂の上をタップ: そこに石が置かれ(最大 5 つ。古いものから消える)、熊手が左から右へ横切りながら砂紋を引き直す
//   ・石をドラッグ: 石を動かせる。離すと、熊手が砂紋を引き直す
//
// 操作:
//   StopWatch … ボタンA 短押し: 砂紋の向き(横 / ななめ / 波)を変えて引き直す   ボタンA 長押し: 砂の色(白砂 / 灰砂)   ボタンB: 明るさ
//   CoreS3系  … 電源ボタン短押し: 明るさ(タップは石を置く)
//
// 必要なもの:
//   ボードマネージャー M5Stack >= 3.3.7 / ボード: M5StopWatch(PSRAM は OPI)、CoreS3 なら M5CoreS3
//   ライブラリ M5Unified >= 0.2.16, M5GFX >= 0.2.21
//
// しくみ:
//   砂紋の絵は PSRAM に1枚取っておき、熊手が通った列だけ描き直します(起動時だけ全部描きます)。
//   砂紋の位相は「石からの距離(なめらかな最小値)」と「まっすぐの線」を、輪の本数の所でなめらかにつないで決めます。
//   石と木漏れ日の影は、毎フレーム上に重ねて描きます(影は 8 ピクセルおきの格子で計算して補間)。
//   2つのコアで帯を描き、StopWatch では帯ごとに丸の中だけを画面へ送ります。



// ---- 調整用 ----------------------------------------------------------------
static constexpr float SPACING   = 12.0f;   // 砂紋の間隔(466 の画面でのピクセル)
static constexpr int   RINGS     = 4;       // 石のまわりの輪の数
static constexpr float RAKE_SPD  = 260.0f;  // 熊手の速さ(ピクセル/秒)
static constexpr int   MAXSTONE  = 5;       // 石の最大の数
static constexpr float SLIDE     = 1.0f;    // 傾けたとき、砂が流れる速さ(0 で流れない)
static constexpr float SHADE     = 1.0f;    // 木漏れ日の影の濃さ(0 でなし)
static constexpr bool  DOUBLE_TAP_UI = false;
// ---------------------------------------------------------------------------
static constexpr int SHG = 8;               // 影の格子の間隔(ピクセル)
static constexpr int NLEAF = 9;

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
struct Stone { float x, y, r, sq; };
struct Scratch { float* sh; };              // 1行ぶん、影の格子の列ごとの値

static const char* SKETCH_NAME = "NullZen v6";

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

// ---------------------------------------------------------------- 砂と石
static uint16_t* sand;                 // [S*S] 砂紋の絵(565。PSRAM)
static constexpr size_t nmOa24 = nmAlignTo(nmOv24, 16), nmOv25 = nmOa24 + sizeof(Stone[MAXSTONE]); static Stone (&stones)[MAXSTONE] = *reinterpret_cast<Stone(*)[MAXSTONE]>(NM_ARENA + nmOa24);
static constexpr size_t nmOa25 = nmAlignTo(nmOv25, 16), nmOv26 = nmOa25 + sizeof(Stone[MAXSTONE]); static Stone (&bake)[MAXSTONE] = *reinterpret_cast<Stone(*)[MAXSTONE]>(NM_ARENA + nmOa25);  // 今の石 / 砂紋を引いたときの石
static int grabbed = -1;
static constexpr size_t nmOa26 = nmAlignTo(nmOv26, alignof(int)), nmOv27 = nmOa26 + sizeof(int); static int& nStone = *reinterpret_cast<int*>(NM_ARENA + nmOa26);
static constexpr size_t nmOa27 = nmAlignTo(nmOv27, alignof(int)), nmOv28 = nmOa27 + sizeof(int); static int& nBake = *reinterpret_cast<int*>(NM_ARENA + nmOa27);
static constexpr size_t nmOa28 = nmAlignTo(nmOv28, alignof(int)), nmOv29 = nmOa28 + sizeof(int); static int& style = *reinterpret_cast<int*>(NM_ARENA + nmOa28);
static constexpr size_t nmOa29 = nmAlignTo(nmOv29, alignof(int)), nmOv30 = nmOa29 + sizeof(int); static int& sandTheme = *reinterpret_cast<int*>(NM_ARENA + nmOa29);
static float sweep = 1e9f;  // 熊手の位置(S より大きいと、止まっている)
static constexpr size_t nmOa30 = nmAlignTo(nmOv30, alignof(int)), nmOv31 = nmOa30 + sizeof(int); static int& SGN = *reinterpret_cast<int*>(NM_ARENA + nmOa30);
static float* shade;                   // [SGN*SGN] 木漏れ日の影(0: なし .. 1: 濃い)
static constexpr size_t nmOa31 = nmAlignTo(nmOv31, alignof(float)), nmOv32 = nmOa31 + sizeof(float); static float& SP = *reinterpret_cast<float*>(NM_ARENA + nmOa31);
static constexpr size_t nmOa32 = nmAlignTo(nmOv32, alignof(bool)), nmOv33 = nmOa32 + sizeof(bool); static bool& wasTouch = *reinterpret_cast<bool*>(NM_ARENA + nmOa32);
static constexpr size_t nmOa33 = nmAlignTo(nmOv33, alignof(float)), nmOv34 = nmOa33 + sizeof(float); static float& slideAcc = *reinterpret_cast<float*>(NM_ARENA + nmOa33);
// 色を混ぜる(四捨五入つき。何度も混ぜても、色が暗く・緑にずれていかないように)
static inline uint16_t mixRound(uint32_t a, uint32_t b, int q) {
  const uint32_t A = (a | (a << 16)) & 0x07E0F81Fu, B = (b | (b << 16)) & 0x07E0F81Fu;
  const uint32_t M = ((B * q + A * (16 - q) + 0x01004008u) >> 4) & 0x07E0F81Fu;
  return (uint16_t)(M | (M >> 16));
}

static bool allocScratch(Scratch& s) { s.sh = (float*)malloc((S / SHG + 4) * 4); return s.sh != nullptr; }
static bool pieceAlloc() {
  sand = (uint16_t*)ps_malloc((size_t)S * S * 2);
  SGN = S / SHG + 3;
  shade = (float*)malloc(SGN * SGN * 4);
  return sand && shade;
}

static inline float smooth01(float a, float b, float x) { float t = (x - a) / (b - a); t = t < 0 ? 0 : t > 1 ? 1 : t; return t * t * (3 - 2 * t); }

// 砂の1点の色(石の中は、ここでは砂として塗る。石は上に重ねて描く)
static uint16_t sandAt(int x, int y) {
  float sum = 0, best = 1e9f;
  const float kk = 14.0f * S / 466.0f;
  for (int i = 0; i < nBake; ++i) {
    const Stone& s = bake[i];
    const float dx = x - s.x, dy = (y - s.y) / s.sq, d = sqrtf(dx * dx + dy * dy) - s.r;
    best = fminf(best, d); sum += expf(-d / kk);
  }
  const float d = sum > 0 ? -kk * logf(sum) : 1e9f;
  float lin;
  if (style == 0) lin = y + 7 * sinf(x * 0.011f * 466.0f / S);
  else if (style == 1) lin = (x + y) * 0.7071f;
  else lin = y + 16 * sinf(x * 0.022f * 466.0f / S);
  const float w = smooth01(RINGS * SP - SP * 0.5f, RINGS * SP + SP * 1.5f, d);
  const float p = (d / SP) * (1 - w) + (lin / SP) * w;
  const float f = p - floorf(p);
  const float amp = 1 - 0.85f * 4 * w * (1 - w);             // 輪とまっすぐの境目は、溝をうすく
  const float g = ((hash3(x, y, 5) & 255) / 255.0f - 0.5f) * 0.07f;
  const float shd = best < 10 ? (10 - best) / 10 * 0.18f : 0;  // 石のすぐまわりは、少し影
  const float v = fmaxf(0, 0.86f + (0.09f * cosf(f * 6.2832f) + 0.04f * sinf(f * 6.2832f)) * amp + g - shd);
  return sandTheme == 0 ? rgb565((int)(228 * v), (int)(222 * v), (int)(205 * v)) : rgb565((int)(150 * v), (int)(150 * v), (int)(148 * v));
}
static void bakeColumns(int x0, int x1) {
  for (int y = 0; y < S; ++y) {
    if (spanS[y] > spanE[y]) continue;
    const int a = max(x0, (int)spanS[y]), b = min(x1, (int)spanE[y] + 1);
    for (int x = a; x < b; ++x) sand[(size_t)y * S + x] = sandAt(x, y);
  }
}
static void startRake() { for (int i = 0; i < nStone; ++i) bake[i] = stones[i]; nBake = nStone; sweep = 0; }

static Stone mkStone(float x, float y, float r) { return {x, y, r, frand(0.8f, 1.1f)}; }
static void pieceInit() {
  const float k = S / 466.0f;
  SP = SPACING * k;
  nStone = 0;
  stones[nStone++] = mkStone(Sc - 70 * k, Sc - 30 * k, 34 * k);
  stones[nStone++] = mkStone(Sc + 80 * k, Sc + 40 * k, 26 * k);
  stones[nStone++] = mkStone(Sc + 10 * k, Sc + 110 * k, 18 * k);
  for (int i = 0; i < nStone; ++i) bake[i] = stones[i];
  nBake = nStone;
  memset(sand, 0, (size_t)S * S * 2);
  bakeColumns(0, S);                                       // 起動時に一度、全部引いておく
}
static void onA() { style = (style + 1) % 3; startRake(); Serial.printf("style: %d\n", style); }
static void onAHold() { sandTheme ^= 1; startRake(); Serial.printf("sand: %d\n", sandTheme); }
static void statusText(char* o, int n) { snprintf(o, n, "stones %d %s", nStone, sweep <= S ? "raking" : ""); }

static void updateFrame(const Input& in) {
  float dt = in.dt > 0.05f ? 0.05f : in.dt;
  if (dt < 0.0005f) dt = 0.0005f;
  simT += dt;
  const float t = simT, k = S / 466.0f;
  // 指: 石の上ならつかむ、砂の上をタップなら石を置く。離したら引き直す
  if (in.tapped) {
    grabbed = -1;
    for (int i = 0; i < nStone; ++i) { const float dx = in.tx - stones[i].x, dy = (in.ty - stones[i].y) / stones[i].sq; if (sqrtf(dx * dx + dy * dy) < stones[i].r + 6 * k) grabbed = i; }
    if (grabbed < 0 && (in.tx - Sc) * (in.tx - Sc) + (in.ty - Sc) * (in.ty - Sc) < (VR - 30 * k) * (VR - 30 * k)) {
      if (nStone == MAXSTONE) { for (int i = 1; i < MAXSTONE; ++i) stones[i - 1] = stones[i]; --nStone; }
      stones[nStone++] = mkStone(in.tx, in.ty, (14 + frand() * 20) * k);
      startRake();
    }
  }
  if (in.touching && grabbed >= 0) { stones[grabbed].x = in.tx; stones[grabbed].y = in.ty; }
  if (wasTouch && !in.touching && grabbed >= 0) { grabbed = -1; startRake(); }
  wasTouch = in.touching;
  // 熊手: 左から右へ横切りながら、通った列だけ新しい砂紋を引く
  if (sweep <= S) {
    const float nx = fminf((float)S + 1, sweep + dt * RAKE_SPD * k);
    bakeColumns((int)sweep, min(S, (int)nx));
    sweep = nx;
    if (sweep > S) sweep = 1e9f;
  }
  // 傾き: 白砂が低い方へ、少しずつ流れる(小さな丸い所で、上の砂を下へずらして混ぜる)
  {
    const float tm = sqrtf(in.tiltX * in.tiltX + in.tiltY * in.tiltY);
    if (tm > 0.15f && sweep > S && SLIDE > 0) {
      slideAcc += dt * 450 * (tm - 0.15f) * SLIDE;
      const int ox = (int)roundf(-in.tiltX / tm * 2), oy = (int)roundf(-in.tiltY / tm * 2);   // 2 ピクセル上の方(砂が来る方)
      const int rr = max(4, (int)(7 * k));
      while (slideAcc > 1) {
        slideAcc -= 1;
        const float a = frand(0, 6.2832f), r = sqrtf(frand()) * (VR - 12);
        const int cx = (int)(Sc + cosf(a) * r), cy = (int)(Sc + sinf(a) * r);
        for (int y = max(2, cy - rr); y <= min(S - 3, cy + rr); ++y)
          for (int x = max(2, cx - rr); x <= min(S - 3, cx + rr); ++x) {
            if ((x - cx) * (x - cx) + (y - cy) * (y - cy) > rr * rr) continue;
            uint16_t& p = sand[(size_t)y * S + x];
            p = mixRound(p, sand[(size_t)(y + oy) * S + (x + ox)], 11);
          }
      }
    } else slideAcc = 0;
  }
  // 木漏れ日の影(ゆっくり揺れる、ぼんやりした丸)
  float lx[NLEAF], ly[NLEAF], lr[NLEAF];
  for (int i = 0; i < NLEAF; ++i) {
    lx[i] = Sc + sinf(t * 0.05f + i * 2.1f) * VR * 0.9f + sinf(t * 0.37f + i) * 8 * k;
    ly[i] = Sc + cosf(t * 0.04f + i * 1.3f) * VR * 0.9f;
    lr[i] = (70 + 20 * sinf((float)i)) * k;
  }
  for (int gy = 0; gy < SGN; ++gy)
    for (int gx = 0; gx < SGN; ++gx) {
      const float x = (float)(gx * SHG), y = (float)(gy * SHG);
      float s = 0;
      for (int i = 0; i < NLEAF; ++i) { const float dx = x - lx[i], dy = y - ly[i], d2 = (dx * dx + dy * dy) / (lr[i] * lr[i]); if (d2 < 1) s += (1 - d2) * (1 - d2); }
      shade[gy * SGN + gx] = fminf(1.0f, s) * 0.42f * SHADE;
    }
}

static void renderBand(int k, uint16_t* buf, Scratch& sc) {
  const int y0 = k * BAND, y1 = min(S, y0 + BAND);
  const int bx0 = bandX0[k], bw = bandW[k], bx1 = bx0 + bw - 1;
  const int rx = (int)sweep;
  for (int y = y0; y < y1; ++y) {
    uint16_t* row = buf + (y - y0) * bw - bx0;
    const int xs = max((int)spanS[y], bx0), xe = min((int)spanE[y], bx1);
    for (int x = bx0; x < xs; ++x) row[x] = 0;
    for (int x = max(xe + 1, bx0); x <= bx1; ++x) row[x] = 0;
    if (xs > xe) continue;
    // 影の格子を縦に補間
    const int gy = y / SHG; const float fy = (float)(y - gy * SHG) / SHG;
    const int gx0 = xs / SHG, gx1 = xe / SHG + 1;
    for (int gx = gx0; gx <= gx1; ++gx) sc.sh[gx - gx0] = shade[gy * SGN + gx] + (shade[(gy + 1) * SGN + gx] - shade[gy * SGN + gx]) * fy;
    const uint16_t* src = sand + (size_t)y * S;
    for (int x = xs; x <= xe; ++x) {
      const int gx = x / SHG; const float fx = (float)(x - gx * SHG) / SHG;
      const float s = sc.sh[gx - gx0] + (sc.sh[gx - gx0 + 1] - sc.sh[gx - gx0]) * fx;
      const int q = (int)(s * 16);
      uint16_t c = src[x];
      if (q > 0) c = mix565(c, rgb565(40, 40, 55), q);
      row[x] = outPix(c);
    }
    // 熊手(暗い木の棒と、その右の明るい筋)
    if (rx <= S) {
      for (int x = max(xs, rx - 3); x <= min(xe, rx + 1); ++x) row[x] = outPix(rgb565(90, 60, 35));
      for (int x = max(xs, rx + 2); x <= min(xe, rx + 11); ++x) row[x] = addPix(row[x], rgb565(30, 30, 28));
    }
  }
  // 石: 暗い灰色、上が少し明るく、ふちに苔
  for (int i = 0; i < nStone; ++i) {
    const Stone& s = stones[i];
    const int ya = max(y0, (int)(s.y - s.r * s.sq) - 1), yb = min(y1 - 1, (int)(s.y + s.r * s.sq) + 1);
    for (int y = ya; y <= yb; ++y) {
      const float ny = (y - s.y) / (s.r * s.sq);
      if (ny * ny >= 1) continue;
      const float hw = sqrtf(1 - ny * ny) * s.r;
      const int xs = max(max((int)spanS[y], bx0), (int)(s.x - hw)), xe = min(min((int)spanE[y], bx1), (int)(s.x + hw));
      uint16_t* row = buf + (y - y0) * bw - bx0;
      for (int x = xs; x <= xe; ++x) {
        const float nx = (x - s.x) / s.r, d = sqrtf(nx * nx + ny * ny);
        const float l = 0.55f - 0.35f * ny - 0.2f * nx + ((hash3(x, y, 9) & 255) / 255.0f - 0.5f) * 0.15f;
        const float rim = d > 0.9f ? (d - 0.9f) * 10 : 0;
        row[x] = outPix(rgb565((int)(60 * l + 30 + rim * 10), (int)(60 * l + 32 + rim * 30), (int)(58 * l + 30)));
      }
    }
  }
}

static constexpr size_t nmOa34 = nmAlignTo(nmOv34, 16), nmOv35 = nmOa34 + sizeof(Scratch[2]); static Scratch (&scr)[2] = *reinterpret_cast<Scratch(*)[2]>(NM_ARENA + nmOa34);

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
static_assert(std::is_trivially_default_constructible<Stone>::value, "NM_ARENA: Stone");
static_assert(std::is_trivially_default_constructible<bool>::value, "NM_ARENA: bool");
static_assert(std::is_trivially_default_constructible<float>::value, "NM_ARENA: float");
static_assert(std::is_trivially_default_constructible<int>::value, "NM_ARENA: int");
static_assert(std::is_trivially_default_constructible<uint32_t>::value, "NM_ARENA: uint32_t");
static_assert(std::is_trivially_default_constructible<uint8_t>::value, "NM_ARENA: uint8_t");
static constexpr size_t OVL_END = nmAlignTo(nmOv35, 16);
static_assert(OVL_END <= NM_ARENA_SIZE, "NM_ARENA_SIZE (nm_common.h) を大きくしてください");
extern const size_t OVL_SIZE = OVL_END;   // 起動時の確認用
}  // namespace NullZen
