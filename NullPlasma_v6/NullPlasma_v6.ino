// NullPlasma v6  -  空中の光の点描。光の点の集まりが、回りながら 球 → トーラス → 蝶 → 二重らせん と組み変わる
//   メディアアートの作品(空中に光の点を打って立体を描く作品)から受けたイメージで作った、非公式の習作です。
//   作品の映像・名前・データは使っていません。
//   M5Stack StopWatch 用(丸い画面)。CoreS3 系でも動きます(自動で切り替わります)。
//
// 画面の中:
//   ・1000 の光の点が、空中に立体を描く。点は1つずつ少し遅れて、次の形へ組み変わっていく(9 秒ごと)
//   ・点はプラズマのように、ぱちぱちと瞬く
//
// 傾けると(本体の傾きセンサー):
//   ・左右に傾けると立体が横に回り、前後に傾けると上下に回る(ドラッグしていないとき)
//   ・傾きの向きが逆のときは、調整用の TILT_FLIP_X / TILT_FLIP_Y / TILT_SWAP を変える。Serial に tilt: の値が出る
//
// 触ると:
//   ・ドラッグ: 立体を回す(離すと、ゆっくりいつもの回り方に戻る)
//   ・タップ: 点がはじけて散り、また集まって形に戻る
//
// 操作:
//   StopWatch … ボタンA 短押し: 次の形へ   ボタンA 長押し: 色(紫 / 青白 / 琥珀)   ボタンB: 明るさ
//   CoreS3系  … 画面の上半分をダブルタップ: 次の形   下半分をダブルタップ: 色   電源ボタン短押し: 明るさ
//
// 必要なもの:
//   ボードマネージャー M5Stack >= 3.3.7 / ボード: M5StopWatch(PSRAM は OPI)、CoreS3 なら M5CoreS3
//   ライブラリ M5Unified >= 0.2.16, M5GFX >= 0.2.21
//
// しくみ:
//   形は起動時に点の位置の表にしておき、点はばねで今の形の位置へ引かれます。
//   毎フレーム、点を回して遠近をつけ、画面の8行ずつの帯ごとに振り分けてから、光を足して描きます。
//   画面は帯ごとに2つのコアで描きます。StopWatch では、帯ごとに丸の中だけを画面へ送ります。

#include <M5Unified.h>
#include <math.h>
#include <algorithm>

#pragma GCC optimize("O3")
#ifndef IRAM_ATTR
#define IRAM_ATTR
#endif

// ---- 調整用 ----------------------------------------------------------------
static constexpr int   NPT       = 1000;    // 光の点の数
static constexpr float MORPH_SEC = 9.0f;    // 形が変わる間隔(秒)
static constexpr float SPIN      = 0.25f;   // いつもの回る速さ(ラジアン/秒)
static constexpr float SPARK     = 1.0f;    // 瞬きの強さ(0 で瞬かない)
static constexpr float GLOW      = 1.0f;    // 点の明るさ
static constexpr bool  DOUBLE_TAP_UI = true;
// ---------------------------------------------------------------------------
static constexpr int NSHAPE    = 4;
static constexpr int MAXDOT    = NPT;
static constexpr int MAXDOTENT = MAXDOT * 4;

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
struct DotDraw { int16_t x, y; uint8_t r, a; };
struct Scratch { int dummy; };

static const char* SKETCH_NAME = "NullPlasma v6";

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

