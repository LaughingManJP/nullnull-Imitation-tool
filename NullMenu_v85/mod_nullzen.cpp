// NullZen  -  NullMenu 用モジュール(元のスケッチ NullZen_v9 を名前空間に入れたもの)
// 0 で始まる変数(配列も)は、ほかの作品と共通の置き場(NM_ARENA)に置いています。
// 一度に動くのは1つだけ(切り替えるときは再起動)なので、重なっていても問題ありません。
#include "nm_common.h"
#include <algorithm>
#pragma GCC optimize("O3")

namespace NullZen {
static constexpr size_t nmOv0 = 0;

// NullZen v9  -  計算機の枯山水。月夜の砂 / 墨の庭 / 光る砂 の3つの見え方
//   null²(大阪・関西万博のパビリオン)の静かな空間から受けたイメージで作った、非公式の習作です。作品の映像・名前・データは使っていません。
//   M5Stack StopWatch 用(丸い画面)。CoreS3 系でも画面いっぱいに動きます(自動で切り替わります)。
//
// 画面の中(真上から見た枯山水。石のまわりは同心円の輪、その外はまっすぐの砂紋):
//   ・月夜の砂 … 低い月の光が斜めに差し、砂紋の山と谷に陰ができて、本物の溝のように見える。
//                石は月と反対の向きに影を落とし、砂粒がところどころきらめく。月の光はゆっくり向きを変える
//   ・墨の庭   … 白い紙に墨で描いた枯山水。砂紋は筆の線(かすれ入り)、石は墨のにじみ
//   ・光る砂   … 暗い中で、砂紋の山だけが光の線になる。石は鏡の球で、まわりの光の線が丸くゆがんで映る。
//                光る砂粒がゆっくり漂う
//
// 触ると:
//   ・砂をなぞる   : 熊手の5本の溝が残る(しばらくすると、風でゆっくり元の砂紋に戻る)
//   ・石をドラッグ : 石を動かせる。離すと、熊手が左から右へ横切って砂紋を引き直す
//   ・長押し       : そこに石を置く(最大 5 つ。古いものから消える)
//   ・タップ       : 月夜は光の輪のきらめき / 墨は一滴の墨が落ちて輪が広がる / 光る砂は光の輪が広がる
//
// 月夜の月の光は、ゆっくり自動で向きを変えます(月が空を渡るように)。光る砂の砂粒は、ふわふわ漂います。
// (傾きセンサーは使いません。使いたいときは、調整用の TILT を true に)
//
// 操作:
//   StopWatch … ボタンA 短押し: 見え方(月夜 / 墨 / 光)   ボタンA 長押し: 砂紋の向き(横 / ななめ / 波)を変えて引き直す
//               ボタンB: 明るさ
//   CoreS3系  … 画面の上半分をダブルタップ: 見え方   下半分をダブルタップ: 砂紋の向き   電源ボタン短押し: 明るさ
//
// 必要なもの:
//   ボードマネージャー M5Stack >= 3.3.7 / ボード: M5StopWatch(PSRAM は OPI)、CoreS3 なら M5CoreS3
//   ライブラリ M5Unified >= 0.2.16, M5GFX >= 0.2.21
//
// しくみ:
//   砂は「高さの地図」(1点 1バイト、PSRAM)で持ちます。熊手で引いた砂紋(base)と、今の砂(hmap)の2枚。
//   砂紋の位相は「石からの距離(なめらかな最小値)」と「まっすぐの線」を、輪の本数の所でなめらかにつないで決めます。
//   毎コマ、見え方ごとに高さの地図を描き分けます(月夜: となりとの高さの差 × 月の向き → 色の表 / 墨: 高さ → 墨の濃さ / 光: 高さ → 光)。
//   砂粒やかすれの模様は、1点ごとに乱数を作らず、128×128 の模様の表(内蔵メモリ)から引きます。
//   陰をつけた砂の絵は1枚取っておき(shadeImg)、毎コマはそれを写すだけ。指の溝・熊手・風で変わった所だけ 16×16 のます目ごとに描き直し、
//   月の光の向きが変わったぶんは、毎コマ数行ずつ描き直します。きらめき・タップの輪・石・砂粒は、その上に重ねて描きます。
//   指の溝は hmap に彫り、風は hmap を少しずつ base に戻します。
//   2つのコアで帯を描き、StopWatch では帯ごとに丸の中だけを画面へ送ります。


#ifndef IRAM_ATTR
#define IRAM_ATTR
#endif

// Arduino IDE は関数の宣言を自動でこのあたりに足すので、型の名前だけ先に宣言しておく
struct Col; struct Input; struct Stone; struct Scratch; struct Drop; struct Grain; struct GDraw;

// ---- 調整用 ----------------------------------------------------------------
static constexpr float SPACING   = 12.0f;   // 砂紋の間隔(466 の画面でのピクセル)
static constexpr int   RINGS     = 4;       // 石のまわりの輪の数
static constexpr float RAKE_SPD  = 260.0f;  // 熊手の速さ(ピクセル/秒)
static constexpr int   MAXSTONE  = 5;       // 石の最大の数
static constexpr float SLIDE     = 1.0f;    // 傾けたとき、砂が流れる速さ(0 で流れない)
static constexpr float WIND      = 1.0f;    // 指の溝が風で元に戻る速さ(0 で戻らない)
static constexpr int   MODE_START = 0;      // 最初の見え方(0: 月夜 / 1: 墨 / 2: 光る砂)
static constexpr bool  DOUBLE_TAP_UI = true;
// ---------------------------------------------------------------------------

// ---- 傾き(本体の傾きセンサー)-----------------------------------------------
static constexpr bool  TILT        = false;   // 傾きで動かすか(NullZen v8 からは使わない。true にすると、砂が低い方へ少しずつ崩れる)
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
static inline float sqf(float x) { return x * x; }
struct Scratch { float* sh; };              // (帯ごとの作業場所)

static const char* SKETCH_NAME = "NullZen v9";

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
// 砂は「高さの地図」(0..255)で持ちます。見え方ごとに、同じ高さの地図を違う描き方で描きます
//   月夜: 高さの傾きに月の光を当てて陰をつける / 墨: 溝を筆の線にする / 光: 砂紋の山だけを光らせる
static uint8_t* base;                  // [S*S] 熊手で引いた砂紋(石の配置から計算)
static uint8_t* hmap;                  // [S*S] 今の砂(指でなぞった溝や、傾けて崩れた所を含む)
static uint8_t* grainTex;              // [128*128] 砂粒・かすれの模様(内蔵メモリ。1点ごとに乱数を作る代わりに引く)
static constexpr size_t nmOa24 = nmAlignTo(nmOv24, 16), nmOv25 = nmOa24 + sizeof(uint8_t[1024]); static uint8_t (&vigTab)[1024] = *reinterpret_cast<uint8_t(*)[1024]>(NM_ARENA + nmOa24);  // 中心からの距離の2乗 → まわりの暗さ(0..15)
static int vigShift = 7;
static constexpr size_t nmOa25 = nmAlignTo(nmOv25, 16), nmOv26 = nmOa25 + sizeof(uint8_t[256]); static uint8_t (&cosTab)[256] = *reinterpret_cast<uint8_t(*)[256]>(NM_ARENA + nmOa25);  // 砂紋の形(cos の表)
static uint16_t* shadeImg;             // [S*S] 陰をつけた砂の絵(画面の色。PSRAM)。毎コマはこれを写すだけ
static constexpr int TS = 16;          // 描き直しの単位(ます目の大きさ)
static constexpr size_t nmOa26 = nmAlignTo(nmOv26, alignof(int)), nmOv27 = nmOa26 + sizeof(int); static int& NT = *reinterpret_cast<int*>(NM_ARENA + nmOa26);  // ます目の数(1辺)
static uint8_t* dirty;                 // [NT*NT] 描き直しが要るます目
static constexpr size_t nmOa27 = nmAlignTo(nmOv27, alignof(int)), nmOv28 = nmOa27 + sizeof(int); static int& rollY = *reinterpret_cast<int*>(NM_ARENA + nmOa27);  // 月の光の向きが変わったぶん、少しずつ描き直していく行
static int16_t* glintXY;               // 砂粒のきらめきの場所(y の順)
static constexpr size_t nmOa28 = nmAlignTo(nmOv28, 16), nmOv29 = nmOa28 + sizeof(int16_t[MAXNB + 1]); static int16_t (&glStart)[MAXNB + 1] = *reinterpret_cast<int16_t(*)[MAXNB + 1]>(NM_ARENA + nmOa28);
static constexpr size_t nmOa29 = nmAlignTo(nmOv29, alignof(int)), nmOv30 = nmOa29 + sizeof(int); static int& nGlint = *reinterpret_cast<int*>(NM_ARENA + nmOa29);
static constexpr size_t nmOa30 = nmAlignTo(nmOv30, 16), nmOv31 = nmOa30 + sizeof(Stone[MAXSTONE]); static Stone (&stones)[MAXSTONE] = *reinterpret_cast<Stone(*)[MAXSTONE]>(NM_ARENA + nmOa30);
static constexpr size_t nmOa31 = nmAlignTo(nmOv31, 16), nmOv32 = nmOa31 + sizeof(Stone[MAXSTONE]); static Stone (&bake)[MAXSTONE] = *reinterpret_cast<Stone(*)[MAXSTONE]>(NM_ARENA + nmOa31);  // 今の石 / 砂紋を引いたときの石
static int grabbed = -1, mode = MODE_START;
static constexpr size_t nmOa32 = nmAlignTo(nmOv32, alignof(int)), nmOv33 = nmOa32 + sizeof(int); static int& nStone = *reinterpret_cast<int*>(NM_ARENA + nmOa32);
static constexpr size_t nmOa33 = nmAlignTo(nmOv33, alignof(int)), nmOv34 = nmOa33 + sizeof(int); static int& nBake = *reinterpret_cast<int*>(NM_ARENA + nmOa33);
static constexpr size_t nmOa34 = nmAlignTo(nmOv34, alignof(int)), nmOv35 = nmOa34 + sizeof(int); static int& style = *reinterpret_cast<int*>(NM_ARENA + nmOa34);
static float sweep = 1e9f;  // 熊手の位置(S より大きいと、止まっている)
static constexpr size_t nmOa35 = nmAlignTo(nmOv35, alignof(float)), nmOv36 = nmOa35 + sizeof(float); static float& SP = *reinterpret_cast<float*>(NM_ARENA + nmOa35);
static constexpr size_t nmOa36 = nmAlignTo(nmOv36, alignof(bool)), nmOv37 = nmOa36 + sizeof(bool); static bool& wasTouch = *reinterpret_cast<bool*>(NM_ARENA + nmOa36);
static constexpr size_t nmOa37 = nmAlignTo(nmOv37, alignof(float)), nmOv38 = nmOa37 + sizeof(float); static float& slideAcc = *reinterpret_cast<float*>(NM_ARENA + nmOa37);
static constexpr size_t nmOa38 = nmAlignTo(nmOv38, alignof(float)), nmOv39 = nmOa38 + sizeof(float); static float& windAcc = *reinterpret_cast<float*>(NM_ARENA + nmOa38);
static float lastRX = -1, lastRY = -1;  // 指の熊手の、前の位置
static constexpr size_t nmOa39 = nmAlignTo(nmOv39, alignof(float)), nmOv40 = nmOa39 + sizeof(float); static float& pressT0 = *reinterpret_cast<float*>(NM_ARENA + nmOa39);
static constexpr size_t nmOa40 = nmAlignTo(nmOv40, alignof(float)), nmOv41 = nmOa40 + sizeof(float); static float& pressX0 = *reinterpret_cast<float*>(NM_ARENA + nmOa40);
static constexpr size_t nmOa41 = nmAlignTo(nmOv41, alignof(float)), nmOv42 = nmOa41 + sizeof(float); static float& pressY0 = *reinterpret_cast<float*>(NM_ARENA + nmOa41);
static constexpr size_t nmOa42 = nmAlignTo(nmOv42, alignof(float)), nmOv43 = nmOa42 + sizeof(float); static float& pressMove = *reinterpret_cast<float*>(NM_ARENA + nmOa42);
static constexpr size_t nmOa43 = nmAlignTo(nmOv43, alignof(bool)), nmOv44 = nmOa43 + sizeof(bool); static bool& pressOn = *reinterpret_cast<bool*>(NM_ARENA + nmOa43);
static constexpr size_t nmOa44 = nmAlignTo(nmOv44, alignof(bool)), nmOv45 = nmOa44 + sizeof(bool); static bool& pressLong = *reinterpret_cast<bool*>(NM_ARENA + nmOa44);
static float litX = -0.55f, litY = -0.62f;  // 月の光の向き(画面の上で)
static constexpr size_t nmOa45 = nmAlignTo(nmOv45, 16), nmOv46 = nmOa45 + sizeof(uint16_t[16][64]); static uint16_t (&lut)[16][64] = *reinterpret_cast<uint16_t(*)[16][64]>(NM_ARENA + nmOa45);  // [まわりの暗さ][明るさ] → 色(見え方ごと)
static constexpr size_t nmOa46 = nmAlignTo(nmOv46, 16), nmOv47 = nmOa46 + sizeof(uint16_t[64]); static uint16_t (&glowLut)[64] = *reinterpret_cast<uint16_t(*)[64]>(NM_ARENA + nmOa46);  // 光の砂: 高さ → 光
static constexpr size_t nmOa47 = nmAlignTo(nmOv47, 16), nmOv48 = nmOa47 + sizeof(uint16_t[64]); static uint16_t (&stoneLut)[64] = *reinterpret_cast<uint16_t(*)[64]>(NM_ARENA + nmOa47);  // 石の明るさ → 色
static constexpr size_t nmOa48 = nmAlignTo(nmOv48, 16), nmOv49 = nmOa48 + sizeof(uint16_t[10]); static uint16_t (&glint)[10] = *reinterpret_cast<uint16_t(*)[10]>(NM_ARENA + nmOa48);  // 砂粒のきらめき(またたきの10コマ)
struct Drop { float x, y, t0, k; };                      // 墨の一滴 / 光の輪 / きらめき
static constexpr int NDROP = 5;
static constexpr size_t nmOa49 = nmAlignTo(nmOv49, 16), nmOv50 = nmOa49 + sizeof(Drop[NDROP]); static Drop (&drop)[NDROP] = *reinterpret_cast<Drop(*)[NDROP]>(NM_ARENA + nmOa49);
static constexpr size_t nmOa50 = nmAlignTo(nmOv50, alignof(int)), nmOv51 = nmOa50 + sizeof(int); static int& dropNext = *reinterpret_cast<int*>(NM_ARENA + nmOa50);
struct Grain { float x, y, vx, vy, tw; };                // 光る砂粒
static constexpr int NGRAIN = 260;
static constexpr size_t nmOa51 = nmAlignTo(nmOv51, 16), nmOv52 = nmOa51 + sizeof(Grain[NGRAIN]); static Grain (&grain)[NGRAIN] = *reinterpret_cast<Grain(*)[NGRAIN]>(NM_ARENA + nmOa51);
struct GDraw { int16_t x, y; uint16_t c; };
static constexpr size_t nmOa52 = nmAlignTo(nmOv52, 16), nmOv53 = nmOa52 + sizeof(GDraw[NGRAIN]); static GDraw (&gdraw)[NGRAIN] = *reinterpret_cast<GDraw(*)[NGRAIN]>(NM_ARENA + nmOa52);
static constexpr size_t nmOa53 = nmAlignTo(nmOv53, 16), nmOv54 = nmOa53 + sizeof(int16_t[NGRAIN]); static int16_t (&gOrd)[NGRAIN] = *reinterpret_cast<int16_t(*)[NGRAIN]>(NM_ARENA + nmOa53);
static constexpr size_t nmOa54 = nmAlignTo(nmOv54, 16), nmOv55 = nmOa54 + sizeof(int16_t[MAXNB + 1]); static int16_t (&gStart)[MAXNB + 1] = *reinterpret_cast<int16_t(*)[MAXNB + 1]>(NM_ARENA + nmOa54);
static constexpr size_t nmOa55 = nmAlignTo(nmOv55, alignof(int)), nmOv56 = nmOa55 + sizeof(int); static int& nG = *reinterpret_cast<int*>(NM_ARENA + nmOa55);
static constexpr size_t nmOa56 = nmAlignTo(nmOv56, alignof(uint32_t)), nmOv57 = nmOa56 + sizeof(uint32_t); static uint32_t& gFrame = *reinterpret_cast<uint32_t*>(NM_ARENA + nmOa56);

static bool allocScratch(Scratch& s) { s.sh = (float*)malloc((S + 8) * 4); return s.sh != nullptr; }
static bool pieceAlloc() {
  base = (uint8_t*)ps_malloc((size_t)S * S);
  hmap = (uint8_t*)ps_malloc((size_t)S * S);
  grainTex = (uint8_t*)allocFast(128 * 128);
  shadeImg = (uint16_t*)ps_malloc((size_t)S * S * 2);
  NT = (S + TS - 1) / TS;
  dirty = (uint8_t*)malloc(NT * NT);
  glintXY = (int16_t*)malloc(2 * 2 * 1200);
  return base && hmap && grainTex && shadeImg && dirty && glintXY;
}

static inline float smooth01(float a, float b, float x) { float t = (x - a) / (b - a); t = t < 0 ? 0 : t > 1 ? 1 : t; return t * t * (3 - 2 * t); }

// 熊手で引いた砂の高さ(石のまわりは同心の輪、その外はまっすぐ / ななめ / 波の筋)
static uint8_t baseAt(int x, int y) {
  float best = 1e9f, d = 1e9f;
  const float kk = 28.0f * S / 466.0f;                       // 石どうしの輪を、なめらかにつなぐ幅
  for (int i = 0; i < nBake; ++i) {
    const Stone& s = bake[i];
    const float dx = x - s.x, dy = (y - s.y) / s.sq, e = sqrtf(dx * dx + dy * dy) - s.r;
    best = fminf(best, e);
    const float h = fmaxf(kk - fabsf(d - e), 0.0f) / kk;     // なめらかな最小値(多項式)
    d = fminf(d, e) - h * h * kk * 0.25f;
  }
  if (best < -1.0f) return 150;                              // 石の下は、平らな砂
  float lin;
  if (style == 0) lin = y + 7 * ((cosTab[(int)(x * 0.011f * 466.0f / S * 40.743f + 192) & 255] - 128) / 127.0f);
  else if (style == 1) lin = (x + y) * 0.7071f;
  else lin = y + 16 * ((cosTab[(int)(x * 0.022f * 466.0f / S * 40.743f + 192) & 255] - 128) / 127.0f);
  const float w = smooth01(RINGS * SP - SP * 0.5f, RINGS * SP + SP * 1.5f, d);
  const float p = (d / SP) * (1 - w) + (lin / SP) * w;
  const float f = p - floorf(p);
  const float amp = 1 - 0.85f * 4 * w * (1 - w);             // 輪とまっすぐの境目は、溝をうすく
  const float g = (grainTex[((y & 127) << 7) | (x & 127)] - 128) * (10.0f / 256.0f);
  const float v = 140.0f + 70.0f * ((cosTab[(int)(f * 256.0f) & 255] - 128) / 127.0f) * amp + g;
  return (uint8_t)(v < 0 ? 0 : v > 255 ? 255 : v);
}
// 描き直しが要る所に印をつける(となりの点も使うので、1点ずつ広げる)
static void markDirty(int x0, int y0, int x1, int y1) {
  const int a = max(0, (x0 - 1) / TS), b = min(NT - 1, (x1 + 1) / TS), c = max(0, (y0 - 1) / TS), d = min(NT - 1, (y1 + 1) / TS);
  for (int ty = c; ty <= d; ++ty) for (int tx = a; tx <= b; ++tx) dirty[ty * NT + tx] = 1;
}
// 四角の範囲の砂に、見え方ごとの陰をつけて shadeImg に書く
static void shadeRect(int x0, int y0, int x1, int y1) {
  const int lx = (int)(litX * 256), ly = (int)(litY * 256);
  for (int y = max(0, y0); y <= min(S - 1, y1); ++y) {
    if (spanS[y] > spanE[y]) continue;
    const int xs = max(x0, (int)spanS[y]), xe = min(x1, (int)spanE[y]);
    const int yu = max(0, y - 1), yd = min(S - 1, y + 1);
    const uint8_t* hu = hmap + (size_t)yu * S, *hc = hmap + (size_t)y * S, *hd = hmap + (size_t)yd * S;
    const int dy = (int)(y - Sc), dy2 = dy * dy;
    const uint8_t* gt = grainTex + ((y & 127) << 7);
    uint16_t* out = shadeImg + (size_t)y * S;
    for (int x = xs; x <= xe; ++x) {
      const int xl = max(0, x - 1), xr = min(S - 1, x + 1);
      const int dx = (int)(x - Sc);
      const int v = vigTab[min(1023, (dx * dx + dy2) >> vigShift)];
      uint16_t c;
      if (mode == 0) {                                           // 月夜: 高さの傾き × 月の光の向き
        const int gx = hc[xr] - hc[xl], gy = hd[x] - hu[x];
        int b = 32 - ((gx * lx + gy * ly) >> 9);
        b = b < 0 ? 0 : b > 63 ? 63 : b;
        c = lut[v][b];
      } else if (mode == 1) {                                    // 墨: 溝の深い所が墨の線(かすれ入り)
        int a = (112 - hc[x]) * 3;
        a = a < 0 ? 0 : a;
        a = (a * (40 + (gt[(x >> 1) & 127] & 63))) >> 7;
        c = lut[v][a > 63 ? 63 : a];
      } else {                                                   // 光: 砂紋の山だけが光の線になる
        c = add565(lut[v][0], glowLut[hc[x] >> 2]);
      }
      out[x] = outPix(c);
    }
  }
}
static void shadeDirty() {
  for (int ty = 0; ty < NT; ++ty)
    for (int tx = 0; tx < NT; ++tx)
      if (dirty[ty * NT + tx]) { dirty[ty * NT + tx] = 0; shadeRect(tx * TS, ty * TS, tx * TS + TS - 1, ty * TS + TS - 1); }
}
static void markAll() { memset(dirty, 1, NT * NT); }

static void bakeColumns(int x0, int x1) {
  for (int y = 0; y < S; ++y) {
    if (spanS[y] > spanE[y]) continue;
    const int a = max(x0, (int)spanS[y] - 1), b = min(x1, (int)spanE[y] + 2);
    for (int x = max(0, a); x < min(S, b); ++x) { const uint8_t v = baseAt(x, y); base[(size_t)y * S + x] = v; hmap[(size_t)y * S + x] = v; }
  }
  markDirty(x0, 0, x1, S - 1);
}
static void startRake() { for (int i = 0; i < nStone; ++i) bake[i] = stones[i]; nBake = nStone; sweep = 0; }

// 見え方ごとの色の表
static void buildLuts() {
  for (int v = 0; v < 16; ++v)
    for (int i = 0; i < 64; ++i) {
      const float b = i / 63.0f, vv = 0.45f + 0.55f * v / 15.0f;
      Col c;
      if (mode == 0) {                                           // 月夜: 青白い砂。明るい面は白く、陰は紺
        const Col dk = C8(26, 30, 48), lt = C8(214, 222, 240);
        c = lerpC(dk, lt, powf(b, 1.25f));
        if (b > 0.82f) c = lerpC(c, C8(255, 255, 255), (b - 0.82f) * 3.0f);
        c = mulC(c, vv);
      } else if (mode == 1) {                                     // 墨: 紙の色 → 墨の色(b は墨の濃さ)
        const Col paper = C8(236, 231, 219), ink = C8(22, 22, 26);
        c = lerpC(paper, ink, powf(b, 0.9f));
        c = mulC(c, 0.86f + 0.14f * vv);
      } else {                                                    // 光: 暗い空
        c = mulC(C8(4, 6, 14), vv);
      }
      lut[v][i] = to565(c);
    }
  for (int i = 0; i < 64; ++i) {
    const float b = i / 63.0f;
    const float e = powf(b, 9.0f) * 1.2f + powf(b, 4.0f) * 0.12f;   // 山の線 + うすいにじみ
    glowLut[i] = to565({0.25f * e + 0.6f * powf(b, 9.0f), 0.75f * e, 1.0f * e});
    if (i < 10) glint[i] = to565(mulC(C8(255, 255, 255), 0.9f * sinf(3.1416f * (i + 0.5f) / 10)));
    stoneLut[i] = mode == 0 ? to565(lerpC(C8(18, 20, 26), C8(150, 152, 160), b)) : to565(lerpC(C8(40, 40, 46), C8(10, 10, 12), b));
  }
}
static void buildVig() {
  for (int i = 0; i < 256; ++i) cosTab[i] = (uint8_t)lrintf(128 + 127 * cosf(i * 6.2832f / 256));
  for (int i = 0; i < 128 * 128; ++i) grainTex[i] = (uint8_t)(hash3(i & 127, i >> 7, 5) & 255);
  const float d2max = 2.0f * S * S * 0.25f + 1;
  vigShift = 0; while ((d2max / (1 << vigShift)) > 1023) ++vigShift;
  for (int i = 0; i < 1024; ++i) {
    const float r2 = (float)(i << vigShift) / (VR * VR);
    vigTab[i] = (uint8_t)(clamp01(1.0f - 0.35f * r2) * 15.0f + 0.5f);
  }
}

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
  memset(base, 140, (size_t)S * S); memset(hmap, 140, (size_t)S * S);
  buildVig(); buildLuts();
  bakeColumns(0, S);                                       // 起動時に一度、全部引いておく
  nGlint = 0;                                              // 砂粒のきらめきの場所(y の順に並べる)
  for (int y = 0; y < S && nGlint < 1200; ++y)
    for (int x = max(0, (int)spanS[y]); x <= min(S - 1, (int)spanE[y]) && nGlint < 1200; ++x)
      if (grainTex[((y & 127) << 7) | (x & 127)] == 255 && ((x >> 7) + (y >> 7)) % 3 != 1 && (x - Sc) * (x - Sc) + (y - Sc) * (y - Sc) < (VR - 3) * (VR - 3)) {
        glintXY[nGlint * 2] = (int16_t)x; glintXY[nGlint * 2 + 1] = (int16_t)y; ++nGlint; }
  for (int b = 0; b <= NB; ++b) glStart[b] = 0;
  for (int i = 0; i < nGlint; ++i) glStart[glintXY[i * 2 + 1] / BAND + 1]++;
  for (int b = 0; b < NB; ++b) glStart[b + 1] += glStart[b];
  shadeRect(0, 0, S - 1, S - 1); memset(dirty, 0, NT * NT);
  for (int i = 0; i < NDROP; ++i) drop[i].t0 = -100;
  for (int i = 0; i < NGRAIN; ++i) { const float a = frand(0, 6.2832f), r = sqrtf(frand()) * VR * 0.95f; grain[i] = {Sc + cosf(a) * r, Sc + sinf(a) * r, 0, 0, frand(0, 6.2832f)}; }
}
static const char* const MODE_NAME[3] = {"moon", "ink", "glow"};
static void onA() { mode = (mode + 1) % 3; buildLuts(); shadeRect(0, 0, S - 1, S - 1); memset(dirty, 0, NT * NT); for (int i = 0; i < NDROP; ++i) drop[i].t0 = -100; Serial.printf("mode: %s\n", MODE_NAME[mode]); }
static void onAHold() { style = (style + 1) % 3; startRake(); Serial.printf("style: %d\n", style); }
static void statusText(char* o, int n) { snprintf(o, n, "%s  stones %d %s", MODE_NAME[mode], nStone, sweep <= S ? "raking" : ""); }

