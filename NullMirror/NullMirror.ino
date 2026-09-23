// NullMirror HD  -  null² の中、砕けた鏡の部屋が、ヌルヌル流れつづける(高精細版)
//   M5Stack StopWatch 用(丸い 466x466 画面)。CoreS3 系でも真ん中に正方形で動きます。
//
// 画面の中:
//   ・割れた鏡のかけら : 1枚1枚が別々の向きを向いていて、映る景色がゆっくりすべっていく
//   ・光のすじ         : 引きのばされたかけらに、LEDの光がすじになって流れる
//   ・くしゃくしゃの箔 : 細かくしわの寄ったかけらが、キラキラ光る
//   ・ひび             : 細く鋭い線。液体みたいにうねって、ところどころ瞬く
//   ・鏡の立方体       : 空を映しながら、ゆっくり回って漂う
//
// 触ると:
//   ・タップ       : そこを中心に、ひびが走ってガラスが割れる(かけらがずれて傾く)
//   ・押し続ける   : ひび割れがどんどん外へ広がっていく
//   ・なぞる       : なぞった跡に沿って、次々と割れていく
//   ・しばらく待つ : 割れたかけらは、ゆっくりもとどおりにつながる
//   ・立方体をつかむ : 指についてぬるっと動く。動かした向きに転がるように回る
//                      (立方体と後ろの割れたガラスは別々。立方体を触っても後ろは流れません)
//   ・立方体を投げる : すーっと滑って、画面のふちでやわらかく跳ね返る
//   ・立方体をタップ : くるっと回る
//
// 操作:
//   StopWatch … ボタンA: 色あい(null / 夕焼け / 銀 / ネオン)  ボタンB: 明るさ
//   CoreS3系  … ダブルタップ: 色あい                          電源ボタン短押し: 明るさ
//
// 必要なもの:
//   ボードマネージャー M5Stack >= 3.3.7 / ボード: M5StopWatch(CoreS3 なら M5CoreS3)
//   ライブラリ M5Unified >= 0.2.15, M5GFX >= 0.2.21
//
// しくみ:
//   1ピクセルずつ「液体のゆがみ越しに、どのかけらの、どこが映るか」を計算して描きます。
//   画面を8行ずつの帯に分け、2つのコアが交互に帯を描き、できた順に画面へ送ります。
//   速くするために: 1ピクセルの計算は整数だけ・となりのピクセルとの差分で進める、
//   景色は速い内蔵メモリへ、画面へは丸の内側だけ送る、描画ループは -O3 と IRAM で。
//   シリアルモニタに fps と、各コアが1フレームに使った時間(ms)が出ます。
//   QUALITY = 0(おまかせ)なら、それでも重いときは自動で軽いモードに切り替わります。

#include <M5Unified.h>
#include <math.h>

// 描画のループは速さ優先でコンパイルする(Arduino の標準は「小ささ優先」の -Os)
#pragma GCC optimize("O3")
#ifndef IRAM_ATTR
#define IRAM_ATTR
#endif

// ---- 画質 ------------------------------------------------------------------
// 0: おまかせ(高精細で始めて、最初の3秒が SMOOTH_FPS 未満なら自動で軽いほうへ)
// 1: いつも高精細(1ピクセルずつ)   2: いつも軽い(2x2ピクセルずつ・前の版と同じ細かさ)
static constexpr int   QUALITY    = 0;
static constexpr float SMOOTH_FPS = 24.0f;   // これ未満なら「ヌルヌルじゃない」と判断して軽くする

// ---- 調整用 ----------------------------------------------------------------
static constexpr float FLOW       = 0.030f;  // 何もしないときの、ゆったりしたうねりの大きさ
static constexpr float FLOW_SPEED = 0.06f;   // うねりの速さ
static constexpr float DRIFT      = 1.0f;    // かけらに映る景色がすべる速さ
// 割れかた
static constexpr float BREAK_R    = 0.16f;   // タップした瞬間に割れる範囲(画面に対する割合)
static constexpr float BREAK_GROW = 0.20f;   // 押し続けたとき、ひび割れが広がる速さ(画面/秒)
static constexpr float CRACK_SPD  = 1.6f;    // ひびが走っていく速さ(画面/秒)
static constexpr float PIECE_MOVE = 16.0f;   // 割れたかけらの、映りのずれ
static constexpr float PIECE_TILT = 0.32f;   // 割れたかけらの、傾き(ラジアン)
static constexpr float HEAL_TIME  = 7.0f;    // 割れたままでいる時間(秒)
static constexpr float FLASH_TIME = 0.16f;   // 割れた瞬間、ひびが光っている時間(秒)
static constexpr float SHOCK      = 9.0f;    // 割れた瞬間の、まわりへの衝撃(ピクセル)
static constexpr float SETTLE     = 1.8f;    // 衝撃がおさまる速さ
static constexpr float RIPPLE     = 1.4f;    // うねりが映り込みをゆがめる強さ
static constexpr float CRINKLE    = 18.0f;   // くしゃくしゃの箔のキラキラ具合
static constexpr float CRACK_W    = 2.2f;    // ひびの太さ(ピクセル)
static constexpr float CUBE_SIZE  = 0.23f;   // 立方体の大きさ(画面に対する割合)
static constexpr float CUBE_GRIP  = 55.0f;   // つかんだ立方体が指についてくる強さ(小さいほど、ぬるっと遅れる)
static constexpr float CUBE_GLIDE = 0.9f;    // 投げた立方体の止まりやすさ(小さいほど長く滑る)
static constexpr float CUBE_ROLL  = 0.6f;    // 動かしたときに転がる回転の強さ(1.0 = 本当に転がる速さ)
static constexpr float CUBE_WAKE  = 0.0f;    // 立方体が鏡の部屋をかき混ぜる強さ(0 = 別々に動く)
// ---------------------------------------------------------------------------

static constexpr int ENVW  = 512;            // 景色テクスチャ(横)
static constexpr int ENVH  = 256;            // 景色テクスチャ(縦)
static constexpr int NOISE = 64;
static constexpr int NGRP  = 26;             // ふだん見えている、かたまりの数
static constexpr int NSUB  = 120;            // 割れる単位(かけら)の数
static constexpr int MAXSH = NGRP;
static constexpr int CSTEP = 16;             // うねりを計算する間隔(ピクセル)
static constexpr int CRKN  = 128;            // しわのテーブルの大きさ
static constexpr int BAND  = 8;              // 1つの帯の行数
static constexpr int NSLOT = 4;              // 帯のバッファの数
static constexpr float UBIAS = ENVW * 64.0f; // 景色の座標をいつも正にするためのかさ上げ
static constexpr float VBIAS = ENVH * 64.0f;

// 画面に合わせて起動時に決まる値
static int   W, H, S, XOFF, YOFF, CW, NB;
static int   PIXEL = 1;                      // いまの描き方(1 か 2)
static float SCP;                            // StopWatch(466) を 1.0 とした大きさ

static uint16_t* env;
static uint16_t* slotBuf[NSLOT];
static uint8_t*  pmap;                       // 0〜119: かけらの中 / +0x80: 切れ目 / 0xFF: もとからのひび
static int8_t*   crk;                        // しわ(du, dv)
static int16_t*  spanS;                      // 行ごとの描く範囲(丸い画面)
static int16_t*  spanE;
static float*    noiseT;                     // なめらかなノイズ
static float*    noiseH;                     // こまかいノイズ(しわ)
static float*    dfX;  static float* dfY;    // 指や立方体でかき混ぜた分のずれ
static float*    tmpX; static float* tmpY;
static float*    wX;   static float* wY;     // このフレームのゆがみ
struct Scratch { int32_t* rx; int32_t* ry; };   // この行のゆがみ(1/256ピクセル単位)
static Scratch   scr[2];

// 色
static uint16_t crackCol, edgeCol, glitCol[3], blackCol;

