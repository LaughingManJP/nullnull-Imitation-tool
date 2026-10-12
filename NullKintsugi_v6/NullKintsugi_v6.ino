// NullKintsugi v6  -  金継ぎの計算。黒い漆の器を叩くとヒビが走り、そのヒビに金がゆっくり流れこんで継がれていく
//   メディアアートの作品(侘び寂びや民藝と計算機を重ねる考え方)から受けたイメージで作った、非公式の習作です。
//   作品の映像・名前・データは使っていません。
//   M5Stack StopWatch 用(丸い画面)。CoreS3 系でも動きます(自動で切り替わります。器は画面のまん中に丸く)。
//
// 画面の中:
//   ・黒い漆の器。叩くとヒビが、ほぼまっすぐ、ときどきカクッと折れながら枝分かれして走る
//   ・少し遅れて、ヒビに金が流れこんで継がれていく。金の上を、光の帯がゆっくりすべる
//   ・しばらく触らなくても、ときどき自然にヒビが入る。器が線でいっぱいになると、ゆっくり消えて新しい器になる
//
// 傾けると(本体の傾きセンサー):
//   ・ヒビに流れこむ金が、低い方へ先に流れる(ヒビが入ったときの傾きで決まる)。金の上の光の帯も、低い方を向く
//   ・傾きの向きが逆のときは、調整用の TILT_FLIP_X / TILT_FLIP_Y / TILT_SWAP を変える。Serial に tilt: の値が出る
//
// 触ると:
//   ・タップした所からヒビが入る
//
// 操作:
//   StopWatch … ボタンA 短押し: どこかにヒビを入れる   ボタンA 長押し: 金継ぎ / 銀継ぎ   ボタンB: 明るさ
//   CoreS3系  … 電源ボタン短押し: 明るさ(タップはヒビ)
//
// 必要なもの:
//   ボードマネージャー M5Stack >= 3.3.7 / ボード: M5StopWatch(PSRAM は OPI)、CoreS3 なら M5CoreS3
//   ライブラリ M5Unified >= 0.2.16, M5GFX >= 0.2.21
//
// しくみ:
//   ヒビと金は、1ピクセル1バイトの「層」(PSRAM)に描きたしていきます(0: なし / 1..30: ヒビのふちの光 /
//   31..90: ヒビ / 128..255: 金のかかり具合)。漆の面は、映り込みの帯・器の丸み・木目を表から読んで、その場で計算します。
//   金の色は、光の帯の位置に合わせて毎フレーム表を作り直します。
//   画面は8行ずつの帯に分けて、2つのコアで描きます。StopWatch では、帯ごとに丸の中だけを画面へ送ります。

#include <M5Unified.h>
#include <math.h>
#include <algorithm>

#pragma GCC optimize("O3")
#ifndef IRAM_ATTR
#define IRAM_ATTR
#endif

// ---- 調整用 ----------------------------------------------------------------
static constexpr float AUTO_SEC    = 5.5f;   // 触らないとき、自然にヒビが入る間隔(秒)
static constexpr float CRACK_SPEED = 520.0f; // ヒビの走る速さ(466 の画面でのピクセル/秒)
static constexpr float GOLD_SPEED  = 95.0f;  // 金が流れこむ速さ(同じ)
static constexpr float GOLD_DELAY  = 0.9f;   // ヒビが入ってから金が流れはじめるまで(秒)
static constexpr int   MAX_CRACKS  = 40;     // この数のヒビが入ると、ゆっくり新しい器になる
static constexpr bool  DOUBLE_TAP_UI = false;
// ---------------------------------------------------------------------------
static constexpr int MAXSEG = 9000;

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
struct Seg { float x0, y0, x1, y1, w, tc, tg; };
struct Scratch { int dummy; };
struct Flash { float x, y, t; };

static const char* SKETCH_NAME = "NullKintsugi v6";

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