// ---------------------------------------------------------------- 光の点(帯ごとに振り分けて、光を足して描く)
static DotDraw* dots;                 // [MAXDOT]
static int      nDot = 0;
static int16_t* dotEnt;               // [MAXDOTENT] 帯ごとの、点の番号
static int16_t  dotStart[MAXNB + 1];
static uint16_t dotCol[16][8];        // [明るさの段][中心からの段] 足す色(565)
static void buildDotColors(Col c, float gain) {
  static const float prof[8] = {1.0f, 0.82f, 0.52f, 0.30f, 0.17f, 0.09f, 0.045f, 0.0f};
  for (int a = 0; a < 16; ++a) for (int q = 0; q < 8; ++q) dotCol[a][q] = to565(mulC(c, gain * a / 15.0f * prof[q]));
}
static inline void addDot(float x, float y, int r, int a) {
  if (nDot >= MAXDOT || a <= 0) return;
  if (r < 1) r = 1;
  if (r > 12) r = 12;
  dots[nDot++] = {(int16_t)x, (int16_t)y, (uint8_t)r, (uint8_t)(a > 15 ? 15 : a)};
}
static void binDots() {
  for (int k = 0; k <= NB; ++k) dotStart[k] = 0;
  int total = 0, na = nDot;
  for (int i = 0; i < na; ++i) {
    const int a = max(0, dots[i].y - dots[i].r) / BAND, b = min(S - 1, max(0, dots[i].y + dots[i].r)) / BAND;
    if (total + (b - a + 1) > MAXDOTENT) { na = i; break; }
    for (int k = a; k <= b; ++k) dotStart[k + 1]++;
    total += b - a + 1;
  }
  for (int k = 0; k < NB; ++k) dotStart[k + 1] += dotStart[k];
  int16_t fill[MAXNB];
  for (int k = 0; k < NB; ++k) fill[k] = dotStart[k];
  for (int i = 0; i < na; ++i) {
    const int a = max(0, dots[i].y - dots[i].r) / BAND, b = min(S - 1, max(0, dots[i].y + dots[i].r)) / BAND;
    for (int k = a; k <= b; ++k) dotEnt[fill[k]++] = (int16_t)i;
  }
}
static void drawDots(int k, uint16_t* buf, int y0, int y1, int bx0, int bx1, int bw) {
  for (int e = dotStart[k]; e < dotStart[k + 1]; ++e) {
    const DotDraw& d = dots[dotEnt[e]];
    const int R = d.r, R1 = (R + 1) * (R + 1);
    const uint16_t* col = dotCol[d.a];
    for (int y = max(y0, d.y - R); y <= min(y1 - 1, d.y + R); ++y) {
      const int dy = y - d.y, xs = max(max((int)spanS[y], bx0), d.x - R), xe = min(min((int)spanE[y], bx1), d.x + R);
      uint16_t* row = buf + (y - y0) * bw - bx0;
      for (int x = xs; x <= xe; ++x) {
        const int dx = x - d.x, d2 = dx * dx + dy * dy;
        if (d2 >= R1) continue;
        const uint16_t c = col[ringQ[d2 * 64 / R1]];
        if (c) row[x] = addPix(row[x], c);
      }
    }
  }
}

// ---------------------------------------------------------------- 点と形
static float* shapes;                  // [NSHAPE * NPT * 3]
static float *P, *V, *delayT, *seed;
static int    shapeIdx = 0, theme = 0;
static float  shapeT = 0, yaw = 0.4f, pitch = -0.25f, yawV = SPIN, pitchV = 0, burst = 0;
static bool   dragging = false;
static float  lastX = 0, lastY = 0;
static uint16_t airLUT[64];            // 中心からの距離の段 → 背景の光(565)
static Col    airCol;

static bool allocScratch(Scratch&) { return true; }

static bool pieceAlloc() {
  shapes = (float*)ps_malloc(sizeof(float) * NSHAPE * NPT * 3);
  P = (float*)allocFast(sizeof(float) * NPT * 3); V = (float*)allocFast(sizeof(float) * NPT * 3);
  delayT = (float*)allocFast(sizeof(float) * NPT); seed = (float*)allocFast(sizeof(float) * NPT);
  dots = (DotDraw*)allocFast(sizeof(DotDraw) * MAXDOT);
  dotEnt = (int16_t*)allocFast(sizeof(int16_t) * MAXDOTENT);
  return shapes && P && V && delayT && seed && dots && dotEnt;
}