struct Shard {
  float cx, cy;
  int   type;                    // 0: 鏡  1: 光のすじ  2: くしゃくしゃ  3: 暗い
  float u0, v0, au, av, w1, w2, p1, p2;
  float sx, sy, th0, tha, w3;
};
struct ShardP { float ou, ov, a11, a12, a21, a22; bool crinkle; };   // フレームごと
static Shard  shards[MAXSH];
static ShardP shp[MAXSH];
struct ShardQ { int32_t ou, ov, a11, a12, a21, a22, crinkle; };    // 整数版(描画用)
static constexpr int CRACK_ID = 127;         // 「もとからのひび」用の番号
static ShardQ shq[128];                      // かけらごとの映り方(割れるとずれる)

// 割れる単位のかけら
struct Sub {
  float cx, cy;            // 中心
  uint8_t grp;             // どのかたまりの一部か
  uint8_t state;           // 0: 無傷  1: ひびが向かっている  2: 割れた
  float t0, healAt, flashT;
  float du, dv, dth, kick; // 割れたあとの、映りのずれ・傾き・勢い
};
static Sub     subs[NSUB];
static uint8_t brokenFlag[128];
static uint16_t crackColOf[128];
static float   breakR = 0;                   // いま割れが広がっている範囲
static int    NS = 0;

// 立方体の面(フレームごと)
struct Face { float e[4][3]; float s0, sx, sy, t0, tx, ty, bu, bv; int bx0, bx1, by0, by1; };
static Face  faces[3];
static int   nFaces = 0;
static int   cubeMinX, cubeMaxX, cubeMinY, cubeMaxY;

// フレームごとの共有値
static float    kR, crkS;
static int32_t  kRq, crkQ;
static int16_t* bandX0;                      // 帯ごとの、送る範囲(丸の内側)
static int16_t* bandW;
static uint32_t busyUs[2];                   // 各コアが描くのに使った時間
static bool     envFast = false;
static int      crkOX, crkOY;
static uint32_t twinkle;

// コアの受け渡し
static SemaphoreHandle_t semGo, semDone;
static volatile int readyOdd = -1, doneUpTo = -1;

// 入力と状態
static float simT = 0;
static float fPX = 0, fPY = 0, holdT = 0;
static bool  fWas = false;

// 立方体
static float Rm[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1};
static float omX = 0.20f, omY = 0.33f, omZ = 0.07f;
static float cubeX, cubeY, cubeVX = 0, cubeVY = 0, lift = 0;
static bool  grabbed = false;
static float grabDX = 0, grabDY = 0, grabT = 0, grabMove = 0;

static int theme = 0;
static const uint8_t brightSteps[] = {60, 120, 190, 255};
static int brightIdx = 2;

// ---------------------------------------------------------------- 道具
struct Col { float r, g, b; };
static inline float clamp01(float v) { return v < 0 ? 0 : (v > 1 ? 1 : v); }
static inline float smooth3(float t) { t = clamp01(t); return t * t * (3 - 2 * t); }
static inline float fracf(float x) { return x - floorf(x); }
static inline Col lerpC(Col a, Col b, float t) {
  return {a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t};
}
static inline uint16_t toPix(Col c) {
  uint16_t col = M5.Display.color565(clamp01(c.r) * 255, clamp01(c.g) * 255, clamp01(c.b) * 255);
  return __builtin_bswap16(col);               // 色がおかしい場合はこの bswap を外す
}
static inline float frand() { return (esp_random() & 0xFFFF) / 65535.0f; }
static inline uint32_t hash2(uint32_t x, uint32_t y) {
  uint32_t h = x * 374761393u + y * 668265263u;
  h = (h ^ (h >> 13)) * 1274126177u;
  return h ^ (h >> 16);
}
static Col hsv(float h, float s, float v) {
  h = fracf(h) * 6.0f; int i = (int)h; float f = h - i;
  float p = v * (1 - s), q = v * (1 - s * f), t = v * (1 - s * (1 - f));
  switch (i % 6) {
    case 0: return {v, t, p}; case 1: return {q, v, p}; case 2: return {p, v, t};
    case 3: return {p, q, v}; case 4: return {t, p, v}; default: return {v, p, q};
  }
}
static void toHsv(Col c, float& h, float& s, float& v) {
  float mx = fmaxf(c.r, fmaxf(c.g, c.b)), mn = fminf(c.r, fminf(c.g, c.b));
  v = mx; float d = mx - mn; s = mx > 1e-5f ? d / mx : 0;
  if (d < 1e-5f) { h = 0; return; }
  if (mx == c.r)      h = (c.g - c.b) / d / 6.0f;
  else if (mx == c.g) h = ((c.b - c.r) / d + 2) / 6.0f;
  else                h = ((c.r - c.g) / d + 4) / 6.0f;
  h = fracf(h);
}

static float sampleNoise(const float* t, float x, float y) {
  float fx0 = floorf(x), fy0 = floorf(y);
  float fx = x - fx0, fy = y - fy0;
  int x0 = (int)fx0 & (NOISE - 1), y0 = (int)fy0 & (NOISE - 1);
  int x1 = (x0 + 1) & (NOISE - 1),  y1 = (y0 + 1) & (NOISE - 1);
  float a = t[y0 * NOISE + x0], b = t[y0 * NOISE + x1];
  float c = t[y1 * NOISE + x0], d = t[y1 * NOISE + x1];
  float top = a + (b - a) * fx, bot = c + (d - c) * fx;
  return top + (bot - top) * fy;
}

static void makeNoise(float* t, int passes) {
  float* tmp = (float*)malloc(NOISE * NOISE * sizeof(float));
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
  for (int i = 0; i < NOISE * NOISE; ++i) t[i] /= mx;
  free(tmp);
}

// ---------------------------------------------------------------- 景色(鏡に映るもの)
// 横に4つの場所がつながっている:  LEDの壁 → 空 → 暗がりと光のパネル → 虹色の膜
// (u, v) は 256x128 の大きさで考え、テクスチャはその2倍の細かさで作る
static inline float wrapDist(float a, float b, float period) {
  float d = fabsf(a - b); return fminf(d, period - d);
}
static inline float bump(float u, float c, float w) {
  float d = wrapDist(u, c, 256.0f) / w;
  return d >= 1 ? 0 : (1 - d * d) * (1 - d * d);
}

