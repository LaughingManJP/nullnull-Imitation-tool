// NullDrop v6  -  鏡の雫。液体金属のような鏡の雫が、まわりの景色を映しながら、くっついたり離れたりする
//   null²(大阪・関西万博のパビリオン)から受けたイメージで作った、非公式の習作です。作品の映像・名前・データは使っていません。
//   M5Stack StopWatch 用(丸い画面)。CoreS3 系でも動きます(自動で切り替わります)。
//
// 画面の中:
//   ・いくつかの鏡の雫が、それぞれの場所のまわりをゆっくりさまよい、近づくとくっついて、また離れる
//   ・雫には、まわりの景色(上は空、まん中は窓の光、下は暗い床)が映り、ふちが白く光る
//
// 傾けると(本体の傾きセンサー):
//   ・雫が低い方へ転がって集まり、くっつく。水平に戻すと、また自分の場所へ散っていく
//   ・傾きの向きが逆のときは、調整用の TILT_FLIP_X / TILT_FLIP_Y / TILT_SWAP を変える。Serial に tilt: の値が出る
//
// 触ると:
//   ・ドラッグ: いちばん近い雫を、指で引っぱる
//   ・タップ: 小さな雫が生まれ、少し落ちながら、大きな雫に吸いこまれる
//
// 操作:
//   StopWatch … ボタンA 短押し: 雫の数(4 / 6 / 9)   ボタンA 長押し: 映る景色(部屋 / 夕焼け / 夜)   ボタンB: 明るさ
//   CoreS3系  … 画面の上半分をダブルタップ: 雫の数   下半分をダブルタップ: 景色   電源ボタン短押し: 明るさ
//
// 必要なもの:
//   ボードマネージャー M5Stack >= 3.3.7 / ボード: M5StopWatch(PSRAM は OPI)、CoreS3 なら M5CoreS3
//   ライブラリ M5Unified >= 0.2.16, M5GFX >= 0.2.21
//
// しくみ:
//   雫は「メタボール」です。4 ピクセルおきの格子で、各雫からの (半径/距離)^2 の合計(場)と、その傾きを計算し、
//   ピクセルでは補間して読みます。場が一定より大きい所が雫の中で、傾きから面の向きを出し、
//   「面の向き → 映る色」の表(毎フレーム作り直す)から色を読みます。
//   2つのコアで帯を描き、StopWatch では帯ごとに丸の中だけを画面へ送ります。

#include <M5Unified.h>
#include <math.h>
#include <algorithm>

#pragma GCC optimize("O3")
#ifndef IRAM_ATTR
#define IRAM_ATTR
#endif

// ---- 調整用 ----------------------------------------------------------------
static constexpr int   MAXBALL   = 16;      // 雫の最大の数(タップでできる小さな雫も含む)
static constexpr float THRESH    = 0.85f;   // 場がこれより大きい所が雫(小さいほど雫が太る)
static constexpr float GRAV      = 320.0f;  // 傾けたとき、雫が転がる強さ
static constexpr float SHINE     = 1.0f;    // ふちの光の強さ
static constexpr bool  DOUBLE_TAP_UI = true;
// ---------------------------------------------------------------------------
static constexpr int GS  = 4;               // 場を計算する格子の間隔(ピクセル)
static constexpr int ENV = 64;              // 面の向きの表の細かさ(1辺)

