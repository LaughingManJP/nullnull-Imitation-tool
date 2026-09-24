// NullVortex  -  うねる光の膜に、いくつもの渦が空いて、まわりの塊を吸い込んでいく
//   M5Stack StopWatch 用(丸い 466x466 画面)。CoreS3 系でも真ん中に正方形で動きます。
//
// 画面の中:
//   ・藤色〜青紫のもやの上に、マゼンタ・深紅・シアンの塊がびっしり浮かんで、
//     それぞれ勝手な速さで明滅しています(ところどころ白い粒がチカチカします)
//   ・渦がいくつかゆっくり漂っていて、まわりの塊を引き伸ばしながら巻き込みます
//   ・渦の芯は暗く落ち、そのふちだけが熱く光ります
//
// 触ると:
//   ・いちばん近い渦が指についてきます。そのまま動かせます
//   ・押さえているあいだ、その渦は口を開けて強く吸い込みます(芯が広がって、渦が締まる)
//   ・離すと、吸い込む力がゆっくりゆるんで、渦は指の勢いのまま流れていきます
//
// 操作:
//   StopWatch … ボタンA 短押し: 色    ボタンA 長押し: 渦の数    ボタンB: 明るさ
//   CoreS3系  … ダブルタップ: 色      電源ボタン短押し: 明るさ
//
// 必要なもの:
//   ボードマネージャー M5Stack >= 3.3.7 / ボード: M5StopWatch(CoreS3 なら M5CoreS3)
//   ライブラリ M5Unified >= 0.2.15, M5GFX >= 0.2.21
//
// しくみ:
//   光の膜は「塊の地図」(テクスチャ)を1枚だけ最初に焼いておきます。
//   1ピクセルごとにやることは「うねりでずらした位置の地図を引いて、色の表を引く」だけ。
//   渦は、地図を引く位置を回して外へ広げる = 絵が内側へ吸い込まれて見える、という仕掛けです。
//   回す角度と広げる量に「外から内へ進む波」を入れてあるので、
//   永遠に巻き込まれて潰れることなく、ずっと流れ続けます。
//   画面は8行ずつの帯に分けて、2つのコアで交互に描きます。

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
// 塊の地図の細かさ。8 = 256x256 (128KB。PSRAM に置きます。きれい)
//                   7 = 128x128 ( 32KB。内蔵メモリに収まります。少し粗いぶん確実に軽い)
// 実機で fps が出ないときは、まずここを 7 にしてみてください。
static constexpr int   TEXSH      = 8;
static constexpr float TEXSPAN    = 1.0f;    // 地図が画面を何回ぶん覆うか(1.0 = ちょうど1回)
static constexpr float FLOWT      = 3.2f;    // 膜のうねりの大きさ(地図の目盛り)
static constexpr float FLOW_SPEED = 0.055f;  // うねりが流れる速さ
static constexpr float WRINKLE    = 0.45f;   // 細かいしわの強さ
static constexpr float DRIFTT     = 3.2f;    // 全体がゆっくり流れていく量

static constexpr int   NVORT_DEF  = 3;       // 渦の数(ボタンA長押しで 3→4→5→6)
static constexpr float VR         = 0.25f;   // 渦がとどく範囲(画面に対して)
static constexpr float TWIST      = 1.75f;   // 渦のねじれの強さ(rad)
static constexpr float TWOSC      = 0.60f;   // ねじれに乗る波の深さ
static constexpr float PULL       = 0.30f;   // 外へ広げる量 = 吸い込んで見える量
static constexpr float PUOSC      = 0.22f;   // 広げる量に乗る波の深さ
static constexpr float WAVE_K     = 0.055f;  // 波の細かさ(1ピクセルあたりの位相)
static constexpr float WAVE_SPD   = 1.55f;   // 波が内へ進む速さ
static constexpr float SUCK_TW    = 1.15f;   // 押さえたとき、さらに締まる量
static constexpr float SUCK_PU    = 0.70f;   // 押さえたとき、さらに吸い込む量

static constexpr float CORE_R     = 0.105f;  // 渦の芯(暗いところ)の大きさ
static constexpr float CORE_GROW  = 0.55f;   // 吸い込んでいるとき、芯が広がる割合
static constexpr float RIM_STR    = 1.55f;   // 芯のふちが光る強さ

static constexpr float ORB_R      = 0.22f;   // 渦がひとりでに漂う範囲
static constexpr float PUSH       = 0.34f;   // 渦どうしが押し合う距離(画面に対して)
static constexpr float GRAB_K     = 17.0f;   // 指についてくる強さ
static constexpr float PULSE      = 1.0f;    // タップしたときパッと光る強さ
// ---------------------------------------------------------------------------

static constexpr int TEX    = 1 << TEXSH;    // 塊の地図(2のべき乗)
static constexpr int TEXM   = TEX - 1;
static constexpr int GRIDN  = 14;            // 塊を置く格子(GRIDN x GRIDN 個)
static constexpr int NBLOB  = GRIDN * GRIDN;
static constexpr int NSPARK = 40;            // チカチカする粒
static constexpr int NBG    = 4;             // もやの色ゆらぎの種類
static constexpr int ID_SPK = NBLOB;                 // 粒の先頭 id
static constexpr int ID_BG  = NBLOB + NSPARK;        // もやの先頭 id
static constexpr int NCELL  = NBLOB + NSPARK + NBG;  // 使う id の総数
static constexpr int HAZEN  = 80;            // 地図の値 0..79 = もや、80..255 = 塊

