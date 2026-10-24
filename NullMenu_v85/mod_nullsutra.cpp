// NullSutra  -  NullMenu 用モジュール(元のスケッチ NullSutra_v8 を名前空間に入れたもの)
// 0 で始まる変数(配列も)は、ほかの作品と共通の置き場(NM_ARENA)に置いています。
// 一度に動くのは1つだけ(切り替えるときは再起動)なので、重なっていても問題ありません。
#include "nm_common.h"

#pragma GCC optimize("O3")

namespace NullSutra {
static constexpr size_t nmPre = 1920;   // 共通の置き場のいちばん前に、内蔵メモリを節約するための表を置く(下の nmOv0 から後ろがいつもの分)
static constexpr size_t nmOv0 = nmPre;

// NullSutra v8  -  暗い鏡の部屋。炎の雲に照らされて、般若心経の文字の柱が流れる
//   M5Stack StopWatch 用(丸い 466x466 画面)。CoreS3 系でも画面いっぱいに動きます。
//
// 画面の中:
//   ・上のほうを、炎と青い光の雲がゆっくり立ちのぼっていく
//   ・その手前に、般若心経の文字が縦の柱になって並び、それぞれの速さで流れていく
//     文字は後ろの炎に照らされて金色に光る(炎の明るい所ほど、文字も明るい)
//   ・宙に鏡の立方体が浮かんで、ゆっくり回りながら炎を映す
//   ・画面の下のほうを、青い蝶がひらひら舞う
//   ・どの柱にも、5〜30 文字おきに「ﾇﾙ」が紛れ込んでいる
//
// 触ると:
//   ・なぞる        : 炎の雲が指についてうねる
//   ・タップ        : そこから光の輪が広がって、通り道の文字がパッと光る。蝶は驚いて散る
//   ・押さえたまま  : 蝶が指のまわりに集まってきて、くるくる回る
//
// 操作:
//   StopWatch … ボタンA 短押し: 色(炎と金 / 蒼 / 紅桜 / 墨)  ボタンA 長押し: 文字の流れる速さ
//               ボタンB: 明るさ
//   CoreS3系  … ダブルタップ: 色      電源ボタン短押し: 明るさ
//
// 必要なもの:
//   ボードマネージャー M5Stack >= 3.3.7 / ボード: M5StopWatch(CoreS3 なら M5CoreS3)
//   ライブラリ M5Unified >= 0.2.15, M5GFX >= 0.2.21
//   文字は M5GFX に入っている明朝体(lgfxJapanMincho_20)で、起動時に描いて使います。
//   その明朝体に入っていない7字だけは、このスケッチの中の小さなフォント(SUTRA_EXTRA_FONT)で描きます。
//   (フォントは1サイズだけ使います。いくつも使うと、プログラムが大きくなりすぎるため)
//
// しくみ:
//   炎は「強さ(6ビット)+ 色の系統(2ビット)」の1枚の絵を、うねりでずらしながら読みます。
//   文字は起動時に1文字ずつ描いて、にじみ(光のぼかし)を付けた「濃さの地図」にしておきます。
//   1ピクセルごとの仕事は「炎の番号」と「文字の濃さ」を組にして、色の表を1回引くだけ。
//   文字の明るさ(暗い柱・明るい柱・光った文字)は、表を丸ごと切り替えて表します。
//   画面は8行ずつの帯に分けて、2つのコアで交互に描きます。
//
// 文字: 般若心経(玄奘訳の流布本)。古い経典で、著作権の対象ではありません。



// Arduino IDE は関数の宣言を自動でこのあたりに足すので、型の名前だけ先に宣言しておく
struct Scratch; struct Column; struct Wave; struct Face; struct Edge; struct Fly; struct Col; struct NMap; struct Pal; struct Input;

// ---- 調整用 ----------------------------------------------------------------
static constexpr float COLS       = 18.0f;   // 画面の幅に並ぶ文字の柱の数(文字の大きさが決まる)
static constexpr float TEXT_SPEED = 1.0f;    // 文字の流れる速さ(ボタンA長押しで ×0.5 / ×1 / ×2)
static constexpr float TEXT_GLOW  = 0.55f;   // 文字のまわりのにじみ
static constexpr float FIRE_SCALE = 0.42f;   // 炎の雲の細かさ(大きいほど細かい)
static constexpr float FIRE_RISE  = 9.0f;    // 炎が立ちのぼる速さ
static constexpr float FIRE_FLOW  = 7.0f;    // 炎がうねる大きさ
static constexpr float FIRE_SPEED = 0.07f;   // うねりが移り変わる速さ
static constexpr float STIR       = 0.9f;    // なぞったとき、炎が指について動く強さ
static constexpr float STIR_KEEP  = 0.55f;   // 指で動かした炎が戻る速さ
static constexpr float CUBE_SIZE  = 0.105f;  // 鏡の立方体の大きさ
static constexpr float CUBE_SPIN  = 0.16f;   // 立方体の回る速さ(rad/秒)
static constexpr int   NFLY       = 5;       // 蝶の数
static constexpr float FLY_SIZE   = 0.078f;  // 蝶の大きさ
static constexpr float RIPPLE     = 0.55f;   // タップの光の輪が広がる速さ(画面/秒)
// ---------------------------------------------------------------------------

static const char SUTRA[] =
  "観自在菩薩行深般若波羅蜜多時照見五蘊皆空度一切苦厄舎利子色不異空空不異色色即是空空即是色"
  "受想行識亦復如是舎利子是諸法空相不生不滅不垢不浄不増不減是故空中無色無受想行識無眼耳鼻舌身意"
  "無色声香味触法無眼界乃至無意識界無無明亦無無明尽乃至無老死亦無老死尽無苦集滅道無智亦無得"
  "以無所得故菩提薩埵依般若波羅蜜多故心無罣礙無罣礙故無有恐怖遠離一切顛倒夢想究竟涅槃"
  "三世諸仏依般若波羅蜜多故得阿耨多羅三藐三菩提故知般若波羅蜜多是大神呪是大明呪是無上呪"
  "是無等等呪能除一切苦真実不虚故説般若波羅蜜多呪即説呪曰羯諦羯諦波羅羯諦波羅僧羯諦菩提薩婆訶"
  "般若心経";

static constexpr int FT     = 256;           // 炎の絵(2のべき乗)
static constexpr int FTSH   = 8;
static constexpr int FTM    = FT - 1;
static constexpr int CSTEP  = 8;             // うねりを計算する間隔(ピクセル)
static constexpr int USHIFT = 3;             // log2(CSTEP)
static constexpr int BAND   = 8;
static constexpr int NSLOT  = 4;
static constexpr int NBANK  = 4;             // 0:暗い柱 1:ふつう 2:明るい柱 3:光った文字
static constexpr int BK_FLASH = 3;
static constexpr int MAXCOL = 48;
static constexpr int MAXTXT = 300;
static constexpr int MAXGLY = 160;
static constexpr int NUL_EVERY_MIN = 5;       // 「ﾇﾙ」は、柱の流れの中に この範囲の文字数おき(ランダム)に出る
static constexpr int NUL_EVERY_MAX = 30;       // (柱ごとに並びが違う。両方 0 にすると出なくなる)
static constexpr int NWAVE  = 3;
static constexpr int NFADE  = 16;
static constexpr int NOISE  = 64;

static constexpr size_t nmOa0 = nmAlignTo(nmOv0, alignof(int)), nmOv1 = nmOa0 + sizeof(int); static int& W = *reinterpret_cast<int*>(NM_ARENA + nmOa0);
static constexpr size_t nmOa1 = nmAlignTo(nmOv1, alignof(int)), nmOv2 = nmOa1 + sizeof(int); static int& H = *reinterpret_cast<int*>(NM_ARENA + nmOa1);
static constexpr size_t nmOa2 = nmAlignTo(nmOv2, alignof(int)), nmOv3 = nmOa2 + sizeof(int); static int& S = *reinterpret_cast<int*>(NM_ARENA + nmOa2);
static constexpr size_t nmOa3 = nmAlignTo(nmOv3, alignof(int)), nmOv4 = nmOa3 + sizeof(int); static int& XOFF = *reinterpret_cast<int*>(NM_ARENA + nmOa3);
static constexpr size_t nmOa4 = nmAlignTo(nmOv4, alignof(int)), nmOv5 = nmOa4 + sizeof(int); static int& YOFF = *reinterpret_cast<int*>(NM_ARENA + nmOa4);
static constexpr size_t nmOa5 = nmAlignTo(nmOv5, alignof(int)), nmOv6 = nmOa5 + sizeof(int); static int& CW = *reinterpret_cast<int*>(NM_ARENA + nmOa5);
static constexpr size_t nmOa6 = nmAlignTo(nmOv6, alignof(int)), nmOv7 = nmOa6 + sizeof(int); static int& NB = *reinterpret_cast<int*>(NM_ARENA + nmOa6);
static constexpr size_t nmOa7 = nmAlignTo(nmOv7, alignof(float)), nmOv8 = nmOa7 + sizeof(float); static float& Sc = *reinterpret_cast<float*>(NM_ARENA + nmOa7);
static constexpr size_t nmOa8 = nmAlignTo(nmOv8, alignof(int)), nmOv9 = nmOa8 + sizeof(int); static int& CELL = *reinterpret_cast<int*>(NM_ARENA + nmOa8);
static constexpr size_t nmOa9 = nmAlignTo(nmOv9, alignof(int)), nmOv10 = nmOa9 + sizeof(int); static int& NCOL = *reinterpret_cast<int*>(NM_ARENA + nmOa9);
static constexpr size_t nmOa10 = nmAlignTo(nmOv10, alignof(int)), nmOv11 = nmOa10 + sizeof(int); static int& colX0 = *reinterpret_cast<int*>(NM_ARENA + nmOa10);
static uint8_t* bandVis;

static uint16_t* slotBuf[NSLOT];
static uint8_t*  fireTex;                    // [FT*FT] (系統<<6) | 強さ
static uint16_t* lut;                        // [NBANK][16 濃さ][256 炎]
static constexpr size_t nmOa11 = nmAlignTo(nmOv11, 16), nmOv12 = nmOa11 + sizeof(uint16_t[3][256]); static uint16_t (&cubeLut)[3][256] = *reinterpret_cast<uint16_t(*)[3][256]>(NM_ARENA + nmOa11);  // 立方体の3つの面
static uint8_t*  atlas;                      // [文字][CELL*CELL] 濃さ 0..15(にじみ込み)
static constexpr size_t nmOa12 = nmAlignTo(nmOv12, 16), nmOv13 = nmOa12 + sizeof(uint8_t[MAXTXT]); static uint8_t (&text)[MAXTXT] = *reinterpret_cast<uint8_t(*)[MAXTXT]>(NM_ARENA + nmOa12);  // 経文の並び(文字の番号)
static constexpr size_t nmOa13 = nmAlignTo(nmOv13, alignof(int)), nmOv14 = nmOa13 + sizeof(int); static int& TL = *reinterpret_cast<int*>(NM_ARENA + nmOa13);
static constexpr size_t nmOa14 = nmAlignTo(nmOv14, alignof(int)), nmOv15 = nmOa14 + sizeof(int); static int& NGLY = *reinterpret_cast<int*>(NM_ARENA + nmOa14);
static constexpr size_t nmOa15 = nmAlignTo(nmOv15, alignof(int)), nmOv16 = nmOa15 + sizeof(int); static int& NULG = *reinterpret_cast<int*>(NM_ARENA + nmOa15);  // 「ﾇﾙ」の字の番号(経文の字の後ろに1つ足す)
static uint32_t (&nulMask)[MAXCOL][(MAXTXT + 31) / 32] = *reinterpret_cast<uint32_t(*)[MAXCOL][(MAXTXT + 31) / 32]>(NM_ARENA + 0);   // 共通の置き場   // 柱 c の、経文の番号 i の字を「ﾇﾙ」にするなら 1
static constexpr size_t nmOa16 = nmAlignTo(nmOv16, 16), nmOv17 = nmOa16 + sizeof(uint8_t[NFADE][64]); static uint8_t (&fadeTab)[NFADE][64] = *reinterpret_cast<uint8_t(*)[NFADE][64]>(NM_ARENA + nmOa16);  // 炎の強さを、場所ごとに暗くする表
static uint8_t*  rowFade;                    // [S] 行ごとの暗さ
static float*    gU;  static float* gV;      // [CW*CW] 炎を読む位置
static float*    dX;  static float* dY;      // 指で動かした分
static int16_t*  spanS;
static int16_t*  spanE;
static int16_t*  bandX0;
static int16_t*  bandW;
static float*    noiseT;
static constexpr size_t nmOa17 = nmAlignTo(nmOv17, alignof(uint16_t)), nmOv18 = nmOa17 + sizeof(uint16_t); static uint16_t& blackCol = *reinterpret_cast<uint16_t*>(NM_ARENA + nmOa17);
static constexpr size_t nmOa18 = nmAlignTo(nmOv18, alignof(uint16_t)), nmOv19 = nmOa18 + sizeof(uint16_t); static uint16_t& edgeCol = *reinterpret_cast<uint16_t*>(NM_ARENA + nmOa18);

struct Scratch { int32_t* rx; int32_t* ry; uint8_t* fire; };
static constexpr size_t nmOa19 = nmAlignTo(nmOv19, 16), nmOv20 = nmOa19 + sizeof(Scratch[2]); static Scratch (&scr)[2] = *reinterpret_cast<Scratch(*)[2]>(NM_ARENA + nmOa19);
static constexpr size_t nmOa20 = nmAlignTo(nmOv20, 16), nmOv21 = nmOa20 + sizeof(uint32_t[2]); static uint32_t (&busyUs)[2] = *reinterpret_cast<uint32_t(*)[2]>(NM_ARENA + nmOa20);

static SemaphoreHandle_t semGo, semDone;
static volatile int readyW = -1, doneUpTo = -1;   // コア0が描き終えた帯 / 画面へ送り終えた帯
static int fbState = -1;  // まだ調べていない / 0: 直接送る / 1: StopWatch の置き場あり
static constexpr size_t nmOa21 = nmAlignTo(nmOv21, alignof(bool)), nmOv22 = nmOa21 + sizeof(bool); static bool& fbMode = *reinterpret_cast<bool*>(NM_ARENA + nmOa21);  // StopWatch: 帯ごとに丸の中の幅だけ送る
static int SPLIT = 2;  // コア1は SPLIT 本に1本の帯を描く(残りはコア0)
static constexpr size_t nmOa22 = nmAlignTo(nmOv22, alignof(uint32_t)), nmOv23 = nmOa22 + sizeof(uint32_t); static uint32_t& sendUs = *reinterpret_cast<uint32_t*>(NM_ARENA + nmOa22);

// ---- 文字の柱
struct Column { float scroll, speed, ph, w; int scrollI, start; int8_t base, level; };
static constexpr size_t nmOa23 = nmAlignTo(nmOv23, 16), nmOv24 = nmOa23 + sizeof(Column[MAXCOL]); static Column (&col)[MAXCOL] = *reinterpret_cast<Column(*)[MAXCOL]>(NM_ARENA + nmOa23);
static float speedMul = 1.0f;
static int speedIdx = 1;

// ---- タップの光の輪
struct Wave { float x, y, t0; bool on; };
static constexpr size_t nmOa24 = nmAlignTo(nmOv24, 16), nmOv25 = nmOa24 + sizeof(Wave[NWAVE]); static Wave (&waves)[NWAVE] = *reinterpret_cast<Wave(*)[NWAVE]>(NM_ARENA + nmOa24);
static constexpr size_t nmOa25 = nmAlignTo(nmOv25, alignof(int)), nmOv26 = nmOa25 + sizeof(int); static int& waveNext = *reinterpret_cast<int*>(NM_ARENA + nmOa25);
static constexpr size_t nmOa26 = nmAlignTo(nmOv26, 16), nmOv27 = nmOa26 + sizeof(float[NWAVE]); static float (&waveR)[NWAVE] = *reinterpret_cast<float(*)[NWAVE]>(NM_ARENA + nmOa26);
static constexpr size_t nmOa27 = nmAlignTo(nmOv27, 16), nmOv28 = nmOa27 + sizeof(float[NWAVE]); static float (&waveW)[NWAVE] = *reinterpret_cast<float(*)[NWAVE]>(NM_ARENA + nmOa27);
static constexpr size_t nmOa28 = nmAlignTo(nmOv28, alignof(bool)), nmOv29 = nmOa28 + sizeof(bool); static bool& anyWave = *reinterpret_cast<bool*>(NM_ARENA + nmOa28);

// ---- 鏡の立方体(描き始める前に決めて、描いている間は読むだけ)
struct Face { int y0, y1; int32_t U0, Ux, Uy, V0, Vx, Vy; };
static constexpr size_t nmOa29 = nmAlignTo(nmOv29, 16), nmOv30 = nmOa29 + sizeof(Face[3]); static Face (&face)[3] = *reinterpret_cast<Face(*)[3]>(NM_ARENA + nmOa29);
static constexpr size_t nmOa30 = nmAlignTo(nmOv30, alignof(int)), nmOv31 = nmOa30 + sizeof(int); static int& nFace = *reinterpret_cast<int*>(NM_ARENA + nmOa30);
static int16_t* faceL[3];                    // [S] 面ごと・行ごとの左端 / 右端
static int16_t* faceR[3];
struct Edge { float x0, y0, x1, y1; };
static constexpr size_t nmOa31 = nmAlignTo(nmOv31, 16), nmOv32 = nmOa31 + sizeof(Edge[12]); static Edge (&edges)[12] = *reinterpret_cast<Edge(*)[12]>(NM_ARENA + nmOa31);
static constexpr size_t nmOa32 = nmAlignTo(nmOv32, alignof(int)), nmOv33 = nmOa32 + sizeof(int); static int& nEdge = *reinterpret_cast<int*>(NM_ARENA + nmOa32);

// ---- 蝶
struct Fly { float x, y, vx, vy, ph, hz, head, R, tx, ty, wand; int sx0, sy0; };
static constexpr size_t nmOa33 = nmAlignTo(nmOv33, 16), nmOv34 = nmOa33 + sizeof(Fly[NFLY]); static Fly (&fly)[NFLY] = *reinterpret_cast<Fly(*)[NFLY]>(NM_ARENA + nmOa33);
static constexpr size_t nmOa34 = nmAlignTo(nmOv34, alignof(int)), nmOv35 = nmOa34 + sizeof(int); static int& SP = *reinterpret_cast<int*>(NM_ARENA + nmOa34);  // 蝶の絵の一辺
static uint16_t* sprC;                       // [NFLY][SP*SP] 色
static uint8_t*  sprA;                       // [NFLY][SP*SP] 1 = 羽の中

static constexpr size_t nmOa35 = nmAlignTo(nmOv35, alignof(float)), nmOv36 = nmOa35 + sizeof(float); static float& simT = *reinterpret_cast<float*>(NM_ARENA + nmOa35);
static constexpr size_t nmOa36 = nmAlignTo(nmOv36, alignof(float)), nmOv37 = nmOa36 + sizeof(float); static float& fPX = *reinterpret_cast<float*>(NM_ARENA + nmOa36);
static constexpr size_t nmOa37 = nmAlignTo(nmOv37, alignof(float)), nmOv38 = nmOa37 + sizeof(float); static float& fPY = *reinterpret_cast<float*>(NM_ARENA + nmOa37);
static constexpr size_t nmOa38 = nmAlignTo(nmOv38, alignof(float)), nmOv39 = nmOa38 + sizeof(float); static float& stillT = *reinterpret_cast<float*>(NM_ARENA + nmOa38);
static constexpr size_t nmOa39 = nmAlignTo(nmOv39, alignof(bool)), nmOv40 = nmOa39 + sizeof(bool); static bool& fWas = *reinterpret_cast<bool*>(NM_ARENA + nmOa39);
static constexpr size_t nmOa40 = nmAlignTo(nmOv40, alignof(float)), nmOv41 = nmOa40 + sizeof(float); static float& driftV = *reinterpret_cast<float*>(NM_ARENA + nmOa40);
static constexpr size_t nmOa41 = nmAlignTo(nmOv41, alignof(float)), nmOv42 = nmOa41 + sizeof(float); static float& cubeT = *reinterpret_cast<float*>(NM_ARENA + nmOa41);

static constexpr size_t nmOa42 = nmAlignTo(nmOv42, alignof(int)), nmOv43 = nmOa42 + sizeof(int); static int& theme = *reinterpret_cast<int*>(NM_ARENA + nmOa42);
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
static inline Col addC(Col a, Col b) { return {a.r + b.r, a.g + b.g, a.b + b.b}; }
static inline float soft(float v) { return v <= 0 ? 0 : v * 1.22f / (1.0f + 0.22f * v); }   // 明るすぎる所をやわらかく
static inline uint16_t toPix(Col c) {
  uint16_t col565 = M5.Display.color565(clamp01(soft(c.r)) * 255, clamp01(soft(c.g)) * 255, clamp01(soft(c.b)) * 255);
  return __builtin_bswap16(col565);           // 色がおかしい場合はこの bswap を外す
}
static inline float frand() { return (esp_random() & 0xFFFF) / 65535.0f; }
static inline float frand(float a, float b) { return a + (b - a) * frand(); }

// うねり用のノイズ(格子の上を等間隔に読むので、読む場所をまとめて先に出す)
struct NMap { uint16_t* i0; uint16_t* i1; float* f; };
static constexpr size_t nmOa43 = nmAlignTo(nmOv43, 16), nmOv44 = nmOa43 + sizeof(NMap[4]); static NMap (&nmX)[4] = *reinterpret_cast<NMap(*)[4]>(NM_ARENA + nmOa43);
static constexpr size_t nmOa44 = nmAlignTo(nmOv44, 16), nmOv45 = nmOa44 + sizeof(NMap[4]); static NMap (&nmY)[4] = *reinterpret_cast<NMap(*)[4]>(NM_ARENA + nmOa44);
static void buildNMap(NMap& m, float scale, float off, int n) {
  for (int g = 0; g < n; ++g) {
    float v = g * scale + off, fl = floorf(v);
    int i0 = (int)fl & (NOISE - 1);
    m.i0[g] = (uint16_t)i0; m.i1[g] = (uint16_t)((i0 + 1) & (NOISE - 1));
    m.f[g] = v - fl;
  }
}
static inline float noiseAt(const NMap& mx, const NMap& my, int gx, int gy) {
  const float* t0 = noiseT + my.i0[gy] * NOISE;
  const float* t1 = noiseT + my.i1[gy] * NOISE;
  const int x0 = mx.i0[gx], x1 = mx.i1[gx];
  const float fx = mx.f[gx];
  float top = t0[x0] + (t0[x1] - t0[x0]) * fx;
  float bot = t1[x0] + (t1[x1] - t1[x0]) * fx;
  return top + (bot - top) * my.f[gy];
}
static void makeNoise(float* t, int passes) {
  float* tmp = (float*)malloc(NOISE * NOISE * sizeof(float));
  if (!tmp) return;
  for (int i = 0; i < NOISE * NOISE; ++i) t[i] = frand() * 2 - 1;
  for (int pass = 0; pass < passes; ++pass) {
    for (int y = 0; y < NOISE; ++y)
      for (int x = 0; x < NOISE; ++x) {
        float s = 0;
        for (int d = -1; d <= 1; ++d) s += t[y * NOISE + ((x + d) & (NOISE - 1))];
        tmp[y * NOISE + x] = s / 3;
      }
    for (int y = 0; y < NOISE; ++y)
      for (int x = 0; x < NOISE; ++x) {
        float s = 0;
        for (int d = -1; d <= 1; ++d) s += tmp[((y + d) & (NOISE - 1)) * NOISE + x];
        t[y * NOISE + x] = s / 3;
      }
  }
  float mx = 0;
  for (int i = 0; i < NOISE * NOISE; ++i) mx = fmaxf(mx, fabsf(t[i]));
  if (mx > 0) for (int i = 0; i < NOISE * NOISE; ++i) t[i] /= mx;
  free(tmp);
}

// ---------------------------------------------------------------- 色の組み合わせ
// h[系統][4] … 炎の強さ 0 / 1/3 / 2/3 / 1 のときの色
//   系統 0: ふつうの炎  1: 白熱した芯  2: 青い光  3: 赤紫の筋
struct Pal { Col h[4][4]; Col gold, goldHi; };
static const Pal PALS[] = {
  // 炎と金(写真の雰囲気)
  { { { {0.00f,0.00f,0.01f}, {0.35f,0.03f,0.02f}, {0.92f,0.26f,0.05f}, {1.00f,0.78f,0.35f} },
      { {0.10f,0.04f,0.02f}, {0.95f,0.55f,0.20f}, {1.00f,0.88f,0.60f}, {1.00f,0.98f,0.90f} },
      { {0.00f,0.01f,0.03f}, {0.03f,0.10f,0.35f}, {0.15f,0.45f,0.95f}, {0.70f,0.90f,1.00f} },
      { {0.01f,0.00f,0.02f}, {0.25f,0.03f,0.18f}, {0.70f,0.12f,0.35f}, {1.00f,0.55f,0.70f} } },
    {0.95f,0.74f,0.36f}, {1.00f,0.95f,0.78f} },
  // 蒼(青い炎と銀の文字)
  { { { {0.00f,0.00f,0.01f}, {0.02f,0.10f,0.30f}, {0.10f,0.45f,0.85f}, {0.65f,0.90f,1.00f} },
      { {0.05f,0.08f,0.12f}, {0.45f,0.70f,0.95f}, {0.85f,0.95f,1.00f}, {1.00f,1.00f,1.00f} },
      { {0.00f,0.02f,0.02f}, {0.02f,0.25f,0.25f}, {0.10f,0.70f,0.65f}, {0.70f,1.00f,0.95f} },
      { {0.01f,0.00f,0.03f}, {0.15f,0.05f,0.35f}, {0.45f,0.25f,0.85f}, {0.85f,0.75f,1.00f} } },
    {0.78f,0.87f,0.98f}, {0.95f,0.98f,1.00f} },
  // 紅桜(桃色の炎と、白い文字)
  { { { {0.01f,0.00f,0.01f}, {0.35f,0.02f,0.15f}, {0.95f,0.30f,0.55f}, {1.00f,0.80f,0.90f} },
      { {0.10f,0.05f,0.08f}, {0.95f,0.60f,0.75f}, {1.00f,0.88f,0.93f}, {1.00f,0.98f,1.00f} },
      { {0.00f,0.00f,0.03f}, {0.10f,0.05f,0.35f}, {0.40f,0.30f,0.95f}, {0.85f,0.80f,1.00f} },
      { {0.02f,0.00f,0.00f}, {0.40f,0.02f,0.02f}, {0.85f,0.12f,0.10f}, {1.00f,0.60f,0.40f} } },
    {1.00f,0.80f,0.85f}, {1.00f,0.96f,0.97f} },
  // 墨(煙と朱、金の文字)
  { { { {0.00f,0.00f,0.00f}, {0.12f,0.11f,0.10f}, {0.45f,0.42f,0.38f}, {0.92f,0.88f,0.80f} },
      { {0.05f,0.05f,0.05f}, {0.55f,0.52f,0.48f}, {0.85f,0.82f,0.76f}, {1.00f,0.98f,0.94f} },
      { {0.00f,0.00f,0.01f}, {0.08f,0.10f,0.14f}, {0.35f,0.40f,0.48f}, {0.80f,0.85f,0.92f} },
      { {0.02f,0.00f,0.00f}, {0.30f,0.02f,0.02f}, {0.75f,0.10f,0.08f}, {1.00f,0.50f,0.40f} } },
    {0.95f,0.78f,0.40f}, {1.00f,0.95f,0.80f} },
};
static constexpr int NPAL = sizeof(PALS) / sizeof(PALS[0]);

static Col fireColor(int f) {
  const Pal& P = PALS[theme];
  const int hue = f >> 6;
  const float t = (f & 63) / 63.0f * 3.0f;
  const int k = t >= 3.0f ? 2 : (int)t;
  return lerpC(P.h[hue][k], P.h[hue][k + 1], t - k);
}

// 色の表: [明るさの段][文字の濃さ 0..15][炎の番号 0..255] → 画面の色
static void buildLut() {
  const Pal& P = PALS[theme];
  static const float textGain[NBANK] = {0.55f, 0.88f, 1.20f, 1.75f};
  Col fc[256];
  for (int f = 0; f < 256; ++f) fc[f] = fireColor(f);
  for (int b = 0; b < NBANK; ++b)
    for (int a = 0; a < 16; ++a) {
      const float al = a / 15.0f;
      for (int f = 0; f < 256; ++f) {
        const int hue = f >> 6;
        const float I = (f & 63) / 63.0f;
        Col F = fc[f];                                   // 炎の明るさは全部の段で同じ(柱の境目が出ないように)
        // 文字は後ろの炎に照らされて光る。青い光の前では、白っぽく冷たい光になる
        const float lit = 0.26f + 0.95f * I;
        Col T = lerpC(P.gold, P.goldHi, 0.35f * I);
        if (hue == 2) T = lerpC(T, {0.80f, 0.92f, 1.00f}, 0.55f * I);
        if (hue == 1) T = lerpC(T, P.goldHi, 0.6f);
        T = mulC(T, lit * textGain[b]);
        if (b == BK_FLASH) T = addC(T, mulC(P.goldHi, 0.35f));
        Col c = addC(mulC(F, 1.0f - 0.80f * al), mulC(T, al));
        lut[(b << 12) | (a << 8) | f] = toPix(c);
      }
    }
  // 鏡の立方体: 炎を暗く、少し青いガラスのように映す(面ごとに明るさを変える)
  static const float shade[3] = {1.05f, 0.62f, 0.34f};
  for (int k = 0; k < 3; ++k)
    for (int f = 0; f < 256; ++f)
      cubeLut[k][f] = toPix(addC(mulC(fc[f], 0.85f * shade[k]), {0.015f * shade[k], 0.02f * shade[k], 0.04f * shade[k]}));
  blackCol = toPix({0, 0, 0});
  edgeCol  = toPix(mulC(P.goldHi, 0.30f));
}

// ---------------------------------------------------------------- 炎の絵(最初に1回だけ)
// 格子の値をなめらかにつないだノイズ。格子の数が絵の幅を割り切るので、端がぴったり繋がります
static inline uint32_t hash3(uint32_t x, uint32_t y, uint32_t s) {
  uint32_t h = x * 374761393u + y * 668265263u + s * 2246822519u;
  h = (h ^ (h >> 13)) * 1274126177u;
  return h ^ (h >> 16);
}
static float vnoise(float x, float y, int nx, int ny, uint32_t seed) {   // x, y: 0..1 で1周
  x *= nx; y *= ny;
  int ix = (int)floorf(x), iy = (int)floorf(y);
  float fx = x - ix, fy = y - iy;
  fx = fx * fx * (3 - 2 * fx); fy = fy * fy * (3 - 2 * fy);
  auto v = [&](int i, int j) {
    return (hash3((uint32_t)((i % nx + nx) % nx), (uint32_t)((j % ny + ny) % ny), seed) & 0xFFFF) / 65535.0f;
  };
  float a = v(ix, iy), b = v(ix + 1, iy), c = v(ix, iy + 1), d = v(ix + 1, iy + 1);
  return (a + (b - a) * fx) + ((c + (d - c) * fx) - (a + (b - a) * fx)) * fy;
}
static void buildFire() {
  const uint32_t s0 = esp_random();
  for (int y = 0; y < FT; ++y)
    for (int x = 0; x < FT; ++x) {
      const float u = (float)x / FT, v = (float)y / FT;
      // 炎: もくもくした雲(大小の格子を重ねる)。濃い所と暗い隙間がはっきり分かれるように
      float n = 0.46f * vnoise(u, v, 6, 5, s0) + 0.27f * vnoise(u, v, 12, 10, s0 + 1)
              + 0.16f * vnoise(u, v, 24, 20, s0 + 2) + 0.11f * vnoise(u, v, 48, 40, s0 + 3);
      float m = 0.40f * vnoise(u, v, 8, 8, s0 + 7) + 0.60f * vnoise(u, v, 32, 24, s0 + 8);   // 青は細かいかけら
      float I = smooth3((n - 0.37f) / 0.36f);
      I = powf(I, 1.10f);
      int hue = 0; float val = I;
      float blue = smooth3((m - 0.70f) / 0.16f) * (0.40f + 0.50f * n);
      if (blue > I * 1.2f && blue > 0.10f) { hue = 2; val = blue; }            // 青い光のかけら
      else if (I > 0.80f)                  { hue = 1; val = 0.35f + 0.65f * (I - 0.80f) / 0.20f; }   // 白熱
      else if (m < 0.30f && I > 0.30f)     { hue = 3; }                         // 赤紫の筋
      int q = (int)(clamp01(val) * 63.0f + 0.5f);
      fireTex[(y << FTSH) | x] = (uint8_t)((hue << 6) | q);
    }
}

// ---------------------------------------------------------------- 柱に紛れ込む「ﾇﾙ」
// 柱 c の流れ(経文を1周すると同じ並びに戻る)に、NUL_EVERY_MIN〜MAX 文字おきに「ﾇﾙ」の印をつける。
// 1周のつなぎ目でも間が範囲に収まるように、間の数の合計がちょうど経文の長さになるように選ぶ
static void makeNulMask(int c) {
  uint32_t* m = nulMask[c];
  for (int i = 0; i < (MAXTXT + 31) / 32; ++i) m[i] = 0;
  const int lo = NUL_EVERY_MIN, hi = NUL_EVERY_MAX;
  if (lo <= 0 || hi < lo || TL < lo) return;
  const int off = (int)(esp_random() % (uint32_t)TL);   // どこから数え始めるか(柱ごとにずらす)
  int pos = 0;
  for (;;) {
    m[((pos + off) % TL) >> 5] |= 1u << (((pos + off) % TL) & 31);
    const int rem = TL - pos;                            // つなぎ目(1周先の最初の印)までの文字数
    if (rem <= hi) break;                                // 最後の間 = rem(lo 以上になるように下で選んである)
    const int top = max(lo, min(hi, rem - lo));          // 残りが lo 未満にならないように
    pos += lo + (int)(esp_random() % (uint32_t)(top - lo + 1));
  }
}

// ---------------------------------------------------------------- 経文の文字(最初に1回だけ)
static uint32_t nextCp(const char*& p) {
  const unsigned char* q = (const unsigned char*)p;
  uint32_t c = q[0];
  if (c < 0x80)      { p += 1; return c; }
  if (c < 0xE0)      { p += 2; return ((c & 0x1F) << 6) | (q[1] & 0x3F); }
  if (c < 0xF0)      { p += 3; return ((c & 0x0F) << 12) | ((q[1] & 0x3F) << 6) | (q[2] & 0x3F); }
  p += 4; return ((c & 0x07) << 18) | ((q[1] & 0x3F) << 12) | ((q[2] & 0x3F) << 6) | (q[3] & 0x3F);
}
static void toUtf8(uint32_t c, char* o) {
  if (c < 0x80) { o[0] = (char)c; o[1] = 0; }
  else if (c < 0x800) { o[0] = (char)(0xC0 | (c >> 6)); o[1] = (char)(0x80 | (c & 0x3F)); o[2] = 0; }
  else { o[0] = (char)(0xE0 | (c >> 12)); o[1] = (char)(0x80 | ((c >> 6) & 0x3F)); o[2] = (char)(0x80 | (c & 0x3F)); o[3] = 0; }
}
// M5GFX の明朝体(lgfxJapanMincho_20)に入っていない字: 蘊 埵 罣 礙 耨 藐 羯
// Noto Serif CJK JP Light を、明朝体と同じ 20px・白黒(ヒンティングつき)で描いて、M5GFX と同じ u8g2 形式にしたもの。
// 太さ・大きさ・基準線の位置も明朝体にそろえてあるので、拡大縮小や位置合わせは M5GFX がほかの字と同じようにしてくれます。
// Noto Serif CJK: Copyright 2017 Adobe (http://www.adobe.com/), SIL Open Font License 1.1
static const uint32_t SUTRA_EXTRA_CP[] = {0x860A, 0x57F5, 0x7F63, 0x7919, 0x8028, 0x85D0, 0x7FAF};
#if defined(ESP_PLATFORM)
static const uint8_t SUTRA_EXTRA_FONT[] = {
  0x07, 0x00, 0x03, 0x02, 0x05, 0x05, 0x06, 0x06, 0x06, 0x14, 0x15, 0x00, 0xFD, 0x10, 0xFE, 0x11, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00,
  0x00, 0x00, 0x04, 0xFF, 0xFF, 0x57, 0xF5, 0x3B, 0x12, 0x86, 0x20, 0x7D, 0x1C, 0x09, 0x75, 0x64, 0xC9, 0xB2, 0x61, 0x47, 0x72, 0x24, 0x47, 0xB2,
  0x61, 0x18, 0x86, 0x61, 0x8A, 0xB2, 0x30, 0x8D, 0xB2, 0x30, 0x8D, 0xB2, 0x24, 0x8A, 0x86, 0x61, 0x18, 0xA2, 0x34, 0xCA, 0xC2, 0x34, 0xCA, 0xC2,
  0x2D, 0xCA, 0x94, 0x6D, 0x18, 0x86, 0x21, 0xC7, 0x72, 0x1C, 0xCB, 0xC1, 0x61, 0x18, 0x86, 0x00, 0x79, 0x19, 0x41, 0x13, 0x86, 0x20, 0x7D, 0x1C,
  0xC7, 0xA6, 0x61, 0x50, 0x86, 0x61, 0x88, 0xC3, 0x4C, 0x4D, 0xD2, 0x4C, 0xCA, 0xE2, 0x70, 0xD0, 0xC2, 0x34, 0x1B, 0x86, 0x68, 0x50, 0x86, 0x2C,
  0x89, 0xA4, 0x24, 0x49, 0x43, 0x49, 0xCA, 0x92, 0x34, 0x4A, 0x06, 0x25, 0x19, 0xA2, 0x28, 0xCB, 0x92, 0x34, 0xCA, 0x14, 0x25, 0x1D, 0xA2, 0x68,
  0x49, 0xA3, 0x24, 0x4C, 0xD4, 0x2C, 0xCC, 0x06, 0x01, 0x7F, 0x63, 0x35, 0xF2, 0x85, 0x20, 0x9D, 0x61, 0x18, 0x86, 0x21, 0xD2, 0xC2, 0x2C, 0xD3,
  0xC2, 0x2C, 0x1B, 0x86, 0x61, 0xD8, 0x54, 0x35, 0xC7, 0xC4, 0x78, 0x18, 0x86, 0x21, 0xC7, 0x74, 0x68, 0x18, 0x86, 0x61, 0xC8, 0xA1, 0x1C, 0xC7,
  0x74, 0x6C, 0x18, 0x86, 0x41, 0xC7, 0x74, 0x1C, 0xD2, 0x91, 0x61, 0x18, 0x86, 0x61, 0x7F, 0xAF, 0x3F, 0x32, 0x86, 0x1F, 0x7D, 0x1C, 0xCD, 0x94,
  0x61, 0xD8, 0xA2, 0x28, 0x0E, 0x93, 0x24, 0x89, 0xA3, 0x61, 0x18, 0x86, 0x21, 0x0D, 0xE3, 0x34, 0x4A, 0x86, 0x41, 0x1B, 0xB6, 0x1C, 0x4B, 0x87,
  0x61, 0x8B, 0x86, 0x28, 0x53, 0x06, 0x45, 0xD2, 0x92, 0x28, 0x4A, 0xA2, 0x24, 0x0A, 0xC3, 0x24, 0x4B, 0xC2, 0x70, 0x18, 0x92, 0x2C, 0xCD, 0xA3,
  0x1C, 0x56, 0x72, 0x74, 0x02, 0x80, 0x28, 0x3E, 0x32, 0x86, 0x1F, 0xBD, 0x1C, 0xC7, 0xC2, 0x61, 0x18, 0x87, 0x24, 0x87, 0x86, 0x70, 0x18, 0xC6,
  0x30, 0x4F, 0x86, 0x21, 0x19, 0x86, 0x21, 0x0B, 0x93, 0x28, 0x49, 0xC3, 0x24, 0x93, 0x86, 0x21, 0x19, 0x34, 0x49, 0x4C, 0xD3, 0x2D, 0x4D, 0x95,
  0x68, 0x18, 0x86, 0x24, 0x89, 0xA4, 0x2C, 0x8B, 0xB2, 0x2C, 0x8A, 0xA3, 0x30, 0x8A, 0xA3, 0x3C, 0xCE, 0x91, 0x0D, 0x85, 0xD0, 0x3C, 0x32, 0x86,
  0x1F, 0x5D, 0x35, 0xC7, 0xE2, 0x2C, 0x19, 0x86, 0x61, 0x18, 0x76, 0x1C, 0x47, 0x54, 0x7D, 0x1C, 0x06, 0x65, 0x4A, 0x92, 0x34, 0x54, 0xA4, 0x34,
  0x0E, 0x87, 0x21, 0x54, 0xB2, 0x34, 0x8B, 0xB4, 0x61, 0x48, 0xA3, 0x28, 0x49, 0x92, 0x4C, 0x12, 0x93, 0x5C, 0xC9, 0xA2, 0x2C, 0xCA, 0x22, 0x29,
  0x1B, 0x93, 0x70, 0xD0, 0x96, 0x1C, 0x03, 0x86, 0x0A, 0x46, 0x32, 0x86, 0x1F, 0xDD, 0x38, 0xC7, 0xE2, 0x2C, 0x19, 0x86, 0x61, 0x18, 0xD6, 0x38,
  0x47, 0xB4, 0x61, 0x10, 0xA3, 0x28, 0x8B, 0xA4, 0x48, 0x8A, 0xB2, 0x30, 0xC9, 0xA2, 0x25, 0x4D, 0xA2, 0x24, 0x0C, 0x23, 0x65, 0x18, 0xA4, 0x41,
  0x49, 0x72, 0x34, 0x1C, 0x86, 0x29, 0x49, 0xA2, 0x24, 0x8A, 0xA2, 0x24, 0x4A, 0x92, 0x28, 0x4A, 0xA2, 0x28, 0x49, 0xA2, 0x28, 0x0C, 0x93, 0x28,
  0xD2, 0xA2, 0x61, 0x18, 0x06, 0x00, 0x00,
};
static const lgfx::U8g2font sutraExtraFont(SUTRA_EXTRA_FONT);
#endif
static bool isExtraGlyph(uint32_t cp) {
  for (uint32_t e : SUTRA_EXTRA_CP) if (e == cp) return true;
  return false;
}

// 日本語のフォントは1サイズでも大きい(数百KB〜)。いくつも使うとプログラムが
// フラッシュの区画に収まらなくなるので、明朝の 20px だけを使い、拡大縮小して描きます。
// (2倍の大きさで描いてから 2x2 を1ピクセルにまとめるので、ふちはなめらかになります)
static void setMincho(M5Canvas& c, int px, uint32_t cp = 0) {
  c.setFont(&fonts::lgfxJapanMincho_20);
#if defined(ESP_PLATFORM)
  if (isExtraGlyph(cp)) c.setFont(&sutraExtraFont);   // 明朝体に入っていない字
#endif
  c.setTextSize(px / 20.0f);
}
static int inkOf(M5Canvas& c, int n) {
  const uint16_t* b = (const uint16_t*)c.getBuffer();
  int k = 0;
  for (int i = 0; i < n * n; ++i) k += (b[i] != 0);
  return k;
}
static bool buildGlyphs() {
  // 経文 → 文字の番号(同じ字は同じ番号)
  uint32_t uniq[MAXGLY];
  TL = 0; NGLY = 0;
  for (const char* p = SUTRA; *p && TL < MAXTXT;) {
    uint32_t cp = nextCp(p);
    int g = -1;
    for (int i = 0; i < NGLY; ++i) if (uniq[i] == cp) { g = i; break; }
    if (g < 0) { if (NGLY >= MAXGLY) continue; uniq[NGLY] = cp; g = NGLY++; }
    text[TL++] = (uint8_t)g;
  }
  NULG = NGLY;                                     // 「ﾇﾙ」(半角2文字で、全角1字ぶんの幅)
  const int NG = NGLY + 1;
  atlas = (uint8_t*)ps_malloc((size_t)NG * CELL * CELL);
  if (!atlas) atlas = (uint8_t*)malloc((size_t)NG * CELL * CELL);
  if (!atlas) return false;

  // 2倍の大きさで描いて、2x2 を1ピクセルにまとめる(ふちがなめらかになる)
  const int N = CELL * 2;
  const int want = (CELL - 3) * 2;
  M5Canvas cv(&M5.Display);
  cv.setColorDepth(16);
  if (!cv.createSprite(N, N)) return false;
  float* core = (float*)malloc(CELL * CELL * sizeof(float));
  float* tmp  = (float*)malloc(CELL * CELL * sizeof(float));
  float* glow = (float*)malloc(CELL * CELL * sizeof(float));
  if (!core || !tmp || !glow) { free(core); free(tmp); free(glow); cv.deleteSprite(); return false; }
  int missing = 0;
  for (int g = 0; g < NG; ++g) {
    char s[8];
    if (g < NGLY) toUtf8(uniq[g], s); else strcpy(s, "ﾇﾙ");   // 半角2文字で、全角1字ぶんの幅
    cv.fillSprite(0); setMincho(cv, want, g < NGLY ? uniq[g] : 0);
    cv.setTextColor(0xFFFF); cv.setTextDatum(middle_center);
    cv.drawString(s, N / 2, N / 2);
    if (inkOf(cv, N) == 0) ++missing;          // 何も描かれなかった字(ふつうは 0。フォントにない字は「□」になるので、ここでは数えられない)
    const uint16_t* b = (const uint16_t*)cv.getBuffer();
    for (int y = 0; y < CELL; ++y)
      for (int x = 0; x < CELL; ++x) {
        int k = (b[(2 * y) * N + 2 * x] != 0) + (b[(2 * y) * N + 2 * x + 1] != 0)
              + (b[(2 * y + 1) * N + 2 * x] != 0) + (b[(2 * y + 1) * N + 2 * x + 1] != 0);
        core[y * CELL + x] = k * 0.25f;
      }
    // にじみ: 5x5 の箱で2回ぼかす
    for (int pass = 0; pass < 2; ++pass) {
      const float* src = pass ? glow : core;
      for (int y = 0; y < CELL; ++y)
        for (int x = 0; x < CELL; ++x) {
          float a = 0;
          for (int d = -2; d <= 2; ++d) { int xx = x + d; if (xx >= 0 && xx < CELL) a += src[y * CELL + xx]; }
          tmp[y * CELL + x] = a * 0.2f;
        }
      for (int y = 0; y < CELL; ++y)
        for (int x = 0; x < CELL; ++x) {
          float a = 0;
          for (int d = -2; d <= 2; ++d) { int yy = y + d; if (yy >= 0 && yy < CELL) a += tmp[yy * CELL + x]; }
          glow[y * CELL + x] = a * 0.2f;
        }
    }
    uint8_t* dst = atlas + (size_t)g * CELL * CELL;
    for (int i = 0; i < CELL * CELL; ++i) {
      float a = fmaxf(core[i], fminf(1.0f, glow[i] * 1.6f) * TEXT_GLOW);
      dst[i] = (uint8_t)(clamp01(a) * 15.0f + 0.5f);
    }
  }
  free(core); free(tmp); free(glow);
  cv.deleteSprite();
  Serial.printf("sutra: %d chars, %d kinds, cell %d px, missing glyphs %d\n", TL, NGLY, CELL, missing);
  return true;
}

// ---------------------------------------------------------------- 蝶
static void spawnFly(Fly& f, int i) {
  f.x = Sc + frand(-0.35f, 0.35f) * S;
  f.y = S * frand(0.62f, 0.86f);
  f.vx = frand(-20, 20); f.vy = frand(-10, 10);
  f.ph = frand(0, 6.2832f); f.hz = frand(2.6f, 3.6f);
  f.head = frand(0, 6.2832f);
  f.R = FLY_SIZE * S * frand(0.80f, 1.12f);
  f.tx = f.x; f.ty = f.y; f.wand = frand(0, 100) + i * 13.0f;
}

// 羽の形: 大きな前ばね + 丸い後ばね(楕円を少し傾けて重ねる)。
// 羽ばたきで横方向がつぶれる(横から見た向きになる)。付け根は暗く、中ほどが青く光り、ふちは黒
static void drawFlySprite(int i) {
  Fly& f = fly[i];
  uint16_t* C = sprC + (size_t)i * SP * SP;
  uint8_t*  A = sprA + (size_t)i * SP * SP;
  const float flap = 0.14f + 0.86f * fabsf(cosf(f.ph));
  const float cs = cosf(f.head), sn = sinf(f.head);
  const float invR = 1.0f / f.R, c0 = (SP - 1) * 0.5f;
  const float glint = 0.60f + 0.55f * flap;                 // 羽がこちらを向くほど青く光る
  static const Col root = {0.02f, 0.04f, 0.16f}, deep = {0.06f, 0.22f, 0.70f},
                   bright = {0.35f, 0.72f, 1.00f}, pale = {0.78f, 0.93f, 1.00f};
  // 羽の色は「羽のどのあたりか(16段)」と「付け根からの近さ(8段)」だけで決まるので、先に表にする
  // (1ピクセルずつ色を計算すると重いため)
  uint16_t pal[16][8];
  for (int a = 0; a < 16; ++a)
    for (int b = 0; b < 8; ++b) {
      const float t = a / 15.0f, fromBody = b / 7.0f;
      Col c = lerpC(deep, bright, smooth3(1.0f - t));
      c = lerpC(c, pale, 0.45f * (1.0f - t) * (1.0f - t) * flap);
      c = lerpC(root, c, smooth3(fromBody));
      pal[a][b] = toPix(mulC(c, glint));
    }
  const uint16_t bodyC = toPix({0.04f, 0.03f, 0.05f}), edgeC = toPix({0.02f, 0.02f, 0.05f}), dotC = toPix({0.80f, 0.85f, 0.95f});
  const float lim2 = (c0 + 0.5f) * (c0 + 0.5f), iflap = 1.0f / flap;
  const float ux = cs * invR, vx = -sn * invR;             // x が1増えたときの u, v の増え方
  for (int y = 0; y < SP; ++y) {
    const float dy = y - c0;
    float u = (-c0 * cs + dy * sn) * invR, v = (c0 * sn + dy * cs) * invR;   // x = 0 のとき
    for (int x = 0; x < SP; ++x, u += ux, v += vx) {
      const int k = y * SP + x;
      A[k] = 0;
      const float dx = x - c0;
      if (dx * dx + dy * dy > lim2) continue;
      const float av = v < 0 ? -v : v;
      if (av < 0.06f && u > -0.52f && u < 0.42f) { A[k] = 1; C[k] = bodyC; continue; }  // 胴
      const float vv = av * iflap;
      // 前ばね: 前ななめ外へ長くとがる楕円。後ばね: 後ろななめ外の、丸めの楕円
      const float fu = u - 0.22f, fv = vv - 0.48f;
      const float fa = fu * 0.581f + fv * 0.814f, fb = -fu * 0.814f + fv * 0.581f;     // 約 54° 回す
      const float e1 = fa * fa * 2.7778f + fb * fb * 11.111f;
      const float hu = u + 0.16f, hv = vv - 0.36f;
      const float ha = hu * -0.666f + hv * 0.746f, hb = -hu * 0.746f - hv * 0.666f;    // 約 132° 回す
      const float e2 = ha * ha * 6.25f + hb * hb * 11.111f;
      const float pu = fa;
      const float e = e1 < e2 ? e1 : e2;
      if (e >= 1.0f || vv < 0.04f) continue;
      A[k] = 1;
      if (e > 0.82f) {                                      // 黒いふち。前ばねの先に、白い小さな点
        C[k] = (e1 < e2 && pu > 0.12f && e > 0.87f && e < 0.95f && ((int)(vv * 9.0f) & 1)) ? dotC : edgeC;
      } else {
        const int a = (int)(e * 18.29f + 0.5f);              // 0 = 羽のまん中、15 = ふち
        const float fb2 = vv * 20.0f + 0.5f;                   // 付け根は暗い(0..7)
        C[k] = pal[a][fb2 > 7.0f ? 7 : (int)fb2];
      }
    }
    }
}

// ---------------------------------------------------------------- 1フレームの準備
struct Input { bool touching; bool tapped; float tx, ty; float dt; };

static void updateCube(float dt) {
  cubeT += dt;
  const float yaw = cubeT * CUBE_SPIN + 0.6f, pitch = -0.42f + 0.05f * sinf(cubeT * 0.21f);
  const float cy_ = cosf(yaw), sy_ = sinf(yaw), cp = cosf(pitch), sp = sinf(pitch);
  const float h = CUBE_SIZE * S, D = 6.0f;
  const float cx = Sc + 0.02f * S * sinf(cubeT * 0.13f), cyS = 0.30f * S + 0.012f * S * sinf(cubeT * 0.37f);
  float X[8], Y[8];
  for (int i = 0; i < 8; ++i) {
    float x = (i & 1) ? 1 : -1, y = (i & 2) ? 1 : -1, z = (i & 4) ? 1 : -1;
    float x1 = x * cy_ + z * sy_, z1 = -x * sy_ + z * cy_;          // 縦軸まわりに回す
    float y2 = y * cp - z1 * sp, z2 = y * sp + z1 * cp;              // 少し下から見上げる
    float k = D / (D - z2);
    X[i] = cx + x1 * h * k; Y[i] = cyS - y2 * h * k;
  }
  // 6つの面(頂点は外から見て一周する順)と法線
  static const int F[6][4] = {{1,3,7,5},{0,4,6,2},{2,6,7,3},{0,1,5,4},{4,5,7,6},{0,2,3,1}};
  static const float N[6][3] = {{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};
  // 明るさの順に並べる(光は左上の手前から)
  struct Vis { int f; float nz, lit; float nx, ny; };
  Vis vis[6]; int nv = 0;
  for (int f = 0; f < 6; ++f) {
    float x = N[f][0], y = N[f][1], z = N[f][2];
    float x1 = x * cy_ + z * sy_, z1 = -x * sy_ + z * cy_;
    float y2 = y * cp - z1 * sp, z2 = y * sp + z1 * cp;
    if (z2 * D <= 1.05f) continue;                               // 遠近をつけて見たとき、こちらを向いている面だけ(面の中心 = 法線、目は手前 D の所)
    float lit = -0.45f * x1 + 0.55f * y2 + 0.70f * z2;
    vis[nv++] = {f, z2, lit, x1, y2};
  }
  for (int a = 0; a < nv; ++a) for (int b = a + 1; b < nv; ++b) if (vis[b].lit > vis[a].lit) { Vis t = vis[a]; vis[a] = vis[b]; vis[b] = t; }
  nFace = min(nv, 3); nEdge = 0;
  const float s = 0.62f * 256.0f;                              // 1ピクセルあたり炎の目盛り(1/256)
  for (int k = 0; k < nFace; ++k) {
    const int f = vis[k].f;
    float qx[4], qy[4];
    for (int j = 0; j < 4; ++j) { qx[j] = X[F[f][j]]; qy[j] = Y[F[f][j]]; }
    int y0 = S, y1 = -1;
    for (int j = 0; j < 4; ++j) { y0 = min(y0, (int)floorf(qy[j])); y1 = max(y1, (int)ceilf(qy[j])); }
    y0 = max(0, y0); y1 = min(S - 1, y1);
    Face& Fc = face[k];
    Fc.y0 = y0; Fc.y1 = y1;
    for (int y = y0; y <= y1; ++y) {                           // この行で、面が左右どこからどこまでか
      float yc = y + 0.5f, xl = 1e9f, xr = -1e9f;
      for (int j = 0; j < 4; ++j) {
        float ax = qx[j], ay = qy[j], bx = qx[(j + 1) & 3], by = qy[(j + 1) & 3];
        if ((yc < ay) == (yc < by)) continue;
        float t = (yc - ay) / (by - ay), xx = ax + (bx - ax) * t;
        xl = fminf(xl, xx); xr = fmaxf(xr, xx);
      }
      if (xl > xr) { faceL[k][y] = 1; faceR[k][y] = 0; continue; }
      faceL[k][y] = (int16_t)ceilf(xl - 0.5f); faceR[k][y] = (int16_t)floorf(xr - 0.5f);
    }
    // 映り込み: 面の向きで回して裏返した炎。回ると、映る景色がすべっていく
    const float ang = atan2f(vis[k].ny, vis[k].nx);
    const float ca = cosf(ang), sa = sinf(ang);
    Fc.Ux = (int32_t)( ca * s); Fc.Uy = (int32_t)( sa * s);
    Fc.Vx = (int32_t)( sa * s); Fc.Vy = (int32_t)(-ca * s);
    Fc.U0 = (int32_t)((vis[k].nx * 120.0f + f * 41.0f) * 256.0f);
    Fc.V0 = (int32_t)((vis[k].ny * 120.0f + f * 23.0f + driftV * 0.6f) * 256.0f);
    for (int j = 0; j < 4 && nEdge < 12; ++j) edges[nEdge++] = {qx[j], qy[j], qx[(j + 1) & 3], qy[(j + 1) & 3]};
  }
}

static constexpr size_t nmOa45 = nmAlignTo(nmOv45, alignof(int)), nmOv46 = nmOa45 + sizeof(int); static int& flyTick = *reinterpret_cast<int*>(NM_ARENA + nmOa45);
static constexpr size_t nmOa46 = nmAlignTo(nmOv46, alignof(bool)), nmOv47 = nmOa46 + sizeof(bool); static bool& flyReady = *reinterpret_cast<bool*>(NM_ARENA + nmOa46);
static void updateFlies(const Input& in, float dt) {
  ++flyTick;
  for (int i = 0; i < NFLY; ++i) {
    Fly& f = fly[i];
    // 行き先: ふだんは床の近くをさまよう / 押さえていると指のまわりを回る
    f.wand += dt * 0.17f;
    float tx = Sc + 0.36f * S * sinf(f.wand * 1.3f + i) * cosf(f.wand * 0.7f);
    float ty = S * (0.72f + 0.10f * sinf(f.wand * 1.7f + i * 2.1f));
    if (in.touching && stillT > 0.35f) {                      // 止めて押さえていると、指のまわりへ
      float a = simT * 1.4f + i * 6.2832f / NFLY, r = 0.11f * S + 0.02f * S * sinf(simT * 2.0f + i);
      tx = in.tx + cosf(a) * r; ty = in.ty + sinf(a) * r * 0.8f;
    }
    float ax = (tx - f.x) * 1.6f - f.vx * 1.2f + 60.0f * sinf(simT * 2.3f + i * 1.7f);
    float ay = (ty - f.y) * 1.6f - f.vy * 1.2f + 60.0f * sinf(simT * 2.9f + i * 0.9f);
    for (int j = 0; j < NFLY; ++j) {                          // 蝶どうしは少し離れる
      if (j == i) continue;
      float ox = f.x - fly[j].x, oy = f.y - fly[j].y, d2 = ox * ox + oy * oy, want = 1.9f * f.R;
      if (d2 < want * want && d2 > 1.0f) { float d = sqrtf(d2), k = (want - d) * 9.0f; ax += ox / d * k; ay += oy / d * k; }
    }
    if (in.tapped) {                                          // 驚いて散る
      float dx = f.x - in.tx, dy = f.y - in.ty, d = sqrtf(dx * dx + dy * dy) + 1.0f;
      if (d < 0.35f * S) { f.vx += dx / d * 260.0f; f.vy += dy / d * 260.0f; }
    }
    f.vx += ax * dt; f.vy += ay * dt;
    float sp = sqrtf(f.vx * f.vx + f.vy * f.vy), lim = 0.55f * S;
    if (sp > lim) { f.vx *= lim / sp; f.vy *= lim / sp; }
    f.x += f.vx * dt; f.y += f.vy * dt;
    // 画面の丸からはみ出さない
    float dx = f.x - Sc, dy = f.y - Sc, d = sqrtf(dx * dx + dy * dy), R = 0.44f * S;
    if (d > R) { f.x = Sc + dx / d * R; f.y = Sc + dy / d * R; f.vx *= 0.5f; f.vy *= 0.5f; }
    // 向きは進む方へ、ゆっくり。速いほど羽ばたきも速い
    if (sp > 5.0f) {
      float want = atan2f(f.vy, f.vx), da = want - f.head;
      while (da > 3.14159f) da -= 6.28318f;
      while (da < -3.14159f) da += 6.28318f;
      f.head += da * fminf(1.0f, dt * 3.0f);
    }
    f.ph += dt * 6.2832f * f.hz * (0.8f + 0.6f * fminf(1.0f, sp / (0.3f * S)));
    f.sx0 = (int)f.x - SP / 2; f.sy0 = (int)f.y - SP / 2;
    if (((i + flyTick) & 1) == 0 || !flyReady) drawFlySprite(i);   // 羽の絵は1フレームおきに描き直す(軽くするため)
  }
  flyReady = true;                                            // 最初の1回だけは全部描く
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
  fPX = in.tx; fPY = in.ty; fWas = in.touching;

  if (in.tapped) {
    waves[waveNext] = {in.tx, in.ty, simT, true};
    waveNext = (waveNext + 1) % NWAVE;
  }
  anyWave = false;
  for (int i = 0; i < NWAVE; ++i) {
    if (!waves[i].on) continue;
    float age = simT - waves[i].t0;
    if (age > 1.8f) { waves[i].on = false; continue; }
    waveR[i] = age * RIPPLE * S;
    waveW[i] = 0.045f * S * (1.0f + age);
    anyWave = true;
  }

  // ---- 文字の柱: 流れて、ゆっくり明るさが変わる
  const int loop = TL * CELL;
  for (int c = 0; c < NCOL; ++c) {
    Column& C = col[c];
    C.scroll += C.speed * speedMul * dt;
    while (C.scroll >= loop) C.scroll -= loop;
    while (C.scroll < 0) C.scroll += loop;
    C.scrollI = (int)C.scroll;
    float s = sinf(simT * C.w + C.ph);
    C.level = (int8_t)max(0, min(2, C.base + (s > 0.55f ? 1 : (s < -0.55f ? -1 : 0))));
  }

  // ---- 炎: 立ちのぼりながら、うねる。指で動かした分は、ゆっくり戻る
  driftV += dt * FIRE_RISE * FIRE_SCALE;
  const float relax = expf(-dt * STIR_KEEP);
  const float rad = 0.16f * S, inv2 = 1.0f / (2 * rad * rad), far = 9.0f * rad * rad;
  for (int i = 0; i < CW * CW; ++i) { dX[i] *= relax; dY[i] *= relax; }
  if (in.touching && (fvx != 0 || fvy != 0)) {
    for (int gy = 0; gy < CW; ++gy)
      for (int gx = 0; gx < CW; ++gx) {
        float x = gx * CSTEP - in.tx, y = gy * CSTEP - in.ty, d2 = x * x + y * y;
        if (d2 > far) continue;
        float g = expf(-d2 * inv2) * STIR * FIRE_SCALE * dt;
        dX[gy * CW + gx] += fvx * g; dY[gy * CW + gx] += fvy * g;
      }
  }
  const float sc = 5.0f / S * CSTEP, tt = simT * FIRE_SPEED * 10.0f;
  buildNMap(nmX[0], sc,        tt,        CW);  buildNMap(nmY[0], sc,        -tt * 0.6f, CW);
  buildNMap(nmX[1], sc * 2.6f, 31,        CW);  buildNMap(nmY[1], sc * 2.6f, -tt * 1.4f, CW);
  buildNMap(nmX[2], sc,       -17,        CW);  buildNMap(nmY[2], sc,        tt,         CW);
  buildNMap(nmX[3], sc * 2.6f, tt * 1.1f, CW);  buildNMap(nmY[3], sc * 2.6f, 11,         CW);
  for (int gy = 0; gy < CW; ++gy) {
    float* pu = gU + gy * CW; float* pv = gV + gy * CW;
    const float y = (float)(gy * CSTEP);
    for (int gx = 0; gx < CW; ++gx) {
      float nx = noiseAt(nmX[0], nmY[0], gx, gy) + 0.5f * noiseAt(nmX[1], nmY[1], gx, gy);
      float ny = noiseAt(nmX[2], nmY[2], gx, gy) + 0.5f * noiseAt(nmX[3], nmY[3], gx, gy);
      const int q = gy * CW + gx;
      pu[gx] = (gx * CSTEP) * FIRE_SCALE + FIRE_FLOW * nx - dX[q];
      pv[gx] = y * FIRE_SCALE + driftV + FIRE_FLOW * ny - dY[q];
    }
  }

  updateCube(dt);
  updateFlies(in, dt);
}

// ---------------------------------------------------------------- 描画(帯ごと)
// タップの光の輪が、この文字(のまん中)を通っているか
static inline int flashOf(float cx, float cy) {
  int best = 0;
  for (int i = 0; i < NWAVE; ++i) {
    if (!waves[i].on) continue;
    float dx = cx - waves[i].x, dy = cy - waves[i].y;
    float d = fabsf(sqrtf(dx * dx + dy * dy) - waveR[i]);
    if (d < waveW[i] * 0.5f) return 2;
    if (d < waveW[i]) best = 1;
  }
  return best;
}

static void renderBand(int k, uint16_t* buf, Scratch& sc) {
  const int y0 = k * BAND, y1 = min(S, y0 + BAND);
  const int bx0 = bandX0[k], bw = bandW[k], bx1 = bx0 + bw - 1;
  const uint8_t* const ft = fireTex;
  uint8_t* const fr = sc.fire;
  for (int y = y0; y < y1; ++y) {
    uint16_t* row = buf + (y - y0) * bw - bx0;               // row[x] で画面の x
    const int xs = max((int)spanS[y], bx0), xe = min((int)spanE[y], bx1);
    for (int x = bx0; x < xs; ++x) row[x] = blackCol;
    for (int x = max(xe + 1, bx0); x <= bx1; ++x) row[x] = blackCol;
    if (xs > xe) continue;
    const int ys = y;
    const uint8_t* fade = fadeTab[rowFade[y]];

    // ---- 炎の番号(この行ぶん)。うねりの格子を縦に補間して、横は足し算で進める
    {
      const int gy = ys / CSTEP; const float fy = (ys - gy * CSTEP) * (1.0f / CSTEP);
      const float *au = gU + gy * CW, *av = gV + gy * CW;
      const int gx0 = xs / CSTEP, gx1 = xe / CSTEP + 1;
      for (int gx = gx0; gx <= gx1; ++gx) {
        sc.rx[gx] = (int32_t)((au[gx] + (au[gx + CW] - au[gx]) * fy) * 256.0f);
        sc.ry[gx] = (int32_t)((av[gx] + (av[gx + CW] - av[gx]) * fy) * 256.0f);
      }
      int x = xs;
      while (x <= xe) {
        const int gx = x / CSTEP;
        const int segEnd = min(xe, gx * CSTEP + CSTEP - 1);
        const int32_t stepu = (sc.rx[gx + 1] - sc.rx[gx] + (1 << (USHIFT - 1))) >> USHIFT;
        const int32_t stepv = (sc.ry[gx + 1] - sc.ry[gx] + (1 << (USHIFT - 1))) >> USHIFT;
        const int off = x - gx * CSTEP;
        int32_t u = sc.rx[gx] + stepu * off, v = sc.ry[gx] + stepv * off;
        for (; x <= segEnd; ++x, u += stepu, v += stepv) {
          const uint8_t f = ft[((((uint32_t)v >> 8) & FTM) << FTSH) | (((uint32_t)u >> 8) & FTM)];
          fr[x] = (uint8_t)((f & 0xC0) | fade[f & 63]);
        }
      }
    }

    // ---- 文字の柱。1ピクセルあたり「文字の濃さ」と「炎の番号」で表を1回引くだけ
    int c0 = (xs - colX0) / CELL, c1 = (xe - colX0) / CELL;
    c0 = max(0, c0); c1 = min(NCOL - 1, c1);
    for (int c = c0; c <= c1; ++c) {
      const int cx0 = colX0 + c * CELL;
      const int xa = max(xs, cx0), xb = min(xe, cx0 + CELL - 1);
      if (xa > xb) continue;
      const Column& C = col[c];
      const int gyy = ys + C.scrollI;                         // いつも 0 以上
      const int r = gyy / CELL, iy = gyy - r * CELL;
      const int ti = (C.start + r) % TL;
      const int gid = ((nulMask[c][ti >> 5] >> (ti & 31)) & 1) ? NULG : text[ti];
      int bank = C.level;
      if (anyWave) {
        int fl = flashOf(cx0 + CELL * 0.5f, (float)(r * CELL - C.scrollI) + CELL * 0.5f);
        bank = fl == 2 ? BK_FLASH : min(2, bank + fl);
      }
      const uint16_t* lp = lut + (bank << 12);
      const uint8_t* gp = atlas + (size_t)gid * CELL * CELL + iy * CELL - cx0;
      for (int x = xa; x <= xb; ++x) row[x] = lp[((uint32_t)gp[x] << 8) | fr[x]];
    }

    // ---- 鏡の立方体(面ごとに、炎を回して裏返したものを映す)
    for (int f = 0; f < nFace; ++f) {
      const Face& Fc = face[f];
      if (y < Fc.y0 || y > Fc.y1) continue;
      const int xa = max(xs, (int)faceL[f][y]), xb = min(xe, (int)faceR[f][y]);
      if (xa > xb) continue;
      int32_t u = Fc.U0 + Fc.Ux * xa + Fc.Uy * y, v = Fc.V0 + Fc.Vx * xa + Fc.Vy * y;
      const uint16_t* cl = cubeLut[f];
      for (int x = xa; x <= xb; ++x, u += Fc.Ux, v += Fc.Vx)
        row[x] = cl[ft[((((uint32_t)v >> 8) & FTM) << FTSH) | (((uint32_t)u >> 8) & FTM)]];
    }
  }

  // ---- 立方体のふち(細い光の線)
  for (int e = 0; e < nEdge; ++e) {
    const Edge& E = edges[e];
    float ya = fminf(E.y0, E.y1), yb = fmaxf(E.y0, E.y1);
    int ra = max(y0, (int)floorf(ya)), rb = min(y1 - 1, (int)ceilf(yb));
    if (ra > rb) continue;
    const float dy = E.y1 - E.y0;
    for (int y = ra; y <= rb; ++y) {
      float xa, xb;
      if (fabsf(dy) < 0.5f) { xa = fminf(E.x0, E.x1); xb = fmaxf(E.x0, E.x1); }
      else {
        float t0 = clamp01((y - E.y0) / dy), t1 = clamp01((y + 1 - E.y0) / dy);
        xa = E.x0 + (E.x1 - E.x0) * t0; xb = E.x0 + (E.x1 - E.x0) * t1;
        if (xa > xb) { float t = xa; xa = xb; xb = t; }
      }
      int ia = max(max(bx0, (int)spanS[y]), (int)floorf(xa)), ib = min(min(bx1, (int)spanE[y]), (int)ceilf(xb));
      uint16_t* row = buf + (y - y0) * bw - bx0;
      for (int x = ia; x <= ib; ++x) row[x] = edgeCol;
    }
  }

  // ---- 蝶
  for (int i = 0; i < NFLY; ++i) {
    const Fly& f = fly[i];
    const int ra = max(y0, f.sy0), rb = min(y1 - 1, f.sy0 + SP - 1);
    if (ra > rb) continue;
    const uint16_t* C = sprC + (size_t)i * SP * SP;
    const uint8_t*  A = sprA + (size_t)i * SP * SP;
    for (int y = ra; y <= rb; ++y) {
      const int xa = max(max(bx0, (int)spanS[y]), f.sx0), xb = min(min(bx1, (int)spanE[y]), f.sx0 + SP - 1);
      uint16_t* row = buf + (y - y0) * bw - bx0;
      const int k = (y - f.sy0) * SP - f.sx0;
      for (int x = xa; x <= xb; ++x) if (A[k + x]) row[x] = C[k + x];
    }
  }
}

static void __attribute__((noinline)) MOD_IRAM renderBandIram(int k, uint16_t* b, Scratch& s) { renderBand(k, b, s); }

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
        renderBandIram(k, slotBuf[k % NSLOT], scr[0]);
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
        renderBandIram(k, slotBuf[k % NSLOT], scr[1]);
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

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);
  Serial.begin(115200);