static Col sceneColor(int i, int j) {
  float u = (i + 0.5f) * 0.5f, v = (j + 0.5f) * 0.5f;
  int iu = (int)u, iv = (int)v;
  float wL = bump(u, 32, 52), wS = bump(u, 100, 70), wV = bump(u, 165, 55), wI = bump(u, 224, 50);
  float wsum = wL + wS + wV + wI + 1e-4f;

  // LEDの壁: 青〜水色の光のすじ。ところどころに赤・ピンクの細い線と、白く光る線
  float lane = u * 0.40f + 1.5f * sinf(v * 0.07f);
  int   li = (int)floorf(lane * 0.5f);
  uint32_t hl = hash2((uint32_t)(li & 127), 77);
  float lf = fracf(lane * 0.5f);
  float thin = expf(-(lf - 0.5f) * (lf - 0.5f) / 0.05f);
  Col led = hsv(0.53f + 0.09f * sinf(li * 0.7f), 0.75f, 0.35f + 0.55f * thin);
  if ((hl & 15) == 0)      led = lerpC(led, {1.00f, 0.18f, 0.20f}, thin);
  else if ((hl & 15) == 1) led = lerpC(led, {0.95f, 0.25f, 0.85f}, thin);
  else if ((hl & 15) < 5)  led = lerpC(led, {0.95f, 0.98f, 1.00f}, thin);
  uint32_t hb = hash2(iu / 16, iv / 16);
  if ((hb & 15) == 0) led = (hb & 16) ? Col{0.95f, 0.14f, 0.10f} : Col{0.80f, 0.12f, 0.70f};

  // 空: 雲の浮かぶ青空と、白い光
  float sy = clamp01(v / 80.0f);
  Col sky = lerpC({0.12f, 0.42f, 0.90f}, {0.85f, 0.93f, 1.00f}, powf(sy, 1.4f));
  float n = sampleNoise(noiseT, u * 0.25f, v * 0.5f) + 0.25f * sampleNoise(noiseH, u * 0.5f, v * 1.0f);
  sky = lerpC(sky, {1, 1, 1}, smooth3((n - 0.05f) / 0.5f) * 0.9f);
  if (v > 82) sky = lerpC(sky, {0.05f, 0.25f, 0.55f}, clamp01((v - 82) / 30.0f));

  // 暗がり: ほとんど黒。ところどころに赤・青・紫の光のパネルと、小さな星のような点
  Col dark = {0.015f, 0.02f, 0.04f};
  uint32_t hv = hash2(iu / 12 + 101, iv / 10 + 7);
  if ((hv % 9) == 0) {
    static const Col P[] = {{0.95f,0.15f,0.12f},{0.15f,0.35f,1.0f},{0.65f,0.20f,0.95f},{0.10f,0.85f,0.95f}};
    dark = P[(hv >> 8) & 3];
    float fade = 0.55f + 0.45f * ((hv >> 12) & 255) / 255.0f;
    dark = {dark.r * fade, dark.g * fade, dark.b * fade};
  }
  if ((hash2(i, j + 999) & 1023) == 0) dark = {0.9f, 0.9f, 1.0f};

  // 虹色の膜: 暗い紺の上を、細い光の線が流れる(油膜の等高線のように)
  float ph = sampleNoise(noiseT, u * 0.25f + 13, v * 0.5f + 29) * 1.8f + v * 0.012f;
  float lc = fracf(ph * 9.0f);
  float line = expf(-(lc - 0.5f) * (lc - 0.5f) / 0.004f);
  Col iri = lerpC({0.02f, 0.06f, 0.14f}, {0.05f, 0.30f, 0.55f}, clamp01(0.5f + 0.5f * sinf(ph * 3.0f)));
  Col lcol = hsv(0.50f + 0.45f * sinf(ph * 2.3f), 0.45f, 1.0f);
  iri = lerpC(iri, lcol, line);

  return {(led.r * wL + sky.r * wS + dark.r * wV + iri.r * wI) / wsum,
          (led.g * wL + sky.g * wS + dark.g * wV + iri.g * wI) / wsum,
          (led.b * wL + sky.b * wS + dark.b * wV + iri.b * wI) / wsum};
}

struct Theme { float hueShift, sat, gamma, bright; Col crack, edge; };
static const Theme THEMES[] = {
  {0.00f, 1.00f, 1.00f, 1.00f, {0.01f, 0.01f, 0.02f}, {0.85f, 0.95f, 1.00f}},   // null
  {0.47f, 1.05f, 0.95f, 1.00f, {0.04f, 0.01f, 0.02f}, {1.00f, 0.85f, 0.65f}},   // 夕焼け
  {0.00f, 0.08f, 1.10f, 1.05f, {0.02f, 0.02f, 0.02f}, {1.00f, 1.00f, 1.00f}},   // 銀
  {0.20f, 1.25f, 1.35f, 1.10f, {0.00f, 0.00f, 0.01f}, {1.00f, 0.40f, 0.95f}},   // ネオン
};
static constexpr int NTHEME = sizeof(THEMES) / sizeof(THEMES[0]);

static void buildEnv(int th) {
  const Theme& T = THEMES[th];
  for (int j = 0; j < ENVH; ++j)
    for (int i = 0; i < ENVW; ++i) {
      Col c = sceneColor(i, j);
      float h, s, v; toHsv(c, h, s, v);
      h += T.hueShift; s = clamp01(s * T.sat); v = clamp01(powf(v, T.gamma) * T.bright);
      env[j * ENVW + i] = toPix(hsv(h, s, v));
    }
  crackCol = toPix(T.crack);
  edgeCol  = toPix(T.edge);
  blackCol = toPix({0, 0, 0});
  glitCol[0] = toPix({1, 1, 1});
  glitCol[1] = toPix(hsv(0.52f + T.hueShift, 0.6f * T.sat, 1));
  glitCol[2] = toPix(hsv(0.88f + T.hueShift, 0.55f * T.sat, 1));
}

// ---------------------------------------------------------------- 割れた鏡の準備
// かたまり(ふだん見えている大きなかけら)の中を、さらに細かいかけらに分けておく。
// ふだんは同じ映り方なので1枚に見えるが、割れるとそれぞれ別々にずれて、切れ目が現れる。
static void buildShards() {
  float c0 = (S - 1) * 0.5f, R0 = S * 0.5f;
  float fx = c0 + (frand() - 0.5f) * 0.4f * R0, fy = c0 + (frand() - 0.5f) * 0.4f * R0;
  NS = 0;
  while (NS < 15) {                                        // 全体にちらばるかたまり
    float a = frand() * 6.2832f, r = sqrtf(frand()) * R0 * 1.05f;
    shards[NS].cx = c0 + cosf(a) * r; shards[NS].cy = c0 + sinf(a) * r; ++NS;
  }
  while (NS < NGRP) {                                      // 割れの中心に集まる小さなかたまり
    float a = frand() * 6.2832f, r = fabsf(frand() + frand() - 1.0f) * 0.35f * R0;
    shards[NS].cx = fx + cosf(a) * r; shards[NS].cy = fy + sinf(a) * r; ++NS;
  }
  // 細かいかけら: まず、どのかたまりにも必ず1枚。残りは全体にばらまく
  for (int i = 0; i < NSUB; ++i) {
    Sub& sb = subs[i];
    if (i < NS) { sb.cx = shards[i].cx; sb.cy = shards[i].cy; sb.grp = (uint8_t)i; }
    else {
      float a = frand() * 6.2832f, r = sqrtf(frand()) * R0 * 1.08f;
      sb.cx = c0 + cosf(a) * r; sb.cy = c0 + sinf(a) * r;
      float best = 1e9f; int bg = 0;
      for (int g = 0; g < NS; ++g) {
        float dx = sb.cx - shards[g].cx, dy = sb.cy - shards[g].cy, d = dx * dx + dy * dy;
        if (d < best) { best = d; bg = g; }
      }
      sb.grp = (uint8_t)bg;
    }
    sb.state = 0; sb.t0 = 0; sb.healAt = 0; sb.flashT = -99;
    sb.du = sb.dv = sb.dth = sb.kick = 0;
  }
  float cw = CRACK_W * fmaxf(SCP, 0.6f);                   // もとからあるひびの太さ
  float iw = cw * 0.9f;                                    // 割れたときに出る切れ目(細め)
  for (int y = 0; y < S; ++y)
    for (int x = 0; x < S; ++x) {
      // 1) ふだん見えているひび = かたまりどうしの境目(もとのまま、なめらか)
      float g1 = 1e9f, g2 = 1e9f; int bg = 0;
      for (int k = 0; k < NS; ++k) {
        float dx = x - shards[k].cx, dy = y - shards[k].cy, d = dx * dx + dy * dy;
        if (d < g1) { g2 = g1; g1 = d; bg = k; } else if (d < g2) g2 = d;
      }
      bool groupCrack = (sqrtf(g2) - sqrtf(g1)) < cw;
      // 2) そのかたまりの中だけで、細かいかけらに分ける(割れるまで見えない切れ目)
      float d1 = 1e9f, d2 = 1e9f; int b1 = -1;
      for (int k = 0; k < NSUB; ++k) {
        if (subs[k].grp != bg) continue;
        float dx = x - subs[k].cx, dy = y - subs[k].cy, d = dx * dx + dy * dy;
        if (d < d1) { d2 = d1; d1 = d; b1 = k; } else if (d < d2) d2 = d;
      }
      uint8_t v;
      if (groupCrack)                              v = 0xFF;                       // もとからのひび
      else if ((sqrtf(d2) - sqrtf(d1)) < iw)       v = (uint8_t)(b1) | 0x80;       // 割れたら出る切れ目
      else                                         v = (uint8_t)(b1 < 0 ? 0 : b1);
      pmap[y * S + x] = v;
    }
  for (int k = 0; k < NS; ++k) {
    Shard& s = shards[k];
    float r = frand();
    s.type = r < 0.38f ? 0 : (r < 0.58f ? 1 : (r < 0.75f ? 2 : 3));
    s.v0 = 20 + frand() * 80;
    if (s.type == 1)      s.u0 = (frand() < 0.5f ? 32 : 224) + (frand() - 0.5f) * 30;
    else if (s.type == 3) s.u0 = 160 + (frand() - 0.5f) * 40;
    else {
      float q = frand();
      s.u0 = q < 0.5f ? 100 + (frand() - 0.5f) * 60 : (q < 0.8f ? 165 + (frand() - 0.5f) * 40 : frand() * 256);
    }
    s.au = (8 + frand() * 30) * DRIFT; s.av = (4 + frand() * 16) * DRIFT;
    s.w1 = 0.04f + frand() * 0.16f;    s.w2 = 0.03f + frand() * 0.12f;
    s.p1 = frand() * 6.2832f;          s.p2 = frand() * 6.2832f;
    s.th0 = frand() * 6.2832f; s.tha = 0.2f + frand() * 0.5f; s.w3 = 0.03f + frand() * 0.10f;
    // 映り方の拡大率(景色のテクセル / ピクセル)。画面が小さいときは同じ見え方になるよう大きく
    float z = 1.0f / SCP;
    if (s.type == 1)      { s.sx = (2.5f + frand() * 3.0f) * z; s.sy = (0.03f + frand() * 0.10f) * z; }
    else if (s.type == 3) { s.sx = s.sy = (0.4f + frand() * 0.4f) * z; }
    else                  { s.sx = (0.5f + frand() * 0.9f) * z; s.sy = s.sx * (0.7f + frand() * 0.6f); }
  }
}

