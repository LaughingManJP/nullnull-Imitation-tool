// NullLife "ぬるっと版" for M5Stack CoreS3 / CoreS3 Lite / CoreS3 SE
//
// 水面の波紋ではなく、水あめ・液体金属のような「ねっとり」した膜。
//   ・触る          : 膜が指に吸いつくように、ぬぷっと沈んでから盛り上がる
//   ・なぞる        : 膜が指に引っぱられて伸び、離すとゆっくりたれて戻る
//   ・触り続ける    : 中の生き物が触手(ぬるっとした腕)を伸ばして指に届こうとする
//   ・放っておく    : 生き物は重たくのたうち、ときどき何かが表面にぬるっと浮かぶ
//   ・ダブルタップ  : 映り込む景色(配色)を切り替え  銀 / 夕暮れ / ネオン / 金
//   ・電源ボタン短押し: 明るさ切り替え
//
// 必要なもの:
//   ボードマネージャー M5Stack / ボード: M5CoreS3
//   ライブラリ M5Unified, M5GFX (最新版推奨)
//
// しくみ:
//   161x121 の格子で「高さ」を計算 → 傾きから鏡に映る方向を求め、
//   景色テクスチャを引いて 2倍に拡大して 320x240 に描画。
//   高さ = ふくらんだ膜 + ねっとりした変形(粘性の拡散で戻る) + 生き物 + 触手
//   計算はコア0、描画はコア1で並行して行います。

#include <M5Unified.h>
#include <math.h>

static constexpr int   W      = 320;          // 画面サイズ
static constexpr int   H      = 240;
static constexpr int   CX_N   = W / 2;        // 格子のマス数 (x2 = 画面)
static constexpr int   CY_N   = H / 2;
static constexpr int   GX     = CX_N + 1;     // 格子点の数
static constexpr int   GY     = CY_N + 1;
static constexpr int   GN     = GX * GY;
static constexpr float MX     = (GX - 1) * 0.5f;   // 中心
static constexpr float MY     = (GY - 1) * 0.5f;
static constexpr float RD     = 100.0f;       // ふくらみの基準半径
static constexpr int   ENV    = 128;          // 景色テクスチャのサイズ
static constexpr int   NLUMP  = 4;            // 膜の下の「生き物」の数

// ---- ぬるっと具合の調整用 --------------------------------------------------
static constexpr float GOO_STICK   = 11.0f;   // 指に吸いつく高さ
static constexpr float GOO_GRIP    = 2.2f;    // 吸いつく速さ
static constexpr float GOO_DIFFUSE = 0.18f;   // 粘りの広がり(0.24以下)
static constexpr float GOO_RELAX   = 0.45f;   // 元に戻る速さ(小さいほどゆっくり)
static constexpr float LUMP_FORCE  = 6.0f;    // 生き物の動く力
static constexpr float LUMP_DRAG   = 2.2f;    // 生き物の重たさ(大きいほど粘る)
static constexpr float TENDRIL_IN  = 0.55f;   // 触手が伸びる速さ
static constexpr float TENDRIL_OUT = 0.45f;   // 触手が引っこむ速さ
static constexpr float REFLECT     = 16.0f;   // 映り込みのゆがみ(大きいほどギラッと)
// ---------------------------------------------------------------------------

struct UV { uint16_t u, v; };                 // 8.8 固定小数点

static uint16_t* env;          // 景色テクスチャ (内部RAM)
static uint16_t* strip[2];     // 描画用バッファ (DMA)
static UV*       uvBuf[2];     // 反射方向 (ダブルバッファ)
static float*    hField;       // 高さ
static float*    gooA;         // ねっとりした変形
static float*    gooB;         // 作業用
static float*    dome;         // 中心からの距離^2
static float     expLut[256];

struct Lump { float x, y, vx, vy, baseAmp, sig, phase, theta; };
static Lump lumps[NLUMP];

struct Input { bool touching; bool tapped; float tx, ty, px, py; float dt; };
static Input simIn;

static SemaphoreHandle_t semGo, semDone;
static int front = 0;

// 生き物の状態
static float simT = 0, activity = 0, fear = 0, wake = 0;
static float breathPhase = 0, hbPhase = 0;