// ---------------------------------------------------------------- 器と、ヒビと金
static uint8_t* layer;                 // [S*S] ヒビと金(PSRAM)
static Seg*     segs;                  // [MAXSEG]
static int      nSeg = 0;
static int16_t* crackQ;                // ヒビを描く順番(まだ描いていないもの)
static int16_t* goldQ;                 // 金を流す順番(まだ流していないもの)
static int      nCQ = 0, nGQ = 0;
static float    clockT = 0, idleT = 0;
static int      cracks = 0, silver = 0;
static int      fadeQ = 16;            // 層の見え方(16: そのまま / 0: 消えた)
static int      fadeDir = 0;
static Flash    flashes[4];
static uint8_t  bandL[512], bowlL[256];
static uint8_t* grain;                 // [64*64] 木目
static uint16_t baseLUT[64];           // 漆の明るさ → 色(565)
static uint16_t goldLUT[256];          // 光の帯の位置 → 金の色(565)
static float    gux = 1, guy = 0;      // 光の帯の向き
static float    crackOX = 0, crackOY = 0, gTX = 0, gTY = 0;   // ヒビの始まり / 今の傾き
static float    PR;                    // 器の半径

static bool allocScratch(Scratch&) { return true; }

static bool pieceAlloc() {
  layer = (uint8_t*)ps_malloc((size_t)S * S);
  segs = (Seg*)ps_malloc(sizeof(Seg) * MAXSEG);
  crackQ = (int16_t*)ps_malloc(sizeof(int16_t) * MAXSEG);
  goldQ = (int16_t*)ps_malloc(sizeof(int16_t) * MAXSEG);
  grain = (uint8_t*)malloc(64 * 64);
  return layer && segs && crackQ && goldQ && grain;
}

static void pieceInit() {
  memset(layer, 0, (size_t)S * S);
  PR = isRound ? VR : VR * 0.98f;
  // 映り込みの帯(器を斜めに横切る、ぼんやりした光の帯と細い光の帯)
  for (int i = 0; i < 512; ++i) {
    const float p = (i - 256) / 256.0f * 1.4f;
    const float band = expf(-powf((p + 0.35f) / 0.18f, 2)) * 0.55f + expf(-powf((p - 0.55f) / 0.05f, 2)) * 0.25f;
    bandL[i] = (uint8_t)(band * 255);
  }
  for (int i = 0; i < 256; ++i) { const float rr = i / 255.0f; bowlL[i] = (uint8_t)(fmaxf(0, 1 - 0.35f * rr) * 255); }
  float* n1 = makeNoise(16, 5); float* n2 = makeNoise(32, 9);
  for (int j = 0; j < 64; ++j) for (int i = 0; i < 64; ++i)
    grain[j * 64 + i] = (uint8_t)((noiseAt(n1, 16, i / 4.0f, j / 4.0f) * 0.6f + noiseAt(n2, 32, i / 2.0f, j / 2.0f) * 0.4f) * 255);
  free(n1); free(n2);
  for (int i = 0; i < 64; ++i) { const float v = (float)i; baseLUT[i] = rgb565((int)(v * 1.25f + 2), (int)(v * 0.92f), (int)(v * 0.85f)); }
}

// 層に、太さ w の線を描く(円をすこしずつずらして押す)。code0..code1 をかかり具合に合わせて、大きい方を残す
static void stampLine(float x0, float y0, float x1, float y1, float w, int code0, int code1) {
  const float len = sqrtf((x1 - x0) * (x1 - x0) + (y1 - y0) * (y1 - y0));
  const int n = max(1, (int)(len * 2));
  const float r = w * 0.5f;
  for (int s = 0; s <= n; ++s) {
    const float cx = x0 + (x1 - x0) * s / n, cy = y0 + (y1 - y0) * s / n;
    const int xa = max(0, (int)floorf(cx - r - 1)), xb = min(S - 1, (int)ceilf(cx + r + 1));
    const int ya = max(0, (int)floorf(cy - r - 1)), yb = min(S - 1, (int)ceilf(cy + r + 1));
    for (int y = ya; y <= yb; ++y) for (int x = xa; x <= xb; ++x) {
      const float d = sqrtf((x + 0.5f - cx) * (x + 0.5f - cx) + (y + 0.5f - cy) * (y + 0.5f - cy));
      const float cov = clamp01(r + 0.5f - d);
      if (cov <= 0) continue;
      const int code = code0 + (int)((code1 - code0) * cov + 0.5f);
      uint8_t& L = layer[(size_t)y * S + x];
      if (code > L) L = (uint8_t)code;
    }
  }
}