static void buildCrinkle() {
  for (int j = 0; j < CRKN; ++j)
    for (int i = 0; i < CRKN; ++i) {
      float a = sampleNoise(noiseH, i * 0.5f, j * 0.5f), b = sampleNoise(noiseH, i * 0.5f + 23, j * 0.5f + 41);
      crk[(j * CRKN + i) * 2]     = (int8_t)(a * 127);
      crk[(j * CRKN + i) * 2 + 1] = (int8_t)(b * 127);
    }
}

// ---------------------------------------------------------------- 立方体
static void rotApply(float* M, float ax, float ay, float az) {
  float a = sqrtf(ax * ax + ay * ay + az * az);
  if (a < 1e-6f) return;
  float kx = ax / a, ky = ay / a, kz = az / a, c = cosf(a), s = sinf(a), t = 1 - c;
  float Q[9] = {t * kx * kx + c,      t * kx * ky - s * kz, t * kx * kz + s * ky,
                t * kx * ky + s * kz, t * ky * ky + c,      t * ky * kz - s * kx,
                t * kx * kz - s * ky, t * ky * kz + s * kx, t * kz * kz + c};
  float N[9];
  for (int r = 0; r < 3; ++r)
    for (int cc = 0; cc < 3; ++cc)
      N[r * 3 + cc] = Q[r * 3] * M[cc] + Q[r * 3 + 1] * M[3 + cc] + Q[r * 3 + 2] * M[6 + cc];
  for (int r = 0; r < 3; ++r) {                   // 少しずつずれるのを防ぐ
    float* v = N + r * 3;
    for (int q = 0; q < r; ++q) {
      float* w = N + q * 3; float d = v[0] * w[0] + v[1] * w[1] + v[2] * w[2];
      v[0] -= d * w[0]; v[1] -= d * w[1]; v[2] -= d * w[2];
    }
    float l = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    v[0] /= l; v[1] /= l; v[2] /= l;
  }
  memcpy(M, N, sizeof(N));
}

// 見えている面(最大3つ)を、ピクセルで判定できる形にしておく
static void prepareCube(float cs) {
  static const float V[8][3] = {{-1,-1,-1},{1,-1,-1},{1,1,-1},{-1,1,-1},{-1,-1,1},{1,-1,1},{1,1,1},{-1,1,1}};
  static const int F[6][4] = {{0,1,2,3},{5,4,7,6},{4,0,3,7},{1,5,6,2},{4,5,1,0},{3,2,6,7}};
  static const float FN[6][3] = {{0,0,-1},{0,0,1},{-1,0,0},{1,0,0},{0,-1,0},{0,1,0}};
  const float dist = 4.5f, f = cs * dist * 0.62f;
  float P[8][2];
  float mnx = 1e9f, mxx = -1e9f, mny = 1e9f, mxy = -1e9f;
  for (int k = 0; k < 8; ++k) {
    float x = Rm[0] * V[k][0] + Rm[1] * V[k][1] + Rm[2] * V[k][2];
    float y = Rm[3] * V[k][0] + Rm[4] * V[k][1] + Rm[5] * V[k][2];
    float z = Rm[6] * V[k][0] + Rm[7] * V[k][1] + Rm[8] * V[k][2] + dist;
    P[k][0] = cubeX + f * x / z; P[k][1] = cubeY + f * y / z;
    mnx = fminf(mnx, P[k][0]); mxx = fmaxf(mxx, P[k][0]);
    mny = fminf(mny, P[k][1]); mxy = fmaxf(mxy, P[k][1]);
  }
  cubeMinX = max(0, (int)mnx - 1); cubeMaxX = min(S - 1, (int)mxx + 1);
  cubeMinY = max(0, (int)mny - 1); cubeMaxY = min(S - 1, (int)mxy + 1);
  nFaces = 0;
  for (int fi = 0; fi < 6 && nFaces < 3; ++fi) {
    float nx = Rm[0] * FN[fi][0] + Rm[1] * FN[fi][1] + Rm[2] * FN[fi][2];
    float ny = Rm[3] * FN[fi][0] + Rm[4] * FN[fi][1] + Rm[5] * FN[fi][2];
    float nz = Rm[6] * FN[fi][0] + Rm[7] * FN[fi][1] + Rm[8] * FN[fi][2];
    float mx = 0, my = 0, mz = 0;
    for (int q = 0; q < 4; ++q) {
      const float* v = V[F[fi][q]];
      mx += Rm[0] * v[0] + Rm[1] * v[1] + Rm[2] * v[2];
      my += Rm[3] * v[0] + Rm[4] * v[1] + Rm[5] * v[2];
      mz += Rm[6] * v[0] + Rm[7] * v[1] + Rm[8] * v[2] + dist;
    }
    mx *= 0.25f; my *= 0.25f; mz *= 0.25f;
    if (nx * mx + ny * my + nz * mz >= 0) continue;       // 裏向き
    float l = sqrtf(mx * mx + my * my + mz * mz), vx = mx / l, vy = my / l, vz = mz / l;
    float dd = vx * nx + vy * ny + vz * nz;
    float rx = vx - 2 * dd * nx, ry = vy - 2 * dd * ny;
    const float* A = P[F[fi][0]]; const float* B = P[F[fi][1]];
    const float* Cc = P[F[fi][2]]; const float* D = P[F[fi][3]];
    float e1x = B[0] - A[0], e1y = B[1] - A[1], e2x = D[0] - A[0], e2y = D[1] - A[1];
    float det = e1x * e2y - e1y * e2x;
    if (fabsf(det) < 1e-3f) continue;
    Face& fc = faces[nFaces];
    // 面の中の位置 (s, t) = 画面座標の1次式
    fc.sx =  e2y / det; fc.sy = -e2x / det; fc.s0 = -(A[0] * fc.sx + A[1] * fc.sy);
    fc.tx = -e1y / det; fc.ty =  e1x / det; fc.t0 = -(A[0] * fc.tx + A[1] * fc.ty);
    fc.bu = UBIAS + (96.0f + rx * 70.0f) * 2.0f;          // 空のあたりを映す
    fc.bv = VBIAS + (50.0f + ry * 45.0f) * 2.0f;
    const float* Q4[4] = {A, B, Cc, D};
    float fx0 = fminf(fminf(A[0], B[0]), fminf(Cc[0], D[0])), fx1 = fmaxf(fmaxf(A[0], B[0]), fmaxf(Cc[0], D[0]));
    float fy0 = fminf(fminf(A[1], B[1]), fminf(Cc[1], D[1])), fy1 = fmaxf(fmaxf(A[1], B[1]), fmaxf(Cc[1], D[1]));
    fc.bx0 = max(0, (int)floorf(fx0)); fc.bx1 = min(S - 1, (int)ceilf(fx1));
    fc.by0 = max(0, (int)floorf(fy0)); fc.by1 = min(S - 1, (int)ceilf(fy1));
    float sgn = det > 0 ? -1.0f : 1.0f;
    for (int q = 0; q < 4; ++q) {                          // 辺からの距離(内側が正)
      const float* p0 = Q4[q]; const float* p1 = Q4[(q + 1) & 3];
      float ex = p1[0] - p0[0], ey = p1[1] - p0[1], el = sqrtf(ex * ex + ey * ey) + 1e-4f;
      fc.e[q][0] =  ey / el * sgn;
      fc.e[q][1] = -ex / el * sgn;
      fc.e[q][2] = -(p0[0] * fc.e[q][0] + p0[1] * fc.e[q][1]);
    }
    ++nFaces;
  }
}

