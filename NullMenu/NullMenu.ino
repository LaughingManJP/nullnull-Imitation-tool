// NullMenu  -  null² スケッチ集のランチャー
//   M5Stack StopWatch(丸い 466x466 画面)用。CoreS3 系でも動きます。
//
//   起動するとメニューが出ます。好きなものを選んで動かし、
//   ボタンB を長押しするといつでもメニューに戻れます。
//
// メニューの操作:
//   ボタンA        … つぎへ
//   ボタンB        … きめる(スタート)
//   左右になぞる   … えらぶ
//   まん中をタップ … スタート
//
// 動かしているとき:
//   ボタンB 長押し … メニューに戻る
//   それ以外のボタンとタッチは、そのスケッチのものです
//
// 必要なもの:
//   ボードマネージャー M5Stack >= 3.3.7 / ボード: M5StopWatch(CoreS3 なら M5CoreS3)
//   ライブラリ M5Unified >= 0.2.15, M5GFX >= 0.2.21
//   このフォルダの mod_*.h も一緒に置いてください
//
// しくみ:
//   5つのスケッチをそれぞれ名前空間に入れて、選ばれた1つだけが
//   メモリを確保して動きます。切り替えるときに、そのスケッチが確保したものを
//   全部返してから次を始めるので、何度行き来しても増えていきません。

#include <M5Unified.h>
#include <math.h>

#pragma GCC optimize("O3")

// 書き込み時に "iram0_0_seg overflowed" と出たら、ここを 0 のままにしてください。
// 1 にすると描画の内側が高速メモリに載りますが、5本ぶん入らないことがあります。
#define USE_IRAM 0
#if USE_IRAM
  #define MOD_IRAM IRAM_ATTR
#else
  #define MOD_IRAM
#endif

#include "mod_nulleye.h"
#include "mod_nullhorn.h"
#include "mod_nullmirror.h"
#include "mod_nullpavilion.h"
#include "mod_nulllife.h"

// ---------------------------------------------------------------- 一覧
enum Emblem { EM_CUBE, EM_RINGS, EM_SHARDS, EM_GRID, EM_BLOB };
struct Entry {
  const char* name;
  const char* jp;                 // 一行説明
  uint8_t r, g, b;                // アクセント色
  Emblem  emblem;
  bool (*begin)();
  void (*step)();
  void (*stop)();
};
static const Entry ITEMS[] = {
  {"NullEye",      "鏡の立方体と、うねる眼",     255,  96,  64, EM_CUBE,   NullEye::begin,      NullEye::step,      NullEye::stop},
  {"NullHorn",     "赤と青の光のリング",         255,  56,  86, EM_RINGS,  NullHorn::begin,     NullHorn::step,     NullHorn::stop},
  {"NullMirror",   "割れた鏡の部屋",              96, 168, 255, EM_SHARDS, NullMirror::begin,   NullMirror::step,   NullMirror::stop},
  {"NullPavilion", "波打つ鏡のファサード",       120, 230, 190, EM_GRID,   NullPavilion::begin, NullPavilion::step, NullPavilion::stop},
  {"NullLife",     "ぬるっと生きている膜",       200, 130, 255, EM_BLOB,   NullLife::begin,     NullLife::step,     NullLife::stop},
};
static constexpr int NITEM = sizeof(ITEMS) / sizeof(ITEMS[0]);

// ---------------------------------------------------------------- 状態
static int  sel = 0;              // 選んでいる番号
static int  running = -1;         // 動かしているスケッチ(-1 = メニュー)
static bool failed = false;       // メモリが足りなかった
static uint32_t guardUntil = 0;   // 切り替え直後、しばらく入力を無視する
static float menuT = 0;
static int   SW, SH, CX, CY, RR;  // 画面の大きさと中心
static M5Canvas* cv = nullptr;    // メニューを描く紙(PSRAM)

// なぞって選ぶ用
static bool  swipeOn = false;
static int   swipeX0 = 0;
static float slide = 0;           // 切り替えのアニメーション(-1..1)

static inline uint16_t rgb(int r, int g, int b) { return M5.Display.color565(r, g, b); }
static inline uint16_t dim(const Entry& e, float k) {
  return rgb((int)(e.r * k), (int)(e.g * k), (int)(e.b * k));
}

