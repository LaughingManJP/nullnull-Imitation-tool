// NullBodhisattva v10  -  オブジェクト指向菩薩
//   M5Stack StopWatch 用(丸い 466x466 画面)。CoreS3 系でも画面いっぱいに動きます。
//
// 画面の中:
//   ・座った菩薩の姿が、大小の「クラスの箱」でできている。大きな箱は小さな箱に分かれ(入れ子)、
//     親の箱と子の箱は、継承の細い線でつながっている。胸のまわりは金色、外へいくほど青から紫
//   ・胸から外へ、呼吸の波がゆっくり広がる。波が来ると、小さな箱がまとまって大きな箱になり、
//     波が引くと、箱がさらに細かく分かれる(箱が呼吸している)
//   ・継承の線の上を、光の点(メッセージ)が胸から手足の先の箱へと流れていく
//   ・頭のうしろの光輪と、体を囲む舟形の光背は、小さな箱(インスタンス)の輪。光が輪をめぐる
//   ・外周を、菩薩のクラス定義の文字がゆっくり回る
//   ・青い蝶(ふつう5匹)が、菩薩のまわりをゆらゆら舞う
//
// 触ると:
//   ・仏像に触ると、バーンと光の輪と火花がはじけ、触った所の箱がまぶしく光りながら画面のふちまで
//     吹き飛ぶ。なぞると、なぞった道すじが次々にはじけていく。
//     指を止めて押さえていると、散る範囲がだんだん広がる。触れば触るほど、菩薩は散っていく
//   ・散った箱は、うずを巻きながら漂い、触られてから約15秒で元の場所へ戻る
//     (散った跡には、継承の線だけの「空の姿」が見える。胸の核と眉間の白毫は散らない)
//   ・蝶はタップで驚いて散り、指を止めて押さえていると、指のまわりに集まってきて回る
//
// 操作:
//   StopWatch … ボタンA 短押し: 蝶の数(5 / 10 / 20)  ボタンA 長押し: 色(金蒼 / 翡翠 / 紅蓮 / 白銀)
//               ボタンB 短押し: 明るさ   ボタンB 長押し: 呼吸の速さ
//   CoreS3系  … 画面の上半分をダブルタップ: 蝶の数   下半分をダブルタップ: 色
//               電源ボタン短押し: 明るさ
//   (色を変えると、動かない部分の絵を描き直すので、1〜2秒ほど止まります)
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

#include <M5Unified.h>
#include <math.h>

#pragma GCC optimize("O3")
#ifndef IRAM_ATTR
#define IRAM_ATTR
#endif

// Arduino IDE は関数の宣言を自動でこのあたりに足すので、型の名前だけ先に宣言しておく
struct Col; struct Theme; struct Node; struct DBox; struct PDraw; struct Pulse; struct Part; struct Ring; struct FBuf; struct Mask; struct Fly; struct Input;

// ---- 調整用 ----------------------------------------------------------------
static const float BREATH_SPEEDS[3] = {0.35f, 0.60f, 1.00f};   // 呼吸の速さ(ボタンA 長押しで切り替え)
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

static int   W, H, S, XOFF, YOFF, NB;
static float Sc, H2;                           // 中心 / 半径(ピクセル)
static bool  isRound;
static uint8_t* bandVis;
static uint16_t* slotBuf[NSLOT];
static uint16_t* statImg;                         // [S*S] 動かない部分の絵(菩薩のにじみ込み。バイト順入れ替え済み)
static uint16_t* stat0;                        // [S*S] 同じ絵から菩薩を抜いたもの(拡散したときに見える、継承の線だけの空の姿)
static int       bbX0, bbX1, bbY0, bbY1;       // 菩薩のにじみがある範囲
static constexpr int PGC = 8;                  // 「箱がそこに居るか」の地図の、ます目の大きさ(ピクセル)
static float*    pg;                           // [PGN*PGN] 箱の居る割合 1 = ぜんぶ居る / 0 = ぜんぶ散った
static int       PGN;
static bool      anyAway = false;              // 散っている箱が1つでもあるか
static int16_t*  spanS;
static int16_t*  spanE;
static int16_t*  bandX0;
static int16_t*  bandW;
static uint32_t  busyUs[2];

