// NullBokeh  -  NullMenu 用モジュール(元のスケッチ NullBokeh_v6 を名前空間に入れたもの)
// 0 で始まる変数(配列も)は、ほかの作品と共通の置き場(NM_ARENA)に置いています。
// 一度に動くのは1つだけ(切り替えるときは再起動)なので、重なっていても問題ありません。
#include "nm_common.h"
#include <algorithm>
#pragma GCC optimize("O3")

namespace NullBokeh {
static constexpr size_t nmOv0 = 0;

// NullBokeh v6  -  焦点の光。夜の街の光が六角形の玉ボケになって浮かび、ピントがゆっくり前後に呼吸する
//   写真の表現(ピントを外した光)から受けたイメージで作った、非公式の習作です。作品の映像・名前・データは使っていません。
//   M5Stack StopWatch 用(丸い画面)。CoreS3 系でも画面いっぱいに動きます(自動で切り替わります)。
//
// 画面の中:
//   ・光が、レンズの絞りの形(六角形)の玉ボケになって浮かぶ。ふちが少し明るい
//   ・ピントがゆっくり前後に呼吸し、手前や奥の光の玉がほどけたり、点に締まったりする。ボケが大きいほど、光はうすく広がる
//   ・下の小さな目盛りが、今のピントの位置
//
// 傾けると(本体の傾きセンサー):
//   ・ピントより手前の光と奥の光が、反対向きにずれる(のぞきこむ角度が変わったように、奥行きが見える)
//   ・傾きの向きが逆のときは、調整用の TILT_FLIP_X / TILT_FLIP_Y / TILT_SWAP を変える。Serial に tilt: の値が出る
//
// 触ると:
//   ・タップ: 触った光にピントが合う(しばらくすると、また呼吸に戻る)
//   ・上下にドラッグ: 手でピントを送る
//
// 操作:
//   StopWatch … ボタンA 短押し: 光の景色(夜の街 / 雪 / 蛍)   ボタンA 長押し: ボケの形(六角 / 丸)   ボタンB: 明るさ
//   CoreS3系  … 画面の上半分をダブルタップ: 景色   下半分をダブルタップ: ボケの形   電源ボタン短押し: 明るさ
//
// 必要なもの:
//   ボードマネージャー M5Stack >= 3.3.7 / ボード: M5StopWatch(PSRAM は OPI)、CoreS3 なら M5CoreS3
//   ライブラリ M5Unified >= 0.2.16, M5GFX >= 0.2.21
//
// しくみ:
//   光ごとに、ピントからの離れ具合でボケの大きさと明るさを決め、画面の8行ずつの帯ごとに振り分けて、
//   行ごとに六角形(または丸)の幅を出して光を足します。ふちの近くだけ明るい色にします。
//   2つのコアで帯を描き、StopWatch では帯ごとに丸の中だけを画面へ送ります。



// ---- 調整用 ----------------------------------------------------------------
static constexpr int   NLIGHT    = 150;     // 光の数
static constexpr float BLUR      = 46.0f;   // ボケの大きさ(466 の画面でのピクセル。ピントからいちばん遠いとき)
static constexpr float BRIGHT    = 1.0f;    // 光の明るさ
static constexpr float PARALLAX  = 60.0f;   // 傾けたとき、光がずれる大きさ(ピクセル)
static constexpr float BREATH    = 0.22f;   // ピントの呼吸の速さ
static constexpr bool  DOUBLE_TAP_UI = true;
// ---------------------------------------------------------------------------
static constexpr int MAXENT = NLIGHT * 20;

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
struct BLight { float x, y, z, b, v, tw; Col c; float vy, wph; };
struct BDraw { int16_t x, y, r; uint8_t point, pad; uint16_t fill, rim; };
struct Scratch { int dummy; };

static const char* SKETCH_NAME = "NullBokeh v6";

static constexpr int BAND    = 8;
static constexpr int NSLOT   = 4;
static constexpr int MAXNB   = 64;

static constexpr size_t nmOa0 = nmAlignTo(nmOv0, alignof(int)), nmOv1 = nmOa0 + sizeof(int); static int& W = *reinterpret_cast<int*>(NM_ARENA + nmOa0);
static constexpr size_t nmOa1 = nmAlignTo(nmOv1, alignof(int)), nmOv2 = nmOa1 + sizeof(int); static int& H = *reinterpret_cast<int*>(NM_ARENA + nmOa1);
static constexpr size_t nmOa2 = nmAlignTo(nmOv2, alignof(int)), nmOv3 = nmOa2 + sizeof(int); static int& S = *reinterpret_cast<int*>(NM_ARENA + nmOa2);
static constexpr size_t nmOa3 = nmAlignTo(nmOv3, alignof(int)), nmOv4 = nmOa3 + sizeof(int); static int& XOFF = *reinterpret_cast<int*>(NM_ARENA + nmOa3);
static constexpr size_t nmOa4 = nmAlignTo(nmOv4, alignof(int)), nmOv5 = nmOa4 + sizeof(int); static int& YOFF = *reinterpret_cast<int*>(NM_ARENA + nmOa4);
static constexpr size_t nmOa5 = nmAlignTo(nmOv5, alignof(int)), nmOv6 = nmOa5 + sizeof(int); static int& NB = *reinterpret_cast<int*>(NM_ARENA + nmOa5);
static constexpr size_t nmOa6 = nmAlignTo(nmOv6, alignof(float)), nmOv7 = nmOa6 + sizeof(float); static float& Sc = *reinterpret_cast<float*>(NM_ARENA + nmOa6);
static constexpr size_t nmOa7 = nmAlignTo(nmOv7, alignof(float)), nmOv8 = nmOa7 + sizeof(float); static float& VR = *reinterpret_cast<float*>(NM_ARENA + nmOa7);  // 中心 / 見えている半径(ピクセル)
static constexpr size_t nmOa8 = nmAlignTo(nmOv8, alignof(float)), nmOv9 = nmOa8 + sizeof(float); static float& vx0 = *reinterpret_cast<float*>(NM_ARENA + nmOa8);
static constexpr size_t nmOa9 = nmAlignTo(nmOv9, alignof(float)), nmOv10 = nmOa9 + sizeof(float); static float& vx1 = *reinterpret_cast<float*>(NM_ARENA + nmOa9);
static constexpr size_t nmOa10 = nmAlignTo(nmOv10, alignof(float)), nmOv11 = nmOa10 + sizeof(float); static float& vy0 = *reinterpret_cast<float*>(NM_ARENA + nmOa10);
static constexpr size_t nmOa11 = nmAlignTo(nmOv11, alignof(float)), nmOv12 = nmOa11 + sizeof(float); static float& vy1 = *reinterpret_cast<float*>(NM_ARENA + nmOa11);  // 見えている範囲(S×S の中の座標)
static constexpr size_t nmOa12 = nmAlignTo(nmOv12, alignof(bool)), nmOv13 = nmOa12 + sizeof(bool); static bool& isRound = *reinterpret_cast<bool*>(NM_ARENA + nmOa12);
static uint8_t*  bandVis;
static uint16_t* slotBuf[NSLOT];
static int16_t*  spanS;
static int16_t*  spanE;
static int16_t*  bandX0;
static int16_t*  bandW;
static constexpr size_t nmOa13 = nmAlignTo(nmOv13, 16), nmOv14 = nmOa13 + sizeof(uint32_t[2]); static uint32_t (&busyUs)[2] = *reinterpret_cast<uint32_t(*)[2]>(NM_ARENA + nmOa13);

static SemaphoreHandle_t semGo, semDone;
static volatile int readyW = -1, doneUpTo = -1;   // コア0が描き終えた帯 / 画面へ送り終えた帯
static constexpr size_t nmOa14 = nmAlignTo(nmOv14, alignof(bool)), nmOv15 = nmOa14 + sizeof(bool); static bool& fbMode = *reinterpret_cast<bool*>(NM_ARENA + nmOa14);  // StopWatch: 画面の手前に PSRAM の置き場がある(帯ごとに丸の中だけ送る)
static int SPLIT = 2;  // コア1は SPLIT 本に1本の帯を描く(残りはコア0)
static constexpr size_t nmOa15 = nmAlignTo(nmOv15, alignof(uint32_t)), nmOv16 = nmOa15 + sizeof(uint32_t); static uint32_t& sendUs = *reinterpret_cast<uint32_t*>(NM_ARENA + nmOa15);

static constexpr size_t nmOa16 = nmAlignTo(nmOv16, alignof(float)), nmOv17 = nmOa16 + sizeof(float); static float& simT = *reinterpret_cast<float*>(NM_ARENA + nmOa16);
static constexpr size_t nmOa17 = nmAlignTo(nmOv17, alignof(bool)), nmOv18 = nmOa17 + sizeof(bool); static bool& imuOK = *reinterpret_cast<bool*>(NM_ARENA + nmOa17);
static constexpr size_t nmOa18 = nmAlignTo(nmOv18, alignof(float)), nmOv19 = nmOa18 + sizeof(float); static float& tiltX = *reinterpret_cast<float*>(NM_ARENA + nmOa18);
static constexpr size_t nmOa19 = nmAlignTo(nmOv19, alignof(float)), nmOv20 = nmOa19 + sizeof(float); static float& tiltY = *reinterpret_cast<float*>(NM_ARENA + nmOa19);  // 傾き(なめらかにしたもの)
static constexpr size_t nmOa20 = nmAlignTo(nmOv20, alignof(float)), nmOv21 = nmOa20 + sizeof(float); static float& levelX = *reinterpret_cast<float*>(NM_ARENA + nmOa20);
static constexpr size_t nmOa21 = nmAlignTo(nmOv21, alignof(float)), nmOv22 = nmOa21 + sizeof(float); static float& levelY = *reinterpret_cast<float*>(NM_ARENA + nmOa21);  // 「水平」の基準(起動したときの姿勢)
static constexpr size_t nmOa22 = nmAlignTo(nmOv22, alignof(int)), nmOv23 = nmOa22 + sizeof(int); static int& levelN = *reinterpret_cast<int*>(NM_ARENA + nmOa22);  // 基準を取った回数
static int brightIdx = 2;
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
static constexpr size_t nmOa23 = nmAlignTo(nmOv23, 16), nmOv24 = nmOa23 + sizeof(uint8_t[64]); static uint8_t (&ringQ)[64] = *reinterpret_cast<uint8_t(*)[64]>(NM_ARENA + nmOa23);

static void fail(const char* msg) {
  M5.Display.setTextDatum(middle_center);
  M5.Display.drawString(msg, M5.Display.width() / 2, M5.Display.height() / 2);
  Serial.println(msg);
  while (true) nmIdle();
}
// 内蔵メモリを優先、足りなければ PSRAM。内蔵メモリは、画面の転送やタスクのために 48KB は残しておく
static void* allocFast(size_t n) {
  if (heap_caps_get_free_size(MALLOC_CAP_INTERNAL) >= n + 48 * 1024) {
    void* p = heap_caps_malloc(n, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (p) return p;
  }
  return ps_malloc(n);
}

// ---------------------------------------------------------------- 光とピント
static constexpr size_t nmOa24 = nmAlignTo(nmOv24, 16), nmOv25 = nmOa24 + sizeof(BLight[NLIGHT]); static BLight (&L)[NLIGHT] = *reinterpret_cast<BLight(*)[NLIGHT]>(NM_ARENA + nmOa24);
static constexpr size_t nmOa25 = nmAlignTo(nmOv25, 16), nmOv26 = nmOa25 + sizeof(BDraw[NLIGHT]); static BDraw (&bd)[NLIGHT] = *reinterpret_cast<BDraw(*)[NLIGHT]>(NM_ARENA + nmOa25);
static constexpr size_t nmOa26 = nmAlignTo(nmOv26, alignof(int)), nmOv27 = nmOa26 + sizeof(int); static int& nBd = *reinterpret_cast<int*>(NM_ARENA + nmOa26);
static int16_t* ent;
static constexpr size_t nmOa27 = nmAlignTo(nmOv27, 16), nmOv28 = nmOa27 + sizeof(int16_t[MAXNB + 1]); static int16_t (&entStart)[MAXNB + 1] = *reinterpret_cast<int16_t(*)[MAXNB + 1]>(NM_ARENA + nmOa27);
static constexpr size_t nmOa28 = nmAlignTo(nmOv28, alignof(int)), nmOv29 = nmOa28 + sizeof(int); static int& scene = *reinterpret_cast<int*>(NM_ARENA + nmOa28);
static constexpr size_t nmOa29 = nmAlignTo(nmOv29, alignof(int)), nmOv30 = nmOa29 + sizeof(int); static int& roundShape = *reinterpret_cast<int*>(NM_ARENA + nmOa29);
static float focus = 0.5f, target = 0.5f;
static constexpr size_t nmOa30 = nmAlignTo(nmOv30, alignof(float)), nmOv31 = nmOa30 + sizeof(float); static float& manual = *reinterpret_cast<float*>(NM_ARENA + nmOa30);
static constexpr size_t nmOa31 = nmAlignTo(nmOv31, alignof(float)), nmOv32 = nmOa31 + sizeof(float); static float& lastY = *reinterpret_cast<float*>(NM_ARENA + nmOa31);
static constexpr size_t nmOa32 = nmAlignTo(nmOv32, alignof(float)), nmOv33 = nmOa32 + sizeof(float); static float& pressY = *reinterpret_cast<float*>(NM_ARENA + nmOa32);
static constexpr size_t nmOa33 = nmAlignTo(nmOv33, alignof(bool)), nmOv34 = nmOa33 + sizeof(bool); static bool& dragging = *reinterpret_cast<bool*>(NM_ARENA + nmOa33);
static constexpr size_t nmOa34 = nmAlignTo(nmOv34, 16), nmOv35 = nmOa34 + sizeof(uint16_t[512]); static uint16_t (&bgRow)[512] = *reinterpret_cast<uint16_t(*)[512]>(NM_ARENA + nmOa34);
static constexpr size_t nmOa35 = nmAlignTo(nmOv35, 16), nmOv36 = nmOa35 + sizeof(uint16_t[8]); static uint16_t (&pointCol)[8] = *reinterpret_cast<uint16_t(*)[8]>(NM_ARENA + nmOa35);  // ピントが合った光の、まん中の白い点(中心からの段)

static bool allocScratch(Scratch&) { return true; }
static bool pieceAlloc() { ent = (int16_t*)allocFast(sizeof(int16_t) * MAXENT); return ent != nullptr; }

static void buildScene() {
  static const Col warm[4] = { C8(255, 190, 110), C8(255, 150, 80), C8(255, 230, 170), C8(255, 120, 120) };
  static const Col cool[3] = { C8(150, 200, 255), C8(190, 230, 255), C8(120, 255, 210) };
  for (int i = 0; i < NLIGHT; ++i) {
    BLight& l = L[i];
    l.x = frand(0, (float)S); l.z = frand(); l.b = frand(0.5f, 1.0f); l.tw = frand(0, 6.2832f); l.wph = frand(0, 6.2832f); l.vy = 0; l.v = 0;
    if (scene == 0) {                           // 夜の街: 暖かい光が多く、いくつかは横に流れる(車)
      l.y = S * (0.25f + 0.6f * frand());
      l.c = frand() < 0.7f ? warm[(int)(frand() * 3.99f)] : cool[(int)(frand() * 2.99f)];
      l.v = frand() < 0.18f ? (frand() < 0.5f ? -1 : 1) * frand(20, 70) : 0;
    } else if (scene == 1) {                    // 雪: 青白い光が、ゆっくり降る
      l.y = frand(0, (float)S); l.c = lerpC(C8(200, 220, 255), C8(255, 255, 255), frand()); l.vy = frand(8, 30); l.b *= 0.8f;
    } else {                                    // 蛍: 黄緑の光が、ゆらゆら漂って明滅する
      l.y = S * (0.3f + 0.6f * frand()); l.c = lerpC(C8(170, 255, 90), C8(230, 255, 120), frand());
    }
  }
}
static void pieceInit() {
  for (int y = 0; y < S && y < 512; ++y) {
    const float v = (float)y / S;
    bgRow[y] = to565(v < 0.6f ? lerpC(C8(4, 4, 10), C8(11, 10, 20), v / 0.6f) : lerpC(C8(11, 10, 20), C8(20, 14, 16), (v - 0.6f) / 0.4f));
  }
  for (int q = 0; q < 8; ++q) { const float f = 1 - q / 7.0f; pointCol[q] = to565(mulC(C8(255, 255, 255), f * f)); }
  buildScene();
}
static void onA() { scene = (scene + 1) % 3; buildScene(); Serial.printf("scene: %d\n", scene); }
static void onAHold() { roundShape ^= 1; Serial.printf("shape: %s\n", roundShape ? "round" : "hexagon"); }
static void statusText(char* o, int n) { snprintf(o, n, "scene %d focus %.2f", scene, focus); }

static void updateFrame(const Input& in) {
  float dt = in.dt > 0.05f ? 0.05f : in.dt;
  if (dt < 0.0005f) dt = 0.0005f;
  simT += dt;
  const float t = simT, k = S / 466.0f;
  if (in.tapped) {
    dragging = false; lastY = pressY = in.ty;
    float best = 1e9f, bz = target;
    for (int i = 0; i < NLIGHT; ++i) { const float d = hypotf(L[i].x - in.tx, L[i].y - in.ty); if (d < best) { best = d; bz = L[i].z; } }
    target = bz; manual = 6;                    // 6 秒は、そのピントのまま
  }
  if (in.touching) {
    if (fabsf(in.ty - pressY) > 4 * k || dragging) { dragging = true; target = fminf(1.0f, fmaxf(0.0f, target + (lastY - in.ty) / S * 1.5f)); manual = 6; }
    lastY = in.ty;
  } else dragging = false;
  manual -= dt;
  if (manual < 0) target = 0.5f + 0.45f * sinf(t * BREATH);     // ピントの呼吸
  focus += (target - focus) * (1 - expf(-dt * 3));
  for (int i = 0; i < NLIGHT; ++i) {
    BLight& l = L[i];
    l.x += l.v * dt * (0.4f + l.z) * k;
    if (scene == 1) { l.y += l.vy * dt * (0.4f + l.z) * k; l.x += sinf(t * 0.7f + l.wph) * 6 * dt * k; if (l.y > S + 60 * k) { l.y = -60 * k; l.x = frand(0, (float)S); } }
    if (scene == 2) { l.x += sinf(t * 0.5f + l.wph) * 14 * dt * k; l.y += cosf(t * 0.37f + l.wph * 1.3f) * 10 * dt * k; }
    if (l.x < -60 * k) l.x = S + 60 * k;
    if (l.x > S + 60 * k) l.x = -60 * k;
  }
  // 描く光(ボケの大きさと、面積に合わせた明るさ)
  nBd = 0;
  for (int i = 0; i < NLIGHT; ++i) {
    const BLight& l = L[i];
    const float blur = fabsf(l.z - focus), r = (2 + blur * BLUR * (0.6f + l.z * 0.6f)) * k;
    float tw = 0.9f + 0.1f * sinf(t * 2 + l.tw);
    if (scene == 2) tw = fmaxf(0.0f, sinf(t * 1.3f + l.tw * 3)) * 0.9f + 0.1f;   // 蛍の明滅
    const float a = fminf(1.0f, l.b * tw * powf(3 * k / r, 1.25f) * 3.2f) * BRIGHT;
    if (a < 0.01f) continue;
    BDraw& d = bd[nBd++];
    d.x = (int16_t)(l.x + in.tiltX * (l.z - focus) * PARALLAX * k); d.y = (int16_t)(l.y + in.tiltY * (l.z - focus) * PARALLAX * k); d.r = (int16_t)fmaxf(1.0f, r); d.point = r < 4 * k ? 1 : 0;
    d.fill = to565(mulC(l.c, a * 0.62f)); d.rim = to565(mulC(l.c, a));
    if (d.point) { d.fill = to565(mulC(l.c, fminf(1.0f, l.b) * 0.8f)); d.r = (int16_t)(6 * k); }
  }
  // 帯ごとに振り分ける
  for (int b = 0; b <= NB; ++b) entStart[b] = 0;
  int total = 0, na = nBd;
  for (int i = 0; i < na; ++i) {
    const int a = max(0, bd[i].y - bd[i].r) / BAND, b = min(S - 1, max(0, bd[i].y + bd[i].r)) / BAND;
    if (total + (b - a + 1) > MAXENT) { na = i; break; }
    for (int q = a; q <= b; ++q) entStart[q + 1]++;
    total += b - a + 1;
  }
  for (int b = 0; b < NB; ++b) entStart[b + 1] += entStart[b];
  int16_t fill[MAXNB];
  for (int b = 0; b < NB; ++b) fill[b] = entStart[b];
  for (int i = 0; i < na; ++i) {
    const int a = max(0, bd[i].y - bd[i].r) / BAND, b = min(S - 1, max(0, bd[i].y + bd[i].r)) / BAND;
    for (int q = a; q <= b; ++q) ent[fill[q]++] = (int16_t)i;
  }
}

static void renderBand(int k, uint16_t* buf, Scratch&) {
  const int y0 = k * BAND, y1 = min(S, y0 + BAND);
  const int bx0 = bandX0[k], bw = bandW[k], bx1 = bx0 + bw - 1;
  const float sq3 = 0.8660254f;
  for (int y = y0; y < y1; ++y) {
    uint16_t* row = buf + (y - y0) * bw - bx0;
    const int xs = max((int)spanS[y], bx0), xe = min((int)spanE[y], bx1);
    for (int x = bx0; x <= bx1; ++x) row[x] = 0;
    if (xs > xe) continue;
    const uint16_t o = outPix(bgRow[y < 512 ? y : 511]);
    for (int x = xs; x <= xe; ++x) row[x] = o;
  }
  for (int e = entStart[k]; e < entStart[k + 1]; ++e) {
    const BDraw& d = bd[ent[e]];
    const int R = d.r;
    if (d.point) {                                // ピントが合った光: 色のにじみ + 白い芯
      const int R1 = (R + 1) * (R + 1);
      for (int y = max(y0, d.y - R); y <= min(y1 - 1, d.y + R); ++y) {
        const int dy = y - d.y, xs = max(max((int)spanS[y], bx0), d.x - R), xe = min(min((int)spanE[y], bx1), d.x + R);
        uint16_t* row = buf + (y - y0) * bw - bx0;
        for (int x = xs; x <= xe; ++x) {
          const int dx = x - d.x, d2 = dx * dx + dy * dy;
          if (d2 >= R1) continue;
          const int q = ringQ[d2 * 64 / R1];
          row[x] = addPix(row[x], add565(mix565(0, d.fill, 16 - q * 2), pointCol[min(7, q * 2)]));
        }
      }
      continue;
    }
    // 玉ボケ: 六角形(平らな辺が上下)または丸。ふちの近くだけ明るい
    const float rf = (float)R, rim = fmaxf(1.5f, rf * 0.08f), hh = roundShape ? rf : rf * sq3;
    for (int y = max(y0, d.y - (int)hh); y <= min(y1 - 1, d.y + (int)hh); ++y) {
      const float dy = fabsf((float)(y - d.y));
      const float hw = roundShape ? sqrtf(fmaxf(0.0f, rf * rf - dy * dy)) : rf - dy * 0.57735f;   // この行の半分の幅
      if (hw <= 0) continue;
      const bool rimRow = roundShape ? false : (hh - dy) < rim;
      const int xs = max(max((int)spanS[y], bx0), (int)(d.x - hw)), xe = min(min((int)spanE[y], bx1), (int)(d.x + hw));
      uint16_t* row = buf + (y - y0) * bw - bx0;
      for (int x = xs; x <= xe; ++x) {
        const float dx = fabsf((float)(x - d.x));
        bool edge = rimRow || (hw - dx) < rim;
        if (roundShape) edge = (rf - sqrtf(dx * dx + dy * dy)) < rim;
        row[x] = addPix(row[x], edge ? d.rim : d.fill);
      }
    }
  }
  // ピントの目盛り(下に小さく)
  const float k466 = S / 466.0f;
  const int gy = (int)(S - 40 * k466), gx0 = (int)(Sc - 60 * k466), gw = (int)(120 * k466), fx = gx0 + (int)(focus * gw);
  for (int y = max(y0, gy - 5); y <= min(y1 - 1, gy + 5); ++y) {
    uint16_t* row = buf + (y - y0) * bw - bx0;
    const int xs = max((int)spanS[y], bx0), xe = min((int)spanE[y], bx1);
    if (y == gy) for (int x = max(xs, gx0); x <= min(xe, gx0 + gw); ++x) row[x] = addPix(row[x], rgb565(60, 60, 60));
    for (int x = max(xs, fx - 1); x <= min(xe, fx); ++x) row[x] = outPix(rgb565(255, 220, 160));
  }
}

static constexpr size_t nmOa36 = nmAlignTo(nmOv36, 16), nmOv37 = nmOa36 + sizeof(Scratch[2]); static Scratch (&scr)[2] = *reinterpret_cast<Scratch(*)[2]>(NM_ARENA + nmOa36);

static void __attribute__((noinline)) MOD_IRAM renderBandIram(int k, uint16_t* b, Scratch& s) { renderBand(k, b, s); }

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


// 共通の置き場に置いたものは、0 で始まる単純な型だけ(コンストラクタが要らないもの)
static_assert(std::is_trivially_default_constructible<BDraw>::value, "NM_ARENA: BDraw");
static_assert(std::is_trivially_default_constructible<BLight>::value, "NM_ARENA: BLight");
static_assert(std::is_trivially_default_constructible<Scratch>::value, "NM_ARENA: Scratch");
static_assert(std::is_trivially_default_constructible<bool>::value, "NM_ARENA: bool");
static_assert(std::is_trivially_default_constructible<float>::value, "NM_ARENA: float");
static_assert(std::is_trivially_default_constructible<int>::value, "NM_ARENA: int");
static_assert(std::is_trivially_default_constructible<int16_t>::value, "NM_ARENA: int16_t");
static_assert(std::is_trivially_default_constructible<uint16_t>::value, "NM_ARENA: uint16_t");
static_assert(std::is_trivially_default_constructible<uint32_t>::value, "NM_ARENA: uint32_t");
static_assert(std::is_trivially_default_constructible<uint8_t>::value, "NM_ARENA: uint8_t");
static constexpr size_t OVL_END = nmAlignTo(nmOv37, 16);
static_assert(OVL_END <= NM_ARENA_SIZE, "NM_ARENA_SIZE (nm_common.h) を大きくしてください");
extern const size_t OVL_SIZE = OVL_END;   // 起動時の確認用
}  // namespace NullBokeh