// 触手
static int   tendrilLump = -1;
static float tendrilS = 0, tendrilTx = 0, tendrilTy = 0;

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
        Col base = lerpC(T.band, T.ground, powf(gnd, 0.45f));   // 地平線近くは明るく、なめらかに暗く
        c = lerpC(base, T.band, bands);
      }
      float g = expf(-(y - hz) * (y - hz) / 0.006f);   // 地平線の光
      c.r += T.glow.r * g * 0.8f; c.g += T.glow.g * g * 0.8f; c.b += T.glow.b * g * 0.8f;
      uint16_t col = M5.Display.color565(clamp01(c.r) * 255, clamp01(c.g) * 255, clamp01(c.b) * 255);
      env[j * ENV + i] = __builtin_bswap16(col);   // 色がおかしい場合はこの bswap を外す
    }
  }
}

// ---------------------------------------------------------------- 道具
static inline float frand() { return (esp_random() & 0xFFFF) / 65535.0f; }

// 丸いふくらみを高さに足す
static void addBlob(float* f, float cx, float cy, float amp, float sig) {
  if (sig < 1.0f) sig = 1.0f;
  float inv = 32.0f / (sig * sig);
  float rad = sig * 2.8f;
  int x0 = max(0, (int)(cx - rad)), x1 = min(GX - 1, (int)(cx + rad));
  int y0 = max(0, (int)(cy - rad)), y1 = min(GY - 1, (int)(cy + rad));
  for (int y = y0; y <= y1; ++y) {
    float dy2 = (y - cy) * (y - cy);
    float* row = f + y * GX;
    for (int x = x0; x <= x1; ++x) {
      float dx = x - cx;
      int idx = (int)((dx * dx + dy2) * inv);
      if (idx < 256) row[x] += amp * expLut[idx];
    }
  }
}

