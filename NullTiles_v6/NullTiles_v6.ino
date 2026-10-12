// NullTiles v6  -  揺れる鏡のタイル。外壁のような鏡のタイルが風の波でうねり、空を映してきらめく
//   null²(大阪・関西万博のパビリオン)から受けたイメージで作った、非公式の習作です。作品の映像・名前・データは使っていません。
//   M5Stack StopWatch 用(丸い画面)。CoreS3 系でも画面いっぱいに動きます(自動で切り替わります)。
//
// 画面の中:
//   ・小さな鏡のタイルが並んだ面。タイルの傾きが風の波でゆっくりうねり、空と雲と太陽を映してきらめく
//   ・ときどき、横から風の大きな波が通りすぎる
//
// 傾けると(本体の傾きセンサー):
//   ・手鏡をのぞくように、タイルに映る空がずれる。タイルも少しだけ、そちらへ傾く
//   ・傾きの向きが逆のときは、調整用の TILT_FLIP_X / TILT_FLIP_Y / TILT_SWAP を変える。Serial に tilt: の値が出る
//
// 触ると:
//   ・タップ: その場所から、タイルの揺れが波紋のように広がる
//   ・押したまま: 小さな波紋が続けて生まれる
//
// 操作:
//   StopWatch … ボタンA 短押し: タイルの細かさ(ふつう / 細かい)   ボタンA 長押し: 空(昼 / 夕焼け)   ボタンB: 明るさ
//   CoreS3系  … 画面の上半分をダブルタップ: 細かさ   下半分をダブルタップ: 空   電源ボタン短押し: 明るさ
//
// 必要なもの:
//   ボードマネージャー M5Stack >= 3.3.7 / ボード: M5StopWatch(PSRAM は OPI)、CoreS3 なら M5CoreS3
//   ライブラリ M5Unified >= 0.2.16, M5GFX >= 0.2.21
//
// しくみ:
//   タイルごとに、傾き(ばねで、風と波紋の向きへ)と、映る空の色(明るい色と、少し暗い色)を毎フレーム計算します。
//   ピクセルでは、表から「どのタイルの、どのあたりか」を読んで、2つの色を混ぜるだけです。
//   2つのコアで帯を描き、StopWatch では帯ごとに丸の中だけを画面へ送ります。

#include <M5Unified.h>
#include <math.h>
#include <algorithm>

#pragma GCC optimize("O3")
#ifndef IRAM_ATTR
#define IRAM_ATTR
#endif

// ---- 調整用 ----------------------------------------------------------------
static constexpr int   TILES_A   = 26;      // タイルの数(1辺。ふつう)
static constexpr int   TILES_B   = 40;      // タイルの数(1辺。細かい)
static constexpr float SWAY      = 1.0f;    // 風の強さ
static constexpr int   MAXRIP    = 24;      // 同時に広がる波紋の数
static constexpr bool  DOUBLE_TAP_UI = true;
// ---------------------------------------------------------------------------
static constexpr int MAXT = TILES_B;

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
struct Ripple { float x, y, t; uint8_t kind; };   // 0: タップ / 1: 押したまま / 2: 風
struct Scratch { int dummy; };

static const char* SKETCH_NAME = "NullTiles v6";

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

// ---------------------------------------------------------------- タイル
static int    NT = TILES_A, skyTheme = 0;
static float  TS;                       // タイルの大きさ(ピクセル)
static float *ax, *ay, *avx, *avy;      // [MAXT*MAXT] 傾きと、その速さ
static uint16_t *colA, *colB;           // [MAXT*MAXT] タイルの明るい色・暗い色
static uint8_t *xT, *xQ, *xGap;         // [S] 列ごと: どのタイルか / タイルの中の位置(0..8)/ すきまか
static Ripple rip[MAXRIP];
static int    nRip = 0;
static float  gustT = 0, pressT = 0;
static float* cloudN;