// 指の熊手: 動いた向きに直角に5本の溝を彫る
static void rakeStamp(float x, float y, float nx, float ny) {
  const float k = S / 466.0f, gap = 7.0f * k, rr = 1.7f * k;
  const int R = (int)ceilf(rr + 1);
  { const float ext = fabsf(nx) * gap * 2 + R + 1, eyt = fabsf(ny) * gap * 2 + R + 1; markDirty((int)(x - ext), (int)(y - eyt), (int)(x + ext), (int)(y + eyt)); }
  for (int t = -2; t <= 2; ++t) {
    const float cx = x + nx * gap * t, cy = y + ny * gap * t;
    for (int yy = (int)cy - R; yy <= (int)cy + R; ++yy) {
      if (yy < 1 || yy >= S - 1) continue;
      for (int xx = (int)cx - R; xx <= (int)cx + R; ++xx) {
        if (xx < 1 || xx >= S - 1) continue;
        const float d = hypotf(xx - cx, yy - cy);
        if (d > rr + 0.8f) continue;
        bool inStone = false;
        for (int i = 0; i < nStone; ++i) { const float dx = xx - stones[i].x, dy = (yy - stones[i].y) / stones[i].sq; if (dx * dx + dy * dy < stones[i].r * stones[i].r) inStone = true; }
        if (inStone) continue;
        const float a = clamp01((rr + 0.8f - d) / 1.6f);
        uint8_t& p = hmap[(size_t)yy * S + xx];
        const float target = 66.0f;
        p = (uint8_t)(p + (target - p) * a);
      }
    }
  }
}
static void addDrop(float x, float y, float k) { drop[dropNext] = {x, y, simT, k}; dropNext = (dropNext + 1) % NDROP; }