// ---------------------------------------------------------------- 絵柄
static void drawEmblem(M5Canvas& c, const Entry& e, int cx, int cy, int rad, float t) {
  const uint16_t col = rgb(e.r, e.g, e.b);
  switch (e.emblem) {
    case EM_CUBE: {                                  // 角を上にした立方体
      float a = t * 0.5f;
      float pts[6][2];
      for (int i = 0; i < 6; ++i) {
        float th = a + i * 1.04719755f;              // 60度おき
        pts[i][0] = cx + rad * cosf(th);
        pts[i][1] = cy + rad * sinf(th);
      }
      for (int i = 0; i < 6; ++i)
        c.drawLine((int)pts[i][0], (int)pts[i][1],
                   (int)pts[(i + 1) % 6][0], (int)pts[(i + 1) % 6][1], col);
      for (int i = 0; i < 6; i += 2)
        c.drawLine(cx, cy, (int)pts[i][0], (int)pts[i][1], dim(e, 0.55f));
      for (int k = 0; k < 3; ++k)                    // まん中の眼
        c.drawCircle(cx, cy, rad * (0.13f + 0.07f * k) * (1.0f + 0.06f * sinf(t * 2 + k)), col);
      c.fillCircle(cx, cy, rad * 0.06f, col);
      break;
    }
    case EM_RINGS: {                                 // 同心の光の輪
      for (int k = 0; k < 7; ++k) {
        float r = rad * (0.14f + k * 0.135f);
        float w = 0.45f + 0.55f * sinf(t * 1.4f - k * 0.7f);
        c.drawCircle(cx, cy, (int)r, dim(e, 0.25f + 0.75f * w));
      }
      break;
    }
    case EM_SHARDS: {                                // 割れた鏡
      for (int k = 0; k < 7; ++k) {
        float a0 = k * 0.897598f + 0.2f * sinf(t * 0.7f + k);
        float a1 = a0 + 0.72f;
        float r0 = rad * (0.32f + 0.12f * sinf(k * 2.1f));
        float r1 = rad * (0.95f + 0.05f * cosf(t + k));
        c.drawTriangle(cx + r0 * cosf(a0), cy + r0 * sinf(a0),
                       cx + r1 * cosf(a0), cy + r1 * sinf(a0),
                       cx + r1 * cosf(a1), cy + r1 * sinf(a1),
                       dim(e, 0.35f + 0.65f * (0.5f + 0.5f * sinf(t + k * 1.7f))));
      }
      c.drawCircle(cx, cy, rad * 0.30f, col);
      break;
    }
    case EM_GRID: {                                  // 波打つパネル
      const int N = 5;
      int cell = rad * 2 / (N + 1);
      for (int j = 0; j < N; ++j)
        for (int i = 0; i < N; ++i) {
          float w = 0.5f + 0.5f * sinf(t * 1.6f + i * 0.9f + j * 0.6f);
          int s = (int)(cell * (0.45f + 0.35f * w));
          int x = cx - rad + cell * (i + 1), y = cy - rad + cell * (j + 1);
          c.fillRect(x - s / 2, y - s / 2, s, s, dim(e, 0.22f + 0.78f * w));
        }
      break;
    }
    case EM_BLOB: {                                  // ぬるっとした膜
      const int N = 48;
      float px = 0, py = 0;
      for (int i = 0; i <= N; ++i) {
        float th = i * (6.2831853f / N);
        float r = rad * (0.74f + 0.16f * sinf(th * 3 + t * 1.1f) + 0.09f * sinf(th * 5 - t * 1.7f));
        float x = cx + r * cosf(th), y = cy + r * sinf(th);
        if (i) c.fillTriangle(cx, cy, px, py, x, y, dim(e, 0.30f));
        px = x; py = y;
      }
      for (int i = 0; i <= N; ++i) {
        float th = i * (6.2831853f / N);
        float r = rad * (0.74f + 0.16f * sinf(th * 3 + t * 1.1f) + 0.09f * sinf(th * 5 - t * 1.7f));
        float x = cx + r * cosf(th), y = cy + r * sinf(th);
        if (i) c.drawLine(px, py, x, y, col);
        px = x; py = y;
      }
      break;
    }
  }
}

