// NullHorn  -  null² の夜、ラッパ形のくぼみの奥で光るLEDリングと、うねる鏡の膜
//   M5Stack StopWatch 用(丸い 466x466 画面)。CoreS3 系でも真ん中に正方形で動きます。
//
// 画面の中:
//   ・くぼみのふち寄りに、赤と青の光のリングが幾重にも重なって光る(2色が同時に出ます)
//   ・内側は黒い鏡の池。リングの映り込みが白熱した筋になって、ぬるぬる渦を巻く
//   ・色はリング1本ごとに赤←→青と移り変わり、その帯がゆっくり流れていきます
//   ・まわりは夜の金属。リングはゆっくり呼吸し、色はじわじわ移り変わる
//
// 触ると:
//   ・なぞる     : 光の筋が指について、ぬるっと流れる
//   ・押し続ける : 指のまわりで渦を巻き、筋がねじれていく
//   ・タップ     : パッと明るくなって、光の波が外へ広がる
//
// 操作:
//   StopWatch … ボタンA: 色の組(赤青 / 赤水色 / 琥珀青紫 / 桃青緑)  ボタンB: 明るさ
//   CoreS3系  … ダブルタップ: 色の組                            電源ボタン短押し: 明るさ
//
// 必要なもの:
//   ボードマネージャー M5Stack >= 3.3.7 / ボード: M5StopWatch(CoreS3 なら M5CoreS3)
//   ライブラリ M5Unified >= 0.2.15, M5GFX >= 0.2.21
//
// しくみ:
//   光のリングは「中心からの距離ごとの色」の表(LUT)にしておき、
//   1ピクセルごとに「うねりでずらした位置の、中心からの距離」を出して表を引くだけ。
//   膜のしわ(うねり)が距離をずらすので、リングが曲がって渦や筋になります。
//   くぼみの底ほど大きくうねるので、内側は液体金属のような marbling になります。
//   画面は8行ずつの帯に分けて、2つのコアで交互に描きます。
//   (1ピクセルあたりは 掛け算2回 + 足し算 + 表引き だけ。重い計算は全部1フレーム1回)

#include <M5Unified.h>
#include <math.h>

#pragma GCC optimize("O3")
#ifndef IRAM_ATTR
#define IRAM_ATTR
#endif

// ---- 画質 ------------------------------------------------------------------
// 0: おまかせ(高精細で始めて、最初の3秒が SMOOTH_FPS 未満なら自動で軽いほうへ)
// 1: いつも高精細(1ピクセルずつ)   2: いつも軽い(2x2ピクセルずつ)
static constexpr int   QUALITY    = 0;
static constexpr float SMOOTH_FPS = 24.0f;

// ---- 調整用 ----------------------------------------------------------------
static constexpr float FLOW       = 0.075f;  // 膜のうねりの大きさ(光の筋のくねり方)
static constexpr float FLOW_SPEED = 0.05f;   // うねりが流れる速さ
static constexpr float WRINKLE    = 0.55f;   // 細かいしわの強さ
static constexpr float DEEP       = 6.5f;    // 底(中心)がどれだけ大きくうねるか
static constexpr float STIR       = 1.2f;    // なぞったときに流れる強さ
static constexpr float SWIRL      = 2.4f;    // 押し続けたときの渦の速さ(rad/秒)
static constexpr float SWIRL_R    = 0.34f;   // 渦の広がり
static constexpr float SETTLE     = 0.32f;   // かき混ぜたあと、もどる速さ
static constexpr float RRIM       = 0.395f;  // くぼみのふちの半径(画面に対して)
static constexpr float RING_N     = 8.0f;    // 光のリングの本数
static constexpr float RING_P     = 1.7f;    // ふち寄りほどリングが密になる度合い
static constexpr float RING_SPEED = 0.13f;   // リングが外へ流れる速さ
static constexpr float BREATH     = 0.045f;  // リングがゆっくり呼吸する量
static constexpr float HUE_SWING  = 0.028f;  // 色がゆらぐ幅(0.0 で固定)
static constexpr float MIX_N      = 7.0f;    // 32本の中で2色が何回入れ替わるか(整数にすること)
static constexpr float MIX_SPEED  = 0.18f;   // 2色の帯が動く速さ
static constexpr float MIX_SHARP  = 1.7f;    // 2色の境目のはっきりさ(大きいほどくっきり)
static constexpr float MIX_BIAS   = 0.18f;   // + で1色目(赤)寄り、- で2色目(青)寄り
static constexpr float PULSE      = 1.0f;    // タップしたときの光の波の強さ
static constexpr float DRIFT      = 0.028f;  // 全体がゆっくり漂う量(手持ちカメラのような)
static constexpr float ELLIP      = 0.95f;   // 楕円っぽさ(のぞき込んだ遠近感)
// ---------------------------------------------------------------------------

