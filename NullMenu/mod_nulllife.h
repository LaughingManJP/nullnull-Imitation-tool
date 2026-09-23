// NullLife  -  NullMenu 用モジュール(元のスケッチを名前空間に入れただけ)
#pragma once

namespace NullLife {

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
// NullLife ぬるぬる版  -  null²風「生きている鏡面」
//   M5Stack StopWatch(丸い画面)/ CoreS3・CoreS3 Lite・CoreS3 SE(四角い画面)両対応
//   画面の形を見て自動で切り替わります。
//
// 触るとぬるっと:
//   ・触る          : 膜が指に吸いつき、まわりがくぼんで、ぬちゃっと持ち上がる
//   ・生き物を押す  : ぬるっと指の下からすり抜けて、ぷるんと震える
//   ・なぞる        : 膜が指についてきて伸び、ぬめった跡がテカテカ光る
//   ・触り続ける    : 中の生き物が腕を伸ばして指にくっつく
//   ・指を離す      : 腕が糸を引いて細く伸び…ぷつっと切れて、ぷるんと縮む
//   ・放っておく    : 表面のぬめりがゆっくり流れ、ときどき何かがぬるっと浮かぶ
//
// 操作:
//   StopWatch  … ボタンA: 景色(銀/夕暮れ/ネオン/金)  ボタンB: 明るさ
//   CoreS3系   … ダブルタップ: 景色               電源ボタン短押し: 明るさ
//
// 必要なもの:
//   ボードマネージャー M5Stack >= 3.3.7
//   ボード: StopWatch なら「M5StopWatch」、CoreS3系なら「M5CoreS3」
//   ライブラリ M5Unified >= 0.2.15, M5GFX >= 0.2.21
//
// しくみ:
//   画面の半分の細かさの格子で「高さ」を計算 → 傾きから鏡に映る方向を求め、
//   景色テクスチャを引いて 2倍に拡大して描画。計算はコア0、描画はコア1で並行。
//   高さ = ふくらんだ膜 + ねっとり変形 + ぬめり + 生き物 + 腕(糸)


// ---- ぬるぬる具合の調整用 --------------------------------------------------
static constexpr float GOO_STICK   = 15.0f;   // 指に吸いつく高さ
static constexpr float GOO_GRIP    = 3.0f;    // 吸いつく速さ
static constexpr float GOO_SUCK    = 0.35f;   // 吸いついたまわりのくぼみ
static constexpr float GOO_DIFFUSE = 0.17f;   // 粘りの広がり(0.24以下)
static constexpr float GOO_RELAX   = 0.35f;   // 元に戻る速さ(小さいほどゆっくり)
static constexpr float LUMP_FORCE  = 6.0f;    // 生き物の動く力
static constexpr float LUMP_DRAG   = 2.4f;    // 生き物の重たさ(大きいほど粘る)
static constexpr float SLIP        = 26.0f;   // 押したときにすり抜ける強さ
static constexpr float JIGGLE      = 0.35f;   // ぷるんと震える大きさ
static constexpr float TENDRIL_IN  = 0.8f;    // 腕が伸びる速さ
static constexpr float STRING_TIME = 1.0f;    // 離したとき糸を引く長さ(秒)
static constexpr float SHEEN       = 0.30f;   // 表面のぬめり(テカりのゆらぎ)
static constexpr float REFLECT     = 16.0f;   // 映り込みのゆがみ(大きいほどギラッと)
// ---------------------------------------------------------------------------

static constexpr int ENV   = 128;             // 景色テクスチャのサイズ
static constexpr int NLUMP = 4;               // 膜の下の「生き物」の数
static constexpr int NOISE = 64;              // ぬめり用ノイズのサイズ

// 画面に合わせて起動時に決まる値
static int   W, H, CXN, CYN, GX, GY, GN;
static float MX, MY, RR, RD, SC;
static bool  ROUND;

struct UV { uint16_t u, v; };                 // 8.8 固定小数点

static uint16_t* env;          // 景色テクスチャ (内部RAM)
static uint16_t* strip[2];     // 描画用バッファ (DMA)
static UV*       uvBuf[2];     // 反射方向 (ダブルバッファ)
static float*    hField;       // 高さ
static float*    gooA;         // ねっとりした変形
static float*    gooB;         // 作業用
static float*    dome;         // 中心からの距離^2
static uint8_t*  mask;         // 膜のある場所=1
static float*    noiseT;       // ぬめり用ノイズ
static float*    sheenC;       // ぬめり(4マスおき)
static float*    sheenRow;     // ぬめり(1行ぶん)
static int       CGX, CGY;
static int16_t*  rowS;         // 高さを計算する範囲(行ごと)
static int16_t*  rowE;
static int16_t*  gradS;        // 傾きを計算する範囲(行ごと)
static int16_t*  gradE;
static float     expLut[257];

struct Lump { float x, y, vx, vy, baseAmp, sig, phase, theta, jig, jigPh; };
static Lump lumps[NLUMP];

struct Input { bool touching; bool tapped; float tx, ty, px, py; float dt; };
static Input simIn;

static SemaphoreHandle_t semGo, semDone;
static int front = 0;

// 生き物の状態
static float simT = 0, activity = 0, fear = 0, wake = 0;
static float breathPhase = 0, hbPhase = 0;
static float gooQuiet = 0;
static bool  gooCleared = false;

// 腕(触手)と糸
enum TState { T_NONE, T_REACH, T_STRING, T_SNAP };
static TState tState = T_NONE;
static int    tLump = -1;
static float  tS = 0, tV = 0, tTx = 0, tTy = 0, strT = 0, strDur = 1;

// 表面にぬるっと浮かぶもの
struct Bubble { float x, y, t, T, amp, sig; bool on; };
static Bubble bubble = {0, 0, 0, 0, 0, 0, false};
static float bubbleTimer = 4;

static int theme = 0;
static const uint8_t brightSteps[] = {60, 120, 190, 255};
static int brightIdx = 2;

// ---------------------------------------------------------------- 景色
struct Col { float r, g, b; };
struct Theme { Col skyTop, horizon, ground, band, sun, glow; };
static const Theme THEMES[] = {
  // 銀 (null²っぽい昼の鏡面)
  {{0.55f,0.72f,0.95f},{0.95f,0.97f,1.00f},{0.07f,0.08f,0.10f},{0.38f,0.40f,0.44f},{1.0f,1.0f,1.0f},{0.85f,0.92f,1.0f}},
  // 夕暮れ
  {{0.16f,0.10f,0.38f},{1.00f,0.55f,0.30f},{0.05f,0.03f,0.08f},{0.42f,0.18f,0.26f},{1.0f,0.85f,0.5f},{1.0f,0.55f,0.30f}},
  // ネオン
  {{0.01f,0.01f,0.04f},{0.12f,0.05f,0.28f},{0.00f,0.00f,0.02f},{0.00f,0.75f,0.90f},{1.0f,0.2f,0.8f},{0.30f,0.90f,1.0f}},
  // 金
  {{0.95f,0.82f,0.50f},{1.00f,0.97f,0.82f},{0.16f,0.09f,0.02f},{0.62f,0.42f,0.12f},{1.0f,1.0f,0.9f},{1.0f,0.90f,0.60f}},
};
static constexpr int NTHEME = sizeof(THEMES) / sizeof(THEMES[0]);

static inline float clamp01(float v) { return v < 0 ? 0 : (v > 1 ? 1 : v); }
static inline Col lerpC(Col a, Col b, float t) {
  return {a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t};
}

static void buildEnv(int th) {
  const Theme& T = THEMES[th];
  for (int j = 0; j < ENV; ++j) {
    for (int i = 0; i < ENV; ++i) {
      float x = (i - 63.5f) / 64.0f;
      float y = (j - 63.5f) / 64.0f;               // 下向きが正
      float hz = 0.08f + 0.06f * sinf(x * 2.2f);   // ゆるく波打つ地平線
      Col c;
      if (y < hz) {                                // 空
        float s = clamp01((hz - y) / (hz + 1.0f));
        c = lerpC(T.horizon, T.skyTop, powf(s, 0.6f));
        float dx = x - 0.35f, dy = y + 0.55f;
        float sun = expf(-(dx * dx + dy * dy) / 0.02f);
        c.r += T.sun.r * sun; c.g += T.sun.g * sun; c.b += T.sun.b * sun;
      } else {                                     // 地面と映り込む構造物
        float gnd = clamp01((y - hz) / (1.0f - hz));
        float bands = 0.5f + 0.5f * sinf(x * 9.0f + sinf(x * 3.0f) * 2.0f);
        bands = powf(bands, 6.0f) * (1.0f - gnd);
        Col base = lerpC(T.band, T.ground, powf(gnd, 0.45f));
        c = lerpC(base, T.band, bands);
      }
      float g = expf(-(y - hz) * (y - hz) / 0.006f);   // 地平線の光
      c.r += T.glow.r * g * 0.8f; c.g += T.glow.g * g * 0.8f; c.b += T.glow.b * g * 0.8f;

      // ぬれたテカり: ふくらみの肩に、小さく鋭いハイライトが乗るように
      float s1x = x + 0.13f, s1y = y + 0.17f, s2x = x + 0.27f, s2y = y + 0.36f;
      float glint = 1.4f * expf(-(s1x * s1x + s1y * s1y) / (2 * 0.04f * 0.04f))
                  + 0.6f * expf(-(s2x * s2x + s2y * s2y) / (2 * 0.07f * 0.07f));
      c.r += glint; c.g += glint; c.b += glint;

      uint16_t col = M5.Display.color565(clamp01(c.r) * 255, clamp01(c.g) * 255, clamp01(c.b) * 255);
      env[j * ENV + i] = __builtin_bswap16(col);   // 色がおかしい場合はこの bswap を外す
    }
  }
}

// ---------------------------------------------------------------- 道具
static inline float frand() { return (esp_random() & 0xFFFF) / 65535.0f; }
static inline float smooth3(float t) { t = clamp01(t); return t * t * (3 - 2 * t); }

static void buildNoise() {
  float* tmp = (float*)malloc(NOISE * NOISE * sizeof(float));
  for (int i = 0; i < NOISE * NOISE; ++i) noiseT[i] = frand() * 2 - 1;
  for (int pass = 0; pass < 4; ++pass) {            // ぼかして、なめらかなうねりに
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

static inline float noiseAt(float x, float y) {
  float fx0 = floorf(x), fy0 = floorf(y);
  float fx = x - fx0, fy = y - fy0;
  int x0 = (int)fx0 & (NOISE - 1), y0 = (int)fy0 & (NOISE - 1);
  int x1 = (x0 + 1) & (NOISE - 1),  y1 = (y0 + 1) & (NOISE - 1);
  float a = noiseT[y0 * NOISE + x0], b = noiseT[y0 * NOISE + x1];
  float c = noiseT[y1 * NOISE + x0], d = noiseT[y1 * NOISE + x1];
  return (a + (b - a) * fx) + ((c + (d - c) * fx) - (a + (b - a) * fx)) * fy;
}

// 丸いふくらみを足す
static void addBlob(float* f, float cx, float cy, float amp, float sig) {
  if (sig < 1.0f) sig = 1.0f;
  float inv = 32.0f / (sig * sig);
  float rad = sig * 2.4f;
  int x0 = max(0, (int)(cx - rad)), x1 = min(GX - 1, (int)(cx + rad));
  int y0 = max(0, (int)(cy - rad)), y1 = min(GY - 1, (int)(cy + rad));
  for (int y = y0; y <= y1; ++y) {
    float dy2 = (y - cy) * (y - cy);
    float* row = f + y * GX;
    for (int x = x0; x <= x1; ++x) {
      float dx = x - cx;
      float q = (dx * dx + dy2) * inv;
      if (q < 255.0f) { int i = (int)q; row[x] += amp * (expLut[i] + (expLut[i + 1] - expLut[i]) * (q - i)); }
    }
  }
}

// 向きをもって伸びた(縮んだ)ふくらみ
static void addStretchedBlob(float* f, float cx, float cy, float amp,
                             float sa, float sp, float dirX, float dirY) {
  float ia = 32.0f / (sa * sa), ip = 32.0f / (sp * sp);
  float rad = fmaxf(sa, sp) * 2.4f;
  int x0 = max(0, (int)(cx - rad)), x1 = min(GX - 1, (int)(cx + rad));
  int y0 = max(0, (int)(cy - rad)), y1 = min(GY - 1, (int)(cy + rad));
  for (int y = y0; y <= y1; ++y) {
    float dy = y - cy;
    float* row = f + y * GX;
    for (int x = x0; x <= x1; ++x) {
      float dx = x - cx;
      float a = dx * dirX + dy * dirY;
      float p = -dx * dirY + dy * dirX;
      float q = a * a * ia + p * p * ip;
      if (q < 255.0f) { int i = (int)q; row[x] += amp * (expLut[i] + (expLut[i + 1] - expLut[i]) * (q - i)); }
    }
  }
}

static inline float sampleGoo(const float* g, float x, float y) {
  x = x < 0 ? 0 : (x > GX - 1.001f ? GX - 1.001f : x);
  y = y < 0 ? 0 : (y > GY - 1.001f ? GY - 1.001f : y);
  int x0 = (int)x, y0 = (int)y;
  float fx = x - x0, fy = y - y0;
  const float* p = g + y0 * GX + x0;
  return (p[0] * (1 - fx) + p[1] * fx) * (1 - fy) + (p[GX] * (1 - fx) + p[GX + 1] * fx) * fy;
}

// 指で膜を引っぱる(指のまわりの膜を、指の動きにあわせて引きずる)
static void dragGoo(float px, float py, float qx, float qy) {
  float mx = px - qx, my = py - qy;
  float m2 = mx * mx + my * my;
  float jump = 30.0f * SC;
  if (m2 < 0.0025f || m2 > jump * jump) return;   // 動いていない or 指が飛んだ
  int rad = (int)(18 * SC);
  float s2 = 2.0f * (8 * SC) * (8 * SC);
  int x0 = max(1, (int)px - rad), x1 = min(GX - 2, (int)px + rad);
  int y0 = max(1, (int)py - rad), y1 = min(GY - 2, (int)py + rad);
  for (int y = y0 - 1; y <= y1 + 1; ++y)            // 作業用に範囲だけ写す
    memcpy(gooB + y * GX + x0 - 1, gooA + y * GX + x0 - 1, (x1 - x0 + 3) * sizeof(float));
  for (int y = y0; y <= y1; ++y)
    for (int x = x0; x <= x1; ++x) {
      float dx = x - px, dy = y - py;
      float w = expf(-(dx * dx + dy * dy) / s2) * 0.95f;
      if (w < 0.01f) continue;
      float sx = x - w * mx, sy = y - w * my;
      if (sx < x0 - 1 || sx >= x1 || sy < y0 - 1 || sy >= y1) continue;
      gooA[y * GX + x] = sampleGoo(gooB, sx, sy);
    }
}

static inline float softPulse(float x) { return expf(-(x * x) / (2 * 0.07f * 0.07f)); }

// 腕が切れたときの、ぷつっ → ぷるん
static void snapString() {
  if (tLump < 0) return;
  Lump& L = lumps[tLump];
  addBlob(gooA, tTx, tTy, L.baseAmp * 0.55f, L.sig * 0.45f);   // 指先に残ったしずく
  float dx = L.x - tTx, dy = L.y - tTy, d = sqrtf(dx * dx + dy * dy) + 0.001f;
  L.vx += dx / d * 20.0f * SC; L.vy += dy / d * 20.0f * SC;   // 反動で少し戻る
  L.jig = 1.0f;
  tState = T_SNAP; tV = -2.5f;
}

// ---------------------------------------------------------------- シミュレーション
static void simulate(const Input& in, UV* out) {
  float dt = in.dt > 0.05f ? 0.05f : in.dt;
  simT += dt;

  // 気分: 触られると興奮し、放っておくと落ち着く
  if (in.touching) activity = fminf(1.0f, activity + dt * 0.8f);
  else             activity = fmaxf(0.0f, activity - dt / 25.0f);
  fear *= expf(-dt * 0.8f);
  wake  = fminf(1.0f, wake + dt / 4.0f);             // 起動時にゆっくり目覚める

  // ---- 触った瞬間: ぬちゃっと沈み、押された生き物はすり抜けて震える
  if (in.tapped) {
    fear = fminf(1.0f, fear + 0.6f);
    addBlob(gooA, in.tx, in.ty, -6.0f * SC, 8.0f * SC);
    for (auto& L : lumps) {
      float dx = L.x - in.tx, dy = L.y - in.ty;
      float d = fmaxf(sqrtf(dx * dx + dy * dy), 1.0f);
      if (d < L.sig * 1.6f) {                        // 押された子: ぬるっと逃げて、ぷるん
        L.vx += (dx / d * 0.8f - dy / d * 0.6f) * 45.0f * SC;
        L.vy += (dy / d * 0.8f + dx / d * 0.6f) * 45.0f * SC;
        L.jig = 1.0f;
      } else {
        float k = 20.0f * SC / (1.0f + d / (30.0f * SC));
        L.vx += dx / d * k; L.vy += dy / d * k;
      }
    }
  }

  // ---- 触っている間: 吸いつき + 引っぱり
  if (in.touching) {
    dragGoo(in.tx, in.ty, in.px, in.py);
    float s1 = 7.0f * SC, s2 = s1 * 2.2f;
    float i1 = 1.0f / (2 * s1 * s1), i2 = 1.0f / (2 * s2 * s2);
    float grip = fminf(1.0f, dt * GOO_GRIP);
    float stick = GOO_STICK * SC;
    int rad = (int)(s2 * 2.3f);
    int x0 = max(1, (int)in.tx - rad), x1 = min(GX - 2, (int)in.tx + rad);
    int y0 = max(1, (int)in.ty - rad), y1 = min(GY - 2, (int)in.ty + rad);
    for (int y = y0; y <= y1; ++y)
      for (int x = x0; x <= x1; ++x) {
        float dx = x - in.tx, dy = y - in.ty, d2 = dx * dx + dy * dy;
        float w1 = expf(-d2 * i1), w2 = expf(-d2 * i2);
        float target = stick * (w1 - GOO_SUCK * w2);   // 真ん中は持ち上がり、まわりはくぼむ
        float& g = gooA[y * GX + x];
        g += (target - g) * grip * w2;
      }
  }

  // ---- 粘りで広がりながら、ゆっくりたれて戻る(振動しない)
  //      しばらく触らないと膜は平らに戻りきるので、計算を休んで軽くする
  if (in.touching || in.tapped || tState != T_NONE) gooQuiet = 0;
  else gooQuiet += dt;
  if (gooQuiet > 30.0f) {
    if (!gooCleared) { memset(gooA, 0, GN * sizeof(float)); gooCleared = true; }
  } else {
    gooCleared = false;
    float relax = expf(-dt * GOO_RELAX);
    for (int pass = 0; pass < 2; ++pass) {
      for (int y = 1; y < GY - 1; ++y) {
        int i = y * GX + 1;
        for (int x = 1; x < GX - 1; ++x, ++i) {
          if (!mask[i]) { gooB[i] = 0; continue; }
          float lap = gooA[i - 1] + gooA[i + 1] + gooA[i - GX] + gooA[i + GX] - 4.0f * gooA[i];
          gooB[i] = (gooA[i] + GOO_DIFFUSE * lap) * (pass == 0 ? relax : 1.0f);
        }
      }
      float* t = gooA; gooA = gooB; gooB = t;
    }
  }

  // ---- 呼吸と鼓動(やわらかく)
  float period = fmaxf(3.0f, 6.0f - 2.0f * activity - 1.0f * fear);
  breathPhase += dt * 6.2832f / period;
  float breath = sinf(breathPhase);
  hbPhase += dt / (1.6f - 0.5f * activity - 0.3f * fear);
  if (hbPhase >= 1.0f) hbPhase -= 1.0f;
  float hb = softPulse(hbPhase - 0.10f) + 0.5f * softPulse(hbPhase - 0.32f);

  // ---- 腕: 伸ばす → (離すと)糸を引く → ぷつっ → ぷるんと戻る
  if (in.touching) {
    if (tState != T_REACH) {
      if (tState == T_STRING) snapString();
      float best = 1e9f;
      for (int n = 0; n < NLUMP; ++n) {
        float dx = lumps[n].x - in.tx, dy = lumps[n].y - in.ty, d = dx * dx + dy * dy;
        if (d < best) { best = d; tLump = n; }
      }
      if (tState == T_NONE || tState == T_SNAP) tS = fmaxf(tS, 0.0f);
      tState = T_REACH;
    }
    tTx = in.tx; tTy = in.ty;
    tS = fminf(1.0f, tS + dt * TENDRIL_IN * (0.5f + activity));
  } else if (tState == T_REACH) {
    if (tS > 0.25f) { tState = T_STRING; strT = 0; strDur = STRING_TIME * (0.6f + 0.8f * tS); }
    else            { tState = T_SNAP; tV = 0; }
  }
  if (tState == T_STRING) {
    strT += dt;
    if (strT >= strDur) snapString();
  } else if (tState == T_SNAP) {                     // ばねのように、少し行き過ぎて戻る
    tV += (-55.0f * tS - 7.0f * tV) * dt;
    tS += tV * dt;
    if (fabsf(tS) < 0.01f && fabsf(tV) < 0.05f) { tState = T_NONE; tS = 0; tV = 0; }
  }

  // ---- 生き物: 重たく、のたうつように動く
  float speed = 0.5f + 0.8f * activity + 0.6f * fear;
  for (int n = 0; n < NLUMP; ++n) {
    Lump& L = lumps[n];
    L.theta += dt * speed * (0.6f * sinf(simT * 0.23f + L.phase) + 0.5f * sinf(simT * 0.51f + L.phase * 1.7f));
    float fx = cosf(L.theta) * LUMP_FORCE * SC * speed;
    float fy = sinf(L.theta) * LUMP_FORCE * SC * speed;

    float dx = L.x - MX, dy = L.y - MY;               // 画面の内側にとどまる
    if (ROUND) {
      float r = sqrtf(dx * dx + dy * dy) + 0.001f, lim = 0.55f * RR;
      if (r > lim) { fx -= dx / r * (r - lim) * 2.0f; fy -= dy / r * (r - lim) * 2.0f; }
    } else {
      float limX = 0.70f * MX, limY = 0.62f * MY;
      if (dx >  limX) fx -= (dx - limX) * 2.0f;
      if (dx < -limX) fx -= (dx + limX) * 2.0f;
      if (dy >  limY) fy -= (dy - limY) * 2.0f;
      if (dy < -limY) fy -= (dy + limY) * 2.0f;
    }

    for (int m = 0; m < NLUMP; ++m) {                // くっつきすぎない(少し重なるのはOK)
      if (m == n) continue;
      float ox = L.x - lumps[m].x, oy = L.y - lumps[m].y;
      float d = sqrtf(ox * ox + oy * oy) + 0.001f;
      float want = (L.sig + lumps[m].sig) * 0.7f;
      if (d < want) { fx += ox / d * (want - d) * 0.8f; fy += oy / d * (want - d) * 0.8f; }
    }

    if (in.touching) {
      float tx = L.x - in.tx, ty = L.y - in.ty;
      float d = sqrtf(tx * tx + ty * ty) + 0.001f;
      float reach = L.sig * 1.3f;
      if (d < reach) {                               // 指の下からぬるっとすり抜ける
        float p = (reach - d) / reach;
        float side = (n & 1) ? 1.0f : -1.0f;
        fx += (tx / d - side * ty / d * 0.7f) * SLIP * SC * p;
        fy += (ty / d + side * tx / d * 0.7f) * SLIP * SC * p;
        L.jig = fmaxf(L.jig, 0.4f * p);
      } else if (n == tLump && tState == T_REACH && d > 35 * SC) {
        fx -= tx / d * 5.0f * SC * tS; fy -= ty / d * 5.0f * SC * tS;   // 腕を伸ばした子は体ごと寄る
      }
    }
    if (n == tLump && tState == T_STRING) {          // 離されたら体を引いて、糸がのびる
      float tx = L.x - tTx, ty = L.y - tTy;
      float d = sqrtf(tx * tx + ty * ty) + 0.001f;
      fx += tx / d * 14.0f * SC; fy += ty / d * 14.0f * SC;
    }

    float damp = expf(-dt * LUMP_DRAG);
    L.vx = (L.vx + fx * dt) * damp;
    L.vy = (L.vy + fy * dt) * damp;
    L.x += L.vx * dt; L.y += L.vy * dt;
    L.jig *= expf(-dt * 2.5f);
    L.jigPh += dt * 13.0f;
  }

  // ---- 表面にときどき何かがぬるっと浮かんで、沈む
  bubbleTimer -= dt;
  if (!bubble.on && bubbleTimer < 0) {
    bubble.on = true; bubble.t = 0;
    bubble.T = 3.5f + frand() * 3.0f;
    float a = frand() * 6.2832f, r = sqrtf(frand()) * 0.6f;
    bubble.x = MX + cosf(a) * r * (ROUND ? RR : MX);
    bubble.y = MY + sinf(a) * r * (ROUND ? RR : MY);
    bubble.amp = (5.0f + frand() * 5.0f) * SC;
    bubble.sig = (7.0f + frand() * 6.0f) * SC;
    bubbleTimer = (6.0f + frand() * 8.0f) * (1.0f - 0.4f * activity);
  }

  // ---- ぬめり: なめらかな模様なので、4マスおきに計算して間を補う(軽くするため)
  {
    float ns = 0.11f / SC;
    float o1x = simT * 0.35f, o1y = simT * 0.20f, o2x = -simT * 0.25f, o2y = simT * 0.30f;
    for (int cy = 0; cy < CGY; ++cy)
      for (int cx = 0; cx < CGX; ++cx) {
        float x = cx * 4.0f, y = cy * 4.0f;
        sheenC[cy * CGX + cx] = SHEEN * SC * (noiseAt(x * ns + o1x, y * ns + o1y) +
                                              0.6f * noiseAt(x * ns * 1.7f + o2x, y * ns * 1.7f + o2y));
      }
  }

  // ---- 高さ = ふくらんだ膜 + ねっとり変形 + ぬめり
  float domeDepth = ((ROUND ? 9.0f : 14.0f) + 2.0f * breath + 1.5f * hb) * SC;
  float wetK = 0.15f / SC;
  for (int y = 0; y < GY; ++y) {
    const float* c0 = sheenC + (y >> 2) * CGX;
    const float* c1 = c0 + CGX;
    float fy = (y & 3) * 0.25f;
    for (int cx = 0; cx < CGX; ++cx) sheenRow[cx] = c0[cx] + (c1[cx] - c0[cx]) * fy;
    int xs = rowS[y], xe = rowE[y];
    int i = y * GX + xs;
    for (int x = xs; x <= xe; ++x, ++i) {
      float a = sheenRow[x >> 2], b = sheenRow[(x >> 2) + 1];
      float n = a + (b - a) * ((x & 3) * 0.25f);
      float g = gooA[i];
      hField[i] = -domeDepth * dome[i] + g + n * (1.0f + wetK * fabsf(g));   // 触った跡ほどテカテカ
    }
  }

  // ---- 生き物
  float gain = wake * (1.0f + 0.10f * breath + 0.05f * hb) * (1.0f - 0.35f * fear);
  for (int n = 0; n < NLUMP; ++n) {
    Lump& L = lumps[n];
    float wob = L.jig * JIGGLE * sinf(L.jigPh);      // ぷるぷる
    float amp = L.baseAmp * gain * (0.85f + 0.15f * sinf(simT * 0.7f + L.phase)) * (1.0f + wob);
    float sig = L.sig * (1.0f + 0.07f * breath);
    float v = sqrtf(L.vx * L.vx + L.vy * L.vy);
    float stretch = fminf(1.3f, v * 0.06f / SC);     // 速く動くほど、ねばっと伸びる
    float dirX = v > 0.01f ? L.vx / v : 1.0f, dirY = v > 0.01f ? L.vy / v : 0.0f;
    float sa = sig * (1.0f + stretch) * (1.0f - 0.5f * wob);
    float sp = sig / (1.0f + 0.5f * stretch) * (1.0f + 0.5f * wob);
    addStretchedBlob(hField, L.x, L.y, amp, sa, sp, dirX, dirY);
    addBlob(hField, L.x - L.vx * 0.35f, L.y - L.vy * 0.35f, amp * 0.35f, sig * 0.8f);   // ぬめっとした余韻
  }

  // ---- 腕 / 糸
  if (tState != T_NONE && tLump >= 0) {
    Lump& L = lumps[tLump];
    float e;
    float neck = 0;                                  // 糸の細さ(0=ふつう → 1=切れる寸前)
    float tipX = tTx, tipY = tTy;
    if (tState == T_REACH)       e = smooth3(tS);
    else if (tState == T_STRING) {
      e = 1.0f;
      neck = strT / strDur;
      float ax = tTx - L.x, ay = tTy - L.y, al = sqrtf(ax * ax + ay * ay) + 0.001f;
      float sag = 6.0f * SC * neck * neck;           // 先のしずくが、重みで少したれる
      tipX += ax / al * sag; tipY += ay / al * sag;
    } else                       e = tS;             // T_SNAP: 少し行き過ぎてもOK
    float ax = (tipX - L.x) * e, ay = (tipY - L.y) * e;
    float len = sqrtf(ax * ax + ay * ay);
    if (len > 3.0f * SC) {
      float nx = -ay / len, ny = ax / len;
      int beads = (int)fminf(22.0f, fmaxf(5.0f, len / (3.5f * SC)));
      float wob = (5.0f * sinf(simT * 1.1f) + 3.0f * sinf(simT * 2.3f + 1.0f)) * SC * (1.0f - neck);
      for (int b = 1; b <= beads; ++b) {
        float t = (float)b / beads;
        float bend = sinf(t * 3.1416f) * wob;
        float bx = L.x + ax * t + nx * bend;
        float by = L.y + ay * t + ny * bend;
        float thick = L.sig * (0.50f - 0.28f * t) + 3.5f * SC;
        float a = L.baseAmp * gain * (0.75f - 0.35f * t);
        if (neck > 0) {                              // 真ん中から細く、糸のように
          float pinch = smooth3(neck * 1.3f) * expf(-(t - 0.55f) * (t - 0.55f) / (2 * 0.26f * 0.26f));
          thick *= 1.0f - 0.80f * pinch;
          a     *= 1.0f - 0.88f * pinch;
        }
        if (b == beads) { thick *= 1.3f; a *= 1.2f; }    // 先っぽは少しふくらむ(しずく)
        addBlob(hField, bx, by, a, fmaxf(thick, 1.5f * SC));
      }
    }
  }

  if (bubble.on) {
    bubble.t += dt;
    if (bubble.t >= bubble.T) bubble.on = false;
    else {
      float s = sinf(3.1416f * bubble.t / bubble.T);
      addBlob(hField, bubble.x, bubble.y, bubble.amp * s * s * wake, bubble.sig * (0.7f + 0.3f * s));
    }
  }

  // ---- 傾き → 鏡に映る方向
  const float k = REFLECT * 256.0f;
  const float maxUV = (ENV - 1) * 256.0f + 255.0f;
  for (int y = 0; y < GY; ++y) {
    int ym = y > 0 ? y - 1 : 0, yp = y < GY - 1 ? y + 1 : GY - 1;
    int xs = gradS[y], xe = gradE[y];                // 丸の外は見えないので計算しない
    for (int x = xs; x <= xe; ++x) {
      int xm = x > 0 ? x - 1 : 0, xp = x < GX - 1 ? x + 1 : GX - 1;
      float gx = hField[y * GX + xp] - hField[y * GX + xm];
      float gy = hField[yp * GX + x] - hField[ym * GX + x];
      float u = 64.0f * 256.0f + gx * k;
      float v = 64.0f * 256.0f + gy * k;
      u = u < 0 ? 0 : (u > maxUV ? maxUV : u);
      v = v < 0 ? 0 : (v > maxUV ? maxUV : v);
      out[y * GX + x] = {(uint16_t)u, (uint16_t)v};
    }
  }
}

static void simTask(void*) {
  for (;;) {
    xSemaphoreTake(semGo, portMAX_DELAY);
    simulate(simIn, uvBuf[front ^ 1]);
    xSemaphoreGive(semDone);
  }
}

// ---------------------------------------------------------------- 描画
static inline uint16_t envAt(uint32_t u, uint32_t v) { return env[(v >> 8) * ENV + (u >> 8)]; }

static void renderFrame(const UV* uv) {
  M5.Display.startWrite();
  int s = 0;
  for (int gy = 0; gy < CYN; ++gy) {
    const UV* r0 = uv + gy * GX;
    const UV* r1 = r0 + GX;
    uint16_t* row0 = strip[s];
    uint16_t* row1 = row0 + W;
    for (int cx = 0; cx < CXN; ++cx) {
      const UV a = r0[cx], b = r0[cx + 1], c = r1[cx], d = r1[cx + 1];
      row0[cx * 2]     = envAt(a.u, a.v);
      row0[cx * 2 + 1] = envAt((a.u + b.u) >> 1, (a.v + b.v) >> 1);
      row1[cx * 2]     = envAt((a.u + c.u) >> 1, (a.v + c.v) >> 1);
      row1[cx * 2 + 1] = envAt((a.u + b.u + c.u + d.u) >> 2, (a.v + b.v + c.v + d.v) >> 2);
    }
    M5.Display.waitDMA();
    M5.Display.pushImageDMA(0, gy * 2, W, 2, strip[s]);
    s ^= 1;
  }
  M5.Display.waitDMA();
  M5.Display.endWrite();
}

// ---------------------------------------------------------------- setup / loop
bool begin() {

  // 画面の形で機種を判断: 正方形 = StopWatch(丸)、横長 = CoreS3系
  ROUND = (M5.Display.width() == M5.Display.height());
  if (!ROUND && M5.Display.width() < M5.Display.height()) M5.Display.setRotation(1);
  W = M5.Display.width();
  H = M5.Display.height();
  CXN = W / 2; CYN = H / 2;
  GX = CXN + 1; GY = CYN + 1; GN = GX * GY;
  MX = (GX - 1) * 0.5f; MY = (GY - 1) * 0.5f;
  RR = fminf(MX, MY);
  RD = ROUND ? RR : 100.0f;
  SC = fminf(GX, GY) / 121.0f;                      // CoreS3 を基準(1.0)にした大きさ
  Serial.printf("screen %dx%d  %s  grid %dx%d\n", W, H, ROUND ? "round" : "rect", GX, GY);

  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setBrightness(brightSteps[brightIdx]);

  env      = (uint16_t*)memCaps(ENV * ENV * 2, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  noiseT   = (float*)memCaps(NOISE * NOISE * sizeof(float), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  strip[0] = (uint16_t*)memCaps(W * 2 * 2, MALLOC_CAP_DMA);
  strip[1] = (uint16_t*)memCaps(W * 2 * 2, MALLOC_CAP_DMA);
  uvBuf[0] = (UV*)memPs(GN * sizeof(UV));
  uvBuf[1] = (UV*)memPs(GN * sizeof(UV));
  hField   = (float*)memPs(GN * sizeof(float));
  dome     = (float*)memPs(GN * sizeof(float));
  gooA     = (float*)memPsCalloc(GN, sizeof(float));
  gooB     = (float*)memPsCalloc(GN, sizeof(float));
  mask     = (uint8_t*)memPs(GN);
  CGX = (GX + 3) / 4 + 2; CGY = (GY + 3) / 4 + 2;
  sheenC   = (float*)memPsCalloc(CGX * CGY, sizeof(float));
  sheenRow = (float*)memCaps(CGX * sizeof(float), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  rowS  = (int16_t*)memAlloc(GY * sizeof(int16_t)); rowE  = (int16_t*)memAlloc(GY * sizeof(int16_t));
  gradS = (int16_t*)memAlloc(GY * sizeof(int16_t)); gradE = (int16_t*)memAlloc(GY * sizeof(int16_t));
  if (!env || !noiseT || !strip[0] || !strip[1] || !uvBuf[0] || !uvBuf[1] ||
      !hField || !dome || !gooA || !gooB || !mask || !sheenC || !sheenRow ||
      !rowS || !rowE || !gradS || !gradE) {
    memFreeAll(); return false;
  }

  for (int y = 0; y < GY; ++y)
    for (int x = 0; x < GX; ++x) {
      float dx = (x - MX) / RD, dy = (y - MY) / RD;
      float r2 = dx * dx + dy * dy;
      dome[y * GX + x] = r2;
      mask[y * GX + x] = ROUND ? (r2 < 0.98f) : 1;
      hField[y * GX + x] = 0;
      uvBuf[0][y * GX + x] = uvBuf[1][y * GX + x] = {64 * 256, 64 * 256};   // 計算しない場所は景色の中心
    }

  // 計算する範囲: 四角い画面は全部。丸い画面は円の内側だけ(少し余裕をもたせる)
  for (int y = 0; y < GY; ++y) {
    if (!ROUND) { rowS[y] = 0; rowE[y] = GX - 1; gradS[y] = 0; gradE[y] = GX - 1; continue; }
    float dy = y - MY;
    float rh = RR + 4.0f, rg = RR + 2.0f;
    float wh = rh * rh - dy * dy, wg = rg * rg - dy * dy;
    int hs = 0, he = -1, gs = 1, ge = 0;
    if (wh > 0) { float s = sqrtf(wh); hs = max(0, (int)floorf(MX - s)); he = min(GX - 1, (int)ceilf(MX + s)); }
    if (wg > 0 && y > 0 && y < GY - 1) {
      float s = sqrtf(wg); gs = max(1, (int)floorf(MX - s)); ge = min(GX - 2, (int)ceilf(MX + s));
    }
    rowS[y] = hs; rowE[y] = he; gradS[y] = gs; gradE[y] = ge;
  }
  for (int i = 0; i < 257; ++i) expLut[i] = expf(-i / 32.0f);
  buildNoise();

  for (auto& L : lumps) {
    L.x = MX + (frand() - 0.5f) * (ROUND ? RR : MX);
    L.y = MY + (frand() - 0.5f) * (ROUND ? RR : MY);
    L.vx = L.vy = 0;
    L.baseAmp = (8.0f + frand() * 5.0f) * SC;
    L.sig     = (13.0f + frand() * 8.0f) * SC;       // 大きめで、ぼてっと
    L.phase   = frand() * 6.2832f;
    L.theta   = frand() * 6.2832f;
    L.jig = 0; L.jigPh = frand() * 6.2832f;
  }

  buildEnv(theme);

  // 最初の1枚
  simIn = {false, false, 0, 0, 0, 0, 0.016f};
  simulate(simIn, uvBuf[front]);

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
  static float prevX = 0, prevY = 0;
  static int frames = 0;

  M5.update();

  auto t = M5.Touch.getDetail();
  uint32_t now = millis();
  bool pressed = t.wasPressed();

  if (ROUND) {                                       // StopWatch: ボタンで操作
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

  float tx = t.x * 0.5f, ty = t.y * 0.5f;
  if (pressed) { prevX = tx; prevY = ty; }

  simIn.touching = t.isPressed();
  simIn.tapped   = pressed;
  simIn.tx = tx;    simIn.ty = ty;
  simIn.px = prevX; simIn.py = prevY;
  simIn.dt = (now - last) / 1000.0f;
  last = now;
  prevX = tx; prevY = ty;

  xSemaphoreGive(semGo);              // 次のフレームを裏で計算
  renderFrame(uvBuf[front]);          // 今のフレームを描画
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
  gooA = nullptr;
  gooB = nullptr;
  dome = nullptr;
  mask = nullptr;
  noiseT = nullptr;
  sheenC = nullptr;
  sheenRow = nullptr;
  rowS = nullptr;
  rowE = nullptr;
  gradS = nullptr;
  gradE = nullptr;
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

}  // namespace NullLife