static constexpr int NBTYPE = 5;             // 塊の色の種類
static constexpr int NTYPE  = NBTYPE + NBG;  // + もやの色ゆらぎ
static constexpr int LUTN   = 256 * 64;      // 色の表 (id, 明るさ) → 色
static constexpr int SINN   = 1024;
static constexpr int NOISE  = 64;
static constexpr int CSTEP  = 8;             // うねりを計算する間隔(ピクセル)
static constexpr int USHIFT = 3;             // log2(CSTEP)。節の差 → 1px あたりへ
static constexpr int BAND   = 8;
static constexpr int NSLOT  = 4;
static constexpr int MAXV   = 6;
static constexpr int CORN   = 48;            // 芯の断面

static int   W, H, S, XOFF, YOFF, CW, NB;
static float Sc;                             // 画面の中心
static float texPpx;                         // 1ピクセルあたりの地図の目盛り
static uint8_t* bandVis;                     // 画面からはみ出す帯は描かない
static int   PIXEL = 1;

static uint16_t* slotBuf[NSLOT];
static uint16_t* tex;                        // [TEX*TEX] (id<<8) | 明るさ
static uint16_t* lut;                        // [LUTN]    (id,明るさ) → 色
static uint16_t* rampTab;                    // [NTYPE*256] 種類ごとの色の階段
static float*    gU;  static float* gV;      // [CW*CW] 節ごとの「地図を引く位置」
static int16_t*  spanS;
static int16_t*  spanE;
static int16_t*  bandX0;
static int16_t*  bandW;
static float*    noiseT;
static float     sinT[SINN];

static uint8_t   cellType[256];
static float     cellBase[256], cellAmp[256], cellW[256], cellPh[256];

struct Scratch { int32_t* rx; int32_t* ry; };
static Scratch   scr[2];
static uint16_t  blackCol;
static uint32_t  busyUs[2];

static SemaphoreHandle_t semGo, semDone;
static volatile int readyOdd = -1, doneUpTo = -1;

static float simT = 0;
static float flash = 0;
static float driftU = 0, driftV = 0;

static int theme = 0;
static const uint8_t brightSteps[] = {60, 120, 190, 255};
static int brightIdx = 2;
static int nVort = NVORT_DEF;

// ---------------------------------------------------------------- 道具
struct Col { float r, g, b; };
static inline float clamp01(float v) { return v < 0 ? 0 : (v > 1 ? 1 : v); }
static inline float smooth3(float t) { t = clamp01(t); return t * t * (3 - 2 * t); }
static inline Col lerpC(Col a, Col b, float t) {
  return {a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t};
}
static constexpr Col C8(int r, int g, int b) {
  return {r / 255.0f, g / 255.0f, b / 255.0f};
}
static inline uint16_t toPix(Col c) {
  uint16_t col = M5.Display.color565(clamp01(c.r) * 255, clamp01(c.g) * 255, clamp01(c.b) * 255);
  return __builtin_bswap16(col);               // 色がおかしい場合はこの bswap を外す
}
static inline float frand() { return (esp_random() & 0xFFFF) / 65535.0f; }
static inline float frand(float a, float b) { return a + (b - a) * frand(); }

