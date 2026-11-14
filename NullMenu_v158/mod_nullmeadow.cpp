// NullMeadow  -  NullMenu 用モジュール(元のスケッチ NullMeadow_v27 を名前空間に入れたもの)
// 0 で始まる変数(配列も)は、ほかの作品と共通の置き場(NM_ARENA)に置いています。
// 一度に動くのは1つだけ(切り替えるときは再起動)なので、重なっていても問題ありません。
#include "nm_common.h"
#include "meadow_paint.h"
#include <FFat.h>
#if defined(ESP_PLATFORM) && __has_include(<lgfx/v1/panel/Panel_AMOLED.hpp>)
#include <lgfx/v1/panel/Panel_AMOLED.hpp>
#define NM_DIRECT_AMOLED 1
struct NmFbPeek : public lgfx::Panel_AMOLED_Framebuffer {
  static lgfx::Panel_AMOLED* inner(lgfx::Panel_Device* p) {
    return static_cast<lgfx::Panel_AMOLED_Framebuffer*>(p)->*(&NmFbPeek::_panel);
  }
};
#endif
static constexpr bool DIRECT_SEND = true;        // StopWatch では、画面へ「直接」送る。画面が乱れるときは false に
static bool workerUp = false;                    // コア0の描く係が作れたか(作れなければ、コア1だけで描く)
static inline float mdSin(float x) {             // sin(x) の速い近似
  x *= 0.15915494f; x -= floorf(x + 0.5f); x *= 6.2831853f;
  float y = x * (1.27323954f - 0.40528473f * fabsf(x));
  return y * (0.775f + 0.225f * fabsf(y));
}
static inline float mdExp(float x) {             // exp(x) の速い近似(x <= 0)
  if (x < -30.0f) return 0.0f;
  union { float f; int32_t i; } u; u.i = (int32_t)(12102203.0f * x + 1064866805.0f); return u.f;
}
static int fxGround0 = 0;
static float viewX = 0, viewY = 0;               // 見る向き(-1..1。傾きで変わる)
static constexpr float VIEW_SKY = 6.0f, VIEW_BOX = 18.0f, VIEW_NEAR = 110.0f, VIEW_FAR = 14.0f, VIEW_VERT = 0.5f, VIEW_GAIN = 2.4f;   // 奥行きごとにずらす量(点。画面 466 のとき)/ 縦は横の何倍                         // 地面のいちばん上(絵を動かすところで決める)

#pragma GCC optimize("O3")