static void updateFrame(const Input& in) {
  float dt = in.dt > 0.05f ? 0.05f : in.dt;
  if (dt < 0.0005f) dt = 0.0005f;
  simT += dt; ++gFrame;
  const float k = S / 466.0f;
  // 指: 石をつかんで動かす / 砂をなぞると熊手の溝 / 短いタップは見え方ごとの仕掛け / 長押しで石を置く
  if (in.touching) {
    if (!pressOn) {
      pressOn = true; pressT0 = simT; pressX0 = in.tx; pressY0 = in.ty; pressMove = 0; pressLong = false; lastRX = in.tx; lastRY = in.ty;
      grabbed = -1;
      for (int i = 0; i < nStone; ++i) { const float dx = in.tx - stones[i].x, dy = (in.ty - stones[i].y) / stones[i].sq; if (sqrtf(dx * dx + dy * dy) < stones[i].r + 6 * k) grabbed = i; }
    }
    pressMove = fmaxf(pressMove, hypotf(in.tx - pressX0, in.ty - pressY0));
    if (grabbed >= 0) { stones[grabbed].x = in.tx; stones[grabbed].y = in.ty; }
    else {
      const float dx = in.tx - lastRX, dy = in.ty - lastRY, L = hypotf(dx, dy);
      if (pressMove > 6 * k && L > 0.5f) {                     // なぞった道すじに、すき間なく溝を彫る
        const int n = (int)(L / (1.5f * k)) + 1;
        for (int i = 1; i <= n; ++i) rakeStamp(lastRX + dx * i / n, lastRY + dy * i / n, -dy / L, dx / L);
        lastRX = in.tx; lastRY = in.ty;
      }
      if (!pressLong && pressMove < 6 * k && simT - pressT0 > 0.7f) {   // 長押し: 石を置く
        pressLong = true;
        if ((in.tx - Sc) * (in.tx - Sc) + (in.ty - Sc) * (in.ty - Sc) < (VR - 30 * k) * (VR - 30 * k)) {
          if (nStone == MAXSTONE) { for (int i = 1; i < MAXSTONE; ++i) stones[i - 1] = stones[i]; --nStone; }
          stones[nStone++] = mkStone(in.tx, in.ty, (14 + frand() * 20) * k);
          startRake();
        }
      }
    }
  } else if (pressOn) {
    pressOn = false;
    if (grabbed >= 0) { grabbed = -1; startRake(); }
    else if (!pressLong && pressMove < 6 * k && simT - pressT0 < 0.4f) addDrop(pressX0, pressY0, 1.0f);   // 短いタップ
  }
  wasTouch = in.touching;
  // 熊手: 左から右へ横切りながら、通った列だけ新しい砂紋を引く
  if (sweep <= S) {
    const float nx = fminf((float)S + 1, sweep + dt * RAKE_SPD * k);
    bakeColumns((int)sweep, min(S, (int)nx));
    sweep = nx;
    if (sweep > S) sweep = 1e9f;
  }
  // 風: 指の溝や崩れた所が、ゆっくり元の砂紋に戻る
  windAcc += dt * WIND * 60.0f;
  const int rr = max(5, (int)(9 * k));
  while (windAcc > 1) {
    windAcc -= 1;
    const float a = frand(0, 6.2832f), r = sqrtf(frand()) * (VR - 4);
    const int cx = (int)(Sc + cosf(a) * r), cy = (int)(Sc + sinf(a) * r);
    bool any = false;
    for (int y = max(1, cy - rr); y <= min(S - 2, cy + rr) && !any; ++y)        // 元の砂紋と違う所がなければ、描き直さない
      for (int x = max(1, cx - rr); x <= min(S - 2, cx + rr); ++x) if (hmap[(size_t)y * S + x] != base[(size_t)y * S + x]) { any = true; break; }
    if (!any) continue;
    markDirty(cx - rr, cy - rr, cx + rr, cy + rr);
    for (int y = max(1, cy - rr); y <= min(S - 2, cy + rr); ++y)
      for (int x = max(1, cx - rr); x <= min(S - 2, cx + rr); ++x) {
        if ((x - cx) * (x - cx) + (y - cy) * (y - cy) > rr * rr) continue;
        uint8_t& p = hmap[(size_t)y * S + x];
        const int b = base[(size_t)y * S + x];
        p = (uint8_t)(p + (b - p) / 6);
      }
  }
  // 傾き: 月夜は月の光の向きが変わる。光の砂は砂粒が流れる。どれも、砂が少しずつ低い方へ崩れる
  const float tm = sqrtf(in.tiltX * in.tiltX + in.tiltY * in.tiltY);
  {                                                            // 月の光の向きは、ゆっくり自動で動く(月が空を渡るように)
    const float a = -2.30f + 0.35f * sinf(simT * 0.045f), m = 0.80f + 0.12f * sinf(simT * 0.031f);
    litX = cosf(a) * m; litY = sinf(a) * m;
  }
  if (mode == 0) {                                             // 月の光の向きが変わったぶん、毎コマ少しずつ(数秒で全体)描き直す
    const int rows = max(2, S / 120);
    shadeRect(0, rollY, S - 1, rollY + rows - 1);
    rollY += rows; if (rollY >= S) rollY = 0;
  }
  if (tm > 0.2f && sweep > S && SLIDE > 0) {
    slideAcc += dt * 300 * (tm - 0.2f) * SLIDE;
    const int ox = (int)roundf(-in.tiltX / tm * 2), oy = (int)roundf(-in.tiltY / tm * 2);
    const int sr = max(4, (int)(7 * k));
    while (slideAcc > 1) {
      slideAcc -= 1;
      const float a = frand(0, 6.2832f), r = sqrtf(frand()) * (VR - 12);
      const int cx = (int)(Sc + cosf(a) * r), cy = (int)(Sc + sinf(a) * r);
      for (int y = max(3, cy - sr); y <= min(S - 4, cy + sr); ++y)
        for (int x = max(3, cx - sr); x <= min(S - 4, cx + sr); ++x) {
          if ((x - cx) * (x - cx) + (y - cy) * (y - cy) > sr * sr) continue;
          uint8_t& p = hmap[(size_t)y * S + x];
          p = (uint8_t)((p * 5 + hmap[(size_t)(y + oy) * S + (x + ox)] * 11) >> 4);
        }
    }
  } else slideAcc = 0;
  shadeDirty();                                                // 指の溝・熊手・風で変わった所だけ描き直す
  // 光る砂粒(光の砂のときだけ): ふわふわ漂う
  nG = 0;
  if (mode == 2) {
    const float damp = expf(-dt * 1.5f);
    for (int i = 0; i < NGRAIN; ++i) {
      Grain& g = grain[i];
      g.vx = g.vx * damp + (sinf(simT * 0.3f + i) * 10 * k + sinf(g.y * 0.02f + simT * 0.2f) * 6 * k) * dt;   // ふわふわ漂う
      g.vy = g.vy * damp + (cosf(simT * 0.27f + i * 1.7f) * 10 * k + cosf(g.x * 0.02f - simT * 0.17f) * 6 * k) * dt;
      g.x += g.vx * dt; g.y += g.vy * dt;
      const float dx = g.x - Sc, dy = g.y - Sc, r = hypotf(dx, dy);
      if (r > VR - 3) { g.x = Sc + dx / r * (VR - 3); g.y = Sc + dy / r * (VR - 3); g.vx *= -0.3f; g.vy *= -0.3f; }
      for (int s = 0; s < nStone; ++s) {                         // 石の球にはぶつかって、よける
        const float sx = g.x - stones[s].x, sy = g.y - stones[s].y, d = hypotf(sx, sy), rs = stones[s].r + 2;
        if (d < rs && d > 0.01f) { g.x = stones[s].x + sx / d * rs; g.y = stones[s].y + sy / d * rs; }
      }
      const int xi = (int)g.x, yi = (int)g.y;
      if (xi < 1 || yi < 1 || xi >= S - 1 || yi >= S - 1) continue;
      const float tw = 0.55f + 0.45f * sinf(simT * 3.0f + g.tw);
      gdraw[nG++] = {(int16_t)xi, (int16_t)yi, to565(mulC(C8(190, 230, 255), tw))};
    }
  }
  for (int b = 0; b <= NB; ++b) gStart[b] = 0;
  for (int i = 0; i < nG; ++i) gStart[gdraw[i].y / BAND + 1]++;
  for (int b = 0; b < NB; ++b) gStart[b + 1] += gStart[b];
  int16_t fill[MAXNB];
  for (int b = 0; b < NB; ++b) fill[b] = gStart[b];
  for (int i = 0; i < nG; ++i) gOrd[fill[gdraw[i].y / BAND]++] = (int16_t)i;
}