static bool allocScratch(Scratch&) { return true; }
static bool pieceAlloc() {
  ax = (float*)malloc(MAXT * MAXT * 4); ay = (float*)malloc(MAXT * MAXT * 4);
  avx = (float*)malloc(MAXT * MAXT * 4); avy = (float*)malloc(MAXT * MAXT * 4);
  colA = (uint16_t*)malloc(MAXT * MAXT * 2); colB = (uint16_t*)malloc(MAXT * MAXT * 2);
  xT = (uint8_t*)malloc(S); xQ = (uint8_t*)malloc(S); xGap = (uint8_t*)malloc(S);
  cloudN = makeNoise(32, 77);
  return ax && ay && avx && avy && colA && colB && xT && xQ && xGap && cloudN;
}
static void layoutTiles() {
  TS = (float)S / NT;
  const float gap = fmaxf(1.0f, TS * 0.08f);
  for (int x = 0; x < S; ++x) {
    const int t = min(NT - 1, (int)(x / TS));
    const float u = x - t * TS;
    xT[x] = (uint8_t)t; xQ[x] = (uint8_t)fminf(8.0f, u / TS * 8.0f);
    xGap[x] = (u < gap * 0.5f || u > TS - gap * 0.5f) ? 1 : 0;
  }
  for (int i = 0; i < MAXT * MAXT; ++i) ax[i] = ay[i] = avx[i] = avy[i] = 0;
}
static void pieceInit() { layoutTiles(); }
static void onA() { NT = NT == TILES_A ? TILES_B : TILES_A; layoutTiles(); Serial.printf("tiles: %d\n", NT); }
static void onAHold() { skyTheme ^= 1; Serial.printf("sky: %d\n", skyTheme); }
static void statusText(char* o, int n) { snprintf(o, n, "tiles %d ripples %d", NT, nRip); }

static void addRipple(float x, float y, int kind) {
  if (nRip == MAXRIP) { for (int i = 1; i < MAXRIP; ++i) rip[i - 1] = rip[i]; --nRip; }
  rip[nRip++] = {x, y, 0, (uint8_t)kind};
}
// 反射の向き → 空の色
static Col skyAt(float rx, float ry) {
  const float h = clamp01((ry + 0.8f) / 1.3f);                 // 0: 天頂 → 1: 地平
  Col c;
  if (skyTheme == 0) c = {(25 + 150 * h * h) / 255, (60 + 150 * h) / 255, (150 + 95 * h) / 255};   // 地平は明るい水色
  else c = {(40 + 215 * h) / 255, (30 + 120 * h * h) / 255, (90 - 20 * h) / 255};
  const float n = noiseAt(cloudN, 32, rx * 5 + 3, ry * 9 + 3);
  const float cl = fmaxf(0, n - 0.55f) * 2.6f * (1 - h * 0.5f);
  const Col cc = skyTheme == 0 ? C8(245, 245, 250) : C8(255, 190, 150);
  c = lerpC(c, cc, clamp01(cl));
  const float d = sqrtf((rx - 0.3f) * (rx - 0.3f) + (ry + 0.3f) * (ry + 0.3f));
  const float sun = fmaxf(0, 1 - d / 0.1f), glow = expf(-d / 0.18f);
  c.r += sun + 0.47f * glow; c.g += 0.96f * sun + 0.39f * glow; c.b += 0.86f * sun + 0.24f * glow;
  return c;
}

