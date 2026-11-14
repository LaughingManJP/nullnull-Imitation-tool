// NullWormhole  -  NullMenu 用モジュール(元のスケッチ NullWormhole_v10 を名前空間に入れたもの)
// 0 で始まる変数(配列も)は、ほかの作品と共通の置き場(NM_ARENA)に置いています。
// 一度に動くのは1つだけ(切り替えるときは再起動)なので、重なっていても問題ありません。
#include "nm_common.h"
#include <string.h>
#if defined(ESP_PLATFORM) && __has_include(<lgfx/v1/panel/Panel_AMOLED.hpp>)
#include <lgfx/v1/panel/Panel_AMOLED.hpp>
#endif
#pragma GCC optimize("O3")

namespace NullWormhole {
static constexpr size_t nmOv0 = 0;

// NullWormhole v10  -  ワームホールをくぐりぬけて、ブラックホールの前に出る
//   大阪・関西万博のパビリオン「null²」を見た印象をもとにした、非公式の習作です。写真や他の作品の画像は使っていません。
//   M5Stack StopWatch 用(丸い 466x466 画面)。CoreS3 系でも真ん中に正方形で動きます。
//
// 画面の中(くり返す):
//   1. ワームホール … まっ暗なトンネルの奥に、白く光る出口。まわりの壁には、金色の稲妻のような細いすじがはしり、
//      光のにじみをまといながら手前へなめらかに流れてくる。光の粒が舞い、横一文字の青い光がさす。出口はだんだん大きくなる
//   2. 光 … 出口の光が画面いっぱいに広がり、放射状の光の筋がのびる
//   3. ブラックホール … 光が引くと、星空の中に黒い穴。斜めに傾いた、炎のような厚い円盤がゆっくり回る。円盤の奥の側は
//      曲げられて穴の上にアーチになり、手前の側は穴の前を横切る。左側が明るく、暗い所は赤く、明るい所は白く光る。うしろの星は、穴のまわりで曲がって見える
//   しばらくすると、また新しいワームホールへ入っていく
//
// 触ると:
//   ・タップ      : ブラックホールのとき → すぐワームホールへ飛びこむ / ワームホールのとき → ぐっと加速
//   ・なぞる      : ブラックホールの円盤の傾きが変わる / ワームホールのすじがうずを巻く
//   ・本体を傾ける: 見る向きが変わる(トンネルの奥が動いて、曲がって見える。左右に傾けると画面も少し回る)
//
// 操作:
//   StopWatch … ボタンA: 光の色(金 / 青白 / 紅 / 白銀)  ボタンB: 速さ(押すたびに速くなる。4段目の次はふつうに戻る)
//   CoreS3系  … 電源ボタン: 光の色
//
// 必要なもの:
//   ボードマネージャー M5Stack >= 3.3.7 / ボード: M5StopWatch(CoreS3 なら M5CoreS3)。StopWatch では PSRAM を「OPI PSRAM」に
//   ライブラリ M5Unified >= 0.2.15, M5GFX >= 0.2.21
//
// しくみ:
//   ワームホールは、点ごとに「中心からの向きと、遠さの逆数(奥行き)」を求め、起動時に作った稲妻の模様(256x256、上下左右が
//   つながる。大きさのちがう4段のノイズの0の所を細く光らせ、ぼかした光も焼きこむ)を、向きと log(距離) で、
//   4点をまぜてなめらかに引く。奥行きを時間でずらすと、手前へ流れる。出口のまわりほど明るくする。
//   ブラックホールは、点ごとに中心からの距離で、黒い影・光子の輪・まわりの円盤の光(上と下が明るい)を決め、横一文字の円盤を
//   重ねる。星空は、穴に近いほど外へずらして引く(重さで光が曲がる)。
//   画面は16行ずつの帯に分けて、2つのコアのうち空いた方が、次の帯を描く。



// Arduino IDE は関数の宣言を自動でこのあたりに足すので、型の名前だけ先に宣言しておく
struct Tone; struct WormSt;

// ---- 調整用 ----------------------------------------------------------------
static constexpr float WORM_SEC  = 20.0f;    // ワームホールの中にいる時間(秒)
static constexpr float FLASH_SEC = 1.8f;     // 光が広がる時間
static constexpr float BH_SEC    = 22.0f;    // ブラックホールの前にいる時間(そのあと、また飛びこむ)
static constexpr float SHADOW    = 0.15f;    // ブラックホールの黒い影の大きさ(画面の半分を 1 として)
static constexpr float DISK_INC  = 0.30f;    // 円盤を見る角度(0 に近いほど真横から)
static constexpr float DISK_IN   = 1.5f;
static constexpr float DISK_H    = 0.22f;    // 円盤の厚み(半分)     // 円盤の内側のふち(影の大きさ = 1)
static constexpr float DISK_OUT  = 6.5f;     // 円盤の外側のふち
static constexpr float DISK_ANGLE = 0.38f;   // 円盤の傾き(右下がり)
static constexpr float DEPTH_POW = 0.55f;    // トンネルの奥行きの見え方(大きいほど、手前が速く流れ去る)
static constexpr float DEPTH_K   = 340.0f;   // トンネルの壁の模様の細かさ(奥行きの向き)    // ブラックホールの黒い影の大きさ(画面の半分を 1 として)
static constexpr bool  TILT        = true;    // 傾きで、見る向きを変えるか
static constexpr float TILT_GAIN   = 3.5f;    // 傾きの感度(大きいほど、少し傾けただけで大きく動く)
static constexpr float TILT_LOOK   = 0.40f;   // 傾けたとき、見る向きが変わる大きさ(いちばん傾けたとき)
static constexpr float TILT_ROLL   = 0.30f;   // 左右に傾けたとき、画面が回る大きさ
static constexpr bool  TILT_FLIP_X = false;   // 左右が逆に動くときは true
static constexpr bool  TILT_FLIP_Y = false;   // 上下が逆に動くときは true
static constexpr bool  TILT_STOPWATCH_FIX = true;   // StopWatch はセンサーが 90 度回ってついているので、自動で読みかえる
static constexpr float TILT_RELEVEL = 8.0f;   // 同じ角度で持ち続けると、何秒くらいでそこを「まっすぐ」にするか(0 = 起動したときの姿勢のまま)
static constexpr float TILT_RELEVEL_MAX = 0.15f;   // これより大きく傾けているあいだは、まっすぐを合わせなおさない
// ---------------------------------------------------------------------------

static constexpr bool DIRECT_SEND = true;          // StopWatch では、画面へ「直接」送る。画面が乱れるときは false に
#if defined(ESP_PLATFORM) && __has_include(<lgfx/v1/panel/Panel_AMOLED.hpp>)
#define NL_DIRECT_AMOLED 1
struct NlFbPeek : public lgfx::Panel_AMOLED_Framebuffer {
  static lgfx::Panel_AMOLED* inner(lgfx::Panel_Device* p) {
    return static_cast<lgfx::Panel_AMOLED_Framebuffer*>(p)->*(&NlFbPeek::_panel);
  }
};
#endif

static constexpr int BAND = 16, NSLOT = 4;
static constexpr int NTONE = 4;
static constexpr int NT = 256;                    // 稲妻の模様の大きさ
static constexpr int NRAY = 512;                  // 光の筋の細かさ
static constexpr int NSPEED = 4;
static const float SPEEDS[NSPEED] = {1.0f, 1.6f, 2.4f, 3.4f};   // ボタンB で速くなる(最後の次はふつうに戻る)

// 光の色: すじ / 出口 / 円盤
struct Tone { const char* name; uint8_t vein[3], core[3], disk[3], flareA[3], flareB[3]; };
static const Tone TONES[NTONE] = {
  {"gold",   {255, 214, 150}, {255, 250, 236}, {255, 150,  70}, {255, 150,  60}, {110, 170, 255}},
  {"blue",   {160, 210, 255}, {236, 246, 255}, {190, 220, 255}, {120, 190, 255}, {255, 190, 120}},
  {"crimson",{255, 120,  90}, {255, 236, 224}, {255, 150, 110}, {255,  80,  60}, {130, 160, 255}},
  {"silver", {220, 224, 232}, {255, 255, 255}, {226, 232, 240}, {200, 210, 230}, {150, 190, 255}},
};
enum Phase { P_WORM = 0, P_FLASH, P_BH, P_DIVE };
struct WormSt {
  int phase; float pt;            // 場面と、その経過時間
  float travel, speed, boost, swirl, tilt, tiltT, zoom;
  int tone, speedIdx;
  float simT;
  float vel;                      // 進む速さ(急に変わらないように、少しずつ目標に寄せる)
  float tiltX, tiltY, levelX, levelY; int levelN;   // 本体の傾き
};

static int W, H, S, XOFF, YOFF, NB;
static bool roundScr = true, fbMode = false, directSend = false, imuOK = false;
static int16_t* spanA = nullptr;
static int16_t* spanB = nullptr;
static int16_t* bandX = nullptr;
static int16_t* bandW = nullptr;
static uint16_t* band[NSLOT] = {nullptr, nullptr, nullptr, nullptr};
static constexpr int NC = 128;                    // 雲の模様(銀河・星雲・壁の色むら)の大きさ
static uint8_t* cloudT = nullptr;
static uint8_t* veinT = nullptr;                  // 稲妻の模様 [NT * NT](0..255)。にじみも焼きこんである
static uint8_t rayT[NRAY];
static constexpr size_t nmOa0 = nmAlignTo(nmOv0, 16), nmOv1 = nmOa0 + sizeof(WormSt); static WormSt& Wm = *reinterpret_cast<WormSt*>(NM_ARENA + nmOa0);
static uint32_t rnd = 12345;
static inline float frand() { rnd = rnd * 1664525u + 1013904223u; return (rnd >> 8) * (1.0f / 16777216.0f); }
// コマごとに決める値(描く方が読む)
static int   fPhase = 0;
static float fSkyRot = 0, fLx = 0, fLy = 0, fRc = 1, fRs = 0, fBx = 0, fBy = 0, fVel = 0, fP = 0, fExitR = 0.2f, fTrav = 0, fSwirl = 0, fFlare = 0, fFlash = 0, fBh = 0, fWormA = 1, fTilt = 0.05f, fZoom = 1, fT = 0;
static float fVein[3], fCore[3], fDisk[3], fFa[3], fFb[3];

static SemaphoreHandle_t semGo = nullptr, semDone = nullptr;
static volatile int doneUpTo = -1;
static volatile int nextBand = 0;
static volatile uint8_t bandReady[64];
static uint32_t busyUs[2] = {0, 0}, sendUs = 0;

static inline __attribute__((always_inline)) float ffloorf(float x) { const int i = (int)x; return (float)(i - (x < (float)i ? 1 : 0)); }   // floorf の速い版
static inline __attribute__((always_inline)) float fminF(float a, float b) { return a < b ? a : b; }
static inline __attribute__((always_inline)) float fmaxF(float a, float b) { return a > b ? a : b; }
static inline __attribute__((always_inline)) float clamp01(float v) { return v < 0 ? 0 : (v > 1 ? 1 : v); }
static inline __attribute__((always_inline)) float smooth01(float v) { v = clamp01(v); return v * v * (3 - 2 * v); }
static inline __attribute__((always_inline)) float frsqrtF(float x) {
  union { float f; uint32_t i; } u = {x}; u.i = 0x5F3759DFu - (u.i >> 1);
  float y = u.f; y = y * (1.5f - 0.5f * x * y * y); return y;
}
static inline __attribute__((always_inline)) float fexp(float x) {               // exp(x) の速い近似(光のにじみには十分)
  if (x < -30.0f) return 0.0f; if (x > 30.0f) x = 30.0f;
  union { float f; int32_t i; } u; u.i = (int32_t)(12102203.0f * x + 1064866805.0f); return u.f;
}
static inline __attribute__((always_inline)) float fsinF(float x) {             // sin(x) の速い近似
  x *= 0.15915494f; x -= ffloorf(x + 0.5f); x *= 6.2831853f;
  float y = x * (1.27323954f - 0.40528473f * fabsf(x));
  return y * (0.775f + 0.225f * fabsf(y));
}
static inline __attribute__((always_inline)) float frcpF(float x) {
  union { float f; uint32_t i; } u = {x}; u.i = 0x7EF311C7u - u.i;
  float r = u.f; r = r * (2.0f - x * r); r = r * (2.0f - x * r); return r;
}
static inline __attribute__((always_inline)) float fatan2(float y, float x) {    // 向き(-π..π)。近似
  const float ax = fabsf(x), ay = fabsf(y), mx = ax > ay ? ax : ay, mn = ax > ay ? ay : ax;
  const float a = mn * frcpF(mx + 1e-9f), s = a * a;
  float r = ((-0.0464964749f * s + 0.15931422f) * s - 0.327622764f) * s * a + a;
  if (ay > ax) r = 1.57079637f - r;
  if (x < 0) r = 3.14159274f - r;
  return y < 0 ? -r : r;
}
static inline __attribute__((always_inline)) uint32_t hash2(int a, int b) { uint32_t h = (uint32_t)a * 374761393u + (uint32_t)b * 668265263u; h = (h ^ (h >> 13)) * 1274126177u; return h ^ (h >> 16); }
static const uint8_t BAYER[16] = {0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5};
static inline __attribute__((always_inline)) uint16_t packD(int r, int g, int b, int x, int y) {
  const int d = BAYER[((y & 3) << 2) | (x & 3)];
  r += d >> 1; g += d >> 2; b += d >> 1;
  r = r > 255 ? 255 : (r < 0 ? 0 : r); g = g > 255 ? 255 : (g < 0 ? 0 : g); b = b > 255 ? 255 : (b < 0 ? 0 : b);
  return __builtin_bswap16((uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)));
}
static inline __attribute__((always_inline)) float flog2(float x) {             // log2 の近似(x > 0)
  union { float f; uint32_t i; } u = {x};
  const float e = (float)((int)(u.i >> 23) - 127);
  u.i = (u.i & 0x007FFFFFu) | 0x3F800000u; const float m = u.f;
  return e + (-0.34484843f * m + 2.02466578f) * m - 0.67487759f;
}
static void fail(const char* msg);