static void buildShapes() {
  const float golden = 3.14159265f * (3.0f - sqrtf(5.0f));
  float* s = shapes;
  for (int i = 0; i < NPT; ++i) {                              // 球
    const float y = 1 - (i + 0.5f) / NPT * 2, r = sqrtf(1 - y * y), th = i * golden;
    s[i * 3] = cosf(th) * r; s[i * 3 + 1] = y; s[i * 3 + 2] = sinf(th) * r;
  }
  s += NPT * 3;
  for (int i = 0; i < NPT; ++i) {                              // トーラス
    const float u = (float)i / NPT * 6.2831853f, v = i * golden, R0 = 0.68f, r0 = 0.3f;
    s[i * 3] = (R0 + r0 * cosf(v)) * cosf(u); s[i * 3 + 1] = r0 * sinf(v); s[i * 3 + 2] = (R0 + r0 * cosf(v)) * sinf(u);
  }
  s += NPT * 3;
  for (int i = 0; i < NPT; ++i) {                              // 蝶(羽の輪郭の内側を埋める)
    const float th = (float)i / NPT * 3.14159265f * 24, k = 0.35f + 0.65f * fmodf(i * 0.61803f, 1.0f);
    const float r = expf(sinf(th)) - 2 * cosf(4 * th) + powf(sinf((2 * th - 3.14159265f) / 24), 5);
    const float x = sinf(th) * r * 0.21f * k, y = -cosf(th) * r * 0.21f * k;
    s[i * 3] = x; s[i * 3 + 1] = y + 0.1f; s[i * 3 + 2] = fabsf(x) * 0.55f * sinf((float)i);
  }
  s += NPT * 3;
  for (int i = 0; i < NPT; ++i) {                              // 二重らせん + はしご
    const int strand = i % 3; const float t = (float)i / NPT * 2 - 1, ang = t * 3.14159265f * 3.2f;
    if (strand < 2) { const float o = strand ? 3.14159265f : 0; s[i * 3] = cosf(ang + o) * 0.42f; s[i * 3 + 1] = t * 0.95f; s[i * 3 + 2] = sinf(ang + o) * 0.42f; }
    else { const float f = fmodf(i * 0.37f, 1.0f) * 2 - 1, tt = roundf(t * 18) / 18, aa = tt * 3.14159265f * 3.2f;
           s[i * 3] = cosf(aa) * 0.42f * f; s[i * 3 + 1] = tt * 0.95f; s[i * 3 + 2] = sinf(aa) * 0.42f * f; }
  }
}

static void buildColors() {
  static const Col dotC[3] = { C8(235, 215, 255), C8(205, 230, 255), C8(255, 215, 150) };
  static const Col airC[3] = { C8(60, 40, 110), C8(30, 55, 100), C8(100, 60, 25) };
  buildDotColors(dotC[theme], GLOW);
  airCol = airC[theme];
}

static void pieceInit() {
  buildShapes();
  for (int i = 0; i < NPT; ++i) {
    for (int c = 0; c < 3; ++c) { P[i * 3 + c] = shapes[i * 3 + c]; V[i * 3 + c] = 0; }
    delayT[i] = frand() * 0.6f; seed[i] = frand() * 100.0f;
  }
  buildColors();
}

static void nextShape() { shapeT = 0; shapeIdx = (shapeIdx + 1) % NSHAPE; for (int i = 0; i < NPT; ++i) delayT[i] = frand() * 0.8f; }
static void onA() { nextShape(); Serial.printf("shape: %d\n", shapeIdx); }
static void onAHold() { theme = (theme + 1) % 3; buildColors(); Serial.printf("color: %d\n", theme); }
static void statusText(char* o, int n) { snprintf(o, n, "shape %d", shapeIdx); }