static constexpr int LUTN   = 2048;          // 距離^2 ごとの色の表
static constexpr int RSHIFT = 14;            // (px*px+py*py) >> RSHIFT が表の番号
static constexpr int NR     = 1024;          // 距離(t)ごとの色の表
static constexpr float TMAX = 2.4f;          // t の最大(1.0 = くぼみのふち)
static constexpr int PROFN  = 256;           // リング1本ぶんの断面
static constexpr int NMIX   = 32;            // 色が一巡するリングの本数(2のべき乗)
static constexpr int NOISE  = 64;
static constexpr int CSTEP  = 8;             // うねりを計算する間隔(ピクセル)
static constexpr int SHSTEP = 3 + 4;         // log2(CSTEP) + 4  (Q8 の差 → 1px あたりの Q4)
static constexpr int BAND   = 8;
static constexpr int NSLOT  = 4;
static constexpr int NRIP   = 3;             // 同時に広がる光の波の数
static constexpr int RIPN   = 128;           // 光の波の断面

static int   W, H, S, XOFF, YOFF, CW, NB;
static uint8_t*  bandVis;                    // 画面からはみ出す帯は描かない
static int   PIXEL = 1;

static uint16_t* slotBuf[NSLOT];
static uint16_t* lut;                        // [LUTN] 距離^2 → 色
static uint16_t* ringCol;                    // [NR]   t     → 色
static uint16_t* rtabQ4;                     // [LUTN] 表番号 → 距離 (1/16px)
static uint32_t* phTab;                      // [NR]   リングの位相 (Q16)
static float*    ampC;  static float* ampH;  // [NR]   筋 / にじみ の強さ
static float*    addR;  static float* addG;  static float* addB;  // [NR] 池・ふち・金属
static float     profC[PROFN], profH[PROFN], profB[PROFN];        // リングの断面
static float     ripProf[RIPN];              // 光の波の断面
static int16_t*  spanS;
static int16_t*  spanE;
static int16_t*  bandX0;
static int16_t*  bandW;
static float*    noiseT;

static float*    dfX;  static float* dfY;    // 指でかき混ぜた分
static float*    tmpX; static float* tmpY;
static float*    wX;   static float* wY;     // このフレームのうねり
struct Scratch { int32_t* rx; int32_t* ry; };
static Scratch   scr[2];
static uint16_t  blackCol;

// フレームごとの共有値
static int32_t cxq, cyq;                     // 光の中心(1/16ピクセル。y は ELLIP 込み)
static uint32_t busyUs[2];

static SemaphoreHandle_t semGo, semDone;
static volatile int readyOdd = -1, doneUpTo = -1;