// ---------------------------------------------------------------- 稲妻の模様(起動時)
static float vnoiseP(float x, float y, int P, uint32_t seed) {   // 上下左右が P でつながる、なめらかなノイズ(-1..1)
  const int ix = (int)ffloorf(x), iy = (int)ffloorf(y); const float fx = x - ix, fy = y - iy;
  const float ux = fx * fx * (3 - 2 * fx), uy = fy * fy * (3 - 2 * fy);
  auto h = [&](int a, int b) { return (hash2(((a % P) + P) % P + seed * 977, ((b % P) + P) % P) & 0xFFFF) * (2.0f / 65535.0f) - 1.0f; };
  const float a = h(ix, iy), b = h(ix + 1, iy), c = h(ix, iy + 1), d = h(ix + 1, iy + 1);
  return (a + (b - a) * ux) + ((c + (d - c) * ux) - (a + (b - a) * ux)) * uy;
}
static void blurWrap(uint16_t* src, uint16_t* tmp, int rad) {   // 上下左右がつながる箱ぼかし(2回で山なりに)
  for (int pass = 0; pass < 2; ++pass) {
    for (int j = 0; j < NT; ++j) {               // 横
      uint32_t acc = 0; uint16_t* row = src + j * NT;
      for (int k = -rad; k <= rad; ++k) acc += row[k & (NT - 1)];
      for (int i = 0; i < NT; ++i) { tmp[j * NT + i] = (uint16_t)(acc / (2 * rad + 1)); acc += row[(i + rad + 1) & (NT - 1)]; acc -= row[(i - rad) & (NT - 1)]; }
    }
    for (int i = 0; i < NT; ++i) {               // 縦
      uint32_t acc = 0;
      for (int k = -rad; k <= rad; ++k) acc += tmp[(k & (NT - 1)) * NT + i];
      for (int j = 0; j < NT; ++j) { src[j * NT + i] = (uint16_t)(acc / (2 * rad + 1)); acc += tmp[((j + rad + 1) & (NT - 1)) * NT + i]; acc -= tmp[((j - rad) & (NT - 1)) * NT + i]; }
    }
    delay(1);
  }
}
static __attribute__((cold)) void buildVeins() {
  uint16_t* line = (uint16_t*)ps_malloc(NT * NT * 2);
  uint16_t* g1 = (uint16_t*)ps_malloc(NT * NT * 2);
  uint16_t* g2 = (uint16_t*)ps_malloc(NT * NT * 2);
  uint16_t* tmp = (uint16_t*)ps_malloc(NT * NT * 2);
  if (!line || !g1 || !g2 || !tmp) fail("Memory error (veins)");
  for (int j = 0; j < NT; ++j) {
    for (int i = 0; i < NT; ++i) {
      // うねらせた座標で、ノイズの 0 の所を細い線にする。大きさのちがう4段を重ねると、枝分かれした稲妻になる
      const float x = i * (8.0f / NT), y = j * (8.0f / NT);
      const float wx = x + 0.7f * vnoiseP(x * 0.5f, y * 0.5f, 4, 3), wy = y + 0.7f * vnoiseP(x * 0.5f + 7, y * 0.5f, 4, 5);
      float v = 0, amp = 1.0f;
      static const int PP[4] = {8, 16, 32, 64}; static const float WW[4] = {10, 13, 16, 20};
      for (int k = 0; k < 4; ++k) {
        const float m = PP[k] * 0.125f, n = fabsf(vnoiseP(wx * m, wy * m, PP[k], 10 + k));
        const float l = fmaxF(0.0f, 1.0f - n * WW[k]); v += l * l * l * amp; amp *= 0.6f;
      }
      const float lump = 0.5f + 0.5f * vnoiseP(x * 0.75f, y * 0.75f, 6, 29);   // 明るい所と暗い所のむら
      v *= 0.3f + 1.0f * lump;
      line[j * NT + i] = (uint16_t)fminF(65535.0f, v * 40000.0f);
    }
    if ((j & 15) == 0) delay(1);
  }
  memcpy(g1, line, NT * NT * 2); blurWrap(g1, tmp, 1);
  memcpy(g2, line, NT * NT * 2); blurWrap(g2, tmp, 4);
  for (int k = 0; k < NT * NT; ++k) {             // 細い線 + 近いにじみ + 遠いにじみ
    const float v = line[k] * (1.0f / 40000.0f) * 0.75f + g1[k] * (1.0f / 40000.0f) * 0.9f + g2[k] * (1.0f / 40000.0f) * 1.0f;
    veinT[k] = (uint8_t)fminF(255.0f, v * 200.0f);
  }
  free(line); free(g1); free(g2); free(tmp);
  for (int j = 0; j < NC; ++j) {                  // 雲: 大きさのちがうノイズを重ねた、やわらかなむら
    for (int i = 0; i < NC; ++i) {
      const float x = i * (8.0f / NC), y = j * (8.0f / NC);
      const float n = vnoiseP(x, y, 8, 51) * 0.5f + vnoiseP(x * 2, y * 2, 16, 52) * 0.27f + vnoiseP(x * 4, y * 4, 32, 53) * 0.15f + vnoiseP(x * 8, y * 8, 64, 54) * 0.08f;
      cloudT[j * NC + i] = (uint8_t)(clamp01(n * 0.75f + 0.5f) * 255.0f);
    }
    if ((j & 15) == 0) delay(1);
  }
  for (int i = 0; i < NRAY; ++i) {                // 光の筋(向きごとの明るさ)
    const float n = 0.5f + 0.5f * vnoiseP(i * (180.0f / NRAY), 0.5f, 180, 7);
    rayT[i] = (uint8_t)(n * n * n * n * 255.0f);
  }
}
static inline __attribute__((always_inline)) float texBL(float fx, float fy) {   // 模様をなめらかに引く(4点のまぜ)
  const int ix = (int)ffloorf(fx), iy = (int)ffloorf(fy);
  const int ax = (int)((fx - ix) * 256.0f), ay = (int)((fy - iy) * 256.0f);
  const int x0 = ix & (NT - 1), x1 = (ix + 1) & (NT - 1), y0 = (iy & (NT - 1)) * NT, y1 = ((iy + 1) & (NT - 1)) * NT;
  const int a = veinT[y0 + x0], b = veinT[y0 + x1], c = veinT[y1 + x0], d = veinT[y1 + x1];
  const int top = a * 256 + (b - a) * ax, bot = c * 256 + (d - c) * ax;
  return (float)(top * 256 + (bot - top) * ay) * (1.0f / (255.0f * 65536.0f));
}
static inline __attribute__((always_inline)) float cloudBL(float fx, float fy) {   // 雲の模様をなめらかに引く(0..1)
  const int ix = (int)ffloorf(fx), iy = (int)ffloorf(fy); const float ax = fx - ix, ay = fy - iy;
  const int x0 = ix & (NC - 1), x1 = (ix + 1) & (NC - 1), y0 = (iy & (NC - 1)) * NC, y1 = ((iy + 1) & (NC - 1)) * NC;
  const float a = cloudT[y0 + x0], b = cloudT[y0 + x1], c = cloudT[y1 + x0], d = cloudT[y1 + x1];
  return ((a + (b - a) * ax) + ((c + (d - c) * ax) - (a + (b - a) * ax)) * ay) * (1.0f / 255.0f);
}
// 天の川(帯になった銀河): 斜めの帯、中ほどのふくらみ、暗い塵の筋、まわりの淡い星雲。lu, lv は空の向き
static __attribute__((noinline)) void galaxyAt(float lu, float lv, float& r, float& g, float& b) {
  const float bx = lu * 0.906f + lv * 0.423f, by = -lu * 0.423f + lv * 0.906f;
  const float cl = cloudBL(bx * 55.0f + 300.0f, by * 55.0f + 300.0f), du = cloudBL(bx * 140.0f + 50.0f, by * 140.0f + 77.0f);
  const float core = fexp(-by * by * 12.8f), tl = clamp01((du - 0.5f) * 4.0f), lane = tl * tl * (3 - 2 * tl) * fexp(-by * by * 150.0f);
  const float bul = fexp(-((bx + 0.45f) * (bx + 0.45f) * 10.0f + by * by * 30.0f));
  const float I = (core * (0.3f + 0.7f * cl * cl) * 62.0f + bul * 95.0f) * (1.0f - 0.8f * lane);
  const float wm = clamp01(1.0f - fabsf(by) * 3.0f + bul);
  r = I * (0.70f + 0.30f * wm); g = I * (0.78f + 0.10f * wm); b = I * (1.0f - 0.28f * wm);
  const float n2 = cloudBL(lu * 18.0f + 11.0f, lv * 18.0f + 97.0f), nb = n2 * n2 * n2 * 14.0f;   // 淡い星雲(青と赤むらさき)
  r += nb * 0.8f; g += nb * 0.35f; b += nb * 1.1f;
}
// 星(向き lu, lv の空)。帯の中ほど多い
static __attribute__((noinline)) void starsAt(float lu, float lv, float dens, float& r, float& g, float& b) {
  const float su = lu * 90.0f + 1000.0f, sv = lv * 90.0f + 1000.0f;
  const int cx = (int)su, cy = (int)sv; const uint32_t hh = hash2(cx, cy);
  if ((hh & 255) < (uint32_t)(5.0f + 50.0f * dens)) {
    const float fx = su - cx - ((hh >> 8) & 15) * 0.0625f, fy = sv - cy - ((hh >> 12) & 15) * 0.0625f;
    const float bri = ((hh >> 16) & 255) * (1.0f / 255.0f), sk = fexp(-(fx * fx + fy * fy) * 220.0f) * bri * bri * 330.0f;
    const float t = ((hh >> 24) & 255) * (1.0f / 255.0f);          // 星の色(青白 〜 黄)
    r += sk * (0.80f + 0.20f * t); g += sk * (0.85f + 0.05f * t); b += sk * (1.0f - 0.25f * t);
  }
}
static inline __attribute__((always_inline)) float rayAt(float a) {               // a: -π..π
  const float f = (a * (1.0f / 6.2831853f) + 1.0f) * NRAY; const int i = (int)f; const float t = f - i;
  return (rayT[i % NRAY] + (rayT[(i + 1) % NRAY] - rayT[i % NRAY]) * t) * (1.0f / 255.0f);
}