static SemaphoreHandle_t semGo, semDone;
static volatile int readyW = -1, doneUpTo = -1;   // コア0が描き終えた帯 / 画面へ送り終えた帯
static int  fbState = -1;                        // まだ調べていない / 0: 直接送る / 1: StopWatch の置き場あり
static bool fbMode = false;                      // StopWatch: 帯ごとに丸の中の幅だけ送る
static int  SPLIT = 2;                           // コア1は SPLIT 本に1本の帯を描く(残りはコア0)
static uint32_t sendUs = 0;

static float simT = 0;
static float holdT = 0, fPX = 0, fPY = 0;        // 指が止まっている時間 / 前の指の位置
static bool  fWas = false;
static int   theme = 0, brightIdx = 2, breathIdx = BREATH_START;
static const uint8_t brightSteps[] = {60, 120, 190, 255};

// ---------------------------------------------------------------- 道具
struct Col { float r, g, b; };
static inline float clamp01(float v) { return v < 0 ? 0 : (v > 1 ? 1 : v); }
static inline float smooth3(float t) { t = clamp01(t); return t * t * (3 - 2 * t); }
static inline Col lerpC(Col a, Col b, float t) { return {a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t}; }
static inline Col mulC(Col a, float k) { return {a.r * k, a.g * k, a.b * k}; }
static inline Col C8(int r, int g, int b) { return {r / 255.0f, g / 255.0f, b / 255.0f}; }
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
  const float t = fminf(1.0f, hypotf(u, (v + 0.12f) * 0.85f) / 0.55f);
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
  return hypotf(pu - du * h, pv - dv * h) - r;
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
static int   nNode = 0;
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
static int      nBox = 0;
static int16_t* ent;                  // 帯ごとに並べた箱の番号
static int16_t  entStart[MAXNB + 1];

struct PDraw { int16_t x, y; uint16_t c, ch; };
static PDraw    pd[MAXPD];
static int      nPD = 0;
static int16_t  pdOrd[MAXPD];
static int16_t  pdStart[MAXNB + 1];

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
static float breathT = 0;
static bool  restMode = false;          // 止まった状態(動かない絵を作るとき)
static inline float breathAt(float u, float v) {
  const float d = hypotf(u, v + 0.12f);
  return 0.62f * sinf(breathT - 7.0f * d) + 0.38f * sinf(0.53f * breathT + 3.1f * u - 2.3f * v + 1.0f);
}

