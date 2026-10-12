// NullGoodbye v13  -  音に合わせて、サイケデリックな液体の模様が渦を巻き、黒い穴(ブラックホール)に吸い込まれていく
//   ある曲を聞いたイメージから(曲も絵も、本体には入っていません。模様はその場で計算)
//   M5Stack StopWatch 用(丸い 466x466 画面)。CoreS3 系でも画面いっぱいに動きます。
//
// 画面の中:
//   ・赤・橙・黄・緑・水色・青の帯が、黒と白の細い線をはさんで、液体のように流れる
//   ・黒い穴(ブラックホール)が4つ浮かび、まわりの模様を渦に巻きこんで吸い込んでいる
//   ・帯の1本1本が、金属の管のようにつやつや光る。模様の中のラメが、ちらちら瞬く
//   ・白い光の粒が、瞬きながら模様と一緒に流れ、穴のそばでは渦を巻いて吸い込まれる
//   ・音が大きいほど、模様が速く流れ、渦が強くなる。キックが鳴るたびに、渦がぐっとねじれて球が脈打ち、
//     色がパッと進む。高い音が多いほど、光の粒が増える
//
// 音の聞き方(自動で切り替わります):
//   ・マイクで音が聞こえているとき … スマホなどで曲を流して近くに置くと、実際の音に反応する
//   ・聞こえないとき … 本体の中の「振り付け」で動く(この曲の流れ、約147 BPM、4分32秒)
//
// 触ると:
//   ・タップ  : そこに新しい渦が生まれて、光の粒がはじける(渦はしばらくすると消える)
//   ・なぞる  : 指の動きで流れができて、模様がうねうね運ばれる(運ばれた模様は、そのまま流れに乗っていく)
//   ・穴をつかんでなぞる : 穴が指についてくる。ほかの穴にぶつけると、渦を巻いて1つに結合する
//   ・穴を長押し        : 穴がまわりを吸い込みながら、大きくなっていく(押している間ずっと。上限まで)
//   ・穴を2回タップ      : 穴がしぼむ。小さくなりすぎると、光をまき散らして消える
//   ・穴のない所を長押し  : 新しい穴が生まれて、そのまま押していると育つ(穴は最大 MAXBH 個)
//
// 操作:
//   StopWatch … ボタンA 短押し: 振り付けを最初から   ボタンA 長押し: 色(Psyche / Ocean / Ember / Neon)
//               ボタンB: 明るさ
//   CoreS3系  … 画面の上半分をダブルタップ: 振り付けを最初から   下半分をダブルタップ: 色
//               電源ボタン短押し: 明るさ
//
// 必要なもの:
//   ボードマネージャー M5Stack >= 3.3.7 / ボード: M5StopWatch(CoreS3 なら M5CoreS3)
//   ライブラリ M5Unified >= 0.2.15, M5GFX >= 0.2.21
//
// しくみ:
//   模様は起動時に 256x256 の「番号の絵」として作ります(ゆがめたノイズを、細かい帯に切ったもの)。
//   色は番号 → 色の表で決まり、毎フレーム表をずらすと、帯が流れて見えます。
//   渦は 8 ピクセルおきの格子の上だけで「どこの模様を読むか」を計算し(球のまわりを回して、少し吸い込む)、
//   間の点は足し算で補間します。黒い球は、模様の映り込みとハイライトを、その場で計算して描きます。
//   画面は8行ずつの帯に分けて、2つのコアで交互に描きます。

#include <M5Unified.h>
#include <math.h>

#pragma GCC optimize("O3")
#ifndef IRAM_ATTR
#define IRAM_ATTR
#endif

// Arduino IDE は関数の宣言を自動でこのあたりに足すので、型の名前だけ先に宣言しておく
struct Scratch; struct Col; struct Stop; struct Theme; struct Lattice; struct Fbm; struct Hole; struct Part; struct Star; struct SDraw; struct Input;

// ---- 調整用 ----------------------------------------------------------------
static constexpr float BPM          = 147.0f;  // 振り付けのテンポ(曲の解析から)
static constexpr float FIRST_BEAT   = 56.0f;   // 振り付けで、最初のドロップの1拍目(秒)
static constexpr float SONG_LEN     = 272.0f;  // 振り付けの長さ(秒)。終わると最初から
static constexpr int   NHOLE        = 4;       // 黒い球の数
static constexpr float SWIRL        = 2.4f;    // 渦の強さ(球のすぐそばで、何ラジアンねじれるか)
static constexpr float SUCK         = 0.30f;   // 球に吸い込まれる強さ
static constexpr float FLOW         = 1.0f;    // 色が流れる速さ
static constexpr float PATTERN      = 1.0f;    // 帯の細かさ(大きいほど細かい)
static constexpr float ZOOM         = 1.4f;    // 模様の細かさ(大きいほど、画面に模様がたくさん入る)
static constexpr float MIC_GAIN     = 1.0f;    // マイクの感度(反応が弱いときは大きく)
static constexpr float GLOSS        = 1.3f;    // 帯のつや(0 でつやなし)
static constexpr float GLITTER      = 1.4f;    // ラメの瞬きの強さ(0 でなし)
static constexpr float STIR         = 0.9f;    // なぞったとき、指の動きでできる流れの強さ
static constexpr float SPIN         = 42.0f;   // 穴のまわりを回る流れの速さ(穴の半径の2倍の所で、ピクセル/秒)
static constexpr float INFLOW       = 10.0f;   // 穴へ吸い込まれる流れの速さ(同じ所で、ピクセル/秒)
static constexpr float WANDER       = 18.0f;   // 全体のうねりの流れの速さ(ピクセル/秒)
static constexpr float LIFE         = 5.0f;    // 1枚の模様を流し続ける時間(秒。長いほど、きつく巻きこまれる)
static constexpr int   MAXBH        = 8;       // ブラックホールの最大の数
static constexpr float R_MAX        = 0.30f;   // 穴の最大の大きさ(画面の半径に対して)
static constexpr float R_MIN        = 0.035f;  // これより小さくしぼむと、穴は消える
static constexpr float GROW         = 0.07f;   // 長押しで大きくなる速さ(画面の半径に対して、1秒あたり)
static constexpr float SHRINK       = 0.62f;   // 2回タップで、何倍にしぼむか
static constexpr float HOLD_T       = 0.40f;   // 何秒押し続けたら「長押し」か
static constexpr float AREA_MAX     = 2.2f;    // 全部の穴の面積の合計の上限(いちばん大きい穴の何個ぶんか)。重くなりすぎないように
// ---------------------------------------------------------------------------

static constexpr int BAND    = 8;
static constexpr int NSLOT   = 4;
static constexpr int MAXNB   = 64;
static constexpr int CSTEP   = 8;              // 渦を計算する格子の間隔(ピクセル)
static constexpr int USHIFT  = 3;
static constexpr int TEXN      = 256;            // 模様の絵の大きさ
static constexpr int NBAR    = 72;             // (マイクの高さごとの大きさの数。v1 と共通)
static constexpr int MAXSTAR = 320;
static constexpr int MAXENT  = 2600;
static constexpr int MICN    = 512;            // マイク: 1回に取る数
static constexpr int MICRATE = 16000;
static constexpr int MAXH    = MAXBH + 3;      // ブラックホール + タップでできた渦

static int   W, H, S, XOFF, YOFF, NB, CW;
static float Sc, VR;                           // 中心 / 見えている半径(ピクセル)
static bool  isRound;
static uint8_t*  bandVis;
static uint16_t* slotBuf[NSLOT];
static int16_t*  spanS;
static int16_t*  spanE;
static int16_t*  bandX0;
static int16_t*  bandW;
static uint8_t*  tex;                          // [TEXN*TEXN] 模様の番号
static uint8_t*  tex2;                         // [TEXN*TEXN] 上4ビット: ラメ(0 = なし、1..15 = 瞬く順番)/ 下4ビット: つや
static uint16_t  shineLUT[256];                // tex2 → 足す光(バイト順はそのまま)
static uint16_t* texP;                         // [TEXN*TEXN] 模様の番号(下8ビット)+ つやとラメ(上8ビット)。1回で読めるように
static float*    gU;                           // [CW*CW] 格子の点で読む、模様の位置
static float*    gV;
static float*    gU2;                          // [CW*CW] 2枚目の模様を読む位置
static float*    gV2;
struct Scratch { int32_t* rx; int32_t* ry; int32_t* rx2; int32_t* ry2; };
static Scratch   scr[2];
static uint16_t  palCur[256];                  // 今の色の表(バイト順はそのまま)
static uint16_t  whiteLv[16];                  // ハイライトの白(16段)
static uint32_t  busyUs[2];

static SemaphoreHandle_t semGo, semDone;
static volatile int readyW = -1, doneUpTo = -1;   // コア0が描き終えた帯 / 画面へ送り終えた帯
static bool fbMode = false;                      // StopWatch: 画面の手前に PSRAM の置き場がある(送るのは帯ごとに)
static int  SPLIT = 2;                           // コア1は SPLIT 本に1本の帯を描く(残りはコア0)
static uint32_t sendUs = 0;

static float simT = 0, scoreT = 0;
static int   theme = 0, brightIdx = 2;
static const uint8_t brightSteps[] = {60, 120, 190, 255};