// ---------------------------------------------------------------- 描く
// 1点の色(0..255 をこえることもある)。コマごとに決まる値は prepShade() で先に計算しておく
static float sLensK = 1, sC = 0, sIR = 0, sEx = 0, sWa = 0, sBh = 0, sFl = 0, sSh = 0, sCt = 1, sSt = 0, sPulse = 0;
static void prepShade() {
  sC = (S - 1) * 0.5f; sIR = 2.0f / S;
  sEx = fExitR; sWa = fWormA; sBh = fBh; sFl = fFlash;
  sSh = SHADOW * fZoom; sCt = cosf(fTilt); sSt = sinf(fTilt);
  sPulse = fsinF(fT * 5.0f);
  sLensK = 1.0f / (sSh * sSh * 0.08f);
}
static void IRAM_ATTR __attribute__((noinline, optimize("O2"))) shadeW(int x, int y, float& R, float& G, float& B) {
  const float c = sC, iR = sIR, ex = sEx, wa = sWa, bh = sBh, fl = sFl, sh = sSh, ct = sCt, st = sSt;
  const float v = (y - c) * iR;
    const float u = (x + 0.5f - c) * iR;
    const float r2 = u * u + v * v + 1e-6f, ir = frsqrtF(r2), r = r2 * ir;
    R = 0; G = 0; B = 0;
    if (wa > 0.003f) {                                  // ---- ワームホール
      // 見る向き: 傾けると、出口(いちばん奥)が動く。奥ほど大きく動くので、トンネルが曲がって見える。左右の傾きで画面も少し回る
      const float ru = u * fRc - v * fRs, rv = u * fRs + v * fRc;
      const float bu = ru - fLx, bv = rv - fLy, q1 = (bu * bu + bv * bv) * frsqrtF(bu * bu + bv * bv + 1e-6f) * frcpF(ex);
      const float bend = 0.35f + 0.65f * fminF(1.0f, 1.8f * frcpF(q1 + 0.8f));
      const float u = ru - fLx * bend, v = rv - fLy * bend;
      const float r2 = u * u + v * v + 1e-6f, ir = frsqrtF(r2), r = r2 * ir;
      (void)ir;
      const float atw = fatan2(v, u);                   // 向き(2か所で使う)
      float wr = 0, wg = 0, wb = 0;
      // 出口: やわらかいふちの白い光と、まわりのにじみ
      const float q = r * frcpF(ex);                    // 出口の大きさを 1 として
      if (q < 1.05f) {                                  // 出口の向こう: まぶしい光の中に、行き先の銀河が透けて見える
        const float k = clamp01((1.04f - q) * 14.0f), ie = frcpF(ex) * 0.7f;
        float gr = 0, gg = 0, gb = 0; galaxyAt(u * ie + 0.1f, v * ie, gr, gg, gb); starsAt(u * ie, v * ie, 0.5f, gr, gg, gb);
        const float hot = fexp(-q * q * 2.5f) * 0.55f + 0.35f;
        wr = (fCore[0] * hot + gr * 3.0f) * k; wg = (fCore[1] * hot + gg * 3.0f) * k; wb = (fCore[2] * hot + gb * 3.0f) * k;
      }
      float glowK = 0;
      if (q > 0.96f) {
        // 壁の稲妻: 筒の壁を中から見たときの奥行き(遠いほどつまって見える)で貼る。手前ほど速く流れ去る
        const float lq = flog2(r * (1.0f / 0.13f));   // 模様の位置は出口の大きさに引っぱられないよう、決まった大きさを基準に
        const float z = fexp(-lq * (0.6931472f * DEPTH_POW));   // 奥行き(出口のあたり 1、画面のふちで小さい)
        const float ang = atw + fSwirl * lq * 0.35f + lq * 0.22f;   // 奥へ行くほど少しねじれる
        const float tx = (ang * (1.0f / 6.2831853f) + 1.0f) * NT * 3.0f;
        const float ty = z * DEPTH_K + fTrav * 70.0f + 4096.0f;
        // 速いほど、流れる向きにぶれる(2点の平均)
        float vn = (texBL(tx, ty) + texBL(tx, ty - fVel * 70.0f * 0.06f)) * 0.5f;
        float vr = vn, vb = vn;
        // 明るさ: 出口のまわりが明るく、外へいくほど落ちる。出口のすぐそばは、細かすぎるのでぼかす
        const float rim = (fexp(-(q - 1.0f) * 0.9f) * 1.4f + 0.35f) * (1.0f + 0.06f * sPulse);   // 光だけ、ゆっくり脈打つ
        const float near = clamp01((q - 1.0f) * 1.7f);
        const float lk = rim * near * 1.25f;
        wr += fVein[0] * vr * lk; wg += fVein[1] * vn * lk * 0.95f; wb += fVein[2] * vb * lk * 0.85f;
        // 壁の星雲の色むら(青むらさき)と、出口の光に照らされた霧
        const float cl = cloudBL(tx * 0.06f, ty * 0.12f), cn = cl * cl * fexp(-(q - 1.0f) * 0.35f) * 36.0f;
        wr += cn * 0.55f; wg += cn * 0.45f; wb += cn * 1.25f;
        const float fog = fexp(-(q - 1.0f) * 0.6f) * 10.0f;
        wr += fog; wg += fog * 0.62f; wb += fog * 0.38f;
        glowK = fexp(-(q - 1.0f) * 2.2f);     // 出口のにじみ
        // 光の粒(流れに乗って動く)
        { const float px = tx * 0.5f, py = ty * 0.25f; const int ipx = (int)px, ipy = (int)ffloorf(py);
          const uint32_t hh = hash2(ipx, ipy);
          if ((hh & 511) < 4) {                         // 細い光のすじ(流れる向きに少し長い)
            const float dx = (px - ipx) - 0.5f, dy = (py - ipy) - 0.5f;
            const float k = fexp(-(dx * dx) * 60.0f - (dy * dy) * 10.0f) * (float)((hh >> 10) & 255) * 0.004f * clamp01(2.0f - q * 0.12f);
            wr += 255 * k; wg += 235 * k; wb += 210 * k; } }
      }
      { const float gr = rayAt(atw + fT * 0.03f) * fexp(-r * 2.2f) * clamp01((q - 1.0f) * 2.0f) * 0.35f;   // 出口からさす光の筋
        wr += fCore[0] * gr; wg += fCore[1] * gr * 0.9f; wb += fCore[2] * gr * 0.75f; }
      wr += fCore[0] * glowK * 0.85f; wg += fCore[1] * glowK * 0.72f; wb += fCore[2] * glowK * 0.5f;
      { const float rb = r * frcpF(ex * 2.5f), bl = fexp(-rb * rb) * 0.4f; wr += fCore[0] * bl; wg += fCore[1] * bl * 0.9f; wb += fCore[2] * bl * 0.75f; }
      // レンズの光: 横一文字の青い光、オレンジの筋、下の緑のにじみ
      if (fFlare > 0.01f) {
        const float au = fexp(-fabsf(u) * 1.5f);
        const float hl = (fexp(-(v * v) * 60000.0f) * 1.2f + fexp(-(v * v) * 2500.0f) * 0.25f * fexp(-fabsf(u) * 1.5f)) * au * fFlare;
        wr += fFb[0] * hl; wg += fFb[1] * hl; wb += fFb[2] * hl;
      }
      R += wr * wa; G += wg * wa; B += wb * wa;
    }
    if (bh > 0.003f) {                                  // ---- ブラックホール
      const float u = (x - c) * iR - fBx, v = (y - c) * iR - fBy;   // 傾けると、穴の見える位置が少し動く
      const float r2 = u * u + v * v + 1e-6f, ir = frsqrtF(r2), r = r2 * ir;
      float br = 0, bg = 0, bb = 0;
      // 天の川と星(穴の近くほど外へずらして引く = 重さで光が曲がる。穴のまわりに、うしろの空が輪のようにゆがんで見える)
      const float dfl = sh * sh * 1.3f * ir;
      if (r > sh * 1.02f) {
        const float k = (r + dfl) * ir, lu = u * k * 0.8f + fSkyRot, lv = v * k * 0.8f;
        float gr = 0, gg = 0, gb = 0; galaxyAt(lu, lv, gr, gg, gb);
        const float byy = -lu * 0.423f + lv * 0.906f;
        starsAt(lu, lv, fexp(-byy * byy * 12.8f), gr, gg, gb);
        const float e = r - sh * 1.05f, lens = 1.0f + 0.6f * fexp(-e * e * sLensK);   // 穴のふちは光が集まって明るい
        br += gr * lens; bg += gg * lens; bb += gb * lens;
      }
      // 円盤: 斜めから見た、厚みのある円盤。奥の側は重さで曲げられて、穴の上にアーチになって見え、穴の下にも細い輪が出る
      const float p = (u * ct + v * st) * frcpF(sh), w = (-u * st + v * ct) * frcpF(sh);   // 円盤にそった向き / 垂直の向き(穴の大きさ = 1)
      const float rho = r * frcpF(sh);
      const float dp0 = 1.0f - 0.72f * p * frcpF(fmaxF(rho, 1.0f)), dop = fminF(1.7f, 0.25f + 0.75f * dp0 * dp0 * (dp0 > 0 ? 1.0f : 0.0f));   // 左(こちらへ向かう側)だけ強く明るい(光が前へ集まる)
      float back = 0, front = 0;
      {                                                 // (1) 円盤そのもの
        const float wi = (fabsf(w) + DISK_H) * (1.0f / DISK_INC), Rd2 = p * p + wi * wi;   // 円盤には厚みがあるので、穴の前にも大きくかかる
        if (Rd2 < 49.0f) {
          const float Rd = Rd2 * frsqrtF(Rd2 + 1e-6f);
          const float phi = fatan2(w * (1.0f / DISK_INC), p);
          const float om = 22.0f * frsqrtF(Rd * Rd * Rd + 0.1f);   // 内側ほど速く回る
          const float t = texBL(phi * (NT * 2.0f / 6.2831853f) + fT * om + 4096.0f, Rd * 170.0f);
          const float Re = Rd + (t - 0.4f) * 1.1f;      // ふちをもこもこに
          float I = clamp01((Re - DISK_IN) * 2.5f) * clamp01((DISK_OUT - Re) * 0.7f) * fexp(-(Rd - DISK_IN) * 0.55f) * 1.6f;
          I *= 0.5f + 1.0f * t;
          { const float e = Rd - DISK_IN - 0.12f; I += fexp(-e * e * 60.0f) * 0.9f; }   // 内側のふち(いちばん熱い所)
          if (w > 0) front = I; else back = I;
        }
      }
      float arc = 0;                                    // (2) 穴の上のアーチ / (3) 穴の下の細い輪
      if (rho > 1.0f && rho < 3.2f) {
        const float s = -w * frcpF(rho);                // 上ほど 1
        float wt, Ra;
        if (s > -0.15f) { wt = clamp01(s * 2.6f + 0.4f); Ra = DISK_IN + (rho - 1.08f) * 2.0f; }
        else { wt = clamp01(-s * 2.0f - 0.2f) * 0.55f * clamp01((1.75f - rho) * 3.0f); Ra = DISK_IN + (rho - 1.02f) * 5.0f; }
        if (wt > 0.01f && Ra > DISK_IN - 0.5f) {
          const float th = fatan2(v, u);
          const float om = 22.0f * frsqrtF(Ra * Ra * Ra + 0.1f);
          const float t = texBL(th * (NT * 2.0f / 6.2831853f) - fT * om + 8192.0f, Ra * 110.0f + 512.0f);
          const float Re = Ra + (t - 0.4f) * 0.8f;
          arc = clamp01((Re - DISK_IN) * 3.0f) * clamp01((DISK_OUT - 1.0f - Re) * 0.8f) * fexp(-(Ra - DISK_IN) * 0.6f) * 2.3f * (0.65f + 0.7f * t) * wt;
        }
      }
      const float I = (back + arc) * dop;
      const float ring = fexp(-((rho - 1.02f) * (rho - 1.02f)) * 4000.0f) * 0.9f * (0.5f + 0.5f * dop);   // 光子の輪(細く、明るい側がより明るい)
      const float glow = fexp(-fabsf(rho - 1.5f) * 1.3f) * 0.28f + fexp(-(w * w) * 1.5f) * fexp(-fabsf(p) * 0.45f) * 0.18f;   // まわりのにじみ
      // 明るさ → 色(暗い所は赤く、明るい所は白く)
      const float bs = clamp01((dop - 1.0f) * 0.8f), rs = clamp01((1.0f - dop) * 1.2f);   // 近づく側は青白く、遠ざかる側は赤く
      auto addFire = [&](float k, float& rr, float& gg, float& bv) {
        const float m = fminF(k, 1.0f), h = fmaxF(0.0f, k - 0.85f) * 0.8f;
        rr += (fDisk[0] * k + 255 * h) * (1.0f - 0.15f * bs); gg += (fDisk[1] * k * (0.35f + 0.65f * m) + 240 * h) * (1.0f - 0.35f * rs); bv += (fDisk[2] * k * (0.2f + 0.8f * m * m) + 215 * h) * (1.0f + 0.6f * bs - 0.5f * rs);
      };
      addFire(I + glow, br, bg, bb);
      br += 255 * ring; bg += 225 * ring; bb += 190 * ring;
      if (rho < 1.0f) {                                 // 影(中はかすかに赤い)
        const float k = clamp01((1.0f - rho) * 40.0f);
        br = br * (1 - k) + fDisk[0] * 0.10f * k; bg = bg * (1 - k) + fDisk[1] * 0.03f * k; bb = bb * (1 - k) + fDisk[2] * 0.02f * k;
      }
      addFire(front * dop, br, bg, bb);                 // 手前の円盤は影の上にかかる
      R += br * bh; G += bg * bh; B += bb * bh;
    }
    if (fl > 0.003f) {                                  // ---- 光(画面いっぱいの白と、放射状の筋)
      const float a = fatan2(v, u) + fT * 0.05f;
      const float rr = r * frcpF(0.25f + 0.8f * fl);
      const float core = fexp(-rr * rr) * 1.6f, rays = rayAt(a) * fexp(-r * 1.4f) * 0.9f;
      const float k = clamp01((core + rays) * fl * 1.2f);
      R += (fCore[0] * 1.05f - R) * k; G += (fCore[1] * 0.98f - G) * k; B += (fCore[2] * 0.9f - B) * k;
    }
}
// 粗い格子: GQ(8)点おきに計算して、間はまぜて埋める。4すみの色の差が GTH より大きい四角は、4点おき → 2点おきと細かくする
static constexpr int GQ = 8, GMAX = 466 / GQ + 3, GTH = 40;
static uint32_t* gridB[NSLOT] = {nullptr, nullptr, nullptr, nullptr};
static inline uint32_t sampW(int x, int y) {
  float R, G, B; shadeW(min(S - 1, max(0, x)), min(S - 1, max(0, y)), R, G, B);
  const int r = R > 255.0f ? 255 : (R < 0 ? 0 : (int)R), g = G > 255.0f ? 255 : (G < 0 ? 0 : (int)G), b = B > 255.0f ? 255 : (B < 0 ? 0 : (int)B);
  return (uint32_t)r | ((uint32_t)g << 8) | ((uint32_t)b << 16);
}
static inline int gDiff(uint32_t a, uint32_t b, uint32_t c, uint32_t d) {   // 4つの色の、いちばん大きいちがい
  int m = 0;
  for (int sh = 0; sh < 24; sh += 8) {
    const int p = (a >> sh) & 255, q = (b >> sh) & 255, r = (c >> sh) & 255, t = (d >> sh) & 255;
    const int hi = max(max(p, q), max(r, t)), lo = min(min(p, q), min(r, t));
    if (hi - lo > m) m = hi - lo;
  }
  return m;
}
// (x0, y0) から n×n 点を、4すみの色をまぜて埋める(帯の中で、丸の中の点だけ)
static inline void fillQuad(uint16_t* out, int bx, int bw, int yb0, int yb1, int x0, int y0, int n, int sh, uint32_t c00, uint32_t c10, uint32_t c01, uint32_t c11) {
  const int r00 = c00 & 255, g00 = (c00 >> 8) & 255, b00 = (c00 >> 16) & 255, r10 = c10 & 255, g10 = (c10 >> 8) & 255, b10 = (c10 >> 16) & 255;
  const int r01 = c01 & 255, g01 = (c01 >> 8) & 255, b01 = (c01 >> 16) & 255, r11 = c11 & 255, g11 = (c11 >> 8) & 255, b11 = (c11 >> 16) & 255;
  for (int dy = 0; dy < n; ++dy) {
    const int y = y0 + dy;
    if (y < yb0 || y >= yb1) continue;
    const int xa = max(max(bx, (int)spanA[y]), x0), xb = min(min(bx + bw - 1, (int)spanB[y]), x0 + n - 1);
    if (xa > xb) continue;
    const int wy = n - dy;
    const int rl = r00 * wy + r01 * dy, gl = g00 * wy + g01 * dy, bl = b00 * wy + b01 * dy;   // 左のふち(n 倍)
    const int rr = r10 * wy + r11 * dy, gr = g10 * wy + g11 * dy, br = b10 * wy + b11 * dy;   // 右のふち
    uint16_t* o = out + (y - yb0) * bw - bx;
    for (int x = xa; x <= xb; ++x) {
      const int dx = x - x0, wx = n - dx;
      o[x] = packD((rl * wx + rr * dx) >> sh, (gl * wx + gr * dx) >> sh, (bl * wx + br * dx) >> sh, x, y);
    }
  }
}
// n 点の四角: 色の差が小さければまぜて埋める。大きければ4つに分けて、まん中とふちの点を計算する(2点まで)
static void __attribute__((noinline, noclone, optimize("O2"))) quadW(uint16_t* out, int bx, int bw, int y0, int y1, int qx, int qy, int n, bool force, uint32_t c00, uint32_t c10, uint32_t c01, uint32_t c11) {
  int sh = 0; for (int t = n; t > 1; t >>= 1) sh += 2;     // n*n = 1 << sh
  if (n <= 2 || (!force && gDiff(c00, c10, c01, c11) <= GTH)) { fillQuad(out, bx, bw, y0, y1, qx, qy, n, sh, c00, c10, c01, c11); return; }
  const int m = n >> 1;
  const uint32_t t0 = sampW(qx + m, qy), l0 = sampW(qx, qy + m), cc = sampW(qx + m, qy + m), r0 = sampW(qx + n, qy + m), b0 = sampW(qx + m, qy + n);
  for (int part = 0; part < 4; ++part) {                    // 4つの小さな四角のうち、丸の中に点があるものだけ
    const int px = qx + (part & 1) * m, py = qy + (part >> 1) * m;
    bool any = false;
    for (int y = max(py, y0); y < min(y1, py + m) && !any; ++y) any = spanA[y] <= spanB[y] && spanA[y] <= px + m - 1 && spanB[y] >= px;
    if (!any) continue;
    if (part == 0) quadW(out, bx, bw, y0, y1, px, py, m, force, c00, t0, l0, cc);
    if (part == 1) quadW(out, bx, bw, y0, y1, px, py, m, force, t0, c10, cc, r0);
    if (part == 2) quadW(out, bx, bw, y0, y1, px, py, m, force, l0, cc, c01, b0);
    if (part == 3) quadW(out, bx, bw, y0, y1, px, py, m, force, cc, r0, b0, c11);
  }
}
static void IRAM_ATTR __attribute__((optimize("O2"))) renderBand(int kb, uint16_t* out, int bx, int bw) {
  const int y0 = kb * BAND, h = min(BAND, S - y0), y1 = y0 + h;
  uint32_t* Gd = gridB[kb % NSLOT];
  const int gx0 = bx & ~(GQ - 1), nx = min(GMAX, (bx + bw - 1 - gx0) / GQ + 2), ny = (h + GQ - 1) / GQ + 1;
  for (int yy = 0; yy < h; ++yy) {                       // 丸の外は黒
    const int y = y0 + yy; uint16_t* o = out + yy * bw - bx;
    const int xa = max(bx, (int)spanA[y]), xb = min(bx + bw - 1, (int)spanB[y]);
    if (xa > xb) { for (int x = bx; x < bx + bw; ++x) o[x] = 0; continue; }
    for (int x = bx; x < xa; ++x) o[x] = 0;
    for (int x = xb + 1; x < bx + bw; ++x) o[x] = 0;
  }
  for (int j = 0; j < ny; ++j) {                         // 粗い格子の点を計算(丸の近くだけ)
    const int gy = y0 + j * GQ;
    int lo = S, hi = -1;
    for (int y = max(0, gy - GQ); y <= min(S - 1, gy + GQ); ++y) if (spanA[y] <= spanB[y]) { lo = min(lo, (int)spanA[y]); hi = max(hi, (int)spanB[y]); }
    for (int i = 0; i < nx; ++i) { const int gx = gx0 + i * GQ; Gd[j * nx + i] = (gx >= lo - GQ && gx <= hi + GQ) ? sampW(gx, gy) : 0; }
  }
  for (int j = 0; j + 1 < ny; ++j) {
    const int qy = y0 + j * GQ;
    for (int i = 0; i + 1 < nx; ++i) {
      const int qx = gx0 + i * GQ;
      bool any = false;                                   // この四角に、丸の中の点があるか
      for (int y = qy; y < min(y1, qy + GQ) && !any; ++y) any = spanA[y] <= qx + GQ - 1 && spanB[y] >= qx && spanA[y] <= spanB[y];
      if (!any) continue;
      const uint32_t c00 = Gd[j * nx + i], c10 = Gd[j * nx + i + 1], c01 = Gd[(j + 1) * nx + i], c11 = Gd[(j + 1) * nx + i + 1];
      const bool flareRow = fFlare > 0.01f && abs(qy + GQ / 2 - (int)sC) <= 8;   // 横一文字の細い光は、色の差にかかわらず細かく
      quadW(out, bx, bw, y0, y1, qx, qy, GQ, flareRow, c00, c10, c01, c11);
    }
  }
}