// 1つの箱(ほどけ具合に応じて、飛ばして・小さくして・明るくする)
static void emitClass(int n, float u0, float v0, float u1, float v1, float k, uint8_t seed) {
  const float dis = nodeDis[n];
  const float cu = (u0 + u1) * 0.5f, cv = (v0 + v1) * 0.5f;
  const float pulse = restMode ? 1.0f : 0.86f + 0.20f * sinf(2.0f * simT - 9.0f * hypotf(cu, cv + 0.12f));
  // はじけた瞬間は、まぶしく光る(0.6秒ほどで引く)
  const float since = simT - nodePush[n];
  const float flash = (since >= 0 && since < 1.5f) ? 1.6f * expf(-since * 4.0f) : 0.0f;
  Col c = boxCol(cu, cv, k * pulse * (1.0f + 0.5f * dis + flash));
  if (dis < 0.004f) { addBox(u0, v0, u1, v1, c, ST_CLASS, seed); return; }
  const float jit = ((seed * 37) & 255) / 255.0f - 0.5f, h2 = ((seed * 91 + n * 7) & 255) / 255.0f;
  // うずを巻きながら外へ飛び、漂っているあいだも、ゆっくり回り続ける
  const float a = nodeDir[n] + jit * 1.0f + dis * 0.9f + dis * 0.4f * sinf(simT * 0.11f + seed * 0.1f);
  const float r = SCATTER * dis * (0.40f + 0.60f * h2);
  float pu = cu + cosf(a) * r + dis * 0.03f * sinf(simT * 0.21f + seed * 0.7f);
  float pv = cv + sinf(a) * r + dis * 0.03f * cosf(simT * 0.17f + seed * 1.3f);
  const float rr = hypotf(pu, pv);
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
static void emitHalo() {
  for (int ring = 0; ring < 3; ++ring)
    for (int i = 0; i < HALO_N[ring]; ++i) {
      const float a = 6.2832f * i / HALO_N[ring] + ring * 0.13f;
      const float u = HCU + HALO_R[ring] * cosf(a), v = HCV + HALO_R[ring] * sinf(a), s = HALO_SZ[ring];
      if (figureSd(u, v) < 0.01f) continue;                     // 体の後ろに隠れる箱は描かない
      const float dir = (ring & 1) ? -1.0f : 1.0f;
      const float tw = 0.70f + 0.55f * powf(0.5f + 0.5f * sinf(a * 3.0f - dir * simT * 0.9f), 3.0f);
      addBox(u - s, v - s, u + s, v + s, boxCol(0, -0.12f, HALO_K[ring] * tw), ST_HALO, 0);
    }
  for (int j = 0; j < 2; ++j) {
    const float rx = j ? 0.64f : 0.56f, ry = j ? 0.68f : 0.60f, s = j ? 0.008f : 0.011f;
    const int n = j ? 96 : 72;
    for (int i = 0; i < n; ++i) {
      const float a = 6.2832f * i / n;
      const float u = rx * sinf(a), v = -0.10f - ry * cosf(a);
      if (v > 0.30f) continue;
      const float tw = 0.65f + 0.6f * powf(0.5f + 0.5f * sinf(a * 2.0f - simT * (j ? 0.5f : -0.7f)), 4.0f);
      addBox(u - s, v - s, u + s, v + s, boxCol(u, v, (j ? 0.55f : 0.8f) * tw), ST_SOLID, 0);
    }
  }
}

// ---------------------------------------------------------------- メッセージ(継承の線を流れる光)
struct Pulse { int16_t path[12]; int8_t len; float pos, speed, wait; };
static Pulse pulse[NPULSE];
static int16_t* leafList;
static int nLeaf = 0;
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
static Part part[MAXPART];
struct Ring { float x, y, t0, k; };                  // はじけた光の輪
static Ring ring[NRING] = {{0, 0, -100, 0}, {0, 0, -100, 0}, {0, 0, -100, 0}, {0, 0, -100, 0}};
static int  ringNext = 0;
static int  partNext = 0;
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
static int       stripW;
static int32_t*  annStart;            // [S+1] 行ごとの、表の始まり
static int16_t*  annX;
static uint16_t* annA;                // 角度(0..65535、上から時計回り)
static uint8_t*  annR;                // 帯の行(0 = 外側)
static uint16_t  textLut[16];
static uint16_t  textRot = 0;
static float     textRotF = 0;

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
struct FBuf { float* p; int w; float cx, sc; };
static inline void fAdd(FBuf& f, int x, int y, Col c, float a) {
  if ((unsigned)x >= (unsigned)f.w || (unsigned)y >= (unsigned)f.w || a <= 0) return;
  float* q = f.p + ((size_t)y * f.w + x) * 3;
  q[0] += c.r * a; q[1] += c.g * a; q[2] += c.b * a;
}
static inline void fSet(FBuf& f, int x, int y, Col c) {
  if ((unsigned)x >= (unsigned)f.w || (unsigned)y >= (unsigned)f.w) return;
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
static void fBlur(FBuf& f, int r) {
  if (r < 1) return;
  const int w = f.w;
  float* line = (float*)malloc((size_t)w * 3 * sizeof(float));
  if (!line) return;
  for (int pass = 0; pass < 3; ++pass)
    for (int dir = 0; dir < 2; ++dir)
      for (int j = 0; j < w; ++j) {
        const size_t step = dir == 0 ? 3 : (size_t)w * 3;
        float* base = f.p + (dir == 0 ? (size_t)j * w * 3 : (size_t)j * 3);
        for (int ch = 0; ch < 3; ++ch) {
          float acc = 0;
          for (int i = -r; i <= r; ++i) acc += base[(size_t)min(max(i, 0), w - 1) * step + ch];
          for (int i = 0; i < w; ++i) {
            line[i * 3 + ch] = acc / (2 * r + 1);
            acc += base[(size_t)min(i + r + 1, w - 1) * step + ch] - base[(size_t)max(i - r, 0) * step + ch];
          }
        }
        for (int i = 0; i < w; ++i)
          for (int ch = 0; ch < 3; ++ch) base[(size_t)i * step + ch] = line[i * 3 + ch];
      }
  free(line);
}
static inline Col fSample(const FBuf& f, float x, float y) {     // 半分の大きさの絵を、なめらかに引き伸ばして読む
  x = x * 0.5f - 0.25f; y = y * 0.5f - 0.25f;
  int ix = (int)floorf(x), iy = (int)floorf(y);
  const float fx = x - ix, fy = y - iy;
  Col o = {0, 0, 0};
  for (int j = 0; j < 2; ++j)
    for (int i = 0; i < 2; ++i) {
      const int xx = min(max(ix + i, 0), f.w - 1), yy = min(max(iy + j, 0), f.w - 1);
      const float wgt = (i ? fx : 1 - fx) * (j ? fy : 1 - fy);
      const float* q = f.p + ((size_t)yy * f.w + xx) * 3;
      o.r += q[0] * wgt; o.g += q[1] * wgt; o.b += q[2] * wgt;
    }
  return o;
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

static bool buildStatic() {
  const Theme& th = THEMES[theme];
  const int Sh = S / 2;
  FBuf cv = {(float*)ps_calloc((size_t)S * S * 3, sizeof(float)), S, (S - 1) * 0.5f, S * 0.5f};
  FBuf gl = {(float*)ps_calloc((size_t)Sh * Sh * 3, sizeof(float)), Sh, (Sh - 1) * 0.5f, Sh * 0.5f};
  FBuf ga = {(float*)ps_calloc((size_t)Sh * Sh * 3, sizeof(float)), Sh, (Sh - 1) * 0.5f, Sh * 0.5f};
  FBuf gb = {(float*)ps_calloc((size_t)Sh * Sh * 3, sizeof(float)), Sh, (Sh - 1) * 0.5f, Sh * 0.5f};
  if (!cv.p || !gl.p || !ga.p || !gb.p) { free(cv.p); free(gl.p); free(ga.p); free(gb.p); return false; }
  const float k = S / 466.0f;

  // 背景: 中ほどが少し明るい暗い空 + うすい格子 + 散らばった点(インスタンス)
  for (int y = 0; y < S; ++y)
    for (int x = 0; x < S; ++x) {
      const float r = hypotf(x - Sc, y - Sc) / H2, f = clamp01(1.0f - r);
      fSet(cv, x, y, {th.bg0.r + th.bg1.r * f * f, th.bg0.g + th.bg1.g * f * f, th.bg0.b + th.bg1.b * f * f});
    }
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

  // 光輪: 放射の線 / 3つの円 / 箱(箱はにじみだけ。くっきりした箱は毎フレーム描く)
  for (int i = 0; i < HALO_N[0]; ++i) {
    const float a = 6.2832f * i / HALO_N[0];
    bothSeg(cv, gl, HCU, HCV, HCU + HALO_R[0] * cosf(a), HCV + HALO_R[0] * sinf(a), 1.0f, boxCol(0, -0.12f, 0.9f * 0.18f));
  }
  for (int ring = 0; ring < 3; ++ring) {
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
  // 舟形光背: 点線の楕円 + 箱(にじみだけ)
  for (int j = 0; j < 2; ++j) {
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
  // 蓮の台座: 奥の列 → 手前の列
  static const int PET[9] = {-4, -2, 0, 2, 4, -3, -1, 1, 3};
  for (int q = 0; q < 9; ++q) {
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
  bothSeg(cv, gl, -0.50f, 0.40f, 0.50f, 0.40f, 1.0f, th.base);
  // 継承の線(親の箱のまん中 → 子の箱のまん中)
  for (int n = 1; n < nNode; ++n) {
    const Node& N = nodes[n];
    if (N.parent < 0) continue;
    const Node& P = nodes[N.parent];
    bothSeg(cv, gl, P.cu, P.cv, N.cu, N.cv, 1.0f, boxCol(N.cu, N.cv, N.depth > 3 ? 0.30f : 0.45f));
  }

  // 菩薩の箱(止まった状態)。箱のある所は、あとで「箱のにじみ」だけにする
  nBox = 0;
  for (int i = 0; i < nNode; ++i) nodeDis[i] = 0;
  restMode = true;                                              // 波の来ていない状態
  emitNode(0, 1.0f);
  restMode = false;
  uint8_t* mask = (uint8_t*)ps_calloc((size_t)S * S, 1);
  if (!mask) { free(cv.p); free(gl.p); free(ga.p); free(gb.p); return false; }
  const int mg = (int)(24 * k);
  bbX0 = S; bbX1 = -1; bbY0 = S; bbY1 = -1;
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
  memcpy(gb.p, ga.p, (size_t)Sh * Sh * 3 * sizeof(float));
  fBlur(gl, max(1, (int)lrintf(2.5f * k)));
  fBlur(ga, max(1, (int)lrintf(4.0f * k)));
  fBlur(gb, 1);

  // 合わせて、画面の色に
  for (int y = 0; y < S; ++y)
    for (int x = 0; x < S; ++x) {
      const float* q = cv.p + ((size_t)y * S + x) * 3;
      Col o = {0, 0, 0};
      const Col g = fSample(gl, x, y);
      const Col o0 = {q[0] + g.r * 0.9f, q[1] + g.g * 0.9f, q[2] + g.b * 0.9f};
      stat0[(size_t)y * S + x] = toPix(o0);                          // 菩薩のいない絵
      if (!mask[(size_t)y * S + x]) o = o0;                          // 箱のある所は「箱のにじみ」だけ
      const Col a = fSample(ga, x, y), b = fSample(gb, x, y);
      o.r += a.r * 0.8f + b.r * 0.35f; o.g += a.g * 0.8f + b.g * 0.35f; o.b += a.b * 0.8f + b.b * 0.35f;
      statImg[(size_t)y * S + x] = toPix(o);
    }
  bbX0 = max(0, bbX0); bbY0 = max(0, bbY0); bbX1 = min(S - 1, bbX1); bbY1 = min(S - 1, bbY1);
  free(mask); free(cv.p); free(gl.p); free(ga.p); free(gb.p);
  const Col tc = th.text;
  for (int i = 0; i < 16; ++i) textLut[i] = to565(mulC(tc, 0.82f * i / 15.0f));
  return true;
}

// ---------------------------------------------------------------- 蝶(NullSutra v2 と同じ、ゆらゆらした飛び方)
// 蝶の絵は、起動時に「羽ばたきの8コマ × 大きさ3段」を描いておき(色は番号)、毎フレーム回して貼ります
static constexpr int NFR = 8, NMIP = 3, NPIDX = 132;
static const int     MIPT[NMIP] = {96, 48, 24};
static uint8_t*  atlas[NMIP];
static float     mipR0[NMIP];
static uint16_t* flyPal;                                        // [色違い2][コマ8][NPIDX]

struct Fly {
  float x, y, vx, vy, ph, hz, head, R, wand;
  uint8_t var;
  float cx, cy, rr, dxx, dxy, dyx, dyy;                         // 描くための値
  int   y0, y1, T;
  const uint8_t* tex; const uint16_t* pal;
};
static Fly fly[MAXFLY];
static int nFly = 0, flyIdx = FLY_START;

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
struct Input { bool touching; bool tapped; float tx, ty; float dt; };


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

static void updateFrame(const Input& in) {
  float dt = in.dt > 0.05f ? 0.05f : in.dt;
  if (dt < 0.0005f) dt = 0.0005f;
  simT += dt;
  breathT += dt * BREATH_SPEEDS[breathIdx];
  textRotF += dt * 65536.0f / TEXT_TURN;
  while (textRotF >= 65536.0f) textRotF -= 65536.0f;
  textRot = (uint16_t)textRotF;
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
    static float sum[80 * 80], wsum[80 * 80];
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
  for (int k = 0; k <= NB; ++k) pdStart[k] = 0;
  for (int i = 0; i < nPD; ++i) pdStart[pd[i].y / BAND + 1]++;
  for (int k = 0; k < NB; ++k) pdStart[k + 1] += pdStart[k];
  int16_t fill[MAXNB];
  for (int k = 0; k < NB; ++k) fill[k] = pdStart[k];
  for (int i = 0; i < nPD; ++i) pdOrd[fill[pd[i].y / BAND]++] = (int16_t)i;
}

// ---------------------------------------------------------------- 描画(帯ごと)
static void renderBand(int k, uint16_t* buf) {
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

static void __attribute__((noinline)) IRAM_ATTR renderBandIram(int k, uint16_t* b) { renderBand(k, b); }

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
  while (true) delay(1000);
}

static void* allocFast(size_t n) {
  void* p = heap_caps_malloc(n, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  return p ? p : ps_malloc(n);
}

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);
  Serial.begin(115200);

  if (M5.Display.width() < M5.Display.height()) M5.Display.setRotation(1);
  W = M5.Display.width(); H = M5.Display.height();
  isRound = (W == H);
  S = (isRound ? W : max(W, H)) & ~1;
  Sc = (S - 1) * 0.5f; H2 = S * 0.5f;
  XOFF = (W - S) / 2;
  YOFF = ((H - S) / 2 / BAND) * BAND;
  if (!isRound) YOFF += 2 * BAND;                     // 四角い画面: 菩薩(光輪から蓮まで)が上下のまん中に来るよう、少し下げる
  NB = (S + BAND - 1) / BAND;
  Serial.println("NullBodhisattva v10");
  Serial.printf("screen %dx%d  area %d\n", W, H, S);

  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setBrightness(brightSteps[brightIdx]);

  for (int i = 0; i < NSLOT; ++i) slotBuf[i] = (uint16_t*)heap_caps_malloc(S * BAND * 2, MALLOC_CAP_DMA);
  statImg = (uint16_t*)ps_malloc((size_t)S * S * 2);
  stat0 = (uint16_t*)ps_malloc((size_t)S * S * 2);
  PGN = S / PGC + 2;
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
  bool ok = atlas[0] && atlas[1] && atlas[2] && flyPal && statImg && stat0 && pg && PGN <= 80 && nodes && nodeDis && nodeDir && nodePush && nodePend && nodeDis0 && nodeRetB && leafList && box && ent &&
            spanS && spanE && bandX0 && bandW && bandVis && NB <= MAXNB;
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
  if (nNode == 0 || nLeaf == 0) fail("Shape error");

  buildRimText();
  if (!strip || !annX || !annA || !annR || !annStart) fail("Memory error");
  uint32_t t0 = millis();
  if (!buildStatic()) fail("Memory error");
  Serial.printf("background built in %u ms\n", (unsigned)(millis() - t0));
  for (int i = 0; i < MAXPART; ++i) part[i].life = 0;
  for (int i = 0; i < NPULSE; ++i) { newPulse(pulse[i]); pulse[i].pos = frand(0, (float)(pulse[i].len - 1)); pulse[i].wait = 0; }

  for (int m = 0; m < NMIP; ++m) {                               // 蝶の絵
    mipR0[m] = MIPT[m] * 0.5f / 1.15f;
    for (int fr = 0; fr < NFR; ++fr) drawFlyAtlas(atlas[m] + (size_t)fr * MIPT[m] * MIPT[m], MIPT[m], 0.14f + 0.86f * fr / (NFR - 1));
  }
  buildFlyPal();
  setFlyCount(FLY_START);

  Input in0 = {false, false, -1000, -1000, 0.016f};
  updateFrame(in0);

  semGo   = xSemaphoreCreateBinary();
  semDone = xSemaphoreCreateBinary();
  xTaskCreatePinnedToCore(workerTask, "band", 8192, nullptr, 1, nullptr, 0);
}

static void nextTheme() {
  theme = (theme + 1) % NTHEME;
  const uint32_t t0 = millis();
  if (!buildStatic()) { theme = (theme + NTHEME - 1) % NTHEME; Serial.println("theme: not enough memory"); return; }
  buildFlyPal();
  Serial.printf("theme: %s  (%u ms)\n", THEMES[theme].name, (unsigned)(millis() - t0));
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
    else if (M5.BtnA.wasClicked()) setFlyCount((flyIdx + 1) % 3);
    if (M5.BtnB.wasHold()) { breathIdx = (breathIdx + 1) % 3; Serial.printf("breath: %d\n", breathIdx); }
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
      if (now - lastTapMs < 350) {                               // ダブルタップ: 上半分なら蝶の数、下半分なら色
        if (t.y < H / 2) setFlyCount((flyIdx + 1) % 3); else { nextTheme(); now = millis(); }
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