// ---------------------------------------------------------------- 道具
struct Col { float r, g, b; };
static inline float clamp01(float v) { return v < 0 ? 0 : (v > 1 ? 1 : v); }
static inline Col lerpC(Col a, Col b, float t) { return {a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t}; }
static inline Col mulC(Col a, float k) { return {a.r * k, a.g * k, a.b * k}; }
static inline Col C8(int r, int g, int b) { return {r / 255.0f, g / 255.0f, b / 255.0f}; }
static inline uint16_t to565(Col c) {
  return M5.Display.color565((uint8_t)(clamp01(c.r) * 255), (uint8_t)(clamp01(c.g) * 255), (uint8_t)(clamp01(c.b) * 255));
}
// 光を足す(バイト順はそのまま。色ごとに頭打ち)。
// 各色のいちばん下の1段を捨てて足すと、あふれが隣の色のすき間に出るので、それを見て頭打ちにする(速い)
static inline uint16_t add565(uint32_t a, uint32_t b) {
  const uint32_t s = (a & 0xF7DE) + (b & 0xF7DE);
  const uint32_t ov = s & 0x10820;
  return (uint16_t)(s | (ov - (ov >> 5)));
}
// 画面の色(バイト順入れ替え済み)に光を足す
static inline uint16_t addPix(uint16_t dst, uint16_t add) {
  return __builtin_bswap16(add565(__builtin_bswap16(dst), add));   // 色がおかしい場合は、ここと下の bswap を外す
}
static inline uint16_t outPix(uint16_t c) { return __builtin_bswap16(c); }
// 画面の色を a/16 の明るさにする(穴のまわりを暗くする)
static inline uint16_t darken(uint16_t dst, int a) {
  if (a >= 16) return dst;
  const uint32_t c = __builtin_bswap16(dst);
  const uint32_t r = ((c >> 11) * a) >> 4, g = (((c >> 5) & 63) * a) >> 4, b = ((c & 31) * a) >> 4;
  return __builtin_bswap16((uint16_t)((r << 11) | (g << 5) | b));
}
// 画面の色を、白い光の色へ a/16 だけ近づける(明るい模様の上でも、光の粒が白く見えるように)
static uint16_t glowCol = 0xFFFF;
static inline uint16_t lighten(uint16_t dst, int a) {
  if (a <= 0) return dst;
  const uint32_t c = __builtin_bswap16(dst), w = glowCol;
  int r = c >> 11, g = (c >> 5) & 63, b = c & 31;
  r += ((int)(w >> 11) - r) * a >> 4; g += ((int)((w >> 5) & 63) - g) * a >> 4; b += ((int)(w & 31) - b) * a >> 4;
  return __builtin_bswap16((uint16_t)((r << 11) | (g << 5) | b));
}
static inline float frand() { return (esp_random() & 0xFFFF) / 65535.0f; }
static inline float frand(float a, float b) { return a + (b - a) * frand(); }

// ---------------------------------------------------------------- 色の組み合わせ
// 帯の並び: 色(と幅)を順に並べ、ところどころに黒と白の細い線をはさむ
struct Stop { Col c; float w; };
struct Theme { const char* name; const Stop* st; int n; Col star; };
static const Stop ST_PSYCHE[] = {
  {C8(200,20,25),3}, {C8(0,0,0),0.6f}, {C8(245,110,30),2}, {C8(250,215,60),2}, {C8(255,255,255),0.5f},
  {C8(120,215,60),2}, {C8(20,90,40),0.8f}, {C8(60,200,235),3}, {C8(0,0,0),0.6f}, {C8(130,195,245),2},
  {C8(35,55,190),2}, {C8(255,255,255),0.5f}, {C8(225,50,140),1.5f}, {C8(150,30,20),1.5f}, {C8(0,0,0),0.7f},
  {C8(230,40,30),2.5f}, {C8(250,150,40),1.2f}, {C8(70,220,220),2}, {C8(255,245,200),0.5f}, {C8(110,60,30),1.2f},
};
static const Stop ST_OCEAN[] = {
  {C8(10,40,110),3}, {C8(0,0,0),0.6f}, {C8(30,140,210),2.5f}, {C8(120,230,240),2}, {C8(255,255,255),0.6f},
  {C8(20,180,160),2}, {C8(5,60,80),1}, {C8(90,120,240),2}, {C8(0,0,0),0.6f}, {C8(200,240,255),1.2f},
  {C8(40,90,180),2}, {C8(255,255,255),0.5f}, {C8(60,220,200),2}, {C8(10,30,70),1.5f},
};
static const Stop ST_EMBER[] = {
  {C8(120,10,10),3}, {C8(0,0,0),0.6f}, {C8(230,60,20),2.5f}, {C8(255,160,40),2}, {C8(255,240,180),0.6f},
  {C8(250,210,80),1.5f}, {C8(90,20,10),1.2f}, {C8(200,30,40),2}, {C8(0,0,0),0.6f}, {C8(255,120,60),2},
  {C8(60,10,20),1.5f}, {C8(255,255,255),0.5f}, {C8(240,190,60),2}, {C8(140,40,10),1.5f},
};
static const Stop ST_NEON[] = {
  {C8(255,30,170),3}, {C8(0,0,0),0.8f}, {C8(40,240,255),2.5f}, {C8(255,255,255),0.5f}, {C8(150,255,60),2},
  {C8(0,0,0),0.8f}, {C8(130,60,255),2.5f}, {C8(255,230,40),1.5f}, {C8(0,0,0),0.8f}, {C8(255,80,90),2},
  {C8(30,120,255),2}, {C8(255,255,255),0.5f},
};
#define NS(a) (int)(sizeof(a) / sizeof(a[0]))
static const Theme THEMES[] = {
  {"Psyche", ST_PSYCHE, NS(ST_PSYCHE), C8(255,250,235)},
  {"Ocean",  ST_OCEAN,  NS(ST_OCEAN),  C8(220,245,255)},
  {"Ember",  ST_EMBER,  NS(ST_EMBER),  C8(255,235,200)},
  {"Neon",   ST_NEON,   NS(ST_NEON),   C8(240,240,255)},
};
static constexpr int NTHEME = sizeof(THEMES) / sizeof(THEMES[0]);
static Col palBase[256];                        // 番号 → 色(明るさ 1)

static void buildPalette() {
  const Theme& th = THEMES[theme];
  float total = 0;
  for (int i = 0; i < th.n; ++i) total += th.st[i].w;
  // 帯の中はほぼ同じ色で、境目だけ少しなめらかに
  float pos = 0;
  for (int i = 0; i < th.n; ++i) {
    const int a = (int)(pos / total * 256.0f), b = (int)((pos + th.st[i].w) / total * 256.0f);
    const Col c = th.st[i].c, nxt = th.st[(i + 1) % th.n].c;
    for (int k = a; k < b && k < 256; ++k) {
      const float t = (float)(k - a) / max(1, b - a);
      palBase[k] = t > 0.8f ? lerpC(c, nxt, (t - 0.8f) / 0.2f * 0.5f) : c;
    }
    pos += th.st[i].w;
  }
  for (int i = 0; i < 16; ++i) whiteLv[i] = to565(mulC(th.star, i / 15.0f));
  glowCol = to565(th.star);
}