static void updateFrame(const Input& in) {
  float dt = in.dt > 0.05f ? 0.05f : in.dt;
  if (dt < 0.0005f) dt = 0.0005f;
  simT += dt;
  shapeT += dt;
  if (shapeT > MORPH_SEC) nextShape();
  // 指: ドラッグで回す、タップではじける
  if (in.touching) {
    if (!dragging) { dragging = true; lastX = in.tx; lastY = in.ty; }
    const float dx = in.tx - lastX, dy = in.ty - lastY; lastX = in.tx; lastY = in.ty;
    const float k = 0.01f * 466.0f / S;              // 指を 1 ピクセル動かすと 0.01 ラジアン回る(離すと、その勢いが少し残る)
    yawV = dx * k / dt; pitchV = dy * k / dt;
  } else { dragging = false; yawV += (SPIN + in.tiltX * 1.8f - yawV) * (1 - expf(-dt * 1.5f)); pitchV *= expf(-dt * 2); }
  if (in.tapped) {
    burst = 1;
    for (int i = 0; i < NPT; ++i) {
      const float x = P[i * 3], y = P[i * 3 + 1], z = P[i * 3 + 2], l = sqrtf(x * x + y * y + z * z) + 0.2f, k = 2.2f + frand() * 2.5f;
      V[i * 3] += x / l * k + (frand() - 0.5f); V[i * 3 + 1] += y / l * k + (frand() - 0.5f); V[i * 3 + 2] += z / l * k + (frand() - 0.5f);
    }
  }
  burst *= expf(-dt * 2);
  yaw += yawV * dt;
  pitch += pitchV * dt + (-0.25f + in.tiltY * 0.9f - pitch) * dt * (0.3f + 0.9f * fminf(1.0f, fabsf(in.tiltY) * 3));
  pitch = pitch < -1.2f ? -1.2f : pitch > 1.2f ? 1.2f : pitch;
  // 点: ばねで、今の形の位置へ(1点ずつ少し遅れて)
  const float* tgt = shapes + shapeIdx * NPT * 3;
  const float kS = 9.0f, damp = expf(-dt * 3.5f);
  for (int i = 0; i < NPT; ++i) {
    const float go = shapeT > delayT[i] ? 1.0f : 0.05f;
    for (int c = 0; c < 3; ++c) {
      const int j = i * 3 + c;
      const float wob = 0.012f * sinf(simT * 3 + seed[i] + c);
      V[j] = (V[j] + (tgt[j] + wob - P[j]) * kS * go * dt) * damp;
      P[j] += V[j] * dt;
    }
  }
  // 回して遠近をつけ、描く点にする
  const float cy = cosf(yaw), sy = sinf(yaw), cp = cosf(pitch), sp = sinf(pitch), D = 3.2f, sc = VR * 0.62f, rs = S / 466.0f;
  nDot = 0;
  for (int i = 0; i < NPT; ++i) {
    const float x = P[i * 3], y = P[i * 3 + 1], z = P[i * 3 + 2];
    const float x1 = x * cy + z * sy, z1 = -x * sy + z * cy;
    const float y2 = y * cp - z1 * sp, z2 = y * sp + z1 * cp;
    const float k = D / (D - z2), X = Sc + x1 * sc * k, Y = Sc - y2 * sc * k;
    const float fl = sinf(simT * 37 + seed[i] * 7.1f) * sinf(simT * 23.3f + seed[i] * 3.3f);
    const float a = fmaxf(0, 0.35f + 0.65f * fl * SPARK + (1 - SPARK) * 0.3f) * (0.55f + 0.45f * (z2 + 1) * 0.5f);
    if (a < 0.05f) continue;
    const int r = (int)((3.2f + 2.2f * k) * (0.8f + 0.4f * a) * rs + 0.5f);
    addDot(X, Y, r, (int)(fminf(1.0f, a) * 15.0f + 0.5f));
  }
  binDots();
  // 背景の、ふわっとした空気の光
  const float g = 0.22f + 0.25f * burst;
  for (int i = 0; i < 64; ++i) { const float d = i / 63.0f; airLUT[i] = to565(mulC(airCol, g * (1 - d) * (1 - d) * 0.9f)); }
}

static void renderBand(int k, uint16_t* buf, Scratch&) {
  const int y0 = k * BAND, y1 = min(S, y0 + BAND);
  const int bx0 = bandX0[k], bw = bandW[k], bx1 = bx0 + bw - 1;
  const float inv = 63.0f / (VR * VR);
  for (int y = y0; y < y1; ++y) {
    uint16_t* row = buf + (y - y0) * bw - bx0;
    const int xs = max((int)spanS[y], bx0), xe = min((int)spanE[y], bx1);
    for (int x = bx0; x < xs; ++x) row[x] = 0;
    for (int x = max(xe + 1, bx0); x <= bx1; ++x) row[x] = 0;
    if (xs > xe) continue;
    const float dy = y - Sc * 1.05f, dy2 = dy * dy;
    for (int x = xs; x <= xe; ++x) {
      const float dx = x - Sc; int q = (int)((dx * dx + dy2) * inv);
      row[x] = outPix(airLUT[q > 63 ? 63 : q]);
    }
  }
  drawDots(k, buf, y0, y1, bx0, bx1, bw);
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