// 動く向きに伸びた(引きのばされた)ふくらみ
static void addStretchedBlob(float* f, float cx, float cy, float amp, float sig,
                             float dirX, float dirY, float stretch) {
  float sa = sig * (1.0f + stretch);          // 進む向き
  float sp = sig / (1.0f + 0.5f * stretch);   // 横方向(細くなる)
  float ia = 32.0f / (sa * sa), ip = 32.0f / (sp * sp);
  float rad = sa * 2.8f;
  int x0 = max(0, (int)(cx - rad)), x1 = min(GX - 1, (int)(cx + rad));
  int y0 = max(0, (int)(cy - rad)), y1 = min(GY - 1, (int)(cy + rad));
  for (int y = y0; y <= y1; ++y) {
    float dy = y - cy;
    float* row = f + y * GX;
    for (int x = x0; x <= x1; ++x) {
      float dx = x - cx;
      float a = dx * dirX + dy * dirY;
      float p = -dx * dirY + dy * dirX;
      int idx = (int)(a * a * ia + p * p * ip);
      if (idx < 256) row[x] += amp * expLut[idx];
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

// 指で膜を引っぱる(指の周りの膜を指の動きにあわせて引きずる)
static void dragGoo(float px, float py, float qx, float qy) {
  float mx = px - qx, my = py - qy;
  float m2 = mx * mx + my * my;
  if (m2 < 0.0025f || m2 > 900.0f) return;    // 動いていない or 指が飛んだ
  memcpy(gooB, gooA, GN * sizeof(float));
  const int rad = 16;
  const float s2 = 2.0f * 7.0f * 7.0f;
  int x0 = max(1, (int)px - rad), x1 = min(GX - 2, (int)px + rad);
  int y0 = max(1, (int)py - rad), y1 = min(GY - 2, (int)py + rad);
  for (int y = y0; y <= y1; ++y) {
    for (int x = x0; x <= x1; ++x) {
      float dx = x - px, dy = y - py;
      float w = expf(-(dx * dx + dy * dy) / s2) * 0.85f;
      if (w < 0.01f) continue;
      gooA[y * GX + x] = sampleGoo(gooB, x - w * mx, y - w * my);
    }
  }
}

static inline float softPulse(float x) { return expf(-(x * x) / (2 * 0.07f * 0.07f)); }

// ---------------------------------------------------------------- シミュレーション
static void simulate(const Input& in, UV* out) {
  float dt = in.dt > 0.05f ? 0.05f : in.dt;
  simT += dt;

  // 気分: 触られると興奮し、放っておくと落ち着く
  if (in.touching) activity = fminf(1.0f, activity + dt * 0.8f);
  else             activity = fmaxf(0.0f, activity - dt / 25.0f);
  fear *= expf(-dt * 0.8f);
  wake  = fminf(1.0f, wake + dt / 4.0f);             // 起動時にゆっくり目覚める

  // ---- ねっとりした変形
  if (in.tapped) {                                   // ぬぷっ(いったん沈む)
    fear = 1.0f;
    addBlob(gooA, in.tx, in.ty, -7.0f, 9.0f);
    for (auto& L : lumps) {                          // 生き物は重たく身を引く
      float dx = L.x - in.tx, dy = L.y - in.ty;
      float d = fmaxf(sqrtf(dx * dx + dy * dy), 1.0f);
      float k = 30.0f / (1.0f + d / 30.0f);
      L.vx += dx / d * k; L.vy += dy / d * k;
    }
  }
  if (in.touching) {
    dragGoo(in.tx, in.ty, in.px, in.py);             // 引っぱって伸ばす
    // 指の下の膜を、指に吸いつくように持ち上げる
    const float s2 = 2.0f * 6.0f * 6.0f;
    float grip = fminf(1.0f, dt * GOO_GRIP);
    int x0 = max(1, (int)in.tx - 14), x1 = min(GX - 2, (int)in.tx + 14);
    int y0 = max(1, (int)in.ty - 14), y1 = min(GY - 2, (int)in.ty + 14);
    for (int y = y0; y <= y1; ++y)
      for (int x = x0; x <= x1; ++x) {
        float dx = x - in.tx, dy = y - in.ty;
        float w = expf(-(dx * dx + dy * dy) / s2);
        float& g = gooA[y * GX + x];
        g += (GOO_STICK * w - g) * grip * w;
      }
  }

  // 粘りで広がりながら、ゆっくりたれて戻る(振動しない)
  {
    float relax = expf(-dt * GOO_RELAX);
    for (int pass = 0; pass < 2; ++pass) {
      for (int y = 1; y < GY - 1; ++y) {
        int i = y * GX + 1;
        for (int x = 1; x < GX - 1; ++x, ++i) {
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

  // ---- 触手: 触り続けると、いちばん近い生き物が指へ腕を伸ばす
  if (in.touching) {
    if (tendrilLump < 0 || tendrilS < 0.02f) {
      float best = 1e9f;
      for (int n = 0; n < NLUMP; ++n) {
        float dx = lumps[n].x - in.tx, dy = lumps[n].y - in.ty, d = dx * dx + dy * dy;
        if (d < best) { best = d; tendrilLump = n; }
      }
    }
    tendrilTx = in.tx; tendrilTy = in.ty;
    if (!in.tapped) tendrilS = fminf(1.0f, tendrilS + dt * TENDRIL_IN * (0.4f + activity));
  } else {
    tendrilS = fmaxf(0.0f, tendrilS - dt * TENDRIL_OUT);
  }

  // ---- 生き物: 重たく、のたうつように動く
  float speed = 0.5f + 0.8f * activity + 0.6f * fear;
  const float limX = 0.70f * MX, limY = 0.62f * MY;
  for (int n = 0; n < NLUMP; ++n) {
    Lump& L = lumps[n];
    L.theta += dt * speed * (0.6f * sinf(simT * 0.23f + L.phase) + 0.5f * sinf(simT * 0.51f + L.phase * 1.7f));
    float fx = cosf(L.theta) * LUMP_FORCE * speed;
    float fy = sinf(L.theta) * LUMP_FORCE * speed;

    float dx = L.x - MX, dy = L.y - MY;               // 画面の内側にとどまる
    if (dx >  limX) fx -= (dx - limX) * 2.0f;
    if (dx < -limX) fx -= (dx + limX) * 2.0f;
    if (dy >  limY) fy -= (dy - limY) * 2.0f;
    if (dy < -limY) fy -= (dy + limY) * 2.0f;

    for (int m = 0; m < NLUMP; ++m) {                // くっつきすぎない(少し重なるのはOK)
      if (m == n) continue;
      float ox = L.x - lumps[m].x, oy = L.y - lumps[m].y;
      float d = sqrtf(ox * ox + oy * oy) + 0.001f;
      float want = (L.sig + lumps[m].sig) * 0.7f;
      if (d < want) { fx += ox / d * (want - d) * 0.8f; fy += oy / d * (want - d) * 0.8f; }
    }

    if (n == tendrilLump && tendrilS > 0) {          // 腕を伸ばした子は、指のほうへ体ごと寄る
      float tx = tendrilTx - L.x, ty = tendrilTy - L.y;
      float d = sqrtf(tx * tx + ty * ty) + 0.001f;
      if (d > 35) { fx += tx / d * 5.0f * tendrilS; fy += ty / d * 5.0f * tendrilS; }
    }

    float damp = expf(-dt * LUMP_DRAG);
    L.vx = (L.vx + fx * dt) * damp;
    L.vy = (L.vy + fy * dt) * damp;
    L.x += L.vx * dt; L.y += L.vy * dt;
  }

  // ---- 表面にときどき何かがぬるっと浮かんで、沈む
  bubbleTimer -= dt;
  if (!bubble.on && bubbleTimer < 0) {
    bubble.on = true; bubble.t = 0;
    bubble.T   = 3.5f + frand() * 3.0f;
    bubble.x   = MX + (frand() - 0.5f) * 1.2f * MX;
    bubble.y   = MY + (frand() - 0.5f) * 1.2f * MY;
    bubble.amp = 5.0f + frand() * 5.0f;
    bubble.sig = 7.0f + frand() * 6.0f;
    bubbleTimer = (6.0f + frand() * 8.0f) * (1.0f - 0.4f * activity);
  }

  // ---- 高さ = ふくらんだ膜 + ねっとり変形 + 生き物 + 触手
  float domeDepth = 14.0f + 2.0f * breath + 1.5f * hb;
  for (int i = 0; i < GN; ++i) hField[i] = -domeDepth * dome[i] + gooA[i];

  float gain = wake * (1.0f + 0.10f * breath + 0.05f * hb) * (1.0f - 0.35f * fear);
  for (int n = 0; n < NLUMP; ++n) {
    Lump& L = lumps[n];
    float amp = L.baseAmp * gain * (0.85f + 0.15f * sinf(simT * 0.7f + L.phase));
    float sig = L.sig * (1.0f + 0.07f * breath);
    float v = sqrtf(L.vx * L.vx + L.vy * L.vy);
    float stretch = fminf(1.3f, v * 0.06f);          // 速く動くほど、ねばっと伸びる
    float dirX = v > 0.01f ? L.vx / v : 1.0f, dirY = v > 0.01f ? L.vy / v : 0.0f;
    addStretchedBlob(hField, L.x, L.y, amp, sig, dirX, dirY, stretch);
    // 少し遅れてついてくる「しっぽ」で、ぬめっとした余韻
    addBlob(hField, L.x - L.vx * 0.35f, L.y - L.vy * 0.35f, amp * 0.35f, sig * 0.8f);
  }

  if (tendrilLump >= 0 && tendrilS > 0.01f) {        // 触手を描く
    Lump& L = lumps[tendrilLump];
    float e = tendrilS * tendrilS * (3 - 2 * tendrilS);   // なめらかに伸び縮み
    float tipX = L.x + (tendrilTx - L.x) * e, tipY = L.y + (tendrilTy - L.y) * e;
    float ax = tipX - L.x, ay = tipY - L.y;
    float len = sqrtf(ax * ax + ay * ay);
    if (len > 4.0f) {
      float nx = -ay / len, ny = ax / len;
      int beads = (int)fminf(26.0f, fmaxf(5.0f, len / 3.5f));
      float wob = 5.0f * sinf(simT * 1.1f) + 3.0f * sinf(simT * 2.3f + 1.0f);
      for (int b = 1; b <= beads; ++b) {
        float t = (float)b / beads;
        float bend = sinf(t * 3.1416f) * wob;
        float bx = L.x + ax * t + nx * bend;
        float by = L.y + ay * t + ny * bend;
        float thick = L.sig * (0.55f - 0.30f * t) + 3.5f;
        float a = L.baseAmp * gain * (0.75f - 0.35f * t) * (0.6f + 0.4f * e);
        if (b == beads) { thick *= 1.3f; a *= 1.2f; }    // 先っぽは少しふくらむ
        addBlob(hField, bx, by, a, thick);
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
    for (int x = 0; x < GX; ++x) {
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
  for (int gy = 0; gy < CY_N; ++gy) {
    const UV* r0 = uv + gy * GX;
    const UV* r1 = r0 + GX;
    uint16_t* row0 = strip[s];
    uint16_t* row1 = row0 + W;
    for (int cx = 0; cx < CX_N; ++cx) {
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
static void fail(const char* msg) {
  M5.Display.setTextDatum(middle_center);
  M5.Display.drawString(msg, W / 2, H / 2);
  Serial.println(msg);
  while (true) delay(1000);
}

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);
  Serial.begin(115200);
  M5.Display.setRotation(1);                  // 横向き 320x240
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setBrightness(brightSteps[brightIdx]);

  env      = (uint16_t*)heap_caps_malloc(ENV * ENV * 2, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  strip[0] = (uint16_t*)heap_caps_malloc(W * 2 * 2, MALLOC_CAP_DMA);
  strip[1] = (uint16_t*)heap_caps_malloc(W * 2 * 2, MALLOC_CAP_DMA);
  uvBuf[0] = (UV*)ps_malloc(GN * sizeof(UV));
  uvBuf[1] = (UV*)ps_malloc(GN * sizeof(UV));
  hField   = (float*)ps_malloc(GN * sizeof(float));
  dome     = (float*)ps_malloc(GN * sizeof(float));
  gooA     = (float*)ps_calloc(GN, sizeof(float));
  gooB     = (float*)ps_calloc(GN, sizeof(float));
  if (!env || !strip[0] || !strip[1] || !uvBuf[0] || !uvBuf[1] ||
      !hField || !dome || !gooA || !gooB) {
    fail("Memory error (PSRAM?)");
  }

  for (int y = 0; y < GY; ++y)
    for (int x = 0; x < GX; ++x) {
      float dx = (x - MX) / RD, dy = (y - MY) / RD;
      dome[y * GX + x] = dx * dx + dy * dy;
    }
  for (int i = 0; i < 256; ++i) expLut[i] = expf(-i / 32.0f);

  for (auto& L : lumps) {
    L.x = MX + (frand() - 0.5f) * MX;
    L.y = MY + (frand() - 0.5f) * MY;
    L.vx = L.vy = 0;
    L.baseAmp = 8.0f + frand() * 5.0f;
    L.sig     = 13.0f + frand() * 8.0f;          // 大きめで、ぼてっと
    L.phase   = frand() * 6.2832f;
    L.theta   = frand() * 6.2832f;
  }

  buildEnv(theme);

  // 最初の1枚
  simIn = {false, false, 0, 0, 0, 0, 0.016f};
  simulate(simIn, uvBuf[front]);

  semGo   = xSemaphoreCreateBinary();
  semDone = xSemaphoreCreateBinary();
  xTaskCreatePinnedToCore(simTask, "sim", 8192, nullptr, 1, nullptr, 0);
}

void loop() {
  static uint32_t last = millis();
  static uint32_t fpsTimer = millis();
  static uint32_t lastTapMs = 0;
  static float prevX = 0, prevY = 0;
  static int frames = 0;

  M5.update();

  // 電源ボタン短押しで明るさ切り替え
  if (M5.BtnPWR.wasClicked()) {
    brightIdx = (brightIdx + 1) % sizeof(brightSteps);
    M5.Display.setBrightness(brightSteps[brightIdx]);
  }

  auto t = M5.Touch.getDetail();
  uint32_t now = millis();
  bool pressed = t.wasPressed();

  // ダブルタップで景色を切り替え
  if (pressed) {
    if (now - lastTapMs < 350) {
      theme = (theme + 1) % NTHEME;
      buildEnv(theme);
      lastTapMs = 0;
    } else {
      lastTapMs = now;
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
