// NullButterfly  -  NullMenu 用モジュール(元のスケッチ NullButterfly_v6 を名前空間に入れたもの)
// 0 で始まる変数(配列も)は、ほかの作品と共通の置き場(NM_ARENA)に置いています。
// 一度に動くのは1つだけ(切り替えるときは再起動)なので、重なっていても問題ありません。
#include "nm_common.h"

#pragma GCC optimize("O3")

namespace NullButterfly {
static constexpr size_t nmOv0 = 0;

// NullButterfly v6  -  暗い空に、たくさんの蝶が舞う(NullSutra v2 の蝶だけを取り出した版)
//   M5Stack StopWatch 用(丸い 466x466 画面)。CoreS3 系でも画面いっぱいに動きます。
//
// 画面の中:
//   ・奥・中・手前の3つの層に分かれた蝶(ふつう100匹)が、見えない風のうずに乗って、ゆらゆら舞う
//   ・奥の蝶ほど小さく暗く、手前の蝶ほど大きい
//   ・蝶の通ったあとに、光る鱗粉がこぼれて消えていく
//   ・ときどき群れがひとところに集まって、大きな輪になって回り、またほどける
//
// 触ると:
//   ・タップ        : そこから蝶がいっせいに飛び散る(鱗粉もはじける)
//   ・なぞる        : 指の近くの蝶が、指の動きに巻き込まれる
//   ・押さえたまま  : 蝶が指のまわりに集まってきて、渦になって回る。離すと、ぱっと散る
//
// 操作:
//   StopWatch … ボタンA 短押し: 蝶の数(5 / 15 / 25 / 50 / 100)  ボタンA 長押し: 色(瑠璃 / 白銀 / 金 / 紅)
//               ボタンB: 明るさ
//   CoreS3系  … 画面の上半分をダブルタップ: 蝶の数   下半分をダブルタップ: 色
//               電源ボタン短押し: 明るさ
//
// 必要なもの:
//   ボードマネージャー M5Stack >= 3.3.7 / ボード: M5StopWatch(CoreS3 なら M5CoreS3)
//   ライブラリ M5Unified >= 0.2.15, M5GFX >= 0.2.21
//   (文字は使わないので、フォントは要りません)
//
// しくみ:
//   蝶の絵は、起動時に「羽ばたきの8コマ × 大きさ3段」を描いておき(色は番号で持つ)、
//   毎フレームそれを回して・縮めて貼るだけにしています。だから数を増やしても軽い。
//   色は「奥行き × 色違い × コマ」ごとの小さな色の表で決まるので、色の切り替えは表を作り直すだけ。
//   風は「カールノイズ」(うずの関数を微分した、湧き出しも吸い込みもない流れ)を式で計算します。
//   画面は8行ずつの帯に分けて、2つのコアで交互に描きます。



// Arduino IDE は関数の宣言を自動でこのあたりに足すので、型の名前だけ先に宣言しておく
struct Fly; struct Dust; struct DustDraw; struct Col; struct Theme; struct Input;

// ---- 調整用 ----------------------------------------------------------------
static constexpr int   NCOUNT      = 5;
static const int       COUNTS[NCOUNT] = {5, 15, 25, 50, 100};  // 蝶の数の段階(ボタンA 短押しで切り替え)
static constexpr int   COUNT_START = 4;       // 最初の数(0: 5匹 / 1: 15匹 / 2: 25匹 / 3: 50匹 / 4: 100匹)
static constexpr float SIZE_FAR    = 0.034f;  // 奥の蝶の大きさ(画面に対する割合)
static constexpr float SIZE_MID    = 0.050f;  // 中の蝶
static constexpr float SIZE_NEAR   = 0.072f;  // 手前の蝶
static constexpr float FLOW_SPEED  = 0.08f;   // 風のうずに乗って進む速さ(画面/秒)
static constexpr float MEANDER     = 0.95f;   // 左右に蛇行する大きさ(ラジアン。大きいほど、くねくね飛ぶ)
static constexpr float MEANDER_HZ  = 0.25f;   // 蛇行の速さ(1秒あたりの往復、だいたい)
static constexpr float BOB         = 0.16f;   // 羽ばたきごとの浮き沈み(蝶の大きさに対する割合)
static constexpr float GLIDE       = 0.30f;   // 滑空する時間の割合(0 で滑空しない)
static constexpr float FLAP_HZ     = 3.0f;    // 羽ばたきの回数(1秒あたり、だいたい)
static constexpr float DUST_RATE   = 3.0f;    // 鱗粉のこぼれる量
static constexpr float SWARM_EVERY = 16.0f;   // 群れが集まって輪になる間隔(秒、だいたい)
// ---------------------------------------------------------------------------

static constexpr int BAND    = 8;
static constexpr int NSLOT   = 4;
static constexpr int MAXFLY  = 100;
static constexpr int MAXDUST = 900;
static constexpr int MAXNB   = 64;
static constexpr int NFR     = 8;             // 羽ばたきのコマ数
static constexpr int NMIP    = 3;             // 蝶の絵の大きさの段数
static const int     MIPT[NMIP] = {96, 48, 24};
static constexpr int NPIDX   = 132;           // 色の番号の数: 0 透明 / 1..128 羽 / 129 胴 / 130 ふち / 131 白い点
static constexpr int BGSH    = 2;             // 背景は 4x4 ピクセルごとに計算

static constexpr size_t nmOa0 = nmAlignTo(nmOv0, alignof(int)), nmOv1 = nmOa0 + sizeof(int); static int& W = *reinterpret_cast<int*>(NM_ARENA + nmOa0);
static constexpr size_t nmOa1 = nmAlignTo(nmOv1, alignof(int)), nmOv2 = nmOa1 + sizeof(int); static int& H = *reinterpret_cast<int*>(NM_ARENA + nmOa1);
static constexpr size_t nmOa2 = nmAlignTo(nmOv2, alignof(int)), nmOv3 = nmOa2 + sizeof(int); static int& S = *reinterpret_cast<int*>(NM_ARENA + nmOa2);
static constexpr size_t nmOa3 = nmAlignTo(nmOv3, alignof(int)), nmOv4 = nmOa3 + sizeof(int); static int& XOFF = *reinterpret_cast<int*>(NM_ARENA + nmOa3);
static constexpr size_t nmOa4 = nmAlignTo(nmOv4, alignof(int)), nmOv5 = nmOa4 + sizeof(int); static int& YOFF = *reinterpret_cast<int*>(NM_ARENA + nmOa4);
static constexpr size_t nmOa5 = nmAlignTo(nmOv5, alignof(int)), nmOv6 = nmOa5 + sizeof(int); static int& NB = *reinterpret_cast<int*>(NM_ARENA + nmOa5);
static constexpr size_t nmOa6 = nmAlignTo(nmOv6, alignof(int)), nmOv7 = nmOa6 + sizeof(int); static int& BW = *reinterpret_cast<int*>(NM_ARENA + nmOa6);
static constexpr size_t nmOa7 = nmAlignTo(nmOv7, alignof(float)), nmOv8 = nmOa7 + sizeof(float); static float& Sc = *reinterpret_cast<float*>(NM_ARENA + nmOa7);
static constexpr size_t nmOa8 = nmAlignTo(nmOv8, alignof(float)), nmOv9 = nmOa8 + sizeof(float); static float& viewR = *reinterpret_cast<float*>(NM_ARENA + nmOa8);
static constexpr size_t nmOa9 = nmAlignTo(nmOv9, alignof(bool)), nmOv10 = nmOa9 + sizeof(bool); static bool& isRound = *reinterpret_cast<bool*>(NM_ARENA + nmOa9);
static constexpr size_t nmOa10 = nmAlignTo(nmOv10, alignof(float)), nmOv11 = nmOa10 + sizeof(float); static float& vx0 = *reinterpret_cast<float*>(NM_ARENA + nmOa10);
static constexpr size_t nmOa11 = nmAlignTo(nmOv11, alignof(float)), nmOv12 = nmOa11 + sizeof(float); static float& vx1 = *reinterpret_cast<float*>(NM_ARENA + nmOa11);
static constexpr size_t nmOa12 = nmAlignTo(nmOv12, alignof(float)), nmOv13 = nmOa12 + sizeof(float); static float& vy0 = *reinterpret_cast<float*>(NM_ARENA + nmOa12);
static constexpr size_t nmOa13 = nmAlignTo(nmOv13, alignof(float)), nmOv14 = nmOa13 + sizeof(float); static float& vy1 = *reinterpret_cast<float*>(NM_ARENA + nmOa13);  // 見えている範囲(S の座標で)
static uint8_t* bandVis;

static uint16_t* slotBuf[NSLOT];
static uint8_t*  atlas[NMIP];                 // [NFR][T*T] 色の番号
static constexpr size_t nmOa14 = nmAlignTo(nmOv14, 16), nmOv15 = nmOa14 + sizeof(float[NMIP]); static float (&mipR0)[NMIP] = *reinterpret_cast<float(*)[NMIP]>(NM_ARENA + nmOa14);  // 絵の中での蝶の大きさ(半径)
static uint16_t* palBuf;                      // [(奥行き*2+色違い)*NFR + コマ][NPIDX]
static uint16_t* bgLow;                       // [BW*BW] 背景(粗い)
static int16_t*  spanS;
static int16_t*  spanE;
static int16_t*  bandX0;
static int16_t*  bandW;
static constexpr size_t nmOa15 = nmAlignTo(nmOv15, 16), nmOv16 = nmOa15 + sizeof(uint32_t[2]); static uint32_t (&busyUs)[2] = *reinterpret_cast<uint32_t(*)[2]>(NM_ARENA + nmOa15);
static constexpr size_t nmOa16 = nmAlignTo(nmOv16, alignof(uint16_t)), nmOv17 = nmOa16 + sizeof(uint16_t); static uint16_t& blackCol = *reinterpret_cast<uint16_t*>(NM_ARENA + nmOa16);

static SemaphoreHandle_t semGo, semDone;
static volatile int readyW = -1, doneUpTo = -1;   // コア0が描き終えた帯 / 画面へ送り終えた帯
static int fbState = -1;  // まだ調べていない / 0: 直接送る / 1: StopWatch の置き場あり
static constexpr size_t nmOa17 = nmAlignTo(nmOv17, alignof(bool)), nmOv18 = nmOa17 + sizeof(bool); static bool& fbMode = *reinterpret_cast<bool*>(NM_ARENA + nmOa17);  // StopWatch: 帯ごとに丸の中の幅だけ送る
static int SPLIT = 2;  // コア1は SPLIT 本に1本の帯を描く(残りはコア0)
static constexpr size_t nmOa18 = nmAlignTo(nmOv18, alignof(uint32_t)), nmOv19 = nmOa18 + sizeof(uint32_t); static uint32_t& sendUs = *reinterpret_cast<uint32_t*>(NM_ARENA + nmOa18);

// ---- 蝶
struct Fly {
  float x, y, vx, vy;         // 位置と速さ(ピクセル、ピクセル/秒)
  float ph, hz, head;         // 羽ばたきの位相・回数、向き
  float R, seed;              // 大きさ(半径)、個性
  float orbR, orbW, orbA;     // 集まったときに回る輪の大きさ・速さ・位置
  float mW, mA, mP;           // 蛇行の速さ・幅・位置
  float flapT; bool glide;    // 羽ばたく残り時間 / 滑空中か
    uint8_t cls, var;           // 奥行き(0 奥 / 1 中 / 2 手前)、色違い
  // 描くための値(フレームの準備で決め、描いている間は読むだけ)
  float cx, cy, rr;           // 中心と、描く円の半径
  float dxx, dxy, dyx, dyy;   // 画面が1ピクセル進むと、絵の中でどれだけ進むか
  int   y0, y1, T;
  const uint8_t*  tex;
  const uint16_t* pal;
};
static constexpr size_t nmOa19 = nmAlignTo(nmOv19, 16), nmOv20 = nmOa19 + sizeof(Fly[MAXFLY]); static Fly (&fly)[MAXFLY] = *reinterpret_cast<Fly(*)[MAXFLY]>(NM_ARENA + nmOa19);
static constexpr size_t nmOa20 = nmAlignTo(nmOv20, 16), nmOv21 = nmOa20 + sizeof(int[MAXFLY]); static int (&order)[MAXFLY] = *reinterpret_cast<int(*)[MAXFLY]>(NM_ARENA + nmOa20);  // 奥から手前の順
static int countIdx = COUNT_START;
static constexpr size_t nmOa21 = nmAlignTo(nmOv21, alignof(int)), nmOv22 = nmOa21 + sizeof(int); static int& nFly = *reinterpret_cast<int*>(NM_ARENA + nmOa21);
static constexpr size_t nmOa22 = nmAlignTo(nmOv22, 16), nmOv23 = nmOa22 + sizeof(float[MAXFLY]); static float (&sepX)[MAXFLY] = *reinterpret_cast<float(*)[MAXFLY]>(NM_ARENA + nmOa22);
static constexpr size_t nmOa23 = nmAlignTo(nmOv23, 16), nmOv24 = nmOa23 + sizeof(float[MAXFLY]); static float (&sepY)[MAXFLY] = *reinterpret_cast<float(*)[MAXFLY]>(NM_ARENA + nmOa23);

// ---- 鱗粉
struct Dust { float x, y, vx, vy, life, maxL, tw; uint8_t cls; };
struct DustDraw { int16_t x, y; uint16_t c, ch; uint8_t plus; };
static constexpr size_t nmOa24 = nmAlignTo(nmOv24, 16), nmOv25 = nmOa24 + sizeof(Dust[MAXDUST]); static Dust (&dust)[MAXDUST] = *reinterpret_cast<Dust(*)[MAXDUST]>(NM_ARENA + nmOa24);
static constexpr size_t nmOa25 = nmAlignTo(nmOv25, 16), nmOv26 = nmOa25 + sizeof(DustDraw[MAXDUST]); static DustDraw (&dd)[MAXDUST] = *reinterpret_cast<DustDraw(*)[MAXDUST]>(NM_ARENA + nmOa25);
static constexpr size_t nmOa26 = nmAlignTo(nmOv26, alignof(int)), nmOv27 = nmOa26 + sizeof(int); static int& nDD = *reinterpret_cast<int*>(NM_ARENA + nmOa26);
static constexpr size_t nmOa27 = nmAlignTo(nmOv27, alignof(int)), nmOv28 = nmOa27 + sizeof(int); static int& dustNext = *reinterpret_cast<int*>(NM_ARENA + nmOa27);
static constexpr size_t nmOa28 = nmAlignTo(nmOv28, 16), nmOv29 = nmOa28 + sizeof(int16_t[MAXDUST]); static int16_t (&ddOrd)[MAXDUST] = *reinterpret_cast<int16_t(*)[MAXDUST]>(NM_ARENA + nmOa28);
static constexpr size_t nmOa29 = nmAlignTo(nmOv29, 16), nmOv30 = nmOa29 + sizeof(int16_t[MAXNB + 1]); static int16_t (&ddStart)[MAXNB + 1] = *reinterpret_cast<int16_t(*)[MAXNB + 1]>(NM_ARENA + nmOa29);
static constexpr size_t nmOa30 = nmAlignTo(nmOv30, 16), nmOv31 = nmOa30 + sizeof(uint16_t[3][16]); static uint16_t (&dustLut)[3][16] = *reinterpret_cast<uint16_t(*)[3][16]>(NM_ARENA + nmOa30);
static constexpr size_t nmOa31 = nmAlignTo(nmOv31, 16), nmOv32 = nmOa31 + sizeof(uint16_t[3][16]); static uint16_t (&dustLutH)[3][16] = *reinterpret_cast<uint16_t(*)[3][16]>(NM_ARENA + nmOa31);  // 足し算する色(バイト順はそのまま)

static constexpr size_t nmOa32 = nmAlignTo(nmOv32, alignof(float)), nmOv33 = nmOa32 + sizeof(float); static float& simT = *reinterpret_cast<float*>(NM_ARENA + nmOa32);
static float autoT = 8.0f;
static constexpr size_t nmOa33 = nmAlignTo(nmOv33, alignof(float)), nmOv34 = nmOa33 + sizeof(float); static float& fPX = *reinterpret_cast<float*>(NM_ARENA + nmOa33);
static constexpr size_t nmOa34 = nmAlignTo(nmOv34, alignof(float)), nmOv35 = nmOa34 + sizeof(float); static float& fPY = *reinterpret_cast<float*>(NM_ARENA + nmOa34);
static constexpr size_t nmOa35 = nmAlignTo(nmOv35, alignof(float)), nmOv36 = nmOa35 + sizeof(float); static float& stillT = *reinterpret_cast<float*>(NM_ARENA + nmOa35);
static constexpr size_t nmOa36 = nmAlignTo(nmOv36, alignof(float)), nmOv37 = nmOa36 + sizeof(float); static float& holdW = *reinterpret_cast<float*>(NM_ARENA + nmOa36);
static constexpr size_t nmOa37 = nmAlignTo(nmOv37, alignof(float)), nmOv38 = nmOa37 + sizeof(float); static float& autoW = *reinterpret_cast<float*>(NM_ARENA + nmOa37);
static constexpr size_t nmOa38 = nmAlignTo(nmOv38, alignof(float)), nmOv39 = nmOa38 + sizeof(float); static float& touchGlow = *reinterpret_cast<float*>(NM_ARENA + nmOa38);
static constexpr size_t nmOa39 = nmAlignTo(nmOv39, alignof(bool)), nmOv40 = nmOa39 + sizeof(bool); static bool& fWas = *reinterpret_cast<bool*>(NM_ARENA + nmOa39);
static constexpr size_t nmOa40 = nmAlignTo(nmOv40, alignof(bool)), nmOv41 = nmOa40 + sizeof(bool); static bool& autoOn = *reinterpret_cast<bool*>(NM_ARENA + nmOa40);

static constexpr size_t nmOa41 = nmAlignTo(nmOv41, alignof(int)), nmOv42 = nmOa41 + sizeof(int); static int& theme = *reinterpret_cast<int*>(NM_ARENA + nmOa41);
static const uint8_t brightSteps[] = {60, 120, 190, 255};
static int brightIdx = 2;

// ---------------------------------------------------------------- 道具
struct Col { float r, g, b; };
static inline float clamp01(float v) { return v < 0 ? 0 : (v > 1 ? 1 : v); }
static inline float smooth3(float t) { t = clamp01(t); return t * t * (3 - 2 * t); }
static inline Col lerpC(Col a, Col b, float t) {
  return {a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t};
}
static inline Col mulC(Col a, float k) { return {a.r * k, a.g * k, a.b * k}; }
static inline float soft(float v) { return v <= 0 ? 0 : v * 1.22f / (1.0f + 0.22f * v); }   // 明るすぎる所をやわらかく
static inline uint16_t to565(Col c) {
  return M5.Display.color565(clamp01(soft(c.r)) * 255, clamp01(soft(c.g)) * 255, clamp01(soft(c.b)) * 255);
}
static inline uint16_t toPix(Col c) {
  return __builtin_bswap16(to565(c));         // 色がおかしい場合はこの bswap を外す(addPix の中も)
}
// 画面の色に光を足す(明るさがあふれないように、色ごとに頭打ち)
static inline uint16_t addPix(uint16_t dst, uint16_t add) {
  uint32_t d = __builtin_bswap16(dst);
  uint32_t r = (d & 0xF800) + (add & 0xF800); if (r > 0xF800) r = 0xF800;
  uint32_t g = (d & 0x07E0) + (add & 0x07E0); if (g > 0x07E0) g = 0x07E0;
  uint32_t b = (d & 0x001F) + (add & 0x001F); if (b > 0x001F) b = 0x001F;
  return __builtin_bswap16((uint16_t)(r | g | b));
}
static inline float frand() { return (esp_random() & 0xFFFF) / 65535.0f; }
static inline float frand(float a, float b) { return a + (b - a) * frand(); }
static inline float wrapPi(float a) {
  while (a > 3.14159265f) a -= 6.2831853f;
  while (a < -3.14159265f) a += 6.2831853f;
  return a;
}

// ---------------------------------------------------------------- 色の組み合わせ
struct Theme {
  const char* name;
  Col root, deep, bright, pale;     // 羽: 付け根 / 深い色 / 光る色 / 白っぽい照り
  Col deep2, bright2;               // 色違いの蝶
  Col edge, dot;                    // ふちと、前ばねの先の点
  Col fog;                          // 奥の蝶が溶けこむ色
  Col bg0, bg1, glowA, glowB;       // 背景: いちばん暗い色 / まん中の明るみ / 漂う光2色
  Col dust;                         // 鱗粉
};
static const Theme THEMES[] = {
  {"ruri (lapis)",
   {0.02f, 0.04f, 0.16f}, {0.06f, 0.22f, 0.70f}, {0.35f, 0.72f, 1.00f}, {0.78f, 0.93f, 1.00f},
   {0.18f, 0.12f, 0.66f}, {0.52f, 0.58f, 1.00f},
   {0.02f, 0.02f, 0.05f}, {0.80f, 0.85f, 0.95f}, {0.02f, 0.05f, 0.14f},
   {0.003f, 0.005f, 0.014f}, {0.010f, 0.020f, 0.052f}, {0.04f, 0.11f, 0.32f}, {0.11f, 0.05f, 0.28f},
   {0.45f, 0.75f, 1.00f}},
  {"hakugin (silver)",
   {0.12f, 0.12f, 0.16f}, {0.45f, 0.48f, 0.58f}, {0.85f, 0.88f, 0.95f}, {1.00f, 1.00f, 1.00f},
   {0.48f, 0.43f, 0.60f}, {0.90f, 0.84f, 0.98f},
   {0.05f, 0.05f, 0.07f}, {0.95f, 0.95f, 1.00f}, {0.05f, 0.06f, 0.10f},
   {0.004f, 0.005f, 0.008f}, {0.016f, 0.018f, 0.028f}, {0.11f, 0.13f, 0.19f}, {0.15f, 0.12f, 0.19f},
   {0.85f, 0.88f, 1.00f}},
  {"kin (gold)",
   {0.14f, 0.05f, 0.00f}, {0.62f, 0.30f, 0.03f}, {1.00f, 0.70f, 0.18f}, {1.00f, 0.95f, 0.70f},
   {0.70f, 0.20f, 0.02f}, {1.00f, 0.55f, 0.10f},
   {0.04f, 0.02f, 0.00f}, {1.00f, 0.95f, 0.80f}, {0.08f, 0.04f, 0.01f},
   {0.008f, 0.004f, 0.002f}, {0.028f, 0.014f, 0.005f}, {0.28f, 0.13f, 0.03f}, {0.24f, 0.06f, 0.02f},
   {1.00f, 0.78f, 0.38f}},
  {"beni (crimson)",
   {0.14f, 0.00f, 0.04f}, {0.60f, 0.04f, 0.16f}, {1.00f, 0.30f, 0.42f}, {1.00f, 0.82f, 0.86f},
   {0.55f, 0.04f, 0.40f}, {1.00f, 0.40f, 0.75f},
   {0.04f, 0.00f, 0.02f}, {1.00f, 0.90f, 0.92f}, {0.08f, 0.01f, 0.04f},
   {0.008f, 0.002f, 0.004f}, {0.028f, 0.005f, 0.012f}, {0.28f, 0.03f, 0.09f}, {0.20f, 0.03f, 0.20f},
   {1.00f, 0.58f, 0.68f}},
};
static constexpr int NTHEME = sizeof(THEMES) / sizeof(THEMES[0]);
static const float DEPTH_K[3] = {0.42f, 0.72f, 1.00f};   // 奥ほど暗く
static const float DEPTH_FOG[3] = {0.35f, 0.12f, 0.0f};  // 奥ほど背景に溶ける
static const float SPEED_K[3] = {0.80f, 1.00f, 1.15f};   // 手前ほど少し速く見える

// 色の表: 奥行き3 × 色違い2 × 羽ばたき8コマ。羽がこちらを向く(開く)ほど明るく照る
static void buildPalettes() {
  const Theme& th = THEMES[theme];
  for (int cls = 0; cls < 3; ++cls)
    for (int var = 0; var < 2; ++var)
      for (int fr = 0; fr < NFR; ++fr) {
        uint16_t* P = palBuf + ((cls * 2 + var) * NFR + fr) * NPIDX;
        const float flap = 0.14f + 0.86f * fr / (NFR - 1);
        const float glint = 0.50f + 0.62f * flap;
        const float k = DEPTH_K[cls], fog = DEPTH_FOG[cls];
        const Col dp = var ? th.deep2 : th.deep, br = var ? th.bright2 : th.bright;
        auto fin = [&](Col c) { return toPix(mulC(lerpC(c, th.fog, fog), k)); };
        P[0] = 0;
        for (int a = 0; a < 16; ++a)
          for (int b = 0; b < 8; ++b) {
            const float t = a / 15.0f, fromBody = b / 7.0f;
            Col c = lerpC(dp, br, smooth3(1.0f - t));
            c = lerpC(c, th.pale, 0.45f * (1.0f - t) * (1.0f - t) * flap);
            c = lerpC(th.root, c, smooth3(fromBody));
            P[1 + a * 8 + b] = fin(mulC(c, glint));
          }
        P[129] = fin({0.04f, 0.03f, 0.05f});
        P[130] = fin(th.edge);
        P[131] = fin(mulC(th.dot, 0.7f + 0.3f * flap));
      }
  for (int cls = 0; cls < 3; ++cls)
    for (int l = 0; l < 16; ++l) {
      const float v = l / 15.0f * (cls == 0 ? 0.30f : (cls == 1 ? 0.55f : 0.95f));
      dustLut[cls][l]  = to565(mulC(th.dust, v));
      dustLutH[cls][l] = to565(mulC(th.dust, v * 0.40f));
    }
}

// ---------------------------------------------------------------- 蝶の絵(最初に1回だけ)
// 羽の形は NullSutra v2 と同じ: 大きな前ばね + 丸い後ばね(楕円を少し傾けて重ねる)。
// 羽ばたきで横方向がつぶれる(横から見た向きになる)。絵の中では、頭が上を向いている
static void drawAtlas(uint8_t* dst, int T, float flap) {
  const float h = T * 0.5f, R0 = h / 1.15f, iR = 1.0f / R0, lim2 = h * h;
  const float bodyW = fmaxf(0.06f, 0.55f * iR), antW = fmaxf(0.022f, 0.45f * iR), iflap = 1.0f / flap;
  const bool detail = T >= 48;
  for (int j = 0; j < T; ++j)
    for (int i = 0; i < T; ++i) {
      const float px = i + 0.5f - h, py = j + 0.5f - h;
      uint8_t o = 0;
      if (px * px + py * py <= lim2) {
        const float u = -py * iR, v = px * iR, av = fabsf(v);   // u: 前へ、v: 横へ
        const float vv = av * iflap;
        const float fu = u - 0.22f, fv = vv - 0.48f;
        const float fa = fu * 0.581f + fv * 0.814f, fb = -fu * 0.814f + fv * 0.581f;     // 前ばね(約 54° 回す)
        const float e1 = fa * fa * 2.7778f + fb * fb * 11.111f;
        const float hu = u + 0.16f, hv = vv - 0.36f;
        const float ha = hu * -0.666f + hv * 0.746f, hb = -hu * 0.746f - hv * 0.666f;    // 後ばね(約 132° 回す)
        const float e2 = ha * ha * 6.25f + hb * hb * 11.111f;
        const float e = e1 < e2 ? e1 : e2;
        if (av < bodyW && u > -0.52f && u < 0.42f) o = 129;                            // 胴
        else if (e < 1.0f && vv >= 0.04f) {
          if (e > 0.82f) o = (detail && e1 < e2 && fa > 0.12f && e > 0.87f && e < 0.95f && ((int)(vv * 9.0f) & 1)) ? 131 : 130;
          else {
            int a = (int)(e * 18.29f + 0.5f); if (a > 15) a = 15;
            const float fb2 = vv * 20.0f + 0.5f;
            o = (uint8_t)(1 + a * 8 + (fb2 > 7.0f ? 7 : (int)fb2));
          }
        } else if (detail && u > 0.40f && u < 0.74f) {                                   // 触角(先が少し太い)
          const float want = 0.05f + (u - 0.40f) * 0.55f;
          const float w = u > 0.68f ? antW * 1.8f : antW;
          if (fabsf(av - want) < w) o = 129;
        }
      }
      dst[j * T + i] = o;
    }
}

static void buildAtlas() {
  for (int m = 0; m < NMIP; ++m) {
    const int T = MIPT[m];
    mipR0[m] = T * 0.5f / 1.15f;
    for (int fr = 0; fr < NFR; ++fr) drawAtlas(atlas[m] + (size_t)fr * T * T, T, 0.14f + 0.86f * fr / (NFR - 1));
  }
}

// ---------------------------------------------------------------- 蝶の出し入れ
static void emitDust(float x, float y, float vx, float vy, int cls, float life) {
  Dust& d = dust[dustNext];
  dustNext = (dustNext + 1) % MAXDUST;
  d = {x, y, vx, vy, life, life, frand(0, 6.2832f), (uint8_t)cls};
}
static void burstDust(float x, float y, int n, float spd, int cls) {
  for (int i = 0; i < n; ++i) {
    float a = frand(0, 6.2832f), s = spd * frand(0.2f, 1.0f);
    emitDust(x, y, cosf(a) * s, sinf(a) * s, cls, frand(0.6f, 1.6f));
  }
}

// 奥行きの割り当て: 20匹ごとに 奥9 / 中7 / 手前4。少ない数でも3つの層が混ざるよう、順番を散らしてある
static const uint8_t CLS_PAT[20] = {2,0,1,0,1, 0,2,0,1,0, 1,0,2,0,1, 0,1,1,2,0};
static inline int clsOf(int i) { return CLS_PAT[i % 20]; }

static void spawnFly(int i) {
  Fly& f = fly[i];
  f.cls = (uint8_t)clsOf(i);
  f.var = (uint8_t)((i * 7 / 3) % 3 == 0 ? 1 : 0);          // 3匹に1匹くらいは色違い
  const float base = f.cls == 0 ? SIZE_FAR : (f.cls == 1 ? SIZE_MID : SIZE_NEAR);
  f.R = base * S * frand(0.85f, 1.12f);
  for (int t = 0; t < 20; ++t) {                            // 見えている所に置く
    f.x = frand(vx0 + 0.1f * (vx1 - vx0), vx1 - 0.1f * (vx1 - vx0));
    f.y = frand(vy0 + 0.1f * (vy1 - vy0), vy1 - 0.1f * (vy1 - vy0));
    if (!isRound) break;
    float dx = f.x - Sc, dy = f.y - Sc;
    if (dx * dx + dy * dy < 0.38f * 0.38f * S * S) break;
  }
  const float a = frand(0, 6.2832f);
  f.vx = cosf(a) * 0.1f * S; f.vy = sinf(a) * 0.1f * S;
  f.head = a;
  f.ph = frand(0, 6.2832f);
  f.hz = FLAP_HZ * frand(0.84f, 1.16f);
  f.seed = frand(0, 6.2832f);
  f.orbR = frand(0.10f, 0.62f);
  f.orbW = frand(0.9f, 1.8f) * (frand() < 0.15f ? -1.0f : 1.0f);
  f.orbA = frand(0, 6.2832f);
  f.mW = 6.2832f * MEANDER_HZ * frand(0.7f, 1.35f);
  f.mA = MEANDER * frand(0.7f, 1.2f);
  f.mP = frand(0, 6.2832f);
  f.flapT = frand(0.5f, 2.5f); f.glide = false;
  f.y0 = 1; f.y1 = 0;
}

static void setCount(int idx) {
  const int n = COUNTS[idx];
  if (n > nFly) {
    for (int i = nFly; i < n; ++i) { spawnFly(i); burstDust(fly[i].x, fly[i].y, 6, 0.10f * S, fly[i].cls); }
  } else {
    for (int i = n; i < nFly; ++i) burstDust(fly[i].x, fly[i].y, 8, 0.12f * S, fly[i].cls);   // 光になって消える
  }
  nFly = n;
  Serial.printf("butterflies: %d\n", nFly);
}

// ---------------------------------------------------------------- 1フレームの準備
struct Input { bool touching; bool tapped; float tx, ty; float dt; };

// 風: うずの関数 ψ = Σ A sin(k・p + ωt) を微分して (∂ψ/∂y, -∂ψ/∂x)。湧き出しのない、うずだけの流れ
static const float FKX[4] = { 5.1f, -3.7f,  7.9f, -6.3f};
static const float FKY[4] = { 2.3f,  6.1f, -4.4f, -7.2f};
static const float FW[4]  = { 0.23f, -0.31f, 0.41f, -0.19f};
static const float FA[4]  = { 1.00f,  0.90f, 0.55f,  0.50f};

static void updateFlies(const Input& in, float dt, float fvx, float fvy, bool released) {
  float th[4], norm = 0;
  for (int k = 0; k < 4; ++k) { th[k] = FW[k] * simT + k * 1.7f; norm += FA[k] * sqrtf(FKX[k] * FKX[k] + FKY[k] * FKY[k]); }
  const float flowK = FLOW_SPEED * S * 2.2f / norm;
  const float invS = 1.0f / S;

  // 集まる先: 押さえた指 / ときどき群れが自分で集まる所
  const float acx = Sc + 0.30f * viewR * sinf(simT * 0.23f), acy = Sc + 0.24f * viewR * sinf(simT * 0.31f + 1.0f);
  const float w1 = holdW, w2 = autoW * 0.8f * (1.0f - holdW), gw = w1 + w2;
  float gcx = Sc, gcy = Sc, orbS = 1.0f;
  if (gw > 0.001f) {
    gcx = (fPX * w1 + acx * w2) / gw; gcy = (fPY * w1 + acy * w2) / gw;
    orbS = (0.55f * w1 + 1.0f * w2) / gw * viewR;
  }
  const bool stir = in.touching && (fvx != 0 || fvy != 0);
  const float sig2 = 2.0f * (0.14f * S) * (0.14f * S);

  // 蝶どうしは近づきすぎない(同じ層どうしは強く、ちがう層は弱く)
  for (int i = 0; i < nFly; ++i) { sepX[i] = 0; sepY[i] = 0; }
  for (int i = 0; i < nFly; ++i) {
    const Fly& a = fly[i];
    for (int j = i + 1; j < nFly; ++j) {
      const Fly& b = fly[j];
      const float want = (a.R + b.R) * 0.85f;
      const float ox = a.x - b.x, oy = a.y - b.y;
      if (ox > want || ox < -want || oy > want || oy < -want) continue;
      const float d2 = ox * ox + oy * oy;
      if (d2 >= want * want || d2 < 0.01f) continue;
      const float d = sqrtf(d2), k = (want - d) * (a.cls == b.cls ? 10.0f : 3.0f) / d;
      sepX[i] += ox * k; sepY[i] += oy * k; sepX[j] -= ox * k; sepY[j] -= oy * k;
    }
  }

  for (int i = 0; i < nFly; ++i) {
    Fly& f = fly[i];
    const float sk = SPEED_K[f.cls];
    // 風に乗る + 1匹ずつのさまよい
    const float px = f.x * invS, py = f.y * invS;
    float u = 0, v = 0;
    for (int k = 0; k < 4; ++k) { const float c = cosf(FKX[k] * px + FKY[k] * py + th[k]); u += FA[k] * FKY[k] * c; v -= FA[k] * FKX[k] * c; }
    float dvx = u * flowK * sk + 0.07f * S * sk * sinf(simT * 0.37f + f.seed);
    float dvy = v * flowK * sk + 0.07f * S * sk * cosf(simT * 0.45f + f.seed * 1.3f);
    // ふちに近いほど、外へ向かう成分を消して「ふちに沿う流れ」にする(ふちに溜まらないように)
    {
      float rx, ry, t;
      if (isRound) {
        const float dx = f.x - Sc, dy = f.y - Sc, d = sqrtf(dx * dx + dy * dy) + 0.001f;
        rx = dx / d; ry = dy / d; t = smooth3((d - 0.27f * S) / (0.19f * S));
      } else {
        const float hx = 0.5f * (vx1 - vx0), hy = 0.5f * (vy1 - vy0);
        const float nx = (f.x - (vx0 + hx)) / hx, ny = (f.y - (vy0 + hy)) / hy;
        const float d = sqrtf(nx * nx + ny * ny) + 0.001f;
        rx = nx / d; ry = ny / d; t = smooth3((fmaxf(fabsf(nx), fabsf(ny)) - 0.45f) / 0.45f);
      }
      const float out = dvx * rx + dvy * ry;
      const float pull = 0.15f * S * sk * t;                   // ふちからは、少し内へ
      if (out > 0) { dvx -= rx * out * t; dvy -= ry * out * t; }
      dvx -= rx * pull; dvy -= ry * pull;
    }
    // 集まって、輪になって回る
    if (gw > 0.001f) {
      f.orbA += f.orbW * dt * (1.0f + 0.6f * w1);
      const float r = f.orbR * orbS * (1.0f + 0.12f * sinf(simT * 1.3f + f.seed));
      const float ca = cosf(f.orbA), sa = sinf(f.orbA);
      const float tx = gcx + ca * r, ty = gcy + sa * r * 0.85f;
      const float gx = (tx - f.x) * 2.4f - sa * r * f.orbW, gy = (ty - f.y) * 2.4f + ca * r * 0.85f * f.orbW;
      dvx += (gx - dvx) * gw; dvy += (gy - dvy) * gw;
    }
    // ゆったり追いかけて(強く引っぱらない)、ゆらゆら揺れる(NullSutra v2 の蝶と同じ動き)
    // 蛇行: 行きたい向きを、左右にゆっくり振る(うねり2つを重ねて、規則的すぎないように)
    {
      f.mP += f.mW * dt;
      const float ang = f.mA * (1.0f - 0.5f * gw) * (sinf(f.mP) + 0.35f * sinf(f.mP * 1.73f + f.seed * 2.1f));
      float spd = sqrtf(dvx * dvx + dvy * dvy);
      const float minSp = 0.08f * S * sk;                      // 止まりかけでも、うねりが見えるように
      if (spd < minSp) {
        if (spd < 0.001f) { dvx = cosf(f.head); dvy = sinf(f.head); spd = 1.0f; }
        dvx *= minSp / spd; dvy *= minSp / spd;
      }
      const float ca = cosf(ang), sa = sinf(ang);
      const float rx = dvx * ca - dvy * sa, ry = dvx * sa + dvy * ca;
      const float gk = f.glide ? 0.85f : 1.0f;                 // 滑空中は少しずつ遅くなる
      dvx = rx * gk; dvy = ry * gk;
    }
    // ゆったり追いかける(強く引っぱらない)
    float ax = (dvx - f.vx) * 1.8f + sepX[i];
    float ay = (dvy - f.vy) * 1.8f + sepY[i];
    // なぞると、指の近くの蝶が巻き込まれる
    if (stir) {
      const float dx = f.x - in.tx, dy = f.y - in.ty, d2 = dx * dx + dy * dy;
      if (d2 < 4.5f * sig2) { const float g = expf(-d2 / sig2); ax += (fvx * 0.8f - f.vx) * 5.0f * g; ay += (fvy * 0.8f - f.vy) * 5.0f * g; }
    }
    // タップで散る / 押さえて集めたのを離すと、ぱっと散る
    if (in.tapped || released) {
      const float cx = in.tapped ? in.tx : fPX, cy = in.tapped ? in.ty : fPY;
      const float dx = f.x - cx, dy = f.y - cy, d = sqrtf(dx * dx + dy * dy) + 1.0f, reach = 0.55f * S;
      if (d < reach) {
        const float s = (in.tapped ? 0.60f : 0.40f) * S * (1.0f - d / reach) * frand(0.7f, 1.2f) * sk;
        f.vx += dx / d * s; f.vy += dy / d * s;
      }
    }
    // 画面からはみ出さない(ふちに近づくと、やわらかく押し戻す)
    if (isRound) {
      const float dx = f.x - Sc, dy = f.y - Sc, d = sqrtf(dx * dx + dy * dy) + 0.001f, rin = 0.40f * S;
      if (d > rin) { const float k = (d - rin) * 9.0f / d; ax -= dx * k; ay -= dy * k; }
    } else {
      const float m = 0.08f * (vy1 - vy0);
      if (f.x < vx0 + m) ax += (vx0 + m - f.x) * 9.0f;
      if (f.x > vx1 - m) ax -= (f.x - vx1 + m) * 9.0f;
      if (f.y < vy0 + m) ay += (vy0 + m - f.y) * 9.0f;
      if (f.y > vy1 - m) ay -= (f.y - vy1 + m) * 9.0f;
    }
    f.vx += ax * dt; f.vy += ay * dt;
    float sp = sqrtf(f.vx * f.vx + f.vy * f.vy);
    const float lim = 0.55f * S * sk;
    if (sp > lim) { f.vx *= lim / sp; f.vy *= lim / sp; sp = lim; }
    f.x += f.vx * dt; f.y += f.vy * dt;
    if (isRound) {
      const float dx = f.x - Sc, dy = f.y - Sc, d = sqrtf(dx * dx + dy * dy), R = 0.48f * S;
      if (d > R) { f.x = Sc + dx / d * R; f.y = Sc + dy / d * R; f.vx *= 0.5f; f.vy *= 0.5f; }
    } else {
      f.x = fminf(fmaxf(f.x, vx0), vx1); f.y = fminf(fmaxf(f.y, vy0), vy1);
    }
    // 羽ばたき。速く飛ぶときだけ、少し速くなる
    // 羽ばたいて → ときどき羽を開いたまま滑空 → また羽ばたく
    if (f.glide) {
      f.flapT -= dt;
      if (f.flapT <= 0 || in.tapped || released) { f.glide = false; f.flapT = frand(1.0f, 3.0f); }   // 驚いたら、すぐ羽ばたく
    } else {
      const float oldPh = f.ph;
      f.ph += dt * 6.2832f * f.hz * (0.8f + 0.6f * fminf(1.0f, sp / (0.3f * S)));
      f.flapT -= dt;
      // 羽がいちばん開いた所(cos = ±1)を通りすぎた瞬間に止めて、滑空に入る
      if (f.flapT <= 0 && GLIDE > 0 && (int)(f.ph * 0.31831f) != (int)(oldPh * 0.31831f)) {
        f.ph = floorf(f.ph * 0.31831f) * 3.14159265f;
        f.glide = true;
        f.flapT = frand(0.4f, 1.0f) * GLIDE / 0.30f;
        if (in.tapped || released) f.flapT = 0;
      }
    }
    if (f.ph > 1000.0f) f.ph -= 318.0f * 3.14159265f;
    // 向きは進む方へ
    if (sp > 0.02f * S) f.head += wrapPi(atan2f(f.vy, f.vx) - f.head) * fminf(1.0f, dt * 4.0f);   // 向きは進む方へ、なめらかに
    f.head = wrapPi(f.head);
    // 鱗粉(速く飛ぶほど、手前の蝶ほどたくさん)
    const float rate = DUST_RATE * (0.35f + 0.65f * fminf(1.0f, sp / (0.35f * S))) * (f.cls == 0 ? 0.4f : (f.cls == 1 ? 0.7f : 1.0f));
    if (frand() < rate * dt) {
      const float a = frand(0, 6.2832f), s = frand(0.0f, 0.03f) * S;
      emitDust(f.x - f.vx * 0.03f, f.y - f.vy * 0.03f, f.vx * 0.15f + cosf(a) * s, f.vy * 0.15f + sinf(a) * s, f.cls, frand(0.9f, 2.2f));
    }

    // ---- 描くための値
    int m = 0;
    while (m < NMIP - 1 && mipR0[m + 1] >= 0.85f * f.R) ++m;       // 縮めすぎない、いちばん小さい絵
    const int T = MIPT[m];
    const float k = mipR0[m] / f.R;
    const float hd = f.head + 0.12f * sinf(f.ph * 0.5f + f.seed);   // 少し首をふる
    const float cs = cosf(hd), sn = sinf(hd);
    f.dxx = -k * sn; f.dxy = k * cs;                               // 絵の横 = 進む向きに直角
    f.dyx = -k * cs; f.dyy = -k * sn;                              // 絵の上 = 進む向き
    // 羽ばたくたびに、ふわっと浮き沈み(羽を打ち下ろすと浮く)。滑空中はそのまま
    f.cx = f.x; f.cy = f.y - BOB * f.R * sinf(f.ph);
    f.rr = T * 0.5f / k;
    f.y0 = (int)floorf(f.cy - f.rr); f.y1 = (int)ceilf(f.cy + f.rr);
    const int fr = (int)(fabsf(cosf(f.ph)) * (NFR - 1) + 0.5f);
    f.T = T;
    f.tex = atlas[m] + (size_t)fr * T * T;
    f.pal = palBuf + ((f.cls * 2 + f.var) * NFR + fr) * NPIDX;
  }
}

static void updateDust(float dt) {
  const float damp = expf(-dt * 1.6f);
  nDD = 0;
  for (int i = 0; i < MAXDUST; ++i) {
    Dust& d = dust[i];
    if (d.life <= 0) continue;
    d.life -= dt;
    if (d.life <= 0) continue;
    d.vx *= damp; d.vy = d.vy * damp + 0.02f * S * dt;           // ほんの少しずつ、下へ落ちる
    d.x += d.vx * dt; d.y += d.vy * dt;
    const int ix = (int)floorf(d.x), iy = (int)floorf(d.y);
    if (ix < 1 || iy < 1 || ix >= S - 1 || iy >= S - 1) continue;
    const float l = d.life / d.maxL;
    const float tw = 0.55f + 0.45f * sinf(d.tw + simT * 11.0f);    // きらきら
    int q = (int)(l * tw * 15.0f + 0.5f); if (q > 15) q = 15;
    if (q <= 0) continue;
    DustDraw& o = dd[nDD++];
    o.x = (int16_t)ix; o.y = (int16_t)iy;
    o.c = dustLut[d.cls][q]; o.ch = dustLutH[d.cls][q];
    o.plus = (d.cls == 2 && q >= 7) ? 1 : 0;
  }
  // 帯ごとに並べる(描くときに、その帯の鱗粉だけを見るため)
  for (int k = 0; k <= NB; ++k) ddStart[k] = 0;
  for (int i = 0; i < nDD; ++i) ddStart[dd[i].y / BAND + 1]++;
  for (int k = 0; k < NB; ++k) ddStart[k + 1] += ddStart[k];
  int16_t fill[MAXNB];
  for (int k = 0; k < NB; ++k) fill[k] = ddStart[k];
  for (int i = 0; i < nDD; ++i) ddOrd[fill[dd[i].y / BAND]++] = (int16_t)i;
}

// 背景: ほとんど黒。まん中が少し明るく、ぼんやりした光がゆっくり漂う。触っている所も少し明るむ
static constexpr size_t nmOa42 = nmAlignTo(nmOv42, 16), nmOv43 = nmOa42 + sizeof(float[5][MAXNB * 2]);
static float (&nmL_gx)[5][MAXNB * 2] = *reinterpret_cast<float(*)[5][MAXNB * 2]>(NM_ARENA + nmOa42);
static constexpr size_t nmOa43 = nmAlignTo(nmOv43, 16), nmOv44 = nmOa43 + sizeof(float[5][MAXNB * 2]);
static float (&nmL_gy)[5][MAXNB * 2] = *reinterpret_cast<float(*)[5][MAXNB * 2]>(NM_ARENA + nmOa43);
static void updateBg() {
  const Theme& th = THEMES[theme];
  struct Glow { float x, y, sig, amp; Col c; };
  Glow g[5];
  int ng = 0;
  g[ng++] = {Sc, Sc, 0.42f * S, 1.0f, th.bg1};
  g[ng++] = {Sc + 0.30f * viewR * sinf(simT * 0.050f), Sc + 0.25f * viewR * sinf(simT * 0.070f + 1.0f), 0.26f * S, 0.30f, th.glowA};
  g[ng++] = {Sc + 0.32f * viewR * sinf(simT * 0.043f + 2.0f), Sc + 0.30f * viewR * sinf(simT * 0.061f + 4.0f), 0.22f * S, 0.26f, th.glowB};
  g[ng++] = {Sc + 0.28f * viewR * sinf(simT * 0.067f + 4.2f), Sc + 0.30f * viewR * sinf(simT * 0.037f + 2.5f), 0.20f * S, 0.20f, lerpC(th.glowA, th.glowB, 0.5f)};
  if (touchGlow > 0.01f) g[ng++] = {fPX, fPY, 0.13f * S, 0.10f * touchGlow, th.dust};
  auto& gx = nmL_gx;
  auto& gy = nmL_gy;
  for (int k = 0; k < ng; ++k) {
    const float is = 1.0f / (2.0f * g[k].sig * g[k].sig);
    for (int i = 0; i < BW; ++i) {
      const float p = (float)((i << BGSH) + (1 << (BGSH - 1)));
      const float dx = p - g[k].x, dy = p - g[k].y;
      gx[k][i] = expf(-dx * dx * is) * g[k].amp;
      gy[k][i] = expf(-dy * dy * is);
    }
  }
  for (int j = 0; j < BW; ++j) {
    uint16_t* o = bgLow + j * BW;
    for (int i = 0; i < BW; ++i) {
      Col c = th.bg0;
      for (int k = 0; k < ng; ++k) {
        const float w = gx[k][i] * gy[k][j];
        c.r += g[k].c.r * w; c.g += g[k].c.g * w; c.b += g[k].c.b * w;
      }
      o[i] = toPix(c);
    }
  }
}

static void updateFrame(const Input& in) {
  float dt = in.dt > 0.05f ? 0.05f : in.dt;
  if (dt < 0.0005f) dt = 0.0005f;
  simT += dt;

  // ---- 指
  float fvx = 0, fvy = 0;
  if (in.touching) {
    if (fWas) { fvx = (in.tx - fPX) / dt; fvy = (in.ty - fPY) / dt; }
    const float mv = sqrtf(fvx * fvx + fvy * fvy);
    stillT = (mv < 0.05f * S) ? stillT + dt : 0.0f;          // 指が止まっている時間(なぞっている間は 0)
  } else stillT = 0;
  const bool released = fWas && !in.touching && holdW > 0.5f;
  if (in.touching) { fPX = in.tx; fPY = in.ty; }
  fWas = in.touching;
  const bool holding = in.touching && stillT > 0.35f;
  holdW = clamp01(holdW + (holding ? dt * 2.0f : -dt * 2.5f));
  touchGlow = clamp01(touchGlow + (in.touching ? dt * 3.0f : -dt * 1.5f));

  // ---- ときどき、群れが自分で集まる
  autoT -= dt;
  if (autoT <= 0) { autoOn = !autoOn; autoT = autoOn ? frand(5.0f, 8.0f) : SWARM_EVERY * frand(0.7f, 1.3f); }
  autoW = clamp01(autoW + (autoOn ? dt : -dt) / 1.5f);

  if (in.tapped) burstDust(in.tx, in.ty, 40, 0.35f * S, 2);
  if (released) burstDust(fPX, fPY, 30, 0.30f * S, 2);

  updateFlies(in, dt, fvx, fvy, released);
  updateDust(dt);
  updateBg();
}

// ---------------------------------------------------------------- 描画(帯ごと)
static void renderBand(int k, uint16_t* buf) {
  const int y0 = k * BAND, y1 = min(S, y0 + BAND);
  const int bx0 = bandX0[k], bw = bandW[k], bx1 = bx0 + bw - 1;

  // ---- 背景(粗い背景を4倍に広げて敷く)
  for (int y = y0; y < y1; ++y) {
    uint16_t* row = buf + (y - y0) * bw - bx0;               // row[x] で画面の x
    const int xs = max((int)spanS[y], bx0), xe = min((int)spanE[y], bx1);
    for (int x = bx0; x < xs; ++x) row[x] = blackCol;
    for (int x = max(xe + 1, bx0); x <= bx1; ++x) row[x] = blackCol;
    const uint16_t* bg = bgLow + (y >> BGSH) * BW;
    for (int x = xs; x <= xe; ++x) row[x] = bg[x >> BGSH];
  }

  // ---- 鱗粉(光を足す)。十字の形のものは上下の帯にまたがるので、となりの帯の分も見る
  for (int kk = max(0, k - 1); kk <= min(NB - 1, k + 1); ++kk) {
    for (int n = ddStart[kk]; n < ddStart[kk + 1]; ++n) {
      const DustDraw& o = dd[ddOrd[n]];
      for (int dy = -1; dy <= 1; ++dy) {
        if (dy != 0 && !o.plus) continue;
        const int y = o.y + dy;
        if (y < y0 || y >= y1) continue;
        const int xs = max((int)spanS[y], bx0), xe = min((int)spanE[y], bx1);
        uint16_t* row = buf + (y - y0) * bw - bx0;
        if (dy != 0) { if (o.x >= xs && o.x <= xe) row[o.x] = addPix(row[o.x], o.ch); continue; }
        if (o.x >= xs && o.x <= xe) row[o.x] = addPix(row[o.x], o.c);
        if (o.plus) {
          if (o.x - 1 >= xs && o.x - 1 <= xe) row[o.x - 1] = addPix(row[o.x - 1], o.ch);
          if (o.x + 1 >= xs && o.x + 1 <= xe) row[o.x + 1] = addPix(row[o.x + 1], o.ch);
        }
      }
    }
  }

  // ---- 蝶(奥から手前へ)。絵を回して・縮めて、透明でない所だけ貼る
  for (int n = 0; n < MAXFLY; ++n) {
    const int i = order[n];
    if (i >= nFly) continue;
    const Fly& f = fly[i];
    const int ra = max(y0, f.y0), rb = min(y1 - 1, f.y1);
    if (ra > rb) continue;
    const int T = f.T;
    const float half = T * 0.5f, rr2 = f.rr * f.rr;
    const uint8_t*  tex = f.tex;
    const uint16_t* pal = f.pal;
    const int32_t du = (int32_t)(f.dxx * 65536.0f), dv = (int32_t)(f.dyx * 65536.0f);
    for (int y = ra; y <= rb; ++y) {
      const float dy = y + 0.5f - f.cy, w2 = rr2 - dy * dy;
      if (w2 <= 0) continue;
      const float hw = sqrtf(w2);
      const int xa = max(max((int)floorf(f.cx - hw), bx0), (int)spanS[y]);
      const int xb = min(min((int)floorf(f.cx + hw), bx1), (int)spanE[y]);
      if (xa > xb) continue;
      const float ex = xa + 0.5f - f.cx;
      int32_t u = (int32_t)((half + f.dxx * ex + f.dxy * dy) * 65536.0f);
      int32_t v = (int32_t)((half + f.dyx * ex + f.dyy * dy) * 65536.0f);
      uint16_t* row = buf + (y - y0) * bw - bx0;
      for (int x = xa; x <= xb; ++x, u += du, v += dv) {
        const uint32_t tx = (uint32_t)(u >> 16), ty = (uint32_t)(v >> 16);
        if (tx < (uint32_t)T && ty < (uint32_t)T) {
          const uint8_t id = tex[ty * T + tx];
          if (id) row[x] = pal[id];
        }
      }
    }
  }
}

static void __attribute__((noinline)) MOD_IRAM renderBandIram(int k, uint16_t* b) { renderBand(k, b); }

// コア0: 奇数番目の帯
static void workerTask(void*) {
  for (;;) {
    xSemaphoreTake(semGo, portMAX_DELAY);
    uint32_t busy = 0;
    for (int k = 0; k < NB; ++k) {
      if (k % SPLIT == 0) continue;
      while (__atomic_load_n(&doneUpTo, __ATOMIC_ACQUIRE) < k - NSLOT) { }
      if (bandVis[k]) {
        uint32_t t0 = micros();
        renderBandIram(k, slotBuf[k % NSLOT]);
        busy += micros() - t0;
      }
      __atomic_store_n(&readyW, k, __ATOMIC_RELEASE);
    }
    busyUs[0] = busy;
    xSemaphoreGive(semDone);
  }
}

// コア1: 偶数番目の帯を描き、順番に画面へ送る
static void renderFrame() {
  if (fbState < 0) {                                // 初めの1回: StopWatch(画面の手前に PSRAM の置き場がある)かどうか
    auto* pnl = M5.Display.getPanel();
    fbState = (pnl && pnl->getAutoDisplay()) ? 1 : 0;
    fbMode = fbState == 1;
    SPLIT = fbMode ? 3 : 2;
    Serial.printf("display: %s\n", fbMode ? "framebuffer (send each band)" : "direct DMA");
  }
  __atomic_store_n(&readyW, -1, __ATOMIC_RELEASE);
  __atomic_store_n(&doneUpTo, -1, __ATOMIC_RELEASE);
  xSemaphoreGive(semGo);
  uint32_t busy = 0, send = 0;
  if (!fbMode) M5.Display.startWrite();
  for (int k = 0; k < NB; ++k) {
    if (k % SPLIT == 0) {
      if (bandVis[k]) {
        uint32_t t0 = micros();
        renderBandIram(k, slotBuf[k % NSLOT]);
        busy += micros() - t0;
      }
    } else {
      while (__atomic_load_n(&readyW, __ATOMIC_ACQUIRE) < k) { }
    }
    if (!fbMode) M5.Display.waitDMA();
    __atomic_store_n(&doneUpTo, k - 1, __ATOMIC_RELEASE);
    if (bandVis[k]) {
      int y0 = k * BAND, h = min(BAND, S - y0);
      if (fbMode) {                                   // 置き場に書いて、この帯の丸の中の幅だけをすぐ画面へ送る
        const uint32_t ts = micros();
        M5.Display.pushImage(XOFF + bandX0[k], YOFF + y0, bandW[k], h, slotBuf[k % NSLOT]);
        send += micros() - ts;
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
static void fail(const char* msg) {
  M5.Display.setTextDatum(middle_center);
  M5.Display.drawString(msg, M5.Display.width() / 2, M5.Display.height() / 2);
  Serial.println(msg);
  while (true) nmIdle();
}

static void* allocFast(size_t n) {                // 内蔵メモリを優先、足りなければ PSRAM
  void* p = heap_caps_malloc(n, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  return p ? p : ps_malloc(n);
}

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
  BW = (S >> BGSH) + 1;
  vx0 = (float)max(0, -XOFF); vx1 = (float)min(S, W - XOFF) - 1;
  vy0 = (float)max(0, -YOFF); vy1 = (float)min(S, H - YOFF) - 1;
  viewR = 0.5f * fminf(vx1 - vx0, vy1 - vy0);
  Serial.println("NullButterfly v6");
  Serial.printf("screen %dx%d  area %d\n", W, H, S);

  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setBrightness(brightSteps[brightIdx]);

  for (int i = 0; i < NSLOT; ++i) slotBuf[i] = (uint16_t*)heap_caps_malloc(S * BAND * 2, MALLOC_CAP_DMA);
  // 1ピクセルごとに読む蝶の絵と色の表、背景は内蔵メモリに
  for (int m = 0; m < NMIP; ++m) atlas[m] = (uint8_t*)allocFast((size_t)NFR * MIPT[m] * MIPT[m]);
  palBuf = (uint16_t*)allocFast((size_t)6 * NFR * NPIDX * 2);
  bgLow  = (uint16_t*)allocFast((size_t)BW * BW * 2);
  spanS  = (int16_t*)malloc(S * sizeof(int16_t)); spanE = (int16_t*)malloc(S * sizeof(int16_t));
  bandX0 = (int16_t*)malloc(NB * sizeof(int16_t)); bandW = (int16_t*)malloc(NB * sizeof(int16_t));
  bandVis = (uint8_t*)malloc(NB);
  bool ok = palBuf && bgLow && spanS && spanE && bandX0 && bandW && bandVis && NB <= MAXNB && BW <= MAXNB * 2;
  for (int i = 0; i < NSLOT; ++i) ok = ok && slotBuf[i];
  for (int m = 0; m < NMIP; ++m) ok = ok && atlas[m];
  if (!ok) fail("Memory error");

  float rr = S * 0.5f + 1.0f;
  for (int y = 0; y < S; ++y) {
    if (!isRound) { spanS[y] = 0; spanE[y] = S - 1; continue; }
    float dy = y - Sc, w = rr * rr - dy * dy;
    if (w <= 0) { spanS[y] = 1; spanE[y] = 0; continue; }
    float s = sqrtf(w);
    spanS[y] = max(0, (int)floorf(Sc - s)); spanE[y] = min(S - 1, (int)ceilf(Sc + s));
  }
  for (int k = 0; k < NB; ++k) {
    int a = S, b = -1;
    for (int y = k * BAND; y < min(S, k * BAND + BAND); ++y)
      if (spanS[y] <= spanE[y]) { a = min(a, (int)spanS[y]); b = max(b, (int)spanE[y]); }
    if (b < a) { a = 0; b = 1; }
    a &= ~1; if (((b - a + 1) & 1) && b < S - 1) ++b;
    bandX0[k] = a; bandW[k] = b - a + 1;
    int sy = YOFF + k * BAND, sh = min(BAND, S - k * BAND);
    bandVis[k] = (sy + sh > 0 && sy < H) ? 1 : 0;
  }
  blackCol = 0;

  buildAtlas();
  buildPalettes();
  for (int i = 0; i < MAXDUST; ++i) dust[i].life = 0;
  int n = 0;                                                    // 描く順: 奥 → 中 → 手前
  for (int c = 0; c < 3; ++c)
    for (int i = 0; i < MAXFLY; ++i) if (clsOf(i) == c) order[n++] = i;
  for (int i = 0; i < MAXFLY; ++i) spawnFly(i);
  nFly = COUNTS[countIdx];
  Serial.printf("butterflies: %d  theme: %s\n", nFly, THEMES[theme].name);

  Input in0 = {false, false, -1000, -1000, 0.016f};
  updateFrame(in0);

  semGo   = xSemaphoreCreateBinary();
  semDone = xSemaphoreCreateBinary();
  xTaskCreatePinnedToCore(workerTask, "band", 8192, nullptr, 1, nullptr, 0);
}

static void nextCount() {
  countIdx = (countIdx + 1) % NCOUNT;
  setCount(countIdx);
}

static void nextTheme() {
  theme = (theme + 1) % NTHEME;
  buildPalettes();
  Serial.printf("theme: %s\n", THEMES[theme].name);
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
    if (M5.BtnA.wasHold()) nextTheme();
    else if (M5.BtnA.wasClicked()) { nextCount(); }
    if (M5.BtnB.wasPressed()) {
      brightIdx = (brightIdx + 1) % sizeof(brightSteps);
      M5.Display.setBrightness(brightSteps[brightIdx]);
    }
  } else {
    if (M5.BtnPWR.wasClicked()) {
      brightIdx = (brightIdx + 1) % sizeof(brightSteps);
      M5.Display.setBrightness(brightSteps[brightIdx]);
    }
    if (pressed) {
      if (now - lastTapMs < 350) {                             // ダブルタップ: 上半分なら数、下半分なら色
        if (t.y < H / 2) nextCount(); else nextTheme();
        lastTapMs = 0;
      }
      else lastTapMs = now;
    }
  }

  Input in;
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
    Serial.printf("fps: %.1f  core0 %.1f ms  core1 %.1f ms  send %.1f ms\n", fps, busyUs[0] / 1000.0f, busyUs[1] / 1000.0f, sendUs / 1000.0f);
    frames = 0; fpsTimer = millis();
  }
}


// 共通の置き場に置いたものは、0 で始まる単純な型だけ(コンストラクタが要らないもの)
static_assert(std::is_trivially_default_constructible<Dust>::value, "NM_ARENA: Dust");
static_assert(std::is_trivially_default_constructible<DustDraw>::value, "NM_ARENA: DustDraw");
static_assert(std::is_trivially_default_constructible<Fly>::value, "NM_ARENA: Fly");
static_assert(std::is_trivially_default_constructible<bool>::value, "NM_ARENA: bool");
static_assert(std::is_trivially_default_constructible<float>::value, "NM_ARENA: float");
static_assert(std::is_trivially_default_constructible<int>::value, "NM_ARENA: int");
static_assert(std::is_trivially_default_constructible<int16_t>::value, "NM_ARENA: int16_t");
static_assert(std::is_trivially_default_constructible<uint16_t>::value, "NM_ARENA: uint16_t");
static_assert(std::is_trivially_default_constructible<uint32_t>::value, "NM_ARENA: uint32_t");
static constexpr size_t OVL_END = nmAlignTo(nmOv44, 16);
static_assert(OVL_END <= NM_ARENA_SIZE, "NM_ARENA_SIZE (nm_common.h) を大きくしてください");
extern const size_t OVL_SIZE = OVL_END;   // 起動時の確認用
}  // namespace NullButterfly