// ---------------------------------------------------------------- 1フレームぶんの準備(描く前に)
struct Input { bool touching; bool tapped; float tx, ty; float dt; };

static void updateFrame(const Input& in) {
  float dt = in.dt > 0.05f ? 0.05f : in.dt;
  simT += dt;
  float cs = CUBE_SIZE * S * (1.0f + 0.08f * lift);
  float c0 = S * 0.5f, R0 = S * 0.5f;

  // ---- 指
  float fvx = 0, fvy = 0;
  if (in.touching) {
    if (fWas) { fvx = (in.tx - fPX) / fmaxf(dt, 0.001f); fvy = (in.ty - fPY) / fmaxf(dt, 0.001f); }
    holdT += dt;
  } else holdT = 0;
  fPX = in.tx; fPY = in.ty; fWas = in.touching;

  // ---- 立方体: つかむ・動かす・投げる
  if (in.tapped) {
    float dx = in.tx - cubeX, dy = in.ty - cubeY;
    if (dx * dx + dy * dy < cs * cs * 0.9f) {
      grabbed = true; grabDX = cubeX - in.tx; grabDY = cubeY - in.ty; grabT = 0; grabMove = 0;
    }
  }
  if (grabbed && !in.touching) {                           // はなした
    grabbed = false;
    if (grabT < 0.25f && grabMove < 0.03f * S) {           // さっとタップ → くるっと回る
      omX += (frand() - 0.5f) * 5.0f; omY += (frand() - 0.5f) * 5.0f; omZ += (frand() - 0.5f) * 2.0f;
    }
  }
  float ax = 0, ay = 0;
  if (grabbed) {
    grabT += dt; grabMove += sqrtf(fvx * fvx + fvy * fvy) * dt;
    float tx = in.tx + grabDX, ty = in.ty + grabDY;
    ax = CUBE_GRIP * (tx - cubeX) - 2.0f * sqrtf(CUBE_GRIP) * cubeVX;   // ぬるっと追いかける(ばね)
    ay = CUBE_GRIP * (ty - cubeY) - 2.0f * sqrtf(CUBE_GRIP) * cubeVY;
    lift += (1.0f - lift) * (1.0f - expf(-dt * 8));
  } else {
    float fr = expf(-dt * CUBE_GLIDE);                     // すーっと滑って止まる
    cubeVX *= fr; cubeVY *= fr;
    ax = 0.004f * S * sinf(simT * 0.21f);                  // ふわふわ漂う
    ay = 0.004f * S * sinf(simT * 0.17f + 1.0f);
    lift += (0.0f - lift) * (1.0f - expf(-dt * 4));
  }
  cubeVX += ax * dt; cubeVY += ay * dt;
  float vmax = 3.0f * S;
  float vm = sqrtf(cubeVX * cubeVX + cubeVY * cubeVY);
  if (vm > vmax) { cubeVX *= vmax / vm; cubeVY *= vmax / vm; }
  cubeX += cubeVX * dt; cubeY += cubeVY * dt;
  {                                                        // 画面のふちで、やわらかく跳ね返る
    float dx = cubeX - c0, dy = cubeY - c0, r = sqrtf(dx * dx + dy * dy) + 1e-4f;
    float lim = R0 - cs * 0.95f;
    if (r > lim) {
      float nx = dx / r, ny = dy / r;
      cubeX = c0 + nx * lim; cubeY = c0 + ny * lim;
      float vn = cubeVX * nx + cubeVY * ny;
      if (vn > 0) { cubeVX -= 1.55f * vn * nx; cubeVY -= 1.55f * vn * ny; }
    }
  }
  // 回転: いつものゆっくり回転 + 動いた向きに転がる
  float tgX = 0.20f - cubeVY / cs * CUBE_ROLL;
  float tgY = 0.33f + cubeVX / cs * CUBE_ROLL;
  float k = 1.0f - expf(-dt * (grabbed ? 4.0f : 0.8f));
  omX += (tgX - omX) * k; omY += (tgY - omY) * k; omZ += (0.07f - omZ) * k;
  rotApply(Rm, omX * dt, omY * dt, omZ * dt);
  prepareCube(cs);

  // ---- かき混ぜ(粗い格子): 指と、動いている立方体
  float relax = expf(-dt * SETTLE);
  float crad = cs * 1.3f, cinv2 = 1.0f / (2 * crad * crad);
  float maxD = 0.20f * S;

  // ---- 指で触ると、そこから割れる(立方体をつかんでいるときは割らない)
  bool fingerBreaks = in.touching && !grabbed;
  if (fingerBreaks) {
    if (in.tapped) breakR = BREAK_R * S;                   // タップ: その場でパッと割れる
    else breakR += BREAK_GROW * S * dt;                    // 押したまま: ひびがじわじわ広がる
    float speed = CRACK_SPD * S;
    for (int i = 0; i < NSUB; ++i) {
      Sub& sb = subs[i];
      if (sb.state != 0) continue;
      float dx = sb.cx - in.tx, dy = sb.cy - in.ty;
      float d = sqrtf(dx * dx + dy * dy);
      if (d < breakR) { sb.state = 1; sb.t0 = simT + d / speed; }   // ひびが走って、届いたら割れる
    }
  } else breakR = 0;

  // ---- かけらの割れ・もどり
  int nShock = 0; float shockX[12], shockY[12];
  for (int i = 0; i < NSUB; ++i) {
    Sub& sb = subs[i];
    if (sb.state == 1 && simT >= sb.t0) {                  // ひびが届いた: 割れる
      sb.state = 2;
      float a = frand() * 6.2832f, m = PIECE_MOVE * (0.4f + 0.8f * frand());
      sb.du = cosf(a) * m; sb.dv = sinf(a) * m * 0.7f;
      sb.dth = (frand() - 0.5f) * 2.0f * PIECE_TILT;
      sb.kick = 1.0f;
      sb.flashT = simT;
      sb.healAt = simT + HEAL_TIME * (0.7f + 0.6f * frand());
      if (nShock < 12) { shockX[nShock] = sb.cx; shockY[nShock] = sb.cy; ++nShock; }
    }
    if (sb.state == 2) {
      sb.kick *= expf(-dt * 4.0f);                         // 割れた瞬間の勢いは、すぐ落ち着く
      if (simT > sb.healAt) {                              // しばらくすると、もとどおりに
        float f = expf(-dt * 1.2f);
        sb.du *= f; sb.dv *= f; sb.dth *= f;
        if (fabsf(sb.du) + fabsf(sb.dv) < 0.3f && fabsf(sb.dth) < 0.01f) {
          sb.state = 0; sb.du = sb.dv = sb.dth = 0;
        }
      }
    }
    brokenFlag[i] = (sb.state == 2);
    float age = simT - sb.flashT;                          // 割れた瞬間、ひびが光る
    crackColOf[i] = (age < FLASH_TIME) ? glitCol[i % 3] : crackCol;
  }

  brokenFlag[CRACK_ID] = 1;                                // もとからのひびは、いつも見える
  crackColOf[CRACK_ID] = crackCol;
  shq[CRACK_ID] = shq[0];                                  // (色は上で決まる。ここは計算の続き用)

  // ---- 割れた衝撃だけ、まわりをふるわせる(すぐおさまる)
  float cvx = cubeVX, cvy = cubeVY;
  bool cubeStirs = (CUBE_WAKE > 0.0f) && (cvx * cvx + cvy * cvy) > 25.0f;
  float srad = 0.16f * S, sinv2 = 1.0f / (2 * srad * srad);
  for (int gy = 0; gy < CW; ++gy)
    for (int gx = 0; gx < CW; ++gx) {
      int q = gy * CW + gx;
      float x = gx * CSTEP, y = gy * CSTEP;
      float dx = dfX[q] * relax, dy = dfY[q] * relax;
      for (int n = 0; n < nShock; ++n) {
        float rx = x - shockX[n], ry = y - shockY[n], r2 = rx * rx + ry * ry;
        float g = expf(-r2 * sinv2);
        if (g > 0.02f) {
          float r = sqrtf(r2) + 1e-3f;
          dx += rx / r * SHOCK * g; dy += ry / r * SHOCK * g;
        }
      }
      if (cubeStirs) {                                     // 立方体の通ったあと(ふだんは 0)
        float rx = x - cubeX, ry = y - cubeY, r2 = rx * rx + ry * ry;
        float g = expf(-r2 * cinv2);
        if (g > 0.01f) { dx += cvx * dt * CUBE_WAKE * g; dy += cvy * dt * CUBE_WAKE * g; }
      }
      float m2 = dx * dx + dy * dy;
      if (m2 > maxD * maxD * 0.25f) {
        float sft = maxD / sqrtf(maxD * maxD + m2 * 0.75f);
        dx *= sft; dy *= sft;
      }
      tmpX[q] = dx; tmpY[q] = dy;
    }
  for (int gy = 0; gy < CW; ++gy)
    for (int gx = 0; gx < CW; ++gx) {
      int q = gy * CW + gx;
      int l = gx > 0 ? q - 1 : q, r = gx < CW - 1 ? q + 1 : q;
      int u = gy > 0 ? q - CW : q, d = gy < CW - 1 ? q + CW : q;
      dfX[q] = tmpX[q] * 0.6f + (tmpX[l] + tmpX[r] + tmpX[u] + tmpX[d]) * 0.1f;
      dfY[q] = tmpY[q] * 0.6f + (tmpY[l] + tmpY[r] + tmpY[u] + tmpY[d]) * 0.1f;
    }

  // ---- このフレームのゆがみ = ゆったりしたうねり − かき混ぜた分
  float amp = FLOW * S, sc = 57.6f / S * (CSTEP / 16.0f);
  float tt = simT * FLOW_SPEED * 10.0f;
  for (int gy = 0; gy < CW; ++gy)
    for (int gx = 0; gx < CW; ++gx) {
      int q = gy * CW + gx;
      float nx = sampleNoise(noiseT, gx * sc + tt, gy * sc) + 0.5f * sampleNoise(noiseT, gx * sc * 2 + 31, gy * sc * 2 - tt);
      float ny = sampleNoise(noiseT, gx * sc - 17, gy * sc + tt) + 0.5f * sampleNoise(noiseT, gx * sc * 2 + tt, gy * sc * 2 + 11);
      wX[q] = amp * nx - dfX[q];
      wY[q] = amp * ny - dfY[q];
    }

  // ---- かけらごとの映り方(ゆっくりすべる・回る)
  for (int q = 0; q < NS; ++q) {                            // かたまりごとの映り方
    const Shard& s = shards[q];
    ShardP& p = shp[q];
    float th = s.th0 + s.tha * sinf(simT * s.w3 + s.p1);
    float c = cosf(th), sn = sinf(th);
    p.a11 =  c * s.sx; p.a12 = sn * s.sx;
    p.a21 = -sn * s.sy; p.a22 = c * s.sy;
    float ou = UBIAS + (s.u0 + s.au * sinf(simT * s.w1 + s.p1)) * 2.0f;
    float ov = VBIAS + (s.v0 + s.av * sinf(simT * s.w2 + s.p2)) * 2.0f;
    p.ou = ou - p.a11 * s.cx - p.a12 * s.cy;               // ピクセル座標をそのまま掛ければよいように
    p.ov = ov - p.a21 * s.cx - p.a22 * s.cy;
    p.crinkle = (s.type == 2);
  }
  kR = RIPPLE / SCP;
  crkS = CRINKLE / 127.0f / SCP;
  // 整数版: 位置は 1/16 ピクセル、係数は 1/4096、景色の座標は 1/256 テクセル
  kRq  = (int32_t)lrintf(kR * 4096.0f);
  crkQ = (int32_t)lrintf(crkS * 256.0f);
  for (int i = 0; i < NSUB; ++i) {
    const Sub& sb = subs[i];
    const ShardP& p = shp[sb.grp];
    ShardQ& z = shq[i];
    z.crinkle = p.crinkle;
    if (sb.state != 2) {                                   // 無傷: かたまりと同じ映り方(1枚に見える)
      z.ou  = (int32_t)lrintf(p.ou * 256.0f);   z.ov  = (int32_t)lrintf(p.ov * 256.0f);
      z.a11 = (int32_t)lrintf(p.a11 * 4096.0f); z.a12 = (int32_t)lrintf(p.a12 * 4096.0f);
      z.a21 = (int32_t)lrintf(p.a21 * 4096.0f); z.a22 = (int32_t)lrintf(p.a22 * 4096.0f);
    } else {                                               // 割れた: その場で傾いて、少しずれる
      float k = 1.0f + 0.7f * sb.kick;                     // 割れた直後だけ、少し大きく動く
      float th = sb.dth * k, c = cosf(th), sn = sinf(th);
      float a11 = c * p.a11 - sn * p.a21, a12 = c * p.a12 - sn * p.a22;
      float a21 = sn * p.a11 + c * p.a21,  a22 = sn * p.a12 + c * p.a22;
      float ucen = p.ou + p.a11 * sb.cx + p.a12 * sb.cy;   // かけらの中心が映していた場所
      float vcen = p.ov + p.a21 * sb.cx + p.a22 * sb.cy;
      ucen += sb.du * k; vcen += sb.dv * k;                // そこから、ずらす
      z.ou  = (int32_t)lrintf((ucen - a11 * sb.cx - a12 * sb.cy) * 256.0f);
      z.ov  = (int32_t)lrintf((vcen - a21 * sb.cx - a22 * sb.cy) * 256.0f);
      z.a11 = (int32_t)lrintf(a11 * 4096.0f); z.a12 = (int32_t)lrintf(a12 * 4096.0f);
      z.a21 = (int32_t)lrintf(a21 * 4096.0f); z.a22 = (int32_t)lrintf(a22 * 4096.0f);
    }
  }
  crkOX = (int)(simT * 7.0f); crkOY = (int)(simT * 4.0f);
  twinkle = (uint32_t)(simT * 7.0f);
}