static void growBranch(float x, float y, float ang, float budget, float w, float dist, float start, int depth) {
  const float k = S / 466.0f;
  float len = 0;
  while (len < budget && nSeg < MAXSEG) {
    const float st = (5 + frand() * 7) * k;
    ang += (frand() - 0.5f) * 0.32f + (frand() < 0.08f ? (frand() - 0.5f) * 1.1f : 0);   // ほぼまっすぐ、ときどきカクッと折れる
    const float nx = x + cosf(ang) * st, ny = y + sinf(ang) * st;
    if ((nx - Sc) * (nx - Sc) + (ny - Sc) * (ny - Sc) > (PR - 16 * k) * (PR - 16 * k)) break;
    {
      // 金は、傾いていると低い方(ヒビの始まりから見て)へ先に流れる
      const float tc = start + (dist + len) / (CRACK_SPEED * k);
      const float down = ((nx - crackOX) * gTX + (ny - crackOY) * gTY) * 1.5f;
      const float tg = fmaxf(tc + 0.2f, start + GOLD_DELAY + fmaxf(0.0f, dist + len - down) / (GOLD_SPEED * k));
      segs[nSeg] = {x, y, nx, ny, w, tc, tg};
    }
    crackQ[nCQ++] = (int16_t)nSeg; goldQ[nGQ++] = (int16_t)nSeg; ++nSeg;
    x = nx; y = ny; len += st;
    if (depth < 6 && frand() < 0.045f && w > 0.6f)
      growBranch(x, y, ang + (frand() < 0.5f ? 1 : -1) * (0.6f + frand() * 0.7f), budget * (0.3f + frand() * 0.3f), w * 0.7f, dist + len, start, depth + 1);
  }
}

static void crackAt(float x0, float y0) {
  if (fadeDir != 0) return;
  const float k = S / 466.0f;
  const int n = 2 + (int)(frand() * 3);
  const float a0 = frand(0, 6.2832f);
  crackOX = x0; crackOY = y0;
  for (int i = 0; i < n; ++i) growBranch(x0, y0, a0 + i * 6.2832f / n + (frand() - 0.5f) * 0.8f, (70 + frand() * 160) * k, 1 + frand() * 0.4f, 0, clockT, 0);
  // まだ描いていない線を、時間の順に並べ直す
  std::sort(crackQ, crackQ + nCQ, [](int16_t a, int16_t b) { return segs[a].tc > segs[b].tc; });   // 末尾がいちばん早い
  std::sort(goldQ, goldQ + nGQ, [](int16_t a, int16_t b) { return segs[a].tg > segs[b].tg; });
  for (int i = 0; i < 4; ++i) if (flashes[i].t >= 0.7f || flashes[i].t <= 0) { flashes[i] = {x0, y0, 0.0001f}; break; }
  ++cracks;
  if (cracks >= MAX_CRACKS || nSeg > MAXSEG - 600) fadeDir = -1;    // 器がいっぱい: ゆっくり消して新しい器へ
}

static void randomCrack() {
  const float a = frand(0, 6.2832f), r = sqrtf(frand()) * (PR - 60 * S / 466.0f);
  crackAt(Sc + cosf(a) * r, Sc + sinf(a) * r);
}

static void onA() { randomCrack(); }
static void onAHold() { silver ^= 1; Serial.printf("%s\n", silver ? "silver" : "gold"); }
static void statusText(char* o, int n) { snprintf(o, n, "cracks %d segs %d", cracks, nSeg); }