namespace NullMeadow {
static constexpr size_t nmOv0 = 0;

// NullMeadow v27  -  null² のあとの花畑。鏡の箱が残る花畑に、モルフォ蝶が舞う(蝶は NullButterfly v6 の応用)
//   M5Stack StopWatch 用(丸い 466x466 画面)。CoreS3 系でも画面いっぱいに動きます。
//   null² が終わったあと、鏡の箱だけが花畑に残っている。そんな場面を5つ描いた絵の上を、モルフォ蝶が舞う。
//
// 画面の中:
//   ・場面は6つ。ボタンA(か、決めた時間ごと)に、ふわっと入れかわる
//     1. 夕暮れの海辺と鏡の箱  2. 朝もやの鏡の回廊  3. むらさきの夕凪  4. 夕日の花野  5. 波打つ鏡と青い時間  6. 光の川とガラスの柱
//     (絵は持たずに、場面を切り替えるたびに本体の上で計算して描く。meadow_paint.h。描くあいだ1〜2秒止まる)
//   ・モルフォ蝶(最初は5匹)が、数回ゆっくり大きくはばたいて斜めに昇り、羽を開いたまま滑って、ゆるく下りる。
//     蝶の絵は NullButterfly と同じく、起動時に計算で描く(絵を持たないので軽い)。羽を開くと青く照り、閉じると裏の茶色
//   ・null² の鏡の建物は NullPavilion の「鏡の膜」と同じしくみ: 面の中のふくらみがゆっくり動いて、映った空がぐにゃっとゆがみ、
//     ふくらみの向きで明るさと照りが変わる。銀色は NullPavilion と同じく、つなぎ目の線と、かたむくほど空を映す銀の照り。
//     鏡の面を押すと膜がぬるっとへこみ、離すとばねで「ぷるん」と揺れもどる。タップすると、そこから波が広がる
//   ・本体を傾けると、見る向きが立体的に変わる(のぞき込むように)。景色を「空と海 / 鏡の箱 / 地面と花」の奥行きに分けて、
//     手前ほど大きくずらす(花畑は手前の行ほど大きく)。鏡の箱は奥行きの分だけ動き、映りも角度でずれる。蝶も奥行きに合わせて動く。
//     鏡の建物と花畑は別の絵(建物のある絵 / 建物のない景色)。建物は奥にあるので動きは小さく、手前の花畑ほど大きく動く。
//     建物のすそは手前の花畑にかくれる。花は風でそよぐ
//   ・鏡の建物には、空だけでなく花畑と蝶も映る(左右が逆で、少し暗く銀がかって。見る向きで映りもずれる)
//   ・絵も動く: 海に夕日がきらめき(NullSunset の波の光)、
//     手前の花が風にそよぎ、光の粒が花畑から立ちのぼる(NullLotus の光の粒)
//   ・奥の蝶ほど小さく、その場面のもやの色に溶ける。蝶の通ったあとに、光る鱗粉がこぼれて消えていく
//
// 触ると:
//   ・タップ        : そこから蝶がいっせいに飛び散る(鱗粉もはじける)
//   ・なぞる        : 指の近くの蝶が、指の動きに巻き込まれる
//   ・押さえたまま  : 蝶が指のまわりに集まってきて、渦になって回る。離すと、ぱっと散る
//
// 操作:
//   StopWatch … ボタンA 短押し: 次の場面   ボタンA 長押し: 蝶の色(モルフォの青 / 白銀 / 金 / 紅)
//               ボタンB: 蝶の数(1 / 3 / 5 / 8 / 12)   明るさは、メニューで決めます(メニューで B を長押し)
//   CoreS3系  … 画面の上半分をダブルタップ: 次の場面   下半分をダブルタップ: 蝶の数
//               電源ボタン短押し: 蝶の数
//
// 必要なもの:
//   ボードマネージャー M5Stack >= 3.3.7 / ボード: M5StopWatch(CoreS3 なら M5CoreS3)
//   ライブラリ M5Unified >= 0.2.15, M5GFX >= 0.2.21
//   (文字は使わないので、フォントは要りません。5つの場面を描くプログラムは同じフォルダの meadow_paint.h に入っています)
//
// しくみ:
//   蝶の絵は、起動時に「羽ばたきの8コマ × 大きさ3段」を描いておき(色は番号で持つ)、
//   場面の絵は計算で描いて PSRAM に置き、帯ごとに写します。切り替えるときは次の絵を描いてから、2枚を混ぜてふわっと入れかえます。
//   毎フレームそれを回して・縮めて貼るだけにしています。だから数を増やしても軽い。
//   色は「奥行き × 色違い × コマ」ごとの小さな色の表で決まるので、色の切り替えは表を作り直すだけ。
//   風は「カールノイズ」(うずの関数を微分した、湧き出しも吸い込みもない流れ)を式で計算します。
//   画面は8行ずつの帯に分けて、2つのコアのうち空いた方が次の帯を描きます(StopWatch は画面へ直接送る)。



// Arduino IDE は関数の宣言を自動でこのあたりに足すので、型の名前だけ先に宣言しておく
struct Fly; struct Dust; struct DustDraw; struct Col; struct Theme; struct Input;

// ---- 調整用 ----------------------------------------------------------------
static constexpr int   NCOUNT      = 5;
static const int       COUNTS[NCOUNT] = {1, 3, 5, 8, 12};     // 蝶の数の段階(ボタンA 短押しで切り替え)
static constexpr int   COUNT_START = 2;       // 最初の数(0: 1匹 / 1: 3匹 / 2: 5匹 / 3: 8匹 / 4: 12匹)
static constexpr float SIZE_FAR    = 0.030f;  // 奥の蝶の大きさ(画面に対する割合)
static constexpr float SIZE_MID    = 0.055f;  // 中の蝶
static constexpr float SIZE_NEAR   = 0.090f;  // 手前の蝶
static constexpr float RMIR        = 0.028f;  // 鏡の箱の遠さ(そこにいる蝶の大きさで。鏡に映る蝶の大きさを決める)
static constexpr int   SCENE_SEC   = 60;      // 場面が自動で変わる間隔(秒。0 = ボタンだけ)
static constexpr int   FADE_MS     = 1600;    // 場面がふわっと入れかわる時間
static constexpr int   SCENE_START = 0;       // 最初の場面(0..4)
static constexpr float FLY_SPEED   = 0.13f;   // 飛ぶ速さ(画面/秒)
static constexpr float CLIMB       = 0.30f;   // はばたいて昇る傾き(tan)
static constexpr float SINK        = 0.16f;   // 滑って下りる傾き(tan)
static constexpr float FLAPS_MIN   = 2.0f;    // 1回にはばたく数(この間で決める)
static constexpr float FLAPS_MAX   = 5.0f;
static constexpr float GLIDE_MIN   = 0.7f;    // 滑る時間(秒。この間で決める)
static constexpr float GLIDE_MAX   = 1.9f;
static constexpr float MEANDER     = 0.95f;   // 向きの変わりやすさ
static constexpr float MEANDER_HZ  = 0.25f;
static constexpr float BOB         = 0.10f;   // 羽ばたきごとの浮き沈み(蝶の大きさに対する割合)
static constexpr float GLIDE       = 0.30f;   // 0 で滑空しない
static constexpr float FLAP_HZ     = 2.6f;    // 羽ばたきの回数(1秒あたり、だいたい。モルフォは大きくゆっくり)
static constexpr float DUST_RATE   = 3.0f;    // 鱗粉のこぼれる量
static constexpr float SWARM_EVERY = 0.0f;    // 群れが集まって輪になる間隔(秒、だいたい。0 = なし)
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
  float flapT; bool glide;  float homeY;    // 羽ばたく残り時間 / 滑空中か
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

// ---------------------------------------------------------------- 道具
struct Col { float r, g, b; };
static inline __attribute__((always_inline)) float ffloorf(float x) { const int i = (int)x; return (float)(i - (x < (float)i ? 1 : 0)); }   // floorf の速い版
static inline __attribute__((always_inline)) float fminF(float a, float b) { return a < b ? a : b; }
static inline __attribute__((always_inline)) float fmaxF(float a, float b) { return a > b ? a : b; }
static inline __attribute__((always_inline)) float clamp01(float v) { return v < 0 ? 0 : (v > 1 ? 1 : v); }
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
  {"morpho (blue)",
   {0.02f, 0.04f, 0.16f}, {0.015f, 0.10f, 0.48f}, {0.10f, 0.52f, 1.00f}, {0.75f, 0.95f, 1.00f},
   {0.02f, 0.14f, 0.55f}, {0.18f, 0.60f, 1.00f},
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
static const float DEPTH_K[3] = {0.85f, 0.95f, 1.00f};   // 奥ほど少し暗く
static const float DEPTH_FOG[3] = {0.50f, 0.20f, 0.0f};  // 奥ほど夕もやに溶ける
static Col HAZE = {0.95f, 0.86f, 0.76f};                  // いまの場面のもやの色
static const Col SCENE_HAZE[NSCENE] = {{0.95f, 0.80f, 0.80f}, {0.86f, 0.87f, 0.90f}, {0.72f, 0.58f, 0.80f}, {0.92f, 0.86f, 0.78f}, {0.58f, 0.58f, 0.74f}, {0.98f, 0.88f, 0.74f}};
static constexpr Col UNDER = {0.36f, 0.25f, 0.15f};      // 羽の裏(茶色)
static const float SPEED_K[3] = {0.80f, 1.00f, 1.15f};   // 手前ほど少し速く見える

// 色の表: 奥行き3 × 色違い2 × 羽ばたき8コマ。羽がこちらを向く(開く)ほど明るく照る
__attribute__((cold)) static void buildPalettes() {
  const Theme& th = THEMES[theme];
  for (int cls = 0; cls < 3; ++cls)
    for (int var = 0; var < 2; ++var)
      for (int fr = 0; fr < NFR; ++fr) {
        uint16_t* P = palBuf + ((cls * 2 + var) * NFR + fr) * NPIDX;
        const float flap = 0.14f + 0.86f * fr / (NFR - 1);
        const float glint = 0.45f + 0.55f * flap;
        const float uw = smooth3((0.42f - flap) / 0.3f);          // 閉じかけは裏の茶色が見える
        const float k = DEPTH_K[cls], fog = DEPTH_FOG[cls];
        const Col dp = var ? th.deep2 : th.deep, br = var ? th.bright2 : th.bright;
        auto fin = [&](Col c) { return toPix(mulC(lerpC(c, HAZE, fog), k)); };
        P[0] = 0;
        for (int a = 0; a < 16; ++a)
          for (int b = 0; b < 8; ++b) {
            const float t = a / 15.0f, fromBody = b / 7.0f;
            Col c = lerpC(dp, br, smooth3(1.0f - t));
            c = lerpC(c, th.pale, 0.22f * (1.0f - t) * (1.0f - t) * (1.0f - t) * flap);
            c = lerpC(th.root, c, smooth3(fromBody));
            P[1 + a * 8 + b] = fin(lerpC(mulC(c, glint), mulC(UNDER, 0.7f + 0.5f * (1.0f - t)), uw));
          }
        P[129] = fin({0.04f, 0.03f, 0.05f});
        P[130] = fin(lerpC(th.edge, mulC(UNDER, 0.6f), uw));
        P[131] = fin(lerpC(mulC(th.dot, 0.7f + 0.3f * flap), UNDER, uw));
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
  const float bodyW = fmaxF(0.04f, 0.55f * iR), antW = fmaxF(0.022f, 0.45f * iR), iflap = 1.0f / flap;
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
          if (e > 0.76f || (e1 < e2 && fa > 0.30f))                                     // 黒いふちと羽の先、白い点が2つ
            o = ((u - 0.50f) * (u - 0.50f) + (vv - 0.74f) * (vv - 0.74f) < 0.0016f || (u - 0.36f) * (u - 0.36f) + (vv - 0.86f) * (vv - 0.86f) < 0.0012f) ? 131 : 130;
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
  f.homeY = Sc + frand(-0.26f, 0.24f) * S;                  // この蝶がよく飛ぶ高さ
  f.mP = frand() < 0.5f ? frand(-0.6f, 0.6f) : 3.14159265f + frand(-0.6f, 0.6f);   // 左か右へ
  f.mA = MEANDER * frand(0.7f, 1.2f);
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
    // モルフォの飛び方: はばたく間は斜めに昇り(約22°)、羽を開いて滑る間はゆるく下りる(約7°)。
    // 画面の上下が高さ。進む向き mP は左右と奥行きの角度で、ゆっくり変わる(奥や手前へ向くと、横には遅く見える)
    f.mP += dt * f.mA * 0.6f * sinf(simT * f.mW * 0.5f + f.seed);
    const float spd0 = FLY_SPEED * S * sk;
    const float cm = cosf(f.mP), cx = (cm >= 0 ? 1.0f : -1.0f) * fmaxF(0.5f, fabsf(cm));   // 真横へも、ちゃんと進む
    if ((f.x - Sc) * cx > 0.12f * S) f.mP += dt * 3.0f * (f.seed > 3.14f ? 1.0f : -1.0f);   // はしに来たら引き返す
    float dvx = spd0 * cx * (f.glide ? 0.95f : 1.05f);
    float dvy = f.glide ? spd0 * SINK : -spd0 * CLIMB;
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
        rx = nx / d; ry = ny / d; t = smooth3((fmaxF(fabsf(nx), fabsf(ny)) - 0.45f) / 0.45f);
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
    // ゆったり追いかける(強く引っぱらない)
    float ax = (dvx - f.vx) * 2.6f + sepX[i];
    float ay = (dvy - f.vy) * 2.6f + sepY[i];
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
      f.x = fminF(fmaxF(f.x, vx0), vx1); f.y = fminF(fmaxF(f.y, vy0), vy1);
    }
    // 羽ばたき。速く飛ぶときだけ、少し速くなる
    // 羽ばたいて → ときどき羽を開いたまま滑空 → また羽ばたく
    if (f.glide) {
      f.flapT -= dt;
      if (f.flapT <= 0 || in.tapped || released || f.y > Sc + 0.36f * S) {   // 低くなりすぎたり、驚いたら、すぐ羽ばたく
        f.glide = false;
        f.flapT = frand(FLAPS_MIN, FLAPS_MAX) / f.hz * (f.y > f.homeY ? 1.7f : 0.6f);
        if (frand() < 0.15f) f.homeY = Sc + frand(-0.26f, 0.24f) * S;
        f.mP += frand(-0.7f, 0.7f);                              // はばたき出しに、ひょいと向きを変える
      }
    } else {
      const float oldPh = f.ph;
      f.ph += dt * 6.2832f * f.hz;
      f.flapT -= dt;
      if (f.y < Sc - 0.30f * S) f.flapT = fminF(f.flapT, 0.0f);   // 高くなりすぎたら、滑りに入る
      // 羽がいちばん開いた所(cos = ±1)を通りすぎた瞬間に止めて、滑空に入る
      if (f.flapT <= 0 && GLIDE > 0 && (int)(f.ph * 0.31831f) != (int)(oldPh * 0.31831f)) {
        f.ph = ffloorf(f.ph * 0.31831f) * 3.14159265f;
        f.glide = true;
        f.flapT = frand(GLIDE_MIN, GLIDE_MAX) * (f.y < f.homeY ? 1.8f : 0.8f);
        if (in.tapped || released) f.flapT = 0;
      }
    }
    if (f.ph > 1000.0f) f.ph -= 318.0f * 3.14159265f;
    // 向きは進む方へ
    if (sp > 0.02f * S) f.head += wrapPi(atan2f(f.vy, f.vx) - f.head) * fminF(1.0f, dt * 4.0f);   // 向きは進む方へ、なめらかに
    f.head = wrapPi(f.head);
    // 鱗粉(速く飛ぶほど、手前の蝶ほどたくさん)
    const float rate = DUST_RATE * (0.35f + 0.65f * fminF(1.0f, sp / (0.35f * S))) * (f.cls == 0 ? 0.4f : (f.cls == 1 ? 0.7f : 1.0f));
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
    f.cx = f.x - viewX * (16.0f + 34.0f * f.cls) * S / 466.0f; f.cy = f.y - BOB * f.R * sinf(f.ph) - viewY * (16.0f + 34.0f * f.cls) * VIEW_VERT * S / 466.0f;   // 奥行きに合わせて動く
    f.rr = T * 0.5f / k;
    f.y0 = (int)ffloorf(f.cy - f.rr); f.y1 = (int)ceilf(f.cy + f.rr);
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
    const int ix = (int)ffloorf(d.x), iy = (int)ffloorf(d.y);
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
  if (SWARM_EVERY > 0 && autoT <= 0) { autoOn = !autoOn; autoT = autoOn ? frand(5.0f, 8.0f) : SWARM_EVERY * frand(0.7f, 1.3f); }
  autoW = clamp01(autoW + (autoOn ? dt : -dt) / 1.5f);

  if (in.tapped) burstDust(in.tx, in.ty, 40, 0.35f * S, 2);
  if (released) burstDust(fPX, fPY, 30, 0.30f * S, 2);

  updateFlies(in, dt, fvx, fvy, released);
  if (frand() < 2.5f * dt) {                      // 光の粒が、花畑から立ちのぼる
    const float x = frand(0.15f, 0.85f) * S, y = fxGround0 + frand(0.02f, 0.30f) * S;
    emitDust(x, y, frand(-0.01f, 0.01f) * S, -frand(0.04f, 0.08f) * S, 1, frand(2.5f, 4.5f));
  }
  updateDust(dt);
}

// 場面の絵を読む(画面の大きさに合わせて縮める。バイトを入れ替えた 565 で PSRAM に置く)。2枚ぶんの置き場を交互に使う
static M5Canvas* bgCv[2] = {nullptr, nullptr};
static uint16_t* bgImg = nullptr;                // いまの場面
static uint16_t* bgNext = nullptr;               // 入れかわり中の、次の場面
static volatile int fadeK = 0;                   // 0 = 入れかわっていない / 1..31 = 次の場面の濃さ
static int sceneNo = SCENE_START, bgSlot = 0;
static uint32_t fadeT0 = 0, sceneT0 = 0;

// ---- 絵を動かす: 鏡のゆらぎ / 海のきらめき / 花のそよぎ(場面ごとの情報は meadow_fx.h)
static int16_t* fxEdge = nullptr;                // 列ごとの、地面の上のふち(画面の y)
static int16_t* fxFace = nullptr;                // [行][鏡の面 8 つ × 左右] なければ -1
static int  fxHz = 0, fxSunX = 0, fxRip = 0;   // 地面のいちばん上 / 水平線 / 夕日の x / ゆらぎの強さ(1/16 点)
static bool fxSea = false;
static int8_t SINT[256];
static int16_t* fxRivA = nullptr;                // 行ごとの川の左右(なければ A > B)
static int16_t* fxRivB = nullptr;
__attribute__((cold)) static void buildFx(int k) {
  if (!fxEdge) { fxEdge = (int16_t*)ps_malloc(S * 2); fxFace = (int16_t*)ps_malloc((size_t)S * 16 * 2); }
  if (!fxEdge || !fxFace) return;
  for (int i = 0; i < 256; ++i) SINT[i] = (int8_t)(127.0f * sinf(i * 6.2832f / 256.0f));
  const MeadowFx& F = MEADOW_FX[k];
  if (!fxRivA) { fxRivA = (int16_t*)ps_malloc(S * 2); fxRivB = (int16_t*)ps_malloc(S * 2); }
  for (int y = 0; fxRivA && y < S; ++y) { const int i = min(465, y * 466 / S); fxRivA[y] = (int16_t)((int32_t)F.rivA[i] * S / 4096); fxRivB[y] = (int16_t)((int32_t)F.rivB[i] * S / 4096); }
  fxGround0 = S;
  for (int x = 0; x < S; ++x) { fxEdge[x] = (int16_t)((int32_t)F.edge[min(465, x * 466 / S)] * S / 4096); fxGround0 = min(fxGround0, (int)fxEdge[x]); }
  fxHz = (int)(F.hz * S); fxSunX = (int)(F.sunX * S); fxSea = F.sea != 0;
  fxRip = (int)(fmaxF(1.3f, F.rip * 1.3f) * S / 466.0f * 16.0f);
  for (int i = 0; i < S * 16; ++i) fxFace[i] = -1;
  for (int b = 0; b < F.nBox; ++b) {
    const float* B = F.box[b];                   // 左のはし, 角, 右のはし, 上のふち(左, 角, 右), 下
    for (int f = 0; f < 2; ++f) {
      const float u0 = f ? B[1] : B[0], u1 = f ? B[2] : B[1], t0 = f ? B[4] : B[3], t1 = f ? B[5] : B[4];
      if (u1 <= u0) continue;
      const int xa = max(0, (int)(u0 * S) + 2), xb = min(S - 1, (int)(u1 * S) - 2), slot = b * 2 + f;
      for (int y = 0; y < S; ++y) {
        if (y > B[6] * S - 2) break;
        int a = -1, e = -1;
        for (int x = xa; x <= xb; ++x) {
          const float top = (t0 + (t1 - t0) * ((x + 0.5f) / S - u0) / (u1 - u0)) * S + 2;
          if (y >= top) { if (a < 0) a = x; e = x; }
        }
        if (a >= 0) { fxFace[y * 16 + slot * 2] = (int16_t)a; fxFace[y * 16 + slot * 2 + 1] = (int16_t)e; }
      }
    }
  }
}

// ---- 鏡の建物の面(NullPavilion の「鏡の膜」のしくみ): 面ごとに高さの格子を持ち、ふくらみを動かして、かたむきで映りをずらす
static constexpr int MGW = 18, MGH = 26, MFN = 8;   // 格子の大きさ / 面の数
struct MFace { bool on; int x0, x1; float t0, t1, bot, seed; float rx, ry, rage; float du, dv, dA, dV; bool held; };
static MFace mf[MFN];
static float* mOff = nullptr;                    // [面][MGH][MGW][2] 映りのずれ(点)。光の当たり具合は ずれ から決める
static float mirK = 10.0f;
static uint16_t silverCol = 0, seamCol = 0;       // 銀の照り / つなぎ目の線(バイトを入れ替えた 565。場面ごと)                       // ずれの大きさ(点)
__attribute__((cold)) static void setupMirror(int k) {
  if (!mOff) mOff = (float*)ps_malloc(sizeof(float) * MFN * MGH * MGW * 2);
  const MeadowFx& F = MEADOW_FX[k];
  for (int i = 0; i < MFN; ++i) mf[i].on = false;
  for (int b = 0; b < F.nBox && b < 4; ++b) {
    const float* B = F.box[b];
    for (int f = 0; f < 2; ++f) {
      MFace& m = mf[b * 2 + f];
      const float u0 = f ? B[1] : B[0], u1 = f ? B[2] : B[1];
      if (u1 <= u0) continue;
      m.on = true; m.x0 = (int)(u0 * S); m.x1 = (int)(u1 * S);
      m.t0 = (f ? B[4] : B[3]) * S; m.t1 = (f ? B[5] : B[4]) * S; m.bot = B[6] * S;
      m.seed = b * 2.3f + f * 1.1f; m.rage = 99; m.dA = m.dV = 0; m.held = false; m.du = m.dv = 0.5f;
    }
  }
  mirK = (6.0f + F.rip * 3.0f) * S / 466.0f;     // 波打つ鏡の場面ほど大きく
  {                                               // 銀: その場面のもやの色を明るくした色 / つなぎ目: 少し暗い銀
    const Col h = SCENE_HAZE[k];
    auto pk = [](float r, float g, float b) { const int R = (int)(fminF(1.0f, r) * 31), G = (int)(fminF(1.0f, g) * 63), B = (int)(fminF(1.0f, b) * 31); return __builtin_bswap16((uint16_t)((R << 11) | (G << 5) | B)); };
    silverCol = pk(0.55f + 0.45f * h.r, 0.57f + 0.45f * h.g, 0.62f + 0.45f * h.b);
    seamCol = pk(0.42f + 0.25f * h.r, 0.44f + 0.25f * h.g, 0.48f + 0.25f * h.b);
  }
}
static void mirrorTap(float x, float y) {        // 鏡の面をタップ: そこから波が広がる
  for (int i = 0; i < MFN; ++i) {
    MFace& m = mf[i];
    if (!m.on || x < m.x0 || x > m.x1) continue;
    const float u = (x - m.x0) / (float)(m.x1 - m.x0), top = m.t0 + (m.t1 - m.t0) * u;
    if (y < top || y > m.bot) continue;
    m.rx = u; m.ry = (y - top) / (m.bot - top); m.rage = 0;
  }
}
// 押している指: 鏡の面の上なら、そこがへこむ(NullPavilion の膜と同じ。離すと、ばねで揺れもどる)
static void mirrorPress(bool touching, float x, float y) {
  for (int i = 0; i < MFN; ++i) {
    MFace& m = mf[i]; m.held = false;
    if (!m.on || !touching || x < m.x0 || x > m.x1) continue;
    const float u = (x - m.x0) / (float)(m.x1 - m.x0), top = m.t0 + (m.t1 - m.t0) * u;
    if (y < top || y > m.bot) continue;
    m.held = true;
    m.du += (u - m.du) * 0.35f; m.dv += ((y - top) / (m.bot - top) - m.dv) * 0.35f;
  }
}
static void stepMirror(float dt) {
  if (!mOff) return;
  for (int i = 0; i < MFN; ++i) {                // へこみ: 押しているあいだは深く、離すと ばね で ぷるん
    MFace& m = mf[i];
    if (!m.on) continue;
    const float target = m.held ? -0.9f : 0.0f, k = m.held ? 30.0f : 70.0f, c = m.held ? 9.0f : 3.2f;
    m.dV += ((target - m.dA) * k - m.dV * c) * dt; m.dA += m.dV * dt;
  }
  static float h[MGH][MGW];
  for (int i = 0; i < MFN; ++i) {
    MFace& m = mf[i];
    if (!m.on) continue;
    m.rage += dt;
    const float asp = (m.bot - fminF(m.t0, m.t1)) / fmaxF(1.0f, (float)(m.x1 - m.x0));   // 縦横の比(ふくらみを丸く)
    float bx[3], by[3];
    for (int q = 0; q < 3; ++q) {                // ふくらみ3つが、ゆっくり動く
      bx[q] = 0.5f + 0.32f * mdSin(simT * (0.23f + 0.07f * q) + m.seed * (q + 1));
      by[q] = 0.5f + 0.36f * mdSin(simT * (0.17f + 0.05f * q) + m.seed * 1.7f + q * 2.1f + 1.5707963f);
    }
    for (int gy = 0; gy < MGH; ++gy) {
      const float v = gy / (float)(MGH - 1);
      for (int gx = 0; gx < MGW; ++gx) {
        const float u = gx / (float)(MGW - 1);
        float hh = 0.10f * mdSin(u * 6.0f + simT * 0.9f + m.seed) * mdSin(v * 5.0f * asp - simT * 0.6f);
        for (int q = 0; q < 3; ++q) { const float du = u - bx[q], dv = (v - by[q]) * asp; hh += (q == 1 ? -0.6f : 0.8f) * mdExp(-(du * du + dv * dv) * 9.0f); }
        if (m.rage < 7.0f) {                     // タップの波(NullPavilion と同じ形: 波の速さ・細かさ・減り方)
          const float du = u - m.rx, dv = (v - m.ry) * asp, d = sqrtf(du * du + dv * dv), front = m.rage * 0.30f;
          if (d < front) hh += 0.45f * mdSin(d * 48.0f - m.rage * 14.0f) * mdExp(-m.rage * 1.2f) * (1.0f - d / (front + 0.01f));
        }
        if (fabsf(m.dA) > 0.002f) {              // 指のへこみ
          const float du = u - m.du, dv = (v - m.dv) * asp;
          hh += m.dA * mdExp(-(du * du + dv * dv) * 14.0f);
        }
        h[gy][gx] = hh;
      }
    }
    float* o = mOff + (size_t)i * MGH * MGW * 2;
    for (int gy = 0; gy < MGH; ++gy)
      for (int gx = 0; gx < MGW; ++gx) {
        const int xa = max(0, gx - 1), xb = min(MGW - 1, gx + 1), ya = max(0, gy - 1), yb = min(MGH - 1, gy + 1);
        o[(gy * MGW + gx) * 2]     = (h[gy][xb] - h[gy][xa]) / (xb - xa) * (MGW - 1) * 0.12f * mirK;
        o[(gy * MGW + gx) * 2 + 1] = (h[yb][gx] - h[ya][gx]) / (yb - ya) * (MGH - 1) * 0.12f * mirK / asp;
      }
  }
}
static constexpr float PANEL_U = 3.0f, PANEL_V = 6.0f;   // 鏡の面のつなぎ目(横に何枚、縦に何枚)
static inline uint16_t mix565(uint16_t p, uint16_t q, uint32_t a);
static inline uint16_t mirShade(uint16_t c, int k256, int spec) {   // 明るさを k256/256 倍して、照りを足す
  const uint32_t v = __builtin_bswap16(c);
  const int r = min(31, (int)((v >> 11) * k256 >> 8) + spec), g = min(63, (int)(((v >> 5) & 63) * k256 >> 8) + spec * 2), b = min(31, (int)((v & 31) * k256 >> 8) + spec);
  return __builtin_bswap16((uint16_t)((r << 11) | (g << 5) | b));
}
static inline uint16_t fxBright(uint16_t c, int add) {   // バイトを入れ替えた 565 を明るく
  const uint32_t v = __builtin_bswap16(c);
  const int r = min(31, (int)(v >> 11) + add), g = min(63, (int)((v >> 5) & 63) + add * 2), b = min(31, (int)(v & 31) + add * 3 / 4);
  return __builtin_bswap16((uint16_t)((r << 11) | (g << 5) | b));
}
// 傾きで花をなびかせる(NullCloud と同じ読み方。はじめの姿勢を「水平」にする)
static constexpr float FLOWER_TILT = 60.0f;      // 傾けたときに花がなびく大きさ(点。画面 466 のとき)
static bool  imuOk = false;
static float flowerP = 0, flowerV = 0, flowerJ = 0;   // なびき(点) / その速さ / ざわざわの強さ(点)
static float levX = 0; static int levN = 0;
static float viewVX = 0, viewVY = 0, levY = 0;
static uint8_t* kindMap = nullptr;
static uint16_t* noBox = nullptr;               // 鏡の箱のない景色(バイトを入れ替えた 565)               // 点ごとの奥行きの種類: 0 空と海 / 1 鏡の箱 / 2 地面と花
static void stepFlowers(float dt) {              // 傾き → 見る向き(ばねで、なめらかに追いかける)
  float sx = 0, sy = 0;
  if (imuOk) {
    float ax, ay, az;
    M5.Imu.update();
    if (M5.Imu.getAccel(&ax, &ay, &az)) {
      sx = -ax; sy = ay;
#if defined(ESP_PLATFORM)
      if (M5.getBoard() == m5::board_t::board_M5StopWatch) { sx = -ay; sy = -ax; }   // StopWatch は 90 度回して読む
#endif
      if (levN < 20) { ++levN; levX += (sx - levX) / levN; levY += (sy - levY) / levN; }
      sx = fmaxF(-1.0f, fminF(1.0f, (sx - levX) * VIEW_GAIN)); sy = fmaxF(-1.0f, fminF(1.0f, (sy - levY) * VIEW_GAIN));
    }
  }
  viewVX += ((sx - viewX) * 30.0f - viewVX * 9.0f) * dt; viewX += viewVX * dt;
  viewVY += ((sy - viewY) * 30.0f - viewVY * 9.0f) * dt; viewY += viewVY * dt;
}
__attribute__((cold)) static void buildKind() {                        // 奥行きの種類: 鏡の箱 = 鏡の面とふちの線(形で決める。地面のふちより上だけ)、地面のふちより下 = 地面と花、ほか = 空と海
  if (!kindMap) kindMap = (uint8_t*)ps_malloc((size_t)S * S);
  if (!kindMap || !fxEdge || !fxFace) return;
  for (int y = 0; y < S; ++y)
    for (int x = 0; x < S; ++x) kindMap[(size_t)y * S + x] = y > fxEdge[x] - 2 ? 2 : 0;
  for (int y = 0; y < S; ++y)                     // 鏡の面(ふちの線のぶん、上と左右に2点広げる)
    for (int f = 0; f < 8; ++f) {
      if (!mf[f].on) continue;
      int a = -1, e = -1;
      for (int yy = max(0, y - 2); yy <= min(S - 1, y + 2) && a < 0; ++yy) if (fxFace[yy * 16 + f * 2] >= 0) { a = fxFace[yy * 16 + f * 2]; e = fxFace[yy * 16 + f * 2 + 1]; }
      if (a < 0) continue;
      for (int x = max(0, a - 2); x <= min(S - 1, e + 2); ++x) if (y < fxEdge[x] - 1) kindMap[(size_t)y * S + x] = 1;
    }
}
static inline int viewShiftG(int y) {            // 地面と花の、行ごとのずらす量(手前ほど大きい)
  const float dep = fmaxF(0.0f, (y - fxGround0) / (float)max(1, S - fxGround0));
  return (int)((VIEW_FAR + (VIEW_NEAR - VIEW_FAR) * dep * sqrtf(dep)) * S / 466.0f);
}
// 鏡の面 f の点(x, y)の色(NullPavilion の鏡の膜)。面の外なら false
static bool mirrorPix(int x, int y, int sxv, uint16_t& out) {
  const uint16_t* bg = bgImg;
  for (int f = 0; f < 8 && mOff; ++f) {
    const int a0 = fxFace[y * 16 + f * 2], b0 = fxFace[y * 16 + f * 2 + 1];
    if (a0 < 0 || !mf[f].on || x < a0 || x > b0) continue;
    const MFace& m = mf[f];
    const float* O = mOff + (size_t)f * MGH * MGW * 2;
    const float u = (x - m.x0) / (float)(m.x1 - m.x0), top = m.t0 + (m.t1 - m.t0) * u;
    const float gxf = fminF(MGW - 1.001f, fmaxF(0.0f, u * (MGW - 1)));
    const float gyf = fminF(MGH - 1.001f, fmaxF(0.0f, (y - top) / (m.bot - top) * (MGH - 1)));
    const int ix = (int)gxf, iy = (int)gyf; const float fx = gxf - ix, fy = gyf - iy;
    const float* p00 = O + (iy * MGW + ix) * 2; const float* p10 = p00 + 2; const float* p01 = p00 + MGW * 2; const float* p11 = p01 + 2;
    const float ox = (p00[0] * (1 - fx) + p10[0] * fx) * (1 - fy) + (p01[0] * (1 - fx) + p11[0] * fx) * fy;
    const float oy = (p00[1] * (1 - fx) + p10[1] * fx) * (1 - fy) + (p01[1] * (1 - fx) + p11[1] * fx) * fy;
    int sx = x + (int)ox + sxv / 2;                              // 見る角度で映りもずれる(面のはしでは折り返す)
    if (sx > b0) sx = 2 * b0 - sx; if (sx < a0) sx = 2 * a0 - sx; sx = min(b0, max(a0, sx));
    int sy = y + (int)oy;
    if (sy < 0 || sy >= S || fxFace[sy * 16 + f * 2] < 0 || sx < fxFace[sy * 16 + f * 2] || sx > fxFace[sy * 16 + f * 2 + 1]) sy = y;
    const float l = -(ox * 0.5f + oy * 0.85f) / mirK;
    const int k256 = (int)(256.0f * fminF(1.45f, fmaxF(0.62f, 1.0f + 0.9f * l)));
    const int spec = l > 0.45f ? (int)((l - 0.45f) * 14.0f) : 0;
    uint16_t c = bg[(size_t)sy * S + sx];
    if (noBox) {                                                 // 鏡に映る花畑と空: 左右を返し、映った地平を少し上げて、花畑が大きく映るように
      const int fcx = (m.x0 + m.x1) / 2, hzM = (int)(top + (m.bot - top) * 0.62f);
      int rx = fcx + (fcx - x) * 2 + (int)ox - sxv;
      int ry = y < hzM ? (int)(y * (float)fxHz / max(1, hzM)) : fxHz + (int)((y - hzM) * (float)(S - 1 - fxHz) / max(1.0f, m.bot - hzM));
      ry += (int)oy;
      if (rx < 0) rx = -rx; if (rx >= S) rx = 2 * S - 2 - rx; rx = min(S - 1, max(0, rx)); ry = min(S - 1, max(0, ry));
      c = mix565(c, mirShade(noBox[(size_t)ry * S + rx], 215, 0), 19);
    }
    c = mirShade(c, k256, min(6, spec));
    const float tl = fminF(1.0f, (fabsf(ox) + fabsf(oy)) / mirK);
    out = mix565(c, silverCol, (uint32_t)(5 + tl * 9));
    return true;
  }
  return false;
}
// 1行を作る: 見る向きで奥行きごとにずらした景色 + 鏡のゆらぎ + 海と川のきらめき + 花のそよぎ
static void fxRow(uint16_t* row, int y, int xs, int xe) {
  const uint16_t* bg = bgImg;
  if (!fxEdge || !kindMap || !noBox) { memcpy(row + xs, bg + (size_t)y * S + xs, (size_t)(xe - xs + 1) * 2); return; }
  const int tt = (int)(simT * 40.0f), tick = (int)(simT * 9.0f);
  const float k466 = S / 466.0f;
  const int s0 = (int)(-viewX * VIEW_SKY * k466), s1 = (int)(-viewX * VIEW_BOX * k466);
  const int v0 = (int)(-viewY * VIEW_SKY * VIEW_VERT * k466), v1 = (int)(-viewY * VIEW_BOX * VIEW_VERT * k466);
  // 地面と花: この行で決まるもの(行ごとに、手前ほど大きくずらす)
  const int gsh = viewShiftG(y);
  int gy = y - (int)(-viewY * gsh * VIEW_VERT);
  if (gy >= S) gy = 2 * S - 1 - gy; gy = min(S - 1, max(0, gy));
  const int gdx = (int)(-viewX * gsh);
  const uint8_t* kG = kindMap + (size_t)gy * S;
  const uint16_t* gsrc = noBox + (size_t)gy * S;
  const int amp = (int)(fmaxF(0.0f, (gy - fxGround0) / (float)max(1, S - fxGround0)) * 40.0f), amp2 = amp >> 1;   // 風のそよぎ(1/16 点)
  const int ra = fxRivA ? fxRivA[gy] : S, rb = fxRivA ? fxRivB[gy] : -1;
  const int ph1 = gy * 2 - tt * 2, ph2 = gy * 7 + tt * 9, dRiv = SINT[(gy * 9 + tt * 3) & 255] >> 6;
  // 鏡の箱と、空と海: この行で決まるもの
  const int by = min(S - 1, max(0, y - v1)), ky = min(S - 1, max(0, y - v0));
  const uint8_t* kB = kindMap + (size_t)by * S;
  const uint16_t* sky = noBox + (size_t)ky * S;
  const bool seaRow = fxSea && ky > fxHz + 1 && ky < fxHz + S / 8;
  const int seaW = 4 + (ky - fxHz) * 9 / 10;
  for (int x = xs; x <= xe; (x + 1 <= xe ? (void)(row[x + 1] = row[x]) : (void)0), x += 2) {   // 2点おきに計算して、右どなりにも同じ色
    // 1) 地面と花
    {
      int sx = x - gdx;                                                         // はしからはみ出したら、折り返す
      if (sx < 0) sx = -1 - sx; if (sx >= S) sx = 2 * S - 1 - sx; sx = min(S - 1, max(0, sx));
      if (kG[sx] == 2) {
        const int sh = ((SINT[(ph1 + (sx >> 2)) & 255] * amp) >> 11) + ((SINT[(ph2 + (sx >> 3) * 3) & 255] * amp2) >> 11);
        if (sx >= ra && sx <= rb) {             // 川(地面なので、そよがない。きらめきだけ)
          const int fx = sx - sh;
          if (fx < ra || fx > rb) { row[x] = gsrc[min(S - 1, max(0, fx))]; continue; }
          row[x] = gsrc[min(rb, max(ra, sx + dRiv))];
          const uint32_t h = ((uint32_t)(sx >> 1) * 73856093u) ^ ((uint32_t)gy * 19349663u) ^ ((uint32_t)(tt >> 2) * 83492791u);
          const int v = (int)((h * 2654435761u) >> 24);
          if (v > 248) row[x] = fxBright(row[x], (v - 248) * 2);
          continue;
        }
        int ss = min(S - 1, max(0, sx - sh));
        if (ss >= ra && ss <= rb) ss = sx < ra ? 2 * ra - ss - 1 : 2 * rb - ss + 1;
        ss = min(S - 1, max(0, ss));
        if (kG[ss] != 2) ss = sx;
        row[x] = gsrc[ss];
        continue;
      }
    }
    // 2) 鏡の箱(奥行きの分だけずれる。映りは見る角度でずれる)
    {
      const int sx = min(S - 1, max(0, x - s1));
      if (kB[sx] == 1) {
        uint16_t c;
        row[x] = mirrorPix(sx, by, s1, c) ? c : bg[(size_t)by * S + sx];   // 面の外(ふちの線)はそのまま
        continue;
      }
    }
    // 3) 空と海(ほとんど動かない)
    {
      const int sx = min(S - 1, max(0, x - s0));
      uint16_t c = sky[sx];                     // 箱のない景色(箱が動いて見えてきた所も、これでうまる)
      if (seaRow && abs(sx - fxSunX) < seaW) {   // 海のきらめき
        const uint32_t h = ((uint32_t)(sx >> 1) * 73856093u) ^ ((uint32_t)ky * 19349663u) ^ ((uint32_t)tick * 83492791u);
        const int v = (int)((h * 2654435761u) >> 24);
        if (v > 246) c = fxBright(c, (v - 246) * 2);
      }
      row[x] = c;
    }
  }
}

static void drawGauge(int pct) {                 // 読み込み中のゲージ(丸の下の方に、横長の棒)。pct < 0: 枠だけ描く
  const int gw = 200, gh = 10, gx = ((W - gw) / 2) & ~1, gy = (YOFF + S * 3 / 4) & ~1;
  if (pct < 0) { M5.Display.fillRect(gx - 4, gy - 4, gw + 8, gh + 8, M5.Display.color565(60, 56, 70)); M5.Display.fillRect(gx - 2, gy - 2, gw + 4, gh + 4, M5.Display.color565(12, 10, 18)); return; }
  const int w = (gw * min(100, pct) / 100) & ~1;
  if (w > 0) M5.Display.fillRect(gx, gy, w, gh, M5.Display.color565(236, 196, 110));
}
// ---- 絵を描く道具の、行ごとの計算を2つのコアで(コア0の描く係に、奇数の行を頼む)
static mpaint::RowFn pjFn = nullptr;
static void* pjCtx = nullptr;
static int pjA = 0, pjB = 0;
static void parRun(mpaint::RowFn fn, void* ctx, int a, int b) {
  if (!workerUp) { fn(ctx, a, b, 1, true); return; }
  pjCtx = ctx; pjA = a + 1; pjB = b; __atomic_store_n(&pjFn, fn, __ATOMIC_RELEASE);
  xSemaphoreGive(semGo);
  fn(ctx, a, b, 2, true);
  xSemaphoreTake(semDone, portMAX_DELAY);
  pjFn = nullptr;
}
// ---- 描いた場面を、フラッシュ(FFat)に保存しておく。2回目からは読むだけ
static constexpr uint32_t CACHE_MAGIC = 0x4E4D4431u, CACHE_VER = 26;   // 絵の描き方を変えたら CACHE_VER を上げる(古い保存は使わない)
static bool fsOk = false;
static void cacheName(int k, char* buf) { snprintf(buf, 40, "/nullmeadow_scene%d.bin", k + 1); }
__attribute__((cold)) static bool cacheLoad(int k, uint16_t* noBoxOut, uint16_t* boxOut) {
  if (!fsOk) return false;
  char nm[40]; cacheName(k, nm);
  File f = FFat.open(nm, "r");
  if (!f) return false;
  const int h = S / 2; uint32_t hd[4] = {0, 0, 0, 0};
  bool ok = f.read((uint8_t*)hd, sizeof(hd)) == sizeof(hd) && hd[0] == CACHE_MAGIC && hd[1] == CACHE_VER && hd[2] == (uint32_t)h && hd[3] == (uint32_t)sizeof(MeadowFx);
  if (!mpaint::HALF) mpaint::HALF = (uint16_t*)ps_malloc((size_t)h * h * 2);
  MeadowFx* fx = (MeadowFx*)malloc(sizeof(MeadowFx));
  const size_t img = (size_t)h * h * 2;
  ok = ok && mpaint::HALF && fx && f.read((uint8_t*)fx, sizeof(MeadowFx)) == sizeof(MeadowFx);
  if (ok) { drawGauge(30); ok = f.read((uint8_t*)mpaint::HALF, img) == img; if (ok) mpaint::expandHalf(mpaint::HALF, noBoxOut, S); }
  if (ok) { drawGauge(70); ok = f.read((uint8_t*)mpaint::HALF, img) == img; if (ok) mpaint::expandHalf(mpaint::HALF, boxOut, S); }
  if (ok) { MEADOW_FX[k] = *fx; drawGauge(100); }
  free(fx); f.close();
  Serial.printf("scene %d: %s\n", k + 1, ok ? "loaded from flash" : "cache not usable, painting");
  return ok;
}
__attribute__((cold)) static uint16_t* decodeScene(int k, int slot) {
  if (!bgCv[slot]) {
    bgCv[slot] = new M5Canvas(&M5.Display);
    bgCv[slot]->setColorDepth(16); bgCv[slot]->setPsram(true);
    if (!bgCv[slot]->createSprite(S, S)) return nullptr;
  }
  bgCv[slot]->fillSprite(0);
  if (!noBox) noBox = (uint16_t*)ps_malloc((size_t)S * S * 2);
  if (!noBox) return nullptr;
  uint16_t* box = (uint16_t*)bgCv[slot]->getBuffer();
  drawGauge(-1);                                   // 読み込み中のゲージ
  if (cacheLoad(k, noBox, box)) return box;        // 保存してあれば、読むだけ
  const uint32_t t0 = millis();
  mpaint::runPar = parRun;                         // 行ごとの計算は2つのコアで
  mpaint::onProg = drawGauge;
  mpaint::progBase = 0; mpaint::progSpan = 100; mpaint::progLast = -1;
  const int h = S / 2; const size_t img = (size_t)h * h * 2;
  const bool ok = mpaint::paintBothHalf(k, noBox, box, S);              // 鏡の箱のない景色と、場面の絵を1回で(半分の大きさで描いて広げる)
  mpaint::onProg = nullptr;
  Serial.printf("scene %d: painted (%u ms)\n", k + 1, (unsigned)(millis() - t0));
  if (ok && fsOk) {                                // フラッシュに保存(次からは読むだけ)
    char nm[40]; cacheName(k, nm);
    File f = FFat.open(nm, "w");
    if (f) {
      const uint32_t hd[4] = {CACHE_MAGIC, CACHE_VER, (uint32_t)h, (uint32_t)sizeof(MeadowFx)};
      bool w = f.write((const uint8_t*)hd, sizeof(hd)) == sizeof(hd);
      w = w && f.write((const uint8_t*)&MEADOW_FX[k], sizeof(MeadowFx)) == sizeof(MeadowFx);
      w = w && f.write((const uint8_t*)mpaint::HALFN, img) == img && f.write((const uint8_t*)mpaint::HALF, img) == img;
      f.close();
      if (!w) FFat.remove(nm);
      Serial.printf("scene %d: %s\n", k + 1, w ? "saved to flash" : "save failed");
    }
  }
  return ok ? box : nullptr;
}
__attribute__((cold)) static bool loadBg() {
  bgImg = decodeScene(sceneNo, 0); bgSlot = 0;
  HAZE = SCENE_HAZE[sceneNo];
  buildFx(sceneNo); setupMirror(sceneNo); buildKind();
  return bgImg != nullptr;
}
static inline uint16_t mix565(uint16_t p, uint16_t q, uint32_t a) {   // バイトを入れ替えた 565 どうしを混ぜる(a: 0..32)
  uint32_t x = __builtin_bswap16(p), y = __builtin_bswap16(q);
  x = (x | (x << 16)) & 0x07E0F81Fu; y = (y | (y << 16)) & 0x07E0F81Fu;
  const uint32_t r = ((x * (32 - a) + y * a) >> 5) & 0x07E0F81Fu;
  return __builtin_bswap16((uint16_t)(r | (r >> 16)));
}

// ---------------------------------------------------------------- 描画(帯ごと)
static void __attribute__((noinline)) bgPart(uint16_t* row, int y, int a, int b, int fk) {   // 場面の絵の a..b(入れかわり中は2枚を混ぜる)
  if (a > b) return;
  if (fk == 0) { fxRow(row, y, a, b); return; }          // 見る向きでずらした景色と、動き
  const uint16_t* A = bgImg + (size_t)y * S, *B = bgNext + (size_t)y * S;
  for (int x = a; x <= b; x += 2) { row[x] = mix565(A[x], B[x], (uint32_t)fk); if (x + 1 <= b) row[x + 1] = row[x]; }
}
static void renderBand(int k, uint16_t* buf) {
  const int y0 = k * BAND, y1 = min(S, y0 + BAND);
  const int bx0 = bandX0[k], bw = bandW[k], bx1 = bx0 + bw - 1;

  // ---- 背景: 縦横2点おきに作って 2x2 に広げる(奇数の行は上の行を写す)
  const int fk = fadeK;
  for (int y = y0; y < y1; ++y) {
    uint16_t* row = buf + (y - y0) * bw - bx0;               // row[x] で画面の x
    const int xs = max((int)spanS[y], bx0), xe = min((int)spanE[y], bx1);
    for (int x = bx0; x < xs; ++x) row[x] = blackCol;
    for (int x = max(xe + 1, bx0); x <= bx1; ++x) row[x] = blackCol;
    if (xe < xs) continue;
    if (((y - y0) & 1) && y > y0) {
      const int ps = max((int)spanS[y - 1], bx0), pe = min((int)spanE[y - 1], bx1);
      const int ca = max(xs, ps), cb = min(xe, pe);
      if (ca <= cb) {
        memcpy(row + ca, row - bw + ca, (size_t)(cb - ca + 1) * 2);
        bgPart(row, y, xs, ca - 1, fk); bgPart(row, y, cb + 1, xe, fk);
        continue;
      }
    }
    bgPart(row, y, xs, xe, fk);
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

  // ---- 蝶(奥から手前へ)。まず鏡に映った蝶(左右を返し、小さく暗く。鏡の面の中だけ)、つぎに本物
  const float k466 = S / 466.0f;
  const int s1 = (int)(-viewX * VIEW_BOX * k466), v1 = (int)(-viewY * VIEW_BOX * VIEW_VERT * k466);
  for (int pass = 0; pass < 2; ++pass)
  for (int n = 0; n < MAXFLY; ++n) {
    const int i = order[n];
    if (i >= nFly) continue;
    const Fly& f = fly[i];
    const bool refl = pass == 0;
    if (refl && !kindMap) continue;
    // 鏡に映る蝶: 鏡までの遠さを「その遠さにいる蝶の大きさ」RMIR で表す。映った像は、鏡の奥(鏡の2倍 − 蝶)の遠さに見えるので、
    //   手前の蝶ほど映りは小さく、画面の奥(消える点)寄りになる。鏡より奥の蝶は映らない
    float sc = 1.0f;
    if (refl) { const float rm = RMIR * S; if (f.R < rm * 0.92f) continue; sc = rm / (2.0f * f.R - rm); }
    const float fcx = refl ? Sc + (f.cx - Sc) * sc : f.cx, fcy = refl ? fxHz + (f.cy - fxHz) * sc : f.cy;
    const float rr = f.rr * sc, rr2 = rr * rr;
    const int fy0 = (int)ffloorf(fcy - rr), fy1 = (int)ceilf(fcy + rr);
    const int ra = max(y0, fy0), rb = min(y1 - 1, fy1);
    if (ra > rb) continue;
    const int T = f.T;
    const float half = T * 0.5f, is = 1.0f / sc, fl = refl ? -1.0f : 1.0f;
    const float dxx = f.dxx * is * fl, dxy = f.dxy * is * fl, dyx = f.dyx * is, dyy = f.dyy * is;
    const uint8_t*  tex = f.tex;
    const uint16_t* pal = f.pal;
    const int32_t du = (int32_t)(dxx * 65536.0f), dv = (int32_t)(dyx * 65536.0f);
    for (int y = ra; y <= rb; ++y) {
      const float dy = y + 0.5f - fcy, w2 = rr2 - dy * dy;
      if (w2 <= 0) continue;
      const float hw = sqrtf(w2);
      const int xa = max(max((int)ffloorf(fcx - hw), bx0), (int)spanS[y]);
      const int xb = min(min((int)ffloorf(fcx + hw), bx1), (int)spanE[y]);
      if (xa > xb) continue;
      const float ex = xa + 0.5f - fcx;
      int32_t u = (int32_t)((half + dxx * ex + dxy * dy) * 65536.0f);
      int32_t v = (int32_t)((half + dyx * ex + dyy * dy) * 65536.0f);
      uint16_t* row = buf + (y - y0) * bw - bx0;
      const int ky = min(S - 1, max(0, y - v1));
      for (int x = xa; x <= xb; ++x, u += du, v += dv) {
        const uint32_t tx = (uint32_t)(u >> 16), ty = (uint32_t)(v >> 16);
        if (tx < (uint32_t)T && ty < (uint32_t)T) {
          const uint8_t id = tex[ty * T + tx];
          if (!id) continue;
          if (refl) {
            if (kindMap[(size_t)ky * S + min(S - 1, max(0, x - s1))] != 1) continue;   // 鏡の面の中だけ
            row[x] = mix565(mirShade(pal[id], 190, 0), row[x], 9);
          } else row[x] = pal[id];
        }
      }
    }
  }
}

static void __attribute__((noinline)) MOD_IRAM renderBandIram(int k, uint16_t* b) { renderBand(k, b); }

// 帯は、空いているコアが次の1本を取って描く(コア1は描いた帯を順番に画面へ送る)
static volatile int nextBand = 0;
static volatile uint8_t bandReady[64];
static int takeBand(int limit) {
  int k = __atomic_load_n(&nextBand, __ATOMIC_ACQUIRE);
  while (k < NB && k <= limit) {
    if (__atomic_compare_exchange_n(&nextBand, &k, k + 1, false, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) {
      if (bandVis[k]) renderBandIram(k, slotBuf[k % NSLOT]);
      __atomic_store_n(&bandReady[k], 1, __ATOMIC_RELEASE);
      return k;
    }
  }
  return -1;
}
static void workerTask(void*) {
  for (;;) {
    xSemaphoreTake(semGo, portMAX_DELAY);
    if (pjFn) { pjFn(pjCtx, pjA, pjB, 2, false); xSemaphoreGive(semDone); continue; }   // 絵を描く道具の、奇数の行
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
static void renderFrame() {
  for (int k = 0; k < NB; ++k) bandReady[k] = 0;
  __atomic_store_n(&doneUpTo, -1, __ATOMIC_RELEASE);
  __atomic_store_n(&nextBand, 0, __ATOMIC_RELEASE);
  uint32_t busy = 0, send = 0;
  if (workerUp) xSemaphoreGive(semGo);
  if (!fbMode) M5.Display.startWrite();
  for (int k = 0; k < NB; ++k) {
    const uint32_t tWait = millis(); bool told = false;
    while (!__atomic_load_n(&bandReady[k], __ATOMIC_ACQUIRE)) {
      if (!told && millis() - tWait > 2000) {      // 2秒たっても帯が描けない: どこで止まったかを出す
        told = true;
        Serial.printf("stall: band %d/%d  next %d  done %d  ready %d %d\n", k, NB, __atomic_load_n(&nextBand, __ATOMIC_ACQUIRE), __atomic_load_n(&doneUpTo, __ATOMIC_ACQUIRE), bandReady[k], k + 1 < NB ? bandReady[k + 1] : -1);
      }
      const uint32_t t0 = micros();
      if (takeBand(__atomic_load_n(&doneUpTo, __ATOMIC_ACQUIRE) + NSLOT) >= 0) busy += micros() - t0;
    }
    if (!fbMode) { const uint32_t tw = micros(); M5.Display.waitDMA(); send += micros() - tw; }
    __atomic_store_n(&doneUpTo, k - 1, __ATOMIC_RELEASE);
    if (bandVis[k]) {
      const int y0 = k * BAND, h = min(BAND, S - y0);
      if (fbMode) { const uint32_t ts = micros(); M5.Display.pushImage(XOFF + bandX0[k], YOFF + y0, bandW[k], h, slotBuf[k % NSLOT]); send += micros() - ts; }
      else M5.Display.pushImageDMA(XOFF + bandX0[k], YOFF + y0, bandW[k], h, slotBuf[k % NSLOT]);
    }
  }
  if (!fbMode) M5.Display.waitDMA();
  __atomic_store_n(&doneUpTo, NB, __ATOMIC_RELEASE);
  if (!fbMode) M5.Display.endWrite();
#if defined(pdMS_TO_TICKS)
  if (workerUp && xSemaphoreTake(semDone, pdMS_TO_TICKS(2000)) != pdTRUE) {   // 2秒たってもコア0が終わらない
    Serial.printf("stall: worker not done  next %d  done %d\n", __atomic_load_n(&nextBand, __ATOMIC_ACQUIRE), __atomic_load_n(&doneUpTo, __ATOMIC_ACQUIRE));
    xSemaphoreTake(semDone, portMAX_DELAY);
  }
#else
  if (workerUp) xSemaphoreTake(semDone, portMAX_DELAY);
#endif
  { static int nf = 0; if (nf < 3) Serial.printf("frame %d ok\n", ++nf); }   // はじめの3コマは、描けたら知らせる
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
  nmStart();                                         // 明るさ(メニューで決めたもの)にそろえる
  Serial.begin(115200);

  if (M5.Display.width() < M5.Display.height()) M5.Display.setRotation(1);
  W = M5.Display.width(); H = M5.Display.height();   // 大きさは、画面へ直接に切り替える前に読む(あとだと 468 になる)
  {                                                // 画面の送り方: StopWatch は、置き場(PSRAM)を通さず本物の画面へ直接
    auto* pnl = M5.Display.getPanel();
    fbState = (pnl && pnl->getAutoDisplay()) ? 1 : 0;
    fbMode = fbState == 1;
    bool direct = false;
#if defined(NM_DIRECT_AMOLED)
    if (fbMode && DIRECT_SEND && M5.getBoard() == m5::board_t::board_M5StopWatch) {
      M5.Display.waitDisplay();                    // 置き場から画面への送りが、終わるのを待ってから切り替える
      lgfx::Panel_AMOLED* inner = NmFbPeek::inner(pnl);
      if (inner) { M5.Display.setPanel(inner); fbMode = false; direct = true; }
    }
#endif
    Serial.printf("display: %s\n", fbMode ? "framebuffer (send each band)" : direct ? "direct DMA (StopWatch, no framebuffer)" : "direct DMA");
  }
  isRound = (W == H);
  S = (isRound ? W : max(W, H)) & ~1;
  Sc = (S - 1) * 0.5f;
  XOFF = (W - S) / 2;
  YOFF = ((H - S) / 2 / BAND) * BAND;
  NB = (S + BAND - 1) / BAND;
  BW = (S >> BGSH) + 1;
  vx0 = (float)max(0, -XOFF); vx1 = (float)min(S, W - XOFF) - 1;
  vy0 = (float)max(0, -YOFF); vy1 = (float)min(S, H - YOFF) - 1;
  viewR = 0.5f * fminF(vx1 - vx0, vy1 - vy0);
  Serial.println("NullMeadow v27");
  Serial.printf("screen %dx%d  area %d\n", W, H, S);

  M5.Display.fillScreen(TFT_BLACK);

  semGo   = xSemaphoreCreateBinary();             // 描く係(コア0)は、ほかのメモリを取る前に作る(あとだと内蔵メモリが足りずに作れなかった)
  semDone = xSemaphoreCreateBinary();
  workerUp = semGo && semDone && xTaskCreatePinnedToCore(workerTask, "band", 8192, nullptr, 1, nullptr, 0) == pdPASS;
  fsOk = FFat.begin(true);                         // 描いた場面の保存先(なければ、毎回描く)
  Serial.printf("worker: %s  internal free %u (largest %u)\n", workerUp ? "ok" : "FAILED (core1 only)", (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL), (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL));
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
    spanS[y] = max(0, (int)ffloorf(Sc - s)); spanE[y] = min(S - 1, (int)ceilf(Sc + s));
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

  imuOk = M5.Imu.isEnabled();
  if (!loadBg()) fail("Image error");
  sceneT0 = millis();
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

}

static void buildPalettes();
static void nextScene() {                         // 次の場面を読んで、ふわっと入れかえはじめる
  if (fadeK) return;
  const int k = (sceneNo + 1) % NSCENE;
  uint16_t* p = decodeScene(k, 1 - bgSlot);
  if (!p) return;
  bgNext = p; sceneNo = k; fadeT0 = millis(); fadeK = 1; sceneT0 = millis();
  Serial.printf("scene %d\n", k + 1);
}
static void stepScene() {
  if (fadeK) {
    const uint32_t el = millis() - fadeT0;
    if (el >= (uint32_t)FADE_MS) {                // 入れかわり終わり: 次の場面をいまの場面に。もやの色も変える
      bgImg = bgNext; bgSlot = 1 - bgSlot; fadeK = 0;
      buildFx(sceneNo); setupMirror(sceneNo); buildKind();
      HAZE = SCENE_HAZE[sceneNo]; buildPalettes();
    } else { const float t = el / (float)FADE_MS; fadeK = max(1, min(31, (int)(t * t * (3 - 2 * t) * 32.0f))); }
  } else if (SCENE_SEC > 0 && millis() - sceneT0 >= (uint32_t)SCENE_SEC * 1000u) nextScene();
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
    else if (M5.BtnA.wasClicked()) nextScene();
    if (M5.BtnB.wasClicked()) nextCount();
  } else {
    if (M5.BtnPWR.wasClicked()) nextCount();
    if (pressed) {
      if (now - lastTapMs < 350) {                             // ダブルタップ: 上半分なら数、下半分なら色
        if (t.y < H / 2) nextScene(); else nextCount();
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

  stepScene();
  {                                               // 鏡の面は見る向きでずれているので、指の位置をもどして当てる
    const float bx = -viewX * VIEW_BOX * S / 466.0f, by = -viewY * VIEW_BOX * VIEW_VERT * S / 466.0f;
    if (in.tapped) mirrorTap(in.tx - bx, in.ty - by);   // 鏡の面をタップすると、波が広がる
    mirrorPress(in.touching, in.tx - bx, in.ty - by);
  }
  stepMirror(in.dt);
  stepFlowers(in.dt);
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
}  // namespace NullMeadow