static int takeBand(int limit) {
  int k = __atomic_load_n(&nextBand, __ATOMIC_ACQUIRE);
  while (k < NB && k <= limit) {
    if (__atomic_compare_exchange_n(&nextBand, &k, k + 1, false, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) {
      if (bandW[k] > 0) renderBand(k, band[k % NSLOT], bandX[k], bandW[k]);
      __atomic_store_n(&bandReady[k], 1, __ATOMIC_RELEASE);
      return k;
    }
  }
  return -1;
}
static void renderFrame() {
  prepShade();
  for (int k = 0; k < NB; ++k) bandReady[k] = 0;
  __atomic_store_n(&doneUpTo, -1, __ATOMIC_RELEASE);
  __atomic_store_n(&nextBand, 0, __ATOMIC_RELEASE);
  uint32_t busy = 0, send = 0;
  xSemaphoreGive(semGo);
  if (!fbMode) M5.Display.startWrite();
  for (int k = 0; k < NB; ++k) {
    while (!__atomic_load_n(&bandReady[k], __ATOMIC_ACQUIRE)) {
      const uint32_t t0 = micros();
      if (takeBand(__atomic_load_n(&doneUpTo, __ATOMIC_ACQUIRE) + NSLOT) >= 0) busy += micros() - t0;
    }
    if (!fbMode) { const uint32_t tw = micros(); M5.Display.waitDMA(); send += micros() - tw; }
    __atomic_store_n(&doneUpTo, k - 1, __ATOMIC_RELEASE);
    if (bandW[k] > 0) {
      const int y0 = k * BAND, h = min(BAND, S - y0);
      if (fbMode) { const uint32_t ts = micros(); M5.Display.pushImage(XOFF + bandX[k], YOFF + y0, bandW[k], h, band[k % NSLOT]); send += micros() - ts; }
      else M5.Display.pushImageDMA(XOFF + bandX[k], YOFF + y0, bandW[k], h, band[k % NSLOT]);
    }
  }
  if (!fbMode) M5.Display.waitDMA();
  __atomic_store_n(&doneUpTo, NB, __ATOMIC_RELEASE);
  if (!fbMode) M5.Display.endWrite();
  xSemaphoreTake(semDone, portMAX_DELAY);
  busyUs[1] = busy; sendUs = send;
}
static void workerTask(void*) {
  for (;;) {
    xSemaphoreTake(semGo, portMAX_DELAY);
    uint32_t busy = 0;
    for (;;) {
      const int k = __atomic_load_n(&nextBand, __ATOMIC_ACQUIRE);
      if (k >= NB) break;
      if (k - NSLOT > __atomic_load_n(&doneUpTo, __ATOMIC_ACQUIRE)) continue;
      const uint32_t t0 = micros();
      if (takeBand(__atomic_load_n(&doneUpTo, __ATOMIC_ACQUIRE) + NSLOT) >= 0) busy += micros() - t0;
    }
    busyUs[0] = busy;
    xSemaphoreGive(semDone);
  }
}

// ---------------------------------------------------------------- 動き
static void setPhase(int p) { Wm.phase = p; Wm.pt = 0; static const char* const N[] = {"wormhole", "flash", "black hole", "dive"}; Serial.printf("phase: %s\n", N[p]); }
static void stepWorm(float dt) {
  const float sp = SPEEDS[Wm.speedIdx];
  Wm.simT += dt * sp; Wm.pt += dt;   // simT は円盤の回転などに使う。速さに合わせて進む
  Wm.boost = fmaxF(0.0f, Wm.boost - dt * 0.35f);
  Wm.swirl *= expf(-dt * 1.5f);
  Wm.tilt += (Wm.tiltT - Wm.tilt) * (1.0f - expf(-dt * 3.0f));
  Wm.tiltT *= expf(-dt * 0.3f);
  const float u = Wm.pt;
  float wa = 0, bh = 0, fl = 0, p = 0, flare = 0;
  switch (Wm.phase) {
    case P_WORM: {
      p = clamp01(u / WORM_SEC);
      // ゆっくり入って少しずつ速く、出口の手前でぐっと加速して光へ
      const float target = (0.45f + 0.55f * p + 1.4f * smooth01((p - 0.82f) / 0.18f)) * sp * (1.0f + 2.0f * Wm.boost);
      Wm.vel += (target - Wm.vel) * (1.0f - expf(-dt * (target > Wm.vel ? 2.0f : 1.2f)));
      Wm.travel += dt * Wm.vel;
      wa = smooth01(u / 0.8f); flare = smooth01((p - 0.35f) / 0.5f);
      if (p >= 1.0f) setPhase(P_FLASH);
      break;
    }
    case P_FLASH: {
      const float k = u / FLASH_SEC;
      p = 1; Wm.travel += dt * Wm.vel;
      wa = 1.0f - smooth01((k - 0.45f) / 0.2f); flare = 1.0f;
      fl = smooth01(k / 0.4f) * (1.0f - smooth01((k - 0.6f) / 0.4f));
      bh = smooth01((k - 0.55f) / 0.45f);
      if (k >= 1.0f) { setPhase(P_BH); Wm.zoom = 1.6f; }
      break;
    }
    case P_BH: {
      bh = 1; Wm.zoom += (1.0f - Wm.zoom) * (1.0f - expf(-dt * 0.5f));   // ゆっくり離れていく
      if (u > BH_SEC) setPhase(P_DIVE);
      break;
    }
    case P_DIVE: {                                        // 暗くなって、また飛びこむ
      const float k = u / 1.2f;
      bh = 1.0f - smooth01(k); Wm.zoom += dt * 1.2f;
      if (k >= 1.0f) { setPhase(P_WORM); Wm.zoom = 1.0f; }
      break;
    }
  }
  fPhase = Wm.phase; fP = p; fWormA = wa; fBh = bh; fFlash = fl; fFlare = flare;
  fExitR = (0.13f + 0.17f * p * p) + (Wm.phase == P_FLASH ? smooth01(u / FLASH_SEC) * 1.2f : 0.0f);
  { const float lx = Wm.tiltX * TILT_LOOK, ly = Wm.tiltY * TILT_LOOK, ro = Wm.tiltX * TILT_ROLL;
    fLx = -lx; fLy = -ly; fRc = cosf(ro); fRs = sinf(ro); fBx = -lx * 0.4f; fBy = -ly * 0.4f; }
  fSkyRot = Wm.simT * 0.004f;
  fVel = Wm.vel; fTrav = Wm.travel; fSwirl = Wm.swirl; fTilt = DISK_ANGLE + Wm.tilt + Wm.tiltX * 0.35f; fZoom = Wm.zoom; fT = Wm.simT;
  const Tone& T = TONES[Wm.tone];
  for (int k = 0; k < 3; ++k) { fVein[k] = T.vein[k]; fCore[k] = T.core[k]; fDisk[k] = T.disk[k]; fFa[k] = T.flareA[k]; fFb[k] = T.flareB[k]; }
}

// ---------------------------------------------------------------- 表示
static void fail(const char* msg) {
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setTextColor(0xF800);
  M5.Display.setTextSize(2);
  M5.Display.setTextDatum(middle_center);
  M5.Display.drawString(msg, W / 2, H / 2);
  Serial.println(msg);
  while (true) nmIdle();
}

// ---------------------------------------------------------------- setup / loop
__attribute__((cold)) void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);
  nmStart();                                         // 明るさ(メニューで決めたもの)にそろえる
  Serial.begin(115200);
  Serial.println("NullWormhole v10");