// ---------------------------------------------------------------- メニューを描く
static void drawMenu() {
  if (!cv) return;
  const Entry& e = ITEMS[sel];
  M5Canvas& c = *cv;

  c.fillScreen(rgb(5, 6, 10));
  const float breathe = 0.85f + 0.15f * sinf(menuT * 1.1f);
  for (int k = 9; k >= 1; --k) {                     // うっすら光る背景
    float f = k / 9.0f;
    float w = (1 - f) * (1 - f) * 0.085f * breathe;
    c.fillCircle(CX, CY - RR * 0.22f, (int)(RR * (0.22f + 0.78f * f)),
                 rgb((int)(e.r * w + 5), (int)(e.g * w + 6), (int)(e.b * w + 10)));
  }
  c.drawCircle(CX, CY, RR - 3, dim(e, 0.16f));       // 画面のふちを縁取る
  c.drawCircle(CX, CY, RR - 4, dim(e, 0.10f));

  int emR = RR * 0.40f;
  int emY = CY - RR * 0.32f;
  int sx  = (int)(slide * SW * 0.55f);               // 切り替え時に横へ流れる
  float fade = 1.0f - fabsf(slide);
  if (fade < 0) fade = 0;

  drawEmblem(c, e, CX + sx, emY, emR, menuT);

  c.setTextDatum(middle_center);
  c.setTextColor(rgb((int)(230 * fade + 8), (int)(232 * fade + 8), (int)(240 * fade + 12)));
  c.setFont(&fonts::FreeSansBold18pt7b);
  c.drawString(e.name, CX + sx, CY + RR * 0.30f);

  c.setTextColor(dim(e, 0.85f * fade + 0.02f));
  c.setFont(&fonts::lgfxJapanGothicP_20);
  c.drawString(e.jp, CX + sx, CY + RR * 0.47f);

  for (int i = 0; i < NITEM; ++i) {                  // 何番目かの点
    int x = CX + (i - (NITEM - 1) * 0.5f) * 22;
    int y = CY + RR * 0.64f;
    if (i == sel) c.fillCircle(x, y, 5, rgb(e.r, e.g, e.b));
    else          c.fillCircle(x, y, 3, rgb(58, 62, 74));
  }

  c.setTextColor(rgb(96, 102, 118));
  c.setFont(&fonts::lgfxJapanGothicP_16);
  c.drawString("A: つぎへ    B: きめる", CX, CY + RR * 0.78f);

  if (failed) {
    c.setTextColor(rgb(255, 90, 90));
    c.drawString("メモリが足りません", CX, CY - RR * 0.76f);
  }

  c.pushSprite(0, 0);
}

// ---------------------------------------------------------------- 行き来
static void openMenu() {
  if (running >= 0) { ITEMS[running].stop(); running = -1; }
  M5.Display.setBrightness(190);
  M5.Display.fillScreen(TFT_BLACK);
  if (!cv) {
    cv = new M5Canvas(&M5.Display);
    cv->setPsram(true);
    cv->setColorDepth(16);
    if (!cv->createSprite(SW, SH)) { delete cv; cv = nullptr; }
  }
  slide = 0;
  guardUntil = millis() + 400;
}

static void launch(int i) {
  if (cv) { cv->deleteSprite(); delete cv; cv = nullptr; }   // 紙を返してから始める
  M5.Display.fillScreen(TFT_BLACK);
  failed = !ITEMS[i].begin();
  if (failed) { running = -1; openMenu(); return; }
  running = i;
  guardUntil = millis() + 400;
  Serial.printf("start %s\n", ITEMS[i].name);
}

// ---------------------------------------------------------------- setup / loop
void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);
  Serial.begin(115200);

  if (M5.Display.width() < M5.Display.height()) M5.Display.setRotation(1);
  SW = M5.Display.width(); SH = M5.Display.height();
  CX = SW / 2; CY = SH / 2;
  RR = min(SW, SH) / 2;
  Serial.printf("NullMenu  screen %dx%d\n", SW, SH);

  openMenu();
}

void loop() {
  uint32_t now = millis();

  if (running >= 0) {
    ITEMS[running].step();                    // 中で M5.update() が呼ばれます
    if (now > guardUntil && M5.BtnB.wasHold()) openMenu();
    return;
  }

  // ---- メニュー
  M5.update();
  auto t = M5.Touch.getDetail();
  static uint32_t last = now;
  float dt = (now - last) / 1000.0f;
  last = now;
  if (dt > 0.1f) dt = 0.1f;
  menuT += dt;
  slide *= expf(-dt * 12.0f);                 // 流れをすっと止める

  if (now > guardUntil) {
    if (M5.BtnA.wasClicked()) { sel = (sel + 1) % NITEM; slide = 1.0f; }
    if (M5.BtnB.wasClicked()) { launch(sel); return; }

    if (t.isPressed()) {
      if (!swipeOn) { swipeOn = true; swipeX0 = t.x; }
      else if (t.x - swipeX0 > SW / 6) {      // 右へなぞる → 前へ
        sel = (sel + NITEM - 1) % NITEM; slide = -1.0f; swipeX0 = t.x;
      } else if (swipeX0 - t.x > SW / 6) {    // 左へなぞる → 次へ
        sel = (sel + 1) % NITEM; slide = 1.0f; swipeX0 = t.x;
      }
    } else if (swipeOn) {
      bool tapped = abs(t.x - swipeX0) < SW / 12;
      swipeOn = false;
      int dy = t.y - (CY - RR * 0.32f);
      if (tapped && abs(t.x - CX) < RR * 0.62f && dy > -RR * 0.62f && dy < RR * 0.62f) {
        launch(sel); return;                  // まん中をタップ → スタート
      }
    }
  }

  drawMenu();
  delay(20);
}
