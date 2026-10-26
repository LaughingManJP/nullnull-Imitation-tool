// NullBodhisattva  -  NullMenu 用モジュール(元のスケッチ NullBodhisattva_v22 を名前空間に入れたもの)
// 0 で始まる変数(配列も)は、ほかの作品と共通の置き場(NM_ARENA)に置いています。
// 一度に動くのは1つだけ(切り替えるときは再起動)なので、重なっていても問題ありません。
#include "nm_common.h"
#if defined(ESP_PLATFORM) && __has_include(<lgfx/v1/panel/Panel_AMOLED.hpp>)
#include <lgfx/v1/panel/Panel_AMOLED.hpp>
#endif
#pragma GCC optimize("O3")

namespace NullBodhisattva {
static constexpr size_t nmOv0 = 0;

// NullBodhisattva v22  -  null菩薩
//   M5Stack StopWatch 用(丸い 466x466 画面)。CoreS3 系でも画面いっぱいに動きます。
//
// 4つの場面があります(StopWatch はボタンA、CoreS3 は画面の上半分をダブルタップで切り替え)
//
// 場面0 「箱の菩薩」
//   ・座った菩薩の姿が、大小の「クラスの箱」でできている。大きな箱は小さな箱に分かれ(入れ子)、
//     親の箱と子の箱は、継承の細い線でつながっている。胸のまわりは金色、外へいくほど青から紫
//   ・胸から外へ、呼吸の波がゆっくり広がる。波が来ると、小さな箱がまとまって大きな箱になり、
//     波が引くと、箱がさらに細かく分かれる(箱が呼吸している)
//   ・継承の線の上を、光の点(メッセージ)が胸から手足の先の箱へと流れていく
//   ・頭のうしろの光輪と、体を囲む舟形の光背は、小さな箱(インスタンス)の輪。光が輪をめぐる
//   ・外周を、菩薩のクラス定義の文字がゆっくり回る
//   ・青い蝶(ふつう5匹)が、菩薩のまわりをゆらゆら舞う
//   触ると: 触った所の箱が光りながら吹き飛び、約15秒で元の場所へ戻る。蝶はタップで驚いて散る
//
// 場面1 「文字の菩薩」
//   ・般若心経の文字が縦の列になって、上から雨のように降ってくる(先頭の字は白く光り、うしろは消えていく)
//   ・文字の列は、それぞれの速さでゆっくり下へ流れていく。どの列にも、5〜30 文字おきに「ﾇﾙ」が紛れ込んでいる(NullSutra と同じ)
//   ・菩薩の形の所だけ、文字が金色に灯り、降る文字の中に菩薩の姿と光輪が浮かび上がる
//   ・触ると、そこから波紋が広がり、通り道の文字が光って横にゆれる。なぞると、なぞった道すじから次々に
//
// 場面2 「分かれる菩薩」(null)
//   ・まっくらな中、まん中の菩薩(this)から、分身が走査線のように上から現れ(new)、らせん状に増えていく
//   ・しばらくすると分身は乱れ(行がずれて赤と青緑に分かれ、抜ける)、ふちだけの抜けがらになって null と出て、消える
//   ・親が null になった分身は「迷子」になって、暗くふらふらする。ときどき、まん中からゴミ集め(GC)の光がぐるっと回り、
//     当たった迷子は null になる。満杯(下の heap 52/52)が続いたときも GC が走り、古いものから片付ける
//   ・分身を長押しすると、それが this になり、親子の線を this を根につなぎ直す(まん中を長押しすると戻る)
//   ・this からなぞって離すと、離した所に new。短いタップは、その近くに new(空きがなければ、近くの分身が null に)
//   ・本体を振ると NullPointerException。this のほかは全部 null になり、this から作り直す
//   ・色は、ほかの場面と同じく A 長押しで変わる(this と光の差し色。白銀はほぼ白黒)
//
// 場面3 「映る菩薩」
//   ・菩薩は1体だけ。見えない合わせ鏡に映った像が、左右の奥へどこまでも続く(映るたびに左右が反転)
//   ・本体を左右・上下に傾けると、奥の像の並びと鏡の枠がずれて、のぞき込む向きが変わる
//     (この場面を開いたときの持ち方が「水平」)。横になぞってもずれる。タップすると、金色の光が奥へ伝わっていく
//
// 操作:
//   StopWatch … ボタンA 短押し: 場面   ボタンA 長押し: 色(金蒼 / 翡翠 / 紅蓮 / 白銀)
//               ボタンB 短押し: 明るさ   ボタンB 長押し: 蝶の数(5 / 10 / 20)
//   CoreS3系  … 画面の上半分をダブルタップ: 場面   下半分をダブルタップ: 色
//               電源ボタン短押し: 明るさ
//   色を変えても止まりません。背景は動きながら少しずつ描き直し、描き終えたところで入れ替わります(今いる場面が先)
//   読み込みのあいだは、画面のまん中に「読み込み中」と進み具合の輪が出ます。
//   場面1〜3の準備(菩薩の絵・般若心経の文字・背景)は、箱の菩薩が動き出してから、1コマに1つずつ進めます。
//   準備が終わる前に場面を切り替えたときは、残りをその場で済ませます(少し待ちます)
//   傾きの向きが逆のときは、調整用の TILT_FLIP_X / TILT_FLIP_Y / TILT_SWAP を変える
//
// 必要なもの:
//   ボードマネージャー M5Stack >= 3.3.7 / ボード: M5StopWatch(CoreS3 なら M5CoreS3)
//   ライブラリ M5Unified >= 0.2.15, M5GFX >= 0.2.21
//   PSRAM を使います(StopWatch と CoreS3 には 8MB 入っています)
//
// しくみ:
//   背景・格子・光背の線・蓮・継承の線と、それらの光のにじみは、起動時に1枚の絵(PSRAM)に描いておきます。
//   毎フレームは、その絵を写してから、動く物(箱・光背の箱・メッセージ・光の粒・外周の文字)を
//   光として足していきます。箱は「四分木」(四角を4つに分けていく木)で、呼吸の波の強さで
//   どの段の箱を見せるかを決め、切り替わりは明るさの入れ替え(クロスフェード)で見せます。
//   外周の文字は、起動時に文字の帯を作っておき、画面の点ごとに「帯のどこか」を表で引きます。
//   文字の形は DejaVu Sans Mono(自由に使えるフォント)から作った、6x12 の小さな文字です。
//   画面は8行ずつの帯に分けて、2つのコアで交互に描きます。
//   場面1〜3の菩薩は、起動時に菩薩の形から作った「光の点で刻んだ絵」(大きさ3段)を、
//   大きさ・向き・色を変えて、何体も光として足しています。
//   場面1の文字は M5GFX に入っている明朝体(lgfxJapanMincho_20)で、起動時に描いて使います(NullSutra と同じ)。
//   その明朝体に入っていない7字だけは、このスケッチの中の小さなフォント(SUTRA_EXTRA_FONT)で描きます。
//
// 文字: 般若心経(玄奘訳の流布本)。古い経典で、著作権の対象ではありません。

// StopWatch では、画面へ「直接」送る(M5GFX の PSRAM の置き場を通さない)。画面が乱れるときは false に
static constexpr bool DIRECT_SEND = true;
#if defined(ESP_PLATFORM) && defined(LGFX_USE_QSPI) && __has_include(<lgfx/v1/panel/Panel_AMOLED.hpp>)
#define NL_DIRECT_AMOLED 1
struct NlFbPeek : public lgfx::Panel_AMOLED_Framebuffer {
  static lgfx::Panel_AMOLED* inner(lgfx::Panel_Device* p) {
    return static_cast<lgfx::Panel_AMOLED_Framebuffer*>(p)->*(&NlFbPeek::_panel);
  }
};
#endif


// Arduino IDE は関数の宣言を自動でこのあたりに足すので、型の名前だけ先に宣言しておく
struct Col; struct Theme; struct Node; struct DBox; struct PDraw; struct Pulse; struct Part; struct Ring; struct FBuf; struct StatJob; struct HaloB; struct Mask; struct Fly; struct Input; struct SprI; struct Lab; struct Kid; struct Drop; struct Rip;

// ---- 調整用 ----------------------------------------------------------------
static const float BREATH_SPEEDS[3] = {0.35f, 0.60f, 1.00f};   // 呼吸の速さ
static constexpr int   BREATH_START = 1;
static constexpr float RETURN_TIME  = 15.0f;  // 触られてから、すっかり元に戻るまでの時間(秒)
static constexpr float SCATTER      = 0.85f;  // ほどけた箱が飛んでいく距離(画面の半径に対する割合)
static constexpr float TOUCH_R      = 0.13f;  // 触ったとき、箱がほどける範囲(画面の半径に対する割合)
static constexpr float BLAST_TIME   = 0.45f;  // 吹き飛ぶのにかかる時間(秒。短いほど派手)
static constexpr int   SPARKS       = 90;     // タップしたときに飛ぶ火花の数
static constexpr float TEXT_TURN    = 150.0f; // 外周の文字が1周する時間(秒)
static constexpr int   NPULSE       = 14;     // 継承の線を流れるメッセージの数
static const int       FLY_COUNTS[3] = {5, 10, 20};   // 蝶の数の段階
static constexpr int   FLY_START    = 0;      // 最初の数(0: 5匹 / 1: 10匹 / 2: 20匹)
static constexpr float FLY_SIZE     = 0.055f; // 蝶の大きさ(画面に対する割合)
static constexpr int   MAXFLY       = 20;
static constexpr int   SCENE_START  = 0;      // 最初の場面(0: 箱 / 1: 文字 / 2: 分かれる / 3: 映る)
static constexpr float MIR_VY       = -0.06f; // 映る菩薩: 奥の消える点の高さ(画面の半径に対する割合)
static constexpr int   MIRD         = 6;      // 映る菩薩: 奥へ何段映すか
static constexpr bool  TILT_FLIP_X  = false;  // 傾けたとき、左右が逆に動くときは true
static constexpr bool  TILT_FLIP_Y  = false;  // 上下が逆に動くときは true
static constexpr bool  TILT_SWAP    = false;  // 左右と上下が入れかわっているときは true
static constexpr bool  TILT_STOPWATCH_FIX = true;   // StopWatch はセンサーが 90 度回ってついているので、自動で読みかえる
static constexpr float TILT_DEAD    = 0.05f;  // これより小さい傾きは無視する
static constexpr float TILT_RELEVEL = 15.0f;  // 同じ角度で持ち続けると、何秒くらいでそこを「水平」にするか
static constexpr float TILT_RELEVEL_MAX = 0.25f;  // これより大きく傾けているあいだは、水平を合わせなおさない
// ---------------------------------------------------------------------------

static const char RIM_TEXT[] =
  "class Bodhisattva extends Being implements Compassion, Wisdom {  "
  "private final Self self = null;  public Form form() { return Emptiness.instance(); }  "
  "public void save(Being b) { b.suffering.release(); }  }  ";

static constexpr int BAND     = 8;
static constexpr int NSLOT    = 4;
static constexpr int MAXNB    = 64;
static constexpr int MAXNODE  = 2400;
static constexpr int MAXBOX   = 2400;
static constexpr int MAXENT   = 5000;
static constexpr int MAXPART  = 1400;
static constexpr int NRING    = 4;          // 同時に広がる光の輪の数
static constexpr int MAXPD    = MAXPART + 64 + NRING * 600;
static constexpr int FCW = 6, FCH = 12;       // 文字の大きさ(帯の中)

static constexpr size_t nmOa0 = nmAlignTo(nmOv0, alignof(int)), nmOv1 = nmOa0 + sizeof(int); static int& W = *reinterpret_cast<int*>(NM_ARENA + nmOa0);
static constexpr size_t nmOa1 = nmAlignTo(nmOv1, alignof(int)), nmOv2 = nmOa1 + sizeof(int); static int& H = *reinterpret_cast<int*>(NM_ARENA + nmOa1);
static constexpr size_t nmOa2 = nmAlignTo(nmOv2, alignof(int)), nmOv3 = nmOa2 + sizeof(int); static int& S = *reinterpret_cast<int*>(NM_ARENA + nmOa2);
static constexpr size_t nmOa3 = nmAlignTo(nmOv3, alignof(int)), nmOv4 = nmOa3 + sizeof(int); static int& XOFF = *reinterpret_cast<int*>(NM_ARENA + nmOa3);
static constexpr size_t nmOa4 = nmAlignTo(nmOv4, alignof(int)), nmOv5 = nmOa4 + sizeof(int); static int& YOFF = *reinterpret_cast<int*>(NM_ARENA + nmOa4);
static constexpr size_t nmOa5 = nmAlignTo(nmOv5, alignof(int)), nmOv6 = nmOa5 + sizeof(int); static int& NB = *reinterpret_cast<int*>(NM_ARENA + nmOa5);
static constexpr size_t nmOa6 = nmAlignTo(nmOv6, alignof(float)), nmOv7 = nmOa6 + sizeof(float); static float& Sc = *reinterpret_cast<float*>(NM_ARENA + nmOa6);
static constexpr size_t nmOa7 = nmAlignTo(nmOv7, alignof(float)), nmOv8 = nmOa7 + sizeof(float); static float& H2 = *reinterpret_cast<float*>(NM_ARENA + nmOa7);  // 中心 / 半径(ピクセル)
static constexpr size_t nmOa8 = nmAlignTo(nmOv8, alignof(bool)), nmOv9 = nmOa8 + sizeof(bool); static bool& isRound = *reinterpret_cast<bool*>(NM_ARENA + nmOa8);
static uint8_t* bandVis;
static uint16_t* slotBuf[NSLOT];
static uint16_t* statImg;                         // [S*S] 動かない部分の絵(菩薩のにじみ込み。バイト順入れ替え済み)
static uint16_t* stat0;                        // [S*S] 同じ絵から菩薩を抜いたもの(拡散したときに見える、継承の線だけの空の姿)
static constexpr size_t nmOa9 = nmAlignTo(nmOv9, alignof(int)), nmOv10 = nmOa9 + sizeof(int); static int& bbX0 = *reinterpret_cast<int*>(NM_ARENA + nmOa9);
static constexpr size_t nmOa10 = nmAlignTo(nmOv10, alignof(int)), nmOv11 = nmOa10 + sizeof(int); static int& bbX1 = *reinterpret_cast<int*>(NM_ARENA + nmOa10);
static constexpr size_t nmOa11 = nmAlignTo(nmOv11, alignof(int)), nmOv12 = nmOa11 + sizeof(int); static int& bbY0 = *reinterpret_cast<int*>(NM_ARENA + nmOa11);
static constexpr size_t nmOa12 = nmAlignTo(nmOv12, alignof(int)), nmOv13 = nmOa12 + sizeof(int); static int& bbY1 = *reinterpret_cast<int*>(NM_ARENA + nmOa12);  // 菩薩のにじみがある範囲
static constexpr int PGC = 8;                  // 「箱がそこに居るか」の地図の、ます目の大きさ(ピクセル)
static float*    pg;                           // [PGN*PGN] 箱の居る割合 1 = ぜんぶ居る / 0 = ぜんぶ散った
static constexpr size_t nmOa13 = nmAlignTo(nmOv13, alignof(int)), nmOv14 = nmOa13 + sizeof(int); static int& PGN = *reinterpret_cast<int*>(NM_ARENA + nmOa13);
static constexpr size_t nmOa14 = nmAlignTo(nmOv14, alignof(bool)), nmOv15 = nmOa14 + sizeof(bool); static bool& anyAway = *reinterpret_cast<bool*>(NM_ARENA + nmOa14);  // 散っている箱が1つでもあるか
static int16_t*  spanS;
static int16_t*  spanE;
static int16_t*  bandX0;
static int16_t*  bandW;
static constexpr size_t nmOa15 = nmAlignTo(nmOv15, 16), nmOv16 = nmOa15 + sizeof(uint32_t[2]); static uint32_t (&busyUs)[2] = *reinterpret_cast<uint32_t(*)[2]>(NM_ARENA + nmOa15);

static SemaphoreHandle_t semGo, semDone;
static volatile int readyW = -1, doneUpTo = -1;   // コア0が描き終えた帯 / 画面へ送り終えた帯
static int fbState = -1;  // まだ調べていない / 0: 直接送る / 1: StopWatch の置き場あり
static constexpr size_t nmOa16 = nmAlignTo(nmOv16, alignof(bool)), nmOv17 = nmOa16 + sizeof(bool); static bool& fbMode = *reinterpret_cast<bool*>(NM_ARENA + nmOa16);  // StopWatch: 帯ごとに丸の中の幅だけ送る
static int SPLIT = 2;  // コア1は SPLIT 本に1本の帯を描く(残りはコア0)
static constexpr size_t nmOa17 = nmAlignTo(nmOv17, alignof(uint32_t)), nmOv18 = nmOa17 + sizeof(uint32_t); static uint32_t& sendUs = *reinterpret_cast<uint32_t*>(NM_ARENA + nmOa17);
static constexpr size_t nmOa18 = nmAlignTo(nmOv18, alignof(bool)), nmOv19 = nmOa18 + sizeof(bool); static bool& directSend = *reinterpret_cast<bool*>(NM_ARENA + nmOa18);  // StopWatch: 置き場を通さず、画面へ直接送っている
static constexpr size_t nmOa19 = nmAlignTo(nmOv19, alignof(uint32_t)), nmOv20 = nmOa19 + sizeof(uint32_t); static uint32_t& updUs = *reinterpret_cast<uint32_t*>(NM_ARENA + nmOa19);  // 動きの計算(updateFrame)にかかった時間
static volatile int wJob = 0;                    // 1: コア0 は、描き直しの仕事をする(描く代わりに)
static constexpr size_t nmOa20 = nmAlignTo(nmOv20, alignof(uint32_t)), nmOv21 = nmOa20 + sizeof(uint32_t); static uint32_t& jobBudgetUs = *reinterpret_cast<uint32_t*>(NM_ARENA + nmOa20);

static constexpr size_t nmOa21 = nmAlignTo(nmOv21, alignof(float)), nmOv22 = nmOa21 + sizeof(float); static float& simT = *reinterpret_cast<float*>(NM_ARENA + nmOa21);
static constexpr size_t nmOa22 = nmAlignTo(nmOv22, alignof(float)), nmOv23 = nmOa22 + sizeof(float); static float& holdT = *reinterpret_cast<float*>(NM_ARENA + nmOa22);
static constexpr size_t nmOa23 = nmAlignTo(nmOv23, alignof(float)), nmOv24 = nmOa23 + sizeof(float); static float& fPX = *reinterpret_cast<float*>(NM_ARENA + nmOa23);
static constexpr size_t nmOa24 = nmAlignTo(nmOv24, alignof(float)), nmOv25 = nmOa24 + sizeof(float); static float& fPY = *reinterpret_cast<float*>(NM_ARENA + nmOa24);  // 指が止まっている時間 / 前の指の位置
static constexpr size_t nmOa25 = nmAlignTo(nmOv25, alignof(bool)), nmOv26 = nmOa25 + sizeof(bool); static bool& fWas = *reinterpret_cast<bool*>(NM_ARENA + nmOa25);
static int brightIdx = 2, breathIdx = BREATH_START;
static constexpr size_t nmOa26 = nmAlignTo(nmOv26, alignof(int)), nmOv27 = nmOa26 + sizeof(int); static int& theme = *reinterpret_cast<int*>(NM_ARENA + nmOa26);
static constexpr size_t nmOa27 = nmAlignTo(nmOv27, alignof(uint32_t)), nmOv28 = nmOa27 + sizeof(uint32_t); static uint32_t& bootT0 = *reinterpret_cast<uint32_t*>(NM_ARENA + nmOa27);
static constexpr size_t nmOa28 = nmAlignTo(nmOv28, alignof(uint32_t)), nmOv29 = nmOa28 + sizeof(uint32_t); static uint32_t& bootMs = *reinterpret_cast<uint32_t*>(NM_ARENA + nmOa28);
static constexpr size_t nmOa29 = nmAlignTo(nmOv29, alignof(uint32_t)), nmOv30 = nmOa29 + sizeof(uint32_t); static uint32_t& bootBgMs = *reinterpret_cast<uint32_t*>(NM_ARENA + nmOa29);  // 読み込みにかかった時間(fps の行で知らせる)
static constexpr int NSCENE = 4;
static int scene = SCENE_START;
static inline float sqf(float x) { return x * x; }
static inline float fsin(float x) {              // sinf の近似(誤差 0.001 ほど。毎コマたくさん呼ぶ所用)
  float t = x * 0.15915494f;
  t -= floorf(t + 0.5f);
  float y = 8.0f * t - 16.0f * t * fabsf(t);
  return y + 0.225f * (y * fabsf(y) - y);
}
static inline float fcos(float x) { return fsin(x + 1.5707963f); }
static const uint8_t brightSteps[] = {60, 120, 190, 255};

// ---------------------------------------------------------------- 道具
struct Col { float r, g, b; };
static inline float clamp01(float v) { return v < 0 ? 0 : (v > 1 ? 1 : v); }
static inline float smooth3(float t) { t = clamp01(t); return t * t * (3 - 2 * t); }
static inline Col lerpC(Col a, Col b, float t) { return {a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t}; }
static inline Col mulC(Col a, float k) { return {a.r * k, a.g * k, a.b * k}; }
static constexpr Col C8(int r, int g, int b) { return {r / 255.0f, g / 255.0f, b / 255.0f}; }   // constexpr: 色の表を内蔵メモリでなくフラッシュに置く
static inline uint16_t to565(Col c) {
  return M5.Display.color565((uint8_t)(clamp01(c.r) * 255), (uint8_t)(clamp01(c.g) * 255), (uint8_t)(clamp01(c.b) * 255));
}
static inline uint16_t toPix(Col c) { return __builtin_bswap16(to565(c)); }   // 色がおかしい場合はこの bswap を外す(addPix も)
// 2つの色を混ぜる(q/16 が a の割合)
static inline uint16_t blend565(uint16_t aSw, uint16_t bSw, int q) {
  const uint32_t a = __builtin_bswap16(aSw), b = __builtin_bswap16(bSw);
  const uint32_t A = (a | (a << 16)) & 0x07E0F81Fu, B = (b | (b << 16)) & 0x07E0F81Fu;
  const uint32_t Rr = ((A * q + B * (16 - q)) >> 4) & 0x07E0F81Fu;
  return __builtin_bswap16((uint16_t)(Rr | (Rr >> 16)));
}
// 画面の色に光を足す(色ごとに頭打ち)
static inline uint16_t addPix(uint16_t dst, uint16_t add) {
  uint32_t d = __builtin_bswap16(dst);
  uint32_t r = (d & 0xF800) + (add & 0xF800); if (r > 0xF800) r = 0xF800;
  uint32_t g = (d & 0x07E0) + (add & 0x07E0); if (g > 0x07E0) g = 0x07E0;
  uint32_t b = (d & 0x001F) + (add & 0x001F); if (b > 0x001F) b = 0x001F;
  return __builtin_bswap16((uint16_t)(r | g | b));
}
static inline float frand() { return (esp_random() & 0xFFFF) / 65535.0f; }
static inline float frand(float a, float b) { return a + (b - a) * frand(); }
// 形を決めるための乱数(毎回同じ形になるように、種を固定)
static uint32_t rs = 7;
static inline float srand01() { rs ^= rs << 13; rs ^= rs >> 17; rs ^= rs << 5; return (rs & 0xFFFFFF) / 16777216.0f; }
static inline float PX(float u) { return Sc + u * H2; }   // 正規化座標(-1..1) → ピクセル

// ---------------------------------------------------------------- 色の組み合わせ
struct Theme {
  const char* name;
  Col warm, mid, cool;              // 箱の色: 胸のまわり / 中ほど / 外側
  Col bg0, bg1, grid;               // 背景: いちばん暗い色 / まん中の明るみ / 格子
  Col ring, dotA, dotB;             // 光輪の円 / 舟形光背の点線(内・外)
  Col petalF, petalB, base;         // 蓮(手前・奥)/ 台座の線
  Col text;                         // 外周の文字
};
static const Theme THEMES[] = {
  {"kinsou (gold & blue)", C8(255,214,140), C8(120,200,255), C8(150,120,255),
   C8(6,7,16), C8(10,12,30), C8(14,18,34), C8(60,70,110), C8(70,90,150), C8(40,60,110),
   C8(255,150,190), C8(255,195,150), C8(200,160,110), C8(150,175,230)},
  {"hisui (jade)", C8(235,255,200), C8(90,230,190), C8(60,140,220),
   C8(4,10,10), C8(8,22,20), C8(12,26,26), C8(50,95,90), C8(60,130,120), C8(35,80,80),
   C8(200,240,170), C8(150,230,200), C8(150,200,150), C8(140,220,200)},
  {"guren (crimson)", C8(255,220,170), C8(255,120,150), C8(190,70,200),
   C8(12,5,9), C8(26,8,16), C8(30,14,22), C8(110,55,75), C8(150,70,100), C8(100,40,80),
   C8(255,120,150), C8(255,190,140), C8(220,140,110), C8(240,160,180)},
  {"hakugin (silver)", C8(255,250,235), C8(190,205,230), C8(140,150,200),
   C8(7,7,9), C8(16,16,20), C8(20,20,26), C8(80,80,95), C8(110,110,130), C8(65,65,80),
   C8(230,225,240), C8(240,235,220), C8(190,185,170), C8(200,205,220)},
};
static constexpr int NTHEME = sizeof(THEMES) / sizeof(THEMES[0]);

// 胸(心)に近いほど warm、遠いほど mid → cool
static Col boxCol(float u, float v, float k) {
  const Theme& th = THEMES[theme];
  const float vv = (v + 0.12f) * 0.85f;
  const float t = fminf(1.0f, sqrtf(u * u + vv * vv) / 0.55f);
  Col c = t < 0.6f ? lerpC(th.warm, th.mid, t / 0.6f) : lerpC(th.mid, th.cool, (t - 0.6f) / 0.4f);
  return mulC(c, k);
}

// ---------------------------------------------------------------- 菩薩の形
// 符号付き距離(負が内側)。頭・髻・首・胴・肩・腕・耳・定印の手・組んだ脚
static inline float sdEll(float u, float v, float cu, float cv, float rx, float ry) {
  const float a = (u - cu) / rx, b = (v - cv) / ry;
  return (sqrtf(a * a + b * b) - 1.0f) * fminf(rx, ry);
}
static inline float sdCap(float u, float v, float au, float av, float bu, float bv, float r) {
  const float pu = u - au, pv = v - av, du = bu - au, dv = bv - av;
  const float h = clamp01((pu * du + pv * dv) / (du * du + dv * dv));
  const float ex = pu - du * h, ey = pv - dv * h;
  return sqrtf(ex * ex + ey * ey) - r;
}
static float figureSd(float u, float v) {
  float d = sdEll(u, v, 0, -0.43f, 0.105f, 0.128f);
  d = fminf(d, sdEll(u, v, 0, -0.575f, 0.052f, 0.045f));
  d = fminf(d, sdCap(u, v, 0, -0.33f, 0, -0.25f, 0.045f));
  d = fminf(d, sdEll(u, v, 0, -0.11f, 0.215f, 0.19f));
  d = fminf(d, sdCap(u, v, -0.17f, -0.20f, 0.17f, -0.20f, 0.07f));
  for (int s = -1; s <= 1; s += 2) {
    d = fminf(d, sdCap(u, v, 0.19f * s, -0.19f, 0.29f * s, 0.03f, 0.055f));
    d = fminf(d, sdCap(u, v, 0.29f * s, 0.03f, 0.07f * s, 0.10f, 0.047f));
    d = fminf(d, sdEll(u, v, 0.075f * s, -0.47f, 0.022f, 0.05f));
  }
  d = fminf(d, sdEll(u, v, 0, 0.105f, 0.11f, 0.045f));
  d = fminf(d, sdEll(u, v, 0, 0.19f, 0.43f, 0.105f));
  return d;
}
static float coverage(float u0, float v0, float u1, float v1) {
  int n = (int)((u1 - u0) / 0.0029f); n = n < 4 ? 4 : (n > 22 ? 22 : n);
  int m = (int)((v1 - v0) / 0.0029f); m = m < 4 ? 4 : (m > 22 ? 22 : m);
  int in = 0;
  for (int j = 0; j < m; ++j)
    for (int i = 0; i < n; ++i)
      if (figureSd(u0 + (u1 - u0) * (i + 0.5f) / n, v0 + (v1 - v0) * (j + 0.5f) / m) < 0) ++in;
  return in / (float)(n * m);
}

// ---------------------------------------------------------------- 四分木(入れ子の箱)
struct Node {
  float u0, v0, u1, v1, cu, cv;
  int16_t parent, child[4];
  uint8_t nch, depth, leaf, canMerge, canSplit;
  float thM, thS;                  // まとまる / 分かれる ときの、波の強さのしきい値
  uint16_t hash;
};
static Node* nodes;
static constexpr size_t nmOa30 = nmAlignTo(nmOv30, alignof(int)), nmOv31 = nmOa30 + sizeof(int); static int& nNode = *reinterpret_cast<int*>(NM_ARENA + nmOa30);
static float* nodeDis;               // ほどけ具合 0..1
static float* nodeDir;               // ほどけて飛ぶ向き
static float* nodePush;              // 最後にほどけ始めた時刻
static float* nodePend;              // どこまで散るか(0..1。指の真下ほど 1)
static float* nodeDis0;              // ほどけ始めたときの、ほどけ具合
static float* nodeRetB;              // 戻り始めるまでの待ち時間のばらつき(0..1)

static int split(float u0, float v0, float u1, float v1, int depth, int parent) {
  const float cov = coverage(u0, v0, u1, v1);
  if (cov < 0.03f) return -1;
  const float size = u1 - u0;
  const bool stop = (size < 0.022f && cov > 0.45f) || (cov > 0.97f && size < 0.16f && srand01() < 0.28f + depth * 0.05f);
  if (!stop && size < 0.022f) return -1;
  if (nNode >= MAXNODE) return -1;
  const int n = nNode++;
  Node& N = nodes[n];
  N.u0 = u0; N.v0 = v0; N.u1 = u1; N.v1 = v1; N.cu = (u0 + u1) * 0.5f; N.cv = (v0 + v1) * 0.5f;
  N.parent = (int16_t)parent; N.nch = 0; N.depth = (uint8_t)depth; N.leaf = stop ? 1 : 0;
  const float sz = fmaxf(u1 - u0, v1 - v0);
  N.canMerge = (!stop && cov > 0.9f && sz < 0.2f) ? 1 : 0;
  N.canSplit = (stop && sz > 0.028f) ? 1 : 0;
  N.thM = 0.30f + 2.2f * sz;
  N.thS = 0.40f + 0.004f / sz;
  N.hash = (uint16_t)(n * 40503u >> 3);
  if (stop) return n;
  const float mu = N.cu, mv = N.cv;
  int kids[4], nk = 0;
  if (srand01() < 0.18f && depth > 1) {                        // たまに2つにだけ分ける(形を不揃いに)
    if (srand01() < 0.5f) { kids[nk++] = split(u0, v0, mu, v1, depth + 1, n); kids[nk++] = split(mu, v0, u1, v1, depth + 1, n); }
    else                  { kids[nk++] = split(u0, v0, u1, mv, depth + 1, n); kids[nk++] = split(u0, mv, u1, v1, depth + 1, n); }
  } else {
    kids[nk++] = split(u0, v0, mu, mv, depth + 1, n); kids[nk++] = split(mu, v0, u1, mv, depth + 1, n);
    kids[nk++] = split(u0, mv, mu, v1, depth + 1, n); kids[nk++] = split(mu, mv, u1, v1, depth + 1, n);
  }
  Node& M = nodes[n];                                           // (配列は動かないが、読みやすさのため)
  for (int i = 0; i < nk; ++i) if (kids[i] >= 0) M.child[M.nch++] = (int16_t)kids[i];
  return n;
}

// ---------------------------------------------------------------- 毎フレーム描く物の一覧
enum { ST_CLASS = 0, ST_HALO = 1, ST_SOLID = 2 };
struct DBox { int16_t x0, y0, x1, y1, hb; uint16_t cO, cF, cH, cL; uint8_t style, nr, seed; };
static DBox*    box;
static constexpr size_t nmOa31 = nmAlignTo(nmOv31, alignof(int)), nmOv32 = nmOa31 + sizeof(int); static int& nBox = *reinterpret_cast<int*>(NM_ARENA + nmOa31);
static int16_t* ent;                  // 帯ごとに並べた箱の番号
static constexpr size_t nmOa32 = nmAlignTo(nmOv32, 16), nmOv33 = nmOa32 + sizeof(int16_t[MAXNB + 1]); static int16_t (&entStart)[MAXNB + 1] = *reinterpret_cast<int16_t(*)[MAXNB + 1]>(NM_ARENA + nmOa32);

struct PDraw { int16_t x, y; uint16_t c, ch; };
static constexpr size_t nmOa33 = nmAlignTo(nmOv33, 16), nmOv34 = nmOa33 + sizeof(PDraw[MAXPD]); static PDraw (&pd)[MAXPD] = *reinterpret_cast<PDraw(*)[MAXPD]>(NM_ARENA + nmOa33);
static constexpr size_t nmOa34 = nmAlignTo(nmOv34, alignof(int)), nmOv35 = nmOa34 + sizeof(int); static int& nPD = *reinterpret_cast<int*>(NM_ARENA + nmOa34);
static constexpr size_t nmOa35 = nmAlignTo(nmOv35, 16), nmOv36 = nmOa35 + sizeof(int16_t[MAXPD]); static int16_t (&pdOrd)[MAXPD] = *reinterpret_cast<int16_t(*)[MAXPD]>(NM_ARENA + nmOa35);
static constexpr size_t nmOa36 = nmAlignTo(nmOv36, 16), nmOv37 = nmOa36 + sizeof(int16_t[MAXNB + 1]); static int16_t (&pdStart)[MAXNB + 1] = *reinterpret_cast<int16_t(*)[MAXNB + 1]>(NM_ARENA + nmOa36);

// 箱を1つ足す(u,v は正規化座標)。k は明るさ
static void addBox(float u0, float v0, float u1, float v1, Col c, int style, uint8_t seed) {
  if (nBox >= MAXBOX) return;
  float x0 = PX(u0), x1 = PX(u1), y0 = PX(v0), y1 = PX(v1);
  if (style == ST_CLASS) {
    const float g = fmaxf(0.5f, (x1 - x0) * 0.08f);
    x0 += g; y0 += g; x1 -= g; y1 -= g;
  }
  DBox& b = box[nBox];
  b.x0 = (int16_t)lrintf(x0); b.x1 = (int16_t)lrintf(x1) - 1;
  b.y0 = (int16_t)lrintf(y0); b.y1 = (int16_t)lrintf(y1) - 1;
  if (b.x1 < b.x0 || b.y1 < b.y0) return;
  if (b.x1 < 0 || b.y1 < 0 || b.x0 >= S || b.y0 >= S) return;
  const int h = b.y1 - b.y0 + 1, w = b.x1 - b.x0 + 1;
  b.style = (uint8_t)style; b.seed = seed;
  if (style == ST_SOLID) { b.cF = to565(c); b.hb = b.y0; b.nr = 0; ++nBox; return; }
  const float oK = (style == ST_CLASS && w <= 6) ? 0.6f : 1.0f;
  b.cO = to565(mulC(c, oK));
  b.cF = to565(mulC(c, 0.16f));
  b.cH = to565(mulC(c, style == ST_HALO ? 0.60f : 0.55f));
  b.cL = to565(mulC(c, 0.45f));
  if (style == ST_HALO) { b.hb = (int16_t)(b.y0 + (h - 1) * 0.325f); b.nr = 0; }
  else if (h > 7) { b.hb = (int16_t)(b.y0 + h * 0.24f); const int rows = (int)((b.y1 - b.hb) / 3.2f); b.nr = (uint8_t)min(rows, 6); }
  else { b.hb = b.y0 - 1; b.nr = 0; }
  ++nBox;
}

// 呼吸の波: 胸から外へ広がる輪 + ゆっくりうねる成分(-1..1)
static constexpr size_t nmOa37 = nmAlignTo(nmOv37, alignof(float)), nmOv38 = nmOa37 + sizeof(float); static float& breathT = *reinterpret_cast<float*>(NM_ARENA + nmOa37);
static constexpr size_t nmOa38 = nmAlignTo(nmOv38, alignof(bool)), nmOv39 = nmOa38 + sizeof(bool); static bool& restMode = *reinterpret_cast<bool*>(NM_ARENA + nmOa38);  // 止まった状態(動かない絵を作るとき)
static inline float breathAt(float u, float v) {
  const float d = sqrtf(u * u + (v + 0.12f) * (v + 0.12f));
  return 0.62f * fsin(breathT - 7.0f * d) + 0.38f * fsin(0.53f * breathT + 3.1f * u - 2.3f * v + 1.0f);
}

// 1つの箱(ほどけ具合に応じて、飛ばして・小さくして・明るくする)
static void emitClass(int n, float u0, float v0, float u1, float v1, float k, uint8_t seed) {
  const float dis = nodeDis[n];
  const float cu = (u0 + u1) * 0.5f, cv = (v0 + v1) * 0.5f;
  const float pulse = restMode ? 1.0f : 0.86f + 0.20f * fsin(2.0f * simT - 9.0f * sqrtf(cu * cu + (cv + 0.12f) * (cv + 0.12f)));
  // はじけた瞬間は、まぶしく光る(0.6秒ほどで引く)
  const float since = simT - nodePush[n];
  const float flash = (since >= 0 && since < 1.5f) ? 1.6f * expf(-since * 4.0f) : 0.0f;
  Col c = boxCol(cu, cv, k * pulse * (1.0f + 0.5f * dis + flash));
  if (dis < 0.004f) { addBox(u0, v0, u1, v1, c, ST_CLASS, seed); return; }
  const float jit = ((seed * 37) & 255) / 255.0f - 0.5f, h2 = ((seed * 91 + n * 7) & 255) / 255.0f;
  // うずを巻きながら外へ飛び、漂っているあいだも、ゆっくり回り続ける
  const float a = nodeDir[n] + jit * 1.0f + dis * 0.9f + dis * 0.4f * fsin(simT * 0.11f + seed * 0.1f);
  const float r = SCATTER * dis * (0.40f + 0.60f * h2);
  float pu = cu + fcos(a) * r + dis * 0.03f * fsin(simT * 0.21f + seed * 0.7f);
  float pv = cv + fsin(a) * r + dis * 0.03f * fcos(simT * 0.17f + seed * 1.3f);
  const float rr = sqrtf(pu * pu + pv * pv);
  if (rr > 0.93f) { pu *= 0.93f / rr; pv *= 0.93f / rr; }        // 画面の外へは出ない(ふちに貼りつく)
  const float sh = 1.0f - 0.35f * dis;                           // 箱の形を残したまま飛ぶ
  const float hu = (u1 - u0) * 0.5f * sh, hv = (v1 - v0) * 0.5f * sh;
  addBox(pu - hu, pv - hv, pu + hu, pv + hv, c, ST_CLASS, seed);
}

// 木をたどって、見せる箱を決める(まとまる・分かれるは、明るさの入れ替えで)
static void emitNode(int n, float k) {
  const Node& N = nodes[n];
  if (N.leaf) {
    // ほどけている間は、呼吸(まとまる・分かれる)を止める
    const float s = (N.canSplit && !restMode) ? smooth3((-breathAt(N.cu, N.cv) - N.thS) / 0.18f) * (1.0f - nodeDis[n]) : 0.0f;
    if (s < 0.99f) emitClass(n, N.u0, N.v0, N.u1, N.v1, k * (1.0f - s), (uint8_t)N.hash);
    if (s > 0.01f) {
      emitClass(n, N.u0, N.v0, N.cu, N.cv, k * s, (uint8_t)(N.hash + 1));
      emitClass(n, N.cu, N.v0, N.u1, N.cv, k * s, (uint8_t)(N.hash + 2));
      emitClass(n, N.u0, N.cv, N.cu, N.v1, k * s, (uint8_t)(N.hash + 3));
      emitClass(n, N.cu, N.cv, N.u1, N.v1, k * s, (uint8_t)(N.hash + 4));
    }
    return;
  }
  if (N.nch == 0) return;
  const float m = (N.canMerge && !restMode) ? smooth3((breathAt(N.cu, N.cv) - N.thM) / 0.18f) * (1.0f - nodeDis[n]) : 0.0f;
  if (m > 0.01f) emitClass(n, N.u0, N.v0, N.u1, N.v1, k * m, (uint8_t)N.hash);
  if (m < 0.99f) for (int i = 0; i < N.nch; ++i) emitNode(N.child[i], k * (1.0f - m));
}

// 光輪と舟形光背の箱(光が輪をめぐる)
static const float HALO_R[3] = {0.20f, 0.265f, 0.325f}, HALO_SZ[3] = {0.018f, 0.014f, 0.010f}, HALO_K[3] = {0.9f, 0.75f, 0.55f};
static const int   HALO_N[3] = {24, 36, 60};
static constexpr float HCU = 0.0f, HCV = -0.43f;
struct HaloB { float u, v, s, a; int8_t ring, j; };   // 光輪(ring 0〜2)と舟形光背(j 0〜1)の箱。場所は起動時に決めておく
static HaloB* haloB = nullptr;
static constexpr size_t nmOa39 = nmAlignTo(nmOv39, alignof(int)), nmOv40 = nmOa39 + sizeof(int); static int& nHaloB = *reinterpret_cast<int*>(NM_ARENA + nmOa39);
static void buildHalo() {
  haloB = (HaloB*)malloc(sizeof(HaloB) * 300);
  nHaloB = 0;
  if (!haloB) return;
  for (int ring = 0; ring < 3; ++ring)
    for (int i = 0; i < HALO_N[ring]; ++i) {
      const float a = 6.2832f * i / HALO_N[ring] + ring * 0.13f;
      const float u = HCU + HALO_R[ring] * cosf(a), v = HCV + HALO_R[ring] * sinf(a);
      if (figureSd(u, v) < 0.01f) continue;                     // 体の後ろに隠れる箱は描かない
      haloB[nHaloB++] = {u, v, HALO_SZ[ring], a, (int8_t)ring, -1};
    }
  for (int j = 0; j < 2; ++j) {
    const float rx = j ? 0.64f : 0.56f, ry = j ? 0.68f : 0.60f, sz = j ? 0.008f : 0.011f;
    const int n = j ? 96 : 72;
    for (int i = 0; i < n; ++i) {
      const float a = 6.2832f * i / n;
      const float u = rx * sinf(a), v = -0.10f - ry * cosf(a);
      if (v > 0.30f) continue;
      haloB[nHaloB++] = {u, v, sz, a, -1, (int8_t)j};
    }
  }
}
static void emitHalo() {
  const Col hc = boxCol(0, -0.12f, 1.0f);
  for (int i = 0; i < nHaloB; ++i) {
    const HaloB& h = haloB[i];
    if (h.ring >= 0) {
      const float dir = (h.ring & 1) ? -1.0f : 1.0f;
      const float q = 0.5f + 0.5f * fsin(h.a * 3.0f - dir * simT * 0.9f);
      const float tw = 0.70f + 0.55f * q * q * q;
      addBox(h.u - h.s, h.v - h.s, h.u + h.s, h.v + h.s, mulC(hc, HALO_K[h.ring] * tw), ST_HALO, 0);
    } else {
      const float q = 0.5f + 0.5f * fsin(h.a * 2.0f - simT * (h.j ? 0.5f : -0.7f)), q2 = q * q;
      const float tw = 0.65f + 0.6f * q2 * q2;
      addBox(h.u - h.s, h.v - h.s, h.u + h.s, h.v + h.s, boxCol(h.u, h.v, (h.j ? 0.55f : 0.8f) * tw), ST_SOLID, 0);
    }
  }
}

// ---------------------------------------------------------------- メッセージ(継承の線を流れる光)
struct Pulse { int16_t path[12]; int8_t len; float pos, speed, wait; };
static constexpr size_t nmOa40 = nmAlignTo(nmOv40, 16), nmOv41 = nmOa40 + sizeof(Pulse[NPULSE]); static Pulse (&pulse)[NPULSE] = *reinterpret_cast<Pulse(*)[NPULSE]>(NM_ARENA + nmOa40);
static int16_t* leafList;
static constexpr size_t nmOa41 = nmAlignTo(nmOv41, alignof(int)), nmOv42 = nmOa41 + sizeof(int); static int& nLeaf = *reinterpret_cast<int*>(NM_ARENA + nmOa41);
static void newPulse(Pulse& p) {
  int n = leafList[esp_random() % nLeaf];
  int tmp[16], m = 0;
  while (n >= 0 && m < 12) { tmp[m++] = n; n = nodes[n].parent; }
  p.len = (int8_t)m;
  for (int i = 0; i < m; ++i) p.path[i] = (int16_t)tmp[m - 1 - i];   // 根 → 葉
  p.pos = 0; p.speed = frand(1.6f, 3.0f); p.wait = frand(0.0f, 1.5f);
}

// ---------------------------------------------------------------- 光の粒
struct Part { float x, y, vx, vy, life, maxL; Col c; };
static constexpr size_t nmOa42 = nmAlignTo(nmOv42, 16), nmOv43 = nmOa42 + sizeof(Part[MAXPART]); static Part (&part)[MAXPART] = *reinterpret_cast<Part(*)[MAXPART]>(NM_ARENA + nmOa42);
struct Ring { float x, y, t0, k; };                  // はじけた光の輪
static Ring ring[NRING] = {{0, 0, -100, 0}, {0, 0, -100, 0}, {0, 0, -100, 0}, {0, 0, -100, 0}};
static constexpr size_t nmOa43 = nmAlignTo(nmOv43, alignof(int)), nmOv44 = nmOa43 + sizeof(int); static int& ringNext = *reinterpret_cast<int*>(NM_ARENA + nmOa43);
static constexpr size_t nmOa44 = nmAlignTo(nmOv44, alignof(int)), nmOv45 = nmOa44 + sizeof(int); static int& partNext = *reinterpret_cast<int*>(NM_ARENA + nmOa44);
static void emitPart(float x, float y, float vx, float vy, Col c, float life) {
  Part& p = part[partNext]; partNext = (partNext + 1) % MAXPART;
  p = {x, y, vx, vy, life, life, c};
}

// ---------------------------------------------------------------- 外周の文字
static const uint8_t FONT[] = {
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x56,0x00,0x00,0x78,0x00,
  0x00,0x78,0x00,0x00,0x78,0x00,0x00,0x67,0x00,0x00,0x12,0x00,0x00,0x56,0x00,0x00,0x44,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x04,0x65,0x40,0x05,0x87,0x60,0x05,0x87,0x60,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x63,0x63,0x00,0xB1,0xB0,
  0x5B,0xEB,0xEB,0x04,0x84,0x80,0x49,0x79,0x72,0x7D,0x8D,0x83,0x0B,0x1B,0x00,0x15,0x14,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x01,0x00,0x00,0x26,0x00,0x07,0xCD,0xB0,0x1D,0x26,0x00,0x0C,0x98,0x10,0x00,0x6B,0xD2,0x00,0x26,0x86,
  0x1A,0x69,0xC3,0x03,0x8A,0x30,0x00,0x26,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x17,0x40,0x00,0xA5,0xA2,0x00,
  0xA5,0xA2,0x13,0x17,0x57,0x82,0x28,0x75,0x71,0x21,0x2A,0x5A,0x00,0x2A,0x5A,0x00,0x04,0x71,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x06,0xCC,0x30,0x0C,0x10,0x10,0x0A,0x50,0x00,0x1B,0xC2,0x03,0x94,0x4B,0x0B,0xA3,0x07,0xA9,
  0x5C,0x56,0xE5,0x03,0x76,0x26,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x55,0x00,0x00,0x66,0x00,
  0x00,0x66,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0B,0x10,0x00,0x49,0x00,0x00,0x94,0x00,0x00,0xD2,0x00,0x00,0xE1,0x00,0x00,0xD2,0x00,
  0x00,0x94,0x00,0x00,0x49,0x00,0x00,0x0B,0x10,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01,0xB0,0x00,0x00,0x94,0x00,
  0x00,0x4A,0x00,0x00,0x1D,0x00,0x00,0x0E,0x00,0x00,0x1D,0x00,0x00,0x4A,0x00,0x00,0x94,0x00,0x01,0xB0,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x44,0x10,0x18,0x99,0x91,0x02,0xBC,0x30,0x17,0x45,0x71,0x00,0x22,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x22,0x00,
  0x00,0x66,0x00,0x00,0x66,0x00,0x6B,0xDD,0xB7,0x00,0x66,0x00,0x00,0x66,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x8A,0x00,0x00,0xA7,0x00,0x00,0xC1,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x02,0x88,0x20,0x01,0x44,0x10,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x99,0x00,0x00,0x55,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x91,0x00,0x04,0xA0,
  0x00,0x0B,0x30,0x00,0x3B,0x00,0x00,0xA4,0x00,0x02,0xC0,0x00,0x09,0x50,0x00,0x1D,0x00,0x00,0x34,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x05,0xCC,0x50,0x1D,0x11,0xD1,0x4B,0x00,0xB4,0x5A,0x66,0x95,0x5A,0x11,0xA5,0x2C,0x00,0xC3,
  0x0B,0x77,0xB0,0x01,0x66,0x10,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x07,0xA9,0x00,0x02,0x3C,0x00,
  0x00,0x2C,0x00,0x00,0x2C,0x00,0x00,0x2C,0x00,0x00,0x2C,0x00,0x03,0x5D,0x41,0x05,0x88,0x83,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x2B,0xBC,0x50,0x12,0x01,0xE1,0x00,0x00,0xE1,0x00,0x07,0xA0,0x00,0x4C,0x10,0x03,0xC2,0x00,
  0x2D,0x64,0x41,0x28,0x88,0x81,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x1C,0xBC,0x60,0x01,0x01,0xE1,
  0x00,0x01,0xE1,0x01,0xBD,0x60,0x00,0x03,0xD1,0x00,0x00,0xB4,0x36,0x47,0xD1,0x16,0x76,0x10,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x09,0x60,0x00,0x6C,0x90,0x01,0xB6,0x90,0x0A,0x36,0x90,0x49,0x06,0x90,0x6B,0xBD,0xD6,
  0x00,0x06,0x90,0x00,0x03,0x40,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0B,0xBB,0x80,0x0E,0x00,0x00,
  0x0E,0x32,0x00,0x09,0x8B,0x90,0x00,0x00,0xD2,0x00,0x00,0xC3,0x36,0x48,0xC0,0x16,0x76,0x10,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x03,0xCB,0xA0,0x0D,0x20,0x10,0x4A,0x57,0x20,0x5E,0x75,0xD1,0x5C,0x00,0xA5,0x3C,0x00,0xA5,
  0x0C,0x75,0xD1,0x01,0x67,0x20,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x4B,0xBB,0xB3,0x00,0x01,0xD0,
  0x00,0x06,0x90,0x00,0x0C,0x30,0x00,0x3C,0x00,0x00,0x96,0x00,0x01,0xE1,0x00,0x02,0x60,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x08,0xCC,0x80,0x2D,0x00,0xD2,0x1D,0x00,0xD2,0x06,0xDD,0x70,0x2C,0x22,0xC2,0x5A,0x00,0xA5,
  0x2D,0x55,0xD2,0x02,0x77,0x20,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x08,0xCC,0x50,0x3C,0x01,0xD1,
  0x59,0x00,0xB4,0x3C,0x01,0xE5,0x08,0xCB,0xB4,0x00,0x00,0xC2,0x06,0x48,0x90,0x05,0x75,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x77,0x00,0x00,0x77,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x99,0x00,0x00,0x55,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x77,0x00,0x00,0x77,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x8A,0x00,0x00,0xA7,0x00,0x00,0xC1,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x02,0x88,0x05,0xBB,0x50,0x8D,0x30,0x00,0x05,0xBB,0x50,
  0x00,0x02,0x88,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x6B,0xBB,0xB7,0x00,0x00,0x00,0x6B,0xBB,0xB7,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x79,0x20,0x00,0x05,0xBB,0x50,0x00,0x03,0xD9,0x05,0xBB,0x50,
  0x79,0x20,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x08,0xCC,0x70,0x03,0x01,0xE1,
  0x00,0x04,0xC0,0x00,0x3C,0x20,0x00,0x86,0x00,0x00,0x43,0x00,0x00,0x74,0x00,0x00,0x53,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x23,0x10,0x08,0xB8,0xC3,0x58,0x03,0x3A,0xA1,0x99,0xAB,0xB0,0xC0,0x0B,0xB0,0xC2,0x3B,
  0x93,0x3A,0x88,0x2B,0x10,0x00,0x03,0xBB,0xC0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x88,0x00,0x00,0xDD,0x10,
  0x04,0x99,0x50,0x09,0x55,0x90,0x0D,0x54,0xE0,0x3C,0x88,0xC3,0x87,0x00,0x78,0x62,0x00,0x26,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x2B,0xBB,0x50,0x3C,0x01,0xD3,0x3C,0x00,0xC3,0x3E,0xBD,0xA0,0x3C,0x01,0xB4,0x3C,0x00,0x78,
  0x3D,0x45,0xC4,0x18,0x87,0x30,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x02,0xBB,0xC2,0x0C,0x40,0x11,
  0x3C,0x00,0x00,0x5B,0x00,0x00,0x4B,0x00,0x00,0x1E,0x10,0x00,0x08,0xA4,0x73,0x00,0x47,0x50,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x4B,0xB8,0x10,0x5A,0x05,0xC0,0x5A,0x00,0xC4,0x5A,0x00,0xA6,0x5A,0x00,0xA5,0x5A,0x00,0xD3,
  0x5B,0x49,0xA0,0x28,0x74,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0B,0xBB,0xB3,0x1E,0x00,0x00,
  0x1E,0x00,0x00,0x1F,0xBB,0xB2,0x1E,0x00,0x00,0x1E,0x00,0x00,0x1E,0x44,0x41,0x08,0x88,0x83,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x0A,0xBB,0xB5,0x0D,0x20,0x00,0x0D,0x20,0x00,0x0D,0xCB,0xB1,0x0D,0x20,0x00,0x0D,0x20,0x00,
  0x0D,0x20,0x00,0x06,0x10,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x04,0xCB,0xB1,0x1D,0x20,0x11,
  0x5A,0x00,0x00,0x78,0x02,0x41,0x79,0x05,0xC6,0x4C,0x00,0x86,0x0A,0x84,0xB5,0x00,0x57,0x40,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x47,0x00,0x74,0x5A,0x00,0xA5,0x5A,0x00,0xA5,0x5E,0xBB,0xE5,0x5A,0x00,0xA5,0x5A,0x00,0xA5,
  0x5A,0x00,0xA5,0x25,0x00,0x53,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0B,0xBB,0xB0,0x00,0x78,0x00,
  0x00,0x78,0x00,0x00,0x78,0x00,0x00,0x78,0x00,0x00,0x78,0x00,0x04,0x99,0x40,0x08,0x88,0x80,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x02,0xBB,0x80,0x00,0x05,0xA0,0x00,0x05,0xA0,0x00,0x05,0xA0,0x00,0x05,0xA0,0x00,0x05,0x90,
  0x78,0x4B,0x50,0x15,0x75,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x47,0x00,0x76,0x5A,0x07,0xA0,
  0x5A,0x6B,0x00,0x5E,0xF4,0x00,0x5C,0x4D,0x10,0x5A,0x08,0x90,0x5A,0x00,0xD5,0x25,0x00,0x36,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x0B,0x00,0x00,0x0E,0x10,0x00,0x0E,0x10,0x00,0x0E,0x10,0x00,0x0E,0x10,0x00,0x0E,0x10,0x00,
  0x0E,0x44,0x42,0x07,0x88,0x84,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x7A,0x00,0xA7,0x9D,0x22,0xD9,
  0x98,0x77,0x89,0x95,0xAB,0x59,0x95,0x88,0x59,0x95,0x00,0x59,0x95,0x00,0x59,0x42,0x00,0x24,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x4B,0x10,0x74,0x5E,0x70,0x95,0x5A,0xC0,0x95,0x5A,0x84,0x95,0x5A,0x2A,0x95,0x5A,0x0B,0xB5,
  0x5A,0x05,0xF5,0x25,0x00,0x73,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x06,0xCC,0x60,0x2D,0x11,0xD2,
  0x5A,0x00,0xA5,0x69,0x00,0x97,0x69,0x00,0x96,0x4B,0x00,0xB4,0x0C,0x66,0xC0,0x01,0x66,0x10,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x0B,0xBB,0x70,0x1E,0x01,0xB6,0x1E,0x00,0x78,0x1E,0x45,0xD4,0x1F,0x87,0x30,0x1E,0x00,0x00,
  0x1E,0x00,0x00,0x07,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x06,0xCC,0x60,0x2D,0x11,0xD2,
  0x5A,0x00,0xA5,0x69,0x00,0x97,0x69,0x00,0x96,0x4B,0x00,0xB4,0x0C,0x66,0xD0,0x01,0x6C,0x60,0x00,0x01,0x80,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x3B,0xBA,0x40,0x5A,0x03,0xE1,0x5A,0x00,0xD3,0x5C,0x46,0xC0,0x5D,0x8C,0x50,0x5A,0x01,0xD1,
  0x5A,0x00,0x88,0x25,0x00,0x17,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x07,0xCB,0xA0,0x3C,0x00,0x30,
  0x4B,0x00,0x00,0x0A,0xDA,0x40,0x00,0x15,0xD2,0x00,0x00,0x95,0x39,0x45,0xD2,0x04,0x77,0x20,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x9B,0xBB,0xB9,0x00,0x78,0x00,0x00,0x78,0x00,0x00,0x78,0x00,0x00,0x78,0x00,0x00,0x78,0x00,
  0x00,0x78,0x00,0x00,0x44,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x38,0x00,0x83,0x4B,0x00,0xA4,
  0x4B,0x00,0xA4,0x4B,0x00,0xA4,0x4B,0x00,0xA4,0x4B,0x00,0xB4,0x1D,0x55,0xD1,0x02,0x77,0x20,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x75,0x00,0x47,0x59,0x00,0x96,0x1D,0x00,0xD1,0x0B,0x22,0xC0,0x07,0x66,0x70,0x03,0xAA,0x30,
  0x00,0xCC,0x00,0x00,0x55,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xB0,0x00,0x0B,0xC2,0x00,0x1C,
  0xA3,0x77,0x3A,0x85,0xBB,0x58,0x57,0xAA,0x76,0x3C,0x77,0xC3,0x1F,0x43,0xF1,0x07,0x10,0x70,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x48,0x00,0x66,0x0C,0x31,0xD1,0x03,0xCA,0x50,0x00,0xAB,0x00,0x01,0xDC,0x20,0x0A,0x64,0xB0,
  0x4B,0x00,0xB5,0x62,0x00,0x26,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x75,0x00,0x57,0x2D,0x10,0xD2,
  0x08,0x77,0x80,0x01,0xCC,0x10,0x00,0x88,0x00,0x00,0x78,0x00,0x00,0x78,0x00,0x00,0x44,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x1B,0xBB,0xB7,0x00,0x00,0xC4,0x00,0x07,0x90,0x00,0x3D,0x10,0x00,0xC4,0x00,0x07,0x90,0x00,
  0x2E,0x54,0x43,0x28,0x88,0x85,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xBC,0x40,0x00,0xB2,0x00,
  0x00,0xB2,0x00,0x00,0xB2,0x00,0x00,0xB2,0x00,0x00,0xB2,0x00,0x00,0xB2,0x00,0x00,0xB2,0x00,0x00,0xBC,0x40,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x47,0x00,0x00,0x0D,0x10,0x00,0x07,0x70,0x00,0x01,0xD0,0x00,0x00,0x86,0x00,0x00,0x1C,0x00,
  0x00,0x09,0x50,0x00,0x02,0xC0,0x00,0x00,0x61,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x03,0xCB,0x00,0x00,0x2B,0x00,
  0x00,0x2B,0x00,0x00,0x2B,0x00,0x00,0x2B,0x00,0x00,0x2B,0x00,0x00,0x2B,0x00,0x00,0x2B,0x00,0x03,0xCB,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x88,0x00,0x07,0x99,0x70,0x4A,0x00,0xA4,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xBB,0xBB,0xBB,
  0x00,0x00,0x00,0x03,0x40,0x00,0x00,0xA2,0x00,0x00,0x12,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x33,0x00,
  0x0B,0x89,0xA0,0x00,0x00,0xC2,0x0A,0xCB,0xE3,0x5A,0x00,0xC3,0x3C,0x48,0xE3,0x04,0x74,0x51,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x1D,0x00,0x00,0x1D,0x13,0x00,0x1E,0xA9,0xC0,0x1E,0x10,0xA4,0x1D,0x00,0x86,0x1E,0x00,0x95,
  0x1E,0x85,0xD1,0x06,0x47,0x20,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x13,0x20,
  0x04,0xC8,0xA3,0x0D,0x20,0x00,0x0E,0x00,0x00,0x0E,0x10,0x00,0x07,0xA4,0x62,0x00,0x47,0x61,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xC1,0x00,0x31,0xC1,0x0B,0x9A,0xE1,0x4B,0x00,0xE1,0x68,0x00,0xC1,0x5A,0x00,0xE1,
  0x1D,0x58,0xE1,0x02,0x74,0x61,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x23,0x00,
  0x09,0xA8,0xB0,0x3B,0x00,0x85,0x6D,0xBB,0xB5,0x4A,0x00,0x00,0x0C,0x74,0x72,0x01,0x67,0x61,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x3C,0xB2,0x00,0x85,0x00,0x1B,0xDC,0xB2,0x00,0x95,0x00,0x00,0x95,0x00,0x00,0x95,0x00,
  0x00,0x95,0x00,0x00,0x42,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x31,0x00,
  0x0B,0x9A,0xE1,0x4B,0x00,0xE1,0x68,0x00,0xC1,0x5A,0x00,0xE1,0x1D,0x57,0xE1,0x02,0x74,0xD1,0x07,0x46,0xB0,0x04,0x76,0x10,
  0x00,0x00,0x00,0x00,0x00,0x00,0x1D,0x00,0x00,0x1D,0x13,0x10,0x1E,0x99,0xC0,0x1E,0x00,0xC1,0x1D,0x00,0xC2,0x1D,0x00,0xC2,
  0x1D,0x00,0xC2,0x06,0x00,0x61,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x58,0x00,0x00,0x12,0x00,
  0x08,0xD8,0x00,0x00,0x58,0x00,0x00,0x58,0x00,0x00,0x58,0x00,0x04,0x8A,0x41,0x18,0x88,0x82,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x1C,0x00,0x00,0x03,0x00,0x07,0xCC,0x00,0x00,0x1C,0x00,0x00,0x1C,0x00,0x00,0x1C,0x00,
  0x00,0x1C,0x00,0x00,0x1C,0x00,0x04,0x7A,0x00,0x18,0x71,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0D,0x10,0x00,0x0D,0x10,0x00,
  0x0D,0x13,0xC2,0x0D,0x4C,0x20,0x0D,0xDA,0x00,0x0D,0x2A,0x60,0x0D,0x11,0xD3,0x06,0x10,0x35,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x2B,0xE2,0x00,0x00,0xC2,0x00,0x00,0xC2,0x00,0x00,0xC2,0x00,0x00,0xC2,0x00,0x00,0xC2,0x00,
  0x00,0x98,0x40,0x00,0x16,0x80,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x31,0x30,
  0x7B,0xCB,0xB5,0x76,0x67,0x58,0x75,0x67,0x48,0x75,0x67,0x48,0x75,0x67,0x48,0x43,0x33,0x24,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x13,0x10,0x1E,0x99,0xC0,0x1E,0x00,0xC1,0x1D,0x00,0xC2,0x1D,0x00,0xC2,
  0x1D,0x00,0xC2,0x06,0x00,0x61,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x33,0x00,
  0x0A,0xAA,0xA0,0x3C,0x00,0xC3,0x59,0x00,0x95,0x4B,0x00,0xA4,0x0D,0x66,0xD0,0x01,0x77,0x10,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x13,0x00,0x1E,0xA9,0xB0,0x1E,0x10,0xA4,0x1D,0x00,0x86,0x1E,0x00,0x95,
  0x1E,0x85,0xD1,0x1C,0x47,0x20,0x1C,0x00,0x00,0x16,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x32,0x00,
  0x0A,0x9A,0xD2,0x3C,0x00,0xE2,0x59,0x00,0xB2,0x4B,0x00,0xD2,0x0D,0x67,0xE2,0x02,0x75,0xB2,0x00,0x00,0xB2,0x00,0x00,0x61,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x31,0x03,0xCA,0x87,0x03,0xD0,0x00,0x03,0xA0,0x00,0x03,0xA0,0x00,
  0x03,0xA0,0x00,0x02,0x50,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x33,0x10,
  0x09,0xA8,0x90,0x0E,0x10,0x00,0x06,0xDB,0x50,0x00,0x03,0xE0,0x08,0x46,0xC0,0x05,0x76,0x10,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x70,0x00,0x00,0xD0,0x00,0x4B,0xFB,0xB0,0x00,0xD0,0x00,0x00,0xD0,0x00,0x00,0xD0,0x00,
  0x00,0xC6,0x40,0x00,0x27,0x80,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x1D,0x00,0xC2,0x1D,0x00,0xC2,0x1D,0x00,0xC2,0x1D,0x00,0xC2,0x0D,0x66,0xE2,0x02,0x74,0x61,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x59,0x00,0x95,0x1D,0x00,0xD1,0x09,0x44,0xA0,0x04,0x99,0x40,
  0x00,0xDD,0x00,0x00,0x55,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0xD0,0x00,0x0D,0xA3,0x33,0x3A,0x66,0x99,0x67,0x3A,0x99,0x93,0x0E,0x66,0xE0,0x06,0x21,0x60,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x2C,0x11,0xC2,0x05,0xAA,0x50,0x00,0xAA,0x00,0x03,0xCC,0x30,
  0x1C,0x32,0xC1,0x34,0x00,0x43,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x4A,0x00,0x87,0x0D,0x10,0xD1,0x08,0x64,0xA0,0x02,0xC9,0x40,0x00,0xBD,0x00,0x00,0x77,0x00,0x04,0xD2,0x00,0x17,0x30,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0A,0xBB,0xF1,0x00,0x07,0x80,0x00,0x4B,0x00,0x02,0xC1,0x00,
  0x0C,0x64,0x40,0x08,0x88,0x81,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x2C,0xB0,0x00,0x68,0x00,
  0x00,0x68,0x00,0x00,0x77,0x00,0x0B,0xD2,0x00,0x00,0x96,0x00,0x00,0x67,0x00,0x00,0x68,0x00,0x00,0x3D,0x70,0x00,0x02,0x30,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x66,0x00,0x00,0x66,0x00,0x00,0x66,0x00,0x00,0x66,0x00,0x00,0x66,0x00,0x00,0x66,0x00,
  0x00,0x66,0x00,0x00,0x66,0x00,0x00,0x66,0x00,0x00,0x66,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0A,0xC2,0x00,0x00,0x86,0x00,
  0x00,0x76,0x00,0x00,0x77,0x00,0x00,0x1D,0xB0,0x00,0x69,0x00,0x00,0x76,0x00,0x00,0x76,0x00,0x07,0xD3,0x00,0x03,0x20,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x03,0x30,0x01,0x68,0x9C,0xB6,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
};
static uint8_t*  strip;               // [FCH][stripW] 文字の帯(濃さ 0..15)
static constexpr size_t nmOa45 = nmAlignTo(nmOv45, alignof(int)), nmOv46 = nmOa45 + sizeof(int); static int& stripW = *reinterpret_cast<int*>(NM_ARENA + nmOa45);
static int32_t*  annStart;            // [S+1] 行ごとの、表の始まり
static int16_t*  annX;
static uint16_t* annA;                // 角度(0..65535、上から時計回り)
static uint8_t*  annR;                // 帯の行(0 = 外側)
static constexpr size_t nmOa46 = nmAlignTo(nmOv46, 16), nmOv47 = nmOa46 + sizeof(uint16_t[16]); static uint16_t (&textLut)[16] = *reinterpret_cast<uint16_t(*)[16]>(NM_ARENA + nmOa46);
static constexpr size_t nmOa47 = nmAlignTo(nmOv47, alignof(uint16_t)), nmOv48 = nmOa47 + sizeof(uint16_t); static uint16_t& textRot = *reinterpret_cast<uint16_t*>(NM_ARENA + nmOa47);
static constexpr size_t nmOa48 = nmAlignTo(nmOv48, alignof(float)), nmOv49 = nmOa48 + sizeof(float); static float& textRotF = *reinterpret_cast<float*>(NM_ARENA + nmOa48);

static void buildRimText() {
  const int n = (int)strlen(RIM_TEXT);
  stripW = n * FCW;
  strip = (uint8_t*)ps_malloc((size_t)FCH * stripW);
  for (int i = 0; i < n; ++i) {
    int ch = (unsigned char)RIM_TEXT[i];
    if (ch < 32 || ch > 126) ch = 32;
    const uint8_t* g = FONT + (ch - 32) * (FCW * FCH / 2);
    for (int y = 0; y < FCH; ++y)
      for (int x = 0; x < FCW; ++x) {
        const int k = y * FCW + x;
        const uint8_t b = g[k >> 1];
        strip[y * stripW + i * FCW + x] = (k & 1) ? (b & 15) : (b >> 4);
      }
  }
  // 画面の点ごとに「帯のどこか」を表にする
  const float rc = 0.915f * H2, half = FCH * 0.5f * (S / 466.0f) * 1.05f;
  const float rO = rc + half, rI = rc - half;
  annStart = (int32_t*)malloc((S + 1) * sizeof(int32_t));
  int cnt = 0;
  for (int pass = 0; pass < 2; ++pass) {
    cnt = 0;
    for (int y = 0; y < S; ++y) {
      if (pass) annStart[y] = cnt;
      const float dy = y + 0.5f - (Sc + 0.5f);
      for (int x = 0; x < S; ++x) {
        const float dx = x + 0.5f - (Sc + 0.5f), r = sqrtf(dx * dx + dy * dy);
        if (r < rI || r >= rO) continue;
        if (pass) {
          float a = atan2f(dy, dx) + 1.5707963f;
          if (a < 0) a += 6.2831853f;
          annX[cnt] = (int16_t)x;
          annA[cnt] = (uint16_t)((int32_t)(a / 6.2831853f * 65536.0f) & 0xFFFF);
          int row = (int)((rO - r) / (rO - rI) * FCH); annR[cnt] = (uint8_t)(row > FCH - 1 ? FCH - 1 : row);
        }
        ++cnt;
      }
    }
    if (!pass) {
      annX = (int16_t*)ps_malloc(cnt * 2 + 2); annA = (uint16_t*)ps_malloc(cnt * 2 + 2); annR = (uint8_t*)ps_malloc(cnt + 1);
    }
  }
  annStart[S] = cnt;
}

// ---------------------------------------------------------------- 動かない部分の絵(起動時・色を変えたとき)
// 浮動小数の RGB の絵に描いて、にじみ(ぼかし)を足してから、画面の色に変換します
struct FBuf { float* p; int w; float cx, sc; uint16_t* h; };   // h があれば、浮動小数のかわりに 16ビット(1/HQ きざみ)で持つ
static constexpr float HQ = 8192.0f;
static inline uint16_t hSat(int v) { return (uint16_t)(v < 0 ? 0 : (v > 65535 ? 65535 : v)); }
static inline void fAdd(FBuf& f, int x, int y, Col c, float a) {
  if ((unsigned)x >= (unsigned)f.w || (unsigned)y >= (unsigned)f.w || a <= 0) return;
  if (f.h) {
    uint16_t* q = f.h + ((size_t)y * f.w + x) * 3;
    q[0] = hSat(q[0] + (int)lrintf(c.r * a * HQ)); q[1] = hSat(q[1] + (int)lrintf(c.g * a * HQ)); q[2] = hSat(q[2] + (int)lrintf(c.b * a * HQ));
    return;
  }
  float* q = f.p + ((size_t)y * f.w + x) * 3;
  q[0] += c.r * a; q[1] += c.g * a; q[2] += c.b * a;
}
static inline void fSet(FBuf& f, int x, int y, Col c) {
  if ((unsigned)x >= (unsigned)f.w || (unsigned)y >= (unsigned)f.w) return;
  if (f.h) {
    uint16_t* q = f.h + ((size_t)y * f.w + x) * 3;
    q[0] = hSat((int)lrintf(c.r * HQ)); q[1] = hSat((int)lrintf(c.g * HQ)); q[2] = hSat((int)lrintf(c.b * HQ));
    return;
  }
  float* q = f.p + ((size_t)y * f.w + x) * 3;
  q[0] = c.r; q[1] = c.g; q[2] = c.b;
}
static inline float fX(const FBuf& f, float u) { return f.cx + u * f.sc; }
// 太さのある線(なめらかなふち)。mask を渡すと、足さずに「濃さの最大」を mask に書く(折れ線のつなぎ目が二重にならないように)
struct Mask { float* p; int x0, y0, w, h; };
static void fSeg(FBuf& f, float u0, float v0, float u1, float v1, float wpx, Col c, Mask* mk = nullptr) {
  const float x0 = fX(f, u0), y0 = fX(f, v0), x1 = fX(f, u1), y1 = fX(f, v1), hw = wpx * 0.5f;
  const int ix0 = (int)floorf(fminf(x0, x1) - hw - 1), ix1 = (int)ceilf(fmaxf(x0, x1) + hw + 1);
  const int iy0 = (int)floorf(fminf(y0, y1) - hw - 1), iy1 = (int)ceilf(fmaxf(y0, y1) + hw + 1);
  const float dx = x1 - x0, dy = y1 - y0, L2 = dx * dx + dy * dy + 1e-6f;
  for (int y = max(0, iy0); y <= min(f.w - 1, iy1); ++y)
    for (int x = max(0, ix0); x <= min(f.w - 1, ix1); ++x) {
      const float px = x + 0.5f - x0 - 0.5f, py = y + 0.5f - y0 - 0.5f;
      const float h = clamp01((px * dx + py * dy) / L2);
      const float d = hypotf(px - dx * h, py - dy * h);
      const float a = clamp01(hw + 0.5f - d);
      if (!mk) { fAdd(f, x, y, c, a); continue; }
      const int mx = x - mk->x0, my = y - mk->y0;
      if ((unsigned)mx < (unsigned)mk->w && (unsigned)my < (unsigned)mk->h) { float& q = mk->p[my * mk->w + mx]; if (a > q) q = a; }
    }
}
// 折れ線(閉じた線にも使う)
static void fPolyline(FBuf& f, const float* pu, const float* pv, int n, float wpx, Col c) {
  float mnx = 1e9f, mxx = -1e9f, mny = 1e9f, mxy = -1e9f;
  for (int i = 0; i < n; ++i) { mnx = fminf(mnx, fX(f, pu[i])); mxx = fmaxf(mxx, fX(f, pu[i])); mny = fminf(mny, fX(f, pv[i])); mxy = fmaxf(mxy, fX(f, pv[i])); }
  Mask mk;
  mk.x0 = (int)floorf(mnx - wpx - 2); mk.y0 = (int)floorf(mny - wpx - 2);
  mk.w = (int)ceilf(mxx + wpx + 2) - mk.x0 + 1; mk.h = (int)ceilf(mxy + wpx + 2) - mk.y0 + 1;
  mk.p = (float*)calloc((size_t)mk.w * mk.h, sizeof(float));
  if (!mk.p) return;
  for (int i = 0; i + 1 < n; ++i) fSeg(f, pu[i], pv[i], pu[i + 1], pv[i + 1], wpx, c, &mk);
  for (int y = 0; y < mk.h; ++y)
    for (int x = 0; x < mk.w; ++x) fAdd(f, mk.x0 + x, mk.y0 + y, c, mk.p[y * mk.w + x]);
  free(mk.p);
}
static void fRect(FBuf& f, float u0, float v0, float u1, float v1, Col c, bool fill) {
  const int x0 = (int)lrintf(fX(f, u0)), x1 = max(x0, (int)lrintf(fX(f, u1)) - 1), y0 = (int)lrintf(fX(f, v0)), y1 = max(y0, (int)lrintf(fX(f, v1)) - 1);
  for (int y = y0; y <= y1; ++y)
    for (int x = x0; x <= x1; ++x)
      if (fill || y == y0 || y == y1 || x == x0 || x == x1) fAdd(f, x, y, c, 1.0f);
}
static void fPoly(FBuf& f, const float* pu, const float* pv, int n, Col c) {
  float mnx = 1e9f, mxx = -1e9f, mny = 1e9f, mxy = -1e9f;
  for (int i = 0; i < n; ++i) { mnx = fminf(mnx, fX(f, pu[i])); mxx = fmaxf(mxx, fX(f, pu[i])); mny = fminf(mny, fX(f, pv[i])); mxy = fmaxf(mxy, fX(f, pv[i])); }
  for (int y = max(0, (int)mny); y <= min(f.w - 1, (int)mxy + 1); ++y)
    for (int x = max(0, (int)mnx); x <= min(f.w - 1, (int)mxx + 1); ++x) {
      bool in = false;
      for (int i = 0, j = n - 1; i < n; j = i++) {
        const float xi = fX(f, pu[i]), yi = fX(f, pv[i]), xj = fX(f, pu[j]), yj = fX(f, pv[j]);
        if (((yi > y) != (yj > y)) && (x < (xj - xi) * (y - yi) / (yj - yi) + xi)) in = !in;
      }
      if (in) fAdd(f, x, y, c, 1.0f);
    }
}
// 箱ぼかしを3回(ガウスぼかしに近くなる)
// ぼかし(16ビットの絵)。横は1行ずつ。縦は上の行から順に進め、PSRAM を飛び飛びに読まない(元の値は直前 r+1 行ぶん取っておく)
static void hBlurRows(FBuf& f, int r, int j0, int j1, float* tmp) {     // tmp: w*3*2
  const int w = f.w, n = w * 3;
  float* src = tmp; float* dst = tmp + n;
  const float inv = 1.0f / (2 * r + 1);
  for (int j = j0; j < j1; ++j) {
    uint16_t* row = f.h + (size_t)j * n;
    for (int i = 0; i < n; ++i) src[i] = row[i];
    for (int ch = 0; ch < 3; ++ch) {
      float acc = 0;
      for (int i = -r; i <= r; ++i) acc += src[min(max(i, 0), w - 1) * 3 + ch];
      for (int i = 0; i < w; ++i) {
        dst[i * 3 + ch] = acc * inv;
        acc += src[min(i + r + 1, w - 1) * 3 + ch] - src[max(i - r, 0) * 3 + ch];
      }
    }
    for (int i = 0; i < n; ++i) row[i] = (uint16_t)(dst[i] + 0.5f);
  }
}
static void vBlurRows(FBuf& f, int r, int y0, int y1, float* acc, float* ring) {   // acc: w*3 / ring: (r+1)*w*3。y0 は 0 から順に
  const int w = f.w, n = w * 3;
  const float inv = 1.0f / (2 * r + 1);
  if (y0 == 0) {
    for (int k = 0; k < n; ++k) acc[k] = 0;
    for (int i = -r; i <= r; ++i) { const uint16_t* q = f.h + (size_t)min(max(i, 0), w - 1) * n; for (int k = 0; k < n; ++k) acc[k] += q[k]; }
  }
  for (int y = y0; y < y1; ++y) {
    uint16_t* row = f.h + (size_t)y * n;
    float* sv = ring + (size_t)(y % (r + 1)) * n;
    for (int k = 0; k < n; ++k) { sv[k] = row[k]; row[k] = (uint16_t)(acc[k] * inv + 0.5f); }
    if (y == w - 1) break;
    const uint16_t* qa = f.h + (size_t)min(y + r + 1, w - 1) * n;
    const float* qb = ring + (size_t)(max(y - r, 0) % (r + 1)) * n;
    for (int k = 0; k < n; ++k) acc[k] += qa[k] - qb[k];
  }
}
// 半分の大きさの絵を、1行ぶん引き伸ばして読む(x, y とも、重み 0.75 / 0.25 が交互に来る)
static void hRow(const FBuf& f, int y, float* out, float* tmp) {         // out: S*3 / tmp: w*3
  const int w = f.w, n = w * 3;
  const int iy = (y - 1) >> 1;
  const float fy = (y & 1) ? 0.25f : 0.75f;
  const uint16_t* A = f.h + (size_t)min(max(iy, 0), w - 1) * n;
  const uint16_t* B = f.h + (size_t)min(max(iy + 1, 0), w - 1) * n;
  const float wa = (1.0f - fy) / HQ, wb = fy / HQ;
  for (int i = 0; i < n; ++i) tmp[i] = A[i] * wa + B[i] * wb;
  for (int x = 0; x < S; ++x) {
    const int ix = (x - 1) >> 1;
    const float fx = (x & 1) ? 0.25f : 0.75f;
    const float* a = tmp + min(max(ix, 0), w - 1) * 3;
    const float* b = tmp + min(max(ix + 1, 0), w - 1) * 3;
    float* o = out + x * 3;
    o[0] = a[0] * (1.0f - fx) + b[0] * fx; o[1] = a[1] * (1.0f - fx) + b[1] * fx; o[2] = a[2] * (1.0f - fx) + b[2] * fx;
  }
}

// 線の仲間(くっきり描く絵 cv と、にじみ用 gl の両方に描く)
static void bothLine(FBuf& cv, FBuf& gl, const float* pu, const float* pv, int n, float wpx, Col c) {
  fPolyline(cv, pu, pv, n, wpx * (S / 466.0f), c);
  const float wh = wpx * (S / 466.0f) * 0.5f;
  fPolyline(gl, pu, pv, n, fmaxf(1.0f, wh), mulC(c, fminf(1.0f, wh)));
}
static void bothSeg(FBuf& cv, FBuf& gl, float u0, float v0, float u1, float v1, float wpx, Col c) {
  fSeg(cv, u0, v0, u1, v1, wpx * (S / 466.0f), c);
  const float wh = wpx * (S / 466.0f) * 0.5f;                   // 半分の大きさの絵では、線の太さも半分
  fSeg(gl, u0, v0, u1, v1, fmaxf(1.0f, wh), mulC(c, fminf(1.0f, wh)));
}

// ---- 動かない部分の絵(場面0の背景)。段に分けて、1回に1段ずつ描く
// 色を変えたときは、古い絵を見せたまま1コマに数段ずつ進めて、描き終えたら入れ替える(止まらない)
struct StatJob {
  int st, sub;                          // いまの段 / 段の中の何番目か
  FBuf cv, gl, ga, gb;
  uint8_t* mask;
  uint16_t *nImg, *n0;                  // 描いている途中の新しい絵
  float *line, *disSave, *vacc, *vring;
  int bx0, bx1, by0, by1;
  bool inPlace;                         // 今の絵(statImg / stat0)の上にじかに描いている
};
static StatJob* sj = nullptr;
static int blurR(int b) {               // にじみの絵 b(0: gl / 1: ga / 2: gb)のぼかしの半径
  const float k = S / 466.0f;
  return b == 0 ? max(1, (int)lrintf(2.5f * k)) : (b == 1 ? max(1, (int)lrintf(4.0f * k)) : 1);
}
static inline bool statParOK(int st) {  // この段は、コア0 で動きの計算と同時に進めてよい(箱の一覧・乱数・入れ替えを使う段はだめ)
  return st == 0 || st == 2 || st == 3 || st == 4 || st == 5 || st == 7 || st == 8 || st == 10;
}
static void statFree() {
  if (!sj) return;
  free(sj->cv.h); free(sj->gl.h); free(sj->ga.h); free(sj->gb.h); free(sj->mask); free(sj->line); free(sj->disSave); free(sj->vacc); free(sj->vring);
  if (!sj->inPlace) { free(sj->nImg); free(sj->n0); }
  free(sj); sj = nullptr;
}
static bool statBegin() {
  statFree();
  sj = (StatJob*)calloc(1, sizeof(StatJob));
  if (!sj) return false;
  StatJob& J = *sj;
  const int Sh = S / 2;
  J.cv = {nullptr, S, (S - 1) * 0.5f, S * 0.5f, (uint16_t*)ps_calloc((size_t)S * S * 3, sizeof(uint16_t))};
  J.gl = {nullptr, Sh, (Sh - 1) * 0.5f, Sh * 0.5f, (uint16_t*)ps_calloc((size_t)Sh * Sh * 3, sizeof(uint16_t))};
  J.ga = {nullptr, Sh, (Sh - 1) * 0.5f, Sh * 0.5f, (uint16_t*)ps_calloc((size_t)Sh * Sh * 3, sizeof(uint16_t))};
  J.gb = {nullptr, Sh, (Sh - 1) * 0.5f, Sh * 0.5f, (uint16_t*)ps_calloc((size_t)Sh * Sh * 3, sizeof(uint16_t))};
  J.mask = (uint8_t*)ps_calloc((size_t)S * S, 1);
  J.line = (float*)malloc((size_t)S * 3 * 3 * sizeof(float));                 // 1行ぶんの作業場(3本)
  J.disSave = (float*)malloc(sizeof(float) * MAXNODE);
  J.vacc = (float*)malloc((size_t)Sh * 3 * sizeof(float));
  J.vring = (float*)malloc((size_t)(blurR(1) + 1) * Sh * 3 * sizeof(float));   // いちばん大きいぼかしの r + 1 行ぶん
  if (!J.cv.h || !J.gl.h || !J.ga.h || !J.gb.h || !J.mask || !J.line || !J.disSave || !J.vacc || !J.vring) { statFree(); return false; }
  J.nImg = (uint16_t*)ps_malloc((size_t)S * S * 2);
  J.n0 = (uint16_t*)ps_malloc((size_t)S * S * 2);
  if (!J.nImg || !J.n0) {
    free(J.nImg); free(J.n0); J.nImg = J.n0 = nullptr;
    if (!statImg || !stat0) { statFree(); return false; }
    J.nImg = statImg; J.n0 = stat0; J.inPlace = true;                // 置き場が足りない: 今の絵の上に、上から少しずつ描く
  }
  return true;
}
static bool statStep() {                // 1段すすめる。描き終えて入れ替えたら true
  StatJob& J = *sj;
  const Theme& th = THEMES[theme];
  const int Sh = S / 2;
  const float k = S / 466.0f;
  FBuf &cv = J.cv, &gl = J.gl, &ga = J.ga, &gb = J.gb;
  (void)Sh; (void)k;
  switch (J.st) {
  case 0: {                             // 背景: 中ほどが少し明るい暗い空(16行ずつ)
    const int y1 = min(S, J.sub + 16);
  for (int y = J.sub; y < y1; ++y)
    for (int x = 0; x < S; ++x) {
      const float r = sqrtf((x - Sc) * (x - Sc) + (y - Sc) * (y - Sc)) / H2, f = clamp01(1.0f - r);
      fSet(cv, x, y, {th.bg0.r + th.bg1.r * f * f, th.bg0.g + th.bg1.g * f * f, th.bg0.b + th.bg1.b * f * f});
    }
    J.sub = y1; if (J.sub >= S) { J.st = 1; J.sub = 0; }
    return false;
  }
  case 1: {                             // うすい格子 + 散らばった点(インスタンス)
  for (int i = -10; i <= 10; ++i) {
    const int p = (int)lrintf(Sc + i * H2 / 10);
    for (int t = 0; t < S; ++t) { fSet(cv, p, t, th.grid); fSet(cv, t, p, th.grid); }
  }
  rs = 12345;
  for (int i = 0; i < 420; ++i) {
    const float a = srand01() * 6.2832f, r = sqrtf(0.05f + srand01() * 0.88f);
    const float u = r * cosf(a), v = r * sinf(a);
    const int sz = srand01() < 0.33f ? 2 : 1;
    const Col c = boxCol(u, v, 0.25f + srand01() * 0.35f);
    const int x = (int)PX(u), y = (int)PX(v);
    for (int j = 0; j < sz; ++j) for (int q = 0; q < sz; ++q) fSet(cv, x + q, y + j, c);
  }

    J.st = 2; return false;
  }
  case 2: {                             // 光輪: 放射の線 → 円と箱(1つずつ)
  // 光輪: 放射の線 / 3つの円 / 箱(箱はにじみだけ。くっきりした箱は毎フレーム描く)
  if (J.sub == 0) for (int i = 0; i < HALO_N[0]; ++i) {
    const float a = 6.2832f * i / HALO_N[0];
    bothSeg(cv, gl, HCU, HCV, HCU + HALO_R[0] * cosf(a), HCV + HALO_R[0] * sinf(a), 1.0f, boxCol(0, -0.12f, 0.9f * 0.18f));
  }
  if (J.sub > 0) {
    const int ring = J.sub - 1;
    for (int i = 0; i < HALO_N[ring]; ++i) {
      const float a = 6.2832f * i / HALO_N[ring] + ring * 0.13f;
      const float u = HCU + HALO_R[ring] * cosf(a), v = HCV + HALO_R[ring] * sinf(a), s = HALO_SZ[ring];
      if (figureSd(u, v) < 0.01f) continue;
      const Col c = boxCol(0, -0.12f, HALO_K[ring]);
      fRect(gl, u - s, v - s, u + s, v + s, mulC(c, 0.5f), false);   // 半分の大きさの絵なので、ふちは半分の明るさ
      fRect(gl, u - s, v - s, u + s, v - s * 0.35f, mulC(c, 0.6f), true);
    }
    float cu[161], cvv[161];
    for (int i = 0; i <= 160; ++i) { const float a = 6.2832f * i / 160; cu[i] = HCU + HALO_R[ring] * cosf(a); cvv[i] = HCV + HALO_R[ring] * sinf(a); }
    bothLine(cv, gl, cu, cvv, 161, 1.0f, th.ring);
  }
    if (++J.sub >= 4) { J.st = 3; J.sub = 0; }
    return false;
  }
  case 3: {
  // 舟形光背: 点線の楕円 + 箱(にじみだけ)。1回に楕円1本
  {
    const int j = J.sub;
    const float rx = j ? 0.64f : 0.56f, ry = j ? 0.68f : 0.60f, s = j ? 0.008f : 0.011f;
    for (int i = 0; i < 400; i += 2) {
      const float a0 = 6.2832f * i / 400, a1 = 6.2832f * (i + 1) / 400;
      bothSeg(cv, gl, rx * sinf(a0), -0.10f - ry * cosf(a0), rx * sinf(a1), -0.10f - ry * cosf(a1), 1.0f, j ? th.dotB : th.dotA);
    }
    const int n = j ? 96 : 72;
    for (int i = 0; i < n; ++i) {
      const float a = 6.2832f * i / n, u = rx * sinf(a), v = -0.10f - ry * cosf(a);
      if (v > 0.30f) continue;
      fRect(gl, u - s, v - s, u + s, v + s, boxCol(u, v, j ? 0.55f : 0.8f), true);
    }
  }
    if (++J.sub >= 2) { J.st = 4; J.sub = 0; }
    return false;
  }
  case 4: {                             // 蓮の台座: 花びら1枚ずつ(奥の列 → 手前の列)
  static const int PET[9] = {-4, -2, 0, 2, 4, -3, -1, 1, 3};
  {
    const int q = J.sub;
    const int kk = PET[q];
    const bool front = (kk & 1) != 0;
    const float cu = kk * 0.105f, cvv = front ? 0.395f : 0.37f;
    const float w = front ? 0.052f : 0.048f, h = front ? 0.15f : 0.17f;
    float pu[82], pv[82];
    for (int i = 0; i <= 40; ++i) {
      const float t = i / 40.0f;
      const float hw = w * powf(fmaxf(0.0f, sinf(3.14159265f * (0.06f + 0.94f * t))), 0.9f) * (0.55f + 0.45f * (1 - t)) * powf(1 - t, 0.25f);
      const float lean = 0.02f * kk * t * 0.3f, v = cvv - h * t;
      pu[i] = cu - hw + lean; pv[i] = v;
      pu[81 - i] = cu + hw + lean; pv[81 - i] = v;
    }
    const Col c = front ? th.petalF : th.petalB;
    fPoly(cv, pu, pv, 82, mulC(c, 0.10f));
    fPoly(gl, pu, pv, 82, mulC(c, 0.10f));
    float lu[83], lv[83];
    for (int i = 0; i < 82; ++i) { lu[i] = pu[i]; lv[i] = pv[i]; }
    lu[82] = pu[0]; lv[82] = pv[0];
    bothLine(cv, gl, lu, lv, 83, 2.0f, mulC(c, 0.85f));
    for (int m = 0; m < 3; ++m) {
      const float t = 0.2f + 0.3f * m, u = cu + 0.02f * kk * t * 0.3f, v = cvv - h * t, s = 0.006f;
      fRect(cv, u - s, v - s, u + s, v + s, c, true);
      fRect(gl, u - s, v - s, u + s, v + s, c, true);
    }
  }
    if (++J.sub >= 9) { J.st = 5; J.sub = 0; }
    return false;
  }
  case 5: {                             // 台座の線と、継承の線(8回に分けて)
  if (J.sub == 0) bothSeg(cv, gl, -0.50f, 0.40f, 0.50f, 0.40f, 1.0f, th.base);
  // 継承の線(親の箱のまん中 → 子の箱のまん中)
  for (int n = max(1, J.sub * nNode / 8); n < (J.sub + 1) * nNode / 8; ++n) {
    const Node& N = nodes[n];
    if (N.parent < 0) continue;
    const Node& P = nodes[N.parent];
    bothSeg(cv, gl, P.cu, P.cv, N.cu, N.cv, 1.0f, boxCol(N.cu, N.cv, N.depth > 3 ? 0.30f : 0.45f));
  }
    if (++J.sub >= 8) { J.st = 6; J.sub = 0; }
    return false;
  }
  case 6: {
  // 菩薩の箱(止まった状態)。箱のある所は、あとで「箱のにじみ」だけにする
  memcpy(J.disSave, nodeDis, sizeof(float) * nNode);              // 散り具合はあとで戻す(色を変えても箱は元に戻らない)
  nBox = 0;
  for (int i = 0; i < nNode; ++i) nodeDis[i] = 0;
  restMode = true;                                              // 波の来ていない状態
  emitNode(0, 1.0f);
  restMode = false;
  uint8_t* mask = J.mask;
  const int mg = (int)(24 * k);
  int bbX0 = S, bbX1 = -1, bbY0 = S, bbY1 = -1;
  for (int b = 0; b < nBox; ++b) {
    const DBox& B = box[b];
    bbX0 = min(bbX0, B.x0 - mg); bbX1 = max(bbX1, B.x1 + mg); bbY0 = min(bbY0, B.y0 - mg); bbY1 = max(bbY1, B.y1 + mg);
    const Col c = boxCol(((B.x0 + B.x1) * 0.5f - Sc) / H2, ((B.y0 + B.y1) * 0.5f - Sc) / H2, 1.0f);
    for (int y = max(0, (int)B.y0); y <= min(S - 1, (int)B.y1); ++y)
      for (int x = max(0, (int)B.x0); x <= min(S - 1, (int)B.x1); ++x) {
        mask[(size_t)y * S + x] = 1;                                 // しるし: ここは箱の中
        const bool edge = (y == B.y0 || y == B.y1 || x == B.x0 || x == B.x1);
        const Col cc = edge ? c : (y < B.hb ? mulC(c, 0.55f) : mulC(c, 0.16f));
        if (((x | y) & 1) == 0) fAdd(ga, x >> 1, y >> 1, cc, 1.0f);
      }
  }
  nBox = 0;
  memcpy(nodeDis, J.disSave, sizeof(float) * nNode);
  J.bx0 = max(0, bbX0); J.by0 = max(0, bbY0); J.bx1 = min(S - 1, bbX1); J.by1 = min(S - 1, bbY1);
    memcpy(gb.h, ga.h, (size_t)Sh * Sh * 3 * sizeof(uint16_t));
    J.st = 7; J.sub = 0; return false;
  }
  case 7: {                             // ぼかし: 3枚 × 3回 × 横縦 × 4つに分けて
    const int b = J.sub / 24, rem = J.sub % 24, dir = (rem / 4) % 2, part = rem % 4;
    FBuf& f = b == 0 ? gl : (b == 1 ? ga : gb);
    const int r = blurR(b), j0 = part * f.w / 4, j1 = (part + 1) * f.w / 4;
    if (dir == 0) hBlurRows(f, r, j0, j1, J.line);
    else vBlurRows(f, r, j0, j1, J.vacc, J.vring);
    if (++J.sub >= 72) { J.st = 10; J.sub = 0; }
    return false;
  }
  case 10: {                            // 箱のにじみ2枚を1枚に(ga = ga * 0.8 + gb * 0.35)。4つに分けて
    const int n = Sh * Sh * 3, i0 = J.sub * n / 4, i1 = (J.sub + 1) * n / 4;
    for (int i = i0; i < i1; ++i) ga.h[i] = hSat((int)lrintf(ga.h[i] * 0.8f + gb.h[i] * 0.35f));
    if (++J.sub >= 4) { J.st = 8; J.sub = 0; }
    return false;
  }
  case 8: {                             // 合わせて、画面の色に(8行ずつ)
    const int y1 = min(S, J.sub + 8);
    float* gR = J.line; float* aR = J.line + S * 3; float* tmp = J.line + S * 6;
    for (int y = J.sub; y < y1; ++y) {
      hRow(gl, y, gR, tmp);
      hRow(ga, y, aR, tmp);
      for (int x = 0; x < S; ++x) {
        const uint16_t* q = cv.h + ((size_t)y * S + x) * 3;
        const float* g = gR + x * 3; const float* a = aR + x * 3;
        const Col o0 = {q[0] * (1.0f / HQ) + g[0] * 0.9f, q[1] * (1.0f / HQ) + g[1] * 0.9f, q[2] * (1.0f / HQ) + g[2] * 0.9f};
        J.n0[(size_t)y * S + x] = toPix(o0);                          // 菩薩のいない絵
        Col o = {a[0], a[1], a[2]};                                   // 箱のある所は「箱のにじみ」だけ
        if (!J.mask[(size_t)y * S + x]) { o.r += o0.r; o.g += o0.g; o.b += o0.b; }
        J.nImg[(size_t)y * S + x] = toPix(o);
      }
    }
    J.sub = y1; if (J.sub >= S) J.st = 9;
    return false;
  }
  default: {                            // 入れ替え
    if (!J.inPlace) { free(statImg); free(stat0); }
    statImg = J.nImg; stat0 = J.n0; J.nImg = J.n0 = nullptr; J.inPlace = true;
    bbX0 = J.bx0; bbX1 = J.bx1; bbY0 = J.by0; bbY1 = J.by1;
    statFree();
    return true;
  }
  }
}
static unsigned psFreeKB() {
#if defined(ESP_PLATFORM)
  return (unsigned)(heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024);
#else
  return 0;
#endif
}
static void buildTextLut() {
  const Col tc = THEMES[theme].text;
  for (int i = 0; i < 16; ++i) textLut[i] = to565(mulC(tc, 0.82f * i / 15.0f));
}
static bool buildStatic() {             // 起動時: 一気に描く
  if (!statBegin()) return false;
  while (!statStep()) {}
  buildTextLut();
  return true;
}

// ---------------------------------------------------------------- 蝶(NullSutra v2 と同じ、ゆらゆらした飛び方)
// 蝶の絵は、起動時に「羽ばたきの8コマ × 大きさ3段」を描いておき(色は番号)、毎フレーム回して貼ります
static constexpr int NFR = 8, NMIP = 3, NPIDX = 132;
static const int     MIPT[NMIP] = {96, 48, 24};
static uint8_t*  atlas[NMIP];
static constexpr size_t nmOa49 = nmAlignTo(nmOv49, 16), nmOv50 = nmOa49 + sizeof(float[NMIP]); static float (&mipR0)[NMIP] = *reinterpret_cast<float(*)[NMIP]>(NM_ARENA + nmOa49);
static uint16_t* flyPal;                                        // [色違い2][コマ8][NPIDX]

struct Fly {
  float x, y, vx, vy, ph, hz, head, R, wand;
  uint8_t var;
  float cx, cy, rr, dxx, dxy, dyx, dyy;                         // 描くための値
  int   y0, y1, T;
  const uint8_t* tex; const uint16_t* pal;
};
static constexpr size_t nmOa50 = nmAlignTo(nmOv50, 16), nmOv51 = nmOa50 + sizeof(Fly[MAXFLY]); static Fly (&fly)[MAXFLY] = *reinterpret_cast<Fly(*)[MAXFLY]>(NM_ARENA + nmOa50);
static int flyIdx = FLY_START;
static constexpr size_t nmOa51 = nmAlignTo(nmOv51, alignof(int)), nmOv52 = nmOa51 + sizeof(int); static int& nFly = *reinterpret_cast<int*>(NM_ARENA + nmOa51);

// 羽の形: 大きな前ばね + 丸い後ばね(楕円を傾けて重ねる)。羽ばたきで横方向がつぶれる。絵の中では頭が上
static void drawFlyAtlas(uint8_t* dst, int T, float flap) {
  const float h = T * 0.5f, R0 = h / 1.15f, iR = 1.0f / R0, lim2 = h * h;
  const float bodyW = fmaxf(0.06f, 0.55f * iR), antW = fmaxf(0.022f, 0.45f * iR), iflap = 1.0f / flap;
  const bool detail = T >= 48;
  for (int j = 0; j < T; ++j)
    for (int i = 0; i < T; ++i) {
      const float px = i + 0.5f - h, py = j + 0.5f - h;
      uint8_t o = 0;
      if (px * px + py * py <= lim2) {
        const float u = -py * iR, v = px * iR, av = fabsf(v), vv = av * iflap;
        const float fu = u - 0.22f, fv = vv - 0.48f;
        const float fa = fu * 0.581f + fv * 0.814f, fb = -fu * 0.814f + fv * 0.581f;
        const float e1 = fa * fa * 2.7778f + fb * fb * 11.111f;
        const float hu = u + 0.16f, hv = vv - 0.36f;
        const float ha = hu * -0.666f + hv * 0.746f, hb = -hu * 0.746f - hv * 0.666f;
        const float e2 = ha * ha * 6.25f + hb * hb * 11.111f;
        const float e = e1 < e2 ? e1 : e2;
        if (av < bodyW && u > -0.52f && u < 0.42f) o = 129;
        else if (e < 1.0f && vv >= 0.04f) {
          if (e > 0.82f) o = (detail && e1 < e2 && fa > 0.12f && e > 0.87f && e < 0.95f && ((int)(vv * 9.0f) & 1)) ? 131 : 130;
          else {
            int a = (int)(e * 18.29f + 0.5f); if (a > 15) a = 15;
            const float fb2 = vv * 20.0f + 0.5f;
            o = (uint8_t)(1 + a * 8 + (fb2 > 7.0f ? 7 : (int)fb2));
          }
        } else if (detail && u > 0.40f && u < 0.74f) {
          const float want = 0.05f + (u - 0.40f) * 0.55f;
          if (fabsf(av - want) < (u > 0.68f ? antW * 1.8f : antW)) o = 129;
        }
      }
      dst[j * T + i] = o;
    }
}

// 蝶の色は、いまの色の組み合わせから作る(金蒼なら NullSutra v2 と同じ青い蝶)
static void buildFlyPal() {
  const Theme& th = THEMES[theme];
  for (int var = 0; var < 2; ++var) {
    const Col bright = var ? lerpC(th.mid, th.cool, 0.45f) : th.mid;
    const Col deep   = mulC(lerpC(bright, th.cool, 0.35f), 0.55f);
    const Col pale   = lerpC(bright, C8(255, 255, 255), 0.7f);
    const Col root   = mulC(th.cool, 0.12f);
    for (int fr = 0; fr < NFR; ++fr) {
      uint16_t* P = flyPal + (var * NFR + fr) * NPIDX;
      const float flap = 0.14f + 0.86f * fr / (NFR - 1), glint = 0.60f + 0.55f * flap;
      P[0] = 0;
      for (int a = 0; a < 16; ++a)
        for (int b = 0; b < 8; ++b) {
          const float t = a / 15.0f, fromBody = b / 7.0f;
          Col c = lerpC(deep, bright, smooth3(1.0f - t));
          c = lerpC(c, pale, 0.45f * (1.0f - t) * (1.0f - t) * flap);
          c = lerpC(root, c, smooth3(fromBody));
          P[1 + a * 8 + b] = toPix(mulC(c, glint));
        }
      P[129] = toPix({0.04f, 0.03f, 0.05f});
      P[130] = toPix({0.02f, 0.02f, 0.05f});
      P[131] = toPix(mulC(C8(205, 215, 240), 0.7f + 0.3f * flap));
    }
  }
}

static void spawnFly(int i) {
  Fly& f = fly[i];
  const float a = frand(0, 6.2832f), r = frand(0.15f, 0.38f) * S;
  f.x = Sc + cosf(a) * r; f.y = Sc + sinf(a) * r * (isRound ? 1.0f : 0.6f);
  f.vx = frand(-20, 20) * (S / 466.0f); f.vy = frand(-10, 10) * (S / 466.0f);
  f.ph = frand(0, 6.2832f); f.hz = frand(2.6f, 3.6f);
  f.head = frand(0, 6.2832f);
  f.R = FLY_SIZE * S * frand(0.80f, 1.12f);
  f.wand = frand(0, 100) + i * 13.0f;
  f.var = (uint8_t)(i % 3 == 2 ? 1 : 0);
  f.y0 = 1; f.y1 = 0;
}

static void setFlyCount(int idx) {
  flyIdx = idx;
  const int n = FLY_COUNTS[idx];
  for (int i = nFly; i < n; ++i) spawnFly(i);
  nFly = n;
  Serial.printf("butterflies: %d\n", nFly);
}

// 動き: ふだんはゆっくりさまよう行き先を追いかける / 止めて押さえていると指のまわりを回る / タップで散る
static void updateFlies(bool touching, bool tapped, float tx, float ty, float dt) {
  const float k = S / 466.0f;
  const float vx0 = (float)max(0, -XOFF), vx1 = (float)min(S, W - XOFF) - 1;
  const float vy0 = (float)max(0, -YOFF), vy1 = (float)min(S, H - YOFF) - 1;
  const float ay0 = 0.5f * (vy1 - vy0) / H2;                    // 見えている縦の広さ(CoreS3 は横長なので狭い)
  for (int i = 0; i < nFly; ++i) {
    Fly& f = fly[i];
    f.wand += dt * 0.17f;
    float gx = Sc + 0.36f * S * sinf(f.wand * 1.3f + i) * cosf(f.wand * 0.7f);
    float gy = Sc + 0.30f * S * fminf(1.0f, ay0) * sinf(f.wand * 1.7f + i * 2.1f);
    if (touching && holdT > 0.35f) {
      const float a = simT * 1.4f + i * 6.2832f / nFly, r = (0.11f + 0.015f * (i % 4)) * S + 0.02f * S * sinf(simT * 2.0f + i);
      gx = tx + cosf(a) * r; gy = ty + sinf(a) * r * 0.8f;
    }
    float ax = (gx - f.x) * 1.6f - f.vx * 1.2f + 60.0f * k * sinf(simT * 2.3f + i * 1.7f);
    float ay = (gy - f.y) * 1.6f - f.vy * 1.2f + 60.0f * k * sinf(simT * 2.9f + i * 0.9f);
    for (int j = 0; j < nFly; ++j) {                            // 蝶どうしは少し離れる
      if (j == i) continue;
      const float ox = f.x - fly[j].x, oy = f.y - fly[j].y, d2 = ox * ox + oy * oy, want = 1.9f * f.R;
      if (d2 < want * want && d2 > 1.0f) { const float d = sqrtf(d2), q = (want - d) * 9.0f; ax += ox / d * q; ay += oy / d * q; }
    }
    if (tapped) {                                               // 驚いて散る
      const float dx = f.x - tx, dy = f.y - ty, d = sqrtf(dx * dx + dy * dy) + 1.0f;
      if (d < 0.35f * S) { f.vx += dx / d * 260.0f * k; f.vy += dy / d * 260.0f * k; }
    }
    f.vx += ax * dt; f.vy += ay * dt;
    float sp = sqrtf(f.vx * f.vx + f.vy * f.vy);
    const float lim = 0.55f * S;
    if (sp > lim) { f.vx *= lim / sp; f.vy *= lim / sp; sp = lim; }
    f.x += f.vx * dt; f.y += f.vy * dt;
    if (isRound) {
      const float dx = f.x - Sc, dy = f.y - Sc, d = sqrtf(dx * dx + dy * dy), R = 0.44f * S;
      if (d > R) { f.x = Sc + dx / d * R; f.y = Sc + dy / d * R; f.vx *= 0.5f; f.vy *= 0.5f; }
    } else {
      const float m = f.R * 0.5f;
      if (f.x < vx0 + m) { f.x = vx0 + m; f.vx *= -0.5f; }
      if (f.x > vx1 - m) { f.x = vx1 - m; f.vx *= -0.5f; }
      if (f.y < vy0 + m) { f.y = vy0 + m; f.vy *= -0.5f; }
      if (f.y > vy1 - m) { f.y = vy1 - m; f.vy *= -0.5f; }
    }
    if (sp > 5.0f * k) {                                        // 向きは進む方へ、ゆっくり
      float da = atan2f(f.vy, f.vx) - f.head;
      while (da > 3.14159f) da -= 6.28318f;
      while (da < -3.14159f) da += 6.28318f;
      f.head += da * fminf(1.0f, dt * 3.0f);
    }
    f.ph += dt * 6.2832f * f.hz * (0.8f + 0.6f * fminf(1.0f, sp / (0.3f * S)));
    if (f.ph > 1000.0f) f.ph -= 318.0f * 3.14159265f;

    // 描くための値
    int m = 0;
    while (m < NMIP - 1 && mipR0[m + 1] >= 0.85f * f.R) ++m;
    const int T = MIPT[m];
    const float s = mipR0[m] / f.R, cs = cosf(f.head), sn = sinf(f.head);
    f.dxx = -s * sn; f.dxy = s * cs; f.dyx = -s * cs; f.dyy = -s * sn;
    f.cx = f.x; f.cy = f.y; f.rr = T * 0.5f / s;
    f.y0 = (int)floorf(f.cy - f.rr); f.y1 = (int)ceilf(f.cy + f.rr);
    const int fr = (int)(fabsf(cosf(f.ph)) * (NFR - 1) + 0.5f);
    f.T = T; f.tex = atlas[m] + (size_t)fr * T * T;
    f.pal = flyPal + (f.var * NFR + fr) * NPIDX;
  }
}

// ---------------------------------------------------------------- 1フレームの準備
struct Input { bool touching; bool tapped; float tx, ty; float dt; float tiltX, tiltY; bool shake; };   // tilt: 画面の上で「下」がどちらか(-1..1 くらい) / shake: 振った


static void bucketBoxes() {
  for (int k = 0; k <= NB; ++k) entStart[k] = 0;
  int total = 0;
  for (int i = 0; i < nBox; ++i) {
    const int a = max(0, (int)box[i].y0) / BAND, b = min(S - 1, (int)box[i].y1) / BAND;
    if (total + (b - a + 1) > MAXENT) { nBox = i; break; }
    for (int k = a; k <= b; ++k) entStart[k + 1]++;
    total += b - a + 1;
  }
  for (int k = 0; k < NB; ++k) entStart[k + 1] += entStart[k];
  int16_t fill[MAXNB];
  for (int k = 0; k < NB; ++k) fill[k] = entStart[k];
  for (int i = 0; i < nBox; ++i) {
    const int a = max(0, (int)box[i].y0) / BAND, b = min(S - 1, (int)box[i].y1) / BAND;
    for (int k = a; k <= b; ++k) ent[fill[k]++] = (int16_t)i;
  }
}

static void addPD(float x, float y, Col c, float k) {
  const int ix = (int)floorf(x), iy = (int)floorf(y);
  if (ix < 1 || iy < 1 || ix >= S - 1 || iy >= S - 1 || nPD >= MAXPD) return;
  PDraw& o = pd[nPD++];
  o.x = (int16_t)ix; o.y = (int16_t)iy; o.c = to565(mulC(c, k)); o.ch = to565(mulC(c, k * 0.4f));
}

// 光の粒と、はじけた光の輪を進めて、点として並べる
static void stepParts(float dt) {
  const float damp = expf(-dt * 1.2f);
  for (int i = 0; i < MAXPART; ++i) {
    Part& p = part[i];
    if (p.life <= 0) continue;
    p.life -= dt;
    if (p.life <= 0) continue;
    p.vx *= damp; p.vy *= damp;
    p.x += p.vx * dt; p.y += p.vy * dt;
    const float l = p.life / p.maxL;
    addPD(p.x, p.y, p.c, l * (0.6f + 0.4f * sinf(p.life * 23.0f + i)));
  }
  // 光の輪: 触った所から、0.7秒で画面の半分ほどまで広がって消える
  for (int r = 0; r < NRING; ++r) {
    const float age = simT - ring[r].t0;
    if (age < 0 || age > 0.7f) continue;
    const float rad = (0.08f + 1.0f * (1.0f - (1.0f - age / 0.7f) * (1.0f - age / 0.7f))) * H2 * (0.5f + 0.5f * ring[r].k);
    const float kk = ring[r].k * (1.0f - age / 0.7f) * 1.8f;
    const int n = min(400, max(12, (int)(6.2832f * rad / 3.0f)));
    const Col c = lerpC(THEMES[theme].warm, C8(255, 255, 255), 0.5f);
    for (int i = 0; i < n; ++i) {                                // 2重の輪(外側が明るく、内側は淡く)
      const float a = 6.2832f * i / n, ca = cosf(a), sa = sinf(a);
      addPD(ring[r].x + ca * rad, ring[r].y + sa * rad, c, kk);
      if (i & 1) addPD(ring[r].x + ca * (rad - 4.0f), ring[r].y + sa * (rad - 4.0f), c, kk * 0.5f);
    }
  }
}

// 点を帯ごとに並べる
static void finishPD() {
  for (int k = 0; k <= NB; ++k) pdStart[k] = 0;
  for (int i = 0; i < nPD; ++i) pdStart[pd[i].y / BAND + 1]++;
  for (int k = 0; k < NB; ++k) pdStart[k + 1] += pdStart[k];
  int16_t fill[MAXNB];
  for (int k = 0; k < NB; ++k) fill[k] = pdStart[k];
  for (int i = 0; i < nPD; ++i) pdOrd[fill[pd[i].y / BAND]++] = (int16_t)i;
}

// ---------------------------------------------------------------- 場面 1〜3(文字の菩薩 / 分かれる菩薩 / 映る菩薩)
// どの場面も、菩薩の形(figureSd)から作った絵を使います。画像のデータは入っていません
static const char* const SCENE_NAME[NSCENE] = {"boxes", "letters", "spawn", "mirror"};

static uint16_t* scnBuf[4];              // [S*S] 場面2の背景(バイト順入れ替え済み)。場面1・3は番号の地図(scnIdx)と色の表で描く

// 菩薩の絵(明るさ 0..255)。大きさ3段。u -0.5..0.5 / v -0.68..0.32 の四角
static constexpr int NFS = 3;
static const int FST[NFS] = {192, 96, 48};
static uint8_t* figSpr[NFS];
static constexpr float FIG_V0 = -0.68f;
// 起動したあとの準備は、少しずつ(1回で1行・1文字くらい)進める。画面を止めないように
static int prepM = 0, prepY = 0, prepPart = 0;     // いまの段の中の進み具合
static uint32_t prepH = 0, prepWork = 0;           // 点の絵の乱数 / この段にかかった時間(マイクロ秒)
static bool figStep() {                            // 菩薩の点の絵を1行。全部終わったら true
  if (prepM >= NFS) return true;
  const int m = prepM, T = FST[m];
  if (prepY == 0) prepH = 2463534242u + m;
  uint8_t* d = figSpr[m];
  const float px = 1.0f / T;
  uint32_t h = prepH;
  const int y = prepY;
  for (int x = 0; x < T; ++x) {
    const float u = (x + 0.5f) * px - 0.5f, v = (y + 0.5f) * px + FIG_V0;
    const float sd = figureSd(u, v);
    h ^= h << 13; h ^= h >> 17; h ^= h << 5;
    float a = 0;
    const float e = sd / (1.3f * px);
    a += 235.0f * expf(-e * e);                                   // ふち
    if (sd < 0) {                                                  // 中: 散らばった点と、うすい格子(ガラスに刻んだように)
      a += 22.0f;
      if ((h & 255) < (m == 2 ? 120u : 80u)) a += 110.0f;
      if ((x % 5 == 0 || y % 5 == 0) && m < 2) a += 28.0f;
    }
    const float hr = hypotf(u - HCU, v - HCV);                    // 頭のうしろの光輪
    if (sd > 0.004f) {
      const float r1 = (hr - 0.205f) / (1.1f * px), r2 = (hr - 0.24f) / (1.0f * px);
      a += 170.0f * expf(-r1 * r1) + 80.0f * expf(-r2 * r2);
    }
    d[y * T + x] = (uint8_t)(a > 255 ? 255 : a);
  }
  prepH = h;
  if (++prepY >= T) { prepY = 0; ++prepM; }
  return prepM >= NFS;
}

// 菩薩を1体描く予定(帯ごとに描く)
//   glitch: 行がずれたり抜けたりする強さ(0..255) / reveal: 上から何割まで見せるか(255 = ぜんぶ) / outline: ふちだけ描く
struct SprI { int16_t x0, y0, x1, y1; int32_t step; float top, left; uint8_t mip, flip, glitch, gseed, reveal, outline; uint16_t lut[16], lutR[16], lutC[16]; };
static constexpr int MAXSPR = 72;
static SprI* spr;
static constexpr size_t nmOa52 = nmAlignTo(nmOv52, alignof(int)), nmOv53 = nmOa52 + sizeof(int); static int& nSpr = *reinterpret_cast<int*>(NM_ARENA + nmOa52);
static constexpr size_t nmOa53 = nmAlignTo(nmOv53, alignof(uint32_t)), nmOv54 = nmOa53 + sizeof(uint32_t); static uint32_t& gFrame = *reinterpret_cast<uint32_t*>(NM_ARENA + nmOa53);  // コマの番号(グリッチの乱れ方を毎コマ変える)
static SprI* addSpr(float cx, float cy, float size, Col c, float k, bool flipX, bool flipY) {
  if (nSpr >= MAXSPR || size < 4 || k < 0.01f) return nullptr;
  int m = NFS - 1;
  while (m > 0 && FST[m] < size) --m;
  const float half = size * 0.5f;
  SprI& o = spr[nSpr];
  o.left = cx - half; o.top = cy - half;
  o.x0 = (int16_t)floorf(o.left); o.y0 = (int16_t)floorf(o.top);
  o.x1 = (int16_t)ceilf(cx + half); o.y1 = (int16_t)ceilf(cy + half);
  if (o.x1 < 0 || o.y1 < 0 || o.x0 >= S || o.y0 >= S) return nullptr;
  o.step = (int32_t)(FST[m] / size * 65536.0f);
  o.mip = (uint8_t)m; o.flip = (uint8_t)((flipX ? 1 : 0) | (flipY ? 2 : 0));
  o.glitch = 0; o.gseed = 0; o.reveal = 255; o.outline = 0;
  for (int i = 0; i < 16; ++i) {                                  // 乱れた行で使う、赤と青緑にずれた色
    const float q = k * i / 15.0f;
    o.lut[i] = to565(mulC(c, q));
    o.lutR[i] = to565({fminf(1.0f, c.r * 1.3f) * q, c.g * 0.15f * q, c.b * 0.1f * q});
    o.lutC[i] = to565({c.r * 0.1f * q, c.g * q, fminf(1.0f, c.b * 1.3f) * q});
  }
  ++nSpr;
  return &o;
}

// 小さな文字のラベル
struct Lab { int16_t x, y; char s[24]; uint8_t sc; uint16_t lut[16]; };
static constexpr int MAXLAB = 40;
static Lab* lab;
static constexpr size_t nmOa54 = nmAlignTo(nmOv54, alignof(int)), nmOv55 = nmOa54 + sizeof(int); static int& nLab = *reinterpret_cast<int*>(NM_ARENA + nmOa54);
static void addLab(float x, float y, const char* s, Col c, float k, int scale = 1) {
  if (nLab >= MAXLAB || k < 0.02f) return;
  Lab& o = lab[nLab++];
  const int n = (int)strlen(s);
  o.sc = (uint8_t)scale;
  o.x = (int16_t)lrintf(x - n * FCW * scale * 0.5f); o.y = (int16_t)lrintf(y);
  strncpy(o.s, s, 23); o.s[23] = 0;
  for (int i = 0; i < 16; ++i) o.lut[i] = to565(mulC(c, k * i / 15.0f));
}

// ---- 般若心経(玄奘訳の流布本。古い経典で、著作権の対象ではありません)と、その文字を描く道具(NullSutra と同じ)
static const char SUTRA[] =
  "観自在菩薩行深般若波羅蜜多時照見五蘊皆空度一切苦厄舎利子色不異空空不異色色即是空空即是色"
  "受想行識亦復如是舎利子是諸法空相不生不滅不垢不浄不増不減是故空中無色無受想行識無眼耳鼻舌身意"
  "無色声香味触法無眼界乃至無意識界無無明亦無無明尽乃至無老死亦無老死尽無苦集滅道無智亦無得"
  "以無所得故菩提薩埵依般若波羅蜜多故心無罣礙無罣礙故無有恐怖遠離一切顛倒夢想究竟涅槃"
  "三世諸仏依般若波羅蜜多故得阿耨多羅三藐三菩提故知般若波羅蜜多是大神呪是大明呪是無上呪"
  "是無等等呪能除一切苦真実不虚故説般若波羅蜜多呪即説呪曰羯諦羯諦波羅羯諦波羅僧羯諦菩提薩婆訶"
  "般若心経";
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

// ---- 場面1: 文字の菩薩(般若心経の文字が、縦の列になって上から降ってくる。菩薩の形の所だけ明るく光る)
static constexpr int   RAIN_CELL = 17;          // 文字のます目の大きさ(466 の画面で。小さいほど文字が多い)
static constexpr int   MAXGLY = 200, MAXTXT = 400;
static constexpr size_t nmOa55 = nmAlignTo(nmOv55, alignof(int)), nmOv56 = nmOa55 + sizeof(int); static int& rCell = *reinterpret_cast<int*>(NM_ARENA + nmOa55);
static constexpr size_t nmOa56 = nmAlignTo(nmOv56, alignof(int)), nmOv57 = nmOa56 + sizeof(int); static int& rCols = *reinterpret_cast<int*>(NM_ARENA + nmOa56);
static constexpr size_t nmOa57 = nmAlignTo(nmOv57, alignof(int)), nmOv58 = nmOa57 + sizeof(int); static int& rRows = *reinterpret_cast<int*>(NM_ARENA + nmOa57);
static constexpr size_t nmOa58 = nmAlignTo(nmOv58, alignof(int)), nmOv59 = nmOa58 + sizeof(int); static int& rX0 = *reinterpret_cast<int*>(NM_ARENA + nmOa58);
static constexpr size_t nmOa59 = nmAlignTo(nmOv59, alignof(int)), nmOv60 = nmOa59 + sizeof(int); static int& rNG = *reinterpret_cast<int*>(NM_ARENA + nmOa59);
static constexpr size_t nmOa60 = nmAlignTo(nmOv60, alignof(int)), nmOv61 = nmOa60 + sizeof(int); static int& rTL = *reinterpret_cast<int*>(NM_ARENA + nmOa60);
static constexpr size_t nmOa61 = nmAlignTo(nmOv61, alignof(int)), nmOv62 = nmOa61 + sizeof(int); static int& rNUL = *reinterpret_cast<int*>(NM_ARENA + nmOa61);
static uint8_t* rAtlas;                         // [文字][rCell*rCell] 濃さ 0..15
static uint8_t* rText;                          // 経文 → 文字の番号
static int16_t* rBase;                          // [列] 列ごとの、経文の読みはじめ
static float*   rScroll;                        // [列] 列ごとに、文字がゆっくり下へ流れた量(ピクセル)
static float*   rSpeed;                         // [列] その速さ(ピクセル/秒)
static int16_t* rScrollI;                       // [列] このコマの流れた量(整数)
static uint32_t* rNul;                          // [列][経文の長さ/32] 「ﾇﾙ」にする所の印
static constexpr int NUL_EVERY_MIN = 5;         // 「ﾇﾙ」は、列の流れの中に この範囲の文字数おき(ランダム)に出る(NullSutra と同じ)
static constexpr int NUL_EVERY_MAX = 30;
static constexpr int NULW = (MAXTXT + 31) / 32;
struct Drop { float head, speed, len; };
static Drop*    rDrop;                          // [列*2] 列ごとに2つの「落ちてくる光」
static uint16_t* rCOut;                         // [行*列] 菩薩の外の色
static uint16_t* rCIn;                          // [行*列] 菩薩の中の色
static int8_t*  rDx;                            // [行*列] 波紋で文字が横にゆれる量
static uint8_t* rMask;                          // [(S/2)*(S/2)] 菩薩の形(0..16)
struct Rip { float x, y, t0, k; };
static constexpr int NRIP = 8;
static constexpr size_t nmOa62 = nmAlignTo(nmOv62, 16), nmOv63 = nmOa62 + sizeof(Rip[NRIP]); static Rip (&rip)[NRIP] = *reinterpret_cast<Rip(*)[NRIP]>(NM_ARENA + nmOa62);
static constexpr size_t nmOa63 = nmAlignTo(nmOv63, alignof(int)), nmOv64 = nmOa63 + sizeof(int); static int& ripNext = *reinterpret_cast<int*>(NM_ARENA + nmOa63);
static float ripLast = -10;
// 列 c の流れ(経文を1周すると同じ並びに戻る)に、NUL_EVERY_MIN〜MAX 文字おきに「ﾇﾙ」の印をつける(NullSutra と同じ決め方)。
// 1周のつなぎ目でも間が範囲に収まるように、間の数の合計がちょうど経文の長さになるように選ぶ
static void makeNulMask(int c) {
  uint32_t* m = rNul + c * NULW;
  for (int i = 0; i < NULW; ++i) m[i] = 0;
  const int lo = NUL_EVERY_MIN, hi = NUL_EVERY_MAX;
  if (lo <= 0 || hi < lo || rTL < lo) return;
  const int off = (int)(esp_random() % (uint32_t)rTL);
  int pos = 0;
  for (;;) {
    const int q = (pos + off) % rTL;
    m[q >> 5] |= 1u << (q & 31);
    const int rem = rTL - pos;
    if (rem <= hi) break;
    const int top = max(lo, min(hi, rem - lo));
    pos += lo + (int)(esp_random() % (uint32_t)(top - lo + 1));
  }
}
// 列 c の、上から r 番目の字(文字は列ごとにゆっくり下へ流れる)
static inline int rGlyph(int c, int r) {
  int i = (rBase[c] + r) % rTL;
  if (i < 0) i += rTL;
  if ((rNul[c * NULW + (i >> 5)] >> (i & 31)) & 1) return rNUL;
  return rText[i];
}
static uint32_t* rnUniq = nullptr;                 // 般若心経の準備のあいだだけ使う
static M5Canvas* rnCv = nullptr;
static float* rnCore = nullptr;
static int rainStep() {                            // 般若心経の文字を少し。1: 終わり / 0: まだ / -1: メモリが足りない
  if (prepPart == 0) {                             // はじめ: 使う文字を数えて、置き場を用意
    rnUniq = (uint32_t*)malloc(MAXGLY * sizeof(uint32_t));
    if (!rnUniq) return -1;
    rTL = 0; rNG = 0;
    for (const char* p = SUTRA; *p && rTL < MAXTXT;) {
      const uint32_t cp = nextCp(p);
      int g = -1;
      for (int i = 0; i < rNG; ++i) if (rnUniq[i] == cp) { g = i; break; }
      if (g < 0) { if (rNG >= MAXGLY) continue; rnUniq[rNG] = cp; g = rNG++; }
      rText[rTL++] = (uint8_t)g;
    }
    rNUL = rNG;
    const int NG = rNG + 1, C = rCell, N = C * 2;
    rAtlas = (uint8_t*)ps_malloc((size_t)NG * C * C);
    if (!rAtlas) return -1;
    rnCv = new M5Canvas(&M5.Display);
    rnCv->setColorDepth(16);
    if (!rnCv->createSprite(N, N)) return -1;
    rnCore = (float*)malloc(C * C * sizeof(float));
    if (!rnCore) return -1;
    prepPart = 1; prepY = 0;
    return 0;
  }
  if (prepPart == 1) {                             // 1文字ずつ
    const int NG = rNG + 1, C = rCell, N = C * 2, g = prepY;
    char s[8];
    if (g < rNG) toUtf8(rnUniq[g], s); else strcpy(s, "ﾇﾙ");
    rnCv->fillSprite(0);
    rnCv->setFont(&fonts::lgfxJapanMincho_20);
#if defined(ESP_PLATFORM)
    if (g < rNG && isExtraGlyph(rnUniq[g])) rnCv->setFont(&sutraExtraFont);
#endif
    rnCv->setTextSize((C - 2) * 2 / 20.0f);
    rnCv->setTextColor(0xFFFF); rnCv->setTextDatum(middle_center);
    rnCv->drawString(s, N / 2, N / 2);
    const uint16_t* b = (const uint16_t*)rnCv->getBuffer();
    for (int y = 0; y < C; ++y)
      for (int x = 0; x < C; ++x)
        rnCore[y * C + x] = ((b[(2 * y) * N + 2 * x] != 0) + (b[(2 * y) * N + 2 * x + 1] != 0) + (b[(2 * y + 1) * N + 2 * x] != 0) + (b[(2 * y + 1) * N + 2 * x + 1] != 0)) * 0.25f;
    uint8_t* d = rAtlas + (size_t)g * C * C;
    for (int y = 0; y < C; ++y)
      for (int x = 0; x < C; ++x) {
        float gl = 0;                                               // うすいにじみ
        for (int j = -1; j <= 1; ++j) for (int i = -1; i <= 1; ++i) {
          const int xx = x + i, yy = y + j;
          if (xx >= 0 && yy >= 0 && xx < C && yy < C) gl += rnCore[yy * C + xx];
        }
        d[y * C + x] = (uint8_t)(clamp01(fmaxf(rnCore[y * C + x], gl / 9.0f * 0.45f)) * 15.0f + 0.5f);
      }
    if (++prepY >= NG) {
      free(rnCore); rnCore = nullptr; rnCv->deleteSprite(); delete rnCv; rnCv = nullptr; free(rnUniq); rnUniq = nullptr;
      prepPart = 2; prepY = 0;
    }
    return 0;
  }
  if (prepPart == 2) {                             // 菩薩の形(半分の大きさの地図)を1行
    const int M = S / 2, y = prepY;
    for (int x = 0; x < M; ++x) {
      const float u = (2 * x + 1 - Sc) / H2 * 0.80f, v = (2 * y + 1 - Sc) / H2 * 0.80f - 0.16f;
      const float sd = figureSd(u, v);
      float m = clamp01(0.5f - sd / 0.012f);
      const float hr = hypotf(u - HCU, v - HCV);
      if (sd > 0) m = fmaxf(m, 0.9f * expf(-sqf((hr - 0.21f) / 0.012f)));
      rMask[y * M + x] = (uint8_t)(m * 16.0f + 0.5f);
    }
    if (++prepY >= M) { prepPart = 3; prepY = 0; }
    return 0;
  }
  for (int c = 0; c < rCols; ++c) {
    rBase[c] = (int16_t)(esp_random() % (uint32_t)rTL);
    rScroll[c] = 0; rSpeed[c] = frand(5.0f, 14.0f) * rCell / 17.0f;
    makeNulMask(c);
    for (int k = 0; k < 2; ++k) { Drop& d = rDrop[c * 2 + k]; d.head = frand(-1.0f, 1.0f) * rRows; d.speed = frand(6, 15); d.len = frand(8, 26); }
  }
  Serial.printf("sutra rain: %d chars, %d kinds, cell %d px, %dx%d\n", rTL, rNG, rCell, rCols, rRows);
  return 1;
}
static void addRipple(float x, float y, float k) {
  rip[ripNext] = {x, y, simT, k}; ripNext = (ripNext + 1) % NRIP;
}

// ---- 場面2: 分かれる菩薩(null)
// 分身は、上から走査線で現れ(new)、しばらくすると乱れて(グリッチ)、ふちだけの抜けがらになり(null)、消える
//   ・親が null になった分身は「迷子」(暗く、ふらふらする)。ときどき回るゴミ集め(GC)の光に当たると null になる
//   ・分身を長押しすると、それが this になり、親子の線をつなぎ直す
//   ・this からなぞって離すと、離した所に new。振ると NullPointerException で全部 null → まん中から作り直し
static constexpr int NSLOTF = 52;
static constexpr int CENTER = -1;                // まん中の場所(はじめの this)
struct Kid { float born, life, dieT, blipT, parentBorn; int8_t parent; uint8_t alive, dying, newLab; };
static Kid* kid;
static float glitchAll = -10, glitchNext = 8;
static constexpr size_t nmOa64 = nmAlignTo(nmOv64, alignof(float)), nmOv65 = nmOa64 + sizeof(float); static float& spawnClock = *reinterpret_cast<float*>(NM_ARENA + nmOa64);
static int thisId = CENTER;  // いまの this(CENTER か、分身の番号)
static int8_t centerParent = CENTER;  // this が引っ越したあとの、まん中の親
static float thisT0 = -10;  // this が引っ越した時刻
static float gcT0 = -10, gcNext = 10;
static constexpr size_t nmOa65 = nmAlignTo(nmOv65, alignof(float)), nmOv66 = nmOa65 + sizeof(float); static float& fullT = *reinterpret_cast<float*>(NM_ARENA + nmOa65);  // ゴミ集めの光が回りはじめた時刻 / 次 / 満杯が続いている時間
static constexpr size_t nmOa66 = nmAlignTo(nmOv66, alignof(float)), nmOv67 = nmOa66 + sizeof(float); static float& gcAng0 = *reinterpret_cast<float*>(NM_ARENA + nmOa66);
static float npeT0 = -10;  // NullPointerException の時刻
static constexpr size_t nmOa67 = nmAlignTo(nmOv67, alignof(bool)), nmOv68 = nmOa67 + sizeof(bool); static bool& pressOn = *reinterpret_cast<bool*>(NM_ARENA + nmOa67);
static constexpr size_t nmOa68 = nmAlignTo(nmOv68, alignof(bool)), nmOv69 = nmOa68 + sizeof(bool); static bool& pressThis = *reinterpret_cast<bool*>(NM_ARENA + nmOa68);
static constexpr size_t nmOa69 = nmAlignTo(nmOv69, alignof(bool)), nmOv70 = nmOa69 + sizeof(bool); static bool& pressLong = *reinterpret_cast<bool*>(NM_ARENA + nmOa69);
static int pressKid = -2;
static constexpr size_t nmOa70 = nmAlignTo(nmOv70, alignof(float)), nmOv71 = nmOa70 + sizeof(float); static float& pressT0 = *reinterpret_cast<float*>(NM_ARENA + nmOa70);
static constexpr size_t nmOa71 = nmAlignTo(nmOv71, alignof(float)), nmOv72 = nmOa71 + sizeof(float); static float& pressX0 = *reinterpret_cast<float*>(NM_ARENA + nmOa71);
static constexpr size_t nmOa72 = nmAlignTo(nmOv72, alignof(float)), nmOv73 = nmOa72 + sizeof(float); static float& pressY0 = *reinterpret_cast<float*>(NM_ARENA + nmOa72);
static constexpr size_t nmOa73 = nmAlignTo(nmOv73, alignof(float)), nmOv74 = nmOa73 + sizeof(float); static float& pressMove = *reinterpret_cast<float*>(NM_ARENA + nmOa73);
static constexpr size_t nmOa74 = nmAlignTo(nmOv74, alignof(float)), nmOv75 = nmOa74 + sizeof(float); static float& pressX = *reinterpret_cast<float*>(NM_ARENA + nmOa74);
static constexpr size_t nmOa75 = nmAlignTo(nmOv75, alignof(float)), nmOv76 = nmOa75 + sizeof(float); static float& pressY = *reinterpret_cast<float*>(NM_ARENA + nmOa75);
static inline float slotR(int i) { return H2 * (0.30f + 0.60f * sqrtf(i / (float)(NSLOTF - 1))); }
static inline float slotX(int i) { return i < 0 ? Sc : Sc + slotR(i) * cosf(i * 2.39996f + 0.6f); }
static inline float slotY(int i) { return i < 0 ? Sc : Sc + slotR(i) * sinf(i * 2.39996f + 0.6f); }
static inline float slotSize(int i) { return i < 0 ? S * 0.30f : S * (0.115f - 0.055f * i / (float)NSLOTF); }
static constexpr float BORN_T = 0.8f, GLITCH_T = 0.7f, NULL_T = 1.6f;   // 現れる / 乱れる / 抜けがらの時間(秒)
static constexpr float GC_T = 1.6f;                                    // ゴミ集めの光が1周する時間(秒)
static inline bool nodeAlive(int id) { return id == CENTER || (kid[id].alive && !kid[id].dying); }
static inline float distThis(int id) { return hypotf(slotX(id) - slotX(thisId), slotY(id) - slotY(thisId)); }
// 迷子: 親が null になった(か、親の場所に別の分身が生まれた)
static inline bool orphan(int i) {
  const Kid& K = kid[i];
  if (i == thisId || K.parent == CENTER) return false;
  const Kid& P = kid[K.parent];
  return !P.alive || P.dying || P.born != K.parentBorn;
}
static void kidPos(int i, float& x, float& y, float& sz) {
  x = slotX(i); y = slotY(i); sz = slotSize(i);
  if (i >= 0 && orphan(i)) { x += sinf(simT * 2.3f + i) * sz * 0.08f; y += cosf(simT * 1.9f + i * 1.7f) * sz * 0.06f; }   // 迷子はふらふら
}
// id の親を決める: this までの道のりが自分より近い分身(か、まん中 / this)のうち、いちばん近いもの
static int8_t pickParent(int id) {
  const float me = distThis(id);
  int best = thisId; float bd = hypotf(slotX(id) - slotX(thisId), slotY(id) - slotY(thisId));
  for (int j = -1; j < NSLOTF; ++j) {
    if (j == id || j == thisId || !nodeAlive(j) || distThis(j) >= me) continue;
    const float d = hypotf(slotX(j) - slotX(id), slotY(j) - slotY(id));
    if (d < bd) { bd = d; best = j; }
  }
  return (int8_t)best;
}
static void setParent(int i, int8_t p) { kid[i].parent = p; kid[i].parentBorn = p >= 0 ? kid[p].born : 0; }
static void spawnKid(int i) {
  Kid& K = kid[i];
  K.alive = 1; K.dying = 0; K.born = simT; K.blipT = -10;
  setParent(i, pickParent(i));
  K.life = 20.0f - 12.0f * i / (float)NSLOTF + frand(0, 5);       // 外側ほど早く null になる
  K.newLab = (i % 3 == 0) ? 1 : 0;
}
static void killKid(int i) {
  Kid& K = kid[i];
  if (!K.alive || K.dying || i == thisId) return;
  K.dying = 1; K.dieT = simT;
}
// this を引っ越す(id = 分身の番号 か CENTER)。親子の線を、新しい this を根にしてつなぎ直す
static void moveThis(int id) {
  if (id == thisId) return;
  if (thisId >= 0) { kid[thisId].born = simT - BORN_T; kid[thisId].life = 12.0f + frand(0, 6); }   // 前の this は、ふつうの分身に
  thisId = id; thisT0 = simT;
  int ord[NSLOTF], n = 0;                                             // this に近い順につなぎ直す
  for (int i = 0; i < NSLOTF; ++i) if (i != thisId && kid[i].alive && !kid[i].dying) ord[n++] = i;
  for (int a = 1; a < n; ++a) { const int v = ord[a]; int b = a; while (b > 0 && distThis(ord[b - 1]) > distThis(v)) { ord[b] = ord[b - 1]; --b; } ord[b] = v; }
  for (int a = 0; a < n; ++a) setParent(ord[a], pickParent(ord[a]));
  centerParent = thisId == CENTER ? CENTER : pickParent(CENTER);
  Serial.printf("this -> %d\n", id);
}
// ゴミ集め: 光が1周するあいだに、迷子を null にする。迷子がいないのに満杯なら、古いものから5つ
static void startGC() {
  if (simT - gcT0 < GC_T) return;
  gcT0 = simT; gcAng0 = frand(0, 6.2832f); gcNext = simT + frand(9, 14);
  int orph = 0;
  for (int i = 0; i < NSLOTF; ++i) if (kid[i].alive && !kid[i].dying && orphan(i)) ++orph;
  if (orph == 0) {
    int alive = 0;
    for (int i = 0; i < NSLOTF; ++i) if (kid[i].alive) ++alive;
    if (alive >= NSLOTF - 1)
      for (int r = 0; r < 5; ++r) {                                    // 古いものから、迷子にする(親を切る)
        int best = -1; float bt = 1e9f;
        for (int i = 0; i < NSLOTF; ++i) if (kid[i].alive && !kid[i].dying && i != thisId && kid[i].parent != -2 && kid[i].born < bt) { bt = kid[i].born; best = i; }
        if (best < 0) break;
        kid[best].parent = (int8_t)best; kid[best].parentBorn = -1;    // 自分を親にして、つながりを切る(迷子になる)
      }
  }
}
// NullPointerException: ぜんぶ null にして、this から作り直す
static void npe() {
  npeT0 = simT;
  for (int i = 0; i < NSLOTF; ++i) if (i != thisId) killKid(i);
  glitchAll = simT;
  Serial.println("NullPointerException");
}

// ---- 場面3: 映る菩薩(本体を傾けると、合わせ鏡の奥がずれる)
static float goldT0 = -10;
static constexpr size_t nmOa76 = nmAlignTo(nmOv76, alignof(float)), nmOv77 = nmOa76 + sizeof(float); static float& mirShift = *reinterpret_cast<float*>(NM_ARENA + nmOa76);
static constexpr size_t nmOa77 = nmAlignTo(nmOv77, alignof(float)), nmOv78 = nmOa77 + sizeof(float); static float& mirShiftV = *reinterpret_cast<float*>(NM_ARENA + nmOa77);
static constexpr size_t nmOa78 = nmAlignTo(nmOv78, alignof(float)), nmOv79 = nmOa78 + sizeof(float); static float& mirTX = *reinterpret_cast<float*>(NM_ARENA + nmOa78);
static constexpr size_t nmOa79 = nmAlignTo(nmOv79, alignof(float)), nmOv80 = nmOa79 + sizeof(float); static float& mirTY = *reinterpret_cast<float*>(NM_ARENA + nmOa79);
static constexpr size_t nmOa80 = nmAlignTo(nmOv80, alignof(float)), nmOv81 = nmOa80 + sizeof(float); static float& tiltX = *reinterpret_cast<float*>(NM_ARENA + nmOa80);
static constexpr size_t nmOa81 = nmAlignTo(nmOv81, alignof(float)), nmOv82 = nmOa81 + sizeof(float); static float& tiltY = *reinterpret_cast<float*>(NM_ARENA + nmOa81);
static constexpr size_t nmOa82 = nmAlignTo(nmOv82, alignof(float)), nmOv83 = nmOa82 + sizeof(float); static float& levelX = *reinterpret_cast<float*>(NM_ARENA + nmOa82);
static constexpr size_t nmOa83 = nmAlignTo(nmOv83, alignof(float)), nmOv84 = nmOa83 + sizeof(float); static float& levelY = *reinterpret_cast<float*>(NM_ARENA + nmOa83);  // 傾き(なめらかにしたもの)と「水平」の基準
static constexpr size_t nmOa84 = nmAlignTo(nmOv84, alignof(int)), nmOv85 = nmOa84 + sizeof(int); static int& levelN = *reinterpret_cast<int*>(NM_ARENA + nmOa84);
static constexpr size_t nmOa85 = nmAlignTo(nmOv85, alignof(bool)), nmOv86 = nmOa85 + sizeof(bool); static bool& imuOK = *reinterpret_cast<bool*>(NM_ARENA + nmOa85);
static constexpr size_t nmOa86 = nmAlignTo(nmOv86, alignof(int)), nmOv87 = nmOa86 + sizeof(int); static int& bgOX = *reinterpret_cast<int*>(NM_ARENA + nmOa86);
static constexpr size_t nmOa87 = nmAlignTo(nmOv87, alignof(int)), nmOv88 = nmOa87 + sizeof(int); static int& bgOY = *reinterpret_cast<int*>(NM_ARENA + nmOa87);  // 背景をずらす量(映る菩薩で傾けたとき)

// 場面1・3の背景は、画素ごとに「まん中からの近さ(6ビット) × 光の強さ(8ビット、平方根で)」の番号を持っておき、
// 色の表(番号 → 画面の色)を引いて塗る。色を変えたときは、表を作り直して塗り直すだけ
static constexpr int LUTA = 64, LUTB = 256;
static constexpr float MIR_WMAX = 3.0f;
static uint16_t* scnIdx[4];              // [場面][S*S] 番号(場面1・3だけ)
static uint16_t* scnLut;                 // [LUTA*LUTB] 色の表(今の場面のぶん)
static int8_t lutFor = -1;  // 色の表がどの場面のものか
static uint16_t* scnCur;                 // [S*S] 今の場面(1・3)の背景(バイト順入れ替え済み)
static int curFillY = 1 << 30;  // 色を変えたあと、ここから下をまだ塗っていない
static void curFill(int y0, int y1) {
  const uint16_t* I = scnIdx[lutFor] + (size_t)y0 * S;
  const uint16_t* L = scnLut;
  uint16_t* D = scnCur + (size_t)y0 * S;
  const size_t n = (size_t)(y1 - y0) * S;
  for (size_t i = 0; i < n; ++i) D[i] = L[I[i]];
}
static constexpr size_t nmOa88 = nmAlignTo(nmOv88, 16), nmOv89 = nmOa88 + sizeof(bool[4]); static bool (&scnReady)[4] = *reinterpret_cast<bool(*)[4]>(NM_ARENA + nmOa88);  // 背景を描き終えたか
static inline uint16_t lutIdx(float f, float v) {   // f: まん中からの近さ 0..1 / v: 光の強さ 0..1
  return (uint16_t)(((int)lrintf(clamp01(f) * (LUTA - 1)) << 8) | (int)lrintf(sqrtf(clamp01(v)) * (LUTB - 1)));
}
static void sceneLut(int sc) {
  const Theme& th = THEMES[theme];
  uint16_t* L = scnLut;
  lutFor = (int8_t)sc;
  const Col fc = lerpC(th.warm, th.mid, 0.5f);
  for (int a = 0; a < LUTA; ++a) {
    const float f = a / (float)(LUTA - 1);
    for (int b = 0; b < LUTB; ++b) {
      const float t = b / (float)(LUTB - 1), v = t * t;
      Col o;
      if (sc == 1) { const float ff = f * 0.5f, g = v * 0.16f;
        o = {th.bg0.r * 0.5f + th.bg1.r * ff * ff + fc.r * g, th.bg0.g * 0.5f + th.bg1.g * ff * ff + fc.g * g, th.bg0.b * 0.5f + th.bg1.b * ff * ff + fc.b * g}; }
      else { const float w = v * MIR_WMAX;
        o = {th.bg0.r * 0.6f + th.bg1.r * 0.6f * f + th.cool.r * w, th.bg0.g * 0.6f + th.bg1.g * 0.6f * f + th.cool.g * w, th.bg0.b * 0.6f + th.bg1.b * 0.6f * f + th.cool.b * w}; }
      L[a * LUTB + b] = toPix(o);
    }
  }
}
static float* scBl = nullptr;                      // 場面1の準備のあいだだけ使う(にじみ)
static float scFc[13], scFs[13], scFsz[13], scFw[13];   // 場面3の鏡の枠
static int sceneStep(int sc) {                     // 場面の背景を1行。1: 終わり / 0: まだ / -1: メモリが足りない
  uint16_t* img = scnBuf[2];
  if (sc == 3) {                                                     // 奥へ続く鏡の枠(枠からの距離で、線とにじみを1回で計算する)
    constexpr int NF = 13;
    if (prepPart == 0) {
      for (int k = 1; k <= NF; ++k) {
        const float a = k * 0.07f + 0.7854f, f = powf(0.90f, (float)k);
        scFc[k - 1] = cosf(a - 0.7854f); scFs[k - 1] = sinf(a - 0.7854f); scFsz[k - 1] = powf(0.90f, (float)k) * 1.30f;
        scFw[k - 1] = 0.25f + 0.55f * f;
      }
      prepPart = 1; prepY = 0;
      return 0;
    }
    const float px = 1.0f / H2;
    const float* fc = scFc; const float* fs = scFs; const float* fsz = scFsz; const float* fw = scFw;
    uint16_t* idx = scnIdx[3];
    const int y = prepY;
    const float v = (y - Sc) / H2 - MIR_VY;
    for (int x = 0; x < S; ++x) {
      const float u = (x - Sc) / H2;
      const float r = hypotf(u, v), f = clamp01(1.0f - r);
      float wsum = 0;
      for (int k = 0; k < NF; ++k) {
        const float ru = u * fc[k] + v * fs[k], rv = -u * fs[k] + v * fc[k];
        const float d = fabsf(fmaxf(fabsf(ru), fabsf(rv)) - fsz[k]) / px;   // 枠からの距離(ピクセル)
        if (d > 14) continue;
        wsum += fw[k] * (clamp01(1.2f - d * 0.8f) + 0.30f / (1.0f + d * d * 0.12f));
      }
      idx[(size_t)y * S + x] = lutIdx(f, wsum / MIR_WMAX);
    }
    if (++prepY >= S) { scnReady[3] = true; return 1; }
    return 0;
  }
  if (sc == 1) {                                                     // 暗い空と、菩薩の形のうすい光(文字は毎コマ上に描く)
    const int M = S / 2;
    if (prepPart == 0) {                                             // はじめに、菩薩の形をぼかして、にじみにする
      float* bl = (float*)ps_malloc(sizeof(float) * M * M);
      float* tm = (float*)ps_malloc(sizeof(float) * M * M);
      if (!bl || !tm) { free(bl); free(tm); return -1; }
      for (int i = 0; i < M * M; ++i) bl[i] = rMask[i] / 16.0f;
      const int R = max(2, M / 40);
      for (int pass = 0; pass < 2; ++pass) {
        for (int y = 0; y < M; ++y) { float acc = 0; for (int x = -R; x <= R; ++x) acc += bl[y * M + min(max(x, 0), M - 1)];
          for (int x = 0; x < M; ++x) { tm[y * M + x] = acc / (2 * R + 1); acc += bl[y * M + min(x + R + 1, M - 1)] - bl[y * M + max(x - R, 0)]; } }
        for (int x = 0; x < M; ++x) { float acc = 0; for (int y = -R; y <= R; ++y) acc += tm[min(max(y, 0), M - 1) * M + x];
          for (int y = 0; y < M; ++y) { bl[y * M + x] = acc / (2 * R + 1); acc += tm[min(y + R + 1, M - 1) * M + x] - tm[max(y - R, 0) * M + x]; } }
      }
      free(tm); scBl = bl;
      prepPart = 1; prepY = 0;
      return 0;
    }
    uint16_t* idx = scnIdx[1];
    const int y = prepY;
    for (int x = 0; x < S; ++x) {
      const float r = hypotf(x - Sc, y - Sc) / H2;
      idx[(size_t)y * S + x] = lutIdx(1.0f - r, scBl[min(M - 1, y / 2) * M + min(M - 1, x / 2)]);
    }
    if (++prepY >= S) { free(scBl); scBl = nullptr; scnReady[1] = true; return 1; }
    return 0;
  }
  if (prepPart == 0) {                                               // まっくら。うすい走査線と、遠くの小さな点(色を変えても同じ)
    const int y = prepY;
    for (int x = 0; x < S; ++x) {
      const float r = hypotf(x - Sc, y - Sc) / H2, f = clamp01(1.0f - r) * 0.35f, ln = (y % 3 == 0) ? 0.018f : 0.0f;
      img[(size_t)y * S + x] = toPix({0.012f + 0.05f * f * f + ln, 0.012f + 0.05f * f * f + ln, 0.016f + 0.06f * f * f + ln});
    }
    if (++prepY >= S) { prepPart = 1; prepY = 0; }
    return 0;
  }
  rs = 4242;
  for (int i = 0; i < 160; ++i) {
    const float a = srand01() * 6.2832f, r = sqrtf(srand01()) * 0.95f;
    const int x = (int)PX(r * cosf(a)), y = (int)PX(r * sinf(a));
    if (x >= 0 && y >= 0 && x < S && y < S) img[(size_t)y * S + x] = toPix(mulC(C8(220, 225, 240), 0.15f + srand01() * 0.35f));
  }
  scnReady[2] = true;
  return 1;
}
static void fail(const char* msg);
// ---- 読み込み中の表示(読み込みの段ごとに、輪を少しずつ伸ばす)
// 画面外の絵に描いてから、まとめて送る(画面へ直接送るときは、横の位置と幅を2の倍数にそろえないと崩れる)
static M5Canvas* loadCv = nullptr;
static void loadingShow(int step, int total) {
  const int w = M5.Display.width(), h = M5.Display.height(), cx = w / 2, cy = h / 2;
  const int R = min(w, h) * 0.22f;
  // 絵の範囲: 輪と、その下の「読み込み中」まで(左上・幅・高さを偶数に)
  const int bx0 = max(0, cx - R - 8) & ~1, by0 = max(0, cy - R - 8) & ~1;
  const int bw = (min(w, cx + R + 8) - bx0 + 1) & ~1, bh = (min(h, cy + R + 44) - by0 + 1) & ~1;
  if (step == 0) {
    M5.Display.fillScreen(TFT_BLACK);
    if (!loadCv) {
      loadCv = new M5Canvas(&M5.Display);
      loadCv->setPsram(true);
      loadCv->setColorDepth(16);
      if (!loadCv->createSprite(bw, bh)) { delete loadCv; loadCv = nullptr; }
    }
  }
  if (!loadCv) return;                                                 // 絵の置き場が取れなければ、何も出さない
  M5Canvas& c = *loadCv;
  const int ox = cx - bx0, oy = cy - by0;
  if (step == 0) {
    c.fillScreen(TFT_BLACK);
    c.setTextDatum(middle_center);
    c.setFont(&fonts::lgfxJapanMincho_20);
    c.setTextColor(M5.Display.color565(235, 210, 150));
    c.setTextSize(min(w, h) >= 400 ? 1.6f : 1.2f);
    c.drawString("null菩薩", ox, oy - 8);
    c.setTextSize(1.0f);
    c.setTextColor(M5.Display.color565(120, 126, 142));
    c.drawString("読み込み中", ox, oy + R + 26);
    for (int a = 0; a < 120; ++a) {
      const float t = a * 6.2832f / 120 - 1.5708f;
      c.fillCircle(ox + (int)(R * cosf(t)), oy + (int)(R * sinf(t)), 2, M5.Display.color565(40, 42, 54));
    }
  }
  const int a0 = step * 120 / total, a1 = (step + 1) * 120 / total;
  for (int a = a0; a < a1; ++a) {
    const float t = a * 6.2832f / 120 - 1.5708f;
    c.fillCircle(ox + (int)(R * cosf(t)), oy + (int)(R * sinf(t)), 3, M5.Display.color565(255, 200, 110));
  }
  c.pushSprite(bx0, by0);
  if (step >= total - 1) { delete loadCv; loadCv = nullptr; }          // 読み込みが終わったら、絵の置き場を返す
}
// 場面1〜3の準備: 1コマに1つずつ(0: 菩薩の点の絵 / 1: 般若心経の文字 / 2〜4: 場面1〜3の背景)
static constexpr size_t nmOa89 = nmAlignTo(nmOv89, alignof(int)), nmOv90 = nmOa89 + sizeof(int); static int& prepStage = *reinterpret_cast<int*>(NM_ARENA + nmOa89);
static constexpr size_t nmOa90 = nmAlignTo(nmOv90, 16), nmOv91 = nmOa90 + sizeof(uint32_t[5]); static uint32_t (&prepMs)[5] = *reinterpret_cast<uint32_t(*)[5]>(NM_ARENA + nmOa90);
static constexpr int NPREP = 5;
static int pendScene = -1;                         // 準備が終わるのを待っている場面(-1: なし)
static bool prepDone[NPREP];                       // 終わった準備の段
static int prepCur = -1;                           // いま進めている段(-1: なし)
static int stM[NPREP], stY[NPREP], stPart[NPREP];  // 段ごとの進み具合(場面を変えたとき、その場面に要る段を先にするため)
static uint32_t stWork[NPREP];
static int sceneNeeds(int sc) {                    // 場面に要る準備の段(ビット)。1: 文字 = 般若心経・背景1 / 2: 分身 = 点の絵・背景2 / 3: 鏡 = 点の絵・背景3
  return sc == 1 ? 0b00110 : sc == 2 ? 0b01001 : sc == 3 ? 0b10001 : 0;
}
static bool sceneReady(int sc) {
  const int need = sceneNeeds(sc);
  for (int i = 0; i < NPREP; ++i) if ((need >> i & 1) && !prepDone[i]) return false;
  return true;
}
static int prepPick() {                            // 次に進める段: 待っている場面に要る段を先に。なければ順番に
  if (prepCur >= 0) {
    if (pendScene < 0 || (sceneNeeds(pendScene) >> prepCur & 1)) return prepCur;
  }
  if (pendScene >= 0) { const int need = sceneNeeds(pendScene); for (int i = 0; i < NPREP; ++i) if ((need >> i & 1) && !prepDone[i]) return i; }
  if (prepCur >= 0) return prepCur;
  for (int i = 0; i < NPREP; ++i) if (!prepDone[i]) return i;
  return -1;
}
static bool prepUnit() {                           // 準備を1つぶん(1行・1文字)だけ進める
  if (prepStage >= NPREP) return true;
  const int st = prepPick();
  if (st < 0) return true;
  if (st != prepCur) {                             // 段を切りかえる: 今の段の進み具合をしまい、次の段のを出す
    if (prepCur >= 0) { stM[prepCur] = prepM; stY[prepCur] = prepY; stPart[prepCur] = prepPart; stWork[prepCur] = prepWork; }
    prepCur = st; prepM = stM[st]; prepY = stY[st]; prepPart = stPart[st]; prepWork = stWork[st];
  }
  const uint32_t t0 = micros();
  int r;
  if (st == 0) r = figStep() ? 1 : 0;
  else if (st == 1) r = rainStep();
  else r = sceneStep(st - 1);
  prepWork += micros() - t0;
  if (r < 0) fail("Memory error");
  if (r > 0) { prepMs[st] = prepWork / 1000; prepWork = 0; prepDone[st] = true; ++prepStage; prepCur = -1; prepM = prepY = prepPart = 0; }
  return prepStage >= NPREP;
}
static void prepRun(uint32_t budgetUs) {           // 決めた時間まで、準備を少しずつ進める(画面は止めない)
  const uint32_t t0 = micros();
  do { prepUnit(); } while (prepStage < NPREP && micros() - t0 < budgetUs && !(pendScene >= 0 && sceneReady(pendScene)));
}
static bool prepOne() {                            // 準備の段を1つ、まるごと
  const int s0 = prepStage;
  while (prepStage == s0 && prepStage < NPREP) prepUnit();
  return prepStage >= NPREP;
}
static float prepProgress() {                      // 待っている場面の準備が、どこまで進んだか(0..1)
  const int need = pendScene >= 0 ? sceneNeeds(pendScene) : 0b11111;
  int n = 0, done = 0;
  for (int i = 0; i < NPREP; ++i) if (need >> i & 1) { ++n; if (prepDone[i]) ++done; }
  float f = 0;
  if (prepCur >= 0 && (need >> prepCur & 1)) {
    if (prepCur == 0) f = (prepM + (float)prepY / FST[prepM < NFS ? prepM : NFS - 1]) / NFS;
    else if (prepCur == 1) f = prepPart * 0.3f;
    else f = (float)prepY / S;
  }
  return n ? (done + clamp01(f)) / n : 1.0f;
}
static void finishPrep() {                                           // 起動のとき、最初の場面に要る準備を、ここで済ませる
  if (prepStage >= NPREP) return;
  const int s0 = prepStage;
  loadingShow(0, NPREP - s0);
  while (prepStage < NPREP) { const int i = prepStage - s0; prepOne(); loadingShow(i, NPREP - s0); }
}


// 色を変えたあとの、場面0の背景の描き直し(1コマに少しずつ。そのあいだも動き続ける)
static constexpr size_t nmOa91 = nmAlignTo(nmOv91, alignof(bool)), nmOv92 = nmOa91 + sizeof(bool); static bool& tjOn = *reinterpret_cast<bool*>(NM_ARENA + nmOa91);
static constexpr size_t nmOa92 = nmAlignTo(nmOv92, alignof(uint32_t)), nmOv93 = nmOa92 + sizeof(uint32_t); static uint32_t& tjT0 = *reinterpret_cast<uint32_t*>(NM_ARENA + nmOa92);
static void themeStep() {
  bool done = false;
  if (!sj) {
    if (!statBegin()) { Serial.println("theme: not enough memory"); done = true; }
    else { if (sj->inPlace) Serial.println("theme: not enough memory for a second picture, drawing over the current one"); return; }
  }
  if (!done) done = statStep();
  if (done) {
    tjOn = false;
    Serial.printf("theme: %s  redrawn in %u ms (without stopping)  PSRAM free %u KB\n", THEMES[theme].name, (unsigned)(millis() - tjT0), psFreeKB());
  }
}
static void themeWork() {                                            // 並べられない段だけ、ここで進める(12ms まで)
  const uint32_t t0 = micros();
  for (int n = 0; n < 8 && tjOn; ++n) {
    if (sj && statParOK(sj->st)) break;
    themeStep();
    if (micros() - t0 > 12000) break;
  }
}

static void setScene(int sc);
static void requestScene(int sc) {                 // 場面を変える。準備がまだなら、止めずに急いで進めて、終わったら変える
  if (sceneReady(sc)) { pendScene = -1; setScene(sc); return; }
  pendScene = sc;
  Serial.printf("scene: %d (preparing %d%%)\n", sc, (int)(prepProgress() * 100.0f));
}
static void setScene(int sc) {
  if (sc != 0 && !sceneReady(sc)) finishPrep();
  if (sc == 1 || sc == 3) {                                          // 今の場面の背景を、番号の地図と色の表から塗る
    if (lutFor != sc) sceneLut(sc);
    curFill(0, S); curFillY = S;
  }
  scene = sc;
  for (int i = 0; i < MAXPART; ++i) part[i].life = 0;
  for (int r = 0; r < NRING; ++r) ring[r].t0 = -100;
  if (sc == 1) { for (int i = 0; i < NRIP; ++i) rip[i].t0 = -100; }
  if (sc == 2) { for (int i = 0; i < NSLOTF; ++i) kid[i].alive = 0; spawnClock = 0; glitchNext = simT + 6; thisId = CENTER; centerParent = CENTER; gcNext = simT + 10; pressOn = false; npeT0 = -10; }
  if (sc == 3) { mirShift = 0; mirShiftV = 0; goldT0 = -10; levelN = 0; }   // 今の持ち方を「水平」に
  Serial.printf("scene: %d %s\n", sc, SCENE_NAME[sc]);
}

// 点線(親子の線)。frac は親の側から何割まで引くか
static void dotLine(float x0, float y0, float x1, float y1, Col c, float k, float frac = 1.0f) {
  const float L = hypotf(x1 - x0, y1 - y0);
  const int n = (int)(L / (5.0f * S / 466.0f));
  for (int i = 1; i < n; ++i) { const float t = i / (float)n; if (t > frac) break; addPD(x0 + (x1 - x0) * t, y0 + (y1 - y0) * t, c, k); }
}

static void updateScene(const Input& in, float dt) {
  const Theme& th = THEMES[theme];
  nSpr = 0; nLab = 0; nPD = 0; ++gFrame; bgOX = 0; bgOY = 0;
  if (scene == 1) {
    // 文字は列ごとにゆっくり下へ流れる(1字ぶん流れたら、上に新しい字が入る)
    for (int c = 0; c < rCols; ++c) {
      rScroll[c] += rSpeed[c] * dt;
      while (rScroll[c] >= rCell) { rScroll[c] -= rCell; if (--rBase[c] < 0) rBase[c] += rTL; }
      rScrollI[c] = (int16_t)rScroll[c];
    }
    // 落ちてくる光
    for (int c = 0; c < rCols; ++c)
      for (int k = 0; k < 2; ++k) {
        Drop& d = rDrop[c * 2 + k];
        d.head += d.speed * dt;
        if (d.head - d.len > rRows) { d.head = -frand(0, rRows * 0.6f); d.speed = frand(6, 15); d.len = frand(8, 26); }
      }
    // 触ると波紋。なぞると、なぞった道すじから次々に
    if (in.tapped) { addRipple(in.tx, in.ty, 1.0f); ripLast = simT; }
    else if (in.touching && simT - ripLast > 0.14f) { addRipple(in.tx, in.ty, 0.55f); ripLast = simT; }
    const Col rain = th.mid, head = lerpC(th.mid, C8(255, 255, 255), 0.75f);
    const Col fig = lerpC(th.warm, C8(255, 255, 255), 0.1f), wave = lerpC(th.warm, C8(255, 255, 255), 0.5f);
    const float spd = 0.55f * S, wid = 0.06f * S;
    for (int r = 0; r < rRows; ++r) {
      const float cy = r * rCell + rCell * 0.5f;
      for (int c = 0; c < rCols; ++c) {
        float k = 0, hd = 0;
        for (int q = 0; q < 2; ++q) {
          const Drop& d = rDrop[c * 2 + q];
          const float dr = d.head - r;
          if (dr < 0 || dr >= d.len) continue;
          if (dr < 1.0f) { hd = 1.0f; k = 1.0f; } else k = fmaxf(k, 0.85f * (1.0f - dr / d.len));
        }
        const float cx = rX0 + c * rCell + rCell * 0.5f;
        float wv = 0, dx = 0;
        for (int i = 0; i < NRIP; ++i) {
          const float age = simT - rip[i].t0;
          if (age < 0 || age > 2.2f) continue;
          const float d = hypotf(cx - rip[i].x, cy - rip[i].y), e = (d - age * spd) / wid;
          if (e < -3 || e > 3) continue;
          const float a = rip[i].k * expf(-age * 0.9f) * expf(-e * e);
          wv += a; dx += a * sinf(e * 2.0f) * 9.0f;
        }
        wv = fminf(1.0f, wv);
        const int i = r * rCols + c;
        rDx[i] = (int8_t)(dx > 9 ? 9 : (dx < -9 ? -9 : dx));
        Col o = lerpC(mulC(rain, 0.06f + 0.95f * k), head, hd);
        Col n = lerpC(mulC(fig, 0.62f + 0.7f * k), C8(255, 250, 235), hd);
        o = lerpC(o, mulC(wave, 1.2f), wv); n = lerpC(n, C8(255, 255, 255), wv * 0.8f);
        rCOut[i] = to565(o); rCIn[i] = to565(n);
      }
    }
  } else if (scene == 2) {
    // 色: 分身は白に近い色、this は差し色(色を変えると、差し色が変わる。白銀はほぼ白黒)
    const Col cKid = lerpC(C8(238, 240, 246), th.mid, 0.22f), cFar = lerpC(lerpC(th.cool, C8(150, 150, 165), 0.6f), th.cool, 0.25f);
    const Col cThis = th.warm, cLine = lerpC(C8(110, 115, 130), th.cool, 0.25f);
    const float ne = simT - npeT0;
    const bool npeOn = ne >= 0 && ne < 2.6f;                          // NullPointerException のあいだ(作り直すまで)
    if (in.shake && !npeOn) npe();
    spawnClock += dt;
    int alive = 0, orph = 0;
    for (int i = 0; i < NSLOTF; ++i) {
      Kid& K = kid[i];
      if (!K.alive) continue;
      if (K.dying && simT - K.dieT > GLITCH_T + NULL_T) {             // 抜けがらも消える(小さな粒に)
        K.alive = 0;
        float x, y, sz; kidPos(i, x, y, sz);
        for (int n = 0; n < 14; ++n) { const float a = frand(0, 6.2832f), sp = frand(0.03f, 0.18f) * H2;
          emitPart(x + frand(-0.25f, 0.25f) * sz, y + frand(-0.35f, 0.35f) * sz, cosf(a) * sp, sinf(a) * sp, cKid, frand(0.6f, 1.6f)); }
        continue;
      }
      if (K.dying) continue;
      ++alive;
      if (orphan(i)) ++orph;
      if (i != thisId && simT - K.born > K.life) killKid(i);
      if (simT - K.born > BORN_T && frand() < 0.004f) K.blipT = simT;   // ときどき、一瞬乱れる
    }
    if (!npeOn || ne > 1.2f)                                         // 新しい分身(NullPointerException のあとは、少し待ってから)
      if (spawnClock > (npeOn ? 0.12f : 0.35f)) {
        spawnClock = 0;
        int best = -1; float bd = 1e9f;                              // this に近い空き場所から
        for (int i = 0; i < NSLOTF; ++i) if (!kid[i].alive) { const float d = distThis(i); if (d < bd) { bd = d; best = i; } }
        if (best >= 0) spawnKid(best);
      }
    if (simT > glitchNext) { glitchAll = simT; glitchNext = simT + frand(7, 13); }   // ときどき、ぜんぶが乱れる
    // ゴミ集め: ときどき / 満杯が続いたとき
    fullT = (alive >= NSLOTF) ? fullT + dt : 0;
    if (simT > gcNext || fullT > 1.5f) { startGC(); fullT = 0; }
    const float gce = simT - gcT0;
    float gcA = -100;
    if (gce >= 0 && gce < GC_T) {
      gcA = gcAng0 + 6.2832f * gce / GC_T;
      for (int i = 0; i < NSLOTF; ++i) {
        if (!kid[i].alive || kid[i].dying || !orphan(i)) continue;
        float a = atan2f(slotY(i) - Sc, slotX(i) - Sc) - gcAng0;
        while (a < 0) a += 6.2832f;
        if (a <= 6.2832f * gce / GC_T) killKid(i);
      }
    }
    // 指: 長押しで this を引っ越す / this からなぞって離すと new / 短いタップはその近くに new
    if (in.touching) {
      if (!pressOn) {
        pressOn = true; pressT0 = simT; pressX0 = in.tx; pressY0 = in.ty; pressMove = 0; pressLong = false;
        pressThis = hypotf(in.tx - slotX(thisId), in.ty - slotY(thisId)) < slotSize(thisId) * 0.45f;
        pressKid = -2; float bd = 1e9f;
        for (int i = -1; i < NSLOTF; ++i) {
          if (i == thisId || !nodeAlive(i)) continue;
          const float d = hypotf(in.tx - slotX(i), in.ty - slotY(i));
          if (d < slotSize(i) * 0.5f && d < bd) { bd = d; pressKid = i; }
        }
      }
      pressX = in.tx; pressY = in.ty;
      pressMove = fmaxf(pressMove, hypotf(in.tx - pressX0, in.ty - pressY0));
      if (!pressLong && pressKid >= -1 && !pressThis && pressMove < S * 0.04f && simT - pressT0 > 0.6f && nodeAlive(pressKid)) {
        pressLong = true; moveThis(pressKid);
        ring[ringNext] = {slotX(pressKid), slotY(pressKid), simT, 0.9f}; ringNext = (ringNext + 1) % NRING;
      }
    } else if (pressOn) {
      pressOn = false;
      if (pressThis && pressMove > S * 0.12f) {                       // this からなぞった → 離した所に new
        int best = -1; float bd = 1e9f;
        for (int i = 0; i < NSLOTF; ++i) { if (kid[i].alive) continue; const float d = hypotf(slotX(i) - pressX, slotY(i) - pressY); if (d < bd) { bd = d; best = i; } }
        if (best >= 0) { spawnKid(best); setParent(best, (int8_t)thisId); kid[best].newLab = 1; ring[ringNext] = {slotX(best), slotY(best), simT, 0.6f}; ringNext = (ringNext + 1) % NRING; }
      } else if (!pressLong && pressMove < S * 0.04f && simT - pressT0 < 0.4f) {   // 短いタップ: 近くに3体。空きがなければ、近くの分身が null に
        ring[ringNext] = {pressX0, pressY0, simT, 0.6f}; ringNext = (ringNext + 1) % NRING;
        int made = 0;
        for (int rep = 0; rep < 3; ++rep) {
          int best = -1; float bd = 1e9f;
          for (int i = 0; i < NSLOTF; ++i) { if (kid[i].alive) continue; const float d = hypotf(slotX(i) - pressX0, slotY(i) - pressY0); if (d < bd) { bd = d; best = i; } }
          if (best < 0) break;
          spawnKid(best); kid[best].newLab = 1; ++made;
        }
        if (!made) {
          int best = -1; float bd = 1e9f;
          for (int i = 0; i < NSLOTF; ++i) { if (!kid[i].alive || kid[i].dying || i == thisId) continue; const float d = hypotf(slotX(i) - pressX0, slotY(i) - pressY0); if (d < bd) { bd = d; best = i; } }
          if (best >= 0) killKid(best);
        }
      }
    }
    const float ga = simT - glitchAll, gAll = (ga >= 0 && ga < 0.35f) ? 1.0f - ga / 0.35f : 0.0f;
    // 線(null・迷子への線は、とちゅうで切れる)
    for (int i = 0; i < NSLOTF; ++i) {
      const Kid& K = kid[i];
      if (!K.alive || i == thisId) continue;
      float x, y, sz; kidPos(i, x, y, sz);
      const int p = K.parent;
      float px = slotX(p >= 0 ? p : CENTER), py = slotY(p >= 0 ? p : CENTER);
      if (orphan(i)) { dotLine(x, y, px, py, cLine, 0.35f, 0.3f); continue; }   // 迷子: 自分の側から少しだけ
      const float bt = smooth3((simT - K.born) / BORN_T);
      const float frac = K.dying ? 0.45f * (1.0f - smooth3((simT - K.dieT) / (GLITCH_T + NULL_T))) : bt;
      dotLine(px, py, x, y, cLine, 0.6f, frac);
    }
    if (thisId != CENTER) dotLine(slotX(centerParent), slotY(centerParent), Sc, Sc, cLine, 0.6f);   // まん中も、ふつうの分身としてつながる
    // ゴミ集めの光(まん中から外へ、回る線)
    if (gcA > -50) {
      for (int tr = 0; tr < 4; ++tr) {
        const float a = gcA - tr * 0.06f, ca = cosf(a), sa = sinf(a), k = 1.0f - tr * 0.24f;
        for (float r = slotSize(CENTER) * 0.35f; r < H2 * 0.97f; r += 4.0f * S / 466.0f) addPD(Sc + ca * r, Sc + sa * r, lerpC(cThis, C8(255, 255, 255), 0.4f), k * 0.8f);
      }
      addLab(Sc, Sc - slotSize(CENTER) * 0.62f, "GC", cThis, 1.0f - gce / GC_T);
    }
    // 分身
    for (int i = NSLOTF - 1; i >= -1; --i) {
      const bool isThis = (i == thisId);
      if (i >= 0 && !kid[i].alive) continue;
      float x, y, sz; kidPos(i, x, y, sz);
      if (i == CENTER && !isThis) sz *= 0.75f;
      if (isThis && i >= 0) sz = fmaxf(sz, S * 0.16f);
      const float t = i < 0 ? 0.0f : i / (float)NSLOTF;
      Col c = isThis ? cThis : lerpC(cKid, cFar, t);
      float k = isThis ? 1.1f : 1.0f - 0.45f * t;
      int gl = (int)(gAll * (isThis ? 110 : 200));
      uint8_t rev = 255, outl = 0;
      const char* tag = nullptr; float tk = 0;
      if (i >= 0 && !isThis) {
        const Kid& K = kid[i];
        const float age = simT - K.born;
        if (K.dying) {
          const float e = simT - K.dieT;
          if (e < GLITCH_T) { gl = max(gl, (int)(60 + 195 * e / GLITCH_T)); if ((gFrame + i) % 3 == 0) k *= 0.4f; }
          else { const float f = (e - GLITCH_T) / NULL_T; outl = 1; k *= 0.75f * (1.0f - f) * (((gFrame + i) % 5) ? 1.0f : 0.3f); tag = "null"; tk = 1.0f - f; }
        } else if (age < BORN_T) {
          rev = (uint8_t)(255 * smooth3(age / BORN_T)); gl = max(gl, (int)(140 * (1.0f - age / BORN_T)));
          if (K.newLab) { tag = "new"; tk = 1.0f; }
        } else {
          const float b = simT - K.blipT;
          if (b >= 0 && b < 0.18f) gl = max(gl, 170);
          if (K.newLab && age < BORN_T + 0.8f) { tag = "new"; tk = 1.0f - (age - BORN_T) / 0.8f; }
          if (orphan(i)) { k *= 0.55f; gl = max(gl, (gFrame + i) % 23 == 0 ? 120 : 0); }   // 迷子は暗く、ときどき乱れる
        }
      }
      if (isThis) { const float hp = 1.0f + 0.05f * sinf(simT * 1.6f); sz *= hp; const float mt = simT - thisT0; if (mt < 0.5f) gl = max(gl, (int)(200 * (1.0f - mt / 0.5f))); }
      if (npeOn && !isThis && ne < 1.2f) k *= 0.0f;                    // NullPointerException のあいだは、this だけ
      SprI* o = addSpr(x, y, sz, c, k, false, false);
      if (o) { o->glitch = (uint8_t)min(255, gl); o->gseed = (uint8_t)(i * 37 + 11); o->reveal = rev; o->outline = outl; }
      if (tag) addLab(x, y + sz * 0.42f, tag, tag[1] == 'u' ? lerpC(C8(235, 240, 255), cThis, 0.2f) : C8(150, 155, 170), tk);
      if (isThis) addLab(x, y + sz * 0.40f, "this", C8(255, 255, 255), 0.9f);
    }
    // this からなぞっているあいだ: 指まで点線と、うすい影
    if (pressOn && pressThis && pressMove > S * 0.05f) {
      dotLine(slotX(thisId), slotY(thisId), pressX, pressY, cThis, 0.9f);
      SprI* o = addSpr(pressX, pressY, S * 0.09f, cKid, 0.6f, false, false);
      if (o) o->outline = 1;
      addLab(pressX, pressY + S * 0.045f, "new", cKid, 0.9f);
    }
    if (npeOn) {                                                      // 画面のまん中に、大きく出す
      const float f = ne < 1.6f ? 1.0f : 1.0f - (ne - 1.6f) / 1.0f;
      addLab(Sc, Sc - S * 0.33f, "NullPointerException", lerpC(C8(255, 90, 90), C8(255, 255, 255), (gFrame & 2) ? 0.0f : 0.4f), f, S >= 400 ? 2 : 1);
    }
    char hb[24];                                                      // 使っている分身の数
    snprintf(hb, sizeof hb, "heap %d/%d", alive, NSLOTF);
    addLab(Sc, Sc + H2 * 0.80f, hb, alive >= NSLOTF ? cThis : C8(120, 125, 140), 0.85f);
    (void)orph;
  } else if (scene == 3) {
    // 傾けると、奥の像の並びが左右・上下にずれる(指で横になぞっても)
    if (in.touching) {
      if (fWas) mirShift += (in.tx - fPX) / (S * 0.18f);
      mirShift = fmaxf(-1.6f, fminf(1.6f, mirShift));
    } else {
      mirShiftV += (-mirShift * 2.5f - mirShiftV * 2.2f) * dt;
      mirShift += mirShiftV * dt;
    }
    const float a = 1.0f - expf(-dt * 4.0f);
    mirTX += (in.tiltX * 1.8f - mirTX) * a; mirTY += (in.tiltY * 1.8f - mirTY) * a;
    if (in.tapped) { goldT0 = simT; ring[ringNext] = {in.tx, in.ty, simT, 0.7f}; ringNext = (ringNext + 1) % NRING; }
    const float sway = 0.18f * sinf(simT * 0.23f) + mirShift + mirTX;
    bgOX = (int)lrintf(-mirTX * S * 0.05f); bgOY = (int)lrintf(mirTY * S * 0.05f);   // 奥の枠も少しずらす
    const float vy = Sc + MIR_VY * H2;
    for (int depth = MIRD - 1; depth >= 0; --depth) {
      const float sc = 1.0f / (1.0f + 0.42f * depth), size = S * 0.40f * sc, f = powf(0.80f, (float)depth);
      const float gw = simT - goldT0, gk = expf(-sqf(gw * 3.2f - depth) * 1.5f);   // 金色の波が、奥へ伝わっていく
      for (int side = -depth; side <= depth; side += 2) {
        const float x = Sc + (side + sway * depth * 0.5f) * S * 0.36f * sc;
        const float y = vy + S * 0.06f * sc - mirTY * depth * S * 0.045f * sc;
        Col c = depth == 0 ? th.warm : lerpC(lerpC(th.mid, C8(255, 255, 255), 0.15f), th.cool, depth / (float)MIRD);
        c = lerpC(c, th.warm, gk);
        const float k = (depth == 0 ? 1.1f : f * 1.05f) * (1.0f + 0.6f * gk) * (0.93f + 0.07f * sinf(simT * 1.1f + depth));
        const bool fx = (side & 1) != 0;
        addSpr(x, y, size, c, k, fx, false);
        if (depth <= 2) addSpr(x, y + size * 0.95f, size, c, k * 0.22f, fx, true);   // 床に映る
      }
    }
  }
  stepParts(dt);
  finishPD();
}

static constexpr size_t nmOa93 = nmAlignTo(nmOv93, alignof(float*)), nmOv94 = nmOa93 + sizeof(float*);
static float*& nmL_sum = *reinterpret_cast<float**>(NM_ARENA + nmOa93);    // 箱の居る割合の地図(PSRAM に置く: 内蔵メモリを空けるため)
static constexpr size_t nmOa94 = nmAlignTo(nmOv94, alignof(float*)), nmOv95 = nmOa94 + sizeof(float*);
static float*& nmL_wsum = *reinterpret_cast<float**>(NM_ARENA + nmOa94);
static void updateFrame(const Input& in) {
  float dt = in.dt > 0.05f ? 0.05f : in.dt;
  if (dt < 0.0005f) dt = 0.0005f;
  simT += dt;
  breathT += dt * BREATH_SPEEDS[breathIdx];
  textRotF += dt * 65536.0f / TEXT_TURN;
  while (textRotF >= 65536.0f) textRotF -= 65536.0f;
  textRot = (uint16_t)textRotF;
  if (scene != 0) {
    nBox = 0; bucketBoxes();
    updateScene(in, dt);
    fPX = in.tx; fPY = in.ty; fWas = in.touching;
    return;
  }
  // 指が止まっている時間(なぞっている間は 0)。止めて押さえているほど、ほどける範囲が広がる
  if (in.touching && fWas) {
    const float mv = hypotf(in.tx - fPX, in.ty - fPY) / dt;
    holdT = (mv < 0.05f * S) ? holdT + dt : 0.0f;
  } else holdT = 0;
  fPX = in.tx; fPY = in.ty; fWas = in.touching;

  // ---- 触った所の箱がほどける。触れば触るほど、なぞればなぞるほど散る
  const float tu = (in.tx - Sc) / H2, tv = (in.ty - Sc) / H2;
  const float sig = TOUCH_R * (1.0f + 0.9f * fminf(1.0f, holdT / 2.0f));   // 止めて押さえていると広がる
  // タップ(と、なぞっている間ときどき)に、光の輪と火花
  static float lastBlast = -10.0f;
  if (in.touching && (in.tapped || (holdT == 0 && simT - lastBlast > 0.35f)) && figureSd(tu, tv) < 0.10f) {
    const bool big = in.tapped;
    lastBlast = simT;
    ring[ringNext] = {in.tx, in.ty, simT, big ? 1.0f : 0.5f};
    ringNext = (ringNext + 1) % NRING;
    const int ns = big ? SPARKS : SPARKS / 4;
    const Col wc = THEMES[theme].warm;
    for (int i = 0; i < ns; ++i) {
      const float a = frand(0, 6.2832f), sp = frand(0.4f, big ? 2.2f : 1.2f) * H2;
      emitPart(in.tx, in.ty, cosf(a) * sp, sinf(a) * sp, lerpC(wc, C8(255, 255, 255), frand(0.3f, 0.9f)), frand(0.5f, 1.4f));
    }
  }
  const float i2 = 1.0f / (2.0f * sig * sig);
  for (int li = 0; li < nLeaf; ++li) {
    const int n = leafList[li];
    const Node& N = nodes[n];
    if (in.touching) {
      const float du = N.cu - tu, dv = N.cv - tv, d2 = du * du + dv * dv;
      if (d2 < 9.0f * sig * sig) {
        const float str = expf(-d2 * i2);                         // 指の真下ほど強く散る
        // まだ散っていない箱、または散り方が足りない箱を、もう一度散らす(触っている間は、戻らない)
        if (str > 0.12f && (str > nodeDis[n] + 0.02f || simT - nodePush[n] > 0.25f)) {
          const bool fresh = nodeDis[n] < 0.3f;
          nodeDis0[n] = nodeDis[n];
          nodePend[n] = fmaxf(str, nodeDis[n]);
          if (fresh) nodeDir[n] = atan2f(dv, du);
          nodePush[n] = simT;
          if (fresh) for (int np = (int)(3.0f * str + frand()); np > 0; --np) {   // 光の粒がこぼれる
            const float a = nodeDir[n] + frand(-0.7f, 0.7f), sp = frand(0.5f, 1.4f) * H2;
            emitPart(PX(N.cu), PX(N.cv), cosf(a) * sp, sinf(a) * sp, boxCol(N.cu, N.cv, 1.4f), frand(0.8f, 2.0f));
          }
        }
      }
    }
    const float e = simT - nodePush[n];
    if (nodePush[n] < -50.0f || e < 0) { nodeDis[n] = 0; continue; }
    const float x = fminf(1.0f, e / BLAST_TIME);
    const float x1 = 1.0f - x, rise = 1.0f - x1 * x1 * x1 * x1;   // 一瞬で吹き飛んで、ふっと止まる
    const float peak = nodeDis0[n] + (nodePend[n] - nodeDis0[n]) * rise;
    // しばらく漂ってから(3〜6秒)、残りの時間をかけて、ゆっくり戻る
    const float wait = RETURN_TIME * (0.20f + 0.20f * nodeRetB[n]);
    const float ret = smooth3((e - wait) / (RETURN_TIME * 0.58f));
    nodeDis[n] = peak * (1.0f - ret);
  }
  // 箱の居る割合の地図(菩薩のにじみを、箱が散った所だけ薄くするため)
  anyAway = false;
  for (int li = 0; li < nLeaf; ++li) if (nodeDis[leafList[li]] > 0.002f) { anyAway = true; break; }
  if (anyAway) {
    float* const sum = nmL_sum;
    float* const wsum = nmL_wsum;
    for (int i = 0; i < PGN * PGN; ++i) { sum[i] = 0; wsum[i] = 0; }
    for (int li = 0; li < nLeaf; ++li) {
      const Node& N = nodes[leafList[li]];
      const int x0 = max(0, (int)PX(N.u0) / PGC), x1 = min(PGN - 1, (int)PX(N.u1) / PGC);
      const int y0 = max(0, (int)PX(N.v0) / PGC), y1 = min(PGN - 1, (int)PX(N.v1) / PGC);
      const float here = 1.0f - nodeDis[leafList[li]];
      for (int y = y0; y <= y1; ++y) for (int x = x0; x <= x1; ++x) { sum[y * PGN + x] += here; wsum[y * PGN + x] += 1.0f; }
    }
    for (int i = 0; i < PGN * PGN; ++i) sum[i] = wsum[i] > 0 ? sum[i] / wsum[i] : 1.0f;
    for (int y = 0; y < PGN; ++y)                                   // 少しぼかす(にじみは広がっているので)
      for (int x = 0; x < PGN; ++x) {
        float a = 0; int c = 0;
        for (int j = -1; j <= 1; ++j) for (int i = -1; i <= 1; ++i) {
          const int xx = x + i, yy = y + j;
          if (xx < 0 || yy < 0 || xx >= PGN || yy >= PGN) continue;
          a += sum[yy * PGN + xx]; ++c;
        }
        pg[y * PGN + x] = a / c;
      }
  }

  // 中の箱(まとまって見えるとき用): 子の中でいちばんほどけているものに合わせる
  for (int n = nNode - 1; n >= 0; --n) if (!nodes[n].leaf) { nodeDis[n] = 0; }
  for (int n = nNode - 1; n > 0; --n) {
    const int p = nodes[n].parent;
    if (p >= 0 && nodeDis[n] > nodeDis[p]) { nodeDis[p] = nodeDis[n]; nodeDir[p] = nodeDir[n]; }
  }

  // ---- 描く箱の一覧
  nBox = 0;
  emitHalo();
  emitNode(0, 1.0f);
  const float hp = 1.0f + 0.12f * sinf(simT * 2.0f);             // 胸の核(this)と、眉間の白毫
  const float hs = 0.022f * hp, bs = 0.012f;
  addBox(-hs, -0.12f - hs, hs, -0.12f + hs, mulC(C8(255, 240, 200), 0.75f + 0.25f * hp), ST_SOLID, 0);
  addBox(-bs, -0.475f - bs, bs, -0.475f + bs, C8(255, 255, 255), ST_SOLID, 0);
  bucketBoxes();
  updateFlies(in.touching, in.tapped, in.tx, in.ty, dt);

  // ---- メッセージと光の粒
  nPD = 0;
  for (int i = 0; i < NPULSE; ++i) {
    Pulse& p = pulse[i];
    if (p.wait > 0) { p.wait -= dt; continue; }
    p.pos += dt * p.speed;
    if (p.pos >= p.len - 1) { newPulse(p); continue; }
    const int s = (int)p.pos;
    const float f = p.pos - s;
    const Node& A = nodes[p.path[s]];
    const Node& B = nodes[p.path[s + 1]];
    const float u = A.cu + (B.cu - A.cu) * f, v = A.cv + (B.cv - A.cv) * f;
    addPD(PX(u), PX(v), boxCol(u, v, 1.0f), 1.0f);
  }
  stepParts(dt);
  finishPD();
}

// ---------------------------------------------------------------- 描画(帯ごと)
// 場面1〜3を、帯ごとに描く
static void renderScene(int k, uint16_t* buf) {
  const int y0 = k * BAND, y1 = min(S, y0 + BAND);
  const int bx0 = bandX0[k], bw = bandW[k], bx1 = bx0 + bw - 1;
  for (int y = y0; y < y1; ++y) {                                   // 背景(映る菩薩では、傾きで少しずらす)
    uint16_t* row = buf + (y - y0) * bw - bx0;
    const int xs = max((int)spanS[y], bx0), xe = min((int)spanE[y], bx1);
    for (int x = bx0; x < xs; ++x) row[x] = 0;
    for (int x = max(xe + 1, bx0); x <= bx1; ++x) row[x] = 0;
    if (xs > xe) continue;
    const int sy = y + bgOY;
    if (sy < 0 || sy >= S) { for (int x = xs; x <= xe; ++x) row[x] = 0; continue; }
    const int a0 = max(xs, -bgOX), a1 = min(xe, S - 1 - bgOX);
    for (int x = xs; x < min(a0, xe + 1); ++x) row[x] = 0;
    for (int x = max(a1 + 1, xs); x <= xe; ++x) row[x] = 0;
    if (a0 <= a1) memcpy(row + a0, (scene == 2 ? scnBuf[2] : scnCur) + (size_t)sy * S + a0 + bgOX, (a1 - a0 + 1) * 2);
  }
  if (scene == 1) {                                                 // 降ってくる般若心経(菩薩の形の所は、菩薩の色)
    const int C = rCell, M = S / 2;
    for (int y = y0; y < y1; ++y) {
      const int xs = max((int)spanS[y], bx0), xe = min((int)spanE[y], bx1);
      if (xs > xe) continue;
      uint16_t* row = buf + (y - y0) * bw - bx0;
      const int r = y / C;
      if (r >= rRows) continue;
      const uint8_t* mk = rMask + (size_t)min(M - 1, y >> 1) * M;
      const int c0 = max(0, (xs - rX0 - 10) / C), c1 = min(rCols - 1, (xe - rX0 + 10) / C);
      for (int c = c0; c <= c1; ++c) {
        const int i = r * rCols + c;
        const uint32_t o = rCOut[i], n = rCIn[i];
        const uint32_t O = (o | (o << 16)) & 0x07E0F81Fu, N = (n | (n << 16)) & 0x07E0F81Fu;
        const int yy = y - rScrollI[c] + C, tr = yy / C - 1, gy = yy % C;   // 流れた分だけ、字を下へずらす
        const uint8_t* gp = rAtlas + (size_t)rGlyph(c, tr) * C * C + gy * C;
        const int xb = rX0 + c * C + rDx[i];
        for (int q = 0; q < C; ++q) {
          const int x = xb + q;
          if (x < xs || x > xe) continue;
          const uint32_t a = gp[q];
          if (!a) continue;
          const uint32_t m = mk[min(M - 1, x >> 1)];
          const uint32_t P = (((O * (16 - m) + N * m) >> 4) & 0x07E0F81Fu) * a >> 4 & 0x07E0F81Fu;
          row[x] = addPix(row[x], (uint16_t)(P | (P >> 16)));
        }
      }
    }
  }
  for (int n = 0; n < nSpr; ++n) {                                  // 菩薩(光として足す)
    const SprI& o = spr[n];
    const int ra = max(y0, (int)o.y0), rb = min(y1 - 1, (int)o.y1);
    if (ra > rb) continue;
    const int T = FST[o.mip];
    const uint8_t* tex = figSpr[o.mip];
    const float stf = o.step / 65536.0f;
    const int revRows = (o.reveal * T) >> 8, thr = o.outline ? 170 : 15;
    for (int y = ra; y <= rb; ++y) {
      int ty = (int)((y + 0.5f - o.top) * stf);
      if (ty < 0 || ty >= T) continue;
      if (o.reveal < 255 && ty > revRows) continue;                 // まだ現れていない行
      int gx = 0;
      if (o.glitch) {                                               // 行がずれる・抜ける
        uint32_t h = (uint32_t)(y >> 2) * 2654435761u ^ (o.gseed * 40503u) ^ (gFrame * 2246822519u);
        h ^= h >> 15; h *= 2246822519u; h ^= h >> 13;
        if ((h & 255) < o.glitch) gx = (int)(((h >> 8) & 63) - 32) * o.glitch / 160;
        if (((h >> 16) & 255) < (uint32_t)(o.glitch >> 2)) continue;
      }
      if (o.flip & 2) ty = T - 1 - ty;
      const uint8_t* srow = tex + ty * T;
      const int xa = max(max((int)o.x0 + gx, bx0), (int)spanS[y]), xb = min(min((int)o.x1 + gx, bx1), (int)spanE[y]);
      if (xa > xb) continue;
      uint16_t* row = buf + (y - y0) * bw - bx0;
      int32_t u = (int32_t)((xa - gx + 0.5f - o.left) * o.step);
      const bool rl = (o.reveal < 255 && ty == revRows);           // 現れかけの行は、走査線のように明るく
      if (gx != 0 && !rl) {                                         // ずれた行は、赤と青緑に分かれる(色ずれ)
        const int32_t d2 = o.step * 2;
        for (int x = xa; x <= xb; ++x, u += o.step) {
          int t1 = (u + d2) >> 16, t2 = (u - d2) >> 16;
          uint16_t add = 0;
          if ((unsigned)t1 < (unsigned)T) { if (o.flip & 1) t1 = T - 1 - t1; const uint8_t a = srow[t1]; if (a > thr) add = o.lutR[a >> 4]; }
          if ((unsigned)t2 < (unsigned)T) { if (o.flip & 1) t2 = T - 1 - t2; const uint8_t a = srow[t2]; if (a > thr) add = add ? (uint16_t)(add | o.lutC[a >> 4]) : o.lutC[a >> 4]; }
          if (add) row[x] = addPix(row[x], add);
        }
        continue;
      }
      for (int x = xa; x <= xb; ++x, u += o.step) {
        int tx = u >> 16;
        if ((unsigned)tx >= (unsigned)T) continue;
        if (o.flip & 1) tx = T - 1 - tx;
        const uint8_t a = srow[tx];
        if (rl) { if (a > 0 || (x & 1)) row[x] = addPix(row[x], o.lut[15]); continue; }
        if (a > thr) row[x] = addPix(row[x], o.lut[a >> 4]);
      }
    }
  }
  for (int kk = max(0, k - 1); kk <= min(NB - 1, k + 1); ++kk)      // 点と光の粒
    for (int n = pdStart[kk]; n < pdStart[kk + 1]; ++n) {
      const PDraw& o = pd[pdOrd[n]];
      for (int dy = -1; dy <= 1; ++dy) {
        const int y = o.y + dy;
        if (y < y0 || y >= y1) continue;
        const int xs = max((int)spanS[y], bx0), xe = min((int)spanE[y], bx1);
        uint16_t* row = buf + (y - y0) * bw - bx0;
        for (int dx = -1; dx <= 1; ++dx) {
          if (dx != 0 && dy != 0) continue;
          const int x = o.x + dx;
          if (x < xs || x > xe) continue;
          row[x] = addPix(row[x], (dx | dy) ? o.ch : o.c);
        }
      }
    }
  for (int n = 0; n < nLab; ++n) {                                  // ラベル(sc 倍の大きさ)
    const Lab& L = lab[n];
    const int sc = L.sc, ra = max(y0, (int)L.y), rb = min(y1 - 1, L.y + FCH * sc - 1);
    for (int y = ra; y <= rb; ++y) {
      const int xs = max((int)spanS[y], bx0), xe = min((int)spanE[y], bx1);
      uint16_t* row = buf + (y - y0) * bw - bx0;
      const int gyr = (y - L.y) / sc;
      for (int c = 0; L.s[c]; ++c) {
        const int ch = (unsigned char)L.s[c];
        if (ch < 33 || ch > 126) continue;
        const uint8_t* g = FONT + (ch - 32) * (FCW * FCH / 2) + gyr * (FCW / 2);
        for (int i = 0; i < FCW * sc; ++i) {
          const int x = L.x + c * FCW * sc + i;
          if (x < xs || x > xe) continue;
          const int q = i / sc;
          const uint8_t b = g[q >> 1], a = (q & 1) ? (b & 15) : (b >> 4);
          if (a) row[x] = addPix(row[x], L.lut[a]);
        }
      }
    }
  }
}

static void renderBandBody(int k, uint16_t* buf);
static void prepOverlay(int k, uint16_t* buf) {    // 場面の準備を待っているあいだ、下に小さな輪(進み具合)
  const int y0 = k * BAND, y1 = min(S, y0 + BAND);
  const int bx0 = bandX0[k], bw = bandW[k];
  const float cx = Sc, cy = S * 0.84f, r0 = S * 0.030f, r1 = r0 + max(2.0f, S * 0.008f);
  if (cy + r1 < y0 || cy - r1 >= y1) return;
  const float pr = prepProgress();
  for (int y = max(y0, (int)(cy - r1)); y < min(y1, (int)(cy + r1) + 1); ++y) {
    uint16_t* row = buf + (y - y0) * bw - bx0;
    for (int x = max(bx0, (int)(cx - r1)); x <= min(bx0 + bw - 1, (int)(cx + r1)); ++x) {
      const float dx = x - cx, dy = y - cy, d = sqrtf(dx * dx + dy * dy);
      if (d < r0 || d > r1) continue;
      const float a = (atan2f(dx, -dy) + 3.14159265f) * (1.0f / 6.2831853f);   // 上から時計回りに 0..1
      const float t = a < 0.5f ? a + 0.5f : a - 0.5f;
      row[x] = t <= pr ? toPix({0.85f, 0.80f, 0.62f}) : toPix({0.16f, 0.15f, 0.13f});
    }
  }
}
static void renderBand(int k, uint16_t* buf) {
  renderBandBody(k, buf);
  if (pendScene >= 0) prepOverlay(k, buf);
}
static void renderBandBody(int k, uint16_t* buf) {
  if (scene != 0) { renderScene(k, buf); return; }
  const int y0 = k * BAND, y1 = min(S, y0 + BAND);
  const int bx0 = bandX0[k], bw = bandW[k], bx1 = bx0 + bw - 1;

  // ---- 動かない部分の絵を写す
  for (int y = y0; y < y1; ++y) {
    uint16_t* row = buf + (y - y0) * bw - bx0;
    const int xs = max((int)spanS[y], bx0), xe = min((int)spanE[y], bx1);
    for (int x = bx0; x < xs; ++x) row[x] = 0;
    for (int x = max(xe + 1, bx0); x <= bx1; ++x) row[x] = 0;
    if (xs > xe) continue;
    const uint16_t* s1 = statImg + (size_t)y * S;
    if (!anyAway || y < bbY0 || y > bbY1) { memcpy(row + xs, s1 + xs, (xe - xs + 1) * 2); continue; }
    // 箱が散った所ほど、菩薩のにじみを抜いた絵(stat0)に近づける
    const uint16_t* s0 = stat0 + (size_t)y * S;
    const float gyf = (y + 0.5f) / PGC - 0.5f;
    const int gy = max(0, min(PGN - 2, (int)floorf(gyf)));
    const float fy = fminf(1.0f, fmaxf(0.0f, gyf - gy));
    const int xa = max(xs, bbX0), xb = min(xe, bbX1);
    for (int x = xs; x < min(xa, xe + 1); ++x) row[x] = s1[x];
    for (int x = max(xb + 1, xs); x <= xe; ++x) row[x] = s1[x];
    if (xa > xb) continue;
    // この行の、ます目ごとの値(縦だけ先に補間しておく。0..4096)
    int32_t rq[82];
    const int c0 = max(0, xa / PGC - 1), c1 = min(PGN - 1, xb / PGC + 1);
    const float* p0 = pg + gy * PGN;
    for (int c = c0; c <= c1; ++c) rq[c] = (int32_t)((p0[c] + (p0[c + PGN] - p0[c]) * fy) * 4096.0f);
    for (int x = xa; x <= xb; ++x) {
      int pos = ((2 * x + 1) * 256) / (2 * PGC) - 128;              // ます目の位置(1/256 単位)
      if (pos < c0 * 256) pos = c0 * 256;
      if (pos > (c1 - 1) * 256 + 255) pos = (c1 - 1) * 256 + 255;
      const int gx = pos >> 8, fx = pos & 255;
      const int q = (int)((rq[gx] * (256 - fx) + rq[gx + 1] * fx + (1 << 15)) >> 16);   // 0..16
      row[x] = q >= 16 ? s1[x] : (q <= 0 ? s0[x] : blend565(s1[x], s0[x], q));
    }
  }

  // ---- 箱(光として足す)
  for (int e = entStart[k]; e < entStart[k + 1]; ++e) {
    const DBox& B = box[ent[e]];
    const int ra = max(y0, (int)B.y0), rb = min(y1 - 1, (int)B.y1);
    for (int y = ra; y <= rb; ++y) {
      const int xa = max(max((int)B.x0, bx0), (int)spanS[y]), xb = min(min((int)B.x1, bx1), (int)spanE[y]);
      if (xa > xb) continue;
      uint16_t* row = buf + (y - y0) * bw - bx0;
      if (B.style == ST_SOLID) { for (int x = xa; x <= xb; ++x) row[x] = addPix(row[x], B.cF); continue; }
      if (y == B.y0 || y == B.y1) { for (int x = xa; x <= xb; ++x) row[x] = addPix(row[x], B.cO); continue; }
      if (B.style == ST_HALO) {
        if (y <= B.hb) { for (int x = xa; x <= xb; ++x) row[x] = addPix(row[x], (x == B.x0 || x == B.x1) ? B.cO : B.cH); }
        else { if (B.x0 >= xa && B.x0 <= xb) row[B.x0] = addPix(row[B.x0], B.cO); if (B.x1 >= xa && B.x1 <= xb && B.x1 != B.x0) row[B.x1] = addPix(row[B.x1], B.cO); }
        continue;
      }
      uint16_t mid = y < B.hb ? B.cH : B.cF;
      int lineEnd = -1;
      if (B.nr > 1 && y > B.hb) {                              // 中の行(メンバー)
        const int span = B.y1 - B.hb;
        for (int r = 1; r < B.nr; ++r)
          if (y == B.hb + r * span / B.nr) { lineEnd = B.x0 + (B.x1 - B.x0) * (35 + ((B.seed * 13 + r * 29) % 51)) / 100; break; }
      }
      for (int x = xa; x <= xb; ++x) {
        uint16_t c = (x == B.x0 || x == B.x1) ? B.cO : ((x >= B.x0 + 2 && x <= lineEnd) ? B.cL : mid);
        row[x] = addPix(row[x], c);
      }
    }
  }

  // ---- メッセージと光の粒(十字の形。上下の帯にまたがるので、となりの帯の分も見る)
  for (int kk = max(0, k - 1); kk <= min(NB - 1, k + 1); ++kk)
    for (int n = pdStart[kk]; n < pdStart[kk + 1]; ++n) {
      const PDraw& o = pd[pdOrd[n]];
      for (int dy = -1; dy <= 1; ++dy) {
        const int y = o.y + dy;
        if (y < y0 || y >= y1) continue;
        const int xs = max((int)spanS[y], bx0), xe = min((int)spanE[y], bx1);
        uint16_t* row = buf + (y - y0) * bw - bx0;
        for (int dx = -1; dx <= 1; ++dx) {
          if (dx != 0 && dy != 0) continue;
          const int x = o.x + dx;
          if (x < xs || x > xe) continue;
          row[x] = addPix(row[x], (dx | dy) ? o.ch : o.c);
        }
      }
    }

  // ---- 外周の文字
  for (int y = y0; y < y1; ++y) {
    const int a = annStart[y], b = annStart[y + 1];
    if (a == b) continue;
    const int xs = max((int)spanS[y], bx0), xe = min((int)spanE[y], bx1);
    uint16_t* row = buf + (y - y0) * bw - bx0;
    for (int i = a; i < b; ++i) {
      const int x = annX[i];
      if (x < xs || x > xe) continue;
      const uint32_t t = ((uint32_t)(uint16_t)(annA[i] - textRot) * (uint32_t)stripW) >> 16;
      const uint8_t al = strip[annR[i] * stripW + t];
      if (al) row[x] = addPix(row[x], textLut[al]);
    }
  }

  // ---- 蝶(いちばん手前)。絵を回して・縮めて、透明でない所だけ貼る
  for (int i = 0; i < nFly; ++i) {
    const Fly& f = fly[i];
    const int ra = max(y0, f.y0), rb = min(y1 - 1, f.y1);
    if (ra > rb) continue;
    const int T = f.T;
    const float half = T * 0.5f, rr2 = f.rr * f.rr;
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
          const uint8_t id = f.tex[ty * T + tx];
          if (id) row[x] = f.pal[id];
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
    if (wJob) {                                     // 描き直しの仕事(コア1 が動きを計算している間)
      const uint32_t t0 = micros();
      while (tjOn && sj && statParOK(sj->st) && micros() - t0 < jobBudgetUs) statStep();
      xSemaphoreGive(semDone);
      continue;
    }
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
    Serial.printf("display: %s\n", fbMode ? "framebuffer (send each band)" : directSend ? "direct DMA (StopWatch, no framebuffer)" : "direct DMA");
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
static void fail(const char* msg) {                 // 読み込み中の輪に重ならないよう、画面を消して赤い大きめの字で出す
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setTextColor(0xF800);
  M5.Display.setTextSize(2);
  M5.Display.setTextDatum(middle_center);
  M5.Display.drawString(msg, M5.Display.width() / 2, M5.Display.height() / 2);
  M5.Display.setTextSize(1);
  Serial.println(msg);
  while (true) nmIdle();
}

static void* allocFast(size_t n) {                 // 速い内蔵メモリに置けるなら置く。ただし、ほかのために余白(24KB)は残す
  if (heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL) >= n + 8192 && heap_caps_get_free_size(MALLOC_CAP_INTERNAL) >= n + 24576) {
    void* p = heap_caps_malloc(n, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (p) return p;
  }
  return ps_malloc(n);
}

__attribute__((cold)) void setup() {   // cold: 1回だけ動くので、小さく作る
  auto cfg = M5.config();
  M5.begin(cfg);
  Serial.begin(115200);
#if defined(NL_DIRECT_AMOLED)
  {
    auto* pnl = M5.Display.getPanel();
    if (DIRECT_SEND && pnl && pnl->getAutoDisplay() && M5.getBoard() == m5::board_t::board_M5StopWatch) {
      lgfx::Panel_AMOLED* inner = NlFbPeek::inner(pnl);
      if (inner) { M5.Display.setPanel(inner); directSend = true; }
    }
  }
#endif

  if (M5.Display.width() < M5.Display.height()) M5.Display.setRotation(1);
  W = M5.Display.width(); H = M5.Display.height();
  isRound = (W == H);
  S = (isRound ? W : max(W, H)) & ~1;
  Sc = (S - 1) * 0.5f; H2 = S * 0.5f;
  XOFF = (W - S) / 2;
  YOFF = ((H - S) / 2 / BAND) * BAND;
  if (!isRound) YOFF += 2 * BAND;                     // 四角い画面: 菩薩(光輪から蓮まで)が上下のまん中に来るよう、少し下げる
  NB = (S + BAND - 1) / BAND;
  Serial.println("NullBodhisattva v22");
  Serial.printf("screen %dx%d  area %d\n", W, H, S);

  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setBrightness(brightSteps[brightIdx]);
  bootT0 = millis();
  loadingShow(0, 4);
#if defined(ESP_PLATFORM)
  Serial.printf("internal free %u B (largest %u B)  DMA largest %u B\n", (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
                (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL), (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_DMA));
#endif

  // もう片方のコアで描く係(タスク)は、内蔵メモリが空いているうちに、いちばん先に作る(あとで作れないと、最初のコアで止まってしまう)
  semGo   = xSemaphoreCreateBinary();
  semDone = xSemaphoreCreateBinary();
  if (!semGo || !semDone || xTaskCreatePinnedToCore(workerTask, "band", 8192, nullptr, 1, nullptr, 0) != pdPASS) fail("Memory error (task)");
  for (int i = 0; i < NSLOT; ++i) slotBuf[i] = (uint16_t*)heap_caps_malloc(S * BAND * 2, MALLOC_CAP_DMA);
  PGN = S / PGC + 2;
  nmL_sum = (float*)ps_malloc(sizeof(float) * PGN * PGN); nmL_wsum = (float*)ps_malloc(sizeof(float) * PGN * PGN);
  if (!nmL_sum || !nmL_wsum) fail("Memory error (map)");
  pg = (float*)calloc(PGN * PGN, sizeof(float));
  nodes = (Node*)ps_malloc(sizeof(Node) * MAXNODE);
  nodeDis = (float*)calloc(MAXNODE, sizeof(float));
  nodeDir = (float*)calloc(MAXNODE, sizeof(float));
  nodePush = (float*)calloc(MAXNODE, sizeof(float));
  nodePend = (float*)calloc(MAXNODE, sizeof(float));
  nodeDis0 = (float*)calloc(MAXNODE, sizeof(float));
  nodeRetB = (float*)calloc(MAXNODE, sizeof(float));
  leafList = (int16_t*)malloc(MAXNODE * sizeof(int16_t));
  box = (DBox*)allocFast(sizeof(DBox) * MAXBOX);
  ent = (int16_t*)allocFast(sizeof(int16_t) * MAXENT);
  spanS  = (int16_t*)malloc(S * sizeof(int16_t)); spanE = (int16_t*)malloc(S * sizeof(int16_t));
  bandX0 = (int16_t*)malloc(NB * sizeof(int16_t)); bandW = (int16_t*)malloc(NB * sizeof(int16_t));
  bandVis = (uint8_t*)malloc(NB);
  for (int m = 0; m < NMIP; ++m) atlas[m] = (uint8_t*)ps_malloc((size_t)NFR * MIPT[m] * MIPT[m]);
  flyPal = (uint16_t*)allocFast((size_t)2 * NFR * NPIDX * 2);
  scnBuf[2] = (uint16_t*)ps_malloc((size_t)S * S * 2);
  scnCur = (uint16_t*)ps_malloc((size_t)S * S * 2);
  for (int i = 1; i <= 3; i += 2) scnIdx[i] = (uint16_t*)ps_malloc((size_t)S * S * 2);
  scnLut = (uint16_t*)ps_malloc((size_t)LUTA * LUTB * 2);
  for (int m = 0; m < NFS; ++m) figSpr[m] = (uint8_t*)ps_malloc((size_t)FST[m] * FST[m]);
  spr = (SprI*)allocFast(sizeof(SprI) * MAXSPR);
  lab = (Lab*)allocFast(sizeof(Lab) * MAXLAB);
  kid = (Kid*)calloc(NSLOTF, sizeof(Kid));
  rCell = max(12, (int)lrintf(RAIN_CELL * S / 466.0f)); rCols = S / rCell; rRows = S / rCell + 1; rX0 = (S - rCols * rCell) / 2;
  rText = (uint8_t*)malloc(MAXTXT);
  rBase = (int16_t*)malloc(sizeof(int16_t) * rCols);
  rScroll = (float*)malloc(sizeof(float) * rCols);
  rSpeed = (float*)malloc(sizeof(float) * rCols);
  rScrollI = (int16_t*)allocFast(sizeof(int16_t) * rCols);
  rNul = (uint32_t*)malloc(sizeof(uint32_t) * NULW * rCols);
  rDrop = (Drop*)malloc(sizeof(Drop) * rCols * 2);
  rCOut = (uint16_t*)allocFast(sizeof(uint16_t) * rCols * rRows);
  rCIn  = (uint16_t*)allocFast(sizeof(uint16_t) * rCols * rRows);
  rDx   = (int8_t*)allocFast(rCols * rRows);
  rMask = (uint8_t*)ps_malloc((size_t)(S / 2) * (S / 2));
  bool ok = atlas[0] && atlas[1] && atlas[2] && flyPal && pg && PGN <= 80 && nodes && nodeDis && nodeDir && nodePush && nodePend && nodeDis0 && nodeRetB && leafList && box && ent &&
            spanS && spanE && bandX0 && bandW && bandVis && NB <= MAXNB;
  ok = ok && scnBuf[2] && scnCur && scnIdx[1] && scnIdx[3] && scnLut && figSpr[0] && figSpr[1] && figSpr[2] && spr && lab && kid && rText && rBase && rScroll && rSpeed && rScrollI && rNul && rDrop && rCOut && rCIn && rDx && rMask;
  for (int i = 0; i < NSLOT; ++i) ok = ok && slotBuf[i];
  if (!ok) fail("Memory error");

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

  // 菩薩の箱の木を作る(毎回同じ形)
  rs = 7; nNode = 0;
  split(-0.5f, -0.66f, 0.5f, 0.34f, 0, -1);
  nLeaf = 0;
  for (int n = 0; n < nNode; ++n) if (nodes[n].leaf) leafList[nLeaf++] = (int16_t)n;
  for (int n = 0; n < nNode; ++n) {
    nodePush[n] = -100.0f; nodePend[n] = 0;
    // 戻り始めるまでの待ち時間を、箱ごとに少しばらつかせる
    nodeRetB[n] = frand();
  }
  Serial.printf("objects: %d nodes, %d leaves\n", nNode, nLeaf);
  buildHalo();
  if (!haloB) fail("Memory error");
  if (nNode == 0 || nLeaf == 0) fail("Shape error");

  loadingShow(1, 4);
  buildRimText();
  if (!strip || !annX || !annA || !annR || !annStart) fail("Memory error");
  uint32_t t0 = millis();
  if (!buildStatic()) fail("Memory error");
  bootBgMs = millis() - t0;
  Serial.printf("background built in %u ms  (PSRAM free %u KB)\n", (unsigned)bootBgMs, psFreeKB());
  loadingShow(2, 4);
  for (int i = 0; i < MAXPART; ++i) part[i].life = 0;
  for (int i = 0; i < NPULSE; ++i) { newPulse(pulse[i]); pulse[i].pos = frand(0, (float)(pulse[i].len - 1)); pulse[i].wait = 0; }

  for (int m = 0; m < NMIP; ++m) {                               // 蝶の絵
    mipR0[m] = MIPT[m] * 0.5f / 1.15f;
    for (int fr = 0; fr < NFR; ++fr) drawFlyAtlas(atlas[m] + (size_t)fr * MIPT[m] * MIPT[m], MIPT[m], 0.14f + 0.86f * fr / (NFR - 1));
  }
  buildFlyPal();
  setFlyCount(FLY_START);
  loadingShow(3, 4);
  imuOK = M5.Imu.isEnabled();
  if (SCENE_START != 0) finishPrep();                                 // 最初から場面1〜3なら、先に準備を済ませる
  setScene(SCENE_START);
  bootMs = millis() - bootT0;

  Input in0 = {false, false, -1000, -1000, 0.016f};
  updateFrame(in0);

#if defined(ESP_PLATFORM)
  Serial.printf("ready: internal free %u B (largest %u B)\n", (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL), (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL));
#endif
}

static void nextTheme() {
  statFree();                                                        // 描きかけの背景は捨てる
  theme = (theme + 1) % NTHEME;
  buildFlyPal();
  buildTextLut();
  if (scene == 1 || scene == 3) { sceneLut(scene); curFillY = 0; }  // 場面1・3は、背景を4コマで塗り直すだけ
  tjT0 = millis();
  tjOn = true;
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
    if (M5.BtnA.wasHold()) { nextTheme(); now = millis(); }
    else if (M5.BtnA.wasClicked()) { requestScene(((pendScene >= 0 ? pendScene : scene) + 1) % NSCENE); now = millis(); }
    if (M5.BtnB.wasHold()) setFlyCount((flyIdx + 1) % 3);
    else if (M5.BtnB.wasClicked()) {
      brightIdx = (brightIdx + 1) % sizeof(brightSteps);
      M5.Display.setBrightness(brightSteps[brightIdx]);
    }
  } else {
    if (M5.BtnPWR.wasClicked()) {
      brightIdx = (brightIdx + 1) % sizeof(brightSteps);
      M5.Display.setBrightness(brightSteps[brightIdx]);
    }
    if (pressed) {
      if (now - lastTapMs < 350) {                               // ダブルタップ: 上半分なら場面、下半分なら色
        if (t.y < H / 2) requestScene(((pendScene >= 0 ? pendScene : scene) + 1) % NSCENE); else nextTheme();
        now = millis();
        lastTapMs = 0;
      }
      else lastTapMs = now;
    }
  }

  // 傾き(映る菩薩で使う): 本体を傾けたとき、画面の上で「下」になる向き
  static float shakeAcc = 0, pAx = 0, pAy = 0, pAz = 1;
  static uint32_t shakeMs = 0;
  bool shook = false;
  if (imuOK && (scene == 2 || scene == 3)) {
    float ax, ay, az;
    M5.Imu.update();
    if (M5.Imu.getAccel(&ax, &ay, &az)) {
      // 振ったか: 加速度の変わり方が大きいコマが続いたら(分かれる菩薩の NullPointerException)
      const float j = fabsf(ax - pAx) + fabsf(ay - pAy) + fabsf(az - pAz);
      pAx = ax; pAy = ay; pAz = az;
      shakeAcc = shakeAcc * 0.75f + j;
      if (shakeAcc > 2.2f && now - shakeMs > 3000) { shook = true; shakeMs = now; shakeAcc = 0; }
      float sx = -ax, sy = ay;
#if defined(ESP_PLATFORM)
      if (TILT_STOPWATCH_FIX && M5.getBoard() == m5::board_t::board_M5StopWatch) { sx = -ay; sy = -ax; }   // 90 度回して読む
#endif
      if (TILT_SWAP) { const float tmp = sx; sx = sy; sy = tmp; }
      if (TILT_FLIP_X) sx = -sx;
      if (TILT_FLIP_Y) sy = -sy;
      const float dts = (float)(now - last) / 1000.0f;
      // 水平の基準: この場面を開いてすぐ(はじめの 20 回)の姿勢。同じ角度で持ち続けると、ゆっくりそこを水平にする
      if (levelN < 20) { ++levelN; levelX += (sx - levelX) / levelN; levelY += (sy - levelY) / levelN; }
      else if (TILT_RELEVEL > 0 && fabsf(sx - levelX) < TILT_RELEVEL_MAX && fabsf(sy - levelY) < TILT_RELEVEL_MAX) {
        const float b = 1.0f - expf(-dts / TILT_RELEVEL); levelX += (sx - levelX) * b; levelY += (sy - levelY) * b; }
      sx -= levelX; sy -= levelY;
      const float m = sqrtf(sx * sx + sy * sy);
      if (m < TILT_DEAD) { sx = sy = 0; } else { const float k = (m - TILT_DEAD) / m; sx *= k; sy *= k; }
      sx = sx < -1 ? -1 : sx > 1 ? 1 : sx; sy = sy < -1 ? -1 : sy > 1 ? 1 : sy;
      const float a = 1.0f - expf(-dts * 8.0f);
      tiltX += (sx - tiltX) * a; tiltY += (sy - tiltY) * a;
    }
  }

  Input in;
  in.tiltX = tiltX; in.tiltY = tiltY; in.shake = shook;
  in.touching = t.isPressed();
  in.tapped   = pressed;
  in.tx = (float)(t.x - XOFF);
  in.ty = (float)(t.y - YOFF);
  in.dt = (now - last) / 1000.0f;
  last = now;

  static int prepWait = 3;  // 最初の数コマは、箱の菩薩をすぐ見せる
  if (prepStage < NPREP && --prepWait <= 0) prepRun(pendScene >= 0 ? 80000u : 6000u);   // 準備は1コマ 6ms まで(場面を待っているときは、その場面に要る分を 80ms まで)
  if (pendScene >= 0 && sceneReady(pendScene)) { const int ps = pendScene; pendScene = -1; setScene(ps); }
  if (tjOn) themeWork();
  const bool par = tjOn && sj && statParOK(sj->st);                  // 描き直しの続きは、コア0 で動きの計算と同時に
  if (par) { wJob = 1; jobBudgetUs = scene == 0 ? max((uint32_t)8000, updUs) : 4000; xSemaphoreGive(semGo); }
  const uint32_t tu = micros();
  updateFrame(in);
  updUs = micros() - tu;
  if (par) { xSemaphoreTake(semDone, portMAX_DELAY); wJob = 0; }
  if (curFillY < S && (scene == 1 || scene == 3)) {                   // 場面1・3で色を変えたら、背景を4コマで塗り直す
    const int y1 = min(S, curFillY + (S + 3) / 4);
    curFill(curFillY, y1); curFillY = y1;
  }
  renderFrame();

  if (++frames, millis() - fpsTimer >= 3000) {
    float fps = frames * 1000.0f / (millis() - fpsTimer);
    Serial.printf("fps: %.1f  core0 %.1f ms  core1 %.1f ms  send %.1f ms  update %.1f ms\n", fps, busyUs[0] / 1000.0f, busyUs[1] / 1000.0f, sendUs / 1000.0f, updUs / 1000.0f);
    static int bootReport = 0;  // 読み込みの時間(再起動の直後は USB がつながっていないので、ここでもう一度)
    if (bootReport < 2 && prepStage >= NPREP) {
      ++bootReport;
      Serial.printf("boot: %u ms (background %u ms)  later: figures %u ms, sutra %u ms, scenes %u / %u / %u ms\n", (unsigned)bootMs, (unsigned)bootBgMs,
                    (unsigned)prepMs[0], (unsigned)prepMs[1], (unsigned)prepMs[2], (unsigned)prepMs[3], (unsigned)prepMs[4]);
    }
    if (imuOK && scene == 3) Serial.printf("tilt: x %+.2f  y %+.2f  右を下げると x が +、手前(画面の下)を下げると y が + なら正しい向き\n", tiltX, tiltY);
    frames = 0; fpsTimer = millis();
  }
}


// 共通の置き場に置いたものは、0 で始まる単純な型だけ(コンストラクタが要らないもの)
static_assert(std::is_trivially_default_constructible<Fly>::value, "NM_ARENA: Fly");
static_assert(std::is_trivially_default_constructible<PDraw>::value, "NM_ARENA: PDraw");
static_assert(std::is_trivially_default_constructible<Part>::value, "NM_ARENA: Part");
static_assert(std::is_trivially_default_constructible<Pulse>::value, "NM_ARENA: Pulse");
static_assert(std::is_trivially_default_constructible<Rip>::value, "NM_ARENA: Rip");
static_assert(std::is_trivially_default_constructible<bool>::value, "NM_ARENA: bool");
static_assert(std::is_trivially_default_constructible<float>::value, "NM_ARENA: float");
static_assert(std::is_trivially_default_constructible<int>::value, "NM_ARENA: int");
static_assert(std::is_trivially_default_constructible<int16_t>::value, "NM_ARENA: int16_t");
static_assert(std::is_trivially_default_constructible<uint16_t>::value, "NM_ARENA: uint16_t");
static_assert(std::is_trivially_default_constructible<uint32_t>::value, "NM_ARENA: uint32_t");
static constexpr size_t OVL_END = nmAlignTo(nmOv95, 16);
static_assert(OVL_END <= NM_ARENA_SIZE, "NM_ARENA_SIZE (nm_common.h) を大きくしてください");
extern const size_t OVL_SIZE = OVL_END;   // 起動時の確認用
}  // namespace NullBodhisattva