#if defined(ESP_PLATFORM)
  if (getCpuFrequencyMhz() < 240) setCpuFrequencyMhz(240);
#endif
  if (M5.Display.width() < M5.Display.height()) M5.Display.setRotation(1);
  W = M5.Display.width(); H = M5.Display.height();
  roundScr = (W == H);
  S = min(W, H) & ~1;
  XOFF = ((W - S) / 2) & ~1; YOFF = (H - S) / 2;
  NB = (S + BAND - 1) / BAND;
  M5.Display.fillScreen(TFT_BLACK);
  { auto* pnl = M5.Display.getPanel(); fbMode = pnl && pnl->getAutoDisplay();
#if defined(NL_DIRECT_AMOLED)
    if (fbMode && DIRECT_SEND && M5.getBoard() == m5::board_t::board_M5StopWatch) {
      lgfx::Panel_AMOLED* inner = NlFbPeek::inner(pnl);
      if (inner) { M5.Display.setPanel(inner); fbMode = false; directSend = true; }
    }
#endif
  }
  Serial.printf("display: %s\n", fbMode ? "framebuffer" : directSend ? "direct DMA (StopWatch, no framebuffer)" : "direct DMA");
  spanA = (int16_t*)malloc(S * 2); spanB = (int16_t*)malloc(S * 2);
  bandX = (int16_t*)malloc(NB * 2); bandW = (int16_t*)malloc(NB * 2);
  veinT = (uint8_t*)heap_caps_malloc(NT * NT, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);   // よく引くので、内蔵メモリに置けるなら置く
  if (!veinT) veinT = (uint8_t*)ps_malloc(NT * NT);
  cloudT = (uint8_t*)ps_malloc(NC * NC);
  for (int i = 0; i < NSLOT; ++i) band[i] = (uint16_t*)heap_caps_malloc((size_t)S * BAND * 2, MALLOC_CAP_DMA);
  for (int i = 0; i < NSLOT; ++i) gridB[i] = (uint32_t*)malloc((size_t)GMAX * (BAND / GQ + 2) * 4);   // 粗い格子の色(帯ごと)
  bool ok = spanA && spanB && bandX && bandW && veinT && cloudT;
  for (int i = 0; i < NSLOT; ++i) ok = ok && band[i];
  if (!ok) fail("Memory error");
  const float c = (S - 1) * 0.5f, rr = S * 0.5f + 1.0f;
  for (int y = 0; y < S; ++y) {
    if (!roundScr) { spanA[y] = 0; spanB[y] = S - 1; continue; }
    const float dy = y - c, w = rr * rr - dy * dy;
    if (w <= 0) { spanA[y] = 1; spanB[y] = 0; continue; }
    const float s = sqrtf(w);
    spanA[y] = (int16_t)max(0, (int)ffloorf(c - s)); spanB[y] = (int16_t)min(S - 1, (int)ceilf(c + s));
  }
  for (int b = 0; b < NB; ++b) {
    int x0 = S, x1 = -1;
    for (int y = b * BAND; y < min(S, (b + 1) * BAND); ++y) if (spanA[y] <= spanB[y]) { x0 = min(x0, (int)spanA[y]); x1 = max(x1, (int)spanB[y]); }
    if (x1 < x0) { bandX[b] = 0; bandW[b] = 0; continue; }
    x0 &= ~1; x1 |= 1; if (x1 > S - 1) x1 = S - 1;
    bandX[b] = (int16_t)x0; bandW[b] = (int16_t)((x1 - x0 + 1) & ~1);
  }
  imuOK = TILT && M5.Imu.isEnabled();
  semGo = xSemaphoreCreateBinary();
  semDone = xSemaphoreCreateBinary();
  if (!semGo || !semDone) fail("Memory error (task)");
  xTaskCreatePinnedToCore(workerTask, "nullworm", 6144, nullptr, 1, nullptr, 0);
  const uint32_t t0 = millis();
  buildVeins();
  Serial.printf("build: %u ms\n", (unsigned)(millis() - t0));
  rnd = (uint32_t)esp_random() | 1u;
  memset(&Wm, 0, sizeof(Wm));
  Wm.zoom = 1.0f;
  setPhase(P_WORM);
  stepWorm(0.0f);
}