static void updateFrame(const Input& in) {
  float dt = in.dt > 0.05f ? 0.05f : in.dt;
  if (dt < 0.0005f) dt = 0.0005f;
  simT += dt; clockT += dt; idleT += dt;
  const float k = S / 466.0f;
  if (in.tapped) {
    idleT = 0;
    if ((in.tx - Sc) * (in.tx - Sc) + (in.ty - Sc) * (in.ty - Sc) < (PR - 22 * k) * (PR - 22 * k)) crackAt(in.tx, in.ty);
  }
  if (idleT > AUTO_SEC) { idleT = 0; randomCrack(); }
  // ヒビ: 時間になった線を、層に描きたす(ふちの光 → ヒビ)
  int budget = 400;                                   // 1フレームに描く線の数の上限(重くなりすぎないように)
  while (nCQ > 0 && segs[crackQ[nCQ - 1]].tc <= clockT && budget-- > 0) {
    const Seg& s = segs[crackQ[--nCQ]];
    stampLine(s.x0 + 0.8f, s.y0 + 0.8f, s.x1 + 0.8f, s.y1 + 0.8f, 0.9f, 1, 30);
    stampLine(s.x0, s.y0, s.x1, s.y1, s.w * 1.4f, 31, 90);
  }
  // 金: ヒビのあとを、ゆっくり流れて継いでいく(まわりにうっすら、にじみ)
  budget = 400;
  while (nGQ > 0 && segs[goldQ[nGQ - 1]].tg <= clockT && budget-- > 0) {
    const Seg& s = segs[goldQ[--nGQ]];
    stampLine(s.x0, s.y0, s.x1, s.y1, 1.1f + s.w * 1.3f + 3.0f, 128, 128 + 40);
    stampLine(s.x0, s.y0, s.x1, s.y1, 1.1f + s.w * 1.3f, 128, 255);
  }
  for (int i = 0; i < 4; ++i) if (flashes[i].t > 0) { flashes[i].t += dt; if (flashes[i].t > 0.7f) flashes[i].t = 0; }
  // 新しい器へ: ゆっくり消して、消えたら層を空にする
  if (fadeDir < 0) {
    static float acc = 0; acc += dt;
    while (acc > 0.18f && fadeQ > 0) { acc -= 0.18f; --fadeQ; }
    if (fadeQ == 0) { memset(layer, 0, (size_t)S * S); nSeg = nCQ = nGQ = 0; cracks = 0; fadeDir = 1; acc = 0; }
  } else if (fadeDir > 0) { fadeQ = 16; fadeDir = 0; }
  // 金の色: 光の帯がゆっくり器の上をすべる
  gTX = in.tiltX; gTY = in.tiltY;
  const float a = simT * 0.25f; gux = cosf(a); guy = sinf(a);
  {
    const float tm = sqrtf(gTX * gTX + gTY * gTY), w = fminf(1.0f, fmaxf(0.0f, (tm - 0.1f) * 3));
    if (w > 0) { gux = gux * (1 - w) + gTX / tm * w; guy = guy * (1 - w) + gTY / tm * w; const float l = sqrtf(gux * gux + guy * guy) + 1e-6f; gux /= l; guy /= l; }
  }
  const float ph = (sinf(simT * 0.6f) * 0.5f + 0.5f) * 0.7f + 0.15f;
  const Col g0 = silver ? C8(70, 72, 78) : C8(107, 69, 18), g1 = silver ? C8(150, 155, 165) : C8(185, 130, 38);
  const Col g2 = silver ? C8(245, 248, 255) : C8(255, 241, 184), g3 = silver ? C8(160, 165, 175) : C8(200, 146, 47);
  for (int i = 0; i < 256; ++i) {
    const float t = i / 255.0f; Col c;
    if (t < ph - 0.15f) c = lerpC(g0, g1, t / fmaxf(0.01f, ph - 0.15f));
    else if (t < ph) c = lerpC(g1, g2, (t - (ph - 0.15f)) / 0.15f);
    else if (t < ph + 0.15f) c = lerpC(g2, g3, (t - ph) / 0.15f);
    else c = lerpC(g3, g0, (t - ph - 0.15f) / fmaxf(0.01f, 1 - ph - 0.15f));
    goldLUT[i] = to565(c);
  }
}