// ---------------------------------------------------------------- 模様の絵(起動時に1回)
static inline uint32_t hash3(uint32_t x, uint32_t y, uint32_t s) {
  uint32_t h = x * 374761393u + y * 668265263u + s * 2246822519u;
  h = (h ^ (h >> 13)) * 1274126177u;
  return h ^ (h >> 16);
}
// 格子の値は先に表にしておく(1点ごとに乱数を計算しない)。表は 1 周 n 個で、端がつながる
struct Lattice { int n; float* v; };
static Lattice makeLattice(int n, uint32_t seed) {
  Lattice L = {n, (float*)malloc(n * n * sizeof(float))};
  if (L.v) for (int j = 0; j < n; ++j) for (int i = 0; i < n; ++i) L.v[j * n + i] = (hash3(i, j, seed) & 0xFFFF) / 65535.0f;
  return L;
}
static inline float vnoise(const Lattice& L, float x, float y) {   // x, y: 0..1 で1周
  const int n = L.n;
  const float fx = x * n + 64.0f * n, fy = y * n + 64.0f * n;      // 少し負でも大丈夫なように、かさ上げしてから切り捨て
  int ix = (int)fx, iy = (int)fy;
  const float tx = fx - ix, ty = fy - iy;
  const float sx = tx * tx * (3 - 2 * tx), sy = ty * ty * (3 - 2 * ty);
  ix %= n; iy %= n;
  const int jx = ix + 1 == n ? 0 : ix + 1, jy = iy + 1 == n ? 0 : iy + 1;
  const float* v = L.v;
  const float a = v[iy * n + ix], b = v[iy * n + jx], c = v[jy * n + ix], d = v[jy * n + jx];
  return a + (b - a) * sx + (c - a) * sy + (a - b - c + d) * sx * sy;
}
struct Fbm { Lattice o[4]; };
static Fbm makeFbm(uint32_t seed) { Fbm f; for (int k = 0; k < 4; ++k) f.o[k] = makeLattice(3 << k, seed + k * 17); return f; }
static void freeFbm(Fbm& f) { for (int k = 0; k < 4; ++k) free(f.o[k].v); }
static inline float fbm(const Fbm& f, float x, float y) {
  return 0.5f * vnoise(f.o[0], x, y) + 0.25f * vnoise(f.o[1], x, y) + 0.125f * vnoise(f.o[2], x, y) + 0.0625f * vnoise(f.o[3], x, y);
}
static void buildTexture() {
  Fbm F11 = makeFbm(11), F23 = makeFbm(23), F37 = makeFbm(37), F41 = makeFbm(41), F59 = makeFbm(59);
  float* bandsF = (float*)ps_malloc(TEXN * TEXN * sizeof(float));   // つやを計算するために、帯に切る前の値も取っておく
  for (int j = 0; j < TEXN; ++j)
    for (int i = 0; i < TEXN; ++i) {
      const float x = (float)i / TEXN, y = (float)j / TEXN;
      // ノイズでノイズをゆがめて(2回)、ぐにゃぐにゃの流れにする
      const float wx = fbm(F11, x, y), wy = fbm(F23, x, y);
      const float x2 = x + 0.45f * (wx - 0.5f), y2 = y + 0.45f * (wy - 0.5f);
      const float vx = fbm(F37, x2 + 0.3f, y2), vy = fbm(F41, x2, y2 + 0.7f);
      const float f = fbm(F59, x2 + 0.35f * (vx - 0.5f), y2 + 0.35f * (vy - 0.5f));
      // 細い帯に切る(帯の幅が場所で少し変わるように)
      const float bands = f * (5.0f + 2.0f * vx) * PATTERN + 0.6f * vy;
      tex[j * TEXN + i] = (uint8_t)((int)((bands - floorf(bands)) * 256.0f) & 255);
      if (bandsF) bandsF[j * TEXN + i] = bands;
    }
  // つや: 帯の1本1本を丸い管と見て、光の来る側(左上)の肩を光らせる
  // ラメ: ところどころの点に、瞬く順番(1..15)をつける
  const float Lx = -0.62f, Ly = -0.78f;
  for (int j = 0; j < TEXN; ++j)
    for (int i = 0; i < TEXN; ++i) {
      int sh = 0;
      if (bandsF) {
        const float* b = bandsF;
        const float gx = b[j * TEXN + ((i + 1) & (TEXN - 1))] - b[j * TEXN + ((i - 1) & (TEXN - 1))];
        const float gy = b[((j + 1) & (TEXN - 1)) * TEXN + i] - b[((j - 1) & (TEXN - 1)) * TEXN + i];
        const float gl = sqrtf(gx * gx + gy * gy) + 1e-6f;
        const float face = (gx * Lx + gy * Ly) / gl;            // 帯の向きが光の方を向いているか
        const float p = b[j * TEXN + i] - floorf(b[j * TEXN + i]);   // 帯の中のどこか(0..1)
        const float tube = cosf(3.14159f * p);                  // 管の肩(+1 / -1)
        const float v = face * tube;
        if (v > 0.40f) sh = (int)((v - 0.40f) / 0.60f * 15.0f + 0.5f);
        if (sh > 15) sh = 15;
      }
      const uint32_t hsh = hash3(i, j, 777);
      const int g = ((hsh & 1023) < 80) ? (int)(1 + (hsh >> 12) % 15) : 0;   // 約8% の点がラメ
      tex2[j * TEXN + i] = (uint8_t)((g << 4) | sh);
    }
  free(bandsF);
  freeFbm(F11); freeFbm(F23); freeFbm(F37); freeFbm(F41); freeFbm(F59);
  for (int i = 0; i < TEXN * TEXN; ++i) texP[i] = (uint16_t)(tex[i] | (tex2[i] << 8));   // 1回で読めるようにまとめる
}

// ---------------------------------------------------------------- 黒い球と渦
static float sinT[1024];
static inline float lsin(float a) { int i = (int)(a * (1024.0f / 6.2831853f)) & 1023; return sinT[i]; }
static inline float lcos(float a) { int i = ((int)(a * (1024.0f / 6.2831853f)) + 256) & 1023; return sinT[i]; }

struct Hole {
  float x, y, vx, vy;              // 位置と速さ(ピクセル)
  float R, dir, wand;              // 球の大きさ / 渦の向き(+1 / -1)/ さまよう位相
  float pulse;                     // キックで脈打つ
  bool  sphere;                    // 球があるか(タップでできた渦は、球なし)
  float life;                      // 球なしの渦の残り時間
  float tR;                        // 目標の大きさ(長押し・2回タップで変わり、R がそこへ近づく)
  bool  dying;                     // しぼんで消えていくところ
  // 描くための値
  int   y0, y1; float cR;
};
static Hole hole[MAXH];
static int  nHole = NHOLE;
static int  grabbed = -1;
static bool dragging = false;     // つかんだ穴を、指で動かしているところ
static float twist = 0;            // キックでぐっとねじれる分
static float flowX = 0, flowY = 0;  // 模様全体のゆっくりした流れ
static float fingerX = 0, fingerY = 0, fingerVX = 0, fingerVY = 0;
// 流れに運ばれる模様(2枚): 格子の点ごとに「今ここに来ている模様は、もとはどこにあったか」(ピクセル)
static float *cAx, *cAy, *cBx, *cBy, *cTx, *cTy;
static float *flowVX, *flowVY;                 // [CW*CW] 流れの速さ(2枚の模様で使い回す)
static float cycT = 0;
static int   mixQ = 0;                         // 2枚目の割合(0..16)
static float palShift = 0;

static bool inHole(float x, float y) {
  for (int i = 0; i < nHole; ++i) {
    const Hole& h = hole[i];
    if (!h.sphere) continue;
    const float dx = x - h.x, dy = y - h.y, r = h.R * 1.15f;
    if (dx * dx + dy * dy < r * r) return true;
  }
  return false;
}
// 指の下にあるブラックホール(なければ -1)。小さい穴もつかみやすいように、少し広めに
static int holeAt(float x, float y) {
  int best = -1; float bd = 1e30f;
  for (int i = 0; i < nHole; ++i) {
    const Hole& h = hole[i];
    if (!h.sphere || h.dying) continue;
    const float lim = fmaxf(h.R * 1.6f, 0.09f * VR), dx = x - h.x, dy = y - h.y, d2 = dx * dx + dy * dy;
    if (d2 < lim * lim && d2 < bd) { bd = d2; best = i; }
  }
  return best;
}
static int countBH() { int n = 0; for (int i = 0; i < nHole; ++i) n += hole[i].sphere; return n; }
// 穴を1つ取りのぞく(最後の穴を空いた所へ移すので、つかんでいる番号も合わせる)
static void removeHole(int i) {
  hole[i] = hole[nHole - 1];
  if (grabbed == i) grabbed = -1;
  else if (grabbed == nHole - 1) grabbed = i;
  --nHole;
}
static float maxR() {                       // 画面に収まる、いちばん大きい穴
  const float vis = (float)min(min(S, H - YOFF) - max(0, -YOFF), min(S, W - XOFF) - max(0, -XOFF));
  return fminf(R_MAX * VR, 0.42f * vis);
}
static void initHoles() {
  for (int i = 0; i < NHOLE; ++i) {
    Hole& h = hole[i];
    const float a = 6.2832f * i / NHOLE + 0.5f, r = (i == 0 ? 0.15f : 0.48f) * VR;
    h.x = Sc + cosf(a) * r; h.y = Sc + sinf(a) * r;
    h.vx = h.vy = 0;
    h.R = VR * (i == 0 ? 0.15f : 0.075f + 0.025f * (i % 3));   // 1つだけ大きい球
    h.dir = (i & 1) ? -1.0f : 1.0f;
    h.wand = i * 1.7f; h.pulse = 0; h.sphere = true; h.life = 1e9f; h.cR = h.R;
    h.tR = h.R; h.dying = false;
  }
  nHole = NHOLE;
}

// ---------------------------------------------------------------- 振り付け(マイクが聞こえないとき)
// 曲の解析から: 0:00 イントロ / 0:56 ドロップ / 2:08 ブレイク / 2:24 ドロップ / 2:46 静か / 3:05 盛り上がり / 3:28 最後のドロップ / 4:28 お別れ
struct Part { float t0, energy, kick; };
static const Part SCORE[] = {
  {0.0f, 0.30f, 0.0f}, {56.0f, 1.00f, 1.0f}, {128.0f, 0.45f, 0.0f}, {144.0f, 0.95f, 1.0f},
  {166.0f, 0.18f, 0.0f}, {185.0f, 0.50f, 0.5f}, {208.0f, 1.00f, 1.0f}, {268.0f, 0.05f, 0.0f},
};
static constexpr int NPART = sizeof(SCORE) / sizeof(SCORE[0]);
static int scorePart(float t) {
  int p = 0;
  while (p + 1 < NPART && t >= SCORE[p + 1].t0) ++p;
  return p;
}

// ---------------------------------------------------------------- 音(マイク)
static int16_t micBuf[4][MICN];
static int     micW = 0, micQueued = 0;
static bool    micOK = false, micLive = false;
static float   micFloor = 0.002f, micAgc = 0.02f, micLow = 0, micLowAvg = 0, micLowAgc = 1e-4f, micHigh = 0, micLevel = 0;
static float   micLastKick = -10.0f, micHeard = -100.0f;
static float   spec[NBAR / 2];                  // 高さごとの大きさ(0..1)
static float   fre[MICN], fim[MICN];
static bool    kickNow = false;

static void fft(float* re, float* im, int n) {
  for (int i = 1, j = 0; i < n; ++i) {
    int bit = n >> 1;
    for (; j & bit; bit >>= 1) j ^= bit;
    j ^= bit;
    if (i < j) { float t = re[i]; re[i] = re[j]; re[j] = t; t = im[i]; im[i] = im[j]; im[j] = t; }
  }
  for (int len = 2; len <= n; len <<= 1) {
    const float ang = -6.2831853f / len, wr = cosf(ang), wi = sinf(ang);
    for (int i = 0; i < n; i += len) {
      float cr = 1, ci = 0;
      for (int k = 0; k < len / 2; ++k) {
        const int a = i + k, b = a + len / 2;
        const float tr = re[b] * cr - im[b] * ci, ti = re[b] * ci + im[b] * cr;
        re[b] = re[a] - tr; im[b] = im[a] - ti; re[a] += tr; im[a] += ti;
        const float nr = cr * wr - ci * wi; ci = cr * wi + ci * wr; cr = nr;
      }
    }
  }
}

