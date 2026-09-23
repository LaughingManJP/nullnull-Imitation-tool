// NullPavilion  -  NullMenu 用モジュール(元のスケッチを名前空間に入れただけ)
#pragma once

namespace NullPavilion {

// ---- このモジュールが確保したものを全部おぼえておく ------------------------
// 別のスケッチに切り替えるとき、ここに積んだものを一括で返します。
static const int MEMN = 192;
static void*  memBlk[MEMN];
static int    memCnt = 0;
static bool   memOver = false;
static inline void* memKeep(void* p) {
  if (!p) return p;
  if (memCnt < MEMN) memBlk[memCnt++] = p; else memOver = true;
  return p;
}
static inline void* memAlloc(size_t n)             { return memKeep(malloc(n)); }
static inline void* memCalloc(size_t a, size_t b)  { return memKeep(calloc(a, b)); }
static inline void* memCaps(size_t n, int caps)    { return memKeep(heap_caps_malloc(n, caps)); }
static inline void* memPs(size_t n)                { return memKeep(ps_malloc(n)); }
static inline void* memPsCalloc(size_t a, size_t b){ return memKeep(ps_calloc(a, b)); }
static void memFreeAll() { for (int i = 0; i < memCnt; ++i) free(memBlk[i]); memCnt = 0; memOver = false; }

static TaskHandle_t modTask = nullptr;
static bool modRunning = false;
// NullPavilion  -  大阪・関西万博 null² をイメージした、手のひらの鏡のパビリオン
//   M5Stack StopWatch 用(丸い 466x466 画面)。CoreS3 系でも真ん中に正方形で動きます。
//
// 画面の中:
//   ・2m / 4m / 8m の立方体を積んだ建物。表面はぜんぶ鏡の膜で、空・雲・大屋根リングが映る
//   ・風   : 膜がゆっくり呼吸するようにふくらむ・へこむ
//   ・低音 : ときどきサブウーファーが鳴り、膜に定在波(ふしぎな模様のゆれ)が立つ
//   ・ロボットアーム : 大きな立方体の膜を、押したり引いたりねじったりする
//   ・ホーン : くぼんだラッパ形の奥で、丸いLEDの光が回る
//
// 触ると:
//   ・押す       : 指がロボットアームになって、膜がぬるっとへこむ(長く押すほど深く)
//   ・なぞる     : へこみが少し遅れて指についてくる。映った景色がぐにゃっと流れる
//   ・タップ     : そのパネルに「ドン」と低音が入り、膜がぶるぶる震える
//   ・離す       : 膜がぷるんぷるんと揺れもどる
//
// 操作:
//   StopWatch … ボタンA: 時間帯(昼 / 夕暮れ / 夜 / くもり)  ボタンB: 明るさ
//   CoreS3系  … ダブルタップ: 時間帯                        電源ボタン短押し: 明るさ
//
// 必要なもの:
//   ボードマネージャー M5Stack >= 3.3.7 / ボード: M5StopWatch(CoreS3 なら M5CoreS3)
//   ライブラリ M5Unified >= 0.2.15, M5GFX >= 0.2.21
//
// しくみ:
//   画面の半分の細かさの格子で膜の「高さ」を計算 → 傾きから鏡に映る方向を求め、
//   景色テクスチャ(横につながった空)を引いて 2倍に拡大して描画。
//   膜は端が枠に固定されているので、揺れは sin の形(モード)の足し合わせで作ります。
//   計算はコア0、描画はコア1で並行して行います。


// ---- 調整用 ----------------------------------------------------------------
static constexpr float REFLECT   = 30.0f;   // 映り込みのゆがみ(大きいほどぐにゃっと)
static constexpr float WIND      = 0.035f;  // 風で呼吸する大きさ
static constexpr float SOUND     = 0.030f;  // 低音の定在波の大きさ
static constexpr float ARM       = 0.12f;   // ロボットアームの押し引きの大きさ
static constexpr float PRESS     = 0.17f;   // 指で押したときの深さ
static constexpr float FOLLOW    = 5.0f;    // へこみが指についてくる速さ(小さいほど、ぬるっと遅れる)
static constexpr float WOBBLE    = 2.2f;    // 離したあとの揺れの止まりやすさ(小さいほど長くぷるぷる)
static constexpr float BASS      = 0.045f;  // タップしたときの低音の強さ
static constexpr float SKY_SPEED = 1.5f;    // 雲が流れる速さ
// ---------------------------------------------------------------------------

static constexpr int ENVW  = 256;           // 景色テクスチャの横幅(横はつながっている)
static constexpr int ENVH  = 192;           // 景色テクスチャの高さ
static constexpr int NOISE = 64;            // 雲・風用ノイズ
static constexpr int MAXP  = 16;            // パネル(立方体の面)の最大数
static constexpr int MAXM  = 4;             // 揺れのモードの最大次数
static constexpr int NLED  = 4;             // LEDの最大数

// 画面に合わせて起動時に決まる値
static int W, H, S, XOFF, YOFF, C, NG, GN, UNIT, ORG;

struct UV { uint16_t u, v; };               // 8.8 固定小数点

static uint16_t* env;          // 景色テクスチャ
static uint16_t* strip[2];     // 描画用バッファ (DMA)
static UV*       uvBuf[2];     // 反射方向 (ダブルバッファ)
static int       scrollBuf[2]; // そのフレームの雲の位置
static float*    hField;       // 膜の高さ
static float*    baseH;        // 膜のもともとの形(ホーン・鞍形)
static int8_t*   pid;          // どのパネルか(-1 = 空)
static uint8_t*  cls;          // 0: 映り込み  1: 枠  2〜: LED
static uint8_t*  ledR;         // LEDの中心からの距離(0〜31)
static float*    noiseT;
static float     expLut[257];
static uint16_t  ledPal[2][NLED][32];   // ダブルバッファ(sim が書く側と、描く側を分ける)
static uint16_t  seamCol, blackCol;

struct Panel {
  int   x0, y0, L;
  float u0, v0, gain, seed, saddle, bulge;
  bool  horn;
  int   led;
  float hitA, hitT, hitF;  int hitM, hitN;
  float* S[MAXM + 1];      // 固定された膜の揺れの形 sin(mπx)
};
static Panel panels[MAXP];
static int   NP = 0;
static int   armPanel = -1;

// 建物の形: 9x9 のます目(1ます = 2m の立方体)に積む  {x, y, 大きさ, ホーン}
struct Cube { int x, y, s, horn; };
static const Cube LAYOUT[] = {
  {1, 4, 4, 1}, {5, 4, 4, 0},                  // 8m
  {3, 2, 2, 1}, {5, 2, 2, 0},                  // 4m
  {7, 3, 1, 0}, {1, 3, 1, 0}, {2, 3, 1, 0},    // 2m
  {0, 6, 1, 0}, {0, 7, 1, 0}, {6, 1, 1, 0}, {4, 1, 1, 0},
};
static constexpr int GROUND_ROW = 8;         // この段から下は広場

struct Input { bool touching; bool tapped; float tx, ty; float dt; };
static Input simIn;
static SemaphoreHandle_t semGo, semDone;
static int front = 0;

static float simT = 0;

// 低音プログラム(サブウーファー)
static int   wM = 2, wN = 1;
static float wT = 0, wRise = 1.5f, wHold = 3.0f, wFall = 1.5f, wRest = 2.0f, wF = 2.5f, soundEnv = 0;

// 指のへこみ
static int   tp = -1;
static float dX = 0, dY = 0, dVX = 0, dVY = 0, dA = 0, dV = 0, holdT = 0;

static int theme = 0;
static const uint8_t brightSteps[] = {60, 120, 190, 255};
static int brightIdx = 2;

// ---------------------------------------------------------------- 景色
struct Col { float r, g, b; };
struct Theme {
  Col skyTop, skyHor, cloud, cloudShade, ring, ringLit, ground, groundFar, seam, sun;
  float stars, ringLights, led;
};
static const Theme THEMES[] = {
  // 昼
  {{0.20f,0.45f,0.85f},{0.78f,0.88f,0.98f},{1.00f,1.00f,1.00f},{0.72f,0.78f,0.88f},
   {0.55f,0.38f,0.22f},{0.70f,0.52f,0.32f},{0.78f,0.76f,0.72f},{0.52f,0.50f,0.48f},
   {0.10f,0.11f,0.13f},{1.0f,1.0f,0.95f}, 0.0f, 0.0f, 0.8f},
  // 夕暮れ
  {{0.22f,0.16f,0.42f},{1.00f,0.58f,0.30f},{1.00f,0.78f,0.62f},{0.55f,0.36f,0.46f},
   {0.30f,0.18f,0.14f},{0.62f,0.34f,0.20f},{0.50f,0.38f,0.36f},{0.25f,0.18f,0.20f},
   {0.08f,0.06f,0.08f},{1.0f,0.80f,0.45f}, 0.0f, 0.3f, 1.0f},
  // 夜
  {{0.01f,0.02f,0.07f},{0.08f,0.10f,0.24f},{0.16f,0.18f,0.30f},{0.06f,0.07f,0.14f},
   {0.10f,0.07f,0.06f},{1.00f,0.72f,0.35f},{0.10f,0.10f,0.14f},{0.04f,0.04f,0.07f},
   {0.03f,0.03f,0.04f},{0.9f,0.9f,1.0f}, 1.0f, 1.0f, 1.4f},
  // くもり(銀)
  {{0.62f,0.66f,0.72f},{0.90f,0.92f,0.95f},{0.97f,0.97f,0.98f},{0.70f,0.72f,0.76f},
   {0.42f,0.36f,0.30f},{0.52f,0.45f,0.38f},{0.70f,0.70f,0.70f},{0.45f,0.45f,0.47f},
   {0.14f,0.15f,0.16f},{1.0f,1.0f,1.0f}, 0.0f, 0.0f, 0.7f},
};
static constexpr int NTHEME = sizeof(THEMES) / sizeof(THEMES[0]);

static inline float clamp01(float v) { return v < 0 ? 0 : (v > 1 ? 1 : v); }
static inline float smooth3(float t) { t = clamp01(t); return t * t * (3 - 2 * t); }
static inline Col lerpC(Col a, Col b, float t) {
  return {a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t};
}
static inline uint16_t toPix(Col c) {
  uint16_t col = M5.Display.color565(clamp01(c.r) * 255, clamp01(c.g) * 255, clamp01(c.b) * 255);
  return __builtin_bswap16(col);               // 色がおかしい場合はこの bswap を外す
}
static inline float frand() { return (esp_random() & 0xFFFF) / 65535.0f; }
static inline uint32_t hash2(int x, int y) {
  uint32_t h = (uint32_t)x * 374761393u + (uint32_t)y * 668265263u;
  h = (h ^ (h >> 13)) * 1274126177u;
  return h ^ (h >> 16);
}

static float noiseAt(float x, float y) {
  float fx0 = floorf(x), fy0 = floorf(y);
  float fx = x - fx0, fy = y - fy0;
  int x0 = (int)fx0 & (NOISE - 1), y0 = (int)fy0 & (NOISE - 1);
  int x1 = (x0 + 1) & (NOISE - 1),  y1 = (y0 + 1) & (NOISE - 1);
  float a = noiseT[y0 * NOISE + x0], b = noiseT[y0 * NOISE + x1];
  float c = noiseT[y1 * NOISE + x0], d = noiseT[y1 * NOISE + x1];
  float top = a + (b - a) * fx, bot = c + (d - c) * fx;
  return top + (bot - top) * fy;
}

static void buildNoise() {
  float* tmp = (float*)malloc(NOISE * NOISE * sizeof(float));
  for (int i = 0; i < NOISE * NOISE; ++i) noiseT[i] = frand() * 2 - 1;
  for (int pass = 0; pass < 3; ++pass) {
    for (int y = 0; y < NOISE; ++y)
      for (int x = 0; x < NOISE; ++x) {
        float s = 0;
        for (int d = -2; d <= 2; ++d) s += noiseT[y * NOISE + ((x + d) & (NOISE - 1))];
        tmp[y * NOISE + x] = s / 5;
      }
    for (int y = 0; y < NOISE; ++y)
      for (int x = 0; x < NOISE; ++x) {
        float s = 0;
        for (int d = -2; d <= 2; ++d) s += tmp[((y + d) & (NOISE - 1)) * NOISE + x];
        noiseT[y * NOISE + x] = s / 5;
      }
  }
  float mx = 0;
  for (int i = 0; i < NOISE * NOISE; ++i) mx = fmaxf(mx, fabsf(noiseT[i]));
  for (int i = 0; i < NOISE * NOISE; ++i) noiseT[i] /= mx;
  free(tmp);
}

// 景色: 上から 空と雲 → 大屋根リング → 広場。横方向はつながっていて流せる
static constexpr int RING_TOP = 88, RING_BOT = 102;
static void buildEnv(int th) {
  const Theme& T = THEMES[th];
  for (int j = 0; j < ENVH; ++j) {
    for (int i = 0; i < ENVW; ++i) {
      Col c;
      if (j < RING_BOT) {                               // 空
        float s = clamp01(j / (float)RING_BOT);
        c = lerpC(T.skyTop, T.skyHor, powf(s, 1.6f));
        float n = noiseAt(i * 0.25f, j * 0.45f) + 0.5f * noiseAt(i * 0.5f + 17, j * 0.9f + 5);
        float cl = smooth3((n - 0.05f) / 0.55f) * (1.0f - 0.6f * s);
        Col cc = lerpC(T.cloud, T.cloudShade, clamp01(0.5f - 0.6f * n + 0.4f * s));
        c = lerpC(c, cc, cl);
        float dx = (float)((i - 180 + 384) % 256 - 128), dy = j - 34.0f;   // 太陽(月)
        float sun = expf(-(dx * dx + dy * dy) / 90.0f);
        c.r += T.sun.r * sun; c.g += T.sun.g * sun; c.b += T.sun.b * sun;
        if (T.stars > 0 && (hash2(i, j) & 1023) < 5 && cl < 0.3f) {   // 星
          float b = 0.5f + 0.5f * ((hash2(j, i) & 255) / 255.0f);
          c.r += b; c.g += b; c.b += b;
        }
      }
      if (j >= RING_TOP && j < RING_BOT) {              // 大屋根リング(木の梁と柱)
        bool beam  = (j < RING_TOP + 3);
        bool pillar = ((i & 15) < 2) || ((i & 15) == 8 && j > RING_TOP + 6);
        if (beam || pillar) {                           // 木組みの格子(後ろの空が少し透ける)
          Col wood = (j == RING_TOP) ? T.ringLit : T.ring;
          if (T.ringLights > 0 && beam && (i % 6) == 1) wood = lerpC(wood, T.ringLit, T.ringLights);
          c = lerpC(c, wood, beam ? 0.85f : 0.55f);
        }
      } else if (j >= RING_BOT) {                       // 広場
        float g = clamp01((j - RING_BOT) / 60.0f);
        c = lerpC(T.ground, T.groundFar, powf(g, 0.7f));
        if ((i & 15) == 0 || ((j - RING_BOT) % 10) == 0) c = lerpC(c, T.groundFar, 0.25f);  // 石畳の目地
        if (j < RING_BOT + 14) {                        // 人
          uint32_t hsh = hash2(i >> 1, (j - RING_BOT) / 5);
          if ((hsh % 23) == 0) c = lerpC(c, {0.12f, 0.12f, 0.15f}, 0.75f);
        }
      }
      env[j * ENVW + i] = toPix(c);
    }
  }
  seamCol  = toPix(T.seam);
  blackCol = toPix({0, 0, 0});
}

// ---------------------------------------------------------------- 建物の準備
static void buildPavilion() {
  for (int i = 0; i < GN; ++i) { pid[i] = -1; cls[i] = 0; ledR[i] = 0; baseH[i] = 0; hField[i] = 0; }
  NP = 0;
  int nled = 0;
  int bestL = 0;
  for (const Cube& cb : LAYOUT) {
    if (NP >= MAXP) break;
    Panel& P = panels[NP];
    P.x0 = ORG + cb.x * UNIT; P.y0 = ORG + cb.y * UNIT; P.L = cb.s * UNIT;
    P.seed = frand();
    P.gain = 0.6f + 0.6f * frand();
    P.saddle = (NP % 3 == 1) ? 0.10f : (NP % 3 == 2 ? -0.08f : 0.0f);
    P.bulge  = 0.20f + 0.10f * frand();          // 膜はクッションのようにふくらんでいる
    P.horn = cb.horn && nled < NLED;
    P.led = P.horn ? nled++ : -1;
    P.hitA = 0; P.hitT = 0; P.hitF = 5; P.hitM = 2; P.hitN = 2;
    float cxp = P.x0 + P.L * 0.5f, cyp = P.y0 + P.L * 0.5f;
    P.u0 = (128.0f + (cxp - NG * 0.5f) * 0.45f + (P.seed - 0.5f) * 40.0f) * 256.0f;
    P.v0 = (RING_TOP - 32.0f + (cyp - NG * 0.5f) * 0.08f + (frand() - 0.5f) * 10.0f) * 256.0f;
    for (int m = 1; m <= MAXM; ++m) {
      P.S[m] = (float*)memPs(P.L * sizeof(float));
      for (int k = 0; k < P.L; ++k) P.S[m][k] = sinf(m * 3.14159265f * (k + 0.5f) / P.L);
    }
    if (P.L > bestL && !P.horn) { bestL = P.L; armPanel = NP; }

    float c = (P.L - 1) * 0.5f;
    float rH = 0.46f * P.L, D = 0.20f * P.L, rLed = 0.11f * P.L;
    for (int ly = 0; ly < P.L; ++ly)
      for (int lx = 0; lx < P.L; ++lx) {
        int x = P.x0 + lx, y = P.y0 + ly;
        if (x < 0 || y < 0 || x >= NG || y >= NG) continue;
        int i = y * NG + x;
        pid[i] = NP;
        float dx = lx - c, dy = ly - c;
        float b = P.saddle * (dx * dx - dy * dy) / P.L;   // 鞍形
        b -= P.bulge * P.L * sinf(3.14159265f * (lx + 0.5f) / P.L) * sinf(3.14159265f * (ly + 0.5f) / P.L);
        if (P.horn) {                                      // ラッパ形のくぼみ
          float r = sqrtf(dx * dx + dy * dy);
          if (r < rH) { float q = 1.0f - r / rH; b -= D * q * q; }
          if (r < rLed) { cls[i] = 2 + P.led; ledR[i] = (uint8_t)fminf(31.0f, r / rLed * 31.0f); }
        }
        baseH[i] = b;
        if (lx == 0 || ly == 0 || lx == P.L - 1 || ly == P.L - 1) cls[i] = 1;   // 枠
      }
    ++NP;
  }

  // 空と広場は動かないので、映る場所を先に決めておく
  float groundY = ORG + GROUND_ROW * UNIT;
  for (int y = 0; y < NG; ++y)
    for (int x = 0; x < NG; ++x) {
      int i = y * NG + x;
      if (pid[i] >= 0) continue;
      float u = 128.0f + (x - NG * 0.5f) * 0.9f;
      float v = 10.0f + y * ((RING_BOT + 1 - 10.0f) / groundY);
      if (y > groundY) v = RING_BOT + 1 + (y - groundY) * 1.6f;
      v = fminf(v, ENVH - 1.0f);
      uvBuf[0][i] = uvBuf[1][i] = {(uint16_t)(u * 256), (uint16_t)(v * 256)};
    }
}

// ---------------------------------------------------------------- 膜の計算
// 枠に固定された膜に、なめらかなへこみ(ふくらみ)を足す。向きをもって伸ばせる(=ねじり)
static void addPanelDent(const Panel& P, float cx, float cy, float amp, float sa, float sp, float ang) {
  float ca = cosf(ang), sa_ = sinf(ang);
  float ia = 32.0f / (sa * sa), ip = 32.0f / (sp * sp);
  float rad = fmaxf(sa, sp) * 2.4f;
  int x0 = max(P.x0 + 1, (int)(cx - rad)), x1 = min(P.x0 + P.L - 2, (int)(cx + rad));
  int y0 = max(P.y0 + 1, (int)(cy - rad)), y1 = min(P.y0 + P.L - 2, (int)(cy + rad));
  for (int y = y0; y <= y1; ++y) {
    float dy = y - cy;
    float ey = P.S[1][y - P.y0];
    float* row = hField + y * NG;
    for (int x = x0; x <= x1; ++x) {
      float dx = x - cx;
      float a = dx * ca + dy * sa_, p = -dx * sa_ + dy * ca;
      float q = a * a * ia + p * p * ip;
      if (q < 255.0f) {
        int k = (int)q;
        float e = expLut[k] + (expLut[k + 1] - expLut[k]) * (q - k);
        row[x] += amp * e * ey * P.S[1][x - P.x0];     // 枠のところは動かない
      }
    }
  }
}

static inline float noise1(float t, float seed) { return noiseAt(t, 7.0f + seed * 40.0f); }

static void simulate(const Input& in, UV* out, int* scrollOut, uint16_t (*pal)[32]) {
  float dt = in.dt > 0.05f ? 0.05f : in.dt;
  simT += dt;
  *scrollOut = (int)(simT * SKY_SPEED) & (ENVW - 1);

  // ---- 低音プログラム: ふくらんで、しばらく鳴って、消えて、休む
  wT += dt;
  float wTotal = wRise + wHold + wFall;
  if (wT < wRise)               soundEnv = smooth3(wT / wRise);
  else if (wT < wRise + wHold)  soundEnv = 1.0f;
  else if (wT < wTotal)         soundEnv = 1.0f - smooth3((wT - wRise - wHold) / wFall);
  else                          soundEnv = 0.0f;
  if (wT > wTotal + wRest) {
    static const int MODES[][2] = {{2,1},{1,2},{2,2},{3,2},{2,3},{3,3},{4,3},{3,1}};
    int k = (int)(frand() * 8) & 7;
    wM = MODES[k][0]; wN = MODES[k][1];
    wF = 1.5f + frand() * 2.5f;
    wRise = 1.0f + frand(); wHold = 2.0f + frand() * 3.0f; wFall = 1.0f + frand();
    wRest = 1.5f + frand() * 4.0f;
    wT = 0;
  }
  float wave = soundEnv * sinf(6.2832f * wF * simT);

  // ---- 指 = ロボットアーム
  if (in.tapped) {
    int p = (in.tx >= 0 && in.ty >= 0 && in.tx < NG && in.ty < NG) ? pid[(int)in.ty * NG + (int)in.tx] : -1;
    if (p >= 0) {
      if (p != tp) { dA = 0; dV = 0; }
      tp = p; dX = in.tx; dY = in.ty; dVX = dVY = 0; holdT = 0;
      Panel& P = panels[p];                               // ドン: 低音が入って膜がぶるぶる
      static const int HM[][2] = {{2,2},{3,2},{2,3},{3,3},{1,3},{3,1}};
      int k = (int)(frand() * 6) % 6;
      P.hitM = HM[k][0]; P.hitN = HM[k][1];
      P.hitA = BASS * P.L; P.hitT = 0; P.hitF = 4.0f + frand() * 3.0f;
    }
  }
  bool pressing = in.touching && tp >= 0;
  if (pressing) {
    holdT += dt;
    const Panel& P = panels[tp];
    float lo = P.x0 + 0.12f * P.L, hi = P.x0 + 0.88f * P.L;   // 膜の外へは行けない(のびるだけ)
    float tx = fminf(fmaxf(in.tx, lo), hi);
    lo = P.y0 + 0.12f * P.L; hi = P.y0 + 0.88f * P.L;
    float ty = fminf(fmaxf(in.ty, lo), hi);
    float f = fminf(1.0f, dt * FOLLOW);                        // 少し遅れて、ぬるっとついてくる
    float nx = dX + (tx - dX) * f, ny = dY + (ty - dY) * f;
    dVX = (nx - dX) / fmaxf(dt, 0.001f); dVY = (ny - dY) / fmaxf(dt, 0.001f);
    dX = nx; dY = ny;
  } else {
    dVX *= expf(-dt * 4); dVY *= expf(-dt * 4);
  }
  if (tp >= 0) {                                               // へこみのばね(離すとぷるんぷるん)
    const Panel& P = panels[tp];
    float target = pressing ? -PRESS * P.L * (0.6f + 0.4f * smooth3(holdT / 1.5f)) : 0.0f;
    float damp = pressing ? 9.0f : WOBBLE;
    dV += (35.0f * (target - dA) - damp * dV) * dt;
    dA += dV * dt;
    if (!pressing && fabsf(dA) < 0.02f && fabsf(dV) < 0.05f) { dA = 0; dV = 0; tp = -1; }
  }

  // ---- パネルごとに膜の高さを計算
  for (int p = 0; p < NP; ++p) {
    Panel& P = panels[p];
    float aW = WIND * P.L * noise1(simT * 0.25f, P.seed) * (0.7f + 0.3f * noise1(simT * 0.07f, 0.5f));
    float aS = SOUND * P.L * P.gain * wave;
    float aH = 0;
    if (P.hitA > 0.001f) {
      P.hitT += dt;
      P.hitA *= expf(-dt * 1.3f);
      aH = P.hitA * sinf(6.2832f * P.hitF * P.hitT);
    } else P.hitA = 0;
    const float *Sx1 = P.S[1], *Sxm = P.S[wM], *Syn = P.S[wN], *Shm = P.S[P.hitM], *Shn = P.S[P.hitN];
    for (int ly = 0; ly < P.L; ++ly) {
      int y = P.y0 + ly;
      if (y < 0 || y >= NG) continue;
      float yw = Sx1[ly] * aW, ys = Syn[ly] * aS, yh = Shn[ly] * aH;
      int i = y * NG + P.x0;
      for (int lx = 0; lx < P.L; ++lx, ++i) {
        int x = P.x0 + lx;
        if (x < 0 || x >= NG) continue;
        hField[i] = baseH[i] + Sx1[lx] * yw + Sxm[lx] * ys + Shm[lx] * yh;
      }
    }
  }

  // ロボットアーム: 押す → 引く → ねじる
  if (armPanel >= 0) {
    const Panel& P = panels[armPanel];
    float cx = P.x0 + P.L * (0.5f + 0.24f * sinf(simT * 0.31f));
    float cy = P.y0 + P.L * (0.5f + 0.22f * sinf(simT * 0.23f + 1.0f));
    float amp = ARM * P.L * sinf(simT * 0.45f);
    addPanelDent(P, cx, cy, amp, 0.20f * P.L, 0.09f * P.L, simT * 0.6f);
  }

  // 指のへこみ(動かした向きに、ねばっと伸びる)
  if (tp >= 0 && fabsf(dA) > 0.01f) {
    const Panel& P = panels[tp];
    float v = sqrtf(dVX * dVX + dVY * dVY);
    float st = fminf(1.0f, v * 0.012f);
    float ang = v > 0.5f ? atan2f(dVY, dVX) : 0.0f;
    float sig = 0.13f * P.L;
    addPanelDent(P, dX, dY, dA, sig * (1.0f + st), sig / (1.0f + 0.4f * st), ang);
  }

  // ---- 傾き → 鏡に映る方向(膜の部分だけ。空と広場は準備済み)
  const float k = REFLECT * 256.0f;
  for (int p = 0; p < NP; ++p) {
    const Panel& P = panels[p];
    for (int ly = 1; ly < P.L - 1; ++ly) {
      int y = P.y0 + ly;
      if (y < 1 || y >= NG - 1) continue;
      for (int lx = 1; lx < P.L - 1; ++lx) {
        int x = P.x0 + lx;
        if (x < 1 || x >= NG - 1) continue;
        int i = y * NG + x;
        float gx = hField[i + 1] - hField[i - 1];
        float gy = hField[i + NG] - hField[i - NG];
        float u = P.u0 + gx * k;
        float v = P.v0 + gy * k;
        u = u < 0 ? 0 : (u > 65535.0f ? 65535.0f : u);
        v = v < 0 ? 0 : (v > (ENVH - 1) * 256.0f + 255.0f ? (ENVH - 1) * 256.0f + 255.0f : v);
        out[i] = {(uint16_t)u, (uint16_t)v};
      }
    }
  }

  // ---- LED: ホーンの奥で、光の輪が回る(低音が鳴ると速く・明るく)
  const Theme& T = THEMES[theme];
  for (int p = 0; p < NP; ++p) {
    const Panel& P = panels[p];
    if (P.led < 0) continue;
    float hue0 = P.seed + simT * 0.04f;
    float spd = 3.0f + 5.0f * soundEnv;
    for (int r = 0; r < 32; ++r) {
      float hue = hue0 + r * 0.004f;
      hue -= floorf(hue);
      float ringw = 0.5f + 0.5f * sinf(r * 0.28f - simT * spd);   // ゆっくり広がる光のうねり
      float edge = 1.0f - smooth3((r - 22) / 10.0f) * 0.55f;       // 縁は少し暗く
      float br = T.led * (0.70f + 0.30f * ringw) * edge;
      float h6 = hue * 6.0f; int hi = (int)h6; float f = h6 - hi;
      float q = 1 - f, t = f;
      Col c;
      switch (hi % 6) {
        case 0: c = {1, t, 0}; break;  case 1: c = {q, 1, 0}; break;
        case 2: c = {0, 1, t}; break;  case 3: c = {0, q, 1}; break;
        case 4: c = {t, 0, 1}; break;  default: c = {1, 0, q}; break;
      }
      c = lerpC({1, 1, 1}, c, 0.35f);                 // 白っぽく、やわらかい光
      pal[P.led][r] = toPix({c.r * br, c.g * br, c.b * br});
    }
  }
}

static void simTask(void*) {
  for (;;) {
    xSemaphoreTake(semGo, portMAX_DELAY);
    simulate(simIn, uvBuf[front ^ 1], &scrollBuf[front ^ 1], ledPal[front ^ 1]);
    xSemaphoreGive(semDone);
  }
}

// ---------------------------------------------------------------- 描画
static void renderFrame(const UV* uv, int scroll, const uint16_t (*pal)[32]) {
  M5.Display.startWrite();
  int s = 0;
  for (int gy = 0; gy < C; ++gy) {
    uint16_t* row0 = strip[s];
    uint16_t* row1 = row0 + S;
    int ia = gy * NG;
    for (int cx = 0; cx < C; ++cx, ++ia) {
      uint8_t ca = cls[ia];
      uint16_t p0, p1, p2, p3;
      if (ca == 0) {
        const UV a = uv[ia];
        if (cls[ia + 1] | cls[ia + NG] | cls[ia + NG + 1]) {       // 枠やLEDのとなりは補間しない
          p0 = p1 = p2 = p3 = env[(a.v >> 8) * ENVW + (((a.u >> 8) + scroll) & (ENVW - 1))];
        } else {
          const UV b = uv[ia + 1], c = uv[ia + NG], d = uv[ia + NG + 1];
          uint32_t u1 = (a.u + b.u) >> 1, v1 = (a.v + b.v) >> 1;
          uint32_t u2 = (a.u + c.u) >> 1, v2 = (a.v + c.v) >> 1;
          uint32_t u3 = (a.u + b.u + c.u + d.u) >> 2, v3 = (a.v + b.v + c.v + d.v) >> 2;
          p0 = env[(a.v >> 8) * ENVW + (((a.u >> 8) + scroll) & (ENVW - 1))];
          p1 = env[(v1 >> 8) * ENVW + (((u1 >> 8) + scroll) & (ENVW - 1))];
          p2 = env[(v2 >> 8) * ENVW + (((u2 >> 8) + scroll) & (ENVW - 1))];
          p3 = env[(v3 >> 8) * ENVW + (((u3 >> 8) + scroll) & (ENVW - 1))];
        }
      } else if (ca == 1) {
        p0 = p1 = p2 = p3 = seamCol;
      } else {
        p0 = p1 = p2 = p3 = pal[ca - 2][ledR[ia]];
      }
      row0[cx * 2] = p0; row0[cx * 2 + 1] = p1;
      row1[cx * 2] = p2; row1[cx * 2 + 1] = p3;
    }
    M5.Display.waitDMA();
    M5.Display.pushImageDMA(XOFF, YOFF + gy * 2, S, 2, strip[s]);
    s ^= 1;
  }
  M5.Display.waitDMA();
  M5.Display.endWrite();
}

// ---------------------------------------------------------------- setup / loop
bool begin() {

  if (M5.Display.width() < M5.Display.height()) M5.Display.setRotation(1);
  W = M5.Display.width(); H = M5.Display.height();
  S = min(W, H) & ~1;                       // 正方形の表示エリア(StopWatch は画面全体)
  XOFF = (W - S) / 2; YOFF = (H - S) / 2;
  C = S / 2; NG = C + 1; GN = NG * NG;
  UNIT = NG / 9; ORG = (NG - 9 * UNIT) / 2;
  Serial.printf("screen %dx%d  area %d  grid %d  unit %d\n", W, H, S, NG, UNIT);

  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setBrightness(brightSteps[brightIdx]);

  env      = (uint16_t*)memCaps(ENVW * ENVH * 2, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  if (!env) env = (uint16_t*)memPs(ENVW * ENVH * 2);   // 内部メモリが足りなければPSRAMへ
  noiseT   = (float*)memCaps(NOISE * NOISE * sizeof(float), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  strip[0] = (uint16_t*)memCaps(S * 2 * 2, MALLOC_CAP_DMA);
  strip[1] = (uint16_t*)memCaps(S * 2 * 2, MALLOC_CAP_DMA);
  uvBuf[0] = (UV*)memPs(GN * sizeof(UV));
  uvBuf[1] = (UV*)memPs(GN * sizeof(UV));
  hField   = (float*)memPs(GN * sizeof(float));
  baseH    = (float*)memPs(GN * sizeof(float));
  pid      = (int8_t*)memPs(GN);
  cls      = (uint8_t*)memCaps(GN, MALLOC_CAP_8BIT);
  ledR     = (uint8_t*)memPs(GN);
  if (!env || !noiseT || !strip[0] || !strip[1] || !uvBuf[0] || !uvBuf[1] ||
      !hField || !baseH || !pid || !cls || !ledR) {
    memFreeAll(); return false;
  }
  for (int i = 0; i < 257; ++i) expLut[i] = expf(-i / 32.0f);
  for (int i = 0; i < GN; ++i) uvBuf[0][i] = uvBuf[1][i] = {128 * 256, (RING_TOP - 4) * 256};

  buildNoise();
  buildPavilion();
  buildEnv(theme);

  simIn = {false, false, -1, -1, 0.016f};
  simulate(simIn, uvBuf[front], &scrollBuf[front], ledPal[front]);

  semGo   = xSemaphoreCreateBinary();
  semDone = xSemaphoreCreateBinary();
  xTaskCreatePinnedToCore(simTask, "sim", 8192, nullptr, 1, &modTask, 0);
  modRunning = true;
  return true;
}

void step() {
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
    if (M5.BtnB.wasClicked()) {
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

  simIn.touching = t.isPressed();
  simIn.tapped   = pressed;
  simIn.tx = (t.x - XOFF) * 0.5f;
  simIn.ty = (t.y - YOFF) * 0.5f;
  simIn.dt = (now - last) / 1000.0f;
  last = now;

  xSemaphoreGive(semGo);                             // 次のフレームを裏で計算
  renderFrame(uvBuf[front], scrollBuf[front], ledPal[front]);   // 今のフレームを描画
  xSemaphoreTake(semDone, portMAX_DELAY);
  front ^= 1;

  if (++frames, millis() - fpsTimer >= 3000) {
    Serial.printf("fps: %.1f\n", frames * 1000.0f / (millis() - fpsTimer));
    frames = 0; fpsTimer = millis();
  }
}

static void memClearPtrs() {   // 解放後に残る「古い住所」を消す
  env = nullptr;
  for (auto& p : strip) p = nullptr;
  for (auto& p : uvBuf) p = nullptr;
  hField = nullptr;
  baseH = nullptr;
  pid = nullptr;
  cls = nullptr;
  ledR = nullptr;
  noiseT = nullptr;
}

// ---- 別のスケッチに切り替えるとき ------------------------------------------
void stop() {
  if (!modRunning) return;
  if (modTask)  { vTaskDelete(modTask);     modTask = nullptr; }
  if (semGo)    { vSemaphoreDelete(semGo);   semGo = nullptr; }
  if (semDone)  { vSemaphoreDelete(semDone); semDone = nullptr; }
  memFreeAll();
  memClearPtrs();
  modRunning = false;
}

}  // namespace NullPavilion