// 波紋(墨の輪 / 光の輪 / きらめき): その点の強さ(0..1)
static inline float dropAt(int x, int y) {
  float s = 0;
  for (int i = 0; i < NDROP; ++i) {
    const float age = simT - drop[i].t0;
    if (age < 0 || age > 3.0f) continue;
    const float d = hypotf(x - drop[i].x, y - drop[i].y);
    if (mode == 1) {                                             // 墨: 4本の輪が広がって、にじんで薄れる
      const float R = 18.0f * age * S / 466.0f + 4;
      if (d > R * 1.0f + 4) continue;
      if (d < 4.0f * S / 466.0f) { s += 1.0f; continue; }        // 落ちた墨のしずく
      const float ph = (R - d) / (7.0f * S / 466.0f);
      if (ph < 0 || ph > 4) continue;
      const float f = ph - floorf(ph);
      s += expf(-sqf((f - 0.5f) * 5.0f)) * (1.0f - age / 3.0f) * 0.8f;
    } else {                                                     // 光の輪 / きらめき: 1本の輪が広がる
      const float R = age * 0.55f * S, e = (d - R) / (0.05f * S);
      if (e < -3 || e > 3) continue;
      s += expf(-e * e) * (1.0f - age / 3.0f);
    }
  }
  return fminf(1.0f, s);
}

