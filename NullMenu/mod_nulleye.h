// NullEye  -  NullMenu 用モジュール(元のスケッチを名前空間に入れただけ)
#pragma once

namespace NullEye {

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
// NullEye  -  null² の夜、宙に浮く鏡の立方体と、その真ん中で光る眼
//   M5Stack StopWatch 用(丸い 466x466 画面)。CoreS3 系でも画面いっぱいに動きます。
//
// 画面の中:
//   ・角をこちらに向けて浮かぶ、鏡の立方体。3つの面それぞれに違う景色が映り込む
//   ・膜がぶよぶよ波打って、映り込みが液体金属のように流れる
//   ・中央に白く光る眼。色は 青 → 白 → 赤 → 白 とゆっくり巡り、
//     ときどきまばたきします(たまに2回続けて)
//   ・背景は空。ボタンAで 朝 → 昼 → 夕方 → 夜 と移り変わります
//
// 触ると:
//   ・なぞる     : 膜が指についてぬるっと流れる。横に払うと立方体が回る
//   ・押し続ける : 指のまわりで渦を巻き、映り込みがねじれる
//   ・タップ     : 眼がまばたきし、パッと光って波が膜を走る
//
// 操作:
//   StopWatch … ボタンA 短押し: 時間帯(朝/昼/夕方/夜)  ボタンA 長押し: 灯りの色
//               ボタンB: 画面の明るさ
//   CoreS3系  … 電源ボタン 短押し: 時間帯          電源ボタン 長押し: 画面の明るさ
//               ダブルタップ: 灯りの色
//
// 必要なもの:
//   ボードマネージャー M5Stack >= 3.3.7 / ボード: M5StopWatch(CoreS3 なら M5CoreS3)
//   ライブラリ M5Unified >= 0.2.15, M5GFX >= 0.2.21
//
// しくみ:
//   まわりの景色を1枚の絵(256x128)にしておき、立方体の面ごとに
//   「画面の位置 → 景色のどこが映るか」を1次式で持ちます。
//   膜のうねりがその位置をずらすので、映り込みがぐにゃりと曲がります。
//   1ピクセルあたりは 足し算2回 + 絵の読み出しだけ。重い計算は全部1フレーム1回です。
//   画面は8行ずつの帯に分けて、2つのコアで交互に描きます。



// ---- 画質 ------------------------------------------------------------------
// 0: おまかせ(高精細で始めて、最初の3秒が SMOOTH_FPS 未満なら自動で軽いほうへ)
// 1: いつも高精細(1ピクセルずつ)   2: いつも軽い(2x2ピクセルずつ)
static constexpr int   QUALITY    = 0;
static constexpr float SMOOTH_FPS = 24.0f;

// ---- 調整用 ----------------------------------------------------------------
static constexpr float CUBE_FILL  = 1.26f;   // 立方体の大きさ(画面に対して)
static constexpr float SWING      = 0.42f;   // ひとりでにゆらゆら回る幅(ラジアン)
static constexpr float SWING_W    = 0.16f;   // ゆらゆらの速さ
static constexpr float TILT       = -0.55f;  // 見上げる角度(マイナスで下から見上げる)
static constexpr float LEAN       = 0.13f;   // 立方体の傾き
static constexpr float BOB        = 0.013f;  // ふわふわ浮く量
static constexpr float FLICK      = 0.00035f;// 横に払ったとき、どれだけ回るか(0 で回らない)
static constexpr float SPIN_DRAG  = 0.7f;    // 払ったあと止まるまでの速さ

static constexpr float FLOW       = 0.062f;  // 膜のうねりの大きさ
static constexpr float FLOW_SPEED = 0.05f;   // うねりが流れる速さ
static constexpr float WRINKLE    = 0.60f;   // 細かいしわの強さ
static constexpr float WSCALE     = 1.70f;   // うねり1pxで映り込みが何テクセルずれるか
static constexpr float STIR       = 1.3f;    // なぞったときに流れる強さ
static constexpr float SWIRL      = 2.2f;    // 押し続けたときの渦の速さ(rad/秒)
static constexpr float SWIRL_R    = 0.30f;   // 渦の広がり
static constexpr float SETTLE     = 0.35f;   // かき混ぜたあと、もどる速さ

static constexpr float EYE_R      = 0.135f;  // 眼(くぼみ込み)の大きさ
static constexpr float EYE_RING   = 4.2f;    // 眼の中の光の輪の本数
static constexpr float EYE_P      = 1.15f;    // ふち寄りほど輪が密になる度合い
static constexpr float EYE_SPEED  = 0.20f;   // 輪が外へ流れる速さ
static constexpr float EYE_BREATH = 0.06f;   // 眼がゆっくり呼吸する量
static constexpr float EYE_DEEP   = 2.20f;   // 眼がどれだけうねるか(眼の半径に対して)
static constexpr float EYE_EDGE   = 1.55f;   // うねりがふちの外まで届く割合(輪郭がグネグネする)
static constexpr float EYE_FINE   = 1.35f;   // 細かいうねりの強さ(グネグネの刻み)
static constexpr float EYE_DISC   = 1.20f;   // まばたきで隠れる範囲(眼の半径に対して)
static constexpr float EYE_SHADE  = 1.15f;   // 虹彩を流れる影の濃さ
static constexpr float EYE_CYCLE  = 0.055f;  // 色が 青 → 白 → 赤 → 白 と巡る速さ
static constexpr float BLINK_MIN  = 3.0f;    // まばたきの間隔(最短・秒)
static constexpr float BLINK_MAX  = 8.0f;    // まばたきの間隔(最長・秒)
static constexpr float BLINK_DUR  = 0.34f;   // まばたき1回にかかる時間
static constexpr float BLINK_TWIN = 0.25f;   // 2回続けてまばたきする割合
static constexpr float EYE_FLOW_S = 0.20f;   // うねりが流れる速さ
static constexpr float EYE_WOB    = 0.018f;  // 眼そのものがゆらゆら動く量
static constexpr float PULSE      = 1.0f;    // タップしたときの強さ
static constexpr float ENV_DRIFT  = 1.1f;    // 景色が流れる速さ(テクセル/秒)
static constexpr float LASER_SPD  = 0.11f;   // レーザーが振れる速さ
static constexpr float ENV_GAIN   = 0.95f;   // 映り込みの明るさ(全体のもと)
static constexpr float ENV_WHITE  = 3.4f;    // 明るいところの粘り(小さいほど白飛び)
static constexpr float ENV_AMB    = 0.030f;  // 暗いところに乗る薄明かり
static constexpr float REFLECT    = 2.6f;    // 1つの面がどれだけ広く景色を映すか
// ---------------------------------------------------------------------------

static constexpr int ENVW  = 256, ENVH = 128;   // まわりの景色
static constexpr int EYEN  = 640;               // 眼の色の表(距離^2 ごと)
static constexpr int PROFN = 256;
static constexpr int NOISE = 64;
static constexpr int CSTEP = 8;                 // うねりを計算する間隔(ピクセル)
static constexpr int SHSTEP = 3;                // log2(CSTEP): 節の差 → 1px あたり
static constexpr int BAND  = 8;
static constexpr int NSLOT = 4;
static constexpr int NRIP  = 3;
static constexpr int RIPN  = 128;
static constexpr int NLAS  = 4;                 // レーザーの本数
static constexpr int NFACE = 3;                 // 同時に見える面の数

static int   W, H, S, XOFF, YOFF, CW, NB;
static int   PIXEL = 1;

static uint16_t* slotBuf[NSLOT];
static uint16_t* envMap;                        // [ENVW*ENVH] まわりの景色
static uint16_t* eyeLut;                        // [EYEN]      眼の色
static uint8_t*  eyeSh;                         // [EYEN] 0=色を置く 1,2=下の映り込みを暗くする
static float*    eyeT;                          // [EYEN]      表番号 → 0..1 の距離
static float*    eyePow;                        // [EYEN]      輪の並び
static float*    eAmpC;  static float* eAmpH;   // [EYEN]      筋 / にじみ の強さ
static float*    eBaseT; static float* eSock; static float* eRim;
static float     profC[PROFN], profH[PROFN];
static float     ripProf[RIPN];
static int16_t*  spanS;  static int16_t* spanE;
static int16_t*  bandX0; static int16_t* bandW;
static uint8_t*  bandVis;
static uint16_t* rowSky;                        // [S] 行ごとの空の色
static float*    noiseT;
static float*    dfX;  static float* dfY;
static float*    tmpX; static float* tmpY;
static float*    wX;   static float* wY;
struct Scratch { int32_t* rx; int32_t* ry; };
static Scratch   scr[2];
static uint16_t  blackCol;

// 面ごと: 画面の行ごとの左右端と、映り込みの1次式
struct FaceRow { int16_t* x0; int16_t* x1; };
static FaceRow   fRow[NFACE];
static int32_t   fU0[NFACE], fV0[NFACE];        // 行の左端(x=0)での値 (Q8)
static int32_t   fDU[NFACE], fDV[NFACE];        // x が1増えたときの差 (Q8)
static int32_t   fEU[NFACE], fEV[NFACE];        // y が1増えたときの差 (Q8)
static int       nvis = 0;
static float     warpMax = 0;                   // このフレームのうねりの最大ずれ(px)
static int16_t*  cubeX0; static int16_t* cubeX1; // [S] 立方体の輪郭(行ごとの左右端)
static int16_t   eyeX0i, eyeY0i, eyeR2i;        // 眼の位置と大きさ
static int32_t   ecxq, ecyq;                    // 眼の中心 (1/16 px)
static uint32_t  eyeK;                          // 距離^2 → 表番号
static int32_t   lasX[NLAS], lasD[NLAS];        // レーザー (Q8)
static uint16_t  lasCol[NLAS];

static uint32_t busyUs[2];
static SemaphoreHandle_t semGo, semDone;
static volatile int readyOdd = -1, doneUpTo = -1;

static float simT = 0, yaw = 0, yawV = 0;
// まばたき: 0 = 開いている, 1 = 閉じきっている
static float blinkPos = 0, blinkT0 = 0, blinkNext = 2.5f;
static int   blinkOn = 0, blinkTwin = 0;
static int32_t lidQ = -32767;                   // まぶたのふち(1/16 px。眼の中心から下向き)
static int32_t eyeDiscQ2 = 0;                   // 眼まるごとの円盤の半径^2(1/16 px)
static uint16_t lidCol = 0;                     // まぶたのふちの光
static float fPX = 0, fPY = 0, holdT = 0;
static bool  fWas = false;
static float flash = 0;
struct Ripple { float t0, on; };
static Ripple rips[NRIP];
static int    ripNext = 0;

static int theme = 0;
static const uint8_t brightSteps[] = {60, 120, 190, 255};
static int brightIdx = 2;
// ボタンA: 時間帯。押すたびに 朝 → 昼 → 夕方 → 夜 と変わります
static int todIdx = 3;                          // 3 = 夜(写真の雰囲気)から始める

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

// しわの下敷き(ノイズ)は等間隔に読むだけなので、読む場所を1フレーム1回まとめて出す
struct NMap { uint16_t* i0; uint16_t* i1; float* f; };
static NMap nmX[8], nmY[8];
static void buildNMap(NMap& m, float scale, float off, int n) {
  for (int g = 0; g < n; ++g) {
    float v = g * scale + off, fl = floorf(v);
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
static constexpr float GCUT = 9.3f * 0.5f;
static float gLut[258];
static inline float gFall(float u) {
  float q = u * (256.0f / GCUT);
  int i = (int)q; if (i > 255) i = 255;
  return gLut[i] + (gLut[i + 1] - gLut[i]) * (q - i);
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

// ---------------------------------------------------------------- 時間帯
// ボタンA を押すたびに 朝 → 昼 → 夕方 → 夜 と変わります。
// 空・太陽・雲・会場の灯りが丸ごと入れ替わるので、明るさも雰囲気も変わります。
struct TimeOfDay {
  Col   skyTop, skyHor, ground, cloud;   // 空のてっぺん / 地平線 / 地面 / 雲
  Col   sun;                             // 太陽(または月)の色
  float sunV, sunSize, sunBr;            // 高さ(0=真上 0.5=地平線) / 大きさ / 明るさ
  float clouds;                          // 雲の量
  float lights;                          // 会場の灯りの強さ(昼は消えている)
  float lasers;                          // レーザーの強さ
  float gain;                            // 全体の明るさ
};
static const TimeOfDay TODS[] = {
  // 朝  … 澄んだ青と、低い朝日。うっすら雲
  {{0.09f,0.15f,0.33f},{0.92f,0.66f,0.55f},{0.14f,0.15f,0.19f},{1.00f,0.88f,0.82f},
   {1.00f,0.74f,0.45f}, 0.455f, 8.0f, 3.4f, 14, 0.12f, 0.00f, 1.25f},
  // 昼  … まぶしい青空と白い雲。灯りは消えている
  {{0.14f,0.33f,0.74f},{0.70f,0.83f,0.97f},{0.30f,0.32f,0.31f},{1.00f,1.00f,1.00f},
   {1.00f,0.98f,0.92f}, 0.140f, 5.0f, 5.5f, 22, 0.00f, 0.00f, 1.60f},
  // 夕方… 焼けた空。灯りがともりはじめる
  {{0.17f,0.09f,0.33f},{1.00f,0.40f,0.20f},{0.12f,0.09f,0.13f},{0.95f,0.58f,0.56f},
   {1.00f,0.42f,0.14f}, 0.495f, 10.0f, 4.2f, 17, 0.45f, 0.18f, 1.20f},
  // 夜  … 写真の雰囲気。灯りとレーザーだけ
  {{0.018f,0.026f,0.062f},{0.055f,0.065f,0.135f},{0.045f,0.040f,0.060f},{0.05f,0.05f,0.09f},
   {0.80f,0.85f,1.00f}, 0.120f, 3.0f, 0.5f, 0, 1.00f, 1.00f, 1.00f},
};
static constexpr int NTOD = sizeof(TODS) / sizeof(TODS[0]);

// ---------------------------------------------------------------- 灯りの色づかい
struct Theme {
  float hues[5];        // 会場の灯りの色
  Col   laser;
  float eyeHueA;        // 眼の色。この2色のあいだを、白を通って行き来します
  float eyeHueB;
};
static const Theme THEMES[] = {
  // 万博(写真の雰囲気): 黄緑・水色・紅紫・金・青
  {{0.22f, 0.50f, 0.88f, 0.13f, 0.62f}, {0.35f, 0.85f, 1.00f}, 0.570f, 0.005f},
  // 青緑
  {{0.47f, 0.54f, 0.42f, 0.60f, 0.50f}, {0.45f, 0.95f, 1.00f}, 0.480f, 0.630f},
  // 赤金
  {{0.02f, 0.09f, 0.96f, 0.13f, 0.05f}, {1.00f, 0.55f, 0.30f}, 0.020f, 0.110f},
  // 白銀
  {{0.58f, 0.62f, 0.54f, 0.08f, 0.60f}, {0.75f, 0.90f, 1.00f}, 0.560f, 0.660f},
};
static constexpr int NTHEME = sizeof(THEMES) / sizeof(THEMES[0]);

// ------------------------------------------------- まわりの景色を1枚の絵にする
// 上が空、まん中が地平線と会場、下が地面。立方体はこの絵を映します。
static inline void envAdd(float* buf, int u, int v, const Col& c, float w) {
  if (v < 0 || v >= ENVH || w <= 0) return;
  int i = (v * ENVW + ((u + ENVW * 8) & (ENVW - 1))) * 3;
  buf[i] += c.r * w; buf[i + 1] += c.g * w; buf[i + 2] += c.b * w;
}
static void buildEnv() {
  const Theme& T = THEMES[theme];
  const TimeOfDay& D = TODS[todIdx];
  float* buf = (float*)malloc(ENVW * ENVH * 3 * sizeof(float));
  if (!buf) return;

  // --- 空と地面のグラデーション
  for (int v = 0; v < ENVH; ++v) {
    float t = (v + 0.5f) / ENVH;                       // 0 = 真上, 0.5 = 地平線, 1 = 真下
    Col c;
    if (t < 0.5f) {
      float k = powf(t * 2.0f, 1.6f);                  // 地平線ぎわで急に明るくなる
      c = lerpC(D.skyTop, D.skyHor, k);
    } else {
      // 地平線ですぐ地面へ。鏡にくっきりした水平線が映ります
      float k = smooth3((t - 0.5f) / 0.10f);
      Col near = {D.skyHor.r * 0.7f, D.skyHor.g * 0.7f, D.skyHor.b * 0.72f};
      c = lerpC(near, D.ground, k);
    }
    for (int u = 0; u < ENVW; ++u) {
      buf[(v * ENVW + u) * 3 + 0] = c.r;
      buf[(v * ENVW + u) * 3 + 1] = c.g;
      buf[(v * ENVW + u) * 3 + 2] = c.b;
    }
  }

  // --- 太陽(夜は月)とその広いにじみ
  {
    float cu = ENVW * 0.30f, cv = ENVH * D.sunV;
    float big = D.sunSize * 6.0f;
    for (int v = max(0, (int)(cv - big * 2.2f)); v <= min(ENVH - 1, (int)(cv + big * 2.2f)); ++v)
      for (int u = (int)(cu - big * 2.2f); u <= (int)(cu + big * 2.2f); ++u) {
        float du = (u - cu) / big, dv = (v - cv) / big;
        envAdd(buf, u, v, D.sun, expf(-(du * du + dv * dv) * 1.3f) * D.sunBr * 0.22f);
      }
    for (int v = max(0, (int)(cv - D.sunSize * 3)); v <= min(ENVH - 1, (int)(cv + D.sunSize * 3)); ++v)
      for (int u = (int)(cu - D.sunSize * 3); u <= (int)(cu + D.sunSize * 3); ++u) {
        float du = (u - cu) / D.sunSize, dv = (v - cv) / D.sunSize;
        envAdd(buf, u, v, D.sun, expf(-(du * du + dv * dv) * 1.6f) * D.sunBr);
      }
  }

  // --- 雲(空の中ほどに、横に長くちぎれて浮かぶ)
  for (int k = 0; k < (int)D.clouds; ++k) {
    float cu = frand() * ENVW, cv = ENVH * (0.06f + frand() * 0.36f);
    float ru = 10.0f + frand() * 26.0f, rv = 1.8f + frand() * 4.0f;
    float br = (0.25f + frand() * 0.75f) * (1.0f - cv / ENVH * 0.7f);
    for (int v = max(0, (int)(cv - rv * 2.4f)); v <= min(ENVH - 1, (int)(cv + rv * 2.4f)); ++v)
      for (int u = (int)(cu - ru * 2.4f); u <= (int)(cu + ru * 2.4f); ++u) {
        float du = (u - cu) / ru, dv = (v - cv) / rv;
        envAdd(buf, u, v, D.cloud, expf(-(du * du + dv * dv) * 1.2f) * br * 0.55f);
      }
  }

  // --- 空のレーザー(夜・夕方だけ)
  if (D.lasers > 0.01f)
    for (int k = 0; k < 7; ++k) {
      float u0 = frand() * ENVW, sl = (frand() * 2 - 1) * 1.8f;
      float br = (0.35f + frand() * 0.5f) * D.lasers;
      for (int v = 0; v < (int)(ENVH * 0.48f); ++v) {
        float uu = u0 + sl * v;
        for (int d = -1; d <= 1; ++d)
          envAdd(buf, (int)uu + d, v, T.laser,
                 (d == 0 ? 1.0f : 0.45f) * br * (1.0f - v / (ENVH * 0.55f)));
      }
    }

  // --- 地平線ぎわの会場: 建物のかたまり(いつでも見える)
  for (int k = 0; k < 40; ++k) {
    int u0 = (int)(frand() * ENVW), wd = 4 + (int)(frand() * 16);
    float hgt = 2.0f + frand() * 9.0f;
    int vb = (int)(ENVH * 0.50f);
    Col c = {D.ground.r * 1.7f + 0.03f, D.ground.g * 1.7f + 0.03f, D.ground.b * 1.7f + 0.04f};
    for (int i = 0; i < wd; ++i)
      for (int v = vb - (int)hgt; v <= vb + 2; ++v)
        envAdd(buf, u0 + i, v, c, 1.0f);
  }

  // --- 地面のむら(昼でも下半分がのっぺりしないように)
  for (int k = 0; k < 46; ++k) {
    float cu = frand() * ENVW, cv = ENVH * (0.55f + frand() * 0.42f);
    float ru = 8.0f + frand() * 26.0f, rv = 4.0f + frand() * 12.0f;
    float sgn = frand() < 0.5f ? -0.55f : 0.9f;
    Col c = {D.ground.r + 0.02f, D.ground.g + 0.02f, D.ground.b + 0.025f};
    for (int v = max(0, (int)(cv - rv * 2.2f)); v <= min(ENVH - 1, (int)(cv + rv * 2.2f)); ++v)
      for (int u = (int)(cu - ru * 2.2f); u <= (int)(cu + ru * 2.2f); ++u) {
        float du = (u - cu) / ru, dv = (v - cv) / rv;
        envAdd(buf, u, v, c, expf(-(du * du + dv * dv) * 1.2f) * sgn);
      }
  }

  // --- 会場の灯り(丸いにじみ)と、光の帯。昼はともらない
  if (D.lights > 0.01f) {
    for (int k = 0; k < 64; ++k) {
      float cu = frand() * ENVW;
      float cv = ENVH * (0.40f + frand() * 0.36f);
      float ru = 3.5f + frand() * 13.0f, rv = 2.2f + frand() * 6.5f;
      Col col = hsv(T.hues[k % 5] + (frand() - 0.5f) * 0.045f, 0.55f + frand() * 0.4f, 1.0f);
      float br = (0.7f + frand() * 2.3f) * D.lights;
      for (int v = max(0, (int)(cv - rv * 2.2f)); v <= min(ENVH - 1, (int)(cv + rv * 2.2f)); ++v)
        for (int u = (int)(cu - ru * 2.2f); u <= (int)(cu + ru * 2.2f); ++u) {
          float du = (u - cu) / ru, dv = (v - cv) / rv;
          envAdd(buf, u, v, col, expf(-(du * du + dv * dv) * 1.1f) * br);
        }
    }
    for (int k = 0; k < 16; ++k) {                     // 光の帯(LEDの列)
      int v = (int)(ENVH * (0.46f + frand() * 0.30f));
      int u0 = (int)(frand() * ENVW), len = 12 + (int)(frand() * 60);
      Col col = hsv(T.hues[k % 5], 0.45f + frand() * 0.4f, 1.0f);
      float br = (1.0f + frand() * 1.8f) * D.lights;
      for (int i = 0; i < len; ++i) {
        float fade = smooth3(i / 6.0f) * smooth3((len - i) / 6.0f);
        for (int d = -2; d <= 2; ++d)
          envAdd(buf, u0 + i, v + d, col,
                 br * fade * (d == 0 ? 1.0f : (d == 1 || d == -1 ? 0.6f : 0.25f)));
      }
    }
    for (int k = 0; k < 260; ++k) {                    // 足もとの光の粒
      int u = (int)(frand() * ENVW), v = (int)(ENVH * (0.78f + frand() * 0.20f));
      envAdd(buf, u, v, hsv(T.hues[k % 5], 0.3f, 1.0f), (0.2f + frand() * 0.5f) * D.lights);
    }
  }

  // ひとなでして、1テクセルだけの点をなくす(拡大したときのブロック防止)
  for (int pass = 0; pass < 1; ++pass)
    for (int c = 0; c < 3; ++c) {
      for (int v = 0; v < ENVH; ++v) {
        float prev = buf[(v * ENVW + ENVW - 1) * 3 + c], cur = buf[(v * ENVW) * 3 + c];
        for (int u = 0; u < ENVW; ++u) {
          float nxt = buf[(v * ENVW + ((u + 1) & (ENVW - 1))) * 3 + c];
          buf[(v * ENVW + u) * 3 + c] = cur * 0.5f + (prev + nxt) * 0.25f;
          prev = cur; cur = nxt;
        }
      }
      for (int u = 0; u < ENVW; ++u) {
        float prev = buf[u * 3 + c], cur = prev;
        for (int v = 0; v < ENVH; ++v) {
          float nxt = buf[(min(v + 1, ENVH - 1) * ENVW + u) * 3 + c];
          buf[(v * ENVW + u) * 3 + c] = cur * 0.5f + (prev + nxt) * 0.25f;
          prev = cur; cur = nxt;
        }
      }
    }
  // 明るさをかけてから、明るいところだけやわらかく丸める(白飛びしないように)
  const float g = ENV_GAIN * D.gain, iw = 1.0f / ENV_WHITE;
  const float amb = ENV_AMB * D.gain;
  for (int i = 0; i < ENVW * ENVH; ++i) {
    float c[3];
    for (int k = 0; k < 3; ++k) {
      float x = buf[i * 3 + k] * g + amb;
      c[k] = x * (1.0f + x * iw) / (1.0f + x);
    }
    envMap[i] = toPix({c[0], c[1], c[2]});
  }
  free(buf);
}

// ------------------------------------------------- 眼の断面(色替えのときだけ)
static void buildStaticEye() {
  for (int i = 0; i < PROFN; ++i) {
    float f = (i + 0.5f) / PROFN - 0.5f;
    profC[i] = expf(-f * f * 30.0f);           // 白熱した細い筋
    profH[i] = expf(-f * f * 15.0f) * 0.38f;    // まわりのにじみ
  }
  for (int i = 0; i < RIPN; ++i) {
    float u = (i + 0.5f) / RIPN * 2.0f - 1.0f;
    ripProf[i] = expf(-u * u * 6.0f) * sinf(u * 4.2f);
  }
  for (int i = 0; i < EYEN; ++i) {
    float t = sqrtf((float)i / EYEN);          // 0 = 中心, 1 = くぼみのふち
    eyeT[i]   = t;
    eyePow[i] = powf(t, EYE_P);                // 輪の並び(ふち寄りほど密)
    // 明るい虹彩の円盤(ふちで消える)と、そこを流れる影の濃さ
    float outer = smooth3((t - 0.30f) / 0.30f);
    eAmpC[i] = (1.0f - smooth3((t - 0.70f) / 0.20f)) * (0.35f + 0.65f * smooth3(t / 0.10f));
    eAmpH[i] = 0.72f + 0.38f * outer;
    eBaseT[i] = clamp01((t - 0.10f) / 0.62f);  // 内側の色 → 外側の色
    eSock[i] = smooth3((t - 0.74f) / 0.14f) * (1.0f - smooth3((t - 0.97f) / 0.04f));
    eRim[i]  = expf(-(t - 0.88f) * (t - 0.88f) * 420.0f) * 0.95f;   // ふちのひろい光
  }
}

// ------------------------------------------------- 眼の色(1フレームに1回)
// 同心の輪をそのまま出すのではなく、膜のうねりが「中心からの距離」をずらすので、
// 輪が折り重なって白い筋になります(黒い鏡の中を液体金属が流れるように)。
// 光っているところだけ色を置き、暗いところは下の映り込みを暗くするだけにして、
// 眼が貼り付いて見えないようにしています。
static void buildEye() {
  const Theme& T = THEMES[theme];
  const float br = (1.0f + flash * 1.1f) * (1.0f - 0.85f * blinkPos);
  const float ibr = 1.0f / powf(1.0f + EYE_BREATH * sinf(simT * 0.5f), EYE_P);
  const float phase = simT * EYE_SPEED;
  // 色は 青 → 白 → 赤 → 白 … とゆっくり巡ります(いつも白がベース)
  const Col white = {1.0f, 1.0f, 1.0f};
  const float m = 0.5f + 0.5f * sinf(simT * EYE_CYCLE * 6.2831853f);
  Col co = (m < 0.5f) ? lerpC(hsv(T.eyeHueA, 0.90f, 1.0f), white, m * 2.0f)
                      : lerpC(white, hsv(T.eyeHueB, 0.90f, 1.0f), (m - 0.5f) * 2.0f);
  const Col rim = lerpC(co, white, 0.20f);             // ふちの光の輪(色が濃い)

  for (int i = 0; i < EYEN; ++i) {
    float ph = EYE_RING * eyePow[i] * ibr - phase;
    int pi = (int)(fracf(ph) * PROFN) & (PROFN - 1);
    // 明るい虹彩の上を、暗い渦が流れていく(動画と同じ向き)
    float d = clamp01((profC[pi] * EYE_SHADE + profH[pi] * 0.8f) * eAmpH[i]);
    float lit = eAmpC[i] * (1.0f - 0.94f * d);

    // まん中はほぼ白、ふちへ行くほど色が濃くなる(動画の白〜水色と同じ)
    float wmix = (0.88f - 0.62f * eBaseT[i]) * (1.0f - 0.45f * d);
    Col c = lerpC(co, white, wmix);
    c = {c.r * lit, c.g * lit, c.b * lit};
    c.r += rim.r * eRim[i]; c.g += rim.g * eRim[i]; c.b += rim.b * eRim[i];
    eyeLut[i] = toPix({c.r * br, c.g * br, c.b * br});

    float bright = lit + eRim[i] * 1.6f;
    eyeSh[i] = bright > 0.08f ? 0 : 2;
  }
  lidCol = toPix({rim.r * 0.55f, rim.g * 0.55f, rim.b * 0.60f});   // まぶたのふちの光
}

// ---------------------------------------------------------------- 立方体
// 8つの角を回して画面に落とし、見えている3面の「行ごとの左右端」を出します。
static const int8_t CUBE_V[8][3] = {
  {-1,-1,-1},{ 1,-1,-1},{ 1, 1,-1},{-1, 1,-1},
  {-1,-1, 1},{ 1,-1, 1},{ 1, 1, 1},{-1, 1, 1}};
static const uint8_t CUBE_F[6][4] = {
  {0,3,2,1},{4,5,6,7},{0,1,5,4},{3,7,6,2},{0,4,7,3},{1,2,6,5}};
static const int8_t CUBE_N[6][3] = {{0,0,-1},{0,0,1},{0,-1,0},{0,1,0},{-1,0,0},{1,0,0}};

static float camD, focal, ccx, ccy;
static float sxv[8], syv[8];

static inline void projectPt(float x, float y, float z, float& sx, float& sy) {
  float w = camD + z; if (w < 0.25f) w = 0.25f;
  sx = ccx + focal * x / w;
  sy = ccy + focal * y / w;
}
// 画面のある点に、ある向きの鏡が何を映すか(景色の絵の座標)
static void mirrorUV(float px, float py, const float* n, float& u, float& v) {
  float vx = (px - ccx) / focal, vy = (py - ccy) / focal, vz = 1.0f;
  float il = 1.0f / sqrtf(vx * vx + vy * vy + vz * vz);
  vx *= il; vy *= il; vz *= il;
  float d = 2.0f * (vx * n[0] + vy * n[1] + vz * n[2]);
  float rx = vx - d * n[0], ry = vy - d * n[1], rz = vz - d * n[2];
  u = atan2f(rx, rz) * (ENVW / 6.2831853f);
  float ry2 = ry < -1 ? -1 : (ry > 1 ? 1 : ry);
  v = (0.5f + asinf(ry2) * (1.0f / 3.14159265f)) * ENVH;
}

static void updateCube(float dt) {
  yaw += yawV * dt;
  yawV *= expf(-dt * SPIN_DRAG);
  float ang = 0.7853982f + yaw + SWING * sinf(simT * SWING_W * 6.2831853f);
  float cy = cosf(ang), sy = sinf(ang);
  float lean = LEAN + 0.03f * sinf(simT * 0.23f);
  float cl = cosf(lean), sl = sinf(lean);
  float ct = cosf(TILT), st = sinf(TILT);
  float bob = BOB * S * sinf(simT * 0.41f);

  float half = 1.0f;
  float rot[3][3];                               // yaw → lean(Z) → tilt(X)
  for (int i = 0; i < 3; ++i) {
    float ax = (i == 0), ay = (i == 1), az = (i == 2);
    float x1 = cy * ax + sy * az, y1 = ay, z1 = -sy * ax + cy * az;   // yaw
    float x2 = cl * x1 - sl * y1, y2 = sl * x1 + cl * y1, z2 = z1;    // lean
    float x3 = x2, y3 = ct * y2 - st * z2, z3 = st * y2 + ct * z2;    // tilt
    rot[0][i] = x3; rot[1][i] = y3; rot[2][i] = z3;
  }
  for (int i = 0; i < 8; ++i) {
    float ox = CUBE_V[i][0] * half, oy = CUBE_V[i][1] * half, oz = CUBE_V[i][2] * half;
    float x = rot[0][0] * ox + rot[0][1] * oy + rot[0][2] * oz;
    float y = rot[1][0] * ox + rot[1][1] * oy + rot[1][2] * oz;
    float z = rot[2][0] * ox + rot[2][1] * oy + rot[2][2] * oz;
    projectPt(x, y + bob / focal * (camD + z), z, sxv[i], syv[i]);
  }

  // 見えている面を選び、行ごとの左右端を作る
  nvis = 0;
  for (int f = 0; f < 6 && nvis < NFACE; ++f) {
    float n[3];
    for (int i = 0; i < 3; ++i)
      n[i] = rot[i][0] * CUBE_N[f][0] + rot[i][1] * CUBE_N[f][1] + rot[i][2] * CUBE_N[f][2];
    // 面の中心は法線の向き(単位立方体なので)。カメラから面へのベクトルと逆を向いていれば見える
    float toF[3] = {n[0], n[1], n[2] + camD};
    if (n[0] * toF[0] + n[1] * toF[1] + n[2] * toF[2] >= 0) continue;

    int16_t* a = fRow[nvis].x0; int16_t* b = fRow[nvis].x1;
    for (int y = 0; y < S; ++y) { a[y] = 1; b[y] = 0; }
    for (int e = 0; e < 4; ++e) {
      int i0 = CUBE_F[f][e], i1 = CUBE_F[f][(e + 1) & 3];
      float y0 = syv[i0], y1 = syv[i1], x0 = sxv[i0], x1 = sxv[i1];
      if (y0 == y1) continue;
      int ya = (int)ceilf(fminf(y0, y1)), yb = (int)floorf(fmaxf(y0, y1));
      ya = max(ya, 0); yb = min(yb, S - 1);
      float m = (x1 - x0) / (y1 - y0);
      for (int y = ya; y <= yb; ++y) {
        int x = (int)lroundf(x0 + m * (y - y0));
        if (x < 0) x = 0;
        if (x > S - 1) x = S - 1;
        if (a[y] > b[y]) { a[y] = x; b[y] = x; }
        else { if (x < a[y]) a[y] = x; if (x > b[y]) b[y] = x; }
      }
    }
    // 映り込みの1次式: 面の真ん中とその近くで実際に計算して、傾きを出す
    float mx = 0, my = 0;
    for (int e = 0; e < 4; ++e) { mx += sxv[CUBE_F[f][e]]; my += syv[CUBE_F[f][e]]; }
    mx *= 0.25f; my *= 0.25f;
    const float hh = 24.0f;
    float u0, v0, ux, vx, uy, vy;
    mirrorUV(mx, my, n, u0, v0);
    mirrorUV(mx + hh, my, n, ux, vx);
    mirrorUV(mx, my + hh, n, uy, vy);
    float du = ux - u0, dv2 = uy - u0;            // u は一周でつながるので、近いほうへ
    while (du  >  ENVW * 0.5f) du  -= ENVW;
    while (du  < -ENVW * 0.5f) du  += ENVW;
    while (dv2 >  ENVW * 0.5f) dv2 -= ENVW;
    while (dv2 < -ENVW * 0.5f) dv2 += ENVW;
    du = du / hh * REFLECT; dv2 = dv2 / hh * REFLECT;
    float ev = (vx - v0) / hh * REFLECT, ev2 = (vy - v0) / hh * REFLECT;
    fDU[nvis] = (int32_t)(du * 256.0f);   fEU[nvis] = (int32_t)(dv2 * 256.0f);
    fDV[nvis] = (int32_t)(ev * 256.0f);   fEV[nvis] = (int32_t)(ev2 * 256.0f);
    fU0[nvis] = (int32_t)((u0 + simT * ENV_DRIFT) * 256.0f) - fDU[nvis] * (int32_t)mx - fEU[nvis] * (int32_t)my;
    fV0[nvis] = (int32_t)(v0 * 256.0f)                      - fDV[nvis] * (int32_t)mx - fEV[nvis] * (int32_t)my;
    ++nvis;
  }

  // 立方体の輪郭(3つの面をまとめた、行ごとの左右端)。眼をここからはみ出させない
  for (int y = 0; y < S; ++y) { cubeX0[y] = 1; cubeX1[y] = 0; }
  for (int f = 0; f < nvis; ++f)
    for (int y = 0; y < S; ++y) {
      if (fRow[f].x0[y] > fRow[f].x1[y]) continue;
      if (cubeX0[y] > cubeX1[y]) { cubeX0[y] = fRow[f].x0[y]; cubeX1[y] = fRow[f].x1[y]; }
      else {
        if (fRow[f].x0[y] < cubeX0[y]) cubeX0[y] = fRow[f].x0[y];
        if (fRow[f].x1[y] > cubeX1[y]) cubeX1[y] = fRow[f].x1[y];
      }
    }

  // 眼は立方体の真ん中に
  float ex, ey;
  projectPt(0, bob / focal * camD, 0, ex, ey);
  ex += EYE_WOB * S * sinf(simT * 0.37f);          // 眼そのものもゆらゆら
  ey += EYE_WOB * S * sinf(simT * 0.29f + 1.7f);
  ecxq = (int32_t)(ex * 16.0f); ecyq = (int32_t)(ey * 16.0f);
  eyeX0i = (int16_t)ex; eyeY0i = (int16_t)ey;
  float Re = EYE_R * S;
  // うねりで眼がずれても切れないよう、余裕をもたせた四角で当たりをとる
  eyeR2i = (int16_t)(fmaxf(Re + warpMax, Re * EYE_DISC) + 2.0f);
  // まぶた: 開いているときは眼の上、閉じきると眼の下まで降りてくる
  lidQ = (int32_t)((-1.35f + 2.70f * blinkPos) * Re * 16.0f);
  // まばたきは渦だけでなく「眼まるごと」を隠すので、うねりを入れない円盤も持っておく
  int32_t dq = (int32_t)(Re * EYE_DISC * 16.0f);
  eyeDiscQ2 = dq * dq;
  eyeK = (uint32_t)((float)EYEN * 65536.0f / (Re * Re));
}

// ---------------------------------------------------------------- 1フレームの準備
struct Input { bool touching; bool tapped; float tx, ty; float dt; };

static void updateFrame(const Input& in) {
  float dt = in.dt > 0.05f ? 0.05f : in.dt;
  simT += dt;

  float fvx = 0, fvy = 0;
  if (in.touching) {
    if (fWas) { fvx = (in.tx - fPX) / fmaxf(dt, 0.001f); fvy = (in.ty - fPY) / fmaxf(dt, 0.001f); }
    holdT += dt;
  } else holdT = 0;
  fPX = in.tx; fPY = in.ty; fWas = in.touching;
  yawV += fvx * FLICK;                               // 横に払うと回る
  if (yawV > 6.0f) yawV = 6.0f;
  if (yawV < -6.0f) yawV = -6.0f;

  if (in.tapped) {
    rips[ripNext].t0 = simT; rips[ripNext].on = 1;
    ripNext = (ripNext + 1) % NRIP;
    flash = 1.0f;
  }
  flash *= expf(-dt * 3.0f);

  // ---- まばたき(たまに2回続けて)。触られたらその場でひとつ
  if (in.tapped && !blinkOn) { blinkOn = 1; blinkT0 = simT; blinkTwin = 0; }
  if (!blinkOn) {
    if (simT >= blinkNext) { blinkOn = 1; blinkT0 = simT; blinkTwin = (frand() < BLINK_TWIN) ? 1 : 0; }
  } else {
    float a = (simT - blinkT0) / BLINK_DUR;
    if (a >= 1.0f) {
      if (blinkTwin) { --blinkTwin; blinkT0 = simT; }
      else { blinkOn = 0; blinkPos = 0; blinkNext = simT + BLINK_MIN + frand() * (BLINK_MAX - BLINK_MIN); }
    } else {
      blinkPos = (a < 0.34f) ? smooth3(a / 0.34f)            // 閉じるのは速く
                             : 1.0f - smooth3((a - 0.34f) / 0.66f);   // 開くのはゆっくり
    }
  }

  // ---- 指でかき混ぜた分
  float relax = expf(-dt * SETTLE);
  float rad = 0.17f * S, inv2 = 1.0f / (2 * rad * rad);
  float maxD = 0.24f * S;
  float swirlStep = in.touching ? SWIRL * smooth3(holdT / 0.6f) * dt : 0.0f;
  float swirlR = SWIRL_R * S, swirlInv = 1.0f / (swirlR * swirlR);
  float swirlSat = 1.0f / (0.22f * S * 0.22f * S);
  float farCut = 9.3f * rad * rad;
  for (int gy = 0; gy < CW; ++gy)
    for (int gx = 0; gx < CW; ++gx) {
      int q = gy * CW + gx;
      float x = gx * CSTEP, y = gy * CSTEP;
      float dx = dfX[q] * relax, dy = dfY[q] * relax;
      if (in.touching) {
        float rx = x - in.tx, ry = y - in.ty;
        if (swirlStep > 0) {
          float sx = rx - dx, sy = ry - dy;
          float th = swirlStep / (1.0f + (sx * sx + sy * sy) * swirlInv);
          th /= 1.0f + (dx * dx + dy * dy) * swirlSat;
          float iv = 1.0f / sqrtf(1.0f + th * th);
          float nx = iv * sx + th * iv * sy, ny = iv * sy - th * iv * sx;
          dx = rx - nx; dy = ry - ny;
        }
        float d2 = rx * rx + ry * ry;
        if (d2 < farCut) {
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

  // ---- 膜のうねり(＋タップの波)
  float amp = FLOW * S, sc = 7.0f / S * CSTEP, tt = simT * FLOW_SPEED * 10.0f;
  buildNMap(nmX[0], sc,        tt,        CW);  buildNMap(nmY[0], sc,        0,         CW);
  buildNMap(nmX[1], sc * 2.8f, 31,        CW);  buildNMap(nmY[1], sc * 2.8f, -tt*1.3f,  CW);
  buildNMap(nmX[2], sc,       -17,        CW);  buildNMap(nmY[2], sc,        tt,        CW);
  buildNMap(nmX[3], sc * 2.8f, tt * 1.1f, CW);  buildNMap(nmY[3], sc * 2.8f, 11,        CW);
  // 眼の底だけ、大きくゆっくり流れるうねりを足す(輪が折れて筋になる)
  float et = simT * EYE_FLOW_S * 10.0f;
  buildNMap(nmX[4], sc,  et,             CW);  buildNMap(nmY[4], sc, -et * 0.7f + 5, CW);
  buildNMap(nmX[5], sc, -et * 0.8f + 19, CW);  buildNMap(nmY[5], sc,  et * 1.1f,     CW);
  buildNMap(nmX[6], sc * 3.2f,  et * 1.6f + 3,  CW);  buildNMap(nmY[6], sc * 3.2f, -et * 1.2f, CW);
  buildNMap(nmX[7], sc * 3.2f, -et * 1.4f + 27, CW);  buildNMap(nmY[7], sc * 3.2f,  et * 1.7f, CW);

  float ripR[NRIP], ripA[NRIP]; int nrip = 0;
  for (int i = 0; i < NRIP; ++i) {
    if (rips[i].on <= 0) continue;
    float age = simT - rips[i].t0;
    if (age > 2.4f) { rips[i].on = 0; continue; }
    ripR[nrip] = age * 0.45f * S;
    ripA[nrip] = expf(-age * 1.2f) * PULSE * amp * 2.2f;
    ++nrip;
  }
  float ecx = ecxq * (1.0f / 16.0f), ecy = ecyq * (1.0f / 16.0f);
  float ripW = RIPN * 0.5f / (0.12f * S);
  float eReye = EYE_R * S;                       // 眼のふち
  float eOut = eReye * EYE_EDGE;                 // うねりが届くところ
  float eCut = eOut * eOut;
  float eInvR2 = 1.0f / eCut;
  float eAmp = EYE_DEEP * eReye;                 // 眼の大きさに合わせてうねる
  // 眼の中心でのうねりを引いておく。こうすると眼が横に流れず、
  // 中だけがぐにゃぐにゃ折れます(全体がずれるのではなく、内側で渦を巻く)
  int egx = (int)(ecx * (1.0f / CSTEP)); if (egx < 0) egx = 0; if (egx > CW - 1) egx = CW - 1;
  int egy = (int)(ecy * (1.0f / CSTEP)); if (egy < 0) egy = 0; if (egy > CW - 1) egy = CW - 1;
  const float eN4 = noiseAt(nmX[4], nmY[4], egx, egy) + EYE_FINE * noiseAt(nmX[6], nmY[6], egx, egy);
  const float eN5 = noiseAt(nmX[5], nmY[5], egx, egy) + EYE_FINE * noiseAt(nmX[7], nmY[7], egx, egy);

  for (int gy = 0; gy < CW; ++gy)
    for (int gx = 0; gx < CW; ++gx) {
      int q = gy * CW + gx;
      float nx = noiseAt(nmX[0], nmY[0], gx, gy) + WRINKLE * noiseAt(nmX[1], nmY[1], gx, gy);
      float ny = noiseAt(nmX[2], nmY[2], gx, gy) + WRINKLE * noiseAt(nmX[3], nmY[3], gx, gy);
      float ax = amp * nx, ay = amp * ny;
      float dx = gx * CSTEP - ecx, dy = gy * CSTEP - ecy;
      float d2e = dx * dx + dy * dy;
      if (d2e < eCut) {                            // 眼のまわりだけ、大きなうねりを重ねる
        float u = d2e * eInvR2;
        float eg = (1.0f - u * u) * eAmp;          // ふちでもまだ効くので、輪郭がグネグネする
        float n4 = noiseAt(nmX[4], nmY[4], gx, gy) + EYE_FINE * noiseAt(nmX[6], nmY[6], gx, gy);
        float n5 = noiseAt(nmX[5], nmY[5], gx, gy) + EYE_FINE * noiseAt(nmX[7], nmY[7], gx, gy);
        ax += eg * (n4 - eN4);                     // 中心の分を引くので、眼は流れていかない
        ay += eg * (n5 - eN5);
      }
      if (nrip) {                                  // 眼から広がる波
        float d = sqrtf(d2e) + 0.001f;
        float ix = dx / d, iy = dy / d;
        for (int k = 0; k < nrip; ++k) {
          float uu = (d - ripR[k]) * ripW + RIPN * 0.5f;
          int ui = (int)uu;
          if ((unsigned)ui < (unsigned)RIPN) {
            float w = ripProf[ui] * ripA[k];
            ax += ix * w; ay += iy * w;
          }
        }
      }
      tmpX[q] = ax - dfX[q];
      tmpY[q] = ay - dfY[q];
    }
  float wm2 = 0;
  for (int gy = 0; gy < CW; ++gy)                  // ひとなでして急な傾きを落とす
    for (int gx = 0; gx < CW; ++gx) {
      int q = gy * CW + gx;
      int l = gx > 0 ? q - 1 : q, r = gx < CW - 1 ? q + 1 : q;
      int u = gy > 0 ? q - CW : q, d = gy < CW - 1 ? q + CW : q;
      float ax = tmpX[q] * 0.5f + (tmpX[l] + tmpX[r] + tmpX[u] + tmpX[d]) * 0.125f;
      float ay = tmpY[q] * 0.5f + (tmpY[l] + tmpY[r] + tmpY[u] + tmpY[d]) * 0.125f;
      wX[q] = ax; wY[q] = ay;
      // 眼のふちより外だけ見る(内側のうねりは外へはみ出さないので)
      float ex2 = gx * CSTEP - ecx, ey2 = gy * CSTEP - ecy;
      float dd = ex2 * ex2 + ey2 * ey2, lim = eOut - CSTEP;
      if (dd >= lim * lim) { float m2 = ax * ax + ay * ay; if (m2 > wm2) wm2 = m2; }
    }
  warpMax = sqrtf(wm2);          // 眼がこれ以上ずれることはない

  // レーザー(背景)
  for (int i = 0; i < NLAS; ++i) {
    float ph = simT * LASER_SPD + i * 1.7f;
    float top = S * (0.5f + 0.42f * sinf(ph * 1.3f + i));
    float slope = 0.9f * sinf(ph + i * 2.1f) + (i - 1.5f) * 0.35f;
    lasX[i] = (int32_t)(top * 256.0f);
    lasD[i] = (int32_t)(slope * 256.0f);
  }

  updateCube(dt);
  buildEye();
}

// ---------------------------------------------------------------- 描画(帯ごと)
template <int P>
static inline __attribute__((always_inline)) void renderBandT(int k, uint16_t* buf, Scratch& sc) {
  const int y0 = k * BAND, y1 = min(S, y0 + BAND);
  const int bx0 = bandX0[k], bw = bandW[k], bx1 = bx0 + bw - 1;
  const uint16_t* const envp = envMap;
  const uint16_t* const elp  = eyeLut;
  const uint8_t*  const esp  = eyeSh;
  const int32_t wsq = (int32_t)(WSCALE * 256.0f);

  for (int y = y0; y < y1; y += P) {
    uint16_t* row = buf + (y - y0) * bw;
    int xs = max((int)spanS[y], bx0), xe = min((int)spanE[y], bx1);
    if (P == 2) xs &= ~1;
    for (int x = bx0; x < xs; ++x) row[x - bx0] = blackCol;
    for (int x = max(xe + 1, bx0); x <= bx1; ++x) row[x - bx0] = blackCol;
    if (xs > xe) { if (P == 2 && y + 1 < y1) memcpy(row + bw, row, bw * sizeof(uint16_t)); continue; }

    // --- 1. 夜空とレーザー
    {
      uint16_t skyc = rowSky[y];
      for (int x = xs; x <= xe; ++x) row[x - bx0] = skyc;
      if (TODS[todIdx].lasers > 0.4f) for (int i = 0; i < NLAS; ++i) {   // 夜だけ
        int lx = (int)((lasX[i] + lasD[i] * y) >> 8);
        for (int d = -1; d <= 1; ++d) {
          int x = lx + d;
          if (x >= xs && x <= xe) row[x - bx0] = lasCol[i];
        }
      }
    }

    // --- 2. 立方体の面(映り込み)
    {
      // この行のうねり(粗い格子を縦に補間。1/256ピクセル単位)
      int gy = y / CSTEP; float fy = (y - gy * CSTEP) * (1.0f / CSTEP);
      const float *ax = wX + gy * CW, *ay = wY + gy * CW;
      for (int gx = 0; gx < CW - 1; ++gx) {
        sc.rx[gx] = (int32_t)((ax[gx] + (ax[gx + CW] - ax[gx]) * fy) * 256.0f);
        sc.ry[gx] = (int32_t)((ay[gx] + (ay[gx + CW] - ay[gx]) * fy) * 256.0f);
      }
      for (int f = 0; f < nvis; ++f) {
        int fx0 = max((int)fRow[f].x0[y], xs), fx1 = min((int)fRow[f].x1[y], xe);
        if (P == 2) fx0 &= ~1;
        if (fx0 < xs) fx0 = xs;
        if (fx0 > fx1) continue;
        const int32_t baseU = fU0[f] + fEU[f] * y, baseV = fV0[f] + fEV[f] * y;
        const int32_t dU = fDU[f], dV = fDV[f];
        int x = fx0;
        while (x <= fx1) {
          const int gx = x / CSTEP;
          const int segEnd = min(fx1, gx * CSTEP + CSTEP - 1);
          const int32_t sx = (sc.rx[gx + 1] - sc.rx[gx] + (1 << (SHSTEP - 1))) >> SHSTEP;
          const int32_t sy = (sc.ry[gx + 1] - sc.ry[gx] + (1 << (SHSTEP - 1))) >> SHSTEP;
          const int off = x - gx * CSTEP;
          int32_t u = baseU + dU * x + (((sc.rx[gx] + sx * off) * wsq) >> 8);
          int32_t v = baseV + dV * x + (((sc.ry[gx] + sy * off) * wsq) >> 8);
          const int32_t du = dU * P + (((sx * wsq) >> 8) * P);
          const int32_t dv = dV * P + (((sy * wsq) >> 8) * P);
          for (; x <= segEnd; x += P, u += du, v += dv) {
            uint16_t col = envp[(((v >> 8) & (ENVH - 1)) << 8) + ((u >> 8) & (ENVW - 1))];
            row[x - bx0] = col;
            if (P == 2 && x + 1 <= bx1) row[x + 1 - bx0] = col;
          }
        }
      }
    }

    // --- 3. 眼(立方体の上に重ねる)
    {
      int dy = y - eyeY0i;
      int half = (int)eyeR2i;
      if (dy > -half && dy < half && cubeX0[y] <= cubeX1[y]) {
        int ex0 = max(max(eyeX0i - half, xs), (int)cubeX0[y]);
        int ex1 = min(min(eyeX0i + half, xe), (int)cubeX1[y]);
        if (P == 2) ex0 &= ~1;
        if (ex0 < xs) ex0 = xs;
        if (ex0 <= ex1) {
          const int32_t yq = y << 4;
          const int32_t uy = yq - ecyq;            // うねりを入れない位置(まぶた用)
          const int32_t uy2 = uy * uy;
          int x = ex0;
          while (x <= ex1) {
            const int gx = x / CSTEP;
            const int segEnd = min(ex1, gx * CSTEP + CSTEP - 1);
            const int32_t sx = (sc.rx[gx + 1] - sc.rx[gx] + (1 << (SHSTEP + 3))) >> (SHSTEP + 4);
            const int32_t sy = (sc.ry[gx + 1] - sc.ry[gx] + (1 << (SHSTEP + 3))) >> (SHSTEP + 4);
            const int off = x - gx * CSTEP;
            int32_t px = (x << 4) + (sc.rx[gx] >> 4) + sx * off - ecxq;
            int32_t py = yq + (sc.ry[gx] >> 4) + sy * off - ecyq;
            const int32_t dpx = (P << 4) + sx * P, dpy = sy * P;
            int32_t ux = (x << 4) - ecxq;
            const int32_t dux = P << 4;
            for (; x <= segEnd; x += P, px += dpx, py += dpy, ux += dux) {
              uint32_t rr = (uint32_t)(px * px + py * py) >> 8;
              uint32_t idx = (rr * eyeK) >> 16;
              const int32_t ux2 = ux * ux;
              if ((idx < EYEN) || (ux2 + uy2 < eyeDiscQ2)) {
                const int32_t edge = lidQ - (ux2 >> 13);   // まぶたのふち(両端がすこし上がる)
                uint16_t col;
                if (uy < edge) {                           // まぶたに隠れている(眼まるごと)
                  if (uy > edge - 56) col = lidCol;        // ふちがほのかに光る
                  else {
                    uint16_t c = __builtin_bswap16(row[x - bx0]);
                    col = __builtin_bswap16((uint16_t)((c >> 3) & 0x18E3));
                  }
                } else if (idx < EYEN) {
                  const int sh = esp[idx];
                  if (sh == 0) col = elp[idx];
                  else {
                    uint16_t c = __builtin_bswap16(row[x - bx0]);
                    c = (sh == 1) ? ((c >> 1) & 0x7BEF) : ((c >> 2) & 0x39E7);
                    col = __builtin_bswap16(c);
                  }
                } else continue;                           // 円盤の中だが虹彩の外
                row[x - bx0] = col;
                if (P == 2 && x + 1 <= bx1) row[x + 1 - bx0] = col;
              }
            }
          }
        }
      }
    }

    if (P == 2 && y + 1 < y1) memcpy(row + bw, row, bw * sizeof(uint16_t));
  }
}

static void __attribute__((noinline)) MOD_IRAM renderBandHD(int k, uint16_t* b, Scratch& s)    { renderBandT<1>(k, b, s); }
static void __attribute__((noinline)) MOD_IRAM renderBandLight(int k, uint16_t* b, Scratch& s) { renderBandT<2>(k, b, s); }
static inline void renderBand(int k, uint16_t* b, Scratch& s) {
  if (PIXEL == 1) renderBandHD(k, b, s); else renderBandLight(k, b, s);
}

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
static void buildScene() {                       // 時間帯・色替えのたびに作り直すもの
  const Theme& T = THEMES[theme];
  const TimeOfDay& D = TODS[todIdx];
  buildEnv();
  buildStaticEye();
  // 画面の背景: 上が空、下が地面。地平線は画面のずっと下のほう
  const float hz = 0.93f;
  for (int y = 0; y < S; ++y) {
    float t = (float)y / S;
    Col c;
    if (t < hz) c = lerpC(D.skyTop, D.skyHor, powf(t / hz, 1.9f));
    else        c = lerpC(D.skyHor, D.ground, smooth3((t - hz) / (1.0f - hz)));
    float sun = expf(-(t - D.sunV * 1.9f) * (t - D.sunV * 1.9f) * 9.0f) * D.sunBr * 0.09f;
    float j = ((hash2(y, 7) & 255) / 255.0f - 0.5f) * 0.006f;   // しま模様よけ
    const float sg = D.gain * 0.85f;             // 空は少し落として、立方体を立たせる
    rowSky[y] = toPix({c.r * sg + D.sun.r * sun + j,
                       c.g * sg + D.sun.g * sun + j,
                       c.b * sg + D.sun.b * sun + j});
  }
  float lg = 0.55f * D.lasers;
  for (int i = 0; i < NLAS; ++i)
    lasCol[i] = toPix({T.laser.r * lg, T.laser.g * lg, T.laser.b * lg});
}

bool begin() {

  if (M5.Display.width() < M5.Display.height()) M5.Display.setRotation(1);
  W = M5.Display.width(); H = M5.Display.height();
  S = ((W == H) ? W : max(W, H)) & ~1;
  XOFF = (W - S) / 2;
  YOFF = ((H - S) / 2 / BAND) * BAND;
  CW = S / CSTEP + 3;
  NB = (S + BAND - 1) / BAND;
  PIXEL = (QUALITY == 2) ? 2 : 1;
  Serial.printf("screen %dx%d  area %d  quality %d\n", W, H, S, QUALITY);

  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setBrightness(brightSteps[brightIdx]);

  noiseT = (float*)memAlloc(NOISE * NOISE * sizeof(float));
  for (int i = 0; i < NSLOT; ++i) slotBuf[i] = (uint16_t*)memCaps(S * BAND * 2, MALLOC_CAP_DMA);
  envMap = (uint16_t*)memAlloc(ENVW * ENVH * sizeof(uint16_t));
  eyeLut = (uint16_t*)memAlloc(EYEN * sizeof(uint16_t));
  eyeSh  = (uint8_t*)memAlloc(EYEN);
  eyeT   = (float*)memAlloc(EYEN * sizeof(float));
  eyePow = (float*)memAlloc(EYEN * sizeof(float));
  eAmpC  = (float*)memAlloc(EYEN * sizeof(float)); eAmpH = (float*)memAlloc(EYEN * sizeof(float));
  eBaseT = (float*)memAlloc(EYEN * sizeof(float)); eSock = (float*)memAlloc(EYEN * sizeof(float));
  eRim   = (float*)memAlloc(EYEN * sizeof(float));
  spanS  = (int16_t*)memAlloc(S * sizeof(int16_t)); spanE = (int16_t*)memAlloc(S * sizeof(int16_t));
  rowSky = (uint16_t*)memAlloc(S * sizeof(uint16_t));
  bandX0 = (int16_t*)memAlloc(NB * sizeof(int16_t)); bandW = (int16_t*)memAlloc(NB * sizeof(int16_t));
  bandVis = (uint8_t*)memAlloc(NB);
  cubeX0 = (int16_t*)memAlloc(S * sizeof(int16_t)); cubeX1 = (int16_t*)memAlloc(S * sizeof(int16_t));
  for (int f = 0; f < NFACE; ++f) {
    fRow[f].x0 = (int16_t*)memAlloc(S * sizeof(int16_t));
    fRow[f].x1 = (int16_t*)memAlloc(S * sizeof(int16_t));
  }
  int CN = CW * CW;
  dfX = (float*)memCalloc(CN, sizeof(float)); dfY = (float*)memCalloc(CN, sizeof(float));
  tmpX = (float*)memCalloc(CN, sizeof(float)); tmpY = (float*)memCalloc(CN, sizeof(float));
  wX = (float*)memCalloc(CN, sizeof(float)); wY = (float*)memCalloc(CN, sizeof(float));
  for (int i = 0; i < 8; ++i) {
    nmX[i].i0 = (uint16_t*)memAlloc(CW * 2); nmX[i].i1 = (uint16_t*)memAlloc(CW * 2);
    nmX[i].f  = (float*)memAlloc(CW * 4);
    nmY[i].i0 = (uint16_t*)memAlloc(CW * 2); nmY[i].i1 = (uint16_t*)memAlloc(CW * 2);
    nmY[i].f  = (float*)memAlloc(CW * 4);
  }
  for (int c = 0; c < 2; ++c) {
    scr[c].rx = (int32_t*)memAlloc(CW * sizeof(int32_t));
    scr[c].ry = (int32_t*)memAlloc(CW * sizeof(int32_t));
  }
  bool ok = noiseT && envMap && eyeLut && eyeSh && eyeT && eyePow && eAmpC && eAmpH &&
            eBaseT && eSock && eRim && spanS && spanE && rowSky &&
            bandX0 && bandW && bandVis && cubeX0 && cubeX1 && dfX && dfY && tmpX && tmpY && wX && wY &&
            scr[0].rx && scr[0].ry && scr[1].rx && scr[1].ry;
  for (int i = 0; i < NSLOT; ++i) ok = ok && slotBuf[i];
  for (int f = 0; f < NFACE; ++f) ok = ok && fRow[f].x0 && fRow[f].x1;
  for (int i = 0; i < 8; ++i) ok = ok && nmX[i].i0 && nmX[i].i1 && nmX[i].f && nmY[i].i0 && nmY[i].i1 && nmY[i].f;
  if (!ok) { memFreeAll(); return false; }

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

  for (int i = 0; i <= 257; ++i) gLut[i] = expf(-(i * (GCUT / 256.0f)));
  makeNoise(noiseT, 3);
  for (int i = 0; i < NRIP; ++i) rips[i].on = 0;

  camD = 5.0f;
  focal = CUBE_FILL * 0.5f * S * (camD - 1.7321f) / 1.7321f;
  ccx = S * 0.5f; ccy = S * 0.500f;

  buildScene();
  Input in0 = {false, false, -1000, -1000, 0.016f};
  updateFrame(in0);

  semGo   = xSemaphoreCreateBinary();
  semDone = xSemaphoreCreateBinary();
  xTaskCreatePinnedToCore(workerTask, "band", 8192, nullptr, 1, &modTask, 0);
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

  if (round) {
    // ボタンA: 押すと背景が明るく(4段階)、長押しで景色の色が変わる
    if (M5.BtnA.wasClicked()) { todIdx = (todIdx + 1) % NTOD; buildScene(); }
    if (M5.BtnA.wasHold())    { theme = (theme + 1) % NTHEME; buildScene(); }
    if (M5.BtnB.wasClicked()) {
      brightIdx = (brightIdx + 1) % sizeof(brightSteps);
      M5.Display.setBrightness(brightSteps[brightIdx]);
    }
  } else {
    if (M5.BtnPWR.wasClicked()) { todIdx = (todIdx + 1) % NTOD; buildScene(); }
    if (M5.BtnPWR.wasHold()) {
      brightIdx = (brightIdx + 1) % sizeof(brightSteps);
      M5.Display.setBrightness(brightSteps[brightIdx]);
    }
    if (pressed) {
      if (now - lastTapMs < 350) { theme = (theme + 1) % NTHEME; buildScene(); lastTapMs = 0; }
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

static void memClearPtrs() {   // 解放後に残る「古い住所」を消す
  for (auto& p : slotBuf) p = nullptr;
  envMap = nullptr;
  eyeLut = nullptr;
  eyeSh = nullptr;
  eyeT = nullptr;
  eyePow = nullptr;
  eAmpC = nullptr;
  eAmpH = nullptr;
  eBaseT = nullptr;
  eSock = nullptr;
  eRim = nullptr;
  spanS = nullptr;
  spanE = nullptr;
  bandX0 = nullptr;
  bandW = nullptr;
  bandVis = nullptr;
  rowSky = nullptr;
  noiseT = nullptr;
  dfX = nullptr;
  dfY = nullptr;
  tmpX = nullptr;
  tmpY = nullptr;
  wX = nullptr;
  wY = nullptr;
  cubeX0 = nullptr;
  cubeX1 = nullptr;
  for (auto& s : scr) { s.rx = nullptr; s.ry = nullptr; }
  for (auto& s : fRow) { s.x0 = nullptr; s.x1 = nullptr; }
  for (auto& s : nmX) { s.i0 = nullptr; s.i1 = nullptr; s.f = nullptr; }
  for (auto& s : nmY) { s.i0 = nullptr; s.i1 = nullptr; s.f = nullptr; }
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

}  // namespace NullEye