// ---- 傾き(本体の傾きセンサー)-----------------------------------------------
static constexpr bool  TILT        = true;    // 傾きで動かすか(false にすると、傾きを使わない)
static constexpr float TILT_GAIN   = 1.0f;    // 傾きの効き方(大きいほど、少し傾けただけで大きく動く)
static constexpr bool  TILT_FLIP_X = false;   // 左右が逆に動くときは true
static constexpr bool  TILT_FLIP_Y = false;   // 上下が逆に動くときは true
static constexpr bool  TILT_SWAP   = false;   // 左右と上下が入れかわっているときは true
static constexpr bool  TILT_STOPWATCH_FIX = true;   // StopWatch はセンサーが 90 度回ってついているので、自動で読みかえる
static constexpr float TILT_DEAD   = 0.08f;   // これより小さい傾きは無視する(机に置いたときや、手のふるえで動かないように)
static constexpr float TILT_RELEVEL = 15.0f;   // 同じ角度で持ち続けると、何秒くらいでそこを「水平」にするか(0 にすると、起動したときの姿勢のまま)
static constexpr float TILT_RELEVEL_MAX = 0.25f;   // これより大きく傾けているあいだは、水平を合わせなおさない(大きく傾けて戻すと逆に動くのを防ぐ)
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------- 型(関数より前にまとめて書く)
struct Col { float r, g, b; };
struct Input { bool touching; bool tapped; float tx, ty; float dt; float tiltX, tiltY; };   // tilt: 画面の上で「下」がどちらか(-1..1 くらい)
struct Ball { float x, y, vx, vy, r, ph, home, life; bool small; };
struct Scratch { float* f; float* gx; float* gy; };     // 1行ぶん、格子の列ごとの場と傾き

static const char* SKETCH_NAME = "NullDrop v6";

static constexpr int BAND    = 8;
static constexpr int NSLOT   = 4;
static constexpr int MAXNB   = 64;

static int   W, H, S, XOFF, YOFF, NB;
static float Sc, VR;                           // 中心 / 見えている半径(ピクセル)
static float vx0, vx1, vy0, vy1;               // 見えている範囲(S×S の中の座標)
static bool  isRound;
static uint8_t*  bandVis;
static uint16_t* slotBuf[NSLOT];
static int16_t*  spanS;
static int16_t*  spanE;
static int16_t*  bandX0;
static int16_t*  bandW;
static uint32_t  busyUs[2];

static SemaphoreHandle_t semGo, semDone;
static volatile int readyW = -1, doneUpTo = -1;   // コア0が描き終えた帯 / 画面へ送り終えた帯
static bool fbMode = false;                      // StopWatch: 画面の手前に PSRAM の置き場がある(帯ごとに丸の中だけ送る)
static int  SPLIT = 2;                           // コア1は SPLIT 本に1本の帯を描く(残りはコア0)
static uint32_t sendUs = 0;

static float simT = 0;
static bool  imuOK = false;
static float tiltX = 0, tiltY = 0;               // 傾き(なめらかにしたもの)
static float levelX = 0, levelY = 0;            // 「水平」の基準(起動したときの姿勢)
static int   levelN = 0;                         // 基準を取った回数
static int   brightIdx = 2;
static const uint8_t brightSteps[] = {60, 120, 190, 255};

// ---------------------------------------------------------------- 道具
static inline float clamp01(float v) { return v < 0 ? 0 : (v > 1 ? 1 : v); }
static inline Col lerpC(Col a, Col b, float t) { return {a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t}; }
static inline Col mulC(Col a, float k) { return {a.r * k, a.g * k, a.b * k}; }
static inline Col C8(int r, int g, int b) { return {r / 255.0f, g / 255.0f, b / 255.0f}; }
// 色 → 565(バイト順はふつう)
static inline uint16_t to565(Col c) {
  const int r = (int)(clamp01(c.r) * 255.0f + 0.5f), g = (int)(clamp01(c.g) * 255.0f + 0.5f), b = (int)(clamp01(c.b) * 255.0f + 0.5f);
  return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}
static inline uint16_t rgb565(int r, int g, int b) {
  r = r < 0 ? 0 : r > 255 ? 255 : r; g = g < 0 ? 0 : g > 255 ? 255 : g; b = b < 0 ? 0 : b > 255 ? 255 : b;
  return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}
