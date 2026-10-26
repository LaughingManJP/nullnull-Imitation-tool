// NullSapiens  -  NullMenu 用モジュール(元のスケッチ NullSapiens_v3 を名前空間に入れたもの)
// 0 で始まる変数(配列も)は、ほかの作品と共通の置き場(NM_ARENA)に置いています。
// 一度に動くのは1つだけ(切り替えるときは再起動)なので、重なっていても問題ありません。
#include "nm_common.h"
#if defined(ESP_PLATFORM) && __has_include(<lgfx/v1/panel/Panel_AMOLED.hpp>)
#include <lgfx/v1/panel/Panel_AMOLED.hpp>
#endif
#pragma GCC optimize("O3")

namespace NullSapiens {
static constexpr size_t nmOv0 = 0;

// NullSapiens v3  -  さようなら、ホモ・サピエンス。体を輪切りにした光の線でできた人が、頭のほうから1本ずつ糸になってほどけ、光の方へ流れていく
//   大阪・関西万博のパビリオン「null²」を見た印象をもとにした、非公式の習作です。写真や他の作品の画像は使っていません。
//   M5Stack StopWatch 用(丸い 466x466 画面)。CoreS3 系でも真ん中に正方形で動きます。
//
// 画面の中:
//   ・ほとんど黒い空と、地平線の細い光。右の奥に、光の入り口。足もとは黒い鏡の床
//   ・人は、体を高さごとに輪切りにした細い輪郭線だけでできている(透けて見える光の彫刻)。縁ほど線が重なって明るく、光の当たる側はうすく虹色
//   ・頭の輪が、光の側でぷつりと切れて、1本の長い糸になってほどける。糸は髪のように束になって、ゆるくうねりながら光の方へ流れていく
//   ・しばらくすると、糸は巻き戻るように頭へ戻り、また輪になる
//   ・床には、輪郭線の脚がさざ波でゆれて映る
//   ・時間帯は3つ(夜明け前 / 夜 / 夕暮れ)
//
// 触ると:
//   タップ … ほどけはじめる(ほどけているときは、巻き戻る)
// 傾けると:
//   左右で、糸が流れる向きが少し変わる。床のさざ波も少し強くなる
// 振ると:
//   胸のあたりまで一気にほどける。画面の上に「self = null;」。しばらくすると巻き戻る
//
// 操作:
//   StopWatch … ボタンA: 時間帯(夜明け前 / 夜 / 夕暮れ。2秒かけて移りかわる)  ボタンB: 時間の速さ(1倍 / 2倍 / 0.5倍)
//   CoreS3系  … 電源ボタン短押し: 時間帯  ダブルタップ: 時間の速さ
//
// 必要なもの:
//   ボードマネージャー M5Stack >= 3.3.7 / ボード: M5StopWatch(CoreS3 なら M5CoreS3)。StopWatch では PSRAM を「OPI PSRAM」に
//   ライブラリ M5Unified >= 0.2.15, M5GFX >= 0.2.21
//
// しくみ:
//   人の形は、カプセルと楕円の玉 40 個を、なめらかにつないだもの(距離の関数)。頭と胴・右腕・左腕・右脚・左脚の5つに分けてある。
//   起動時に、高さごとに、それぞれの部分の輪切りの輪郭を求める(中心から放射状に、表面まで探していく)。
//     腰から下の輪は動かないので、起動時に光の絵(体と、床の映り込み)にしておく。
//     腰から上の輪は、点ごとに「光の方への流れ」にそって進んだ道すじ(0.24m おきに 13 か所)を、画面の位置にして覚えておく。
//   コマごとに、「ほどける高さ」から輪ごとのほどけ具合を決め、切れ目からの道のりに応じて、道すじのどこまで流されたかを求めて、
//     となりの点どうしを線でつなぐ。線は帯(16行ずつ)ごとに分けておき、2つのコアのうち空いた方が、次の帯を描く。
//   空と床は、時間帯ごとに画面と同じ大きさの絵を作っておく。床のさざ波は、行ごとに読む場所を少しずらす。

#ifndef IRAM_ATTR
#define IRAM_ATTR
#endif


// Arduino IDE は関数の宣言を自動で先頭近くに足すので、型の名前だけ先に宣言しておく
struct TodDef;
struct Prim;
struct Loop;
struct Sg;

// ---- 調整用 ----------------------------------------------------------------
static constexpr int   TOD_START   = 0;       // 始めの時間帯(0: 夜明け前 / 1: 夜 / 2: 夕暮れ)
static constexpr float UNRAVEL_SEC = 14.0f;   // ほどけきるまで(秒)
static constexpr float HOLD_SEC    = 4.0f;    // ほどけたまま(秒)
static constexpr float RETRACT_SEC = 8.0f;    // 巻き戻るまで(秒)
static constexpr float REST_MIN    = 5.0f;    // 輪のままでいる時間(秒。この間でばらばら)
static constexpr float REST_MAX    = 10.0f;
static constexpr bool  TILT        = true;    // 傾きで見え方を変えるか
static constexpr bool  TILT_FLIP_X = false;   // 左右が逆に動くときは true
static constexpr bool  TILT_FLIP_Y = false;   // 上下が逆に動くときは true
static constexpr bool  TILT_SWAP   = false;   // 左右と上下が入れかわっているときは true
static constexpr bool  TILT_STOPWATCH_FIX = true;   // StopWatch はセンサーが 90 度回ってついているので、自動で読みかえる
static constexpr float TILT_RELEVEL = 8.0f;   // 同じ角度で持ち続けると、何秒くらいでそこを「水平」にするか(0 = 起動したときの姿勢のまま)
static constexpr float TILT_RELEVEL_MAX = 0.5f;   // これより大きく傾けているあいだは、水平を合わせなおさない
// ---------------------------------------------------------------------------

// StopWatch では、画面へ「直接」送る(M5GFX の PSRAM の置き場を通さない)。画面が乱れるときは false に
static constexpr bool DIRECT_SEND = true;
#if defined(ESP_PLATFORM) && __has_include(<lgfx/v1/panel/Panel_AMOLED.hpp>)
#define NL_DIRECT_AMOLED 1
struct NlFbPeek : public lgfx::Panel_AMOLED_Framebuffer {
  static lgfx::Panel_AMOLED* inner(lgfx::Panel_Device* p) {
    return static_cast<lgfx::Panel_AMOLED_Framebuffer*>(p)->*(&NlFbPeek::_panel);
  }
};
#endif

static constexpr int BAND = 16, NSLOT = 4;
static constexpr int NTOD = 3;
static const char* const TOD_NAME[NTOD] = {"blue hour", "night", "dusk"};
static const float SPEEDS[3] = {1.0f, 2.0f, 0.5f};
static constexpr int NPRIM = 40, NGROUP = 5;
static constexpr int MAXL = 360, MAXP = 8000, NS = 13, NCOPY = 3, MAXSEG = 32000;
static constexpr float SSTEP = 0.24f;             // 道すじを覚える間かく(メートル)
static constexpr float TRANS_SEC = 2.0f;          // 時間帯が移りかわる時間(秒)
static constexpr float CAMY = 0.85f, CAMZ = 3.06f;   // 目の高さと、人までの距離(メートル)
static constexpr float FIGX = -0.22f, FIGROT = 0.40f;  // 人の位置と向き
static constexpr float LX = 2.0f, LY = 1.72f, LZ = -2.4f;   // 光の入り口
static constexpr float Y_DYN = 1.0f;              // これより上の輪は、ほどけることがある(コマごとに描く)
static constexpr float H_TOP = 1.85f, H_NORMAL = 1.48f, H_SHAKE = 1.05f;   // ほどける高さ: ほどけていない / ふだん / 振ったとき

static int W, H, S, XOFF, YOFF, NB;
static bool roundScr = true, fbMode = false, directSend = false;
static int16_t* spanA = nullptr;
static int16_t* spanB = nullptr;
static int16_t* bandX = nullptr;
static int16_t* bandW = nullptr;
static uint16_t* band[NSLOT] = {nullptr, nullptr, nullptr, nullptr};

static float HZ, FOC;                             // 地平線の高さ(画面の行)、遠近の強さ
static int   iHZ, feetY;                          // 地平線の行、足もとの行
static float FC = 1, FS = 0;                      // 人の向き(cos, sin)

// 時間帯ごとの空と床
static uint16_t* bg[NTOD] = {nullptr, nullptr, nullptr};
static uint16_t* layB = nullptr;                  // 腰から下の輪(光の絵)
static uint16_t* layR = nullptr;                  // 床に映った脚
static int8_t*   ripX = nullptr;                  // 床の行ごとの、横のずれ(さざ波)
static int8_t*   ripY = nullptr;                  // 床の行ごとの、縦のずれ
static float*    sinT = nullptr;                  // sin の表(1024)

struct TodDef {
  float skyA[3], skyB[3], skyK;                   // 空: 地平線から上の色(A + B × exp(-高さ / K))
  float hor[3];                                   // 地平線の細い光
  float floorC[3];                                // 床の色
  float light[3], lightK;                         // 光の入り口の色と強さ
  float stars;                                    // 星
};
static const TodDef TODS[NTOD] = {
  {{0.010f, 0.014f, 0.030f}, {0.030f, 0.050f, 0.100f}, 0.25f, {0.21f, 0.23f, 0.27f}, {0.004f, 0.005f, 0.010f}, {1.00f, 0.88f, 0.72f}, 1.0f, 0.5f},   // 夜明け前
  {{0.003f, 0.004f, 0.008f}, {0.008f, 0.010f, 0.020f}, 0.30f, {0.10f, 0.12f, 0.16f}, {0.002f, 0.003f, 0.006f}, {0.80f, 0.90f, 1.00f}, 0.9f, 1.0f},   // 夜
  {{0.015f, 0.008f, 0.016f}, {0.300f, 0.110f, 0.060f}, 0.12f, {0.40f, 0.22f, 0.12f}, {0.006f, 0.004f, 0.006f}, {1.00f, 0.70f, 0.42f}, 1.1f, 0.15f},  // 夕暮れ
};
static int   tod = TOD_START, todFrom = TOD_START;
static float transT = 1.0f;
static uint32_t wB = 0;

// 人の形: カプセル(kind 0: a, b, 太さ r1 → r2)と楕円の玉(kind 1: 中心 a, 半径 b)。足もとが原点、y が上、z が手前(メートル)
struct Prim { uint8_t kind, group; float ax, ay, az, bx, by, bz, r1, r2; };
static const Prim BODY[NPRIM] = {
  {1, 0, -0.0065f, 1.6388f, -0.0178f, 0.0780f, 0.1000f, 0.0980f, 0.0f, 0.0f},   // 頭
  {1, 0, 0.0111f, 1.5786f, 0.0304f, 0.0580f, 0.0500f, 0.0620f, 0.0f, 0.0f},   // あご
  {0, 0, 0.0247f, 1.6329f, 0.0677f, 0.0309f, 1.6045f, 0.0846f, 0.0160f, 0.0200f},   // 鼻
  {1, 0, 0.0152f, 1.6602f, 0.0415f, 0.0700f, 0.0300f, 0.0500f, 0.0f, 0.0f},   // ひたい
  {0, 0, 0.0000f, 1.4400f, -0.0100f, -0.0029f, 1.5846f, -0.0080f, 0.0540f, 0.0470f},   // 首
  {0, 0, -0.0800f, 1.4450f, -0.0300f, 0.0800f, 1.4450f, -0.0300f, 0.0550f, 0.0550f},   // 首のつけね
  {0, 0, -0.1600f, 1.4150f, -0.0200f, 0.1600f, 1.4150f, -0.0200f, 0.0500f, 0.0500f},   // 肩
  {1, 0, 0.0000f, 1.3000f, 0.0000f, 0.1500f, 0.1850f, 0.1050f, 0.0f, 0.0f},   // 胸かご
  {1, 0, -0.0680f, 1.3300f, 0.0600f, 0.0750f, 0.0550f, 0.0450f, 0.0f, 0.0f},   // 右の胸
  {1, 0, 0.0680f, 1.3300f, 0.0600f, 0.0750f, 0.0550f, 0.0450f, 0.0f, 0.0f},   // 左の胸
  {1, 0, 0.0000f, 1.1050f, 0.0120f, 0.1220f, 0.1500f, 0.0880f, 0.0f, 0.0f},   // 腹
  {1, 0, 0.0000f, 0.9500f, -0.0100f, 0.1600f, 0.1050f, 0.1050f, 0.0f, 0.0f},   // 腰
  {1, 0, -0.0720f, 0.9050f, -0.0600f, 0.0850f, 0.0950f, 0.0700f, 0.0f, 0.0f},   // 右のおしり
  {1, 0, 0.0720f, 0.9050f, -0.0600f, 0.0850f, 0.0950f, 0.0700f, 0.0f, 0.0f},   // 左のおしり
  {1, 1, -0.1880f, 1.3950f, -0.0100f, 0.0620f, 0.0720f, 0.0620f, 0.0f, 0.0f},   // 右の肩
  {0, 1, -0.1880f, 1.3950f, -0.0100f, -0.2330f, 1.1350f, -0.0100f, 0.0500f, 0.0380f},   // 右の上腕
  {1, 1, -0.2082f, 1.2780f, -0.0100f, 0.0500f, 0.0750f, 0.0500f, 0.0f, 0.0f},   // 右の力こぶ
  {0, 1, -0.2330f, 1.1350f, -0.0100f, -0.2630f, 0.8900f, 0.0300f, 0.0380f, 0.0240f},   // 右の前腕
  {1, 1, -0.2420f, 1.0615f, 0.0020f, 0.0420f, 0.0650f, 0.0400f, 0.0f, 0.0f},   // 右の前腕のふくらみ
  {0, 1, -0.2630f, 0.8900f, 0.0300f, -0.2630f, 0.8050f, 0.0400f, 0.0260f, 0.0200f},   // 右の手
  {0, 1, -0.2630f, 0.8050f, 0.0400f, -0.2630f, 0.7450f, 0.0400f, 0.0160f, 0.0100f},   // 右の指先
  {1, 2, 0.1880f, 1.3950f, -0.0100f, 0.0620f, 0.0720f, 0.0620f, 0.0f, 0.0f},   // 左の肩
  {0, 2, 0.1880f, 1.3950f, -0.0100f, 0.2380f, 1.1400f, 0.0200f, 0.0500f, 0.0380f},   // 左の上腕
  {1, 2, 0.2105f, 1.2803f, 0.0035f, 0.0500f, 0.0750f, 0.0500f, 0.0f, 0.0f},   // 左の力こぶ
  {0, 2, 0.2380f, 1.1400f, 0.0200f, 0.2880f, 0.9400f, 0.1700f, 0.0380f, 0.0240f},   // 左の前腕
  {1, 2, 0.2530f, 1.0800f, 0.0650f, 0.0420f, 0.0650f, 0.0400f, 0.0f, 0.0f},   // 左の前腕のふくらみ
  {0, 2, 0.2880f, 0.9400f, 0.1700f, 0.3080f, 0.8900f, 0.2500f, 0.0260f, 0.0200f},   // 左の手
  {0, 2, 0.3080f, 0.8900f, 0.2500f, 0.3180f, 0.8700f, 0.3000f, 0.0160f, 0.0100f},   // 左の指先
  {0, 3, -0.0900f, 0.9300f, 0.0000f, -0.0900f, 0.5050f, 0.0200f, 0.0880f, 0.0560f},   // 右のもも
  {1, 3, -0.0900f, 0.7813f, 0.0070f, 0.0800f, 0.1400f, 0.0850f, 0.0f, 0.0f},   // 右のももの太さ
  {1, 3, -0.0900f, 0.3874f, -0.0134f, 0.0550f, 0.1100f, 0.0580f, 0.0f, 0.0f},   // 右のふくらはぎ
  {0, 3, -0.0900f, 0.5050f, 0.0200f, -0.0900f, 0.0850f, -0.0100f, 0.0500f, 0.0280f},   // 右のすね
  {0, 3, -0.0900f, 0.0450f, -0.0400f, -0.0900f, 0.0350f, 0.1000f, 0.0360f, 0.0280f},   // 右の足
  {0, 3, -0.0900f, 0.0400f, -0.0500f, -0.0900f, 0.0350f, -0.0300f, 0.0340f, 0.0340f},   // 右のかかと
  {0, 4, 0.0900f, 0.9300f, 0.0000f, 0.1100f, 0.5100f, 0.0500f, 0.0880f, 0.0560f},   // 左のもも
  {1, 4, 0.0970f, 0.7830f, 0.0175f, 0.0800f, 0.1400f, 0.0850f, 0.0f, 0.0f},   // 左のももの太さ
  {1, 4, 0.1100f, 0.3952f, 0.0138f, 0.0550f, 0.1100f, 0.0580f, 0.0f, 0.0f},   // 左のふくらはぎ
  {0, 4, 0.1100f, 0.5100f, 0.0500f, 0.1100f, 0.1000f, 0.0100f, 0.0500f, 0.0280f},   // 左のすね
  {0, 4, 0.1100f, 0.0600f, -0.0200f, 0.1100f, 0.0500f, 0.1200f, 0.0360f, 0.0280f},   // 左の足
  {0, 4, 0.1100f, 0.0550f, -0.0300f, 0.1100f, 0.0500f, -0.0100f, 0.0340f, 0.0340f},   // 左のかかと
};

// 腰から上の輪
struct Loop { int start, n; float y, noise; int16_t brk; uint8_t group; };
static Loop*   lp = nullptr;
static int     nLoop = 0, nPt = 0;
static int16_t* stX = nullptr;                    // 点ごとの道すじ(画面の位置 × 4)[点][写し][13]
static int16_t* stY = nullptr;
static uint8_t* ptBr = nullptr;                   // 点ごとの明るさ
static uint8_t* ptHue = nullptr;                  // 点ごとの色合い
static uint8_t* ptOk = nullptr;                   // 輪郭が見つかったか
static constexpr size_t nmOa0 = nmAlignTo(nmOv0, 16), nmOv1 = nmOa0 + sizeof(uint8_t[256]); static uint8_t (&iriR)[256] = *reinterpret_cast<uint8_t(*)[256]>(NM_ARENA + nmOa0);
static constexpr size_t nmOa1 = nmAlignTo(nmOv1, 16), nmOv2 = nmOa1 + sizeof(uint8_t[256]); static uint8_t (&iriG)[256] = *reinterpret_cast<uint8_t(*)[256]>(NM_ARENA + nmOa1);
static constexpr size_t nmOa2 = nmAlignTo(nmOv2, 16), nmOv3 = nmOa2 + sizeof(uint8_t[256]); static uint8_t (&iriB)[256] = *reinterpret_cast<uint8_t(*)[256]>(NM_ARENA + nmOa2);  // 虹色の表

// コマごとの線
struct Sg { int16_t x0, y0, x1, y1; uint8_t r, g, b, w; };
static Sg*      sg = nullptr;
static int      nSg = 0;
static int32_t* bandIdx = nullptr;                // 帯ごとの線の番号
static int32_t* bandStart = nullptr;

// ほどけ
enum { ST_REST = 0, ST_UNRAVEL, ST_HOLD, ST_RETRACT };
static int   st = ST_REST;
static float hD = H_TOP, hTarget = H_NORMAL, stTimer = 3.0f, hSpeed = 0;

// 「self = null;」
static uint8_t* txtMask = nullptr;
static int txtW = 0, txtH = 0, txtX = 0, txtY = 0;
static float txtA = 0, txtHold = 0;

static float simT = 0;
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
static volatile uint8_t bandReady[64];
static uint32_t busyUs[2] = {0, 0}, sendUs = 0;
static volatile int job = 0;                      // コア0 にたのむ仕事: 0 = 帯を描く / 1 = 空と床 / 2 = 輪 / 3 = 道すじ
static int buildNo = 0;

// ---------------------------------------------------------------- 小さな道具
static inline float clamp01(float v) { return v < 0 ? 0 : (v > 1 ? 1 : v); }
static inline int clampi(int v, int a, int b) { return v < a ? a : (v > b ? b : v); }
static inline float smooth01(float v) { v = clamp01(v); return v * v * (3 - 2 * v); }
static inline uint16_t toPix(float r, float g, float b) {
  int R = (int)(r * 255.0f + 0.5f), G = (int)(g * 255.0f + 0.5f), B = (int)(b * 255.0f + 0.5f);
  R = R < 0 ? 0 : R > 255 ? 255 : R; G = G < 0 ? 0 : G > 255 ? 255 : G; B = B < 0 ? 0 : B > 255 ? 255 : B;
  return __builtin_bswap16((uint16_t)(((R & 0xF8) << 8) | ((G & 0xFC) << 3) | (B >> 3)));
}
static inline float aces(float x) { x *= 1.1f; const float v = (x * (2.51f * x + 0.03f)) / (x * (2.43f * x + 0.59f) + 0.14f); return powf(clamp01(v), 1.0f / 1.04f); }
static inline __attribute__((always_inline)) uint32_t spread(uint16_t p) { const uint32_t c = __builtin_bswap16(p); return (c | (c << 16)) & 0x07E0F81Fu; }
static inline __attribute__((always_inline)) uint16_t unspread(uint32_t v) { v &= 0x07E0F81Fu; return __builtin_bswap16((uint16_t)(v | (v >> 16))); }
static inline __attribute__((always_inline)) uint16_t mix565(uint16_t a, uint16_t b, uint32_t w) { return unspread((spread(a) * (32 - w) + spread(b) * w) >> 5); }
static inline __attribute__((always_inline)) uint16_t addSat565(uint16_t a, uint16_t b) {   // 光を足す(あふれたら止める)
  const uint32_t x = __builtin_bswap16(a), y = __builtin_bswap16(b);
  int R = (int)(x >> 11) + (int)(y >> 11), G = (int)((x >> 5) & 63) + (int)((y >> 5) & 63), B = (int)(x & 31) + (int)(y & 31);
  R = R > 31 ? 31 : R; G = G > 63 ? 63 : G; B = B > 31 ? 31 : B;
  return __builtin_bswap16((uint16_t)((R << 11) | (G << 5) | B));
}
static inline __attribute__((always_inline)) uint16_t screenAdd(uint16_t p, int r, int g, int b, int w) {   // r, g, b: 0..255 / w: 0..256
  const uint32_t c = __builtin_bswap16(p);
  int R = c >> 11, G = (c >> 5) & 63, B = c & 31;
  R += ((31 - R) * r * w) >> 16; G += ((63 - G) * g * w) >> 16; B += ((31 - B) * b * w) >> 16;
  return __builtin_bswap16((uint16_t)((R << 11) | (G << 5) | B));
}
static inline uint32_t hash32(uint32_t x) { x ^= x >> 16; x *= 0x7feb352d; x ^= x >> 15; x *= 0x846ca68b; x ^= x >> 16; return x; }
static inline float fsin(float x) { return sinT[(int)(x * (1024.0f / 6.2831853f)) & 1023]; }
static inline float fcos(float x) { return sinT[((int)(x * (1024.0f / 6.2831853f)) + 256) & 1023]; }

static void showMsg(const char* msg, float prog);
static void fail(const char* msg);

// ---------------------------------------------------------------- 人の形
static inline float primD(const Prim& k, float x, float y, float z) {
  if (k.kind == 0) {                               // 太さの変わるカプセル
    const float bx = k.bx - k.ax, by = k.by - k.ay, bz = k.bz - k.az;
    const float px = x - k.ax, py = y - k.ay, pz = z - k.az;
    float t = (px * bx + py * by + pz * bz) / (bx * bx + by * by + bz * bz);
    t = t < 0 ? 0 : (t > 1 ? 1 : t);
    const float qx = px - bx * t, qy = py - by * t, qz = pz - bz * t;
    return sqrtf(qx * qx + qy * qy + qz * qz) - (k.r1 + (k.r2 - k.r1) * t);
  }
  const float qx = (x - k.ax) / k.bx, qy = (y - k.ay) / k.by, qz = (z - k.az) / k.bz;   // 楕円の玉(だいたいの距離)
  const float k0 = sqrtf(qx * qx + qy * qy + qz * qz);
  const float k1 = sqrtf(qx * qx / (k.bx * k.bx) + qy * qy / (k.by * k.by) + qz * qz / (k.bz * k.bz));
  return k0 * (k0 - 1.0f) / fmaxf(k1, 1e-6f);
}
static inline float sminf(float a, float b, float k) { const float h = clamp01(0.5f + 0.5f * (b - a) / k); return b + (a - b) * h - k * h * (1.0f - h); }
static float sdfMask(float x, float y, float z, uint64_t mask) {   // mask: 使う部品
  float d = 1e9f; bool first = true;
  while (mask) {
    const int i = __builtin_ctzll(mask); mask &= mask - 1;
    const float e = primD(BODY[i], x, y, z);
    d = first ? e : sminf(d, e, 0.032f); first = false;
  }
  return d;
}
static void primYRange(const Prim& k, float& y0, float& y1) {
  if (k.kind == 0) { const float r = fmaxf(k.r1, k.r2); y0 = fminf(k.ay, k.by) - r; y1 = fmaxf(k.ay, k.by) + r; }
  else { y0 = k.ay - k.by; y1 = k.ay + k.by; }
}
static inline void toWorld(float lx, float ly, float lz, float& wx, float& wy, float& wz) { wx = FIGX + FC * lx + FS * lz; wy = ly; wz = -FS * lx + FC * lz; }
static inline void toScreen(float x, float y, float z, float& sx, float& sy) { const float zc = CAMZ - z; sx = S * 0.5f + FOC * x / zc; sy = HZ - FOC * (y - CAMY) / zc; }
// 高さ y での、部分 g の輪切りの輪郭(中心から放射状に探す)。M 点。見つからない点は ok = 0
static int sliceRing(int g, float y, int M, float* px, float* pz, uint8_t* ok) {
  uint64_t mask = 0; float cx = 0, cz = 0, wsum = 0;
  for (int i = 0; i < NPRIM; ++i) {
    const Prim& k = BODY[i];
    if (k.group != g) continue;
    float y0, y1; primYRange(k, y0, y1);
    if (y < y0 - 0.04f || y > y1 + 0.04f) continue;
    mask |= 1ull << i;
    if (y < y0 || y > y1) continue;
    float ax, az, r;
    if (k.kind == 0) {
      const float dy = k.by - k.ay; float t = fabsf(dy) > 1e-5f ? (y - k.ay) / dy : 0.5f; t = clamp01(t);
      ax = k.ax + (k.bx - k.ax) * t; az = k.az + (k.bz - k.az) * t; r = k.r1 + (k.r2 - k.r1) * t;
    } else { ax = k.ax; az = k.az; r = fminf(k.bx, k.bz); }
    cx += ax * r; cz += az * r; wsum += r;
  }
  if (!mask || wsum <= 0) return 0;
  cx /= wsum; cz /= wsum;
  if (sdfMask(cx, y, cz, mask) > -0.002f) return 0;
  int found = 0;
  for (int j = 0; j < M; ++j) {
    const float a = j * (6.2831853f / M), dx = cosf(a), dz = sinf(a);
    float r0 = 0, r1 = 0; bool hit = false;
    for (float r = 0.004f; r < 0.40f; r += 0.004f) {
      if (sdfMask(cx + dx * r, y, cz + dz * r, mask) > 0) { r1 = r; hit = true; break; }
      r0 = r;
    }
    ok[j] = hit;
    if (!hit) { px[j] = cx; pz[j] = cz; continue; }
    for (int it = 0; it < 6; ++it) { const float m = 0.5f * (r0 + r1); if (sdfMask(cx + dx * m, y, cz + dz * m, mask) > 0) r1 = m; else r0 = m; }
    const float r = 0.5f * (r0 + r1);
    px[j] = cx + dx * r; pz[j] = cz + dz * r; ++found;
  }
  return found > M / 3 ? M : 0;
}
static void normalAt(int g, float x, float y, float z, float* n) {   // 表面の向き(人の向きの座標 → 世界)
  uint64_t mask = 0;
  for (int i = 0; i < NPRIM; ++i) if (BODY[i].group == g) mask |= 1ull << i;
  const float e = 0.002f;
  float nx = sdfMask(x + e, y, z, mask) - sdfMask(x - e, y, z, mask);
  float ny = sdfMask(x, y + e, z, mask) - sdfMask(x, y - e, z, mask);
  float nz = sdfMask(x, y, z + e, mask) - sdfMask(x, y, z - e, mask);
  const float l = 1.0f / (sqrtf(nx * nx + ny * ny + nz * nz) + 1e-9f); nx *= l; ny *= l; nz *= l;
  n[0] = FC * nx + FS * nz; n[1] = ny; n[2] = -FS * nx + FC * nz;
}
// 点の明るさと色合い(縁ほど明るく、光の側は強く。体の向こう側はうすく)
static void shadePoint(float wx, float wy, float wz, const float* n, float& br, float& hue) {
  float vx = -wx, vy = CAMY - wy, vz = CAMZ - wz; float l = 1.0f / sqrtf(vx * vx + vy * vy + vz * vz); vx *= l; vy *= l; vz *= l;
  float tx = LX - wx, ty = LY - wy, tz = LZ - wz; l = 1.0f / sqrtf(tx * tx + ty * ty + tz * tz); tx *= l; ty *= l; tz *= l;
  const float ndv = n[0] * vx + n[1] * vy + n[2] * vz, lit = clamp01(n[0] * tx + n[1] * ty + n[2] * tz);
  const float e = 1.0f - fabsf(ndv);
  br = 0.05f + 0.75f * powf(e, 2.2f) + 0.70f * lit * lit * lit;
  if (ndv < 0) br *= 0.45f;
  hue = 0.55f * wy + 0.9f * e;
}

// ---------------------------------------------------------------- 起動時に作るもの
// 空と床(時間帯 t)の行。y0 から step おきに
static float* acesT = nullptr;                    // 光の強さ(0..8)→ 画面の明るさ [2048]
static float* glowT = nullptr;                    // 光の入り口からの距離 → 光 [1024]
static float* flT = nullptr;                      // 床に映る光 [1024]
static inline float acesL(float x) { const int i = (int)(x * 256.0f); return i <= 0 ? 0.0f : acesT[i > 2047 ? 2047 : i]; }
static void buildBgTables() {
  for (int i = 0; i < 2048; ++i) acesT[i] = aces(i / 256.0f);
  for (int i = 0; i < 1024; ++i) {
    const float rr = i * (1.6f / 1024.0f);
    glowT[i] = 1.4f * expf(-rr / 0.012f) + 0.45f * expf(-rr / 0.05f) + 0.20f * expf(-rr / 0.18f) + 0.08f * expf(-rr / 0.5f);
    flT[i] = 0.25f * expf(-rr / 0.03f) + 0.06f * expf(-rr / 0.15f);
  }
}
static void bgRows(int t, int y0, int step, int core) {
  const TodDef& T = TODS[t];
  float lsx, lsy; toScreen(LX, LY, LZ, lsx, lsy);
  const float my = 2 * HZ - lsy, k1024 = 1024.0f / (1.6f * S);
  int cnt = 0;
  for (int y = y0; y < S; y += step) {
    if (core == 0 && ((++cnt) & 7) == 0) vTaskDelay(1);
    const float e = (HZ - (y + 0.5f)) / FOC, ea = fabsf(e);
    float base[3];                                 // 行ごとに同じもの: 空(床)の色と、地平線の光
    const float sk = expf(-ea / T.skyK), hz = expf(-(e / 0.0035f) * (e / 0.0035f)), fr = 0.25f * expf(-ea / 0.05f);
    for (int i = 0; i < 3; ++i) base[i] = ((e > 0) ? T.skyA[i] + T.skyB[i] * sk : T.floorC[i] + (T.skyA[i] + T.skyB[i] * sk) * fr) + T.hor[i] * hz;
    const float dy = y - lsy, dy2 = 0.12f * (y - my) * (y - my);
    const float ey = (y - S * 0.5f) / (S * 0.5f);
    for (int x = 0; x < S; ++x) {
      const float dx = x - lsx;
      float gl = glowT[min(1023, (int)(sqrtf(dx * dx + dy * dy) * k1024))];
      if (e < 0) gl += flT[min(1023, (int)(sqrtf(dx * dx + dy2) * k1024))];   // 床に映る光(縦にのびる)
      gl *= T.lightK;
      float c[3] = {base[0] + gl * T.light[0], base[1] + gl * T.light[1], base[2] + gl * T.light[2]};
      if (T.stars > 0 && e > 0.04f) {
        const uint32_t hh = hash32((uint32_t)(y * 7919 + x) ^ 0x9e3779b9u);
        if ((hh & 1023) < 2) { float b = ((hh >> 12) & 255) / 255.0f; b = b * b * b * T.stars * 0.8f; c[0] += b; c[1] += b; c[2] += b * 1.1f; }
      }
      const float ex = (x - S * 0.5f) / (S * 0.5f), r2 = ex * ex + ey * ey;
      const float vg = clamp01(1.0f - 0.25f * r2 * (0.6f + 0.4f * r2));   // まわりを少し暗く
      const uint32_t dh = hash32((uint32_t)(y * 4099 + x * 7) ^ 0x51ed27u);
      const float d = ((dh & 255) * (1.0f / 255.0f) - 0.5f);
      bg[t][y * S + x] = toPix(acesL(c[0]) * vg + d * (8.0f / 255.0f), acesL(c[1]) * vg + d * (4.0f / 255.0f), acesL(c[2]) * vg + d * (8.0f / 255.0f));
    }
  }
}
// 光の線を、光の置き場(赤・緑・青 × 256)へ足す
static void splatLine(uint16_t* acc, float x0, float y0, float x1, float y1, float r, float g, float b) {
  const float dx = x1 - x0, dy = y1 - y0;
  const int n = max(1, (int)ceilf(fmaxf(fabsf(dx), fabsf(dy)) / 0.7f));
  const float L = sqrtf(dx * dx + dy * dy) / n;
  for (int i = 0; i <= n; ++i) {
    const float x = x0 + dx * i / n, y = y0 + dy * i / n;
    const int ix = (int)floorf(x), iy = (int)floorf(y);
    if (ix < 0 || iy < 0 || ix >= S - 1 || iy >= S - 1) continue;
    const float fx = x - ix, fy = y - iy;
    const float w[4] = {(1 - fx) * (1 - fy) * L, fx * (1 - fy) * L, (1 - fx) * fy * L, fx * fy * L};
    const int o[4] = {iy * S + ix, iy * S + ix + 1, (iy + 1) * S + ix, (iy + 1) * S + ix + 1};
    for (int k = 0; k < 4; ++k) {
      uint16_t* p = acc + o[k] * 3;
      p[0] = (uint16_t)min(65535, (int)p[0] + (int)(r * w[k] * 256.0f));
      p[1] = (uint16_t)min(65535, (int)p[1] + (int)(g * w[k] * 256.0f));
      p[2] = (uint16_t)min(65535, (int)p[2] + (int)(b * w[k] * 256.0f));
    }
  }
}
static void iriCol(float t, float mix, float br, float* c) {   // 虹色を少しまぜた白
  const int i = ((int)(t * 256.0f)) & 255;
  c[0] = (iriR[i] / 255.0f * mix + 0.86f * (1 - mix)) * br;
  c[1] = (iriG[i] / 255.0f * mix * 0.85f + 0.86f * (1 - mix)) * br;   // 緑は少しひかえめに(うすい紫がかった白)
  c[2] = (iriB[i] / 255.0f * mix + 1.00f * (1 - mix)) * br;
}
// 腰から下の輪を、光の絵にする(体と、床の映り込み)
static void buildStatic(uint16_t* acc) {
  const int M = 40, MAXR = 520;
  // 輪の形は1回だけ求めて、体と床の映り込みの2回、光の絵にする
  float* gx = (float*)ps_malloc(sizeof(float) * MAXR * M * 7);   // 点ごと: 画面 x, y, 映り込みの x, y, 赤, 緑, 青
  uint8_t* gok = (uint8_t*)ps_malloc((size_t)MAXR * M);
  if (!gx || !gok) fail("Memory error (lines)");
  float px[64], pz[64], br[64], hue[64]; uint8_t ok[64];
  int nr = 0, ri = 0;
  for (float y = 0.012f; y < Y_DYN && nr < MAXR; y += 0.012f, ++ri) {
    if ((ri & 7) == 0) vTaskDelay(1);
    for (int g = 0; g < NGROUP && nr < MAXR; ++g) {
      if (!sliceRing(g, y, M, px, pz, ok)) continue;
      float* q = gx + (size_t)nr * M * 7;
      for (int j = 0; j < M; ++j) {
        float wx, wy, wz; toWorld(px[j], y, pz[j], wx, wy, wz);
        toScreen(wx, wy, wz, q[j * 7], q[j * 7 + 1]);
        toScreen(wx, -wy, wz, q[j * 7 + 2], q[j * 7 + 3]);
        float n[3]; normalAt(g, px[j], y, pz[j], n);
        shadePoint(wx, wy, wz, n, br[j], hue[j]);
      }
      const float ov = (g == 0 && y < 0.98f) ? 0.55f : ((g >= 3 && y > 0.80f) ? 0.65f : 1.0f);   // 腰は、胴と脚の輪が重なるので、少し暗く
      for (int j = 0; j < M; ++j) {
        const int k = (j + 1) % M;
        gok[nr * M + j] = ok[j] && ok[k];
        float c[3]; iriCol(0.5f * (hue[j] + hue[k]), 0.30f, 0.5f * (br[j] + br[k]) * 0.75f * ov, c);
        q[j * 7 + 4] = c[0]; q[j * 7 + 5] = c[1]; q[j * 7 + 6] = c[2];
      }
      ++nr;
    }
  }
  for (int pass = 0; pass < 2; ++pass) {           // 0: 体 / 1: 床の映り込み
    memset(acc, 0, (size_t)S * S * 3 * 2);
    const float kb = pass ? 0.30f : 1.0f;
    for (int r = 0; r < nr; ++r) {
      const float* q = gx + (size_t)r * M * 7;
      for (int j = 0; j < M; ++j) {
        if (!gok[r * M + j]) continue;
        const int k = (j + 1) % M, o = pass ? 2 : 0;
        splatLine(acc, q[j * 7 + o], q[j * 7 + o + 1], q[k * 7 + o], q[k * 7 + o + 1], q[j * 7 + 4] * kb, q[j * 7 + 5] * kb, q[j * 7 + 6] * kb);
      }
    }
    uint16_t* out = pass ? layR : layB;
    for (int i = 0; i < S * S; ++i) {
      const uint16_t* p = acc + i * 3;
      const float r = p[0] / 256.0f, g = p[1] / 256.0f, b = p[2] / 256.0f;
      out[i] = (r + g + b < 0.004f) ? 0 : toPix(1.0f - expf(-r * 1.15f), 1.0f - expf(-g * 1.15f), 1.0f - expf(-b * 1.15f));
    }
  }
  free(gx); free(gok);
}
// 腰から上の輪(点を覚える)
static void buildLoops() {
  const int MT = 48, MA = 24;
  float px[64], pz[64]; uint8_t ok[64];
  int ri = 0;
  for (float y = Y_DYN; y < 1.775f; y += (y < 1.45f ? 0.012f : 0.007f), ++ri) {
    if ((ri & 3) == 0) vTaskDelay(1);
    for (int g = 0; g < 3; ++g) {                 // 頭と胴、右腕、左腕
      const int M = g ? MA : MT;
      if (nLoop >= MAXL || nPt + M > MAXP) break;
      if (!sliceRing(g, y, M, px, pz, ok)) continue;
      Loop& L = lp[nLoop++];
      L.start = nPt; L.n = M; L.y = y; L.group = (uint8_t)g;
      L.noise = 0.20f * sinf(ri * 1.7f + g) + 0.12f * sinf(ri * 0.31f);
      float best = -1e9f; int bi = 0;
      for (int j = 0; j < M; ++j) {
        float wx, wy, wz; toWorld(px[j], y, pz[j], wx, wy, wz);
        float n[3]; normalAt(g, px[j], y, pz[j], n);
        float b, h; shadePoint(wx, wy, wz, n, b, h);
        const int p = nPt + j;
        ptBr[p] = (uint8_t)clampi((int)(b * 255.0f), 0, 255); ptHue[p] = (uint8_t)(((int)(h * 256.0f)) & 255); ptOk[p] = ok[j];
        stX[(p * NCOPY) * NS] = (int16_t)lrintf(wx * 1000.0f);   // 道すじを作るまで、世界の位置(mm)を置いておく
        stY[(p * NCOPY) * NS] = (int16_t)lrintf(wz * 1000.0f);
        stX[(p * NCOPY) * NS + 1] = (int16_t)lrintf(wy * 1000.0f);
        float tx = LX - wx, ty = LY - wy, tz = LZ - wz; const float l = 1.0f / sqrtf(tx * tx + ty * ty + tz * tz);
        const float d = (n[0] * tx + n[1] * ty + n[2] * tz) * l;
        if (ok[j] && d > best) { best = d; bi = j; }
      }
      L.brk = (int16_t)bi;
      nPt += M;
    }
  }
}
// 点ごとの道すじ(光の方への流れにそって進む)。p0 から step おきの点
static void flowAt(float x, float y, float z, float ph, float tilt, float* v) {
  float tx = LX - x, ty = LY - y, tz = LZ - z; float l = 1.0f / sqrtf(tx * tx + ty * ty + tz * tz); tx *= l; ty *= l; tz *= l;
  const float wx = fsin(y * 3.1f + z * 2.3f + 1.0f + ph), wy = 0.6f * fcos(x * 2.7f + y * 1.9f + ph * 0.7f), wz = fsin(x * 2.2f - y * 2.6f + 2.0f + ph * 1.3f);
  float vx = tx * 0.70f + wx * 0.50f + tilt, vy = ty * 0.70f + wy * 0.50f + 0.20f, vz = tz * 0.70f + wz * 0.50f;
  l = 1.0f / sqrtf(vx * vx + vy * vy + vz * vz); v[0] = vx * l; v[1] = vy * l; v[2] = vz * l;
}
static void buildStreams(int p0, int step, int core) {
  static const float PH[NCOPY] = {0.0f, 1.2f, 2.5f};
  int cnt = 0;
  for (int p = p0; p < nPt; p += step) {
    if (core == 0 && ((++cnt) & 63) == 0) vTaskDelay(1);
    const float wx0 = stX[(p * NCOPY) * NS] / 1000.0f, wz0 = stY[(p * NCOPY) * NS] / 1000.0f, wy0 = stX[(p * NCOPY) * NS + 1] / 1000.0f;
    for (int c = 0; c < NCOPY; ++c) {
      float x = wx0 + (c ? 0.008f * (frand() - 0.5f) : 0), y = wy0 + (c ? 0.008f * (frand() - 0.5f) : 0), z = wz0;
      int16_t* sx = stX + (p * NCOPY + c) * NS; int16_t* sy = stY + (p * NCOPY + c) * NS;
      for (int k = 0; k < NS; ++k) {
        float fx, fy; toScreen(x, y, z, fx, fy);
        sx[k] = (int16_t)clampi((int)lrintf(fx * 4.0f), -32000, 32000); sy[k] = (int16_t)clampi((int)lrintf(fy * 4.0f), -32000, 32000);
        for (int q = 0; q < 4; ++q) { float v[3]; flowAt(x, y, z, PH[c], 0.0f, v); x += v[0] * (SSTEP / 4); y += v[1] * (SSTEP / 4); z += v[2] * (SSTEP / 4); }
      }
    }
  }
}

// ---------------------------------------------------------------- 動き
static void startUnravel(float target, float sec) {
  st = ST_UNRAVEL; hTarget = target; hSpeed = (H_TOP - target) / sec;
}
static void shakeAll() {
  startUnravel(H_SHAKE, 5.0f);
  txtHold = 3.5f;
  Serial.println("self = null;");
}
static void simulate(float dt) {
  simT += dt;
  const float spd = SPEEDS[speedIdx];
  switch (st) {
    case ST_REST: stTimer -= dt * spd; if (stTimer <= 0) startUnravel(H_NORMAL, UNRAVEL_SEC); break;
    case ST_UNRAVEL: hD -= hSpeed * dt * spd; if (hD <= hTarget) { hD = hTarget; st = ST_HOLD; stTimer = HOLD_SEC; } break;
    case ST_HOLD: stTimer -= dt * spd; if (stTimer <= 0) { st = ST_RETRACT; hSpeed = (H_TOP - hD) / RETRACT_SEC; } break;
    case ST_RETRACT: hD += hSpeed * dt * spd; if (hD >= H_TOP) { hD = H_TOP; st = ST_REST; stTimer = REST_MIN + (REST_MAX - REST_MIN) * frand(); } break;
  }
  // 床のさざ波(手前ほど大きい。傾けると少し強く)
  const float amp = 1.0f + 1.2f * fminf(1.0f, fabsf(tiltX) + fabsf(tiltY));
  for (int y = 0; y < S; ++y) {
    if (y <= iHZ) { ripX[y] = 0; ripY[y] = 0; continue; }
    const float f = (y - HZ) / (S - HZ);
    ripX[y] = (int8_t)lrintf(amp * (0.3f + 1.6f * f) * fsin(y * 0.83f + simT * 2.3f) * (0.6f + 0.4f * fsin(y * 0.11f - simT * 0.7f)));
    ripY[y] = (int8_t)lrintf(amp * (0.2f + 0.8f * f) * fsin(y * 0.57f - simT * 1.9f));
  }
  if (txtHold > 0) { txtHold -= dt; txtA = fminf(1.0f, txtA + dt * 2.5f); } else txtA = fmaxf(0.0f, txtA - dt * 0.8f);
}
// コマごとの線を作る(腰から上の輪と、ほどけた糸)
static inline void addSeg(float x0, float y0, float x1, float y1, int r, int g, int b, int w) {
  if (nSg >= MAXSEG || w <= 0) return;
  Sg& s = sg[nSg++];
  s.x0 = (int16_t)x0; s.y0 = (int16_t)y0; s.x1 = (int16_t)x1; s.y1 = (int16_t)y1;
  s.r = (uint8_t)r; s.g = (uint8_t)g; s.b = (uint8_t)b; s.w = (uint8_t)min(255, w);
}
static constexpr size_t nmOa3 = nmAlignTo(nmOv3, 16), nmOv4 = nmOa3 + sizeof(uint8_t[256]); static uint8_t (&fadeT)[256] = *reinterpret_cast<uint8_t(*)[256]>(NM_ARENA + nmOa3);
static constexpr size_t nmOa4 = nmAlignTo(nmOv4, 16), nmOv5 = nmOa4 + sizeof(uint8_t[256]); static uint8_t (&mixT)[256] = *reinterpret_cast<uint8_t(*)[256]>(NM_ARENA + nmOa4);
static constexpr size_t nmOa5 = nmAlignTo(nmOv5, 16), nmOv6 = nmOa5 + sizeof(uint8_t[256]); static uint8_t (&iriG85)[256] = *reinterpret_cast<uint8_t(*)[256]>(NM_ARENA + nmOa5);   // 流された長さ(0..2.88m を 256 段)→ うすれ方・虹色のまぜ方
static void buildSegTables() {
  for (int i = 0; i < 256; ++i) {
    const float Sd = i * (SSTEP * (NS - 1) / 256.0f);
    fadeT[i] = (uint8_t)clampi((int)(expf(-Sd / 2.2f) * (1.0f + 0.9f * fminf(1.0f, Sd * 3.0f) * expf(-Sd / 0.8f)) * 128.0f), 0, 255);
    mixT[i] = (uint8_t)clampi((int)(fminf(0.45f, 0.22f + 0.20f * Sd) * 256.0f), 0, 255);
    iriG85[i] = (uint8_t)(iriG[i] * 0.85f);
  }
}
static void buildSegs() {
  nSg = 0;
  const float shimmer = simT * 0.8f;
  const float tiltShift = tiltX * 40.0f;           // 傾けると、糸の先が少し横へ(画面の点 × 4)
  const float SMAX = SSTEP * (NS - 1) - 0.01f, kS = 256.0f / (SSTEP * (NS - 1));
  float upw[64];
  for (int li = 0; li < nLoop; ++li) {
    const Loop& L = lp[li];
    float a = (L.y - hD) / 0.16f + L.noise;
    a = a > 0 ? a * a : 0.0f;
    const int M = L.n;
    const int copies = a > 0 ? NCOPY : 1;
    const int sh = (int)((0.85f + 0.15f * fsin(L.y * 30.0f - shimmer * 3.0f)) * 256.0f);   // ゆっくり上へ流れる光
    const int wk = a > 0 ? 205 : 180;
    if (a > 0) for (int j = 0; j < M; ++j) upw[j] = a * powf((float)j / (M - 1), 1.2f) * 4.6f;   // 流された長さ(メートル)
    for (int c = 0; c < copies; ++c) {
      int lx = 0, ly = 0, lr = 0, lg = 0, lb = 0, lw = 0; bool have = false;
      for (int j = 0; j <= (a > 0 ? M - 1 : M); ++j) {
        const int jj = (L.brk + j) % M, p = L.start + jj;
        if (!ptOk[p]) { have = false; continue; }
        const int16_t* sx = stX + (p * NCOPY + c) * NS; const int16_t* sy = stY + (p * NCOPY + c) * NS;
        int x, y, si = 0;
        if (a > 0) {
          float Sd = upw[j]; if (Sd > SMAX) Sd = SMAX;
          const float f = Sd / SSTEP; const int k = (int)f; const float t = f - k;
          x = (int)(sx[k] + (sx[k + 1] - sx[k]) * t + tiltShift * Sd * Sd * 0.25f); y = (int)(sy[k] + (sy[k + 1] - sy[k]) * t);
          si = (int)(Sd * kS);
        } else { x = sx[0]; y = sy[0]; }
        const int h = (ptHue[p] + (si * 141 >> 8)) & 255;   // 流されるほど、色合いがずれる
        const int m = mixT[si];
        const int r = (iriR[h] * m + 219 * (256 - m)) >> 8, g = (iriG85[h] * m + 219 * (256 - m)) >> 8, b = (iriB[h] * m + 255 * (256 - m)) >> 8;
        const int w = (int)((((uint32_t)ptBr[p] * fadeT[si] * (uint32_t)sh) >> 8) * wk >> 15);
        if (have) addSeg((float)lx, (float)ly, (float)x, (float)y, (r + lr) >> 1, (g + lg) >> 1, (b + lb) >> 1, (w + lw) >> 1);
        lx = x; ly = y; lr = r; lg = g; lb = b; lw = w; have = true;
      }
    }
  }
  // 帯ごとに分ける(数える → 置く)
  for (int k = 0; k <= NB; ++k) bandStart[k] = 0;
  for (int i = 0; i < nSg; ++i) {
    const int a = clampi((min(sg[i].y0, sg[i].y1) >> 2) - 1, 0, S - 1) / BAND, b = clampi((max(sg[i].y0, sg[i].y1) >> 2) + 1, 0, S - 1) / BAND;
    for (int k = a; k <= b; ++k) bandStart[k + 1]++;
  }
  for (int k = 0; k < NB; ++k) bandStart[k + 1] += bandStart[k];
  static int32_t* fillp = nullptr;
  if (!fillp) fillp = (int32_t*)malloc(sizeof(int32_t) * (NB + 1));
  for (int k = 0; k < NB; ++k) fillp[k] = bandStart[k];
  for (int i = 0; i < nSg; ++i) {
    const int a = clampi((min(sg[i].y0, sg[i].y1) >> 2) - 1, 0, S - 1) / BAND, b = clampi((max(sg[i].y0, sg[i].y1) >> 2) + 1, 0, S - 1) / BAND;
    for (int k = a; k <= b; ++k) bandIdx[fillp[k]++] = i;
  }
}
static void startTod(int t) {
  todFrom = (transT >= 1.0f) ? tod : (wB >= 16 ? tod : todFrom);
  tod = t; transT = 0.0f;
  Serial.printf("time of day: %s\n", TOD_NAME[tod]);
}
static void stepTod(float dt) {
  if (transT < 1.0f) transT = fminf(1.0f, transT + dt / TRANS_SEC);
  wB = transT >= 1.0f ? 0 : (uint32_t)(smooth01(transT) * 32.0f + 0.5f);
}

// ---------------------------------------------------------------- 描く
static void IRAM_ATTR renderBand(int kb, uint16_t* out, int bx, int bw) {
  const int y0 = kb * BAND, h = min(BAND, S - y0);
  const uint16_t* bA = bg[transT >= 1.0f ? tod : todFrom];
  const uint16_t* bB = bg[tod];
  for (int rr = 0; rr < h; ++rr) {
    const int y = y0 + rr;
    uint16_t* o = out + rr * bw - bx;
    const int sa = max((int)spanA[y], bx), sb = min((int)spanB[y], bx + bw - 1);
    for (int x = bx; x < sa; ++x) o[x] = 0;
    for (int x = sb + 1 < bx ? bx : sb + 1; x < bx + bw; ++x) o[x] = 0;
    if (sa > sb) continue;
    int sy = y, xo = 0;                           // 1. 空と床(床は、さざ波で読む所をずらす)
    if (y > iHZ) { sy = clampi(y + ripY[y], iHZ + 1, S - 1); xo = ripX[y]; }
    const uint16_t* a = bA + sy * S;
    if (!wB) {
      if (!xo) memcpy(o + sa, a + sa, (size_t)(sb - sa + 1) * 2);
      else for (int x = sa; x <= sb; ++x) o[x] = a[clampi(x + xo, 0, S - 1)];
    } else {
      const uint16_t* b = bB + sy * S;
      for (int x = sa; x <= sb; ++x) { const int q = clampi(x + xo, 0, S - 1); o[x] = mix565(a[q], b[q], wB); }
    }
    if (y > feetY - 2) {                          // 2. 床に映った脚(さざ波でゆれる)
      const uint16_t* r = layR + clampi(y + ripY[y] / 2, 0, S - 1) * S;
      const int xr = xo / 3;
      for (int x = sa; x <= sb; ++x) { const uint16_t v = r[clampi(x + xr, 0, S - 1)]; if (v) o[x] = addSat565(o[x], v); }
    }
    const uint16_t* lb = layB + y * S;            // 3. 腰から下の輪
    for (int x = sa; x <= sb; ++x) { const uint16_t v = lb[x]; if (v) o[x] = addSat565(o[x], v); }
  }
  // 4. 腰から上の輪と、ほどけた糸(この帯にかかる線だけ)。1歩ごとに、となり合う2つの点へ分けて足す
  const int i0 = bandStart[kb], i1 = bandStart[kb + 1];
  for (int ii = i0; ii < i1; ++ii) {
    const Sg& s = sg[bandIdx[ii]];
    const int dx = s.x1 - s.x0, dy = s.y1 - s.y0;
    const int n = max(1, (max(abs(dx), abs(dy)) + 3) >> 2);
    const int ww = min(256, s.w * 5 / 4);
    const bool xMajor = abs(dx) >= abs(dy);
    for (int i = 0; i <= n; ++i) {
      const int x4 = s.x0 + dx * i / n, y4 = s.y0 + dy * i / n;
      int px, py, f;
      if (xMajor) { px = (x4 + 2) >> 2; py = y4 >> 2; f = y4 & 3; } else { px = x4 >> 2; py = (y4 + 2) >> 2; f = x4 & 3; }
      for (int k = 0; k < 2; ++k) {
        const int qx = xMajor ? px : px + k, qy = xMajor ? py + k : py;
        if (qy < y0 || qy >= y0 + h || qx < spanA[qy] || qx > spanB[qy]) continue;
        const int wgt = k ? f : 4 - f;
        if (!wgt) continue;
        uint16_t* o = out + (qy - y0) * bw + (qx - bx);
        *o = screenAdd(*o, s.r, s.g, s.b, (ww * wgt) >> 2);
      }
    }
  }
  if (txtA > 0.01f && txtMask) {                   // 5. self = null;
    for (int rr = 0; rr < h; ++rr) {
      const int y = y0 + rr;
      if (y < txtY || y >= txtY + txtH) continue;
      uint16_t* o = out + rr * bw - bx;
      const uint8_t* m = txtMask + (y - txtY) * txtW;
      const int a8 = (int)(txtA * 200.0f);
      for (int x = max((int)spanA[y], max(bx, txtX)); x <= min((int)spanB[y], min(bx + bw - 1, txtX + txtW - 1)); ++x) if (m[x - txtX]) o[x] = screenAdd(o[x], 200, 212, 240, (m[x - txtX] * a8) >> 8);
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
  job = 0;
  xSemaphoreGive(semGo);
  uint32_t busy = 0, send = 0;
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
    if (job == 1) { bgRows(buildNo, 1, 2, 0); xSemaphoreGive(semDone); continue; }
    if (job == 3) { buildStreams(1, 2, 0); xSemaphoreGive(semDone); continue; }
    uint32_t busy = 0;
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

// ---------------------------------------------------------------- 表示
static void showMsg(const char* msg, float prog) {
  static M5Canvas* cv = nullptr;
  static const char* text = "";
  const int w = 280, h = 64, x0 = ((W - w) / 2) & ~1, y0 = ((H - h) / 2) & ~1;
  if (msg) {
    text = msg;
    M5.Display.fillScreen(TFT_BLACK);
    if (!cv) { cv = new M5Canvas(&M5.Display); cv->setPsram(true); cv->setColorDepth(16); if (!cv->createSprite(w, h)) { delete cv; cv = nullptr; } }
  }
  if (!cv) return;
  cv->fillScreen(TFT_BLACK);
  cv->setTextDatum(middle_center);
  cv->setFont(&fonts::lgfxJapanGothicP_16);
  cv->setTextColor(M5.Display.color565(170, 190, 215));
  cv->drawString(text, w / 2, 20);
  cv->fillRect(40, 44, w - 80, 4, M5.Display.color565(36, 40, 52));
  cv->fillRect(40, 44, (int)((w - 80) * clamp01(prog)), 4, M5.Display.color565(200, 215, 255));
  cv->pushSprite(x0, y0);
  if (prog >= 1.0f) { delete cv; cv = nullptr; }
}
static void fail(const char* msg) {
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setTextColor(0xF800);
  M5.Display.setTextDatum(middle_center);
  M5.Display.drawString(msg, W / 2, H / 2);
  Serial.println(msg);
  while (true) nmIdle();
}
static void makeText() {
  M5Canvas cv(&M5.Display);
  cv.setColorDepth(16);
  txtW = 200; txtH = 24;
  if (!cv.createSprite(txtW, txtH)) return;
  cv.fillSprite(0);
  cv.setFont(&fonts::lgfxJapanGothicP_20);
  cv.setTextDatum(middle_center);
  cv.setTextColor(0xFFFF);
  cv.drawString("self = null;", txtW / 2, txtH / 2);
  const uint16_t* p = (const uint16_t*)cv.getBuffer();
  txtMask = (uint8_t*)malloc((size_t)txtW * txtH);
  if (!txtMask || !p) { txtMask = nullptr; return; }
  for (int i = 0; i < txtW * txtH; ++i) txtMask[i] = p[i] ? 255 : 0;
  cv.deleteSprite();
  txtX = (S - txtW) / 2; txtY = (int)(S * 0.90f) - txtH / 2;
}

// ---------------------------------------------------------------- setup / loop
__attribute__((cold)) void setup() {   // cold: 1回だけ動くので、小さく作る
  auto cfg = M5.config();
  M5.begin(cfg);
  Serial.begin(115200);
  Serial.println("NullSapiens v3");
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

  HZ = S * 0.409f; FOC = S * 1.05f; iHZ = (int)HZ;
  FC = cosf(FIGROT); FS = sinf(FIGROT);
  { float fx, fy; toScreen(FIGX, 0.0f, 0.0f, fx, fy); feetY = (int)fy; }
  spanA = (int16_t*)malloc(S * 2); spanB = (int16_t*)malloc(S * 2);
  bandX = (int16_t*)malloc(NB * 2); bandW = (int16_t*)malloc(NB * 2);
  ripX = (int8_t*)malloc(S); ripY = (int8_t*)malloc(S);
  sinT = (float*)malloc(1024 * sizeof(float));
  acesT = (float*)malloc(2048 * sizeof(float)); glowT = (float*)malloc(1024 * sizeof(float)); flT = (float*)malloc(1024 * sizeof(float));
  for (int i = 0; i < NSLOT; ++i) band[i] = (uint16_t*)heap_caps_malloc((size_t)S * BAND * 2, MALLOC_CAP_DMA);
  for (int t = 0; t < NTOD; ++t) bg[t] = (uint16_t*)ps_malloc((size_t)S * S * 2);
  layB = (uint16_t*)ps_malloc((size_t)S * S * 2); layR = (uint16_t*)ps_malloc((size_t)S * S * 2);
  lp = (Loop*)ps_malloc(sizeof(Loop) * MAXL);
  stX = (int16_t*)ps_malloc((size_t)MAXP * NCOPY * NS * 2); stY = (int16_t*)ps_malloc((size_t)MAXP * NCOPY * NS * 2);
  ptBr = (uint8_t*)ps_malloc(MAXP); ptHue = (uint8_t*)ps_malloc(MAXP); ptOk = (uint8_t*)ps_malloc(MAXP);
  sg = (Sg*)ps_malloc(sizeof(Sg) * MAXSEG);
  bandIdx = (int32_t*)ps_malloc(sizeof(int32_t) * MAXSEG * 3);
  bandStart = (int32_t*)malloc(sizeof(int32_t) * (NB + 2));
  bool ok = spanA && spanB && bandX && bandW && ripX && ripY && sinT && acesT && glowT && flT && layB && layR && lp && stX && stY && ptBr && ptHue && ptOk && sg && bandIdx && bandStart;
  for (int i = 0; i < NSLOT; ++i) ok = ok && band[i];
  for (int t = 0; t < NTOD; ++t) ok = ok && bg[t];
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
  for (int i = 0; i < 1024; ++i) sinT[i] = sinf(i * 6.2831853f / 1024.0f);
  for (int i = 0; i < 256; ++i) {                  // 薄い膜のような虹色
    const float t = i / 256.0f;
    iriR[i] = (uint8_t)(255 * (0.5f + 0.5f * cosf(6.2831853f * t)));
    iriG[i] = (uint8_t)(255 * (0.5f + 0.5f * cosf(6.2831853f * (t + 0.33f))));
    iriB[i] = (uint8_t)(255 * (0.5f + 0.5f * cosf(6.2831853f * (t + 0.67f))));
  }
  buildBgTables();
  buildSegTables();
  imuOK = TILT && M5.Imu.isEnabled();
  semGo = xSemaphoreCreateBinary();
  semDone = xSemaphoreCreateBinary();
  if (!semGo || !semDone) fail("Memory error (task)");
  xTaskCreatePinnedToCore(workerTask, "sapiens", 6144, nullptr, 1, nullptr, 0);

  const uint32_t t0 = millis();
  showMsg("光の線を用意しています…", 0.0f);
  for (int t = 0; t < NTOD; ++t) {                 // 空と床(2つのコアで1行おきに)
    buildNo = t; job = 1;
    xSemaphoreGive(semGo);
    bgRows(t, 0, 2, 1);
    xSemaphoreTake(semDone, portMAX_DELAY);
    showMsg(nullptr, (t + 1) / 6.0f);
  }
  const uint32_t t1 = millis();
  {
    uint16_t* acc = (uint16_t*)ps_malloc((size_t)S * S * 3 * 2);
    if (!acc) fail("Memory error (lines)");
    buildStatic(acc);
    free(acc);
  }
  showMsg(nullptr, 4.0f / 6.0f);
  const uint32_t t2 = millis();
  buildLoops();
  showMsg(nullptr, 5.0f / 6.0f);
  job = 3; xSemaphoreGive(semGo);                  // 道すじ(2つのコアで1点おきに)
  buildStreams(0, 2, 1);
  xSemaphoreTake(semDone, portMAX_DELAY);
  makeText();
  showMsg(nullptr, 1.0f);
  Serial.printf("build: %u ms (sky %u, lower body %u, upper body %u)  loops %d, points %d\n", (unsigned)(millis() - t0), (unsigned)(t1 - t0), (unsigned)(t2 - t1), (unsigned)(millis() - t2), nLoop, nPt);
  Serial.printf("time of day: %s\n", TOD_NAME[tod]);
  rnd = (uint32_t)esp_random() | 1u;
  tod = todFrom = TOD_START; transT = 1.0f;
  st = ST_UNRAVEL; hD = 1.62f; hTarget = H_NORMAL; hSpeed = (H_TOP - H_NORMAL) / UNRAVEL_SEC;   // 始めから少しほどけている
  stepTod(0.0f);
  simulate(0.0f);
}

void loop() {
  static uint32_t last = millis(), fpsT = millis(), lastTapMs = 0;
  static int frames = 0;
  static bool was = false;
  static uint32_t tPrev = micros(), sumIn = 0, sumTilt = 0, sumSim = 0, sumDraw = 0, sumAll = 0;
  const uint32_t t0 = micros();
  sumAll += t0 - tPrev; tPrev = t0;
  M5.update();
  auto t = M5.Touch.getDetail();
  sumIn += micros() - t0;
  uint32_t now = millis();
  const bool corner = !roundScr && t.x < 56 && t.y < 56;
  bool dbl = false;
  if (roundScr) {
    if (M5.BtnA.wasClicked()) startTod((tod + 1) % NTOD);
    if (M5.BtnB.wasClicked()) { speedIdx = (speedIdx + 1) % 3; Serial.printf("speed: x%.1f\n", SPEEDS[speedIdx]); }
  } else {
    if (M5.BtnPWR.wasClicked()) startTod((tod + 1) % NTOD);
    if (t.wasPressed() && !corner) {
      if (now - lastTapMs < 350) { speedIdx = (speedIdx + 1) % 3; Serial.printf("speed: x%.1f\n", SPEEDS[speedIdx]); lastTapMs = 0; dbl = true; } else lastTapMs = now;
    }
  }
  float dt = (now - last) / 1000.0f; last = now;
  if (dt > 0.1f) dt = 0.1f;

  const uint32_t tTilt0 = micros();
  static float shakeAcc = 0, pAx = 0, pAy = 0, pAz = 1;
  static uint32_t shakeMs = 0;
  if (imuOK) {
    float ax, ay, az;
    M5.Imu.update();
    if (M5.Imu.getAccel(&ax, &ay, &az)) {
      const float j = fabsf(ax - pAx) + fabsf(ay - pAy) + fabsf(az - pAz);
      pAx = ax; pAy = ay; pAz = az;
      shakeAcc = shakeAcc * 0.75f + j;
      if (shakeAcc > 2.2f && now - shakeMs > 3000) { shakeAll(); shakeMs = now; shakeAcc = 0; }
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
  sumTilt += micros() - tTilt0;

  static int pressX = 0, pressY = 0;
  static bool moved = false;
  const int tx = t.x - XOFF, ty = t.y - YOFF;
  const bool touching = t.isPressed() && !corner && tx >= 0 && ty >= 0 && tx < S && ty < S;
  if (touching && !was) { pressX = tx; pressY = ty; moved = false; }
  if (touching && was && ((tx - pressX) * (tx - pressX) + (ty - pressY) * (ty - pressY)) > (int)(S * S * 0.002f)) moved = true;
  if (!touching && was && !moved && !dbl) {        // タップ: ほどける / 巻き戻る
    if (st == ST_REST || st == ST_RETRACT) { startUnravel(H_NORMAL, UNRAVEL_SEC * 0.6f); Serial.println("goodbye"); }
    else { st = ST_RETRACT; hSpeed = (H_TOP - hD) / (RETRACT_SEC * 0.6f); Serial.println("new Human()"); }
  }
  was = touching;

  const uint32_t tSim0 = micros();
  stepTod(dt);
  simulate(dt);
  buildSegs();
  const uint32_t tDraw0 = micros();
  sumSim += tDraw0 - tSim0;
  renderFrame();
  sumDraw += micros() - tDraw0;
  if (++frames, now - fpsT >= 3000) {
    Serial.printf("fps: %.1f  (%s, x%.1f, lines %d, h %.2f)  core0 %.1f ms  core1 %.1f ms  send %.1f ms\n", frames * 1000.0f / (now - fpsT), TOD_NAME[tod], SPEEDS[speedIdx],
                  nSg, hD, busyUs[0] / 1000.0f, busyUs[1] / 1000.0f, sendUs / 1000.0f);
    {
      const float k = 1.0f / (1000.0f * frames);
      const float a = sumAll * k, in = sumIn * k, ti = sumTilt * k, si = sumSim * k, dr = sumDraw * k;
      Serial.printf("time: input %.1f  tilt %.1f  sim %.1f  draw %.1f  other %.1f  (frame %.1f ms)\n", in, ti, si, dr, fmaxf(0.0f, a - in - ti - si - dr), a);
      sumIn = sumTilt = sumSim = sumDraw = sumAll = 0;
    }
    frames = 0; fpsT = now;
  }
}

// 共通の置き場に置いたものは、0 で始まる単純な型だけ(コンストラクタが要らないもの)
static_assert(std::is_trivially_default_constructible<uint8_t>::value, "NM_ARENA: uint8_t");
static constexpr size_t OVL_END = nmAlignTo(nmOv6, 16);
static_assert(OVL_END <= NM_ARENA_SIZE, "NM_ARENA_SIZE (nm_common.h) を大きくしてください");
extern const size_t OVL_SIZE = OVL_END;   // 起動時の確認用
}  // namespace NullSapiens
