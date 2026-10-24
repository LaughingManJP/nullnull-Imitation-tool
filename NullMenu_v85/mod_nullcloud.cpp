// NullCloud  -  NullMenu 用モジュール(元のスケッチ NullCloud_v6 を名前空間に入れたもの)
// 0 で始まる変数(配列も)は、ほかの作品と共通の置き場(NM_ARENA)に置いています。
// 一度に動くのは1つだけ(切り替えるときは再起動)なので、重なっていても問題ありません。
#include "nm_common.h"
#if defined(ESP_PLATFORM) && __has_include(<lgfx/v1/panel/Panel_AMOLED.hpp>)
#include <lgfx/v1/panel/Panel_AMOLED.hpp>
#endif
#pragma GCC optimize("O3")

namespace NullCloud {
static constexpr size_t nmOv0 = 0;

// NullCloud v6  -  高い空から見下ろす雲の海。雲がゆっくり流れ、雲の上に虹の輪(光輪)が浮かぶ。朝・昼・夕方・夜を切りかえられる
//   大阪・関西万博のパビリオン「null²」を見た印象をもとにした、非公式の習作です。写真や他の作品の画像は使っていません。
//   M5Stack StopWatch 用(丸い 466x466 画面)。CoreS3 系でも真ん中に正方形で動きます。
//
// 画面の中:
//   ・上は、丸い地平線と、うすく光る大気の帯、その上は宇宙の濃い紺。下は、どこまでも続く雲の海
//   ・雲は、もこもこした積雲のつぶがびっしり並んだ雲の海。つぶごとに光の当たる側が白く、反対側は青みの灰色。ゆっくり流れていく。遠くほど青白くかすむ。ところどころに雲の切れ間
//   ・雲の上に、虹の輪(光輪)がいくつか浮かぶ。まん中がほんのり白く、細い虹の輪が2重(内側が青紫、外側が赤)。
//     輪は雲の面に寝た丸なので、斜めから見て楕円になる(手前ほど大きく、遠いほど細い)。本物の光輪のように自分についてきて、
//     雲は輪の下を流れていく。向きや傾きを変えると、輪の形も見え方どおりに変わる。少しずつすべって、大きさもゆらぐ。雲のすきまでは弱まる
//   ・時間帯は4つ(朝 / 昼 / 夕方 / 夜)
//       朝   … 低い太陽が左の地平線からさして、雲は桃色がかった白。空は紺から桃・オレンジへ
//       昼   … 白い雲と、青い大気の帯
//       夕方 … 右に沈む太陽。雲はオレンジ、影は赤紫。地平線は赤から金
//       夜   … 月明かりの青い雲と、星空。光輪は淡い月の光輪
//
// 触ると:
//   タップ … 触った所に、新しい光輪がふわっと生まれる(しばらくして消える)
//   なぞる … 横になぞると、見ている向きが回る。縦になぞると、見下ろす角度が変わる(はなすと、ゆっくり戻る)
// 傾けると:
//   左右で地平線が傾き、手前・奥で見下ろす角度が変わる
//
// 操作:
//   StopWatch … ボタンA: 時間帯(朝 / 昼 / 夕方 / 夜。2秒かけて移りかわる)  ボタンB: 雲の流れる速さ(1倍 / 3倍 / 8倍)
//   CoreS3系  … 電源ボタン短押し: 時間帯  ダブルタップ: 雲の流れる速さ
//
// 必要なもの:
//   ボードマネージャー M5Stack >= 3.3.7 / ボード: M5StopWatch(CoreS3 なら M5CoreS3)。StopWatch では PSRAM を「OPI PSRAM」に
//   ライブラリ M5Unified >= 0.2.15, M5GFX >= 0.2.21
//
// しくみ:
//   起動時に、上下左右がつながった雲の模様(1024x1024。雲の面の 4x4 の広さ)を作っておく。
//     積雲のつぶ(大きなつぶの上に小さなつぶ)を丸い山の形で置いて高さの絵を作り、その傾きから光の当たり方を決める。
//     模様の点には、色ではなく「濃さ・陰・切れ間」の番号を入れておき、描くときに小さな色の表(内蔵メモリ)を引く。
//     時間帯を変えるときは、この色の表だけを、2秒かけて新しい時間帯の表へまぜていく(雲の絵は作りなおさない)。
//   模様の絵は、4行ずつの帯にして縦に4点ずつ並べておく(PSRAM から一度に読む 32 バイトに、たて横となりの 4x4 点が入る。帯のあいだは少しあける)。
//     遠くを描くときは、半分・1/4…に縮めた絵(5段)から引く(読む所がとびとびにならず、ちらつきも減る)。
//   雲の面と空は、2x2 点に1回だけ計算して、あいだはまわりの4点からなめらかにつなぐ。地平線の近くだけは1点ずつ。
//   コマごとには、画面の点から「雲の面のどこか」を、遠近(手前ほど大きい)で求めて、模様を引く。
//     割り算は 8 点ごとに1回だけして、あいだは足し算でつなぐ。遠くほど、かすみの色をまぜる。
//   空は、地平線からの高さで色の表を引く。光輪は、雲の面の上の丸として、その近くの点だけで計算する。
//   帯(16行ずつ)は、2つのコアのうち空いた方が、次の帯を取って描く。

#ifndef IRAM_ATTR
#define IRAM_ATTR
#endif


// Arduino IDE は関数の宣言を自動で先頭近くに足すので、型の名前だけ先に宣言しておく
struct Glory;
struct Star;
struct Tod;
struct Samp;

// ---- 調整用 ----------------------------------------------------------------
static constexpr int   TOD_START   = 1;       // 始めの時間帯(0: 朝 / 1: 昼 / 2: 夕方 / 3: 夜)
static constexpr float WIND        = 0.25f;   // 雲の流れる速さ(1倍のとき。雲の面の広さ / 秒)
static constexpr float TILT_GAIN   = 1.0f;    // 傾けたときの動きの大きさ
static constexpr float TILT_PITCH  = 1.0f;    // 手前・奥に傾けたとき、見下ろす角度が変わる強さ
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
static constexpr int TB = 10, T = 1 << TB, TM = T - 1;   // 雲の模様の大きさ(1024x1024。上下左右がつながっている)
static constexpr float TW = 4.0f;                 // 模様1枚 = 雲の面の何マス分か
static constexpr float TPU = T / TW;              // 雲の面の 1 = 模様の何点か
static constexpr int MAXG = 10, NFIX = 6, NSTAR = 260;
static constexpr int NTOD = 4;
static const char* const TOD_NAME[NTOD] = {"morning", "noon", "evening", "night"};
static const float SPEEDS[3] = {1.0f, 3.0f, 8.0f};

static int W, H, S, XOFF, YOFF, NB;
static bool roundScr = true, fbMode = false, directSend = false;
static int16_t* spanA = nullptr;
static int16_t* spanB = nullptr;
static int16_t* bandX = nullptr;
static int16_t* bandW = nullptr;
static uint16_t* band[NSLOT] = {nullptr, nullptr, nullptr, nullptr};
static constexpr int NMIP = 5;                    // 色の絵の段(1024, 512, 256, 128, 64)
static uint16_t* lev[NMIP] = {nullptr, nullptr, nullptr, nullptr, nullptr};   // 雲の色の絵(画面へ送る形。4x4 のかたまりで並べる)
static uint16_t*& tex = lev[0];
// 雲の模様の点には、色ではなく「濃さ 32段・陰 32段・切れ間 8段」の番号(13 ビット)を入れておき、描くときに色の表 clut を引く。
// 時間帯を変えるときは、この小さな表だけをまぜればよい
static constexpr int NKEY = 32 * 32 * 8;
static constexpr size_t nmOa0 = nmAlignTo(nmOv0, 16), nmOv1 = nmOa0 + sizeof(uint16_t[NKEY]); static uint16_t (&clut)[NKEY] = *reinterpret_cast<uint16_t(*)[NKEY]>(NM_ARENA + nmOa0);                        // 今の雲の色の表(内蔵メモリ)
static uint8_t*  cD = nullptr;                    // 雲の濃さ(0..255)[T * T]
static uint8_t*  cS = nullptr;                    // 雲の陰(明るさ)[T * T]
static uint8_t*  cH = nullptr;                    // 雲の切れ間(0 = 下の海が見える)[T * T]
static uint8_t*  latt = nullptr;                  // ノイズの格子の値 [512 * 512]
static uint16_t* luts = nullptr;                  // 時間帯ごとの雲の色の表 [NTOD][NKEY]
static constexpr size_t nmOa1 = nmAlignTo(nmOv1, 16), nmOv2 = nmOa1 + sizeof(float[1024]); static float (&sinT)[1024] = *reinterpret_cast<float(*)[1024]>(NM_ARENA + nmOa1);

// 時間帯の色
struct Tod { float lit[3], amb[3], gap[3], sea[3], fog[3], top[3], mid[3], bnd[3], limb[3], sun[3]; float sunx, glory; bool moon, stars; };
static const Tod TODS[NTOD] = {
  {{1.00f, 0.88f, 0.80f}, {0.55f, 0.55f, 0.78f}, {0.36f, 0.36f, 0.52f}, {0.04f, 0.08f, 0.18f}, {0.92f, 0.78f, 0.80f},
   {0.04f, 0.06f, 0.18f}, {0.20f, 0.25f, 0.50f}, {0.95f, 0.62f, 0.45f}, {1.00f, 0.75f, 0.55f}, {1.00f, 0.80f, 0.55f}, -0.55f, 0.90f, false, false},   // 朝
  {{0.95f, 0.97f, 1.00f}, {0.45f, 0.58f, 0.80f}, {0.30f, 0.38f, 0.52f}, {0.03f, 0.10f, 0.20f}, {0.64f, 0.76f, 0.93f},
   {0.01f, 0.015f, 0.04f}, {0.03f, 0.06f, 0.15f}, {0.28f, 0.42f, 0.78f}, {0.40f, 0.60f, 0.95f}, {0, 0, 0}, 9.0f, 1.00f, false, false},           // 昼
  {{1.00f, 0.66f, 0.42f}, {0.50f, 0.35f, 0.55f}, {0.32f, 0.22f, 0.36f}, {0.06f, 0.04f, 0.12f}, {0.98f, 0.55f, 0.35f},
   {0.03f, 0.02f, 0.10f}, {0.25f, 0.10f, 0.25f}, {1.00f, 0.40f, 0.15f}, {1.00f, 0.55f, 0.25f}, {1.00f, 0.55f, 0.20f}, 0.45f, 0.75f, false, false},  // 夕方
  {{0.42f, 0.50f, 0.70f}, {0.12f, 0.16f, 0.30f}, {0.06f, 0.08f, 0.16f}, {0.01f, 0.02f, 0.05f}, {0.14f, 0.18f, 0.32f},
   {0.00f, 0.00f, 0.01f}, {0.01f, 0.02f, 0.06f}, {0.08f, 0.14f, 0.32f}, {0.18f, 0.30f, 0.60f}, {0, 0, 0}, 9.0f, 0.45f, true, true},             // 夜
};
static int tod = TOD_START;

// 時間帯ごとの表(色の表を作りなおす)
static constexpr size_t nmOa2 = nmAlignTo(nmOv2, 16), nmOv3 = nmOa2 + sizeof(uint16_t[256]); static uint16_t (&skyL)[256] = *reinterpret_cast<uint16_t(*)[256]>(NM_ARENA + nmOa2);                        // 空: 地平線からの高さ → 色
static constexpr size_t nmOa3 = nmAlignTo(nmOv3, 16), nmOv4 = nmOa3 + sizeof(uint8_t[256]); static uint8_t (&limbL)[256] = *reinterpret_cast<uint8_t(*)[256]>(NM_ARENA + nmOa3);                       // 地平線の光る帯の濃さ(地平線からの近さ)
static constexpr size_t nmOa4 = nmAlignTo(nmOv4, 16), nmOv5 = nmOa4 + sizeof(uint8_t[256]); static uint8_t (&sunX)[256] = *reinterpret_cast<uint8_t(*)[256]>(NM_ARENA + nmOa4);
static constexpr size_t nmOa5 = nmAlignTo(nmOv5, 16), nmOv6 = nmOa5 + sizeof(uint8_t[256]); static uint8_t (&sunD)[256] = *reinterpret_cast<uint8_t(*)[256]>(NM_ARENA + nmOa5);
static constexpr size_t nmOa6 = nmAlignTo(nmOv6, 16), nmOv7 = nmOa6 + sizeof(uint8_t[256]); static uint8_t (&sunS)[256] = *reinterpret_cast<uint8_t(*)[256]>(NM_ARENA + nmOa6);  // 太陽の光: 横のずれ / 雲の面の深さ / 空の高さ
static constexpr size_t nmOa7 = nmAlignTo(nmOv7, 16), nmOv8 = nmOa7 + sizeof(uint8_t[1024]); static uint8_t (&glR)[1024] = *reinterpret_cast<uint8_t(*)[1024]>(NM_ARENA + nmOa7);
static constexpr size_t nmOa8 = nmAlignTo(nmOv8, 16), nmOv9 = nmOa8 + sizeof(uint8_t[1024]); static uint8_t (&glG)[1024] = *reinterpret_cast<uint8_t(*)[1024]>(NM_ARENA + nmOa8);
static constexpr size_t nmOa9 = nmAlignTo(nmOv9, 16), nmOv10 = nmOa9 + sizeof(uint8_t[1024]); static uint8_t (&glB)[1024] = *reinterpret_cast<uint8_t(*)[1024]>(NM_ARENA + nmOa9);
static constexpr size_t nmOa10 = nmAlignTo(nmOv10, 16), nmOv11 = nmOa10 + sizeof(uint8_t[1024]); static uint8_t (&glOn)[1024] = *reinterpret_cast<uint8_t(*)[1024]>(NM_ARENA + nmOa10);
static constexpr size_t nmOa11 = nmAlignTo(nmOv11, 16), nmOv12 = nmOa11 + sizeof(uint16_t[1025]); static uint16_t (&glCum)[1025] = *reinterpret_cast<uint16_t(*)[1025]>(NM_ARENA + nmOa11);                         // glOn の足し合わせ(区間に色のある所があるか、すぐ分かる)  // 光輪の色(中心からの距離の2乗 → 色)
static int fogR = 0, fogG = 0, fogB = 0, limbR = 0, limbG = 0, limbB = 0, sunR = 0, sunG = 0, sunB = 0;
static bool sunOn = false;
static int   sunXi = 0;                               // 太陽の横の位置(×64)
static float starW = 0, gloryW = 1;                   // 星の明るさ、光輪の濃さ(時間帯の移りかわりで変わる)
// 時間帯ごとの、空・光輪・かすみ・地平線・太陽の色(起動時に4つとも作っておく)
struct TodTab { uint16_t sky[256]; uint8_t gr[1024], gg[1024], gb[1024]; float fog[3], limb[3], sun[3]; float sunx, glory, stars; bool on; };
static TodTab* tabs = nullptr;
static constexpr float TRANS_SEC = 2.0f;              // 時間帯が移りかわる時間(秒)
static int   todFrom = TOD_START;
static float transT = 1.0f;                           // 0 → 1


struct Glory { float gx, gz, R, A, age, life; bool on, fixed; float cx, cy, qa, qb, qc; int32_t wa; int x0, x1, y0, y1; float a; };
static constexpr size_t nmOa12 = nmAlignTo(nmOv12, 16), nmOv13 = nmOa12 + sizeof(Glory[MAXG]); static Glory (&gl)[MAXG] = *reinterpret_cast<Glory(*)[MAXG]>(NM_ARENA + nmOa12);
static int   glNext = NFIX;
struct Star { float rx, sd, b, tw; int sx, sy; bool vis; };
static constexpr size_t nmOa13 = nmAlignTo(nmOv13, 16), nmOv14 = nmOa13 + sizeof(Star[NSTAR]); static Star (&stars)[NSTAR] = *reinterpret_cast<Star(*)[NSTAR]>(NM_ARENA + nmOa13);

// 見え方
static float simT = 0, scrX = 0, scrZ = 0;        // 雲が流れた分(模様1枚の中に折りたたむ)
static float yaw = 0, pitchOff = 0, roll = 0, H0 = -0.60f;
static float vCr = 1, vSr = 0, vCy = 1, vSy = 0;  // 地平線の傾き(roll)と、見ている向き(yaw)
static int   speedIdx = 0;
static float tiltX = 0, tiltY = 0, levelX = 0, levelY = 0;
static int   levelN = 0;
static bool  imuOK = false, grabbing = false;
static uint32_t rnd = 12345;
static inline float frand() { rnd = rnd * 1664525u + 1013904223u; return (rnd >> 8) * (1.0f / 16777216.0f); }   // 0..1

// 2つのコアで描く
static SemaphoreHandle_t semGo = nullptr, semDone = nullptr;
static volatile int doneUpTo = -1;               // ここまでの帯は、画面へ送り終わった(置き場を使い回してよい)
static volatile int nextBand = 0;                 // 次に描く帯(2つのコアで、空いた方が取る)
static volatile uint8_t bandReady[64];            // 帯を描き終えた
static uint32_t busyUs[2] = {0, 0}, sendUs = 0;
static volatile int job = 0;                      // コア0 にたのむ仕事: 0 = 帯を描く / 1 = 雲の模様の奇数行

// ---------------------------------------------------------------- 小さな道具
static inline float clamp01(float v) { return v < 0 ? 0 : (v > 1 ? 1 : v); }
static inline uint16_t toPix(float r, float g, float b) {
  int R = (int)(r * 255.0f + 0.5f), G = (int)(g * 255.0f + 0.5f), B = (int)(b * 255.0f + 0.5f);
  R = R < 0 ? 0 : R > 255 ? 255 : R; G = G < 0 ? 0 : G > 255 ? 255 : G; B = B < 0 ? 0 : B > 255 ? 255 : B;
  return __builtin_bswap16((uint16_t)(((R & 0xF8) << 8) | ((G & 0xFC) << 3) | (B >> 3)));
}
// 光を足す(r, g, b は 0..255 くらい。a: 0..32 の濃さ)
static inline __attribute__((always_inline)) uint16_t addLight(uint16_t p, int r, int g, int b, uint32_t a) {
  const uint32_t c = __builtin_bswap16(p);
  int R = (int)(c >> 11) + ((r * (int)a) >> 8), G = (int)((c >> 5) & 63) + ((g * (int)a) >> 7), B = (int)(c & 31) + ((b * (int)a) >> 8);
  R = R > 31 ? 31 : R; G = G > 63 ? 63 : G; B = B > 31 ? 31 : B;
  return __builtin_bswap16((uint16_t)((R << 11) | (G << 5) | B));
}
// 光を重ねる(明るい所ほど足しにくい。r, g, b: 0..255 / w: 0..256)
static inline __attribute__((always_inline)) uint16_t screenAdd(uint16_t p, int r, int g, int b, int w) {
  const uint32_t c = __builtin_bswap16(p);
  int R = c >> 11, G = (c >> 5) & 63, B = c & 31;
  R += ((31 - R) * r * w) >> 16; G += ((63 - G) * g * w) >> 16; B += ((31 - B) * b * w) >> 16;
  return __builtin_bswap16((uint16_t)((R << 11) | (G << 5) | B));
}
// かすみの色をまぜる(f: 0..32)。赤・緑・青を 32 ビットに広げて、3つ一度にまぜる
static uint32_t fogSpread = 0;
static inline __attribute__((always_inline)) uint16_t fogMix(uint16_t p, uint32_t f) {
  if (!f) return p;
  uint32_t c = __builtin_bswap16(p);
  c = (c | (c << 16)) & 0x07E0F81Fu;
  c = ((c * (32 - f) + fogSpread * f) >> 5) & 0x07E0F81Fu;
  return __builtin_bswap16((uint16_t)(c | (c >> 16)));
}
static inline uint32_t spread565(uint16_t p) { const uint32_t c = __builtin_bswap16(p); return (c | (c << 16)) & 0x07E0F81Fu; }
static inline uint16_t unspread565(uint32_t v) { v &= 0x07E0F81Fu; return __builtin_bswap16((uint16_t)(v | (v >> 16))); }
static inline uint16_t mix565(uint16_t a, uint16_t b, uint32_t w) {   // 2つの色をまぜる(w: 0..32)
  return unspread565(((spread565(a) * (32 - w) + spread565(b) * w) >> 5));
}
static inline float fsin(float x) { return sinT[(int)(x * (1024.0f / 6.2831853f)) & 1023]; }
// 段 L の絵の (u, v) の場所。4行ずつの帯にして、帯の中は「縦に4点」を横へ並べる(32 バイトに 4x4 点が入る)
static inline uint32_t tStride(int L) { return ((uint32_t)T >> L) * 4 + 48; }   // 帯1本の長さ(少しすき間をあけて、キャッシュの同じ所にばかり入らないように)
static inline __attribute__((always_inline)) uint32_t tIdx(int L, uint32_t u, uint32_t v) {
  return (v >> 2) * tStride(L) + (u << 2) + (v & 3);
}
static inline uint32_t hash32(uint32_t x) { x ^= x >> 16; x *= 0x7feb352d; x ^= x >> 15; x *= 0x846ca68b; x ^= x >> 16; return x; }

// 上下左右がつながるノイズ(格子の間隔 P で、くり返す)
static inline float vnP(float x, float y, int P, int ox, int oy) {
  const int xi = (int)floorf(x), yi = (int)floorf(y);
  float fx = x - xi, fy = y - yi;
  fx = fx * fx * (3 - 2 * fx); fy = fy * fy * (3 - 2 * fy);
  int x0 = xi % P; if (x0 < 0) x0 += P;
  int y0 = yi % P; if (y0 < 0) y0 += P;
  const int x1 = (x0 + 1) % P, y1 = (y0 + 1) % P;
  const uint8_t* r0 = latt + ((y0 + oy) & 511) * 512;
  const uint8_t* r1 = latt + ((y1 + oy) & 511) * 512;
  const float a = r0[(x0 + ox) & 511], b = r0[(x1 + ox) & 511], c = r1[(x0 + ox) & 511], d = r1[(x1 + ox) & 511];
  return ((a * (1 - fx) + b * fx) * (1 - fy) + (c * (1 - fx) + d * fx) * fy) * (1.0f / 255.0f);
}
static float fbmP(float x, float y, int P, int oct, float gain, int seed) {   // 0..1(まん中 0.5)
  float v = 0, a = 1, t = 0;
  for (int i = 0; i < oct; ++i) {
    v += a * vnP(x, y, P, (seed * 97 + i * 131) & 511, (seed * 53 + i * 71) & 511);
    t += a; a *= gain; x *= 2; y *= 2; P *= 2;
  }
  return v / t;
}

static void showMsg(const char* msg, float prog);

// ---------------------------------------------------------------- 雲の模様を作る
// 雲の面の 1 = ノイズ 6 マス。模様1枚(4x4)= 24 マスで、上下左右がつながる
// 雲の模様は3段で作る
//   1. 雲の多い所・少ない所(ゆるいうねり)と、大きな切れ間(cloudRows。2つのコアで1行おき)
//   2. 積雲のつぶ: 大きなつぶ(半径 9〜20 点)を、まばらな格子に1つずつ置き、その上に小さなつぶ(半径 3〜6 点)を重ねる。
//      つぶは丸い山の形で、重なった所は高い方を取る(高さの絵 hp)
//   3. 高さの絵から、雲の濃さと、光の当たり方(上・左奥から光が来る)を作る(shadeRows。2つのコアで1行おき)
static uint16_t* hp = nullptr;                     // 積雲のつぶの高さ(0..65535)[T * T]。色の絵の置き場を、いっとき借りる
static constexpr size_t nmOa14 = nmAlignTo(nmOv14, 16), nmOv15 = nmOa14 + sizeof(uint16_t[257]); static uint16_t (&domeL)[257] = *reinterpret_cast<uint16_t(*)[257]>(NM_ARENA + nmOa14);                        // 丸い山の形(中心からの距離の2乗 0..1 → 高さ)
static void cloudRows(int y0, int step, bool show) {
  const float k = 24.0f / T;
  for (int y = y0; y < T; y += step) {
    if (y != y0 && ((y - y0) & 15) == 0) vTaskDelay(1);   // ときどき休む(コア0 は、5秒休まないと見張りに止められて再起動になる)
    if (show && (y & 63) == 0) showMsg(nullptr, 0.05f + 0.40f * y / T);
    for (int x = 0; x < T; ++x) {
      const float u = x * k, v = y * k;
      const float n = fbmP(u, v, 24, 4, 0.55f, 0);
      const float big = fbmP(x * (2.0f / T), y * (2.0f / T), 2, 3, 0.5f, 3);
      const float hole = clamp01((big - 0.30f) / 0.05f);   // 雲の切れ間
      const float cov = clamp01((n - 0.20f + (big - 0.5f) * 0.25f) * 2.2f) * hole;   // 雲の多さ
      const size_t i = (size_t)y * T + x;
      cS[i] = (uint8_t)(cov * 255.0f);             // 雲の多さ(3. で陰に作りかえる)
      cH[i] = (uint8_t)(hole * 255.0f);
    }
  }
}
static void puff(float cx, float cy, float R, float hgt, float base) {   // 丸い山を1つ置く(高い方を取る)。上下左右はつながっている
  const int r = (int)R + 1, ix = (int)floorf(cx), iy = (int)floorf(cy);
  const float iR2 = 256.0f / (R * R);
  for (int dy = -r; dy <= r; ++dy) {
    const float fy = iy + dy - cy;
    uint16_t* row = hp + (size_t)((iy + dy) & TM) * T;
    for (int dx = -r; dx <= r; ++dx) {
      const float fx = ix + dx - cx, t = (fx * fx + fy * fy) * iR2;
      if (t >= 256.0f) continue;
      const int h = (int)((base + hgt * domeL[(int)t] * (1.0f / 65535.0f)) * 65535.0f);
      uint16_t& q = row[(ix + dx) & TM];
      if (h > q) q = (uint16_t)(h > 65535 ? 65535 : h);
    }
  }
}
static void puffClouds() {
  for (int i = 0; i <= 256; ++i) domeL[i] = (uint16_t)(65535.0f * powf(1.0f - i / 256.0f, 0.55f));   // 上がまるい山
  memset(hp, 0, (size_t)T * T * 2);
  rnd = 4711;
  const int G = 72;                                // 大きなつぶ: 72x72 の格子に1つずつ
  const float cell = (float)T / G;
  for (int gy = 0; gy < G; ++gy) {
    if ((gy & 7) == 0) { vTaskDelay(1); showMsg(nullptr, 0.45f + 0.25f * gy / G); }
    for (int gx = 0; gx < G; ++gx) {
      const float cx = (gx + 0.5f + (frand() - 0.5f) * 0.8f) * cell, cy = (gy + 0.5f + (frand() - 0.5f) * 0.8f) * cell;
      const float cov = cS[(size_t)((int)cy & TM) * T + ((int)cx & TM)] * (1.0f / 255.0f);
      const float rr = frand();
      if (cov < 0.08f + rr * 0.15f) continue;      // 雲の少ない所は、つぶもまばら
      puff(cx, cy, cell * (0.55f + 0.42f * rr) * (0.72f + 0.40f * cov), 0.45f + 0.40f * cov + 0.15f * frand(), 0.0f);
    }
  }
  const int G2 = 200;                              // 小さなつぶ: 大きなつぶの上に重ねる(もくもく)
  const float cell2 = (float)T / G2;
  for (int gy = 0; gy < G2; ++gy) {
    if ((gy & 15) == 0) { vTaskDelay(1); showMsg(nullptr, 0.70f + 0.15f * gy / G2); }
    for (int gx = 0; gx < G2; ++gx) {
      const float cx = (gx + 0.5f + (frand() - 0.5f)) * cell2, cy = (gy + 0.5f + (frand() - 0.5f)) * cell2;
      const float h0 = hp[(size_t)((int)cy & TM) * T + ((int)cx & TM)] * (1.0f / 65535.0f);
      const float rr = frand();
      if (h0 < 0.15f) continue;
      puff(cx, cy, cell2 * (0.45f + 0.45f * rr), 0.10f + 0.10f * frand(), h0 * 0.92f);
    }
  }
}
static void shadeRows(int y0, int step) {         // 高さの絵 → 雲の濃さ(cD)と光の当たり方(cS)
  const float Lx = -0.40f, Ly = 0.78f, Lz = 0.48f;   // 光の来る向き(左・上・奥)
  const float sK = 14.0f / 65535.0f;               // 高さの傾きの強さ
  for (int y = y0; y < T; y += step) {
    if (y != y0 && ((y - y0) & 31) == 0) vTaskDelay(1);
    const uint16_t* r0 = hp + (size_t)((y - 1) & TM) * T; const uint16_t* r1 = hp + (size_t)y * T; const uint16_t* r2 = hp + (size_t)((y + 1) & TM) * T;
    for (int x = 0; x < T; ++x) {
      const float H = r1[x] * (1.0f / 65535.0f);
      const float gx = (float)(r1[(x + 1) & TM] - r1[(x - 1) & TM]) * 0.5f * sK, gz = (float)(r2[x] - r0[x]) * 0.5f * sK;
      const float dt = (-gx * Lx + Ly - gz * Lz) / sqrtf(1.0f + gx * gx + gz * gz);   // 面が光の方を向いているほど明るい
      float sh = clamp01(dt * 1.25f - 0.25f) * (0.70f + 0.30f * H);   // くぼみは少し暗く
      const size_t i = (size_t)y * T + x;
      const float hole = cH[i] * (1.0f / 255.0f);
      cD[i] = (uint8_t)(clamp01((H - 0.03f) / 0.30f) * hole * 255.0f);
      cS[i] = (uint8_t)(sh * 255.0f);
    }
  }
}
static void blur3(uint8_t* a, uint8_t* tmp) {
  for (int y = 0; y < T; ++y) {
    const uint8_t* r = a + (size_t)y * T;
    uint8_t* t = tmp + (size_t)y * T;
    for (int x = 0; x < T; ++x) t[x] = (uint8_t)((r[(x - 1) & TM] + 2 * r[x] + r[(x + 1) & TM] + 2) >> 2);
  }
  for (int y = 0; y < T; ++y) {
    const uint8_t* u = tmp + (size_t)((y - 1) & TM) * T; const uint8_t* m = tmp + (size_t)y * T; const uint8_t* d = tmp + (size_t)((y + 1) & TM) * T;
    uint8_t* o = a + (size_t)y * T;
    for (int x = 0; x < T; ++x) o[x] = (uint8_t)((u[x] + 2 * m[x] + d[x] + 2) >> 2);
  }
}
static void buildClouds() {
  rnd = 777;
  for (int i = 0; i < 512 * 512; ++i) latt[i] = (uint8_t)(frand() * 256.0f);
  if (semGo) { job = 1; xSemaphoreGive(semGo); cloudRows(0, 2, true); xSemaphoreTake(semDone, portMAX_DELAY); job = 0; }
  else cloudRows(0, 1, true);
  hp = tex;                                        // 色の絵の置き場を、いっとき借りる(このあと作りなおす)
  puffClouds();
  showMsg(nullptr, 0.88f);
  if (semGo) { job = 8; xSemaphoreGive(semGo); shadeRows(0, 2); xSemaphoreTake(semDone, portMAX_DELAY); job = 0; }
  else shadeRows(0, 1);
  uint8_t* tmp = (uint8_t*)tex;
  blur3(cD, tmp); blur3(cH, tmp);                 // 陰(cS)はぼかさない(つぶの形をくっきり)
  hp = nullptr;
}
// 時間帯 t の表を作る(雲の色の表・空・光輪・色)
static void buildTod(int t) {
  const Tod& C = TODS[t];
  uint16_t* lut = luts + (size_t)t * NKEY;        // 雲の色: 「濃さ 32段 × 陰 32段 × 切れ間 8段」
  for (int di = 0; di < 32; ++di)
    for (int si = 0; si < 32; ++si)
      for (int hi = 0; hi < 8; ++hi) {
        const float d = di * (1.0f / 31.0f), h = hi * (1.0f / 7.0f);
        const float sl = si * (1.0f / 31.0f);       // 光の当たり方(0 = 陰 〜 1 = 光の方を向いた面)
        const float lt = powf(sl, 1.3f), thin = clamp01(1.0f - d * 1.6f) * 0.30f;
        const float gm = 0.55f + 0.45f * sl;
        float c[3];
        for (int j = 0; j < 3; ++j) {
          const float cl = (C.amb[j] * 0.62f + (C.lit[j] * 0.90f - C.amb[j] * 0.62f) * lt) * (1 - thin) + C.amb[j] * 0.70f * thin;   // 陰は青みの灰色、光の当たる所は白
          const float gp = C.gap[j] * gm * h + C.sea[j] * (1 - h);
          c[j] = gp * (1 - d) + cl * d;
        }
        lut[(di << 8) | (si << 3) | hi] = toPix(c[0], c[1], c[2]);
      }
  TodTab& tb = tabs[t];
  for (int i = 0; i < 256; ++i) {                  // 空(地平線からの高さ 0..1.6)
    const float sd = i / 160.0f, e10 = expf(-sd * 10.0f), e3 = expf(-sd * 3.0f);
    tb.sky[i] = toPix(C.top[0] + C.bnd[0] * e10 + C.mid[0] * e3, C.top[1] + C.bnd[1] * e10 + C.mid[1] * e3, C.top[2] + C.bnd[2] * e10 + C.mid[2] * e3);
  }
  for (int i = 0; i < 1024; ++i) {                 // 光輪: まん中がほんのり白く、細い虹の輪が2重
    const float r = sqrtf(i / 1023.0f) * 1.75f;
    float g[3] = {0.88f, 0.93f, 1.0f};
    const float w0 = expf(-(r / 0.45f) * (r / 0.45f)) * 0.22f;
    for (int j = 0; j < 3; ++j) g[j] *= w0;
    const float ring[2][3] = {{0.94f, 0.050f, 1.25f}, {1.30f, 0.045f, 0.28f}};
    for (int q = 0; q < 2; ++q) {
      const float qq = (r - ring[q][0]) / ring[q][1], amp = expf(-qq * qq * 0.30f) * ring[q][2];
      const float hue = (1.0f - clamp01(qq * 0.40f + 0.5f)) * 0.78f;   // 内側が青紫、外側が赤
      const float cr = clamp01(fabsf(hue * 6 - 3) - 1), cg = clamp01(2 - fabsf(hue * 6 - 2)), cb = clamp01(2 - fabsf(hue * 6 - 4));
      g[0] += cr * amp; g[1] += cg * amp; g[2] += cb * amp;
    }
    if (C.moon) { const float m = (g[0] + g[1] + g[2]) / 3.0f; for (int j = 0; j < 3; ++j) g[j] = m * 0.6f + g[j] * 0.4f; }
    tb.gr[i] = (uint8_t)(clamp01(g[0] * 0.85f) * 255.0f); tb.gg[i] = (uint8_t)(clamp01(g[1] * 0.85f) * 255.0f); tb.gb[i] = (uint8_t)(clamp01(g[2] * 0.85f) * 255.0f);
  }
  for (int j = 0; j < 3; ++j) { tb.fog[j] = C.fog[j]; tb.limb[j] = C.limb[j]; tb.sun[j] = C.sun[j]; }
  tb.on = C.sunx < 5.0f; tb.sunx = C.sunx; tb.glory = C.glory; tb.stars = C.stars ? 1.0f : 0.0f;
}
// 時間帯によらない表(地平線の光る帯・太陽の光の形)
static void buildFixedTabs() {
  for (int i = 0; i < 256; ++i) {
    const float dd = i / 1020.0f;                  // 地平線の光る帯(0..0.25)
    limbL[i] = (uint8_t)fminf(32.0f, 32.0f * (0.5f * expf(-(dd / 0.012f) * (dd / 0.012f)) + 0.16f * expf(-(dd / 0.06f) * (dd / 0.06f))));
    const float dx = (i - 128) / 64.0f;            // 太陽: 横のずれ(-2..2)
    sunX[i] = (uint8_t)(255.0f * expf(-(dx / 0.35f) * (dx / 0.35f)));
    sunD[i] = (uint8_t)(255.0f * expf(-(i / 255.0f) / 0.25f));   // 雲の面の深さ(0..1)
    sunS[i] = (uint8_t)(255.0f * expf(-((i / 160.0f) * 2.5f) * ((i / 160.0f) * 2.5f) / 0.08f));   // 空の高さ
  }
}
// 雲の模様の番号の絵を作る(濃さ・陰・切れ間 → 番号)。縮めた絵は、濃さ・陰・切れ間をそれぞれ平均して番号にする
static inline uint32_t keyOf(uint32_t d, uint32_t sh, uint32_t h) { return (d << 8) | (sh << 3) | h; }   // d, sh: 0..31、h: 0..7
static void buildKeys() {
  for (uint32_t y = 0; y < (uint32_t)T; ++y) {
    if ((y & 63) == 0) vTaskDelay(1);
    const uint8_t* rd = cD + (size_t)y * T; const uint8_t* rs = cS + (size_t)y * T; const uint8_t* rh = cH + (size_t)y * T;
    for (uint32_t x = 0; x < (uint32_t)T; ++x) tex[tIdx(0, x, y)] = (uint16_t)keyOf((rd[x] * 31 + 127) / 255, (rs[x] * 31 + 127) / 255, (rh[x] * 7 + 127) / 255);
  }
  for (int L = 1; L < NMIP; ++L) {
    const uint32_t n = (uint32_t)T >> L;
    for (uint32_t v = 0; v < n; ++v)
      for (uint32_t u = 0; u < n; ++u) {
        uint32_t d = 0, sh = 0, h = 0;
        for (int k = 0; k < 4; ++k) {
          const uint32_t q = lev[L - 1][tIdx(L - 1, u * 2 + (k & 1), v * 2 + (k >> 1))];
          d += q >> 8; sh += (q >> 3) & 31; h += q & 7;
        }
        lev[L][tIdx(L, u, v)] = (uint16_t)keyOf((d + 2) >> 2, (sh + 2) >> 2, (h + 2) >> 2);
      }
  }
}
// 2つの時間帯の表を、b(0..1)の割合でまぜて、描くときに使う表にする
static void applyBlend(float b) {
  const TodTab& A = tabs[todFrom]; const TodTab& B = tabs[tod];
  const uint32_t w = (uint32_t)(b * 32.0f + 0.5f), w8 = (uint32_t)(b * 256.0f + 0.5f);
  for (int i = 0; i < 256; ++i) skyL[i] = mix565(A.sky[i], B.sky[i], w);
  { const uint16_t* la = luts + (size_t)todFrom * NKEY; const uint16_t* lb = luts + (size_t)tod * NKEY;
    if (w >= 32) memcpy(clut, lb, sizeof(clut)); else for (int i = 0; i < NKEY; ++i) clut[i] = mix565(la[i], lb[i], w); }
  for (int i = 0; i < 1024; ++i) {
    glR[i] = (uint8_t)((A.gr[i] * (256 - w8) + B.gr[i] * w8) >> 8);
    glG[i] = (uint8_t)((A.gg[i] * (256 - w8) + B.gg[i] * w8) >> 8);
    glB[i] = (uint8_t)((A.gb[i] * (256 - w8) + B.gb[i] * w8) >> 8);
    glOn[i] = glR[i] >= 9 || glG[i] >= 5 || glB[i] >= 9;   // これより弱いと、重ねても色が1段も変わらない
  }
  glCum[0] = 0;
  for (int i = 0; i < 1024; ++i) glCum[i + 1] = (uint16_t)(glCum[i] + glOn[i]);
  float f[3], l[3], su[3];
  const float sa = A.on ? 1.0f - b : 0.0f, sb = B.on ? b : 0.0f;
  for (int j = 0; j < 3; ++j) { f[j] = A.fog[j] + (B.fog[j] - A.fog[j]) * b; l[j] = A.limb[j] + (B.limb[j] - A.limb[j]) * b; su[j] = A.sun[j] * sa + B.sun[j] * sb; }
  fogR = (int)(f[0] * 31.0f); fogG = (int)(f[1] * 63.0f); fogB = (int)(f[2] * 31.0f);
  { const uint32_t c = (uint32_t)((fogR << 11) | (fogG << 5) | fogB); fogSpread = (c | (c << 16)) & 0x07E0F81Fu; }
  limbR = (int)(l[0] * 255.0f); limbG = (int)(l[1] * 255.0f); limbB = (int)(l[2] * 255.0f);
  sunR = (int)(su[0] * 255.0f); sunG = (int)(su[1] * 255.0f); sunB = (int)(su[2] * 255.0f);
  sunOn = (sa + sb) > 0.01f;
  sunXi = (int)((A.on ? A.sunx : B.sunx) * 64.0f);
  starW = A.stars + (B.stars - A.stars) * b;
  gloryW = A.glory + (B.glory - A.glory) * b;
}
// 時間帯を t にする(雲の色は、このあと stepTod で少しずつ塗りかえる)
static void startTod(int t) {
  todFrom = transT >= 0.5f ? tod : todFrom;      // 移りかわりの途中なら、近い方から始める
  tod = t; transT = 0.0f;
  Serial.printf("time of day: %s\n", TOD_NAME[tod]);
}
static void stepTod(float dt) {
  if (transT >= 1.0f) return;
  transT = fminf(1.0f, transT + dt / TRANS_SEC);
  const float b = transT * transT * (3.0f - 2.0f * transT);   // ゆっくり始まって、ゆっくり終わる
  applyBlend(b);
}

// ---------------------------------------------------------------- 見え方(画面 ⇔ 雲の面)
// 画面の点(-1..1)→ 地平線の傾きを戻した点 → 雲の面(見ている人から見た X: 右 / Z: 奥)
static constexpr size_t nmOa15 = nmAlignTo(nmOv15, 16), nmOv16 = nmOa15 + sizeof(int32_t[1024]); static int32_t (&fogLut)[1024] = *reinterpret_cast<int32_t(*)[1024]>(NM_ARENA + nmOa15);                       // かすみ(距離の2乗 → 0..32 × 65536)
struct Samp { bool plane; float d, rx; uint32_t U, V; int32_t f; };
static inline __attribute__((always_inline)) Samp sampleAt(float nx, float ny) {
  Samp s;
  s.rx = vCr * nx + vSr * ny;
  const float ry = -vSr * nx + vCr * ny;
  s.d = ry - (H0 + 0.12f * s.rx * s.rx);           // 地平線からの深さ(マイナスは空)
  if (s.d < 0.002f) { s.plane = false; s.U = s.V = 0; s.f = 0; return s; }
  s.plane = true;
  const float Z = 1.0f / (s.d * 1.25f + 0.04f), X = s.rx * Z * 0.85f;
  const float Xw = vCy * X + vSy * Z, Zw = -vSy * X + vCy * Z;   // 見ている向き
  s.U = (uint32_t)(int32_t)((Xw + scrX) * (TPU * 65536.0f));
  s.V = (uint32_t)(int32_t)((Zw + scrZ) * (TPU * 65536.0f));
  const float q = X * X + Z * Z;                   // 見ている所までの距離の2乗 → かすみ(表を引く)
  s.f = fogLut[q < 1023.0f ? (int)q : 1023];
  return s;
}
static bool camToScreen(float X, float Z, float& sx, float& sy) {   // 雲の面 → 画面(点)
  if (Z < 0.05f) return false;
  const float d = (1.0f / Z - 0.04f) / 1.25f, rx = X / (Z * 0.85f), ry = d + H0 + 0.12f * rx * rx;
  const float nx = vCr * rx - vSr * ry, ny = vSr * rx + vCr * ry;
  sx = S * 0.5f + nx * S * 0.5f; sy = S * 0.5f + ny * S * 0.5f;
  return true;
}
static bool screenToCam(float sx, float sy, float& X, float& Z) {   // 画面(点)→ 雲の面
  const float nx = (sx - S * 0.5f) / (S * 0.5f), ny = (sy - S * 0.5f) / (S * 0.5f);
  const Samp s = sampleAt(nx, ny);
  if (!s.plane) return false;
  Z = 1.0f / (s.d * 1.25f + 0.04f); X = s.rx * Z * 0.85f;
  return true;
}

// ---------------------------------------------------------------- 光輪と星
static void initGlories() {
  // 雲の面の上の場所(横・奥)・輪の半径(雲の面の上で)・濃さ。下の方のまん中に大きい輪、左右と下のはしに小さめの輪
  const float P[NFIX][4] = {{0.0f, 0.95f, 0.46f, 1.0f}, {-1.20f, 1.25f, 0.24f, 0.8f}, {1.30f, 1.40f, 0.24f, 0.8f},
                            {-0.80f, 0.58f, 0.17f, 0.75f}, {0.85f, 0.58f, 0.17f, 0.75f}, {1.30f, 3.8f, 0.34f, 0.6f}};
  for (int i = 0; i < MAXG; ++i) { gl[i] = Glory(); gl[i].y0 = 1; }
  for (int i = 0; i < NFIX; ++i) { gl[i] = Glory(); gl[i].gx = P[i][0]; gl[i].gz = P[i][1]; gl[i].R = P[i][2]; gl[i].A = P[i][3]; gl[i].on = gl[i].fixed = true; }
}
static void addGlory(float sx, float sy) {
  float X, Z;
  if (!screenToCam(sx, sy, X, Z)) return;
  Glory& g = gl[glNext]; glNext = glNext + 1 >= MAXG ? NFIX : glNext + 1;
  g = Glory();
  g.gx = vCy * X + vSy * Z; g.gz = -vSy * X + vCy * Z;
  g.R = 0.22f + 0.18f * clamp01((Z - 1.0f) / 3.0f) + frand() * 0.06f; g.A = 0.9f; g.life = 22.0f + frand() * 8.0f; g.on = true;
}
static void initStars() {
  rnd = 4242;
  for (int i = 0; i < NSTAR; ++i) {
    const float q = frand();
    stars[i] = {frand() * 2.8f - 1.4f, 0.04f + frand() * 1.6f, 0.15f + q * q * q * 0.85f, frand() * 6.28f, 0, 0, false};
  }
}
static void updateGlories(float dt) {
  for (int i = 0; i < MAXG; ++i) {
    Glory& g = gl[i];
    if (!g.on) { g.y0 = 1; g.y1 = 0; continue; }
    float A = g.A;
    if (!g.fixed) {                                // 触って生まれた光輪: ふわっと出て、しばらくして消える
      g.age += dt;
      if (g.age > g.life) { g.on = false; g.y0 = 1; g.y1 = 0; continue; }
      A *= clamp01(g.age / 1.5f) * clamp01((g.life - g.age) / 4.0f);
    }
    g.a = A * gloryW * 0.65f;
    // 少しずつすべって、大きさもゆらぐ
    const float wx = g.gx + 0.05f * fsin(simT * 0.9f + i), wz = g.gz + 0.05f * fsin(simT * 0.7f + i * 1.7f + 1.57f);
    // 輪は雲の面に寝た丸を斜めから見た楕円。輪の中心での見え方(向き・かたむき・つぶれ方)で形を決めるので、輪の太さはどこでもそろう。
    // 場所は自分についてくる(雲の流れには乗らない)。向きや傾きを変えると、形も見え方どおりに変わる
    const float R = g.R * (1.0f + 0.03f * fsin(simT * 1.3f + i));
    const float X0 = vCy * wx - vSy * wz, Z0 = vSy * wx + vCy * wz;
    float sx, sy, ax, ay, bx, by;
    const float e = 0.01f;
    if (!camToScreen(X0, Z0, sx, sy) || !camToScreen(X0 + e, Z0, ax, ay) || !camToScreen(X0, Z0 + e, bx, by) || g.a < 0.01f) { g.y0 = 1; g.y1 = 0; continue; }
    const float j11 = (ax - sx) / e, j21 = (ay - sy) / e, j12 = (bx - sx) / e, j22 = (by - sy) / e;   // 雲の面 → 画面(輪の中心で)
    const float det = j11 * j22 - j12 * j21;
    if (fabsf(det) < 1e-3f) { g.y0 = 1; g.y1 = 0; continue; }
    // 楕円の長い軸は、雲の面の「横」の向き(地平線にそう向き)。長さは横の伸び、短い軸は面積が合うように(ななめに引き伸ばさない)
    const float lu = sqrtf(j11 * j11 + j21 * j21);
    if (lu < 1e-3f) { g.y0 = 1; g.y1 = 0; continue; }
    const float ux = j11 / lu, uy = j21 / lu;      // 長い軸の向き(画面)
    const float au = 1.75f * R * lu, av = 1.75f * R * fabsf(det) / lu;   // 輪の外側までの半径(画面の点)
    const float iu = 1023.0f / (au * au), iv = 1023.0f / (av * av);
    g.cx = sx; g.cy = sy;
    g.qa = ux * ux * iu + uy * uy * iv; g.qb = 2.0f * ux * uy * (iu - iv); g.qc = uy * uy * iu + ux * ux * iv;
    g.wa = (int32_t)(g.a * 256.0f);
    const float hx = sqrtf(au * au * ux * ux + av * av * uy * uy), hy = sqrtf(au * au * uy * uy + av * av * ux * ux);   // 楕円のはしまで
    g.x0 = max(0, (int)(sx - hx) - 2); g.x1 = min(S - 1, (int)(sx + hx) + 2); g.y0 = max(0, (int)(sy - hy) - 2); g.y1 = min(S - 1, (int)(sy + hy) + 2);
    if (g.x0 > g.x1 || g.y0 > g.y1) { g.y0 = 1; g.y1 = 0; }
  }
}
static void updateStars() {
  for (int i = 0; i < NSTAR; ++i) {
    Star& s = stars[i];
    float rx = s.rx - yaw * 0.6f;                  // 向きを回すと、星も横に流れる(端から端へ回りこむ)
    rx -= floorf((rx + 1.4f) / 2.8f) * 2.8f;
    const float ry = H0 + 0.12f * rx * rx - s.sd;
    const float nx = vCr * rx - vSr * ry, ny = vSr * rx + vCr * ry;
    s.sx = (int)(S * 0.5f + nx * S * 0.5f); s.sy = (int)(S * 0.5f + ny * S * 0.5f);
    s.vis = s.sx >= 0 && s.sy >= 0 && s.sx < S && s.sy < S;
  }
}

// ---------------------------------------------------------------- 動き
static void simulate(float dt) {
  simT += dt;
  const float sp = SPEEDS[speedIdx];
  scrX += dt * WIND * sp; scrZ += dt * WIND * sp * 0.24f;
  while (scrX >= TW) scrX -= TW;
  while (scrZ >= TW) scrZ -= TW;
  if (!grabbing) pitchOff *= expf(-dt * 0.30f);   // はなすと、見下ろす角度はゆっくり戻る
  const float a = 1.0f - expf(-dt * (grabbing ? 14.0f : 3.0f));
  const float rollGoal = tiltX * 0.45f * TILT_GAIN;
  float hGoal = -0.60f + tiltY * TILT_PITCH * TILT_GAIN + pitchOff;
  hGoal = hGoal < -1.05f ? -1.05f : (hGoal > -0.08f ? -0.08f : hGoal);
  roll += (rollGoal - roll) * a; H0 += (hGoal - H0) * a;
  vCr = cosf(roll); vSr = sinf(roll); vCy = cosf(yaw); vSy = sinf(yaw);
  updateGlories(dt);
  if (starW > 0.01f) updateStars();
}

// ---------------------------------------------------------------- 描く
// どの段の絵から引くか: 1点となりへ進むと、模様の何点ぶん動くか(横は dUV、縦は奥行きから)
static float mipRowK = 0;                          // 1行下がると、奥行き Z が Z^2 の何倍動くか(模様の点)
static inline int mipLevel(int32_t dUV, float d, float rows) {   // dUV: 1歩で動く模様の点(16.16)、rows: 1歩が何行ぶんか
  const float Z = 1.0f / (fmaxf(d, 0.002f) * 1.25f + 0.04f);
  float fp = Z * Z * mipRowK * rows;
  const float h = dUV * (1.0f / 65536.0f);
  if (h > fp) fp = h;
  return (fp > 1.6f) + (fp > 3.2f) + (fp > 6.4f) + (fp > 12.8f);   // 0..4
}
// 雲の面の点に、太陽の照り返しと地平線の光る帯を足す(D: 地平線からの深さ×65536、RX: 横×64×65536)
static inline __attribute__((always_inline)) uint16_t planeExtra(uint16_t p, int32_t D, int32_t RX, int sunXi, bool SUN) {
  if (D < 0) D = 0;
  if (SUN && sunOn) {
    int xi = (RX >> 16) - sunXi + 128; xi = xi < 0 ? 0 : (xi > 255 ? 255 : xi);
    int di = (D * 255) >> 16; di = di > 255 ? 255 : di;
    const uint32_t a = (sunX[xi] * sunD[di]) >> 11;
    if (a) p = addLight(p, sunR, sunG, sunB, (a * 18) >> 5);
  }
  if (D < 16384) { const uint32_t a = limbL[(D * 1020) >> 16]; if (a) p = addLight(p, limbR, limbG, limbB, a >> 1); }
  return p;
}
// 空の点
static inline __attribute__((always_inline)) uint16_t skyPix(int32_t D, int32_t RX, int sunXi, bool SUN) {
  int32_t sd = -D; if (sd < 0) sd = 0;
  int si = (sd * 160) >> 16; si = si > 255 ? 255 : si;
  uint16_t p = skyL[si];
  if (SUN && sunOn) {
    int xi = (RX >> 16) - sunXi + 128; xi = xi < 0 ? 0 : (xi > 255 ? 255 : xi);
    const uint32_t a = (sunX[xi] * sunS[si]) >> 11;
    if (a) p = addLight(p, sunR, sunG, sunB, (a * 26) >> 5);
  }
  if (sd < 16384) { const uint32_t a = limbL[(sd * 1020) >> 16]; if (a) p = addLight(p, limbR, limbG, limbB, a); }
  return p;
}
// 雲の面の区間を描く(EXTRA: 太陽の照り返し・地平線の帯も足す)
// 2x2 点に1回だけ計算する(粗い点)。あいだの点は、まわりの粗い点からなめらかにつなぐ。
// 地平線の近く(|d| < D_EXACT)だけは、1点ずつきちんと計算する(地平線のふちと光る帯をぼかさない)
static constexpr int32_t D_EXACT = 1966;           // 0.03 × 65536
static constexpr int CROWS = BAND / 2 + 1;         // 帯ごとの粗い行(次の帯の1行目まで)
struct Coarse { uint32_t* c; uint8_t* e; };        // c: 色(赤・緑・青を 32 ビットに広げた形)、e: 1 = この点のまわりは1点ずつ計算
static Coarse crs[2];
static int CW = 0;                                 // 粗い行の長さ
static inline __attribute__((always_inline)) uint32_t spread(uint16_t p) { const uint32_t c = __builtin_bswap16(p); return (c | (c << 16)) & 0x07E0F81Fu; }
static inline __attribute__((always_inline)) uint16_t unspread(uint32_t v) { v &= 0x07E0F81Fu; return __builtin_bswap16((uint16_t)(v | (v >> 16))); }
// 光輪を1点に重ねる(x, y: 画面の点、f: かすみ 0..32、dens: 雲の濃さ 0..248)
static inline __attribute__((always_inline)) uint16_t gloryAdd(const Glory& g, uint16_t p, int x, int y, int f, int dens) {
  const float dx = x - g.cx, dy = y - g.cy;
  const float q = (g.qa * dx + g.qb * dy) * dx + g.qc * dy * dy;
  if (!(q < 1023.0f)) return p;
  const int32_t qq = (int32_t)q;
  if (qq >= 1024 || !glOn[qq]) return p;
  const int32_t w = (((g.wa * (205 + ((51 * dens) >> 8))) >> 8) * (32 - ((22 * f) >> 5))) >> 5;   // 雲が濃いほど、近いほど少し強い
  return w > 0 ? screenAdd(p, glR[qq], glG[qq], glB[qq], w > 256 ? 256 : w) : p;
}
// 画面の y 行の、x = xa〜xb の区間に、光輪の色のある所がかかるかを先に調べる。かからなければ false
static inline bool spanHitsGlory(const Glory& g, int y, int xa, int xb) {
  const float dy = y - g.cy, da = xa - g.cx, db = xb - g.cx;
  auto qf = [&](float dx) { return (g.qa * dx + g.qb * dy) * dx + g.qc * dy * dy; };
  float dm = g.qa > 0 ? -g.qb * dy / (2.0f * g.qa) : da;   // 区間の中で、いちばん中心に近い所
  dm = dm < da ? da : (dm > db ? db : dm);
  const float qa = qf(dm), qb = fmaxf(qf(da), qf(db));
  if (qa >= 1023.0f) return false;
  const int ia = (int)qa, ib = qb >= 1023.0f ? 1023 : (int)qb + 1;
  return glCum[ib + 1] != glCum[ia];
}
// 区間の横の範囲(rx0〜rx1)が、太陽の光の届く所(太陽の横の位置から ±0.85)にかかるか
static inline bool sunNear(float rx0, float rx1, float sunx) {
  if (!sunOn) return false;
  const float lo = rx0 < rx1 ? rx0 : rx1, hi = rx0 < rx1 ? rx1 : rx0;
  return hi > sunx - 0.85f && lo < sunx + 0.85f;
}
// 1点をきちんと計算する(地平線の近く用)
static uint16_t exactPix(int x, int y, int sunXi) {
  const float hh = S * 0.5f, ih = 1.0f / hh;
  const Samp s = sampleAt((x - hh) * ih, (y - hh) * ih);
  const int32_t D = (int32_t)(s.d * 65536.0f), RX = (int32_t)(s.rx * 4194304.0f);
  if (!s.plane) return skyPix(D, RX, sunXi, true);
  const int L = mipLevel(0, s.d, 1.0f);
  const uint32_t m = TM >> L;
  const uint16_t key = lev[L][tIdx(L, (s.U >> (16 + L)) & m, (s.V >> (16 + L)) & m)];
  uint16_t p = planeExtra(fogMix(clut[key], (uint32_t)s.f >> 16), D, RX, sunXi, true);
  const int dens = (key >> 8) << 3, f = (int)((uint32_t)s.f >> 16);
  for (int i = 0; i < MAXG; ++i) {
    const Glory& g = gl[i];
    if (y < g.y0 || y > g.y1 || x < g.x0 || x > g.x1) continue;
    p = gloryAdd(g, p, x, y, f, dens);
  }
  return p;
}
// 粗い行を1本作る(画面の y 行の、x = bx, bx+2, … の点)。光輪も重ねる
static void coarseRow(int y, int bx, int n, uint32_t* cr, uint8_t* er, int sunXi) {
  const float sunxF = sunXi * (1.0f / 64.0f);
  const float hh = S * 0.5f, ih = 1.0f / hh, ny = (y - hh) * ih;
  Samp s0 = sampleAt((bx - hh) * ih, ny);
  for (int k0 = 0; k0 < n; k0 += 8) {
    const int k1 = min(k0 + 8, n), m = k1 - k0;
    const Samp s1 = sampleAt((bx + 2 * k1 - hh) * ih, ny);
    if (s0.plane == s1.plane) {                    // 同じ側: 足し算でつなぐ
      int32_t D = (int32_t)(s0.d * 65536.0f), RX = (int32_t)(s0.rx * 4194304.0f);
      const int32_t dD = ((int32_t)(s1.d * 65536.0f) - D) / m, dRX = ((int32_t)(s1.rx * 4194304.0f) - RX) / m;
      if (s0.plane) {
        const int32_t dU = (int32_t)(s1.U - s0.U) / m, dV = (int32_t)(s1.V - s0.V) / m, df = (s1.f - s0.f) / m;
        const int L = mipLevel(abs(dU) > abs(dV) ? abs(dU) : abs(dV), s0.d < s1.d ? s0.d : s1.d, 2.0f);
        const uint16_t* lv = lev[L];
        const int sh = 16 + L; const uint32_t mk = TM >> L, strd = tStride(L);
        const bool sn = sunNear(s0.rx, s1.rx, sunxF);
        const bool extra = sn || s0.d < 0.25f || s1.d < 0.25f;
        int ag[MAXG], na = 0;                      // この区間にかかる光輪
        for (int i = 0; i < MAXG; ++i) {
          const Glory& g = gl[i];
          if (y < g.y0 || y > g.y1 || g.x1 < bx + 2 * k0 || g.x0 > bx + 2 * k1) continue;
          if (spanHitsGlory(g, y, bx + 2 * k0, bx + 2 * (k1 - 1))) ag[na++] = i;
        }
        uint32_t U = s0.U, V = s0.V; int32_t f = s0.f;
        for (int k = k0; k < k1; ++k) {
          const uint32_t u = (U >> sh) & mk, v = (V >> sh) & mk;
          const uint16_t key = lv[(v >> 2) * strd + (u << 2) + (v & 3)];
          uint16_t p = fogMix(clut[key], (uint32_t)f >> 16);
          if (extra) p = sn ? planeExtra(p, D, RX, sunXi, true) : planeExtra(p, D, RX, sunXi, false);
          if (na) {
            const int dens = (key >> 8) << 3, ff = (int)((uint32_t)f >> 16), x = bx + 2 * k;
            for (int j = 0; j < na; ++j) { const Glory& g = gl[ag[j]]; if (x >= g.x0 && x <= g.x1) p = gloryAdd(g, p, x, y, ff, dens); }
          }
          cr[k] = spread(p); er[k] = D < D_EXACT;
          U += dU; V += dV; f += df; D += dD; RX += dRX;
        }
      } else {
        if (sunNear(s0.rx, s1.rx, sunxF)) for (int k = k0; k < k1; ++k) { cr[k] = spread(skyPix(D, RX, sunXi, true)); er[k] = -D < D_EXACT; D += dD; RX += dRX; }
        else for (int k = k0; k < k1; ++k) { cr[k] = spread(skyPix(D, RX, sunXi, false)); er[k] = -D < D_EXACT; D += dD; RX += dRX; }
      }
    } else {                                       // 地平線をまたぐ所: 1点ずつ(まわりは、あとで1点ずつ計算しなおす)
      for (int k = k0; k < k1; ++k) { cr[k] = spread(exactPix(bx + 2 * k, y, sunXi)); er[k] = 1; }
    }
    s0 = s1;
  }
}
static void IRAM_ATTR renderBand(int kb, uint16_t* out, int bx, int bw, int core) {
  const int y0 = kb * BAND, h = min(BAND, S - y0);
  const float hh = S * 0.5f, ih = 1.0f / hh;
  const Coarse& cb = crs[core];
  const int n = bw / 2 + 1;                        // 粗い点の数(右はしの1つ先まで)
  for (int r = 0; r * 2 <= h; ++r) coarseRow(y0 + 2 * r, bx, n, cb.c + r * CW, cb.e + r * CW, sunXi);
  for (int rr = 0; rr < h; ++rr) {
    const int y = y0 + rr;
    uint16_t* o = out + rr * bw - bx;
    const int sa = max((int)spanA[y], bx), sb = min((int)spanB[y], bx + bw - 1);
    for (int x = bx; x < sa; ++x) o[x] = 0;
    for (int x = sb + 1 < bx ? bx : sb + 1; x < bx + bw; ++x) o[x] = 0;
    if (sa > sb) continue;
    const int r = rr >> 1;
    const uint32_t* c0 = cb.c + r * CW; const uint8_t* e0 = cb.e + r * CW;
    int x = sa;
    if ((rr & 1) == 0) {                           // 粗い行の上: 偶数の点はそのまま、奇数の点は左右のまん中
      if ((x - bx) & 1) { const int k = (x - bx) >> 1; o[x] = (e0[k] | e0[k + 1]) ? exactPix(x, y, sunXi) : unspread((c0[k] + c0[k + 1]) >> 1); ++x; }
      int k = (x - bx) >> 1;
      for (; x < sb; x += 2, ++k) {
        o[x] = unspread(c0[k]);
        o[x + 1] = (e0[k] | e0[k + 1]) ? exactPix(x + 1, y, sunXi) : unspread((c0[k] + c0[k + 1]) >> 1);
      }
      if (x == sb) o[x] = unspread(c0[k]);
    } else {                                       // 粗い行のあいだ: 上下のまん中 / 4つのまん中(縦の和を使い回す)
      const uint32_t* c1 = c0 + CW; const uint8_t* e1 = e0 + CW;
      int k = (x - bx) >> 1;
      uint32_t m = c0[k] + c1[k]; uint8_t fm = e0[k] | e1[k];
      if ((x - bx) & 1) {
        const uint32_t n = c0[k + 1] + c1[k + 1]; const uint8_t fn = e0[k + 1] | e1[k + 1];
        o[x] = (fm | fn) ? exactPix(x, y, sunXi) : unspread((m + n) >> 2);
        m = n; fm = fn; ++k; ++x;
      }
      for (; x < sb; x += 2, ++k) {
        const uint32_t n = c0[k + 1] + c1[k + 1]; const uint8_t fn = e0[k + 1] | e1[k + 1];
        o[x] = fm ? exactPix(x, y, sunXi) : unspread(m >> 1);
        o[x + 1] = (fm | fn) ? exactPix(x + 1, y, sunXi) : unspread((m + n) >> 2);
        m = n; fm = fn;
      }
      if (x == sb) o[x] = fm ? exactPix(x, y, sunXi) : unspread(m >> 1);
    }
  }
  if (starW > 0.01f) {                             // 夜の星(空の所だけ)
    for (int i = 0; i < NSTAR; ++i) {
      const Star& st = stars[i];
      if (!st.vis || st.sy < y0 || st.sy >= y0 + h) continue;
      if (st.sx < max(bx, (int)spanA[st.sy]) || st.sx > min(bx + bw - 1, (int)spanB[st.sy])) continue;
      const float ny = (st.sy - hh) * ih, nx = (st.sx - hh) * ih;
      const float rx = vCr * nx + vSr * ny, ry = -vSr * nx + vCr * ny;
      if (ry - (H0 + 0.12f * rx * rx) > -0.01f) continue;   // 地平線より下
      const float tw = 0.7f + 0.3f * fsin(simT * 2.0f + st.tw);
      uint16_t* o = out + (st.sy - y0) * bw - bx;
      o[st.sx] = addLight(o[st.sx], 230, 235, 255, (uint32_t)(st.b * tw * starW * 32.0f));
    }
  }
}
// 帯を1本取って描く(空いた方のコアが取る)。取れなかったら -1
static int takeBand(int core, int limit) {
  int k = __atomic_load_n(&nextBand, __ATOMIC_ACQUIRE);
  while (k < NB && k <= limit) {
    if (__atomic_compare_exchange_n(&nextBand, &k, k + 1, false, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) {
      if (bandW[k] > 0) renderBand(k, band[k % NSLOT], bandX[k], bandW[k], core);
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
  xSemaphoreGive(semGo);
  uint32_t busy = 0, send = 0;
  if (!fbMode) M5.Display.startWrite();
  for (int k = 0; k < NB; ++k) {                   // 帯を順に画面へ送る。待つあいだは、自分でも帯を描く
    while (!__atomic_load_n(&bandReady[k], __ATOMIC_ACQUIRE)) {
      const uint32_t t0 = micros();
      if (takeBand(1, __atomic_load_n(&doneUpTo, __ATOMIC_ACQUIRE) + NSLOT) >= 0) busy += micros() - t0;   // 置き場が空いている帯だけ(送っている最中の帯の置き場は使わない)
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
static void workerTask(void*) {                   // コア0: 空いている帯を描く(と、雲の模様を作る手伝い)
  for (;;) {
    xSemaphoreTake(semGo, portMAX_DELAY);
    if (job == 1) { cloudRows(1, 2, false); xSemaphoreGive(semDone); continue; }
    if (job == 8) { shadeRows(1, 2); xSemaphoreGive(semDone); continue; }
    uint32_t busy = 0;
    for (;;) {
      const int k = __atomic_load_n(&nextBand, __ATOMIC_ACQUIRE);
      if (k >= NB) break;
      if (k - NSLOT > __atomic_load_n(&doneUpTo, __ATOMIC_ACQUIRE)) continue;   // 置き場が空くのを待つ
      const uint32_t t0 = micros();
      if (takeBand(0, __atomic_load_n(&doneUpTo, __ATOMIC_ACQUIRE) + NSLOT) >= 0) busy += micros() - t0;
    }
    busyUs[0] = busy;
    xSemaphoreGive(semDone);
  }
}

// ---------------------------------------------------------------- 「雲を作っています」の表示
// 画面外の絵に描いてから、まとめて送る(画面へ直接送るときは、横の位置と幅を2の倍数にそろえないと崩れる)
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
  cv->setTextColor(M5.Display.color565(170, 180, 205));
  cv->drawString(text, w / 2, 20);
  cv->fillRect(40, 44, w - 80, 4, M5.Display.color565(40, 42, 54));
  cv->fillRect(40, 44, (int)((w - 80) * clamp01(prog)), 4, M5.Display.color565(200, 220, 255));
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

// ---------------------------------------------------------------- setup / loop
void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);
  Serial.begin(115200);
  Serial.println("NullCloud v6");
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

  spanA = (int16_t*)malloc(S * 2); spanB = (int16_t*)malloc(S * 2);
  bandX = (int16_t*)malloc(NB * 2); bandW = (int16_t*)malloc(NB * 2);
  for (int i = 0; i < NSLOT; ++i) band[i] = (uint16_t*)heap_caps_malloc((size_t)S * BAND * 2, MALLOC_CAP_DMA);
  for (int L = 0; L < NMIP; ++L) lev[L] = (uint16_t*)heap_caps_aligned_alloc(32, ((size_t)T >> L) / 4 * tStride(L) * 2, MALLOC_CAP_SPIRAM);
  cD = (uint8_t*)ps_malloc((size_t)T * T); cS = (uint8_t*)ps_malloc((size_t)T * T); cH = (uint8_t*)ps_malloc((size_t)T * T);
  latt = (uint8_t*)ps_malloc(512 * 512);
  luts = (uint16_t*)ps_malloc((size_t)NTOD * NKEY * 2);
  tabs = (TodTab*)ps_malloc(sizeof(TodTab) * NTOD);
  if (!spanA || !spanB || !bandX || !bandW || !band[0] || !band[1] || !band[2] || !band[3] || !lev[0] || !lev[1] || !lev[2] || !lev[3] || !lev[4] || !cD || !cS || !cH || !latt || !luts || !tabs) fail("Memory error");
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
  mipRowK = 1.25f * TPU / (S * 0.5f);
  CW = S / 2 + 2;
  for (int c = 0; c < 2; ++c) {                     // 粗い行の置き場(内蔵メモリ)
    crs[c].c = (uint32_t*)heap_caps_malloc((size_t)CROWS * CW * 4, MALLOC_CAP_INTERNAL);
    crs[c].e = (uint8_t*)heap_caps_malloc((size_t)CROWS * CW, MALLOC_CAP_INTERNAL);
    if (!crs[c].c || !crs[c].e) fail("Memory error (coarse)");
  }
  for (int i = 0; i < 1024; ++i) fogLut[i] = (int32_t)(clamp01(1.0f - expf(-fmaxf(sqrtf(i + 0.5f) - 1.2f, 0.0f) / 6.5f)) * 32.0f * 65536.0f);
  imuOK = TILT && M5.Imu.isEnabled();
  semGo = xSemaphoreCreateBinary();
  semDone = xSemaphoreCreateBinary();
  if (!semGo || !semDone) fail("Memory error (task)");
  xTaskCreatePinnedToCore(workerTask, "cloud", 6144, nullptr, 1, nullptr, 0);   // コア0 で帯を描く(雲の模様を作るのも手伝う)
  const uint32_t t0 = millis();
  showMsg("雲を作っています…", 0.0f);
  buildClouds();
  buildFixedTabs();
  for (int t = 0; t < NTOD; ++t) { buildTod(t); vTaskDelay(1); }
  buildKeys();
  free(cD); free(cS); free(cH); cD = cS = cH = nullptr;   // 番号の絵ができたら、もう要らない
  tod = todFrom = TOD_START; transT = 1.0f;
  applyBlend(1.0f);
  Serial.printf("time of day: %s\n", TOD_NAME[tod]);
  showMsg(nullptr, 1.0f);
  Serial.printf("clouds: %u ms\n", (unsigned)(millis() - t0));
  initGlories();
  initStars();
  rnd = (uint32_t)esp_random() | 1u;
  simulate(1.0f);
}

void loop() {
  static uint32_t last = millis(), fpsT = millis(), lastTapMs = 0;
  static int frames = 0;
  static bool was = false;
  static uint32_t tPrev = micros(), sumIn = 0, sumTilt = 0, sumSim = 0, sumDraw = 0, sumAll = 0;   // 1コマの時間の内わけ(Serial に出す)
  const uint32_t t0 = micros();
  sumAll += t0 - tPrev; tPrev = t0;
  M5.update();
  auto t = M5.Touch.getDetail();
  sumIn += micros() - t0;
  uint32_t now = millis();
  const bool corner = !roundScr && t.x < 56 && t.y < 56;   // 左上のすみはメニューに戻る所なので使わない
  bool changed = false;
  if (roundScr) {
    if (M5.BtnA.wasClicked()) { startTod((tod + 1) % NTOD); }
    if (M5.BtnB.wasClicked()) { speedIdx = (speedIdx + 1) % 3; Serial.printf("speed: x%d\n", (int)SPEEDS[speedIdx]); }
  } else {
    if (M5.BtnPWR.wasClicked()) { startTod((tod + 1) % NTOD); }
    if (t.wasPressed() && !corner) {
      if (now - lastTapMs < 350) { speedIdx = (speedIdx + 1) % 3; Serial.printf("speed: x%d\n", (int)SPEEDS[speedIdx]); lastTapMs = 0; changed = true; } else lastTapMs = now;
    }
  }
  if (changed) { now = millis(); last = now; }
  float dt = (now - last) / 1000.0f; last = now;
  if (dt > 0.1f) dt = 0.1f;

  // 傾き: 本体を傾けたとき、画面の上で「下」になる向き
  const uint32_t tTilt0 = micros();
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
      if (levelN < 20) { ++levelN; levelX += (sx - levelX) / levelN; levelY += (sy - levelY) / levelN; }   // はじめの姿勢を「水平」に
      else if (TILT_RELEVEL > 0 && fabsf(sx - levelX) < TILT_RELEVEL_MAX && fabsf(sy - levelY) < TILT_RELEVEL_MAX) {   // 同じ角度で持ち続けたら、そこを少しずつ「水平」にする
        const float b = 1.0f - expf(-dt / TILT_RELEVEL); levelX += (sx - levelX) * b; levelY += (sy - levelY) * b;
      }
      sx -= levelX; sy -= levelY;
      sx = sx < -1 ? -1 : sx > 1 ? 1 : sx; sy = sy < -1 ? -1 : sy > 1 ? 1 : sy;
      const float a = 1.0f - expf(-dt * 4.0f);
      tiltX += (sx - tiltX) * a; tiltY += (sy - tiltY) * a;
    }
  }
  sumTilt += micros() - tTilt0;

  // 触る: タップで光輪が生まれる。なぞると、横で向きが回り、縦で見下ろす角度が変わる
  static int pressX = 0, pressY = 0, lastX = 0, lastY = 0;
  static bool moved = false, skipTap = false;
  const int tx = t.x - XOFF, ty = t.y - YOFF;
  const bool touching = t.isPressed() && !corner && tx >= 0 && ty >= 0 && tx < S && ty < S;
  if (touching && !was) { pressX = lastX = tx; pressY = lastY = ty; moved = false; skipTap = changed; }
  if (touching && was) {
    if (!moved) {
      const float mx = (float)(tx - pressX), my = (float)(ty - pressY);
      if (mx * mx + my * my > (S * 0.03f) * (S * 0.03f)) { moved = true; grabbing = true; lastX = pressX; lastY = pressY; }
    }
    if (moved) {
      yaw += (tx - lastX) * (1.4f / S);
      pitchOff += (ty - lastY) * (1.6f / S);
      pitchOff = pitchOff < -0.6f ? -0.6f : (pitchOff > 0.6f ? 0.6f : pitchOff);
      lastX = tx; lastY = ty;
    }
  }
  if (!touching && was) {
    if (!moved && !skipTap) addGlory((float)pressX, (float)pressY);   // タップ: 光輪が生まれる
    grabbing = false;
  }
  was = touching;

  const uint32_t tSim0 = micros();
  stepTod(dt);                                     // 時間帯の移りかわり(雲の色を少しずつ塗りかえる)
  simulate(dt);
  const uint32_t tDraw0 = micros();
  sumSim += tDraw0 - tSim0;
  renderFrame();
  sumDraw += micros() - tDraw0;
  if (++frames, now - fpsT >= 3000) {
    Serial.printf("fps: %.1f  (%s, x%d)  core0 %.1f ms  core1 %.1f ms  send %.1f ms\n", frames * 1000.0f / (now - fpsT), TOD_NAME[tod], (int)SPEEDS[speedIdx],
                  busyUs[0] / 1000.0f, busyUs[1] / 1000.0f, sendUs / 1000.0f);
    {                                              // 1コマの時間の内わけ(ボタン・タッチ / 傾き / 動き / 描く / それ以外)
      const float k = 1.0f / (1000.0f * frames);
      const float a = sumAll * k, in = sumIn * k, ti = sumTilt * k, si = sumSim * k, dr = sumDraw * k;
      Serial.printf("time: input %.1f  tilt %.1f  sim %.1f  draw %.1f  other %.1f  (frame %.1f ms)\n", in, ti, si, dr, fmaxf(0.0f, a - in - ti - si - dr), a);
      sumIn = sumTilt = sumSim = sumDraw = sumAll = 0;
    }
    if (imuOK) Serial.printf("tilt: x %+.2f  y %+.2f\n", tiltX, tiltY);
    frames = 0; fpsT = now;
  }
}

// 共通の置き場に置いたものは、0 で始まる単純な型だけ(コンストラクタが要らないもの)
static_assert(std::is_trivially_default_constructible<float>::value, "NM_ARENA: float");
static_assert(std::is_trivially_default_constructible<uint16_t>::value, "NM_ARENA: uint16_t");
static_assert(std::is_trivially_default_constructible<uint8_t>::value, "NM_ARENA: uint8_t");
static_assert(std::is_trivially_default_constructible<int32_t>::value, "NM_ARENA: int32_t");
static_assert(std::is_trivially_default_constructible<Glory>::value, "NM_ARENA: Glory");
static_assert(std::is_trivially_default_constructible<Star>::value, "NM_ARENA: Star");
static constexpr size_t OVL_END = nmAlignTo(nmOv16, 16);
static_assert(OVL_END <= NM_ARENA_SIZE, "NM_ARENA_SIZE (nm_common.h) を大きくしてください");
extern const size_t OVL_SIZE = OVL_END;   // 起動時の確認用
}  // namespace NullCloud