// 1回分(512個 = 32ミリ秒)の音を調べる: 大きさ / 低い音(キック) / 高い音 / 高さごとの大きさ
static float micWin[MICN];                       // 窓(起動時に1回だけ計算)
static void analyzeMic(const int16_t* d) {
  if (micWin[MICN / 2] == 0) for (int i = 0; i < MICN; ++i) micWin[i] = 0.5f - 0.5f * cosf(6.2831853f * i / (MICN - 1));
  float sum = 0;
  for (int i = 0; i < MICN; ++i) {
    const float s = d[i] * (MIC_GAIN / 32768.0f);
    fre[i] = s * micWin[i]; fim[i] = 0;
    sum += s * s;
  }
  const float rms = sqrtf(sum / MICN);
  fft(fre, fim, MICN);
  float low = 0, high = 0;
  static float band[NBAR / 2];
  for (int b = 0; b < NBAR / 2; ++b) band[b] = 0;
  // (高さごとの大きさは v2 から使っていないので、キックと高い音に使う所だけ計算する)
  const int kLow = (int)(110.0f * MICN / MICRATE), kHigh = (int)(3000.0f * MICN / MICRATE) + 1;
  for (int k = 1; k < MICN / 2; ++k) {
    if (k >= kLow && k < kHigh) { k = kHigh - 1; continue; }
    const float m = sqrtf(fre[k] * fre[k] + fim[k] * fim[k]);
    if (k < kLow) low += m; else high += m;
  }
  // 静かなときの大きさ(雑音の床)と、大きい音の目安(自動で合わせる)
  // 雑音の床: 静かになるとすぐ下がり、音が続いてもほとんど上がらない(曲を流し続けても「聞こえない」にならないように)
  micFloor = rms < micFloor ? micFloor * 0.9f + rms * 0.1f : micFloor * 0.99995f + rms * 0.00005f;
  micAgc = fmaxf(micAgc * 0.9995f, rms);                   // 大きい音の目安は、1分くらいかけてゆっくり下がる
  micLowAgc = fmaxf(micLowAgc * 0.9995f, low);
  const bool heard = rms > fmaxf(0.004f, micFloor * 2.5f);
  if (heard) micHeard = simT;
  micLevel = clamp01((rms / (micAgc + 1e-6f) - 0.3f) / 0.7f);   // いちばん大きい所で 1、3割以下で 0
  const float lowN = low / (MICN * 0.25f), highN = high / (MICN * 0.25f);
  micHigh = micHigh * 0.6f + clamp01(highN / (micAgc * 6.0f + 1e-6f)) * 0.4f;
  // キック: いちばん低い音が、ふだんよりぐっと大きくなった瞬間(しかも、この曲の中でも大きい方)
  kickNow = false;
  if (low > micLowAvg * 1.35f && low > micLow * 1.1f && low > micLowAgc * 0.40f && simT - micLastKick > 0.25f && heard) {
    kickNow = true; micLastKick = simT;
  }
  micLow = low;
  micLowAvg = micLowAvg * 0.9f + low * 0.1f;
  (void)lowN;
  float mx = 1e-6f;
  for (int b = 0; b < NBAR / 2; ++b) mx = fmaxf(mx, band[b]);
  static float bandAgc = 1e-3f;
  bandAgc = fmaxf(bandAgc * 0.995f, mx);
  for (int b = 0; b < NBAR / 2; ++b) {
    const float v = clamp01(sqrtf(band[b] / bandAgc));
    spec[b] = v > spec[b] ? v : spec[b] * 0.85f + v * 0.15f;
  }
}

static void pollMic() {
  if (!micOK) return;
  // 録音を順番に予約していき、2つ前に予約した分(もう録り終わっている)を調べる
  for (int guard = 0; guard < 3; ++guard) {
    if (M5.Mic.isRecording() >= 2) break;                   // 予約がいっぱい(待たずに、次のフレームで)
    if (!M5.Mic.record(micBuf[micW], MICN, MICRATE)) break;
    ++micQueued;
    if (micQueued >= 3) analyzeMic(micBuf[(micW + 2) & 3]);
    micW = (micW + 1) & 3;
  }
}

// ---------------------------------------------------------------- 光の粒
struct Star { float x, y, L, life, maxL, tw; };
static Star star[MAXSTAR];
static int  starNext = 0;
struct SDraw { int16_t x, y, L; uint16_t col[8]; };
static SDraw sd[MAXSTAR];
static int   nSD = 0;
static int16_t ent[MAXENT];                     // 帯ごとの、光の粒(番号)と球(10000 + 番号)
static int16_t entStart[MAXNB + 1];
static bool inHole(float x, float y);
static uint8_t starQ[64];                       // 距離の2乗 → 明るさの段(平方根の表)
static void addStar(float x, float y, float L, float life) {
  if (inHole(x, y)) return;                                  // 黒い穴の中には描かない
  // 空いている所を少し探す(見つからなければ、いちばん古いものと入れかえ)
  for (int t = 0; t < 8 && star[starNext].life > 0; ++t) starNext = (starNext + 1) % MAXSTAR;
  Star& s = star[starNext]; starNext = (starNext + 1) % MAXSTAR;
  s = {x, y, L, life, life, frand(0, 6.2832f)};
}

// ---------------------------------------------------------------- 1フレームの準備
struct Input { bool touching; bool tapped; float tx, ty; float dt; };
static float level = 0, hiLevel = 0;            // 今の盛り上がり / 高い音の多さ(0..1)
static bool  liveWas = false;
static float flash = 0;