static void renderBand(int k, uint16_t* buf, Scratch&) {
  const int y0 = k * BAND, y1 = min(S, y0 + BAND);
  const int bx0 = bandX0[k], bw = bandW[k], bx1 = bx0 + bw - 1;
  const float iR = 1.0f / PR;
  const int ax = (int)(0.8f * 256.0f / (1.4f * PR) * 65536.0f), ay = (int)(0.6f * 256.0f / (1.4f * PR) * 65536.0f);   // 映り込みの帯の向き(固定小数)
  const int PR2 = (int)(PR * PR), bowlSh = (int)(255.0f * 65536.0f / (PR * PR));
  const int gx = (int)(gux * 127.5f * 65536.0f * iR), gy = (int)(guy * 127.5f * 65536.0f * iR);
  const int fq = fadeQ;
  const uint16_t table = rgb565(4, 3, 3);
  for (int y = y0; y < y1; ++y) {
    uint16_t* row = buf + (y - y0) * bw - bx0;
    const int xs = max((int)spanS[y], bx0), xe = min((int)spanE[y], bx1);
    for (int x = bx0; x < xs; ++x) row[x] = 0;
    for (int x = max(xe + 1, bx0); x <= bx1; ++x) row[x] = 0;
    if (xs > xe) continue;
    const int dy = (int)(y - Sc), dy2 = dy * dy;
    const uint8_t* L = layer + (size_t)y * S;
    const uint8_t* gr = grain + (y & 63) * 64;
    for (int x = xs; x <= xe; ++x) {
      const int dx = (int)(x - Sc), r2 = dx * dx + dy2;
      if (r2 > PR2) { row[x] = outPix(table); continue; }                 // 器の外(CoreS3 の左右)
      // 漆: 映り込みの帯 × 器の丸み + 木目
      int bi = ((dx * ax + dy * ay) >> 16) + 256; bi = bi < 0 ? 0 : bi > 511 ? 511 : bi;
      int bq = (r2 * bowlSh) >> 16; bq = bq > 255 ? 255 : bq;
      const int bowl = bowlL[bq];
      int v = (7 * 255 + 22 * ((bandL[bi] * bowl) >> 8) + 5 * gr[x & 63]) * bowl >> 16;   // 0..約 34
      v = v > 63 ? 63 : v;
      uint32_t c = baseLUT[v];
      const int code = L[x];
      if (code) {
        if (code >= 128) {                                           // 金(かかり具合で混ぜる)
          int gi = ((dx * gx + dy * gy) >> 16) + 128; gi = gi < 0 ? 0 : gi > 255 ? 255 : gi;
          const int q = ((code - 128) * fq) >> 7;                    // 0..16
          c = mix565(c, goldLUT[gi], q > 16 ? 16 : q);
        } else if (code > 30) {                                      // ヒビ(暗く)
          const int q = ((code - 30) * fq) / 60;
          c = mix565(c, 0, q > 16 ? 16 : q);
        } else {                                                     // ヒビのふちの光
          const int q = (code * fq) / 150;
          c = mix565(c, rgb565(255, 235, 210), q);
        }
      }
      row[x] = outPix((uint16_t)c);
    }
    // 叩いた瞬間の、かすかな光
    for (int i = 0; i < 4; ++i) {
      const Flash& f = flashes[i];
      if (f.t <= 0) continue;
      const float rad = (26 + 40 * f.t) * S / 466.0f, fdy = y - f.y;
      if (fdy * fdy >= rad * rad) continue;
      const float hw = sqrtf(rad * rad - fdy * fdy), kk = 1 - f.t / 0.7f;
      const int a0 = max(xs, (int)(f.x - hw)), a1 = min(xe, (int)(f.x + hw));
      for (int x = a0; x <= a1; ++x) {
        const float d = sqrtf((x - f.x) * (x - f.x) + fdy * fdy) / rad;
        const int q = (int)((1 - d) * kk * 6);
        if (q > 0) row[x] = addPix(row[x], (uint16_t)(((q * 2) << 11) | ((q * 4 - 1) << 5) | (q * 2 - 1)));   // あたたかい白(赤:緑:青 をそろえて)
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