  if (M5.Display.width() < M5.Display.height()) M5.Display.setRotation(1);
  W = M5.Display.width(); H = M5.Display.height();
  S = ((W == H) ? W : max(W, H)) & ~1;
  Sc = (S - 1) * 0.5f;
  XOFF = (W - S) / 2;
  YOFF = ((H - S) / 2 / BAND) * BAND;
  CW = S / CSTEP + 3;
  NB = (S + BAND - 1) / BAND;
  CELL = max(12, (int)(S / COLS));
  NCOL = S / CELL + 2;
  if (NCOL > MAXCOL) NCOL = MAXCOL;
  colX0 = (S - NCOL * CELL) / 2;
  SP = 2 * (int)ceilf(FLY_SIZE * S * 1.15f) + 4;
  Serial.println("NullSutra v8");
  Serial.printf("screen %dx%d  area %d  cell %d  cols %d\n", W, H, S, CELL, NCOL);

  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setBrightness(brightSteps[brightIdx]);

  noiseT = (float*)ps_malloc(NOISE * NOISE * sizeof(float));
  if (!noiseT) noiseT = (float*)malloc(NOISE * NOISE * sizeof(float));
  for (int i = 0; i < NSLOT; ++i) slotBuf[i] = (uint16_t*)heap_caps_malloc(S * BAND * 2, MALLOC_CAP_DMA);
  // 1ピクセルごとに引く炎の絵と色の表は、内蔵メモリに
  fireTex = (uint8_t*)heap_caps_malloc(FT * FT, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  if (!fireTex) fireTex = (uint8_t*)ps_malloc(FT * FT);
  lut = (uint16_t*)heap_caps_malloc(NBANK * 4096 * 2, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  if (!lut) lut = (uint16_t*)ps_malloc(NBANK * 4096 * 2);
  spanS  = (int16_t*)malloc(S * sizeof(int16_t)); spanE = (int16_t*)malloc(S * sizeof(int16_t));
  bandX0 = (int16_t*)malloc(NB * sizeof(int16_t)); bandW = (int16_t*)malloc(NB * sizeof(int16_t));
  bandVis = (uint8_t*)malloc(NB);
  rowFade = (uint8_t*)malloc(S);
  const int CN = CW * CW;
  gU = (float*)calloc(CN, sizeof(float)); gV = (float*)calloc(CN, sizeof(float));
  dX = (float*)ps_calloc(CN, sizeof(float)); dY = (float*)ps_calloc(CN, sizeof(float));
  for (int k = 0; k < 3; ++k) { faceL[k] = (int16_t*)malloc(S * 2); faceR[k] = (int16_t*)malloc(S * 2); }
  sprC = (uint16_t*)ps_malloc((size_t)NFLY * SP * SP * 2);
  sprA = (uint8_t*)ps_malloc((size_t)NFLY * SP * SP);
  for (int i = 0; i < 4; ++i) {
    nmX[i].i0 = (uint16_t*)malloc(CW * 2); nmX[i].i1 = (uint16_t*)malloc(CW * 2); nmX[i].f = (float*)malloc(CW * 4);
    nmY[i].i0 = (uint16_t*)malloc(CW * 2); nmY[i].i1 = (uint16_t*)malloc(CW * 2); nmY[i].f = (float*)malloc(CW * 4);
  }
  for (int c = 0; c < 2; ++c) {
    scr[c].rx = (int32_t*)malloc(CW * sizeof(int32_t));
    scr[c].ry = (int32_t*)malloc(CW * sizeof(int32_t));
    scr[c].fire = (uint8_t*)malloc(S);
  }
  bool ok = noiseT && fireTex && lut && spanS && spanE && bandX0 && bandW && bandVis && rowFade &&
            gU && gV && dX && dY && sprC && sprA;
  for (int i = 0; i < NSLOT; ++i) ok = ok && slotBuf[i];
  for (int k = 0; k < 3; ++k) ok = ok && faceL[k] && faceR[k];
  for (int i = 0; i < 4; ++i) ok = ok && nmX[i].i0 && nmX[i].i1 && nmX[i].f && nmY[i].i0 && nmY[i].i1 && nmY[i].f;
  for (int c = 0; c < 2; ++c) ok = ok && scr[c].rx && scr[c].ry && scr[c].fire;
  if (!ok) fail("Memory error");

  bool round = (W == H);
  float rr = S * 0.5f + 1.0f;
  for (int y = 0; y < S; ++y) {
    if (!round) { spanS[y] = 0; spanE[y] = S - 1; continue; }
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

  // 暗さの表: 上ほど炎が明るく、壁の下のほうは少し沈む
  for (int l = 0; l < NFADE; ++l) {
    float g = 1.0f - l / (float)(NFADE - 1) * 0.85f;
    for (int i = 0; i < 64; ++i) fadeTab[l][i] = (uint8_t)(i * g + 0.5f);
  }
  for (int y = 0; y < S; ++y) {
    float t = 0.72f * smooth3(((float)y / S - 0.24f) / 0.56f);
    rowFade[y] = (uint8_t)(t * (NFADE - 1) + 0.5f);
  }

  makeNoise(noiseT, 3);
  buildFire();
  if (!buildGlyphs()) fail("Glyph error");
  buildLut();

  for (int c = 0; c < NCOL; ++c) {
    Column& C = col[c];
    C.speed = frand(8.0f, 26.0f) * (S / 466.0f) * ((esp_random() & 3) == 0 ? -1.0f : 1.0f);
    C.scroll = frand(0, (float)TL * CELL); C.scrollI = (int)C.scroll;
    C.start = (int)(frand() * TL) % TL;
    C.base = (int8_t)((esp_random() % 10) < 3 ? 0 : ((esp_random() % 10) < 6 ? 1 : 2));
    C.level = C.base; C.ph = frand(0, 6.2832f); C.w = frand(0.08f, 0.35f);
    makeNulMask(c);
  }
  for (int i = 0; i < NFLY; ++i) spawnFly(fly[i], i);
  for (int i = 0; i < NWAVE; ++i) waves[i].on = false;

  Input in0 = {false, false, -1000, -1000, 0.016f};
  updateFrame(in0);

  semGo   = xSemaphoreCreateBinary();
  semDone = xSemaphoreCreateBinary();
  xTaskCreatePinnedToCore(workerTask, "band", 8192, nullptr, 1, nullptr, 0);
}

void loop() {
  static uint32_t last = millis();
  static uint32_t fpsTimer = millis();
  static uint32_t lastTapMs = 0;
  static int frames = 0;
  bool round = (W == H);

  M5.update();
  auto t = M5.Touch.getDetail();
  uint32_t now = millis();
  bool pressed = t.wasPressed();

  if (round) {
    if (M5.BtnA.wasHold()) {
      speedIdx = (speedIdx + 1) % 3;
      speedMul = speedIdx == 0 ? 0.5f : (speedIdx == 1 ? 1.0f : 2.0f);
    } else if (M5.BtnA.wasClicked()) { theme = (theme + 1) % NPAL; buildLut(); }
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
      if (now - lastTapMs < 350) { theme = (theme + 1) % NPAL; buildLut(); lastTapMs = 0; }
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
static_assert(std::is_trivially_default_constructible<Column>::value, "NM_ARENA: Column");
static_assert(std::is_trivially_default_constructible<Edge>::value, "NM_ARENA: Edge");
static_assert(std::is_trivially_default_constructible<Face>::value, "NM_ARENA: Face");
static_assert(std::is_trivially_default_constructible<Fly>::value, "NM_ARENA: Fly");
static_assert(std::is_trivially_default_constructible<NMap>::value, "NM_ARENA: NMap");
static_assert(std::is_trivially_default_constructible<Scratch>::value, "NM_ARENA: Scratch");
static_assert(std::is_trivially_default_constructible<Wave>::value, "NM_ARENA: Wave");
static_assert(std::is_trivially_default_constructible<bool>::value, "NM_ARENA: bool");
static_assert(std::is_trivially_default_constructible<float>::value, "NM_ARENA: float");
static_assert(std::is_trivially_default_constructible<int>::value, "NM_ARENA: int");
static_assert(std::is_trivially_default_constructible<uint16_t>::value, "NM_ARENA: uint16_t");
static_assert(std::is_trivially_default_constructible<uint32_t>::value, "NM_ARENA: uint32_t");
static_assert(std::is_trivially_default_constructible<uint8_t>::value, "NM_ARENA: uint8_t");
static constexpr size_t OVL_END = nmAlignTo(nmOv47, 16);
static_assert(OVL_END <= NM_ARENA_SIZE, "NM_ARENA_SIZE (nm_common.h) を大きくしてください");
extern const size_t OVL_SIZE = OVL_END;   // 起動時の確認用
}  // namespace NullSutra