// 光を足す(565 のまま、色ごとに頭打ち)
static inline uint16_t add565(uint32_t a, uint32_t b) {
  const uint32_t s = (a & 0xF7DE) + (b & 0xF7DE);
  const uint32_t ov = s & 0x10820;
  return (uint16_t)(s | (ov - (ov >> 5)));
}
// 2つの色を a/16 で混ぜる(a=16 で b)
static inline uint16_t mix565(uint32_t a, uint32_t b, int q) {
  const uint32_t A = (a | (a << 16)) & 0x07E0F81Fu, B = (b | (b << 16)) & 0x07E0F81Fu;
  const uint32_t M = ((B * q + A * (16 - q)) >> 4) & 0x07E0F81Fu;
  return (uint16_t)(M | (M >> 16));
}
// 画面へ出す色(バイト順を入れ替える)/ 画面の色(入れ替え済み)に光を足す
static inline uint16_t outPix(uint16_t c) { return __builtin_bswap16(c); }
static inline uint16_t addPix(uint16_t dst, uint16_t add) { return __builtin_bswap16(add565(__builtin_bswap16(dst), add)); }
static inline float frand() { return (esp_random() & 0xFFFF) / 65535.0f; }
static inline float frand(float a, float b) { return a + (b - a) * frand(); }
static inline uint32_t hash3(uint32_t x, uint32_t y, uint32_t s) {
  uint32_t h = x * 374761393u + y * 668265263u + s * 2246822519u;
  h = (h ^ (h >> 13)) * 1274126177u;
  return h ^ (h >> 16);
}
// 端がつながる値ノイズ(n×n の格子。x, y は格子の単位)
static float* makeNoise(int n, uint32_t seed) {
  float* a = (float*)malloc(n * n * sizeof(float));
  if (a) for (int i = 0; i < n * n; ++i) a[i] = (hash3(i % n, i / n, seed) & 0xFFFF) / 65535.0f;
  return a;
}
static inline float noiseAt(const float* a, int n, float x, float y) {
  const float fx0 = floorf(x), fy0 = floorf(y);
  const float fx = x - fx0, fy = y - fy0;
  const float sx = fx * fx * (3 - 2 * fx), sy = fy * fy * (3 - 2 * fy);
  int x0 = ((int)fx0 % n + n) % n, y0 = ((int)fy0 % n + n) % n;
  const int x1 = (x0 + 1) % n, y1 = (y0 + 1) % n;
  const float v00 = a[y0 * n + x0], v10 = a[y0 * n + x1], v01 = a[y1 * n + x0], v11 = a[y1 * n + x1];
  return v00 + (v10 - v00) * sx + (v01 - v00) * sy + (v00 - v10 - v01 + v11) * sx * sy;
}
// 中心からの離れ具合(距離の2乗 × 64 /(半径+1)^2)→ 段(0..7)
static uint8_t ringQ[64];