// ---------------------------------------------------------------- 描画(帯ごと)
// 立方体: この行にかかる面だけ、面の範囲だけを、差分で進めながら上書きする
template <int P>
static inline __attribute__((always_inline)) void cubeRowPass(uint16_t* row, int bx0, int bx1, int y, int xs, int xe) {
  const float fy = (float)y, edgeW = 1.3f * fmaxf(SCP, 0.7f);
  for (int f = 0; f < nFaces; ++f) {
    const Face& fc = faces[f];
    if (y < fc.by0 || y > fc.by1) continue;
    int a = max(fc.bx0, xs), b = min(fc.bx1, xe);
    if (P == 2) a &= ~1;
    if (a < xs) a += 2;
    if (a > b) continue;
    const float fx = (float)a;
    float d0 = fc.e[0][0] * fx + fc.e[0][1] * fy + fc.e[0][2];
    float d1 = fc.e[1][0] * fx + fc.e[1][1] * fy + fc.e[1][2];
    float d2 = fc.e[2][0] * fx + fc.e[2][1] * fy + fc.e[2][2];
    float d3 = fc.e[3][0] * fx + fc.e[3][1] * fy + fc.e[3][2];
    float s  = fc.s0 + fc.sx * fx + fc.sy * fy, t = fc.t0 + fc.tx * fx + fc.ty * fy;
    const float i0 = fc.e[0][0] * P, i1 = fc.e[1][0] * P, i2 = fc.e[2][0] * P, i3 = fc.e[3][0] * P;
    const float is = fc.sx * P, it = fc.tx * P;
    for (int x = a; x <= b; x += P, d0 += i0, d1 += i1, d2 += i2, d3 += i3, s += is, t += it) {
      if (d0 < 0 || d1 < 0 || d2 < 0 || d3 < 0) continue;
      uint16_t col;
      // (fminf は ESP32 では関数呼び出しになって遅いので、ただの比較で)
      if (d0 < edgeW || d1 < edgeW || d2 < edgeW || d3 < edgeW) col = edgeCol;   // 光るふち
      else {
        int u = (int)(fc.bu + (s - 0.5f) * 92.0f), v = (int)(fc.bv + (t - 0.5f) * 76.0f);
        col = env[(v & (ENVH - 1)) * ENVW + (u & (ENVW - 1))];
      }
      row[x - bx0] = col;
      if (P == 2 && x + 1 <= bx1) row[x + 1 - bx0] = col;
    }
  }
}