static void updateFrame(const Input& in) {
  float dt = in.dt > 0.05f ? 0.05f : in.dt;
  if (dt < 0.0005f) dt = 0.0005f;
  simT += dt;
  scoreT += dt;
  if (scoreT >= SONG_LEN) scoreT -= SONG_LEN;

  // ---- 音: マイクが聞こえていればそれを、聞こえなければ振り付けを使う(v1 と同じ)
  pollMic();
  micLive = micOK && (simT - micHeard < 3.0f);
  if (micLive != liveWas) { Serial.println(micLive ? "mode: mic" : "mode: score"); liveWas = micLive; }
  bool kick = false;
  float targetLevel, targetHi;
  if (micLive) {
    kick = kickNow; kickNow = false;
    targetLevel = micLevel; targetHi = micHigh;
  } else {
    const int p = scorePart(scoreT);
    const Part& P = SCORE[p];
    float e = P.energy;
    if (p == 5) e = 0.4f + 0.5f * (scoreT - P.t0) / (SCORE[6].t0 - P.t0);
    if (p == NPART - 1) e = 0.05f;
    const float beat = (scoreT - FIRST_BEAT) * BPM / 60.0f;
    static float lastBeat = -1e9f;
    const float bi = floorf(beat);
    if (bi != lastBeat) {
      lastBeat = bi;
      const int ib = (int)bi;
      if (P.kick >= 1.0f) kick = true;
      else if (P.kick > 0 && (ib & 1) == 0) kick = true;
    }
    const float ph = beat - bi;
    targetLevel = e * (0.8f + 0.2f * expf(-ph * 4.0f));
    targetHi = e * (0.4f + 0.3f * (0.5f + 0.5f * sinf(scoreT * 9.3f)));
  }
  level += (targetLevel - level) * (1.0f - expf(-dt * 6.0f));
  hiLevel += (targetHi - hiLevel) * (1.0f - expf(-dt * 6.0f));

  // ---- キック: 渦がぐっとねじれて、球が脈打ち、色がパッと進む
  if (kick) {
    twist += 0.9f + 0.6f * level;
    palShift += 10.0f;
    flash = 1.0f;
    for (int i = 0; i < nHole; ++i) hole[i].pulse = 1.0f;
    for (int i = 0; i < 16; ++i) {                           // 穴のまわりに光の粒
      if (nHole == 0) break;
      const Hole& h = hole[esp_random() % nHole];
      if (!h.sphere) continue;
      const float a = frand(0, 6.2832f), r = h.R * frand(1.3f, 2.2f);
      addStar(h.x + cosf(a) * r, h.y + sinf(a) * r, frand(4, 11) * (VR / 233.0f), frand(0.5f, 1.2f));
    }
    for (int i = 0; i < 3; ++i) {                            // 大きな星がはじける
      const float a = frand(0, 6.2832f), r = sqrtf(frand(0.05f, 0.7f)) * VR;
      addStar(Sc + cosf(a) * r, Sc + sinf(a) * r, frand(12, 20) * (VR / 233.0f), frand(0.35f, 0.7f));
    }
  }
  twist *= expf(-dt * 2.5f);
  flash *= expf(-dt * 6.0f);
  palShift += dt * (18.0f + 110.0f * level) * FLOW;
  while (palShift >= 256.0f) palShift -= 256.0f;
  flowX += dt * (6.0f + 14.0f * level); flowY += dt * (4.0f + 9.0f * level);
  { const float per = 256.0f / (256.0f / S * ZOOM);                 // 模様1枚ぶん(ピクセル)で巻き戻す(数が大きくなりすぎないように)
    if (flowX > per) flowX -= per;
    if (flowY > per) flowY -= per; }

  // ---- 指: タップで新しい渦 / なぞると、いちばん近い球が指についてくる
  // 押した瞬間: 穴の上なら、つかむ(続けて2回なら、しぼむ)。穴のない所なら、小さな渦が生まれる
  static float pressT = 0, pressX = 0, pressY = 0, lastTapT = -9, lastTapX = 0, lastTapY = 0;
  static bool  pressMoved = false, pressEmpty = false;
  const float RMX = maxR();
  if (in.tapped) {
    pressT = simT; pressX = in.tx; pressY = in.ty; pressMoved = false;
    const int hi = holeAt(in.tx, in.ty);
    pressEmpty = hi < 0;
    const float ddx = in.tx - lastTapX, ddy = in.ty - lastTapY;
    const bool dbl = simT - lastTapT < 0.35f && ddx * ddx + ddy * ddy < 0.01f * VR * VR;
    lastTapT = simT; lastTapX = in.tx; lastTapY = in.ty;
    grabbed = -1;
    if (hi >= 0 && dbl) {
      // 2回タップ: しぼむ
      Hole& h = hole[hi];
      h.tR *= SHRINK; h.pulse = 1.0f; twist += 0.5f;
      if (h.tR < R_MIN * VR) {                                  // 小さくなりすぎた: 光をまき散らして消える
        h.dying = true; h.tR = 0;
        twist += 1.2f; flash = 1.0f; palShift += 12.0f;
        for (int k = 0; k < 40; ++k) {
          const float a = frand(0, 6.2832f), r = h.R * frand(1.2f, 3.0f);
          addStar(h.x + cosf(a) * r, h.y + sinf(a) * r, frand(4, 14) * (VR / 233.0f), frand(0.6f, 1.6f));
        }
      }
      pressEmpty = false;
    } else if (hi >= 0) {
      grabbed = hi;
    } else if (nHole < MAXH) {                                  // 穴のない所: 小さな渦(しばらくすると消える)
      Hole& h = hole[nHole++];
      h = {in.tx, in.ty, 0, 0, VR * 0.06f, (esp_random() & 1) ? 1.0f : -1.0f, 0, 1.0f, false, 2.5f, VR * 0.06f, false, 0, 0, 0};
    }
    for (int i = 0; i < 24; ++i) {
      const float a = frand(0, 6.2832f), r = frand(0.0f, 0.15f) * VR;
      addStar(in.tx + cosf(a) * r, in.ty + sinf(a) * r, frand(3, 10) * (VR / 233.0f), frand(0.4f, 1.0f));
    }
  }
  // 押し続けている間: 動かさずに長押しすると、穴が育つ(穴のない所なら、新しい穴が生まれる)
  if (in.touching) {
    const float mx = in.tx - pressX, my = in.ty - pressY;
    if (mx * mx + my * my > 0.0036f * VR * VR) pressMoved = true;
    dragging = pressMoved && grabbed >= 0;
    if (!pressMoved && simT - pressT > HOLD_T) {
      if (pressEmpty) {
        pressEmpty = false;
        if (countBH() < MAXBH && nHole < MAXH) {
          Hole& h = hole[nHole];
          const float r0 = R_MIN * VR * 1.3f;
          h = {in.tx, in.ty, 0, 0, r0 * 0.3f, (esp_random() & 1) ? 1.0f : -1.0f, frand(0, 6.0f), 1.0f, true, 1e9f, r0, false, 0, 0, r0 * 0.3f};
          grabbed = nHole++;
          twist += 0.8f; flash = fmaxf(flash, 0.6f);
        }
      } else if (grabbed >= 0) {
        Hole& h = hole[grabbed];
        float others = 0;                                          // ほかの穴の面積(合計の上限を超えないように)
        for (int j = 0; j < nHole; ++j) if (j != grabbed && hole[j].sphere && !hole[j].dying) others += hole[j].tR * hole[j].tR;
        const float room = sqrtf(fmaxf(0.0f, AREA_MAX * RMX * RMX - others));
        h.tR = fminf(fminf(h.tR + GROW * VR * dt, RMX), fmaxf(h.tR, room));
        h.pulse = fmaxf(h.pulse, 0.35f);
        if (h.tR < fminf(RMX, room) && frand() < dt * 45.0f) {                // まわりの光を吸い込んでいるように
          const float a = frand(0, 6.2832f), r = h.R * frand(1.6f, 2.8f);
          addStar(h.x + cosf(a) * r, h.y + sinf(a) * r, frand(3, 8) * (VR / 233.0f), frand(0.6f, 1.4f));
        }
      }
    }
  }
  if (!in.touching) { grabbed = -1; dragging = false; }
  // なぞると、指の動きで流れができる(球をつかんでいないとき)
  {
    static float pX = 0, pY = 0; static bool was = false;
    fingerVX = fingerVY = 0;
    if (in.touching && was && grabbed < 0) { fingerVX = (in.tx - pX) / dt; fingerVY = (in.ty - pY) / dt; }
    pX = in.tx; pY = in.ty; was = in.touching;
    fingerX = in.tx; fingerY = in.ty;
  }

  // ---- 球の動き: ゆっくりさまよい、互いに離れ、画面からはみ出さない
  const float vx0 = (float)max(0, -XOFF), vx1 = (float)min(S, W - XOFF) - 1;
  const float vy0 = (float)max(0, -YOFF), vy1 = (float)min(S, H - YOFF) - 1;
  for (int i = 0; i < nHole; ++i) {
    Hole& h = hole[i];
    if (!h.sphere) {                                          // タップの渦は、しばらくして消える
      h.life -= dt;
      if (h.life <= 0) { removeHole(i); --i; continue; }
      continue;
    }
    // 大きさ: 目標へなめらかに近づく。消えていく穴は、すっとしぼんでなくなる
    if (h.dying) {
      h.R *= expf(-dt * 7.0f);
      if (h.R < 1.5f) { removeHole(i); --i; continue; }
    } else {
      if (h.tR > RMX) h.tR = RMX;
      h.R += (h.tR - h.R) * (1.0f - expf(-dt * 6.0f));
    }
    h.wand += dt * 0.12f;
    float tx = Sc + 0.45f * VR * sinf(h.wand * 1.3f + i * 2.0f) * cosf(h.wand * 0.6f + i);
    float ty = Sc + 0.40f * VR * sinf(h.wand * 1.1f + i * 1.3f);
    if (i == grabbed) { tx = in.tx; ty = in.ty; }
    const float kk = (i == grabbed) ? 12.0f : 0.4f;
    float ax = (tx - h.x) * kk - h.vx * (i == grabbed ? 6.0f : 0.9f);
    float ay = (ty - h.y) * kk - h.vy * (i == grabbed ? 6.0f : 0.9f);
    for (int j = 0; j < nHole; ++j) {
      if (j == i || !hole[j].sphere || hole[j].dying) continue;
      const float dx = h.x - hole[j].x, dy = h.y - hole[j].y, d2 = dx * dx + dy * dy;
      if (d2 < 1.0f) continue;
      if (j == grabbed && i != grabbed && dragging) {
        // つかんだ穴が近づくと、引き寄せられて、まわりを回りながら落ちていく(結合)
        const float reach = (h.R + hole[j].R) * 1.8f;
        if (d2 < reach * reach) {
          const float d = sqrtf(d2), k = (reach - d) * 7.0f, t = (reach - d) * 4.0f * hole[j].dir;
          ax += -dx / d * k - dy / d * t; ay += -dy / d * k + dx / d * t;
        }
      } else if (i != grabbed) {
        const float want = (h.R + hole[j].R) * 2.2f;          // ふだんは、互いに離れる
        if (d2 < want * want) { const float d = sqrtf(d2), k = (want - d) * 3.0f; ax += dx / d * k; ay += dy / d * k; }
      }
    }
    h.vx += ax * dt; h.vy += ay * dt;
    h.x += h.vx * dt; h.y += h.vy * dt;
    if (isRound) {
      const float dx = h.x - Sc, dy = h.y - Sc, d = sqrtf(dx * dx + dy * dy) + 1e-3f, lim = 0.86f * VR - h.R;
      if (d > lim) { h.x = Sc + dx / d * lim; h.y = Sc + dy / d * lim; h.vx *= 0.5f; h.vy *= 0.5f; }
    } else {
      h.x = fminf(fmaxf(h.x, vx0 + h.R), vx1 - h.R); h.y = fminf(fmaxf(h.y, vy0 + h.R), vy1 - h.R);
    }
    h.pulse *= expf(-dt * 5.0f);
  }

  // ---- 結合: 2つの穴が重なったら1つに(面積を足した大きさ。1フレームに1回まで)
  {
    int ma = -1, mb = -1;
    for (int i = 0; i < nHole && ma < 0; ++i) {
      if (!hole[i].sphere || hole[i].dying) continue;
      for (int j = i + 1; j < nHole; ++j) {
        if (!hole[j].sphere || hole[j].dying) continue;
        const float dx = hole[i].x - hole[j].x, dy = hole[i].y - hole[j].y, lim = (hole[i].R + hole[j].R) * 0.6f;
        if (dx * dx + dy * dy < lim * lim) { ma = i; mb = j; break; }
      }
    }
    if (ma >= 0) {
      // 残すのは、つかんでいる方(なければ大きい方)
      if (mb == grabbed || (ma != grabbed && hole[mb].R > hole[ma].R)) { const int t = ma; ma = mb; mb = t; }
      Hole& a = hole[ma]; const Hole& b = hole[mb];
      const float wa = a.R * a.R, wb = b.R * b.R, wsum = wa + wb;
      a.x = (a.x * wa + b.x * wb) / wsum; a.y = (a.y * wa + b.y * wb) / wsum;   // 大きい方に寄った所(つかんでいれば、そこから指へ寄っていく)
      a.vx = (a.vx * wa + b.vx * wb) / wsum; a.vy = (a.vy * wa + b.vy * wb) / wsum;
      a.tR = fminf(sqrtf(a.tR * a.tR + b.tR * b.tR), RMX);
      a.R = fmaxf(a.R, b.R);
      a.pulse = 1.0f;
      twist += 1.8f; flash = 1.0f; palShift += 24.0f;
      for (int k = 0; k < 36; ++k) {
        const float an = frand(0, 6.2832f), r = a.tR * frand(1.3f, 2.8f);
        addStar(a.x + cosf(an) * r, a.y + sinf(an) * r, frand(5, 16) * (VR / 233.0f), frand(0.6f, 1.5f));
      }
      removeHole(mb);
    }
  }

  // ---- 流れ: うねり + 穴のまわりを回る流れ + 穴への吸い込み + 指の流れ
  //      模様の2枚の地図を、この流れで少しずつ運ぶ(上流から模様を持ってくる)
  {
    const float k = VR / 233.0f;
    const float boost = 0.7f + 0.8f * level + 1.6f * twist;
    struct HV { float x, y, G, Q, c2; } hv[MAXH];
    int nh = 0;
    for (int i = 0; i < nHole; ++i) {
      const Hole& h = hole[i];
      const float st = h.sphere ? 1.0f : h.life / 2.5f;
      const float R = h.sphere ? h.cR : h.R;
      hv[nh++] = {h.x, h.y, h.dir * SPIN * k * boost * st * 2.32f * R, INFLOW * k * boost * st * 2.32f * R, 0.64f * R * R};
    }
    const float fr = 0.16f * VR, fInv = 1.0f / (2 * fr * fr);
    const float wt = WANDER * k * (0.8f + 0.6f * level);
    auto vel = [&](float px, float py, float& vx, float& vy) {
      // うねり(見えない渦のゆっくりした流れ。湧き出しのない流れ)
      const float a1 = px * 0.013f + simT * 0.21f, a2 = py * 0.011f - simT * 0.17f, a3 = (px + py) * 0.008f + simT * 0.13f;
      vx = wt * (lcos(a2) * 1.0f + lcos(a3) * 0.7f);
      vy = wt * (-lcos(a1) * 1.0f - lcos(a3) * 0.7f);
      for (int i = 0; i < nh; ++i) {
        const float dx = px - hv[i].x, dy = py - hv[i].y, inv = 1.0f / (dx * dx + dy * dy + hv[i].c2);
        vx += (-dy * hv[i].G - dx * hv[i].Q) * inv;
        vy += ( dx * hv[i].G - dy * hv[i].Q) * inv;
      }
      if (fingerVX != 0 || fingerVY != 0) {
        const float fx = px - fingerX, fy = py - fingerY, d2 = fx * fx + fy * fy;
        if (d2 < 9.0f * fr * fr) { const float g = expf(-d2 * fInv) * STIR; vx += fingerVX * g; vy += fingerVY * g; }
      }
    };
    // 交代の時間割: 前半は A を見せて B を作り直し、後半は B を見せて A を作り直す。境目でそっと入れかえる
    const float half = LIFE * 0.5f, fade = 0.8f;                  // 入れかわりは 0.8 秒(2回読む時間を短く)
    const float prev = cycT;
    cycT += dt;
    if (cycT >= LIFE) cycT -= LIFE;
    auto resetMap = [&](float* cx, float* cy) {
      const float ox = frand(0, 400.0f), oy = frand(0, 400.0f);
      for (int gy = 0; gy < CW; ++gy)
        for (int gx = 0; gx < CW; ++gx) { cx[gy * CW + gx] = gx * CSTEP + ox; cy[gy * CW + gx] = gy * CSTEP + oy; }
    };
    if (prev > cycT) resetMap(cBx, cBy);                      // 前半に入った: B は見えていないので作り直す
    if (prev < half && cycT >= half) resetMap(cAx, cAy);      // 後半に入った: A を作り直す
    float wB;
    if (cycT < half) wB = cycT > half - fade ? (cycT - (half - fade)) / fade : 0.0f;
    else wB = cycT > LIFE - fade ? 1.0f - (cycT - (LIFE - fade)) / fade : 1.0f;
    wB = wB * wB * (3 - 2 * wB);
    mixQ = (int)(wB * 16.0f + 0.5f);
    // 運ぶ(格子の点ごとに、流れの上流の模様を持ってくる)
    const float inv = 1.0f / CSTEP;
    // 画面の外から来る分は、ふちの模様をそのまま外へのばしたものとみなす(ふちが平らな色にならないように)
    auto sample = [&](const float* c, float qx, float qy, bool isX) {
      const float gx0 = qx * inv, gy0 = qy * inv;
      const float gx = fminf(fmaxf(gx0, 0.0f), CW - 1.001f), gy = fminf(fmaxf(gy0, 0.0f), CW - 1.001f);
      const int ix = (int)gx, iy = (int)gy; const float fx = gx - ix, fy = gy - iy;
      const float* p0 = c + iy * CW + ix;
      const float a = p0[0] + (p0[1] - p0[0]) * fx, b = p0[CW] + (p0[CW + 1] - p0[CW]) * fx;
      return a + (b - a) * fy + (isX ? (gx0 - gx) : (gy0 - gy)) * CSTEP;
    };
    for (int gy = 0; gy < CW; ++gy)                          // 流れの速さは1回だけ計算して、2枚で使う
      for (int gx = 0; gx < CW; ++gx) {
        float vx, vy;
        vel((float)(gx * CSTEP), (float)(gy * CSTEP), vx, vy);
        const float sp2 = vx * vx + vy * vy, lim = 3.0f * CSTEP / dt;   // 1フレームで飛びすぎないように
        if (sp2 > lim * lim) { const float s2 = lim / sqrtf(sp2); vx *= s2; vy *= s2; }
        flowVX[gy * CW + gx] = vx * dt; flowVY[gy * CW + gx] = vy * dt;
      }
    for (int m = 0; m < 2; ++m) {
      float* cx = m ? cBx : cAx; float* cy = m ? cBy : cAy;
      for (int gy = 0; gy < CW; ++gy)
        for (int gx = 0; gx < CW; ++gx) {
          const float px = (float)(gx * CSTEP), py = (float)(gy * CSTEP);
          const float qx = px - flowVX[gy * CW + gx], qy = py - flowVY[gy * CW + gx];
          cTx[gy * CW + gx] = sample(cx, qx, qy, true);
          cTy[gy * CW + gx] = sample(cy, qx, qy, false);
        }
      memcpy(cx, cTx, CW * CW * sizeof(float)); memcpy(cy, cTy, CW * CW * sizeof(float));
    }
    const float sc = 256.0f / S * ZOOM;                       // 1ピクセルが模様の何目盛りか
    for (int i = 0; i < CW * CW; ++i) {
      gU[i] = cAx[i] * sc; gV[i] = cAy[i] * sc;
      gU2[i] = cBx[i] * sc; gV2[i] = cBy[i] * sc;
    }
  }

  // ---- 色の表(ずらすと、帯が流れる)
  {
    const float br = 0.90f + 0.15f * level + 0.25f * flash;
    const int sh = (int)palShift;
    for (int i = 0; i < 256; ++i) palCur[i] = to565(mulC(palBase[(i + sh) & 255], br));
    // つやとラメ: つやは盛り上がるほど強く、ラメは1つずつ順番に瞬く(キックや高い音で強く)
    const Col wc = THEMES[theme].star;
    float shine[16], glit[16];
    const float gloss = GLOSS * (0.80f + 0.20f * level + 0.4f * flash);   // いつもつやつや
    for (int q = 0; q < 16; ++q) shine[q] = gloss * powf(q / 15.0f, 1.3f);
    glit[0] = 0;
    for (int g = 1; g < 16; ++g) {
      float tw = 0.5f + 0.5f * sinf(simT * 7.0f + g * 2.618f);
      tw = tw * tw;
      glit[g] = GLITTER * tw * (1.3f + 0.6f * hiLevel + 1.0f * flash);   // いつもちらちら
    }
    for (int i = 0; i < 256; ++i) shineLUT[i] = to565(mulC(wc, fminf(1.2f, shine[i & 15] + glit[i >> 4])));
  }

  // ---- 球(描くための値)
  for (int i = 0; i < nHole; ++i) {
    Hole& h = hole[i];
    h.cR = h.R * (1.0f + 0.06f * h.pulse);
    h.y0 = (int)floorf(h.y - h.cR * 1.6f - 2); h.y1 = (int)ceilf(h.y + h.cR * 1.6f + 2);
  }

  // ---- 光の粒: 盛り上がるほど、高い音が多いほど増える
  {
    float n = (95.0f + 85.0f * level + 110.0f * hiLevel) * dt;    // 静かな所でも、光の粒はたくさん(長生きなので数は少し控えめに)
    while (n > 0) {
      if (frand() < n) {
        const float a = frand(0, 6.2832f), r = sqrtf(frand(0.0f, 0.85f)) * VR;
        addStar(Sc + cosf(a) * r, Sc + sinf(a) * r, frand(4.0f, 8.0f + 10.0f * level) * (VR / 233.0f), frand(0.7f, 2.0f));
      }
      n -= 1.0f;
    }
  }
  nSD = 0;
  {
    for (int i = 0; i < MAXSTAR; ++i) {
      Star& s = star[i];
      if (s.life <= 0) continue;
      s.life -= dt;
      if (s.life <= 0) continue;
      // 模様と同じ流れに乗って運ばれる(格子の流れを、粒の場所で補間)
      {
        const float gxf = fminf(fmaxf(s.x * (1.0f / CSTEP), 0.0f), CW - 1.001f);
        const float gyf = fminf(fmaxf(s.y * (1.0f / CSTEP), 0.0f), CW - 1.001f);
        const int ix = (int)gxf, iy = (int)gyf; const float fx = gxf - ix, fy = gyf - iy;
        const int p = iy * CW + ix;
        const float ax = flowVX[p] + (flowVX[p + 1] - flowVX[p]) * fx, bx = flowVX[p + CW] + (flowVX[p + CW + 1] - flowVX[p + CW]) * fx;
        const float ay = flowVY[p] + (flowVY[p + 1] - flowVY[p]) * fx, by = flowVY[p + CW] + (flowVY[p + CW + 1] - flowVY[p + CW]) * fx;
        s.x += ax + (bx - ax) * fy;
        s.y += ay + (by - ay) * fy;
      }
      // 穴に吸い込まれたら消える / 画面の外へ流れ出たら消える
      if (s.x < -20 || s.y < -20 || s.x > S + 20 || s.y > S + 20) { s.life = 0; continue; }
      {
        bool gone = false;
        for (int j = 0; j < nHole && !gone; ++j) {
          const Hole& h = hole[j];
          if (!h.sphere) continue;
          const float dx = s.x - h.x, dy = s.y - h.y;
          gone = dx * dx + dy * dy < h.R * h.R * 0.9f;
        }
        if (gone) { s.life = 0; continue; }
      }
      const float age = 1.0f - s.life / s.maxL;
      const float env = age < 0.2f ? age / 0.2f : 1.0f - (age - 0.2f) / 0.8f;
      const float k = env * (0.75f + 0.25f * sinf(s.tw + simT * 17.0f));
      SDraw& o = sd[nSD++];
      o.x = (int16_t)s.x; o.y = (int16_t)s.y; o.L = (int16_t)fmaxf(1.0f, s.L * (0.6f + 0.4f * env));
      for (int q = 0; q < 8; ++q) {
        const float f = 1.0f - q / 8.0f;
        int al = (int)(fminf(1.0f, 2.6f * k) * powf(f, 1.4f) * 16.0f + 0.5f);                  // 白へ近づける割合(0..16)
        o.col[q] = (uint16_t)(al > 16 ? 16 : al);
      }
    }
  }

  // ---- 帯ごとに並べる(球を先に、光の粒はその上)
  for (int k = 0; k <= NB; ++k) entStart[k] = 0;
  int total = 0, na = nSD, nh = 0;
  auto bandsOf = [&](int ya, int yb, int& a, int& b) { a = max(0, ya) / BAND; b = min(S - 1, max(0, yb)) / BAND; };
  int hid[MAXH];
  for (int i = 0; i < nHole; ++i) if (hole[i].sphere) hid[nh++] = i;
  for (int i = 0; i < nh; ++i) { int a, b; bandsOf(hole[hid[i]].y0, hole[hid[i]].y1, a, b); for (int k = a; k <= b; ++k) entStart[k + 1]++; total += b - a + 1; }
  for (int i = 0; i < na; ++i) {
    int a, b; bandsOf(sd[i].y - 2 * sd[i].L, sd[i].y + 2 * sd[i].L, a, b);
    if (total + (b - a + 1) > MAXENT) { na = i; break; }
    for (int k = a; k <= b; ++k) entStart[k + 1]++;
    total += b - a + 1;
  }
  for (int k = 0; k < NB; ++k) entStart[k + 1] += entStart[k];
  int16_t fill[MAXNB];
  for (int k = 0; k < NB; ++k) fill[k] = entStart[k];
  for (int i = 0; i < na; ++i) { int a, b; bandsOf(sd[i].y - 2 * sd[i].L, sd[i].y + 2 * sd[i].L, a, b); for (int k = a; k <= b; ++k) ent[fill[k]++] = (int16_t)i; }
  for (int i = 0; i < nh; ++i) { int a, b; bandsOf(hole[hid[i]].y0, hole[hid[i]].y1, a, b); for (int k = a; k <= b; ++k) ent[fill[k]++] = (int16_t)(10000 + hid[i]); }   // 穴は光の粒の上に
}