static float simT = 0;
static float fPX = 0, fPY = 0, holdT = 0;
static bool  fWas = false;
static float flash = 0;                      // タップの明るさ
struct Ripple { float t0, on; };
static Ripple rips[NRIP];
static int    ripNext = 0;

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
static Col hsv(float h, float s, float v) {
  h = fracf(h) * 6.0f; int i = (int)h; float f = h - i;
  float p = v * (1 - s), q = v * (1 - s * f), t = v * (1 - s * (1 - f));
  switch (i % 6) {
    case 0: return {v, t, p}; case 1: return {q, v, p}; case 2: return {p, v, t};
    case 3: return {p, q, v}; case 4: return {t, p, v}; default: return {v, p, q};
  }
}
// しわの下敷き(ノイズ)は、格子の上を等間隔に読むだけなので、
// 「どこを読むか」を1フレームに1回まとめて出しておく。毎回 floor しないぶん速い。
struct NMap { uint16_t* i0; uint16_t* i1; float* f; };
static NMap nmX[4], nmY[4];
static void buildNMap(NMap& m, float scale, float off, int n) {
  for (int g = 0; g < n; ++g) {
    float v = g * scale + off;
    float fl = floorf(v);
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

// 指のまわりの効き具合 exp(-u) を表にしておく(毎回 expf を呼ばないため)
static constexpr float GCUT = 9.3f * 0.5f;   // ここまで計算すれば g < 0.01
static float gLut[258];
static inline float gFall(float u) {          // u = 0..GCUT -> exp(-u)
  float q = u * (256.0f / GCUT);
  int i = (int)q; if (i > 255) i = 255;
  float fr = q - i;
  return gLut[i] + (gLut[i + 1] - gLut[i]) * fr;
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

// ---------------------------------------------------------------- 色の組み合わせ
struct Theme {
  float hueA, hueB;     // 同時に出る2色(リング1本ごとに混ざる)
  float satA, satB;
  Col   rim;            // ふちのハイライト
  Col   metal;          // まわりの金属
  Col   glow;           // 池にたまる照り返し
  float pool;
  float halo;           // ふち寄りに広がる光のもや
};
static const Theme THEMES[] = {
  // 赤 × 青 (動画の雰囲気。赤が多めで、ところどころ青いリング)
  {0.005f, 0.605f, 0.95f, 0.92f, {0.55f, 0.78f, 1.00f}, {0.105f, 0.098f, 0.125f}, {1.00f, 0.18f, 0.12f}, 0.14f, 0.40f},
  // 赤 × 水色 (半々。コントラストが強い)
  {0.985f, 0.520f, 0.92f, 0.85f, {0.80f, 0.95f, 1.00f}, {0.100f, 0.105f, 0.140f}, {1.00f, 0.25f, 0.35f}, 0.14f, 0.38f},
  // 琥珀 × 青紫
  {0.085f, 0.690f, 0.95f, 0.90f, {0.85f, 0.95f, 1.00f}, {0.120f, 0.100f, 0.080f}, {1.00f, 0.45f, 0.10f}, 0.14f, 0.40f},
  // 桃 × 青緑
  {0.930f, 0.455f, 0.80f, 0.75f, {0.90f, 0.98f, 1.00f}, {0.095f, 0.110f, 0.135f}, {0.60f, 0.35f, 1.00f}, 0.12f, 0.36f},
};
static constexpr int NTHEME = sizeof(THEMES) / sizeof(THEMES[0]);

// ------------------------------------------------- 形だけの表(色替えのときだけ作る)
static void buildStatic() {
  const Theme& T = THEMES[theme];
  for (int i = 0; i < PROFN; ++i) {
    float f = (i + 0.5f) / PROFN - 0.5f;             // リング1本の中での位置
    profC[i] = expf(-f * f * 22.0f);                // 白熱した細い筋
    profH[i] = expf(-f * f * 4.5f) * 0.55f;         // まわりのにじみ
    float d = f - 0.105f;
    profB[i] = expf(-d * d * 120.0f) * 0.80f;        // 筋の外側につく青い縁取り
  }
  for (int i = 0; i < RIPN; ++i) {
    float u = (i + 0.5f) / RIPN * 2.0f - 1.0f;       // -1..1
    ripProf[i] = expf(-u * u * 6.0f) * sinf(u * 4.2f);
  }
  const float dt = TMAX / NR;
  for (int j = 0; j < NR; ++j) {
    float t = j * dt;
    phTab[j] = (uint32_t)(int32_t)(RING_N * powf(t, RING_P) * 65536.0f);
    float band  = smooth3((t - 0.04f) / 0.10f) * (1.0f - smooth3((t - 0.97f) / 0.10f));
    float outer = smooth3((t - 0.55f) / 0.26f);      // ふち寄りだけ、もやっと光る
    ampC[j] = band * (0.80f + 0.50f * outer);
    ampH[j] = band * outer * 1.50f;

    float pool = expf(-t * t * 11.0f) * T.pool                    // 池にたまる照り返し
               + smooth3((t - 0.42f) / 0.25f) * (1.0f - smooth3((t - 1.00f) / 0.12f)) * T.halo;
    float rim  = expf(-(t - 1.04f) * (t - 1.04f) * 900.0f);      // ふちのハイライト
    float met  = 0;
    if (t > 0.99f) {                                             // 夜の金属: パネルの縞
      float m = (0.5f + 0.5f * sinf(t * 34.0f - 1.0f));
      m = m * m * m;
      met = (0.35f + 3.2f * m) * expf(-(t - 0.99f) * 2.6f);
    }
    addR[j] = T.glow.r * pool + T.rim.r * rim * 0.32f + T.metal.r * met;
    addG[j] = T.glow.g * pool + T.rim.g * rim * 0.32f + T.metal.g * met;
    addB[j] = T.glow.b * pool + T.rim.b * rim * 0.32f + T.metal.b * met;
  }
}

// ------------------------------------------------- 距離ごとの色(1フレームに1回)
static void buildLut() {
  const Theme& T = THEMES[theme];
  const float Rrim = RRIM * S * (1.0f + BREATH * sinf(simT * 0.35f));
  const float hueShift = HUE_SWING * sinf(simT * 0.055f);   // 色がゆっくり行き来する
  // リング1本ごとの色: 2色を行ったり来たりさせて、赤い帯と青い帯を作る
  const Col cA = hsv(T.hueA + hueShift, T.satA, 1.0f);
  const Col cB = hsv(T.hueB - hueShift, T.satB, 1.0f);
  Col ringMix[NMIX];
  {
    const float kk = 6.2831853f * MIX_N / NMIX, ph0 = simT * MIX_SPEED;
    for (int i = 0; i < NMIX; ++i) {
      float m = 0.5f + 0.5f * MIX_SHARP * sinf(i * kk + ph0) - MIX_BIAS;
      ringMix[i] = lerpC(cA, cB, smooth3(m));
    }
  }
  const uint32_t phase = (uint32_t)(int32_t)(simT * RING_SPEED * 65536.0f);
  const float fl = 1.0f + flash * 0.85f;

  // 広がっていく光の波
  float ripR[NRIP], ripA[NRIP], ripW[NRIP];
  int nrip = 0;
  for (int i = 0; i < NRIP; ++i) {
    if (rips[i].on <= 0) continue;
    float age = simT - rips[i].t0;
    if (age > 2.6f) { rips[i].on = 0; continue; }
    ripR[nrip] = age * 0.42f * S;                                // 外へ広がる
    ripA[nrip] = expf(-age * 1.05f) * PULSE * 2.6f * 65536.0f;   // 位相を押し広げる量
    ripW[nrip] = RIPN * 0.5f / (0.13f * S);                      // 波の幅
    ++nrip;
  }

  const float dt = TMAX / NR;
  const float dr = dt * Rrim;
  float rj = 0;
  for (int j = 0; j < NR; ++j, rj += dr) {
    uint32_t ph = phTab[j] - phase;
    if (nrip) {
      for (int k = 0; k < nrip; ++k) {
        float u = (rj - ripR[k]) * ripW[k] + RIPN * 0.5f;
        int ui = (int)u;
        if ((unsigned)ui < (unsigned)RIPN) ph += (uint32_t)(int32_t)(ripProf[ui] * ripA[k]);
      }
    }
    const int pi = (int)((ph >> 8) & (PROFN - 1));
    const float core = profC[pi], halo = profH[pi], fringe = profB[pi];
    const float ac = ampC[j], ah = ampH[j];
    const float lit = core * ac, haze = halo * ah;

    const Col base = ringMix[(ph >> 16) & (NMIX - 1)];   // このリングの色
    const float w = core * core * 0.62f;                                // 筋の芯は白熱する
    const float v = lit + haze;
    const float fb = fringe * ac;
    Col c = {(base.r + (1.0f - base.r) * w) * v,
             (base.g + (1.0f - base.g) * w) * v + fb * 0.32f,
             (base.b + (1.0f - base.b) * w) * v + fb * 0.90f};
    c.r += addR[j]; c.g += addG[j]; c.b += addB[j];
    ringCol[j] = toPix({c.r * fl, c.g * fl, c.b * fl});
  }

  // t の表 → 距離^2 の表へ並べ替え(1ピクセルずつの計算はこれを引くだけ)
  const int32_t kq = (int32_t)((NR / (TMAX * Rrim * 16.0f)) * 65536.0f);
  uint16_t* const lp = lut;
  const uint16_t* const rc = ringCol;
  for (int i = 0; i < LUTN; ++i) {
    int32_t j = ((int32_t)rtabQ4[i] * kq) >> 16;
    if (j >= NR) j = NR - 1;
    lp[i] = rc[j];
  }
}

// ---------------------------------------------------------------- 1フレームの準備
struct Input { bool touching; bool tapped; float tx, ty; float dt; };

static void updateFrame(const Input& in) {
  float dt = in.dt > 0.05f ? 0.05f : in.dt;
  simT += dt;

  // 全体がゆっくり漂う(手持ちカメラのように)
  float cx = S * 0.5f + DRIFT * S * sinf(simT * 0.11f) + 0.4f * DRIFT * S * sinf(simT * 0.27f + 1.0f);
  float cy = S * 0.5f + DRIFT * S * sinf(simT * 0.09f + 2.0f) + 0.4f * DRIFT * S * sinf(simT * 0.23f);
  cxq = (int32_t)(cx * 16.0f); cyq = (int32_t)(cy * 16.0f * ELLIP);

  // 指
  float fvx = 0, fvy = 0;
  if (in.touching) {
    if (fWas) { fvx = (in.tx - fPX) / fmaxf(dt, 0.001f); fvy = (in.ty - fPY) / fmaxf(dt, 0.001f); }
    holdT += dt;
  } else holdT = 0;
  fPX = in.tx; fPY = in.ty; fWas = in.touching;

  if (in.tapped) {                                   // タップ: 光の波と、パッと明るく
    rips[ripNext].t0 = simT; rips[ripNext].on = 1;
    ripNext = (ripNext + 1) % NRIP;
    flash = 1.0f;
  }
  flash *= expf(-dt * 3.0f);

  // ---- 指でかき混ぜた分(粗い格子)
  float relax = expf(-dt * SETTLE);
  float rad = 0.17f * S, inv2 = 1.0f / (2 * rad * rad);
  float maxD = 0.26f * S;
  float swirlStep = in.touching ? SWIRL * smooth3(holdT / 0.6f) * dt : 0.0f;
  float swirlR = SWIRL_R * S, swirlInv = 1.0f / (swirlR * swirlR);
  float swirlSat = 1.0f / (0.22f * S * 0.22f * S);
  float farCut = 9.3f * rad * rad;                 // これより遠いと g < 0.01 になる
  for (int gy = 0; gy < CW; ++gy)
    for (int gx = 0; gx < CW; ++gx) {
      int q = gy * CW + gx;
      float x = gx * CSTEP, y = gy * CSTEP;
      float dx = dfX[q] * relax, dy = dfY[q] * relax;
      if (in.touching) {
        float rx = x - in.tx, ry = y - in.ty;
        if (swirlStep > 0) {                         // 渦: 中心ほど速く回る
          float sx = rx - dx, sy = ry - dy;
          float th = swirlStep / (1.0f + (sx * sx + sy * sy) * swirlInv);
          th /= 1.0f + (dx * dx + dy * dy) * swirlSat;
          float iv = 1.0f / sqrtf(1.0f + th * th);
          float c = iv, sn = th * iv;
          float nx = c * sx + sn * sy, ny = c * sy - sn * sx;
          dx = rx - nx; dy = ry - ny;
        }
        float d2 = rx * rx + ry * ry;
        if (d2 < farCut) {                           // 遠い所は効かないので計算しない
          float g = gFall(d2 * inv2);
          if (g > 0.01f) { dx += fvx * dt * STIR * g; dy += fvy * dt * STIR * g; }
        }
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

  // ---- 膜のしわ = ゆったりしたうねり + 細かいしわ − かき混ぜた分
  //      くぼみの底(中心)ほど大きくうねる
  float amp = FLOW * S, sc = 7.0f / S * CSTEP, tt = simT * FLOW_SPEED * 10.0f;
  float rr2 = 1.0f / (RRIM * S * RRIM * S);
  buildNMap(nmX[0], sc,          tt,        CW);  buildNMap(nmY[0], sc,          0,          CW);
  buildNMap(nmX[1], sc * 2.8f,   31,        CW);  buildNMap(nmY[1], sc * 2.8f,  -tt * 1.3f,  CW);
  buildNMap(nmX[2], sc,         -17,        CW);  buildNMap(nmY[2], sc,          tt,         CW);
  buildNMap(nmX[3], sc * 2.8f,   tt * 1.1f, CW);  buildNMap(nmY[3], sc * 2.8f,   11,         CW);
  for (int gy = 0; gy < CW; ++gy)
    for (int gx = 0; gx < CW; ++gx) {
      int q = gy * CW + gx;
      float x = gx * CSTEP, y = gy * CSTEP;
      float nx = noiseAt(nmX[0], nmY[0], gx, gy) + WRINKLE * noiseAt(nmX[1], nmY[1], gx, gy);
      float ny = noiseAt(nmX[2], nmY[2], gx, gy) + WRINKLE * noiseAt(nmX[3], nmY[3], gx, gy);
      float dx = x - cx, dy = y - cy;
      float t2 = (dx * dx + dy * dy) * rr2;         // 中心からの距離^2(1.0 = ふち)
      float k = DEEP / (1.0f + t2 * t2 * t2 * 220.0f) + 0.06f;
      tmpX[q] = amp * k * nx - dfX[q];
      tmpY[q] = (amp * k * ny - dfY[q]) * ELLIP;
    }
  // うねりをひとなでして、急すぎる傾きを落とす(細かいモアレ防止)
  for (int gy = 0; gy < CW; ++gy)
    for (int gx = 0; gx < CW; ++gx) {
      int q = gy * CW + gx;
      int l = gx > 0 ? q - 1 : q, r = gx < CW - 1 ? q + 1 : q;
      int u = gy > 0 ? q - CW : q, d = gy < CW - 1 ? q + CW : q;
      wX[q] = tmpX[q] * 0.5f + (tmpX[l] + tmpX[r] + tmpX[u] + tmpX[d]) * 0.125f;
      wY[q] = tmpY[q] * 0.5f + (tmpY[l] + tmpY[r] + tmpY[u] + tmpY[d]) * 0.125f;
    }

  buildLut();
}

// ---------------------------------------------------------------- 描画(帯ごと)
template <int P>
static inline __attribute__((always_inline)) void renderBandT(int k, uint16_t* buf, Scratch& sc) {
  const int y0 = k * BAND, y1 = min(S, y0 + BAND);
  const int bx0 = bandX0[k], bw = bandW[k], bx1 = bx0 + bw - 1;
  const uint16_t* const lp = lut;
  for (int y = y0; y < y1; y += P) {
    uint16_t* row = buf + (y - y0) * bw;
    int xs = max((int)spanS[y], bx0), xe = min((int)spanE[y], bx1);
    if (P == 2) xs &= ~1;
    for (int x = bx0; x < xs; ++x) row[x - bx0] = blackCol;
    for (int x = max(xe + 1, bx0); x <= bx1; ++x) row[x - bx0] = blackCol;
    if (xs <= xe) {
      // この行のうねり(粗い格子を縦に補間。1/256ピクセル単位)
      int gy = y / CSTEP; float fy = (y - gy * CSTEP) * (1.0f / CSTEP);
      const float *ax = wX + gy * CW, *ay = wY + gy * CW;
      for (int gx = 0; gx < CW - 1; ++gx) {
        sc.rx[gx] = (int32_t)((ax[gx] + (ax[gx + CW] - ax[gx]) * fy) * 256.0f);
        sc.ry[gx] = (int32_t)((ay[gx] + (ay[gx + CW] - ay[gx]) * fy) * 256.0f);
      }
      const int32_t yq = (int32_t)(y * 16.0f * ELLIP);
      int x = xs;
      while (x <= xe) {
        const int gx = x / CSTEP;
        const int segEnd = min(xe, gx * CSTEP + CSTEP - 1);
        // 1ピクセルあたりの傾き: 節(CSTEP px ごと)の差を 1/CSTEP にして Q8→Q4
        const int32_t stepx = (sc.rx[gx + 1] - sc.rx[gx] + (1 << (SHSTEP - 1))) >> SHSTEP;
        const int32_t stepy = (sc.ry[gx + 1] - sc.ry[gx] + (1 << (SHSTEP - 1))) >> SHSTEP;
        const int off = x - gx * CSTEP;
        int32_t px = (x << 4) + (sc.rx[gx] >> 4) + stepx * off - cxq;   // 光の中心から見た位置
        int32_t py = yq + (sc.ry[gx] >> 4) + stepy * off - cyq;
        const int32_t dPx = (P << 4) + stepx * P, dPy = stepy * P;
        for (; x <= segEnd; x += P, px += dPx, py += dPy) {
          uint32_t idx = (uint32_t)(px * px + py * py) >> RSHIFT;       // 中心からの距離^2
          if (idx >= LUTN) idx = LUTN - 1;
          uint16_t col = lp[idx];
          row[x - bx0] = col;
          if (P == 2 && x + 1 <= bx1) row[x + 1 - bx0] = col;
        }
      }
    }
    if (P == 2 && y + 1 < y1) memcpy(row + bw, row, bw * sizeof(uint16_t));
  }
}

static void __attribute__((noinline)) IRAM_ATTR renderBandHD(int k, uint16_t* b, Scratch& s)    { renderBandT<1>(k, b, s); }
static void __attribute__((noinline)) IRAM_ATTR renderBandLight(int k, uint16_t* b, Scratch& s) { renderBandT<2>(k, b, s); }
static inline void renderBand(int k, uint16_t* b, Scratch& s) {
  if (PIXEL == 1) renderBandHD(k, b, s); else renderBandLight(k, b, s);
}

// コア0: 奇数番目の帯
static void workerTask(void*) {
  for (;;) {
    xSemaphoreTake(semGo, portMAX_DELAY);
    uint32_t busy = 0;
    for (int k = 1; k < NB; k += 2) {
      while (__atomic_load_n(&doneUpTo, __ATOMIC_ACQUIRE) < k - NSLOT) { }
      if (bandVis[k]) {
        uint32_t t0 = micros();
        renderBand(k, slotBuf[k % NSLOT], scr[0]);
        busy += micros() - t0;
      }
      __atomic_store_n(&readyOdd, k, __ATOMIC_RELEASE);
    }
    busyUs[0] = busy;
    xSemaphoreGive(semDone);
  }
}

// コア1: 偶数番目の帯を描き、順番に画面へ送る
static void renderFrame() {
  __atomic_store_n(&readyOdd, -1, __ATOMIC_RELEASE);
  __atomic_store_n(&doneUpTo, -1, __ATOMIC_RELEASE);
  xSemaphoreGive(semGo);
  uint32_t busy = 0;
  M5.Display.startWrite();
  for (int k = 0; k < NB; ++k) {
    if ((k & 1) == 0) {
      if (bandVis[k]) {
        uint32_t t0 = micros();
        renderBand(k, slotBuf[k % NSLOT], scr[1]);
        busy += micros() - t0;
      }
    } else {
      while (__atomic_load_n(&readyOdd, __ATOMIC_ACQUIRE) < k) { }
    }
    M5.Display.waitDMA();
    __atomic_store_n(&doneUpTo, k - 1, __ATOMIC_RELEASE);
    if (bandVis[k]) {
      int y0 = k * BAND, h = min(BAND, S - y0);
      M5.Display.pushImageDMA(XOFF + bandX0[k], YOFF + y0, bandW[k], h, slotBuf[k % NSLOT]);
    }
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
  // 丸い画面はそのまま、四角い画面は短辺からはみ出させて画面いっぱいに使う
  S = ((W == H) ? W : max(W, H)) & ~1;
  XOFF = (W - S) / 2;
  YOFF = ((H - S) / 2 / BAND) * BAND;
  CW = S / CSTEP + 3;
  NB = (S + BAND - 1) / BAND;
  PIXEL = (QUALITY == 2) ? 2 : 1;
  Serial.printf("screen %dx%d  area %d  quality %d\n", W, H, S, QUALITY);

  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setBrightness(brightSteps[brightIdx]);

  noiseT = (float*)malloc(NOISE * NOISE * sizeof(float));
  for (int i = 0; i < NSLOT; ++i) slotBuf[i] = (uint16_t*)heap_caps_malloc(S * BAND * 2, MALLOC_CAP_DMA);
  lut     = (uint16_t*)malloc(LUTN * sizeof(uint16_t));
  rtabQ4  = (uint16_t*)malloc(LUTN * sizeof(uint16_t));
  ringCol = (uint16_t*)malloc(NR * sizeof(uint16_t));
  phTab   = (uint32_t*)malloc(NR * sizeof(uint32_t));
  ampC    = (float*)malloc(NR * sizeof(float)); ampH = (float*)malloc(NR * sizeof(float));
  addR    = (float*)malloc(NR * sizeof(float)); addG = (float*)malloc(NR * sizeof(float));
  addB    = (float*)malloc(NR * sizeof(float));
  spanS  = (int16_t*)malloc(S * sizeof(int16_t)); spanE = (int16_t*)malloc(S * sizeof(int16_t));
  bandX0 = (int16_t*)malloc(NB * sizeof(int16_t)); bandW = (int16_t*)malloc(NB * sizeof(int16_t));
  bandVis = (uint8_t*)malloc(NB);
  int CN = CW * CW;
  dfX = (float*)calloc(CN, sizeof(float)); dfY = (float*)calloc(CN, sizeof(float));
  tmpX = (float*)calloc(CN, sizeof(float)); tmpY = (float*)calloc(CN, sizeof(float));
  wX = (float*)calloc(CN, sizeof(float)); wY = (float*)calloc(CN, sizeof(float));
  for (int i = 0; i < 4; ++i) {
    nmX[i].i0 = (uint16_t*)malloc(CW * 2); nmX[i].i1 = (uint16_t*)malloc(CW * 2);
    nmX[i].f  = (float*)malloc(CW * 4);
    nmY[i].i0 = (uint16_t*)malloc(CW * 2); nmY[i].i1 = (uint16_t*)malloc(CW * 2);
    nmY[i].f  = (float*)malloc(CW * 4);
  }
  for (int c = 0; c < 2; ++c) {
    scr[c].rx = (int32_t*)malloc(CW * sizeof(int32_t));
    scr[c].ry = (int32_t*)malloc(CW * sizeof(int32_t));
  }
  bool ok = noiseT && lut && rtabQ4 && ringCol && phTab && ampC && ampH && addR && addG && addB &&
            spanS && spanE && bandX0 && bandW && bandVis && dfX && dfY && tmpX && tmpY && wX && wY &&
            scr[0].rx && scr[0].ry && scr[1].rx && scr[1].ry;
  for (int i = 0; i < NSLOT; ++i) ok = ok && slotBuf[i];
  for (int i = 0; i < 4; ++i) ok = ok && nmX[i].i0 && nmX[i].i1 && nmX[i].f && nmY[i].i0 && nmY[i].i1 && nmY[i].f;
  if (!ok) fail("Memory error");

  blackCol = toPix({0, 0, 0});
  bool round = (W == H);
  float c0 = (S - 1) * 0.5f, rr = S * 0.5f + 1.0f;
  for (int y = 0; y < S; ++y) {
    if (!round) { spanS[y] = 0; spanE[y] = S - 1; continue; }
    float dy = y - c0, w = rr * rr - dy * dy;
    if (w <= 0) { spanS[y] = 1; spanE[y] = 0; continue; }
    float s = sqrtf(w);
    spanS[y] = max(0, (int)floorf(c0 - s)); spanE[y] = min(S - 1, (int)ceilf(c0 + s));
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

  // 表番号 → 中心からの距離(1/16ピクセル)
  for (int i = 0; i < LUTN; ++i) {
    float r = sqrtf((float)i * (float)(1 << RSHIFT) / 256.0f);
    rtabQ4[i] = (uint16_t)(r * 16.0f + 0.5f);
  }

  for (int i = 0; i <= 257; ++i) gLut[i] = expf(-(i * (GCUT / 256.0f)));
  makeNoise(noiseT, 3);
  for (int i = 0; i < NRIP; ++i) rips[i].on = 0;
  buildStatic();

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
    if (M5.BtnA.wasPressed()) { theme = (theme + 1) % NTHEME; buildStatic(); }
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
      if (now - lastTapMs < 350) { theme = (theme + 1) % NTHEME; buildStatic(); lastTapMs = 0; }
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
    Serial.printf("fps: %.1f  (%s)  core0 %.1f ms  core1 %.1f ms\n", fps, PIXEL == 1 ? "HD" : "light",
                  busyUs[0] / 1000.0f, busyUs[1] / 1000.0f);
    static int checks = 0;
    if (QUALITY == 0 && PIXEL == 1 && ++checks == 1 && fps < SMOOTH_FPS) {
      PIXEL = 2;
      Serial.println("-> switched to light mode");
    }
    frames = 0; fpsTimer = millis();
  }
}