// ひびの色(めったに通らないので、別の関数にして速い道をじゃましないように)
static uint16_t __attribute__((noinline)) IRAM_ATTR crackPixel(int qi, int id) {
  uint32_t h = hash2((uint32_t)qi, twinkle);
  return (h & 63) == 0 ? glitCol[(h >> 6) % 3] : crackColOf[id];   // 割れた直後は光る
}

template <int P>   // P = 1: 高精細、2: 軽い(それぞれ専用に最適化される)
static inline __attribute__((always_inline)) void renderBandT(int k, uint16_t* buf, Scratch& sc) {
  const int y0 = k * BAND, y1 = min(S, y0 + BAND);
  const int bx0 = bandX0[k], bw = bandW[k], bx1 = bx0 + bw - 1;
  const int32_t maxq = (S - 1) << 4;
  const uint16_t* const envp = env;
  const uint8_t* const pm = pmap;
  for (int y = y0; y < y1; y += P) {
    uint16_t* row = buf + (y - y0) * bw;                     // row[x - bx0]
    int xs = max((int)spanS[y], bx0), xe = min((int)spanE[y], bx1);
    if (P == 2) xs &= ~1;
    for (int x = bx0; x < xs; ++x) row[x - bx0] = blackCol;
    for (int x = max(xe + 1, bx0); x <= bx1; ++x) row[x - bx0] = blackCol;
    if (xs <= xe) {
      // この行のゆがみ(粗い格子を縦に補間。1/256ピクセル単位の整数)
      int gy = y / CSTEP; float fy = (y - gy * CSTEP) * (1.0f / CSTEP);
      const float *ax = wX + gy * CW, *ay = wY + gy * CW;
      for (int gx = 0; gx < CW - 1; ++gx) {
        sc.rx[gx] = (int32_t)((ax[gx] + (ax[gx + CW] - ax[gx]) * fy) * 256.0f);
        sc.ry[gx] = (int32_t)((ay[gx] + (ay[gx + CW] - ay[gx]) * fy) * 256.0f);
      }
      const int32_t yq = y << 4;
      // 同じかけらの中では、映る場所は一定の歩幅で進む。
      // だから歩幅を足すだけにして、かけらが変わったときだけ計算し直す(ここがいちばん軽くなる)
      int x = xs;
      while (x <= xe) {
        const int gx = x / CSTEP;
        const int segEnd = min(xe, gx * CSTEP + CSTEP - 1);
        const int32_t stepx = (sc.rx[gx + 1] - sc.rx[gx] + 128) >> 8;   // 1ピクセルあたりのゆがみ
        const int32_t stepy = (sc.ry[gx + 1] - sc.ry[gx] + 128) >> 8;
        const int off = x - gx * CSTEP;
        int32_t pxq = (x << 4) + (sc.rx[gx] >> 4) + stepx * off;        // 見にいく場所(1/16ピクセル)
        int32_t pyq = yq + (sc.ry[gx] >> 4) + stepy * off;
        const int32_t dPx = (P << 4) + stepx * P, dPy = stepy * P;
        int curId = -1;
        int32_t u = 0, v = 0, du = 0, dv = 0;
        bool crinkle = false;
        for (; x <= segEnd; x += P, pxq += dPx, pyq += dPy) {
          const int32_t cxq = max((int32_t)0, min(maxq, pxq));          // 画面の外は見ない
          const int32_t cyq = max((int32_t)0, min(maxq, pyq));
          const int ix = cxq >> 4, iy = cyq >> 4;
          const int qi = iy * S + ix;
          const uint8_t m = pm[qi];
          const int id = m & 0x7F;
          if (__builtin_expect(id != curId, 0)) {                       // かけらが変わった
            const ShardQ& p = shq[id];
            u = p.ou + ((p.a11 * pxq + p.a12 * pyq + kRq * (pxq - (x << 4))) >> 8);
            v = p.ov + ((p.a21 * pxq + p.a22 * pyq + kRq * (pyq - yq)) >> 8);
            du = (p.a11 * dPx + p.a12 * dPy + kRq * (dPx - (P << 4))) >> 8;
            dv = (p.a21 * dPx + p.a22 * dPy + kRq * dPy) >> 8;
            curId = id; crinkle = p.crinkle;
          } else { u += du; v += dv; }                                  // ふだんは足すだけ
          uint16_t col;
          // もとからのひび、または「割れた」かけらの切れ目
          if (__builtin_expect((m & 0x80) && brokenFlag[id], 0)) {      // ひび
            col = crackPixel(qi, id);
          } else if (__builtin_expect(crinkle, 0)) {                    // くしゃくしゃの箔
            const int8_t* c = crk + ((((iy + crkOY) & (CRKN - 1)) * CRKN) + ((ix + crkOX) & (CRKN - 1))) * 2;
            int32_t uu = u + c[0] * crkQ, vv = v + c[1] * crkQ;
            col = envp[((vv >> 8) & (ENVH - 1)) * ENVW + ((uu >> 8) & (ENVW - 1))];
          } else {
            col = envp[((v >> 8) & (ENVH - 1)) * ENVW + ((u >> 8) & (ENVW - 1))];
          }
          row[x - bx0] = col;
          if (P == 2 && x + 1 <= bx1) row[x + 1 - bx0] = col;
        }
      }
      if (y >= cubeMinY && y <= cubeMaxY) cubeRowPass<P>(row, bx0, bx1, y, xs, xe);
    }
    if (P == 2 && y + 1 < y1) memcpy(row + bw, row, bw * sizeof(uint16_t));
  }
}

// 速い内蔵メモリ(IRAM)に置く、2つの描き方
static void __attribute__((noinline)) IRAM_ATTR renderBandHD(int k, uint16_t* buf, Scratch& sc)    { renderBandT<1>(k, buf, sc); }
static void __attribute__((noinline)) IRAM_ATTR renderBandLight(int k, uint16_t* buf, Scratch& sc) { renderBandT<2>(k, buf, sc); }
static inline void renderBand(int k, uint16_t* buf, Scratch& sc) {
  if (PIXEL == 1) renderBandHD(k, buf, sc); else renderBandLight(k, buf, sc);
}

// コア0: 奇数番目の帯を描く
static void workerTask(void*) {
  for (;;) {
    xSemaphoreTake(semGo, portMAX_DELAY);
    uint32_t busy = 0;
    for (int k = 1; k < NB; k += 2) {
      while (__atomic_load_n(&doneUpTo, __ATOMIC_ACQUIRE) < k - NSLOT) { }   // その帯のバッファが空くまで
      uint32_t t0 = micros();
      renderBand(k, slotBuf[k % NSLOT], scr[0]);
      busy += micros() - t0;
      __atomic_store_n(&readyOdd, k, __ATOMIC_RELEASE);
    }
    busyUs[0] = busy;
    xSemaphoreGive(semDone);
  }
}