static void updateFrame(const Input& in) {
  float dt = in.dt > 0.05f ? 0.05f : in.dt;
  if (dt < 0.0005f) dt = 0.0005f;
  simT += dt;
  const float t = simT, k = S / 466.0f;
  if (in.tapped) addRipple(in.tx, in.ty, 0);
  if (in.touching) { pressT += dt; if (pressT > 0.12f) { pressT = 0; addRipple(in.tx, in.ty, 1); } } else pressT = 0;
  gustT -= dt;
  if (gustT < 0) { gustT = frand(3, 7); addRipple(frand() < 0.5f ? -40 * k : S + 40 * k, frand(0, (float)S), 2); }
  for (int i = 0; i < nRip; ++i) rip[i].t += dt;
  { int m = 0; for (int i = 0; i < nRip; ++i) if (rip[i].t < (rip[i].kind == 2 ? 5.0f : 3.0f)) rip[m++] = rip[i]; nRip = m; }
  const float sx = 0.018f / k, sy = 0.011f / k, sy2 = 0.016f / k, sx2 = 0.004f / k;
  for (int j = 0; j < NT; ++j)
    for (int i = 0; i < NT; ++i) {
      const int p = j * NT + i;
      const float x = (i + 0.5f) * TS, y = (j + 0.5f) * TS;
      // いつもの風: ゆっくり流れる波
      float tx = 0.12f * SWAY * sinf(x * sx + t * 0.9f) * cosf(y * sy - t * 0.5f) + in.tiltX * 0.05f;
      float ty = 0.12f * SWAY * sinf(y * sy2 - t * 0.7f + x * sx2) + in.tiltY * 0.05f;
      // 波紋: 輪の通るところで、外向きに傾く
      for (int r = 0; r < nRip; ++r) {
        const Ripple& R = rip[r];
        const float dx = x - R.x, dy = y - R.y, d = sqrtf(dx * dx + dy * dy) + 1e-3f;
        const float sp = (R.kind == 2 ? 140 : 230) * k, w = (R.kind == 2 ? 70 : 26) * k;
        const float ring = d - R.t * sp;
        if (fabsf(ring) > w * 3) continue;
        const float a = expf(-ring * ring / (w * w)) * (R.kind == 1 ? 0.25f : R.kind == 2 ? 0.35f : 0.55f) * expf(-R.t * (R.kind == 2 ? 0.4f : 1.1f));
        tx += dx / d * a; ty += dy / d * a;
      }
      // ばね(タイルの重さで、少し遅れて揺れる)
      avx[p] = (avx[p] + (tx - ax[p]) * 60 * dt) * expf(-dt * 6); avy[p] = (avy[p] + (ty - ay[p]) * 60 * dt) * expf(-dt * 6);
      ax[p] += avx[p] * dt; ay[p] += avy[p] * dt;
      // 映る空(少し下から見上げている)。タイルの中は、端に向かって少し暗く
      const Col c = skyAt(ax[p] * 3 + (x - Sc) / S * 1.3f + in.tiltX * 0.7f, ay[p] * 3 + (y - Sc) / S * 1.4f - 0.15f + in.tiltY * 0.7f);
      colA[p] = to565(c);
      colB[p] = to565({c.r * 0.72f, c.g * 0.74f, c.b * 0.8f});
    }
}

static void renderBand(int k, uint16_t* buf, Scratch&) {
  const int y0 = k * BAND, y1 = min(S, y0 + BAND);
  const int bx0 = bandX0[k], bw = bandW[k], bx1 = bx0 + bw - 1;
  const uint16_t gapC = outPix(rgb565(6, 7, 10));
  for (int y = y0; y < y1; ++y) {
    uint16_t* row = buf + (y - y0) * bw - bx0;
    const int xs = max((int)spanS[y], bx0), xe = min((int)spanE[y], bx1);
    for (int x = bx0; x < xs; ++x) row[x] = 0;
    for (int x = max(xe + 1, bx0); x <= bx1; ++x) row[x] = 0;
    if (xs > xe) continue;
    if (xGap[y]) { for (int x = xs; x <= xe; ++x) row[x] = gapC; continue; }   // 横のすきま
    const int ty = xT[y], vq = xQ[y];
    const uint16_t* A = colA + ty * NT; const uint16_t* B = colB + ty * NT;
    for (int x = xs; x <= xe; ++x) {
      if (xGap[x]) { row[x] = gapC; continue; }
      const int t = xT[x];
      row[x] = outPix(mix565(A[t], B[t], xQ[x] + vq));               // 左上は明るく、右下へ少し暗く
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