static void renderBand(int k, uint16_t* buf, Scratch& sc) {
  const int y0 = k * BAND, y1 = min(S, y0 + BAND);
  const int bx0 = bandX0[k], bw = bandW[k], bx1 = bx0 + bw - 1;
  const int rx = (int)sweep;
  // 波紋がこの帯にかかるか(かからなければ、計算しない)
  bool anyDrop = false;
  for (int i = 0; i < NDROP; ++i) {
    const float age = simT - drop[i].t0;
    if (age < 0 || age > 3.0f) continue;
    const float R = mode == 1 ? 18.0f * age * S / 466.0f + 12 : age * 0.55f * S + 0.15f * S;
    if (drop[i].y + R >= y0 && drop[i].y - R < y1) anyDrop = true;
  }
  const uint16_t inkC = rgb565(22, 22, 26);
  for (int y = y0; y < y1; ++y) {
    uint16_t* row = buf + (y - y0) * bw - bx0;
    const int xs = max((int)spanS[y], bx0), xe = min((int)spanE[y], bx1);
    for (int x = bx0; x < xs; ++x) row[x] = 0;
    for (int x = max(xe + 1, bx0); x <= bx1; ++x) row[x] = 0;
    if (xs > xe) continue;
    memcpy(row + xs, shadeImg + (size_t)y * S + xs, (xe - xs + 1) * 2);   // 陰をつけた砂の絵を写す
    if (anyDrop) {                                               // タップの輪(見え方ごと)
      const uint8_t* gt = grainTex + ((y & 127) << 7);
      for (int x = xs; x <= xe; ++x) {
        const float d = dropAt(x, y);
        if (d < 0.02f) continue;
        uint16_t c = outPix(row[x]);
        if (mode == 0) { if (gt[x & 127] < 32) c = add565(c, to565(mulC(C8(255, 250, 230), d))); }
        else if (mode == 1) c = mix565(c, inkC, (int)(d * 16));
        else c = add565(c, glowLut[min(63, (int)(d * 40) + 8)]);
        row[x] = outPix(c);
      }
    }
    // 熊手(月夜と墨: 木の棒。光: 光の棒)
    if (rx <= S) {
      const uint16_t bar = mode == 2 ? rgb565(120, 200, 255) : rgb565(90, 60, 35);
      for (int x = max(xs, rx - 3); x <= min(xe, rx + 1); ++x) row[x] = outPix(bar);
    }
  }
  // 石
  for (int i = 0; i < nStone; ++i) {
    const Stone& s = stones[i];
    // 月夜: 月と反対の向きに、ぼんやりした影を落とす
    if (mode == 0) {
      const float shx = s.x - litX * s.r * 0.9f, shy = s.y - litY * s.r * 0.9f, sr = s.r * 1.08f;
      const int ya = max(y0, (int)(shy - sr * s.sq) - 3), yb = min(y1 - 1, (int)(shy + sr * s.sq) + 3);
      for (int y = ya; y <= yb; ++y) {
        uint16_t* row = buf + (y - y0) * bw - bx0;
        const int xs = max(max((int)spanS[y], bx0), (int)(shx - sr) - 3), xe = min(min((int)spanE[y], bx1), (int)(shx + sr) + 3);
        for (int x = xs; x <= xe; ++x) {
          const float dx = (x - shx) / sr, dy = (y - shy) / (sr * s.sq), d = sqrtf(dx * dx + dy * dy);
          if (d > 1.15f) continue;
          const int q = (int)(clamp01((1.15f - d) / 0.25f) * 10);
          row[x] = outPix(mix565(outPix(row[x]), rgb565(8, 10, 20), q));
        }
      }
    }
    const int ya = max(y0, (int)(s.y - s.r * s.sq) - 1), yb = min(y1 - 1, (int)(s.y + s.r * s.sq) + 1);
    for (int y = ya; y <= yb; ++y) {
      const float ny = (y - s.y) / (s.r * s.sq);
      if (ny * ny >= 1) continue;
      const float hw = sqrtf(1 - ny * ny) * s.r;
      const int xs = max(max((int)spanS[y], bx0), (int)(s.x - hw)), xe = min(min((int)spanE[y], bx1), (int)(s.x + hw));
      uint16_t* row = buf + (y - y0) * bw - bx0;
      for (int x = xs; x <= xe; ++x) {
        const float nx = (x - s.x) / s.r, d2 = nx * nx + ny * ny, nz = sqrtf(fmaxf(0.0f, 1 - d2));
        const uint32_t h = grainTex[((y & 127) << 7) | ((x + i * 37) & 127)] * 0x01010101u;
        if (mode == 0) {                                         // ごつごつした石に、月の光。ふちに苔
          const float g = ((h & 255) / 255.0f - 0.5f) * 0.35f;
          const float l = clamp01(0.15f + 0.85f * fmaxf(0.0f, nx * litX + ny * litY + nz * 0.55f) + g);
          uint16_t c = stoneLut[(int)(l * 63)];
          if (d2 > 0.75f && ((h >> 8) & 3) == 0) c = add565(c, rgb565(10, 34, 14));
          row[x] = outPix(c);
        } else if (mode == 1) {                                  // 墨の石: まん中が濃く、ふちはかすれてにじむ
          const float dry = (h & 255) / 255.0f;
          float a = fminf(1.0f, powf(nz, 0.3f) * 1.15f) * (0.72f + 0.28f * dry);
          if (d2 > 0.82f && dry > 0.55f) a *= 0.5f;
          const int q = (int)(clamp01(a) * 16);
          row[x] = outPix(mix565(outPix(row[x]), rgb565(16, 16, 20), q));
        } else {                                                 // 鏡の球: まわりの光の砂紋が、丸くゆがんで映る
          const float rr2 = sqrtf(d2), f = 1.0f + 2.6f * (1.0f - nz);
          const int sx = (int)(s.x + nx * s.r * f), sy = (int)(s.y + ny * s.r * s.sq * f);
          int hv = 0;
          if (sx >= 0 && sy >= 0 && sx < S && sy < S) hv = hmap[(size_t)sy * S + sx];
          const float fres = 0.25f + 0.75f * rr2 * rr2 * rr2;
          uint16_t c = glowLut[(int)(hv * fres) >> 2];
          c = add565(c, rgb565((int)(6 + 10 * nz), (int)(8 + 14 * nz), (int)(16 + 24 * nz)));
          const float hl = expf(-((nx + 0.35f) * (nx + 0.35f) + (ny + 0.4f) * (ny + 0.4f)) / 0.03f);   // 光の照りかえし
          if (hl > 0.05f) c = add565(c, to565(mulC(C8(255, 255, 255), hl)));
          if (rr2 > 0.9f) c = add565(c, to565(mulC(C8(120, 200, 255), (rr2 - 0.9f) * 6)));
          row[x] = outPix(c);
        }
      }
    }
  }
  // 砂粒のきらめき(月夜だけ)
  if (mode == 0)
    for (int n = glStart[k]; n < glStart[k + 1]; ++n) {
      const int x = glintXY[n * 2], y = glintXY[n * 2 + 1];
      const int ph = (gFrame + ((x * 7 + y * 13) & 63)) & 63;
      if (ph >= 10 || x < bx0 || x > bx1) continue;
      bool under = false;
      for (int i = 0; i < nStone; ++i) { const float dx = x - stones[i].x, dy = (y - stones[i].y) / stones[i].sq; if (dx * dx + dy * dy < stones[i].r * stones[i].r * 1.3f) under = true; }
      if (under) continue;
      uint16_t* row = buf + (y - y0) * bw - bx0;
      row[x] = addPix(row[x], glint[ph]);
    }
  // 光る砂粒(十字の点)
  for (int kk = max(0, k - 1); kk <= min(NB - 1, k + 1); ++kk)
    for (int n = gStart[kk]; n < gStart[kk + 1]; ++n) {
      const GDraw& o = gdraw[gOrd[n]];
      for (int dy = -1; dy <= 1; ++dy) {
        const int y = o.y + dy;
        if (y < y0 || y >= y1) continue;
        const int xs = max((int)spanS[y], bx0), xe = min((int)spanE[y], bx1);
        uint16_t* row = buf + (y - y0) * bw - bx0;
        for (int dx = -1; dx <= 1; ++dx) {
          if (dx && dy) continue;
          const int x = o.x + dx;
          if (x < xs || x > xe) continue;
          row[x] = addPix(row[x], (dx | dy) ? (uint16_t)((o.c >> 1) & 0x7BEF) : o.c);
        }
      }
    }
}