// コア1: 偶数番目の帯を描き、すべての帯を順番に画面へ送る(丸の内側だけ)
static void renderFrame() {
  __atomic_store_n(&readyOdd, -1, __ATOMIC_RELEASE);
  __atomic_store_n(&doneUpTo, -1, __ATOMIC_RELEASE);
  xSemaphoreGive(semGo);
  uint32_t busy = 0;
  M5.Display.startWrite();
  for (int k = 0; k < NB; ++k) {
    if ((k & 1) == 0) {
      uint32_t t0 = micros();
      renderBand(k, slotBuf[k % NSLOT], scr[1]);
      busy += micros() - t0;
    } else {
      while (__atomic_load_n(&readyOdd, __ATOMIC_ACQUIRE) < k) { }
    }
    M5.Display.waitDMA();                                  // ひとつ前の帯を送り終えた
    __atomic_store_n(&doneUpTo, k - 1, __ATOMIC_RELEASE);
    int y0 = k * BAND, h = min(BAND, S - y0);
    M5.Display.pushImageDMA(XOFF + bandX0[k], YOFF + y0, bandW[k], h, slotBuf[k % NSLOT]);
  }
  M5.Display.waitDMA();
  __atomic_store_n(&doneUpTo, NB, __ATOMIC_RELEASE);
  M5.Display.endWrite();
  xSemaphoreTake(semDone, portMAX_DELAY);
  busyUs[1] = busy;
}

// ---------------------------------------------------------------- setup / loop
static void fail(const char* msg) {
  M5.Display.setTextDatum(middle_center);
  M5.Display.drawString(msg, M5.Display.width() / 2, M5.Display.height() / 2);
  Serial.println(msg);
  while (true) delay(1000);
}

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);
  Serial.begin(115200);

  if (M5.Display.width() < M5.Display.height()) M5.Display.setRotation(1);
  W = M5.Display.width(); H = M5.Display.height();
  S = min(W, H) & ~1;
  XOFF = (W - S) / 2; YOFF = (H - S) / 2;
  SCP = S / 466.0f;
  CW = S / CSTEP + 3;
  NB = (S + BAND - 1) / BAND;
  PIXEL = (QUALITY == 2) ? 2 : 1;
  Serial.printf("screen %dx%d  area %d  quality %d\n", W, H, S, QUALITY);

  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setBrightness(brightSteps[brightIdx]);

  noiseT = (float*)malloc(NOISE * NOISE * sizeof(float));
  noiseH = (float*)malloc(NOISE * NOISE * sizeof(float));
  for (int i = 0; i < NSLOT; ++i) slotBuf[i] = (uint16_t*)heap_caps_malloc(S * BAND * 2, MALLOC_CAP_DMA);
  pmap   = (uint8_t*)ps_malloc(S * S);
  crk    = (int8_t*)heap_caps_malloc(CRKN * CRKN * 2, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  if (!crk) crk = (int8_t*)ps_malloc(CRKN * CRKN * 2);
  spanS  = (int16_t*)malloc(S * sizeof(int16_t)); spanE = (int16_t*)malloc(S * sizeof(int16_t));
  int CN = CW * CW;
  dfX = (float*)calloc(CN, sizeof(float)); dfY = (float*)calloc(CN, sizeof(float));
  tmpX = (float*)calloc(CN, sizeof(float)); tmpY = (float*)calloc(CN, sizeof(float));
  wX = (float*)calloc(CN, sizeof(float)); wY = (float*)calloc(CN, sizeof(float));
  for (int c = 0; c < 2; ++c) {
    scr[c].rx = (int32_t*)heap_caps_malloc(CW * sizeof(int32_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    scr[c].ry = (int32_t*)heap_caps_malloc(CW * sizeof(int32_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  }
  bandX0 = (int16_t*)malloc(NB * sizeof(int16_t)); bandW = (int16_t*)malloc(NB * sizeof(int16_t));
  bool ok = noiseT && noiseH && bandX0 && bandW && pmap && crk && spanS && spanE && dfX && dfY && tmpX && tmpY && wX && wY &&
            scr[0].rx && scr[0].ry && scr[1].rx && scr[1].ry;
  for (int i = 0; i < NSLOT; ++i) ok = ok && slotBuf[i];
  if (!ok) fail("Memory error (PSRAM?)");

  // 丸い画面は円の内側だけ描く(外は黒)
  bool round = (W == H);
  float c0 = (S - 1) * 0.5f, rr = S * 0.5f + 1.0f;
  for (int y = 0; y < S; ++y) {
    if (!round) { spanS[y] = 0; spanE[y] = S - 1; continue; }
    float dy = y - c0, w = rr * rr - dy * dy;
    if (w <= 0) { spanS[y] = 1; spanE[y] = 0; continue; }
    float s = sqrtf(w);
    spanS[y] = max(0, (int)floorf(c0 - s)); spanE[y] = min(S - 1, (int)ceilf(c0 + s));
  }
  // 帯ごとに、画面へ送る横の範囲(その帯の丸の内側。偶数にそろえる)
  for (int k = 0; k < NB; ++k) {
    int a = S, b = -1;
    for (int y = k * BAND; y < min(S, k * BAND + BAND); ++y)
      if (spanS[y] <= spanE[y]) { a = min(a, (int)spanS[y]); b = max(b, (int)spanE[y]); }
    if (b < a) { a = 0; b = 1; }
    a &= ~1; if (((b - a + 1) & 1) && b < S - 1) ++b;
    bandX0[k] = a; bandW[k] = b - a + 1;
  }

  // 景色は、内蔵メモリに余裕があればそちらへ(速い)。なければ PSRAM へ
  size_t envBytes = ENVW * ENVH * 2;
  if (heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT) > envBytes + 48 * 1024)
    env = (uint16_t*)heap_caps_malloc(envBytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  envFast = (env != nullptr);
  if (!env) env = (uint16_t*)ps_malloc(envBytes);
  if (!env) fail("Memory error (env)");
  Serial.printf("scenery in %s memory\n", envFast ? "internal (fast)" : "PSRAM");

  makeNoise(noiseT, 4);
  makeNoise(noiseH, 1);
  buildShards();
  buildCrinkle();
  buildEnv(theme);

  cubeX = S * 0.62f; cubeY = S * 0.60f;
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

  if (round) {                                       // StopWatch: ボタンで操作
    if (M5.BtnA.wasPressed()) { theme = (theme + 1) % NTHEME; buildEnv(theme); }
    if (M5.BtnB.wasPressed()) {
      brightIdx = (brightIdx + 1) % sizeof(brightSteps);
      M5.Display.setBrightness(brightSteps[brightIdx]);
    }
  } else {                                           // CoreS3: ダブルタップと電源ボタン
    if (M5.BtnPWR.wasClicked()) {
      brightIdx = (brightIdx + 1) % sizeof(brightSteps);
      M5.Display.setBrightness(brightSteps[brightIdx]);
    }
    if (pressed) {
      if (now - lastTapMs < 350) { theme = (theme + 1) % NTHEME; buildEnv(theme); lastTapMs = 0; }
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

  updateFrame(in);          // 動きの計算(軽い)
  renderFrame();            // 2つのコアで1ピクセルずつ描く

  if (++frames, millis() - fpsTimer >= 3000) {
    float fps = frames * 1000.0f / (millis() - fpsTimer);
    Serial.printf("fps: %.1f  (%s)  core0 %.1f ms  core1 %.1f ms  scenery:%s\n", fps, PIXEL == 1 ? "HD" : "light",
                  busyUs[0] / 1000.0f, busyUs[1] / 1000.0f, envFast ? "fast" : "psram");
    static int checks = 0;
    if (QUALITY == 0 && PIXEL == 1 && ++checks == 1 && fps < SMOOTH_FPS) {   // 最初の3秒で判断
      PIXEL = 2;
      Serial.println("-> switched to light mode");
    }
    frames = 0; fpsTimer = millis();
  }
}