// ---------------------------------------------------------------- 描画(帯ごと)
static void renderBand(int k, uint16_t* buf, Scratch& sc) {
  const int y0 = k * BAND, y1 = min(S, y0 + BAND);
  const int bx0 = bandX0[k], bw = bandW[k], bx1 = bx0 + bw - 1;
  const uint16_t* const TP = texP;

  // ---- 模様: 格子の値を縦に補間し、横は足し算で進める
  for (int y = y0; y < y1; ++y) {
    uint16_t* row = buf + (y - y0) * bw - bx0;
    const int xs = max((int)spanS[y], bx0), xe = min((int)spanE[y], bx1);
    for (int x = bx0; x < xs; ++x) row[x] = 0;
    for (int x = max(xe + 1, bx0); x <= bx1; ++x) row[x] = 0;
    if (xs > xe) continue;
    const int gy = y / CSTEP; const float fy = (y - gy * CSTEP) * (1.0f / CSTEP);
    const int q = mixQ;                                       // 2枚目の割合(0..16)
    const int gx0 = xs / CSTEP, gx1 = xe / CSTEP + 1;
    // 1枚だけ見えているときは1回、入れかわりの途中は2回読んで混ぜる
    const float *au = (q >= 16 ? gU2 : gU) + gy * CW, *av = (q >= 16 ? gV2 : gV) + gy * CW;
    for (int gx = gx0; gx <= gx1; ++gx) {
      sc.rx[gx] = (int32_t)((au[gx] + (au[gx + CW] - au[gx]) * fy) * 256.0f);
      sc.ry[gx] = (int32_t)((av[gx] + (av[gx + CW] - av[gx]) * fy) * 256.0f);
    }
    const bool two = (q > 0 && q < 16);
    if (two) {
      const float *bu = gU2 + gy * CW, *bv = gV2 + gy * CW;
      for (int gx = gx0; gx <= gx1; ++gx) {
        sc.rx2[gx] = (int32_t)((bu[gx] + (bu[gx + CW] - bu[gx]) * fy) * 256.0f);
        sc.ry2[gx] = (int32_t)((bv[gx] + (bv[gx + CW] - bv[gx]) * fy) * 256.0f);
      }
    }
    int x = xs;
    while (x <= xe) {
      const int gx = x / CSTEP;
      const int segEnd = min(xe, gx * CSTEP + CSTEP - 1);
      const int32_t su = (sc.rx[gx + 1] - sc.rx[gx] + (1 << (USHIFT - 1))) >> USHIFT;
      const int32_t sv = (sc.ry[gx + 1] - sc.ry[gx] + (1 << (USHIFT - 1))) >> USHIFT;
      const int off = x - gx * CSTEP;
      int32_t u = sc.rx[gx] + su * off, v = sc.ry[gx] + sv * off;
      if (!two) {
        for (; x <= segEnd; ++x, u += su, v += sv) {
          const uint32_t tv = TP[((((uint32_t)v >> 8) & 255) << 8) | (((uint32_t)u >> 8) & 255)];
          const uint16_t c = palCur[tv & 255];
          row[x] = __builtin_bswap16((tv >> 8) ? add565(c, shineLUT[tv >> 8]) : c);
        }
        continue;
      }
      const int32_t su2 = (sc.rx2[gx + 1] - sc.rx2[gx] + (1 << (USHIFT - 1))) >> USHIFT;
      const int32_t sv2 = (sc.ry2[gx + 1] - sc.ry2[gx] + (1 << (USHIFT - 1))) >> USHIFT;
      int32_t u2 = sc.rx2[gx] + su2 * off, v2 = sc.ry2[gx] + sv2 * off;
      for (; x <= segEnd; ++x, u += su, v += sv, u2 += su2, v2 += sv2) {
        const uint32_t t1 = ((((uint32_t)v >> 8) & 255) << 8) | (((uint32_t)u >> 8) & 255);
        const uint32_t t2 = ((((uint32_t)v2 >> 8) & 255) << 8) | (((uint32_t)u2 >> 8) & 255);
        const uint32_t tv1 = TP[t1], tv2 = TP[t2];
        const uint16_t c1 = palCur[tv1 & 255], c2 = palCur[tv2 & 255];
        const uint32_t A = ((uint32_t)c2 | ((uint32_t)c2 << 16)) & 0x07E0F81Fu, B = ((uint32_t)c1 | ((uint32_t)c1 << 16)) & 0x07E0F81Fu;
        const uint32_t M = ((A * q + B * (16 - q)) >> 4) & 0x07E0F81Fu;
        const uint16_t c = (uint16_t)(M | (M >> 16));
        const uint32_t s2 = (q >= 8 ? tv2 : tv1) >> 8;
        row[x] = __builtin_bswap16(s2 ? add565(c, shineLUT[s2]) : c);
      }
    }
  }

  // ---- 黒い鏡の球 と 光の粒
  for (int e = entStart[k]; e < entStart[k + 1]; ++e) {
    const int id = ent[e];
    if (id >= 10000) {
      // ブラックホール: まん中はまっ黒。まわりは光が吸い込まれて、穴に近いほど暗くなる
      const Hole& h = hole[id - 10000];
      const float R = h.cR, iR = 1.0f / R, outer = 1.6f, out2 = outer * outer, kd = 12.0f / (out2 - 1.0f);
      for (int y = max(y0, h.y0); y <= min(y1 - 1, h.y1); ++y) {
        const float dy = (y + 0.5f - h.y) * iR;
        const float w2 = out2 - dy * dy;
        if (w2 <= 0) continue;
        const float hw = sqrtf(w2) * R;
        const int xs = max(max((int)floorf(h.x - hw), bx0), (int)spanS[y]), xe = min(min((int)ceilf(h.x + hw), bx1), (int)spanE[y]);
        uint16_t* row = buf + (y - y0) * bw - bx0;
        float dx = (xs + 0.5f - h.x) * iR;
        for (int x = xs; x <= xe; ++x, dx += iR) {
          const float rr = dx * dx + dy * dy;
          if (rr >= out2) continue;
          if (rr < 1.0f) { row[x] = 0; continue; }             // まっ黒
          row[x] = darken(row[x], 4 + (int)((rr - 1.0f) * kd));  // まわり: 平方根を使わず、距離の2乗で暗さを決める
        }
      }
    } else {
      // 光の粒: まるい光(まん中ほど明るい)+ 大きい粒には十字の光の筋
      const SDraw& s = sd[id];
      const int L = s.L, R = (L * 3 + 2) / 4, A = L >= 6 ? L * 2 : 0;
      for (int y = max(y0, s.y - max(R, A)); y <= min(y1 - 1, s.y + max(R, A)); ++y) {
        const int xs = max((int)spanS[y], bx0), xe = min((int)spanE[y], bx1);
        uint16_t* row = buf + (y - y0) * bw - bx0;
        const int dy = y - s.y;
        if (abs(dy) <= R) {
          const int hw = (int)sqrtf((float)(R * R - dy * dy) + 0.5f);
          for (int x = max(xs, s.x - hw); x <= min(xe, s.x + hw); ++x) {
            const int dx = x - s.x;
            int q = starQ[(dx * dx + dy * dy) * 64 / ((R + 1) * (R + 1))];   // 中心からの離れ具合(表で)
            row[x] = lighten(row[x], s.col[q]);
          }
        }
        if (A > 0) {                                            // 十字の筋(細く、外ほど暗く)
          if (dy == 0) {
            for (int x = max(xs, s.x - A); x <= min(xe, s.x + A); ++x) {
              const int d = abs(x - s.x);
              if (d <= R) continue;
              int q = 2 + d * 6 / (A + 1); if (q > 7) q = 7;
              row[x] = lighten(row[x], s.col[q]);
            }
          } else if (abs(dy) > R && s.x >= xs && s.x <= xe) {
            int q = 2 + abs(dy) * 6 / (A + 1); if (q > 7) q = 7;
            row[s.x] = lighten(row[s.x], s.col[q]);
          }
        }
      }
    }
  }
}

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
      if (fbMode) {
        // 置き場に書いて、すぐにこの帯の丸の中の幅だけを画面へ送る(pushImage の終わりで自動で送られる)
        const uint32_t t0 = micros();
        M5.Display.pushImage(XOFF + bandX0[k], YOFF + y0, bandW[k], h, slotBuf[k % NSLOT]);
        send += micros() - t0;
      } else {
        M5.Display.pushImageDMA(XOFF + bandX0[k], YOFF + y0, bandW[k], h, slotBuf[k % NSLOT]);
      }
    }
  }
  if (!fbMode) { M5.Display.waitDMA(); }
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
  Sc = (S - 1) * 0.5f;
  XOFF = (W - S) / 2;
  YOFF = ((H - S) / 2 / BAND) * BAND;
  NB = (S + BAND - 1) / BAND;
  CW = S / CSTEP + 3;
  VR = 0.5f * min(W, H);
  Serial.println("NullGoodbye v13");
  Serial.printf("screen %dx%d  area %d\n", W, H, S);

  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setBrightness(brightSteps[brightIdx]);

  for (int i = 0; i < NSLOT; ++i) slotBuf[i] = (uint16_t*)heap_caps_malloc(S * BAND * 2, MALLOC_CAP_DMA);
  tex = (uint8_t*)ps_malloc(TEXN * TEXN);
  tex2 = (uint8_t*)ps_malloc(TEXN * TEXN);                       // 作るときだけ使う(できたら texP にまとめる)
  texP = (uint16_t*)allocFast(TEXN * TEXN * 2);                  // 1ピクセルごとに読むので、内蔵メモリに
  gU = (float*)calloc(CW * CW, sizeof(float)); gV = (float*)calloc(CW * CW, sizeof(float));
  gU2 = (float*)calloc(CW * CW, sizeof(float)); gV2 = (float*)calloc(CW * CW, sizeof(float));
  cAx = (float*)ps_calloc(CW * CW, sizeof(float)); cAy = (float*)ps_calloc(CW * CW, sizeof(float));
  cBx = (float*)ps_calloc(CW * CW, sizeof(float)); cBy = (float*)ps_calloc(CW * CW, sizeof(float));
  cTx = (float*)ps_calloc(CW * CW, sizeof(float)); cTy = (float*)ps_calloc(CW * CW, sizeof(float));
  flowVX = (float*)ps_calloc(CW * CW, sizeof(float)); flowVY = (float*)ps_calloc(CW * CW, sizeof(float));
  for (int c = 0; c < 2; ++c) {
    scr[c].rx = (int32_t*)malloc(CW * 4); scr[c].ry = (int32_t*)malloc(CW * 4);
    scr[c].rx2 = (int32_t*)malloc(CW * 4); scr[c].ry2 = (int32_t*)malloc(CW * 4);
  }
  spanS  = (int16_t*)malloc(S * sizeof(int16_t)); spanE = (int16_t*)malloc(S * sizeof(int16_t));
  bandX0 = (int16_t*)malloc(NB * sizeof(int16_t)); bandW = (int16_t*)malloc(NB * sizeof(int16_t));
  bandVis = (uint8_t*)malloc(NB);
  bool ok = tex && tex2 && texP && flowVX && flowVY && gU && gV && gU2 && gV2 && cAx && cAy && cBx && cBy && cTx && cTy && spanS && spanE && bandX0 && bandW && bandVis && NB <= MAXNB;
  for (int i = 0; i < NSLOT; ++i) ok = ok && slotBuf[i];
  for (int c = 0; c < 2; ++c) ok = ok && scr[c].rx && scr[c].ry && scr[c].rx2 && scr[c].ry2;
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

  for (int i = 0; i < 1024; ++i) sinT[i] = sinf(6.2831853f * i / 1024);
  const uint32_t t0 = millis();
  buildTexture();
  free(tex); free(tex2); tex = nullptr; tex2 = nullptr;      // 1つの絵(texP)にまとめたので、もう要らない
  for (int i = 0; i < 64; ++i) { int q = (int)(sqrtf(i / 64.0f) * 8.0f); starQ[i] = (uint8_t)(q > 7 ? 7 : q); }
  Serial.printf("pattern built in %u ms\n", (unsigned)(millis() - t0));
  buildPalette();
  initHoles();
  for (int gy = 0; gy < CW; ++gy)                              // 流れる模様の地図(はじめは、そのままの位置)
    for (int gx = 0; gx < CW; ++gx) {
      cAx[gy * CW + gx] = cBx[gy * CW + gx] = (float)(gx * CSTEP);
      cAy[gy * CW + gx] = cBy[gy * CW + gx] = (float)(gy * CSTEP);
    }
  cycT = 0;
  for (int i = 0; i < MAXSTAR; ++i) star[i].life = 0;
  for (int b = 0; b < NBAR / 2; ++b) spec[b] = 0;

  // マイク(スピーカーと同じ部品を使う機種があるので、先にスピーカーを止める)
  M5.Speaker.end();
  micOK = M5.Mic.begin();
  Serial.printf("mic: %s\n", micOK ? "ok" : "not available (score only)");

  Input in0 = {false, false, -1000, -1000, 0.016f};
  updateFrame(in0);

  // StopWatch の M5GFX は、画面の手前に PSRAM の置き場を持ち、描き終わるたびに自動で画面へ送る
  { auto* pnl = M5.Display.getPanel(); fbMode = pnl && pnl->getAutoDisplay(); }
  SPLIT = fbMode ? 3 : 2;
  Serial.printf("display: %s\n", fbMode ? "framebuffer (send each band)" : "direct DMA");
  semGo   = xSemaphoreCreateBinary();
  semDone = xSemaphoreCreateBinary();
  xTaskCreatePinnedToCore(workerTask, "band", 8192, nullptr, 1, nullptr, 0);
}