void loop() {
  static uint32_t last = millis(), fpsT = millis();
  static int frames = 0;
  static bool was = false;
  static int pressX = 0, pressY = 0, lastX = 0, lastY = 0;
  static bool moved = false;
  M5.update();
  auto t = M5.Touch.getDetail();
  const uint32_t now = millis();
  const bool corner = !roundScr && t.x < 56 && t.y < 56;
  if (roundScr) {
    if (M5.BtnA.wasClicked()) { Wm.tone = (Wm.tone + 1) % NTONE; Serial.printf("tone: %s\n", TONES[Wm.tone].name); }
    if (M5.BtnB.wasClicked()) { Wm.speedIdx = (Wm.speedIdx + 1) % NSPEED; Serial.printf("speed: x%.2f\n", SPEEDS[Wm.speedIdx]); }
  } else {
    if (M5.BtnPWR.wasClicked()) { Wm.tone = (Wm.tone + 1) % NTONE; Serial.printf("tone: %s\n", TONES[Wm.tone].name); }
  }
  float dt = (now - last) / 1000.0f; last = now;
  if (dt > 0.1f) dt = 0.1f;
  if (imuOK) {                                    // 傾き(NullCloud と同じ読み方)
    float ax, ay, az;
    M5.Imu.update();
    if (M5.Imu.getAccel(&ax, &ay, &az)) {
      float sx = -ax, sy = ay;
#if defined(ESP_PLATFORM)
      if (TILT_STOPWATCH_FIX && M5.getBoard() == m5::board_t::board_M5StopWatch) { sx = -ay; sy = -ax; }   // 90 度回して読む
#endif
      if (TILT_FLIP_X) sx = -sx;
      if (TILT_FLIP_Y) sy = -sy;
      if (Wm.levelN < 20) { ++Wm.levelN; Wm.levelX += (sx - Wm.levelX) / Wm.levelN; Wm.levelY += (sy - Wm.levelY) / Wm.levelN; }   // はじめの姿勢を「まっすぐ」に
      else if (TILT_RELEVEL > 0 && fabsf(sx - Wm.levelX) < TILT_RELEVEL_MAX && fabsf(sy - Wm.levelY) < TILT_RELEVEL_MAX) {
        const float b = 1.0f - expf(-dt / TILT_RELEVEL); Wm.levelX += (sx - Wm.levelX) * b; Wm.levelY += (sy - Wm.levelY) * b;
      }
      sx = (sx - Wm.levelX) * TILT_GAIN; sy = (sy - Wm.levelY) * TILT_GAIN;
      sx = sx < -1 ? -1 : sx > 1 ? 1 : sx; sy = sy < -1 ? -1 : sy > 1 ? 1 : sy;
      const float a = 1.0f - expf(-dt * 4.0f);
      Wm.tiltX += (sx - Wm.tiltX) * a; Wm.tiltY += (sy - Wm.tiltY) * a;
    }
  }
  const int tx = t.x - XOFF, ty = t.y - YOFF;
  const bool touching = t.isPressed() && !corner && tx >= 0 && ty >= 0 && tx < S && ty < S;
  if (touching && !was) { pressX = lastX = tx; pressY = lastY = ty; moved = false; }
  if (touching && was) {
    if (!moved && ((tx - pressX) * (tx - pressX) + (ty - pressY) * (ty - pressY)) > (int)(S * S * 0.002f)) moved = true;
    if (moved) {
      if (Wm.phase == P_BH) Wm.tiltT = fmaxF(-0.6f, fminF(0.6f, Wm.tiltT + (ty - lastY) * (1.5f / S)));
      else Wm.swirl += (tx - lastX) * (8.0f / S);
    }
    lastX = tx; lastY = ty;
  }
  if (!touching && was && !moved) {
    if (Wm.phase == P_BH) setPhase(P_DIVE);
    else if (Wm.phase == P_WORM) Wm.boost = 1.0f;
  }
  was = touching;
  stepWorm(dt);
  renderFrame();
  if (++frames, now - fpsT >= 3000) {
    static const char* const N[] = {"wormhole", "flash", "black hole", "dive"};
    Serial.printf("fps: %.1f  (%s, %s, x%.2f)  core0 %.1f ms  core1 %.1f ms  send %.1f ms\n", frames * 1000.0f / (now - fpsT), N[Wm.phase], TONES[Wm.tone].name, SPEEDS[Wm.speedIdx],
                  busyUs[0] / 1000.0f, busyUs[1] / 1000.0f, sendUs / 1000.0f);
    frames = 0; fpsT = now;
  }
}

// 共通の置き場に置いたものは、0 で始まる単純な型だけ(コンストラクタが要らないもの)
static_assert(std::is_trivially_default_constructible<WormSt>::value, "NM_ARENA: WormSt");
static constexpr size_t OVL_END = nmAlignTo(nmOv1, 16);
static_assert(OVL_END <= NM_ARENA_SIZE, "NM_ARENA_SIZE (nm_common.h) を大きくしてください");
extern const size_t OVL_SIZE = OVL_END;   // 起動時の確認用
}  // namespace NullWormhole