static constexpr size_t nmOa57 = nmAlignTo(nmOv57, 16), nmOv58 = nmOa57 + sizeof(Scratch[2]); static Scratch (&scr)[2] = *reinterpret_cast<Scratch(*)[2]>(NM_ARENA + nmOa57);

static void __attribute__((noinline)) IRAM_ATTR renderBandIram(int k, uint16_t* b, Scratch& s) { renderBand(k, b, s); }

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
static_assert(std::is_trivially_default_constructible<Drop>::value, "NM_ARENA: Drop");
static_assert(std::is_trivially_default_constructible<GDraw>::value, "NM_ARENA: GDraw");
static_assert(std::is_trivially_default_constructible<Grain>::value, "NM_ARENA: Grain");
static_assert(std::is_trivially_default_constructible<Scratch>::value, "NM_ARENA: Scratch");
static_assert(std::is_trivially_default_constructible<Stone>::value, "NM_ARENA: Stone");
static_assert(std::is_trivially_default_constructible<bool>::value, "NM_ARENA: bool");
static_assert(std::is_trivially_default_constructible<float>::value, "NM_ARENA: float");
static_assert(std::is_trivially_default_constructible<int>::value, "NM_ARENA: int");
static_assert(std::is_trivially_default_constructible<int16_t>::value, "NM_ARENA: int16_t");
static_assert(std::is_trivially_default_constructible<uint16_t>::value, "NM_ARENA: uint16_t");
static_assert(std::is_trivially_default_constructible<uint32_t>::value, "NM_ARENA: uint32_t");
static_assert(std::is_trivially_default_constructible<uint8_t>::value, "NM_ARENA: uint8_t");
static constexpr size_t OVL_END = nmAlignTo(nmOv58, 16);
static_assert(OVL_END <= NM_ARENA_SIZE, "NM_ARENA_SIZE (nm_common.h) を大きくしてください");
extern const size_t OVL_SIZE = OVL_END;   // 起動時の確認用
}  // namespace NullZen