static void nextTheme() {
  theme = (theme + 1) % NTHEME;
  buildPalette();
  Serial.printf("theme: %s\n", THEMES[theme].name);
}
static void restartScore() {
  scoreT = 0;
  twist += 1.5f;
  Serial.println("score: restart");
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
    else if (M5.BtnA.wasClicked()) restartScore();
    if (M5.BtnB.wasPressed()) {
      brightIdx = (brightIdx + 1) % sizeof(brightSteps);
      M5.Display.setBrightness(brightSteps[brightIdx]);
    }
  } else {
    if (M5.BtnPWR.wasClicked()) {
      brightIdx = (brightIdx + 1) % sizeof(brightSteps);
      M5.Display.setBrightness(brightSteps[brightIdx]);
    }
    if (pressed && holeAt((float)(t.x - XOFF), (float)(t.y - YOFF)) >= 0) lastTapMs = 0;   // 穴の上のタップは、穴の操作
    else if (pressed) {
      if (now - lastTapMs < 350) {                             // ダブルタップ: 上半分なら振り付けを最初から、下半分なら色
        if (t.y < H / 2) restartScore(); else nextTheme();
        lastTapMs = 0;
      } else lastTapMs = now;
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
    Serial.printf("fps: %.1f  core0 %.1f ms  core1 %.1f ms  send %.1f ms  %s\n", fps, busyUs[0] / 1000.0f, busyUs[1] / 1000.0f, sendUs / 1000.0f, micLive ? "mic" : "score");
    frames = 0; fpsTimer = millis();
  }
}