static void fail(const char* msg) {
  M5.Display.setTextDatum(middle_center);
  M5.Display.drawString(msg, M5.Display.width() / 2, M5.Display.height() / 2);
  Serial.println(msg);
  while (true) delay(1000);
}
// 内蔵メモリを優先、足りなければ PSRAM。内蔵メモリは、画面の転送やタスクのために 48KB は残しておく
static void* allocFast(size_t n) {
  if (heap_caps_get_free_size(MALLOC_CAP_INTERNAL) >= n + 48 * 1024) {
    void* p = heap_caps_malloc(n, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (p) return p;
  }
  return ps_malloc(n);
}

// ---------------------------------------------------------------- 雫
static Ball   balls[MAXBALL];
static int    nBall = 0, nBig = 6, envTheme = 0, grab = -1;
static int    GN;
static float *F, *GXa, *GYa;           // [GN*GN] 場と、その傾き(1 ピクセルあたり)
static uint16_t* envLUT;               // [ENV*ENV] 面の向き → 色(565)
static uint16_t bgRow[512];
static float  NK;                      // 傾き → 面の向き

static bool allocScratch(Scratch& s) {
  const int n = S / GS + 4;
  s.f = (float*)malloc(n * 4); s.gx = (float*)malloc(n * 4); s.gy = (float*)malloc(n * 4);
  return s.f && s.gx && s.gy;
}
static bool pieceAlloc() {
  GN = S / GS + 3;
  F = (float*)allocFast(GN * GN * 4); GXa = (float*)allocFast(GN * GN * 4); GYa = (float*)allocFast(GN * GN * 4);
  envLUT = (uint16_t*)allocFast(ENV * ENV * 2);
  return F && GXa && GYa && envLUT;
}
static void resetBalls() {
  nBall = 0;
  const float k = S / 466.0f;
  for (int i = 0; i < nBig; ++i) {
    const float a = 6.2832f * i / nBig;
    balls[nBall++] = {Sc + cosf(a) * VR * 0.45f, Sc + sinf(a) * VR * 0.45f, 0, 0, (24 + frand() * 22) * k * (nBig > 6 ? 0.8f : 1.0f), frand(0, 6.2832f), a, 1e9f, false};
  }
  grab = -1;
}
static void pieceInit() {
  NK = 6.4f * 466.0f / S;
  for (int y = 0; y < S && y < 512; ++y) { const float b = 8 + 10.0f * y / S; bgRow[y] = rgb565((int)b, (int)b, (int)(b + 6)); }
  resetBalls();
}
static void onA() { nBig = nBig == 4 ? 6 : nBig == 6 ? 9 : 4; resetBalls(); Serial.printf("drops: %d\n", nBig); }
static void onAHold() { envTheme = (envTheme + 1) % 3; Serial.printf("env: %d\n", envTheme); }
static void statusText(char* o, int n) { snprintf(o, n, "drops %d env %d", nBall, envTheme); }

// 面の向き (nx, ny) から、映る景色の色(目は正面から)
static uint16_t envColor(float nx, float ny, float t) {
  const float e = fminf(1.0f, nx * nx + ny * ny), nz = sqrtf(fmaxf(0.02f, 1 - e));
  const float rx = 2 * nz * nx, ry = 2 * nz * ny, rz = 2 * nz * nz - 1;
  const float up = -ry;
  float r, g, b;
  if (envTheme == 0) {                         // 部屋: 空色の天井、窓の光、暗い床
    if (up > 0.15f) { const float k = fminf(1.0f, (up - 0.15f) * 1.6f); r = 150 - 110 * k; g = 180 - 100 * k; b = 230 - 80 * k; }
    else if (up > -0.15f) { r = 230; g = 225; b = 215; }
    else { const float k = fminf(1.0f, (-up - 0.15f) * 2); r = 60 - 45 * k; g = 55 - 42 * k; b = 52 - 38 * k; }
  } else if (envTheme == 1) {                  // 夕焼け
    if (up > 0.15f) { const float k = fminf(1.0f, (up - 0.15f) * 1.6f); r = 250 - 150 * k; g = 140 - 90 * k; b = 90 + 40 * k; }
    else if (up > -0.15f) { r = 255; g = 200; b = 140; }
    else { const float k = fminf(1.0f, (-up - 0.15f) * 2); r = 70 - 55 * k; g = 40 - 30 * k; b = 50 - 38 * k; }
  } else {                                     // 夜: 暗い空と街の明かり
    if (up > 0.15f) { const float k = fminf(1.0f, (up - 0.15f) * 1.6f); r = 30 - 20 * k; g = 40 - 25 * k; b = 80 - 40 * k; }
    else if (up > -0.15f) { r = 120; g = 110; b = 140; }
    else { const float k = fminf(1.0f, (-up - 0.15f) * 2); r = 20 - 15 * k; g = 20 - 15 * k; b = 30 - 22 * k; }
  }
  const float win = sinf(rx * 9 + t * 0.2f) > 0.82f ? 1.0f : 0.0f;     // 窓の光の帯(縦じま)
  r += win * 90; g += win * 90; b += win * 95;
  const float facing = fmaxf(0, rz);
  r *= 0.35f + 0.65f * (1 - facing * 0.6f); g *= 0.35f + 0.65f * (1 - facing * 0.6f); b *= 0.4f + 0.6f * (1 - facing * 0.55f);
  const float sp = powf(fmaxf(0, -nx * 0.45f - ny * 0.6f + nz * 0.66f), 40) * 255 * SHINE;   // ふちの鋭い光
  return rgb565((int)(r + sp), (int)(g + sp), (int)(b + sp));
}

static void updateFrame(const Input& in) {
  float dt = in.dt > 0.05f ? 0.05f : in.dt;
  if (dt < 0.0005f) dt = 0.0005f;
  simT += dt;
  const float t = simT, k = S / 466.0f;
  // 指: ドラッグで近い雫を引く / タップで小さな雫
  if (in.touching) {
    if (grab < 0) { float best = 1e9f; for (int i = 0; i < nBall; ++i) { const float d = hypotf(balls[i].x - in.tx, balls[i].y - in.ty); if (d < best) { best = d; grab = i; } } }
  } else grab = -1;
  if (in.tapped && nBall < MAXBALL) balls[nBall++] = {in.tx, in.ty, (frand() - 0.5f) * 60 * k, -40 * k, 14 * k, frand(0, 6.2832f), 0, 6.0f, true};
  const float tm = sqrtf(in.tiltX * in.tiltX + in.tiltY * in.tiltY), pullK = 1.0f - fminf(0.85f, tm * 1.6f);   // 傾くほど、自分の場所へ戻る力を弱く
  for (int i = 0; i < nBall; ++i) {
    Ball& b = balls[i];
    float ax = in.tiltX * GRAV * k, ay = in.tiltY * GRAV * k;
    if (!b.small) {                                   // 自分の場所(円の上をゆっくり回る)のまわりをさまよう
      const float hr = VR * (0.42f + 0.3f * sinf(t * 0.11f + b.ph)), ha = b.home + t * 0.07f;
      ax += ((Sc + cosf(ha) * hr - b.x) * 0.9f + sinf(t * 0.5f + b.ph) * 25 * k) * pullK; ay += ((Sc + sinf(ha) * hr - b.y) * 0.9f + cosf(t * 0.43f + b.ph * 1.3f) * 25 * k) * pullK;
    } else ay += 60 * k * pullK;                              // 小さな雫は、少し落ちていく
    for (int j = 0; j < nBall; ++j) {
      if (j == i) continue;
      const float dx = balls[j].x - b.x, dy = balls[j].y - b.y, d = sqrtf(dx * dx + dy * dy) + 1;
      if (d < (balls[j].r + b.r) * 1.6f) { ax += dx / d * 30 * k; ay += dy / d * 30 * k; }
    }
    if (i == grab) { ax += (in.tx - b.x) * 30 - b.vx * 6; ay += (in.ty - b.y) * 30 - b.vy * 6; }
    b.vx = (b.vx + ax * dt) * expf(-dt * 1.2f); b.vy = (b.vy + ay * dt) * expf(-dt * 1.2f);
    b.x += b.vx * dt; b.y += b.vy * dt;
    const float dx = b.x - Sc, dy = b.y - Sc, d = sqrtf(dx * dx + dy * dy) + 1e-3f, lim = VR - b.r * 0.6f;
    if (d > lim) { b.x = Sc + dx / d * lim; b.y = Sc + dy / d * lim; b.vx *= -0.4f; b.vy *= -0.4f; }
    b.life -= dt;
  }
  // 小さな雫は、大きな雫に近づくと吸いこまれる(大きい方が少し育つ)。大きくなりすぎたら、ゆっくり元へ
  for (int i = nBall - 1; i >= 0; --i) {
    Ball& b = balls[i];
    if (!b.small) { b.r += ((26 + 20 * sinf(b.ph)) * k + 13 * k - b.r) * dt * 0.05f; continue; }
    for (int j = 0; j < nBall; ++j) {
      Ball& o = balls[j];
      if (o.small) continue;
      if (hypotf(o.x - b.x, o.y - b.y) < o.r * 0.7f) { o.r = fminf(70 * k, sqrtf(o.r * o.r + b.r * b.r * 0.5f)); b.life = 0; break; }
    }
    if (b.life <= 0) { balls[i] = balls[nBall - 1]; --nBall; grab = -1; }
  }
  // 場: Σ r^2 / d^2(格子の点で)と、その傾き
  for (int gy = 0; gy < GN; ++gy)
    for (int gx = 0; gx < GN; ++gx) {
      const float x = (float)(gx * GS), y = (float)(gy * GS);
      float f = 0;
      for (int i = 0; i < nBall; ++i) { const float dx = x - balls[i].x, dy = y - balls[i].y; f += balls[i].r * balls[i].r / (dx * dx + dy * dy + 1); }
      F[gy * GN + gx] = f;
    }
  for (int gy = 0; gy < GN; ++gy)
    for (int gx = 0; gx < GN; ++gx) {
      const int p = gy * GN + gx;
      const int l = gx > 0 ? p - 1 : p, r = gx < GN - 1 ? p + 1 : p, u = gy > 0 ? p - GN : p, d = gy < GN - 1 ? p + GN : p;
      GXa[p] = (F[r] - F[l]) / ((r - l) * GS); GYa[p] = (F[d] - F[u]) / (((d - u) / GN) * GS);
    }
  // 面の向き → 色の表(窓の光が少しずつ動くので、毎フレーム作り直す)
  for (int j = 0; j < ENV; ++j)
    for (int i = 0; i < ENV; ++i)
      envLUT[j * ENV + i] = envColor((i + 0.5f) / ENV * 2 - 1, (j + 0.5f) / ENV * 2 - 1, t);
}

static void renderBand(int k, uint16_t* buf, Scratch& sc) {
  const int y0 = k * BAND, y1 = min(S, y0 + BAND);
  const int bx0 = bandX0[k], bw = bandW[k], bx1 = bx0 + bw - 1;
  for (int y = y0; y < y1; ++y) {
    uint16_t* row = buf + (y - y0) * bw - bx0;
    const int xs = max((int)spanS[y], bx0), xe = min((int)spanE[y], bx1);
    for (int x = bx0; x < xs; ++x) row[x] = 0;
    for (int x = max(xe + 1, bx0); x <= bx1; ++x) row[x] = 0;
    if (xs > xe) continue;
    const int gy = y / GS; const float fy = (float)(y - gy * GS) / GS;
    const int gx0 = xs / GS, gx1 = xe / GS + 1;
    for (int gx = gx0; gx <= gx1; ++gx) {
      const int p = gy * GN + gx, q = p + GN, o = gx - gx0;
      sc.f[o] = F[p] + (F[q] - F[p]) * fy; sc.gx[o] = GXa[p] + (GXa[q] - GXa[p]) * fy; sc.gy[o] = GYa[p] + (GYa[q] - GYa[p]) * fy;
    }
    const uint16_t bg = bgRow[y < 512 ? y : 511];
    int x = xs;
    while (x <= xe) {
      const int gx = x / GS, segEnd = min(xe, gx * GS + GS - 1), o = gx - gx0;
      const float inv = 1.0f / GS, t0 = (float)(x - gx * GS) * inv;
      const float df = (sc.f[o + 1] - sc.f[o]) * inv, dgx = (sc.gx[o + 1] - sc.gx[o]) * inv, dgy = (sc.gy[o + 1] - sc.gy[o]) * inv;
      float f = sc.f[o] + df * t0 * GS, ggx = sc.gx[o] + dgx * t0 * GS, ggy = sc.gy[o] + dgy * t0 * GS;
      for (; x <= segEnd; ++x, f += df, ggx += dgx, ggy += dgy) {
        if (f < THRESH) {                              // 床(雫のまわりは影で暗く)
          int q = (int)((f - 0.25f) * 1.2f * 0.7f * 16); q = q < 0 ? 0 : q > 11 ? 11 : q;
          row[x] = outPix(q ? mix565(bg, 0, q) : bg);
          continue;
        }
        float nx = -ggx * NK, ny = -ggy * NK;          // 面の向き(ふちほど横向き)
        const float l2 = nx * nx + ny * ny;
        if (l2 > 0.999f) { const float s = 0.999f / sqrtf(l2); nx *= s; ny *= s; }
        int ix = (int)((nx + 1) * (ENV * 0.5f)), iy = (int)((ny + 1) * (ENV * 0.5f));
        ix = ix < 0 ? 0 : ix > ENV - 1 ? ENV - 1 : ix; iy = iy < 0 ? 0 : iy > ENV - 1 ? ENV - 1 : iy;
        row[x] = outPix(envLUT[iy * ENV + ix]);
      }
    }
  }
}

static Scratch scr[2];

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
      if (fbMode) {                                   // 置き場に書いて、この帯の丸の中の幅だけをすぐ画面へ送る
        const uint32_t t0 = micros();
        M5.Display.pushImage(XOFF + bandX0[k], YOFF + y0, bandW[k], h, slotBuf[k % NSLOT]);
        send += micros() - t0;
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
  VR = 0.5f * min(W, H);
  vx0 = (float)max(0, -XOFF); vx1 = (float)min(S, W - XOFF) - 1;
  vy0 = (float)max(0, -YOFF); vy1 = (float)min(S, H - YOFF) - 1;
  Serial.println(SKETCH_NAME);
  Serial.printf("screen %dx%d  area %d\n", W, H, S);

  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setBrightness(brightSteps[brightIdx]);

  for (int i = 0; i < NSLOT; ++i) slotBuf[i] = (uint16_t*)heap_caps_malloc(S * BAND * 2, MALLOC_CAP_DMA);
  spanS  = (int16_t*)malloc(S * sizeof(int16_t)); spanE = (int16_t*)malloc(S * sizeof(int16_t));
  bandX0 = (int16_t*)malloc(NB * sizeof(int16_t)); bandW = (int16_t*)malloc(NB * sizeof(int16_t));
  bandVis = (uint8_t*)malloc(NB);
  bool ok = spanS && spanE && bandX0 && bandW && bandVis && NB <= MAXNB;
  for (int i = 0; i < NSLOT; ++i) ok = ok && slotBuf[i];
  for (int c = 0; c < 2; ++c) ok = ok && allocScratch(scr[c]);
  ok = ok && pieceAlloc();
  if (!ok) fail("Memory error");

  // 丸い画面: 行ごとに、丸の中の左右の端。帯ごとに、送る幅(偶数にそろえる)
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
  for (int i = 0; i < 64; ++i) { int q = (int)(sqrtf(i / 64.0f) * 8.0f); ringQ[i] = (uint8_t)(q > 7 ? 7 : q); }

  const uint32_t t0 = millis();
  pieceInit();
  Serial.printf("prepared in %u ms\n", (unsigned)(millis() - t0));

  imuOK = TILT && M5.Imu.isEnabled();
  Serial.printf("tilt sensor: %s\n", imuOK ? "ok" : "not used");
  Input in0 = {false, false, -1000, -1000, 0.016f, 0, 0};
  updateFrame(in0);

  // StopWatch の M5GFX は、画面の手前に PSRAM の置き場を持ち、描き終わるたびに自動で画面へ送る
  { auto* pnl = M5.Display.getPanel(); fbMode = pnl && pnl->getAutoDisplay(); }
  SPLIT = fbMode ? 3 : 2;
  Serial.printf("display: %s\n", fbMode ? "framebuffer (send each band)" : "direct DMA");
  semGo   = xSemaphoreCreateBinary();
  semDone = xSemaphoreCreateBinary();
  xTaskCreatePinnedToCore(workerTask, "band", 8192, nullptr, 1, nullptr, 0);
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
    if (M5.BtnA.wasHold()) onAHold();
    else if (M5.BtnA.wasClicked()) onA();
    if (M5.BtnB.wasPressed()) {
      brightIdx = (brightIdx + 1) % sizeof(brightSteps);
      M5.Display.setBrightness(brightSteps[brightIdx]);
    }
  } else {
    if (M5.BtnPWR.wasClicked()) {
      brightIdx = (brightIdx + 1) % sizeof(brightSteps);
      M5.Display.setBrightness(brightSteps[brightIdx]);
    }
    if (DOUBLE_TAP_UI && pressed) {
      if (now - lastTapMs < 350) {                             // ダブルタップ: 上半分 = ボタンA、下半分 = ボタンA長押し
        if (t.y < H / 2) onA(); else onAHold();
        lastTapMs = 0;
      } else lastTapMs = now;
    }
  }

  // 傾き: 本体を傾けたとき、画面の上で「下」になる向き。向きが逆なら、調整用の TILT_FLIP_X / Y / SWAP で直す
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
      const float dts = (float)(now - last) / 1000.0f;           // (符号なしの引き算に、マイナスをつけないように)
      // 水平の基準: 起動してすぐ(はじめの 20 回)の姿勢を水平とみなす。
      // そのあとも同じ角度で持ち続けると、TILT_RELEVEL 秒くらいかけて、ゆっくりそこを水平にする
      if (levelN < 20) { ++levelN; levelX += (sx - levelX) / levelN; levelY += (sy - levelY) / levelN; }
      else if (TILT_RELEVEL > 0 && fabsf(sx - levelX) < TILT_RELEVEL_MAX && fabsf(sy - levelY) < TILT_RELEVEL_MAX) {   // 少しの傾きだけ「水平」に合わせなおす
        const float b = 1.0f - expf(-dts / TILT_RELEVEL); levelX += (sx - levelX) * b; levelY += (sy - levelY) * b; }
      sx -= levelX; sy -= levelY;
      sx *= TILT_GAIN; sy *= TILT_GAIN;
      const float m = sqrtf(sx * sx + sy * sy);                // 少しの傾きは無視(机に置いたときや、手のふるえで動かないように)
      if (m < TILT_DEAD) { sx = sy = 0; } else { const float k = (m - TILT_DEAD) / m; sx *= k; sy *= k; }
      sx = sx < -1 ? -1 : sx > 1 ? 1 : sx; sy = sy < -1 ? -1 : sy > 1 ? 1 : sy;
      const float a = 1.0f - expf(-dts * 8.0f);
      tiltX += (sx - tiltX) * a; tiltY += (sy - tiltY) * a;
    }
  }

  Input in;
  in.tiltX = tiltX; in.tiltY = tiltY;
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
    char st[48]; statusText(st, sizeof(st));
    Serial.printf("fps: %.1f  core0 %.1f ms  core1 %.1f ms  send %.1f ms  %s\n", fps, busyUs[0] / 1000.0f, busyUs[1] / 1000.0f, sendUs / 1000.0f, st);
    if (imuOK) Serial.printf("tilt: x %+.2f  y %+.2f  (水平の基準 x %+.2f  y %+.2f)  右を下げると x が +、手前(画面の下)を下げると y が + なら正しい向き\n", tiltX, tiltY, levelX, levelY);
    frames = 0; fpsTimer = millis();
  }
}