// 三角関数は表引きで。節ごとに何度も呼ぶので、sinf/cosf のままだと間に合いません。
static inline float sinTabF(float a) {
  int i = (int)(a * (SINN / 6.2831853f)) & (SINN - 1);
  return sinT[i];
}
static inline void sincosT(float a, float& s, float& c) {
  int i = (int)(a * (SINN / 6.2831853f)) & (SINN - 1);
  s = sinT[i]; c = sinT[(i + (SINN >> 2)) & (SINN - 1)];
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
static void makeNoise(float* t, int passes) {
  float* tmp = (float*)malloc(NOISE * NOISE * sizeof(float));
  if (!tmp) return;
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
  if (mx > 0) for (int i = 0; i < NOISE * NOISE; ++i) t[i] /= mx;
  free(tmp);
}

// ---------------------------------------------------------------- 色の組み合わせ
// haze0/1/2/Hi … もや(暗いところ → 藤色 → 明るいところ → 白に近いところ)
// bgA/bgB      … もやの色を少しだけ振る2色
// A/B/C        … 塊の 縁 / 中 / 芯 の色(5種類)
struct Pal {
  Col haze0, haze1, haze2, hazeHi;
  Col bgA, bgB;
  Col A[NBTYPE], B[NBTYPE], C[NBTYPE];
  Col rim;
};
static const Pal PALS[] = {
  // 0: 動画の色(藤色のもやに、マゼンタと深紅。ときどきシアンと白い粒)
  { C8(0x2E,0x12,0x2E), C8(0x7A,0x4A,0x96), C8(0xC4,0x89,0xC4), C8(0xF2,0xE2,0xF6),
    C8(0x50,0x7A,0xDC), C8(0xD0,0x50,0x8E),
    { C8(0xA8,0x30,0x6A), C8(0xA6,0x3C,0x9E), C8(0xB0,0x3C,0x5A), C8(0x4A,0x80,0xBE), C8(0xA8,0x8C,0xC8) },
    { C8(0xD4,0x2E,0x62), C8(0xD0,0x46,0xA8), C8(0xF0,0x6A,0x34), C8(0x58,0xD4,0xE4), C8(0xDC,0xC8,0xEC) },
    { C8(0xFF,0x6A,0x72), C8(0xFF,0x8E,0xE0), C8(0xFF,0xE8,0xC0), C8(0xD8,0xFF,0xFF), C8(0xFF,0xFF,0xFF) },
    C8(0xFF,0x5A,0xA0) },
  // 1: 青緑(深い海の色。翡翠と水色)
  { C8(0x10,0x22,0x30), C8(0x22,0x50,0x66), C8(0x5E,0xA8,0xC0), C8(0xD2,0xF4,0xF4),
    C8(0x3C,0x8E,0xC8), C8(0x2E,0xA0,0x94),
    { C8(0x1E,0x7E,0x8E), C8(0x2A,0x8A,0x6A), C8(0x50,0x8E,0xB4), C8(0x7C,0x8E,0x50), C8(0x96,0xC0,0xD0) },
    { C8(0x22,0xC0,0xC8), C8(0x2E,0xC8,0x8A), C8(0x60,0xB0,0xF0), C8(0xC8,0xD0,0x50), C8(0xD0,0xEC,0xF0) },
    { C8(0xB0,0xFF,0xFF), C8(0xA8,0xFF,0xD0), C8(0xD0,0xE8,0xFF), C8(0xFF,0xFF,0xC0), C8(0xFF,0xFF,0xFF) },
    C8(0x50,0xE8,0xD8) },
  // 2: 琥珀(炭火の色。橙と朱に、緑の火花)
  { C8(0x28,0x18,0x12), C8(0x60,0x38,0x22), C8(0xC0,0x7E,0x48), C8(0xF8,0xE4,0xC0),
    C8(0xC8,0x6E,0x2E), C8(0xA8,0x50,0x58),
    { C8(0xA0,0x3A,0x22), C8(0xA8,0x56,0x1E), C8(0x8E,0x2E,0x40), C8(0x5A,0x7A,0x30), C8(0xC8,0xA0,0x78) },
    { C8(0xE0,0x5A,0x22), C8(0xF0,0x92,0x24), C8(0xD0,0x38,0x58), C8(0x9C,0xC8,0x3A), C8(0xF0,0xD8,0xB0) },
    { C8(0xFF,0xB4,0x70), C8(0xFF,0xE2,0x90), C8(0xFF,0x90,0xA8), C8(0xE8,0xFF,0xA0), C8(0xFF,0xFF,0xFF) },
    C8(0xFF,0x9A,0x40) },
  // 3: 白銀(色を落として、鏡のような明暗だけ)
  { C8(0x18,0x1A,0x22), C8(0x44,0x4A,0x5E), C8(0x8E,0x96,0xAC), C8(0xE8,0xEC,0xF4),
    C8(0x76,0x8A,0xB0), C8(0x8A,0x82,0xA0),
    { C8(0x5E,0x66,0x7C), C8(0x6E,0x72,0x86), C8(0x7A,0x6E,0x76), C8(0x62,0x74,0x84), C8(0x9A,0xA2,0xB4) },
    { C8(0x9C,0xA6,0xBC), C8(0xB0,0xB4,0xC2), C8(0xC0,0xAE,0xAE), C8(0x96,0xB2,0xCA), C8(0xD8,0xDE,0xEA) },
    { C8(0xF0,0xF4,0xFF), C8(0xFF,0xFF,0xFF), C8(0xFF,0xF0,0xE8), C8(0xDC,0xF0,0xFF), C8(0xFF,0xFF,0xFF) },
    C8(0xC8,0xD8,0xFF) },
};
static constexpr int NPAL = sizeof(PALS) / sizeof(PALS[0]);

// ------------------------------------------------- 種類ごとの色の階段(色替えのときだけ作る)
//   0..79   もや(どの種類でも同じ階段。だから塊のふちがもやに溶けます)
//   80..255 その種類の色(縁 → 中 → 芯)
static void buildRamps() {
  const Pal& P = PALS[theme];
  for (int ty = 0; ty < NTYPE; ++ty) {
    Col h0 = P.haze0, h1 = P.haze1, h2 = P.haze2;
    if (ty >= NBTYPE) {                                   // もやの色を少しだけ振る
      float k = (float)(ty - NBTYPE) - (NBG - 1) * 0.5f;  // -1.5 .. +1.5
      float w = fabsf(k) * (0.62f / ((NBG - 1) * 0.5f));
      Col t = (k < 0) ? P.bgA : P.bgB;
      h0 = lerpC(h0, t, w * 0.5f); h1 = lerpC(h1, t, w); h2 = lerpC(h2, t, w);
    }
    uint16_t* dst = rampTab + ty * 256;
    for (int i = 0; i < HAZEN; ++i) {
      float t = (float)i / (HAZEN - 1);
      dst[i] = toPix(t < 0.55f ? lerpC(h0, h1, t / 0.55f)
                               : lerpC(h1, h2, (t - 0.55f) / 0.45f));
    }
    const int bt = (ty < NBTYPE) ? ty : 0;
    const Col A = P.A[bt], B = P.B[bt], Cc = P.C[bt];
    const Col edge = lerpC(h2, P.haze0, 0.42f);          // 塊と塊のあいだの影
    for (int i = HAZEN; i < 256; ++i) {
      float u = (float)(i - HAZEN) / (255 - HAZEN);
      Col c;
      if (ty >= NBTYPE)   c = lerpC(h2, P.hazeHi, u);      // もやはそのまま白へ
      else if (u < 0.14f) c = lerpC(h2, edge, smooth3(u / 0.14f));   // ふちを一度だけ沈める
      else if (u < 0.40f) c = lerpC(edge, A, smooth3((u - 0.14f) / 0.26f));
      else if (u < 0.82f) c = lerpC(A, B, smooth3((u - 0.40f) / 0.42f));
      else                c = lerpC(B, Cc, smooth3((u - 0.82f) / 0.18f));
      dst[i] = toPix(c);
    }
  }
}

// ------------------------------------------------- 塊の地図(最初に1回だけ焼く)
static void buildTexture() {
  // もや: 整数の波を重ねる。整数だけ使うので、端がぴったり繋がります(継ぎ目が出ない)
  struct Wv { int fx, fy; float a, p; };
  Wv wv[8];
  for (int i = 0; i < 8; ++i) {
    wv[i].fx = (int)frand(-4, 5); wv[i].fy = (int)frand(-4, 5);
    if (wv[i].fx == 0 && wv[i].fy == 0) wv[i].fx = 1 + i % 3;
    wv[i].a = 1.0f / (1.0f + fabsf((float)wv[i].fx) + fabsf((float)wv[i].fy));
    wv[i].p = frand(0, 6.2831853f);
  }
  float amax = 0; for (int i = 0; i < 8; ++i) amax += wv[i].a;
  const float k = 6.2831853f / TEX;
  for (int y = 0; y < TEX; ++y)
    for (int x = 0; x < TEX; ++x) {
      float s = 0, s2 = 0;
      for (int i = 0; i < 8; ++i) {
        float v = sinTabF((wv[i].fx * x + wv[i].fy * y) * k + wv[i].p);
        s += wv[i].a * v;
        if (i < 3) s2 += v;
      }
      float h = clamp01(0.46f + 0.62f * (s / amax));
      int d = (int)(h * (HAZEN - 1) + 0.5f);
      int bg = (int)((s2 / 3.0f * 0.5f + 0.5f) * NBG);
      if (bg < 0) bg = 0;
      if (bg >= NBG) bg = NBG - 1;
      tex[(y << TEXSH) + x] = (uint16_t)(((ID_BG + bg) << 8) | d);
    }

  // 塊: 格子の上にひとつずつ、少しずらして置く(かたよらせないため)
  auto splat = [&](float cx, float cy, float R, float asp, float rot, int id) {
    const float Ra = R * asp, Rb = R / asp;
    const float ia = 1.0f / (Ra * Ra), ib = 1.0f / (Rb * Rb);
    float sn, cs; sincosT(rot, sn, cs);
    int ri = (int)ceilf(fmaxf(Ra, Rb)) + 1;
    for (int dy = -ri; dy <= ri; ++dy)
      for (int dx = -ri; dx <= ri; ++dx) {
        const float uu = dx * cs + dy * sn, vv = -dx * sn + dy * cs;
        float t2 = uu * uu * ia + vv * vv * ib;
        if (t2 >= 1.0f) continue;
        float g = 1.0f - t2; g *= g;                    // ふちで 0、まん中で 1
        int v = (int)(g * 255.0f + 0.5f);
        if (v <= 0) continue;
        int xx = ((int)cx + dx) & TEXM, yy = ((int)cy + dy) & TEXM;
        uint16_t* p = &tex[(yy << TEXSH) + xx];
        if (v > (*p & 255)) *p = (uint16_t)((id << 8) | v);
      }
  };
  // 色の下敷き。2枚のなめらかな場で「どのあたりが何色になりやすいか」を決める
  Wv tv[2][3];
  for (int a = 0; a < 2; ++a)
    for (int i = 0; i < 3; ++i) {
      tv[a][i].fx = (int)frand(-2, 3); tv[a][i].fy = (int)frand(-2, 3);
      if (tv[a][i].fx == 0 && tv[a][i].fy == 0) tv[a][i].fx = 1 + i;
      tv[a][i].a = 1.0f; tv[a][i].p = frand(0, 6.2831853f);
    }
  auto field = [&](int a, float x, float y) {
    float v = 0;
    for (int i = 0; i < 3; ++i) v += sinTabF((tv[a][i].fx * x + tv[a][i].fy * y) * k + tv[a][i].p);
    return v / 3.0f;                                   // -1 .. 1
  };

  const float cellSz = (float)TEX / GRIDN;
  for (int gy = 0; gy < GRIDN; ++gy)
    for (int gx = 0; gx < GRIDN; ++gx) {
      int id = gy * GRIDN + gx;
      float cx = (gx + 0.5f) * cellSz + frand(-0.42f, 0.42f) * cellSz;
      float cy = (gy + 0.5f) * cellSz + frand(-0.42f, 0.42f) * cellSz;
      float f1 = field(0, cx, cy), f2 = field(1, cx, cy);
      int ty;
      if (f2 > 0.42f && frand() < 0.80f)      ty = 3;  // 水色の一帯
      else if (frand() < 0.085f)              ty = 2;  // 熱い火花
      else if (frand() < 0.020f)              ty = 4;  // 白
      else ty = (f1 + frand(-0.40f, 0.40f) > 0.0f) ? 0 : 1;   // 深紅の一帯 / 桃色の一帯
      cellType[id] = (uint8_t)ty;
      splat(cx, cy, frand(0.48f, 1.22f) * cellSz,
            frand(0.55f, 1.95f), frand(0, 3.14159f), id);
    }
  const float sparkSz = TEX / 256.0f;
  for (int i = 0; i < NSPARK; ++i)
    splat(frand(0, TEX), frand(0, TEX), frand(1.5f, 3.1f) * sparkSz,
          frand(0.8f, 1.25f), frand(0, 3.14159f), ID_SPK + i);
}

// ------------------------------------------------- id ごとの性格(最初に1回だけ)
static void buildCells() {
  for (int i = 0; i < NBLOB; ++i) {
    // cellType[i] は buildTexture() で「色の下敷き」から決めてあるので、ここでは触らない
    cellBase[i] = frand(0.78f, 1.14f);
    cellAmp[i]  = frand(0.18f, 0.46f);
    cellW[i]    = frand(0.22f, 1.35f);
    cellPh[i]   = frand(0, 6.2831853f);
  }
  for (int i = 0; i < NSPARK; ++i) {
    int id = ID_SPK + i;
    float r = frand();
    cellType[id] = (r < 0.55f) ? 4 : (r < 0.80f) ? 3 : 2;
    cellBase[id] = frand(1.05f, 1.45f);
    cellAmp[id]  = frand(0.55f, 0.90f);                  // 粒は強くチカチカする
    cellW[id]    = frand(2.0f, 7.0f);
    cellPh[id]   = frand(0, 6.2831853f);
  }
  for (int i = 0; i < NBG; ++i) {
    int id = ID_BG + i;
    cellType[id] = NBTYPE + i;
    cellBase[id] = 1.0f;
    cellAmp[id]  = 0.07f;                                // もやはごくゆっくり息をする
    cellW[id]    = frand(0.10f, 0.22f);
    cellPh[id]   = frand(0, 6.2831853f);
  }
}

// ------------------------------------------------- 色の表(1フレームに1回)
static void buildLut() {
  const float fl = 1.0f + flash * 0.9f;
  for (int c = 0; c < NCELL; ++c) {
    float b = cellBase[c] * (1.0f + cellAmp[c] * sinTabF(simT * cellW[c] + cellPh[c])) * fl;
    if (b < 0.05f) b = 0.05f; else if (b > 3.0f) b = 3.0f;
    const uint16_t* rp = rampTab + cellType[c] * 256;
    uint16_t* dst = lut + c * 64;
    uint32_t acc = 0, step = (uint32_t)(b * 1024.0f);    // 明るさ d(0..63) → 階段の番号
    for (int d = 0; d < 64; ++d, acc += step) {
      uint32_t i = acc >> 8; if (i > 255) i = 255;
      dst[d] = rp[i];
    }
  }
}

// ---------------------------------------------------------------- 渦
struct Vort {
  float x, y, vx, vy;
  float p1, p2, w1, w2;      // ひとりでに漂うときの、小さな揺れ
  float a0, wr;              // ゆっくり大回りする軌道(渦どうしが重ならないように)
  float spin;                // 回る向きと速さ
  float ph;                  // 波の位相
  float suck;                // 吸い込んでいる度合い 0..1
  float R;
};
static Vort vor[MAXV];
static int  grabbed = -1;

struct CoreDraw {                               // 芯を上から塗るための表
  float cx, cy, r2, scale;
  int   y0, y1, on;
  uint16_t kTab[CORN];
  uint8_t  aR[CORN], aG[CORN], aB[CORN];
};
static CoreDraw coreD[MAXV];

// 渦の「大きな輪」の上での持ち場を、数に合わせて等間隔に配り直す
static void spreadVort() {
  for (int i = 0; i < nVort; ++i) vor[i].a0 = 6.2831853f * i / nVort - simT * vor[i].wr;
}

static void initVort() {
  for (int i = 0; i < MAXV; ++i) {
    float a = 6.2831853f * i / MAXV;
    vor[i].x = Sc + ORB_R * S * cosf(a);
    vor[i].y = Sc + ORB_R * S * sinf(a);
    vor[i].vx = vor[i].vy = 0;
    vor[i].p1 = frand(0, 6.2831853f); vor[i].p2 = frand(0, 6.2831853f);
    vor[i].w1 = frand(0.10f, 0.26f); vor[i].w2 = frand(0.10f, 0.26f);
    vor[i].a0 = a; vor[i].wr = frand(0.028f, 0.052f) * ((i & 1) ? 1.0f : -1.0f);
    vor[i].spin = (i & 1) ? 1.0f : -1.0f;
    vor[i].ph = frand(0, 6.2831853f);
    vor[i].suck = 0;
    vor[i].R = VR * S * frand(0.86f, 1.16f);
  }
  spreadVort();
}

// 節ごとに「地図を引く位置」を渦で回して、外へ広げる
static void applyVortex(const Vort& v) {
  const float R = v.R, R2 = R * R, invR2 = 1.0f / R2;
  int ga = (int)floorf((v.x - R) / CSTEP), gb = (int)ceilf((v.x + R) / CSTEP);
  int gc = (int)floorf((v.y - R) / CSTEP), gd = (int)ceilf((v.y + R) / CSTEP);
  if (ga < 0) ga = 0;
  if (gb > CW - 1) gb = CW - 1;
  if (gc < 0) gc = 0;
  if (gd > CW - 1) gd = CW - 1;
  const float vu = (v.x - Sc) * texPpx + driftU;      // 渦の中心を地図の座標で
  const float vv = (v.y - Sc) * texPpx + driftV;
  const float ang0 = v.spin * (TWIST + SUCK_TW * v.suck);
  const float angO = v.spin * TWOSC;
  const float pul0 = PULL + SUCK_PU * v.suck;
  const float wph  = v.ph - simT * WAVE_SPD;
  for (int gy = gc; gy <= gd; ++gy) {
    const float y = (float)(gy * CSTEP), dy = y - v.y, dy2 = dy * dy;
    if (dy2 >= R2) continue;
    float* pu = gU + gy * CW; float* pv = gV + gy * CW;
    for (int gx = ga; gx <= gb; ++gx) {
      const float dx = (float)(gx * CSTEP) - v.x;
      const float r2 = dx * dx + dy2;
      if (r2 >= R2) continue;
      float f = 1.0f - r2 * invR2; f *= f;             // ふちで 0、中心で 1
      const float r = sqrtf(r2);
      // 外から内へ進む波。これがあるので、ずっと流れていても巻き潰れません
      const float sw = sinTabF(r * WAVE_K + wph);
      const float sw2 = sinTabF(r * WAVE_K + wph + 1.5708f);
      const float a = f * (ang0 + angO * sw);
      const float m = 1.0f + f * (pul0 + PUOSC * sw2);
      float sn, cs; sincosT(a, sn, cs);
      const float ru = pu[gx] - vu, rv = pv[gx] - vv;
      pu[gx] = vu + (cs * ru - sn * rv) * m;
      pv[gx] = vv + (sn * ru + cs * rv) * m;
    }
  }
}

static void buildCore(int i) {
  CoreDraw& c = coreD[i];
  const Vort& v = vor[i];
  const float r = CORE_R * S * (1.0f + CORE_GROW * v.suck);
  c.cx = v.x; c.cy = v.y; c.r2 = r * r; c.scale = (float)CORN / c.r2;
  c.y0 = (int)floorf(v.y - r); c.y1 = (int)ceilf(v.y + r) + 1;
  c.on = (c.y1 > 0 && c.y0 < S);
  const Pal& P = PALS[theme];
  const float rs = RIM_STR * (0.55f + 0.85f * v.suck) * (1.0f + flash * 0.6f);
  for (int j = 0; j < CORN; ++j) {
    float t = sqrtf((float)j / (CORN - 1));            // 番号は距離^2 に比例
    float k = 0.13f + 0.87f * smooth3((t - 0.04f) / 0.80f);
    c.kTab[j] = (uint16_t)(k * 256.0f + 0.5f);
    float edge = 1.0f - t * t; edge *= edge;           // ふちでぴたりと 0 にする
    float g = expf(-(t - 0.48f) * (t - 0.48f) * 11.0f) * rs * edge;
    c.aR[j] = (uint8_t)(clamp01(P.rim.r * g) * 31.0f);
    c.aG[j] = (uint8_t)(clamp01(P.rim.g * g) * 63.0f);
    c.aB[j] = (uint8_t)(clamp01(P.rim.b * g) * 31.0f);
  }
}

// ---------------------------------------------------------------- 1フレームの準備
struct Input { bool touching; bool tapped; float tx, ty; float dt; };

static void updateFrame(const Input& in) {
  float dt = in.dt > 0.05f ? 0.05f : in.dt;
  if (dt < 0.0005f) dt = 0.0005f;
  simT += dt;

  if (in.tapped) flash = PULSE;
  flash *= expf(-dt * 3.2f);

  // ---- 指: いちばん近い渦をつかんで、押さえているあいだ吸い込ませる
  if (in.tapped) {
    float best = 1e18f; int bi = -1;
    for (int i = 0; i < nVort; ++i) {
      float dx = vor[i].x - in.tx, dy = vor[i].y - in.ty;
      float d2 = dx * dx + dy * dy;
      if (d2 < best) { best = d2; bi = i; }
    }
    grabbed = bi;
  }
  if (!in.touching) grabbed = -1;

  // ---- 渦を動かす
  const float lim = 0.42f * S;
  const float push = PUSH * S, push2 = push * push;
  for (int i = 0; i < nVort; ++i) {
    Vort& v = vor[i];
    float tx, ty, k, damp;
    if (i == grabbed) {                                   // 指についてくる
      v.suck += (1.0f - v.suck) * (1.0f - expf(-dt * 3.4f));
      v.ph -= dt * 1.75f;                                 // つかんでいると波が速まる
      tx = in.tx; ty = in.ty; k = GRAB_K; damp = 7.0f;
    } else {                                              // ひとりでに漂う
      v.suck *= expf(-dt * 1.15f);
      v.ph += dt * 0.35f;
      // 大きな輪の上をゆっくり回りながら、その場で少し揺れる
      float th = v.a0 + simT * v.wr;
      float sn, cs; sincosT(th, sn, cs);
      tx = Sc + ORB_R * S * cs + 0.055f * S * sinTabF(simT * v.w1 + v.p1);
      ty = Sc + ORB_R * S * sn + 0.055f * S * sinTabF(simT * v.w2 + v.p2);
      k = 1.15f; damp = 1.9f;
    }
    v.vx += ((tx - v.x) * k - v.vx * damp) * dt;
    v.vy += ((ty - v.y) * k - v.vy * damp) * dt;
    // 渦どうしは、近づきすぎたら押し合う
    for (int j = 0; j < nVort; ++j) {
      if (j == i) continue;
      float dx = v.x - vor[j].x, dy = v.y - vor[j].y;
      float d2 = dx * dx + dy * dy;
      if (d2 > push2 || d2 < 1e-4f) continue;
      float d = sqrtf(d2), k = (1.0f - d / push) * 130.0f;
      v.vx += dx / d * k * dt; v.vy += dy / d * k * dt;
    }
    v.x += v.vx * dt; v.y += v.vy * dt;
    // 画面からはみ出さないように、外周でやわらかく押し戻す
    float dx = v.x - Sc, dy = v.y - Sc, d2 = dx * dx + dy * dy;
    if (d2 > lim * lim) {
      float d = sqrtf(d2), o = (d - lim);
      v.x -= dx / d * o; v.y -= dy / d * o;
      v.vx -= dx / d * o * 5.0f; v.vy -= dy / d * o * 5.0f;
    }
  }

  // ---- 節ごとの「地図を引く位置」= 基本の位置 + 膜のうねり
  const float texK = TEX / 256.0f;             // 地図の細かさに合わせて、幅を揃える
  const float flowT = FLOWT * texK;
  driftU += dt * DRIFTT * texK * 0.030f;
  driftV -= dt * DRIFTT * texK * 0.021f;
  const float sc = 6.5f / S * CSTEP, tt = simT * FLOW_SPEED * 10.0f;
  buildNMap(nmX[0], sc,        tt,        CW);  buildNMap(nmY[0], sc,        0,          CW);
  buildNMap(nmX[1], sc * 2.9f, 31,        CW);  buildNMap(nmY[1], sc * 2.9f, -tt * 1.3f, CW);
  buildNMap(nmX[2], sc,       -17,        CW);  buildNMap(nmY[2], sc,        tt,         CW);
  buildNMap(nmX[3], sc * 2.9f, tt * 1.1f, CW);  buildNMap(nmY[3], sc * 2.9f, 11,         CW);
  for (int gy = 0; gy < CW; ++gy) {
    const float y = (float)(gy * CSTEP);
    const float baseV = (y - Sc) * texPpx + driftV;
    float* pu = gU + gy * CW; float* pv = gV + gy * CW;
    for (int gx = 0; gx < CW; ++gx) {
      float nx = noiseAt(nmX[0], nmY[0], gx, gy) + WRINKLE * noiseAt(nmX[1], nmY[1], gx, gy);
      float ny = noiseAt(nmX[2], nmY[2], gx, gy) + WRINKLE * noiseAt(nmX[3], nmY[3], gx, gy);
      pu[gx] = ((float)(gx * CSTEP) - Sc) * texPpx + driftU + flowT * nx;
      pv[gx] = baseV + flowT * ny;
    }
  }
  for (int i = 0; i < nVort; ++i) applyVortex(vor[i]);
  for (int i = 0; i < nVort; ++i) buildCore(i);

  buildLut();
}

// ---------------------------------------------------------------- 描画(帯ごと)
// 渦の芯を上から塗る。面積が小さいので、ここだけ掛け算を使っても大したことはありません。
static void applyCores(int k, uint16_t* buf) {
  const int y0 = k * BAND, y1 = min(S, y0 + BAND);
  const int bx0 = bandX0[k], bw = bandW[k], bx1 = bx0 + bw - 1;
  for (int i = 0; i < nVort; ++i) {
    const CoreDraw& c = coreD[i];
    if (!c.on) continue;
    int ya = max(y0, c.y0), yb = min(y1, c.y1);
    for (int y = ya; y < yb; ++y) {
      const float dy = (float)y - c.cy, dy2 = dy * dy;
      const float w2 = c.r2 - dy2;
      if (w2 <= 0) continue;
      const float hw = sqrtf(w2);
      int xs = max(max((int)spanS[y], bx0), (int)(c.cx - hw));
      int xe = min(min((int)spanE[y], bx1), (int)(c.cx + hw) + 1);
      if (xs > xe) continue;
      uint16_t* row = buf + (y - y0) * bw;
      float dx = (float)xs - c.cx;
      for (int x = xs; x <= xe; ++x, dx += 1.0f) {
        const float d2 = dx * dx + dy2;
        const int j = (int)(d2 * c.scale);
        if (j >= CORN) continue;
        const uint32_t cc = __builtin_bswap16(row[x - bx0]);
        const uint32_t kk = c.kTab[j];
        uint32_t r = (((cc >> 11) & 31) * kk) >> 8;
        uint32_t g = (((cc >> 5) & 63) * kk) >> 8;
        uint32_t b = ((cc & 31) * kk) >> 8;
        r += c.aR[j]; if (r > 31) r = 31;
        g += c.aG[j]; if (g > 63) g = 63;
        b += c.aB[j]; if (b > 31) b = 31;
        row[x - bx0] = __builtin_bswap16((uint16_t)((r << 11) | (g << 5) | b));
      }
    }
  }
}

template <int P>
static inline __attribute__((always_inline)) void renderBandT(int k, uint16_t* buf, Scratch& sc) {
  const int y0 = k * BAND, y1 = min(S, y0 + BAND);
  const int bx0 = bandX0[k], bw = bandW[k], bx1 = bx0 + bw - 1;
  const uint16_t* const lp = lut;
  const uint16_t* const tp = tex;
  for (int y = y0; y < y1; y += P) {
    uint16_t* row = buf + (y - y0) * bw;
    int xs = max((int)spanS[y], bx0), xe = min((int)spanE[y], bx1);
    if (P == 2) xs &= ~1;
    for (int x = bx0; x < xs; ++x) row[x - bx0] = blackCol;
    for (int x = max(xe + 1, bx0); x <= bx1; ++x) row[x - bx0] = blackCol;
    if (xs <= xe) {
      // この行の「地図を引く位置」(粗い格子を縦に補間。1/256 目盛り単位)
      int gy = y / CSTEP; float fy = (y - gy * CSTEP) * (1.0f / CSTEP);
      const float *au = gU + gy * CW, *av = gV + gy * CW;
      for (int gx = 0; gx < CW - 1; ++gx) {
        sc.rx[gx] = (int32_t)((au[gx] + (au[gx + CW] - au[gx]) * fy) * 256.0f);
        sc.ry[gx] = (int32_t)((av[gx] + (av[gx + CW] - av[gx]) * fy) * 256.0f);
      }
      const int32_t rowOff = (y & 1) ? 128 : 0;   // 奇数行は半目盛りずらす(四角い段差を崩す)
      int x = xs;
      while (x <= xe) {
        const int gx = x / CSTEP;
        const int segEnd = min(xe, gx * CSTEP + CSTEP - 1);
        // 1ピクセルあたりの傾き: 節(CSTEP px ごと)の差を 1/CSTEP にする
        const int32_t stepu = (sc.rx[gx + 1] - sc.rx[gx] + (1 << (USHIFT - 1))) >> USHIFT;
        const int32_t stepv = (sc.ry[gx + 1] - sc.ry[gx] + (1 << (USHIFT - 1))) >> USHIFT;
        const int off = x - gx * CSTEP;
        int32_t u = sc.rx[gx] + stepu * off + rowOff;
        int32_t v = sc.ry[gx] + stepv * off;
        const int32_t du = stepu * P, dv = stepv * P;
        for (; x <= segEnd; x += P, u += du, v += dv) {
          const uint32_t iu = ((uint32_t)u >> 8) & TEXM;
          const uint32_t iv = ((uint32_t)v >> 8) & TEXM;
          const uint16_t col = lp[tp[(iv << TEXSH) | iu] >> 2];
          row[x - bx0] = col;
          if (P == 2 && x + 1 <= bx1) row[x + 1 - bx0] = col;
        }
      }
    }
    if (P == 2 && y + 1 < y1) memcpy(row + bw, row, bw * sizeof(uint16_t));
  }
  applyCores(k, buf);
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
  Sc = (S - 1) * 0.5f;
  XOFF = (W - S) / 2;
  YOFF = ((H - S) / 2 / BAND) * BAND;
  CW = S / CSTEP + 3;
  texPpx = TEXSPAN * TEX / (float)S;
  NB = (S + BAND - 1) / BAND;
  PIXEL = (QUALITY == 2) ? 2 : 1;
  Serial.printf("screen %dx%d  area %d  quality %d\n", W, H, S, QUALITY);

  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setBrightness(brightSteps[brightIdx]);

  for (int i = 0; i <= SINN - 1; ++i) sinT[i] = sinf(i * (6.2831853f / SINN));

  noiseT = (float*)malloc(NOISE * NOISE * sizeof(float));
  for (int i = 0; i < NSLOT; ++i) slotBuf[i] = (uint16_t*)heap_caps_malloc(S * BAND * 2, MALLOC_CAP_DMA);
  // 塊の地図は大きいので PSRAM を先に試す(無ければ内蔵メモリ)
  if (TEX * TEX * 2 <= 40 * 1024)             // 小さければ内蔵メモリのほうが速い
    tex = (uint16_t*)heap_caps_malloc((size_t)TEX * TEX * 2, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  if (!tex) tex = (uint16_t*)ps_malloc((size_t)TEX * TEX * 2);
  if (!tex) tex = (uint16_t*)malloc((size_t)TEX * TEX * 2);
  lut     = (uint16_t*)malloc(LUTN * sizeof(uint16_t));
  rampTab = (uint16_t*)malloc(NTYPE * 256 * sizeof(uint16_t));
  spanS  = (int16_t*)malloc(S * sizeof(int16_t)); spanE = (int16_t*)malloc(S * sizeof(int16_t));
  bandX0 = (int16_t*)malloc(NB * sizeof(int16_t)); bandW = (int16_t*)malloc(NB * sizeof(int16_t));
  bandVis = (uint8_t*)malloc(NB);
  int CN = CW * CW;
  gU = (float*)calloc(CN, sizeof(float)); gV = (float*)calloc(CN, sizeof(float));
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
  bool ok = noiseT && tex && lut && rampTab && spanS && spanE && bandX0 && bandW && bandVis &&
            gU && gV && scr[0].rx && scr[0].ry && scr[1].rx && scr[1].ry;
  for (int i = 0; i < NSLOT; ++i) ok = ok && slotBuf[i];
  for (int i = 0; i < 4; ++i) ok = ok && nmX[i].i0 && nmX[i].i1 && nmX[i].f && nmY[i].i0 && nmY[i].i1 && nmY[i].f;
  if (!ok) fail("Memory error");
  memset(lut, 0, LUTN * sizeof(uint16_t));

  blackCol = toPix({0, 0, 0});
  bool round = (W == H);
  float rr = S * 0.5f + 1.0f;
  for (int y = 0; y < S; ++y) {
    if (!round) { spanS[y] = 0; spanE[y] = S - 1; continue; }
    float dy = y - Sc, w = rr * rr - dy * dy;
    if (w <= 0) { spanS[y] = 1; spanE[y] = 0; continue; }
    float s = sqrtf(w);
    spanS[y] = max(0, (int)floorf(Sc - s)); spanE[y] = min(S - 1, (int)ceilf(Sc + s));
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

  makeNoise(noiseT, 3);
  buildRamps();
  buildTexture();
  buildCells();
  initVort();

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
    if (M5.BtnA.wasHold())         { nVort = (nVort >= MAXV) ? 3 : nVort + 1; spreadVort(); }
    else if (M5.BtnA.wasClicked()) { theme = (theme + 1) % NPAL; buildRamps(); }
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
      if (now - lastTapMs < 350) { theme = (theme + 1) % NPAL; buildRamps(); lastTapMs = 0; }
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
