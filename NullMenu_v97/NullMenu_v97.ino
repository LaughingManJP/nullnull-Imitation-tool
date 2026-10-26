// NullMenu v97  -  null² スケッチ集のランチャー(29作品入り)
//   M5Stack StopWatch(丸い画面)用。CoreS3 系でも動きます(画面の形を見て自動で切り替わります)。
//
// メニューの操作:
//   ボタンA        … つぎへ(長押しで ひとつ前へ)
//   ボタンB        … きめる(スタート)
//   (上に電池の残り。作品の最中は、残りが少ないときだけ画面の下に赤い電池の絵が出ます)
//   左右になぞる   … えらぶ(1回なぞると1つ。指を離したときに決まる)
//   まん中の絵をタップ … スタート(0.2秒後。なぞったすぐあとや、絵の外をさわったとき、速くはらったときは始まらない)
//
// 動かしているとき:
//   StopWatch … ボタンB を1.5秒 長押し → メニューに戻る
//   CoreS3 系 … 画面の左上のすみを1.5秒 長押し → メニューに戻る
//   それ以外のボタンとタッチは、その作品のものです(作品ごとの操作は mod_*.cpp の先頭に書いてあります)
//
// 必要なもの:
//   ボードマネージャー M5Stack >= 3.3.7 / ボード: M5StopWatch(CoreS3 なら M5CoreS3)
//     StopWatch は「PSRAM: OPI PSRAM」「Flash Size: 16MB」「Partition Scheme: 16M Flash (3MB APP/9.9MB FATFS)」
//   ライブラリ M5Unified >= 0.2.16(NullGoodbye のマイクに必要), M5GFX >= 0.2.21
//   このフォルダの nm_common.h と mod_*.cpp(29個)も一緒に置いてください(Arduino IDE で開くと、タブとして並びます)
//
// しくみ:
//   29本をそれぞれ名前空間に入れて(mod_*.cpp)、1つのプログラムにまとめています。
//   ライブラリ(M5Unified / M5GFX など)は1回ぶんしか入らないので、29本でも約3MBで済みます。
//   選んだ作品の番号を、再起動しても消えないメモリ(RTC)に書いてから再起動し、
//   起動したらその作品の setup() / loop() だけを動かします。
//   各作品の大きな配列は、共通の置き場 NM_ARENA を重ねて使います(一度に動くのは1つだけなので)。
//   作品の途中で万一止まって再起動したときは、メニューに戻ります。

#include <M5Unified.h>
#include <esp_system.h>
#include <esp_attr.h>
#include <esp_heap_caps_init.h>
#include "nm_common.h"
static const char* const MENU_VER = "NullMenu v97";   // メニューの上と Serial に出す版の名前

// メニューへ戻る長押しの時間
static constexpr uint32_t NM_HOLD_MS = 1500;

// Arduino IDE は関数の宣言を自動でファイルの先頭近くに足すので、型の名前を先に宣言しておく
struct Entry;

// 作品どうしで共通の置き場(大きさは nm_common.h)
alignas(16) uint8_t NM_ARENA[NM_ARENA_SIZE];

// 置き場のうち、今回の作品が使わない残りを、ふつうのメモリ(ヒープ)として返す。
// こうすると、小さな作品は単体で書き込んだときとほぼ同じだけ、速い内蔵メモリを使えます。
static size_t nmGiveBack(size_t used) {
  uintptr_t a = ((uintptr_t)NM_ARENA + used + 15) & ~(uintptr_t)15;
  uintptr_t b = ((uintptr_t)NM_ARENA + NM_ARENA_SIZE) & ~(uintptr_t)15;
  if (b < a + 2048) return 0;
  return heap_caps_add_region((intptr_t)a, (intptr_t)b) == ESP_OK ? (size_t)(b - a) : 0;
}

// ---- 再起動しても消えないメモリ ---------------------------------------------
static constexpr uint32_t NM_MAGIC = 0x4E554C32;   // "NUL2"
static constexpr uint32_t NM_NONE  = 0xFFFFFFFF;
RTC_NOINIT_ATTR static uint32_t nmMagic;
RTC_NOINIT_ATTR static uint32_t nmRun;     // 再起動したら動かす作品(NM_NONE = メニュー)
RTC_NOINIT_ATTR static uint32_t nmSel;     // メニューで選んでいた作品

// ---- メニューへ戻る ----------------------------------------------------------
static bool     nmHolding = false;
static uint32_t nmHoldT0 = 0;
static bool nmWantsMenu() {
  if (M5.Display.width() == M5.Display.height())      // StopWatch: ボタンB 長押し
    return M5.BtnB.pressedFor(NM_HOLD_MS);
  auto t = M5.Touch.getDetail();                       // CoreS3 系: 左上のすみを長押し
  if (!(t.isPressed() && t.x < 56 && t.y < 56)) { nmHolding = false; return false; }
  if (!nmHolding) { nmHolding = true; nmHoldT0 = millis(); }
  return millis() - nmHoldT0 >= NM_HOLD_MS;
}
static void nmRestart() {
  M5.Display.setBrightness(0);
  M5.Display.fillScreen(TFT_BLACK);
  delay(30);
  esp_restart();
}
static void nmBackToMenu() {
  nmRun = NM_NONE;
  Serial.println("-> menu");
  nmRestart();
}
// 作品の中で「メモリが足りません」と出て止まったとき用
void nmIdle() {
  M5.update();
  if (nmWantsMenu()) nmBackToMenu();
  delay(20);
}

// ---------------------------------------------------------------- 電池
static constexpr int BAT_LOW = 15;               // 作品の最中は、残りがこれ(%)以下になったら、画面の下に赤い電池の絵を出す
static int      batLevel = -1;                   // 電池の残り(%)。読めないときは -1
static bool     batCharging = false;
static uint32_t batReadMs = 0;
static void batUpdate(uint32_t everyMs) {        // 電池を読む(読むのは everyMs ごとに1回)
  const uint32_t now = millis();
  if (batReadMs != 0 && now - batReadMs < everyMs) return;
  batReadMs = now | 1;
  const int lv = (int)M5.Power.getBatteryLevel();
  batLevel = (lv >= 0 && lv <= 100) ? lv : -1;
  batCharging = (int)M5.Power.isCharging() == 1;
}
static inline uint16_t batRgb(int r, int g, int b) { return M5.Display.color565(r, g, b); }
static uint16_t batColor(int lv) {
  return lv > 50 ? batRgb(96, 205, 120) : lv > 20 ? batRgb(232, 192, 80) : batRgb(235, 70, 58);
}
// 電池の絵(幅 24、高さ 12)。線の太さ 2px で、x と幅はいつも偶数にしてある
// (StopWatch の画面へ直接描くときは、x と幅が偶数でないと描かれないため)
static void drawBatIcon(lgfx::LGFXBase& g, int x, int y, int lv, uint16_t line, uint16_t fill) {   // 画面にも紙(Canvas)にも描ける
  x &= ~1;
  g.fillRect(x, y, 22, 2, line);       g.fillRect(x, y + 10, 22, 2, line);
  g.fillRect(x, y, 2, 12, line);       g.fillRect(x + 20, y, 2, 12, line);
  g.fillRect(x + 22, y + 4, 2, 4, line);                       // 頭
  g.fillRect(x + 2, y + 2, 18, 8, 0);                          // 中は黒
  int w = (14 * (lv < 0 ? 0 : lv > 100 ? 100 : lv) / 100) & ~1;
  if (lv > 0 && w < 2) w = 2;
  if (w > 0) g.fillRect(x + 4, y + 4, w, 4, fill);
}
// 作品の最中: 残りが少ないときだけ、画面の下に小さな赤い電池の絵(1フレームごとに上から描き直す)
static void batOverlay() {
  batUpdate(10000);
  if (batLevel < 0 || batLevel > BAT_LOW || batCharging) return;
  const int W = M5.Display.width(), H = M5.Display.height();
  const int x = (W == H) ? W / 2 - 12 : W - 34;               // 丸い画面: 下のまん中 / 四角い画面: 右下
  const int y = (W == H) ? H - 28 : H - 18;
  drawBatIcon(M5.Display, x, y, batLevel, batRgb(235, 70, 58), batRgb(235, 70, 58));
}

// ---------------------------------------------------------------- 一覧
enum Emblem {
  EM_GRID, EM_CUBE, EM_RINGS, EM_SHARDS, EM_BLOB, EM_CURL, EM_SPOTS, EM_SWIRL, EM_HOLE, EM_HALO, EM_SUTRA,
  EM_BUTTERFLY, EM_FLOAT, EM_POND, EM_PLASMA, EM_KINTSUGI, EM_INFINITY, EM_TWIN, EM_DROP, EM_TILES, EM_ZEN, EM_BOKEH, EM_GIF, EM_SLIDE, EM_CHRON, EM_KALEIDO, EM_GALAXY, EM_CLOUD, EM_SAPIENS, EM_SUNSET
};
struct Entry {
  const char* name;
  const char* jp;                 // 一行説明
  uint8_t r, g, b;                // アクセント色
  Emblem  emblem;
  void (*setup)();
  void (*loop)();
  const size_t* ovl;              // 共通の置き場をどれだけ使うか
};
static const Entry ITEMS[] = {
  {"NullPavilion",    "空を映して波打つ鏡",       120, 230, 190, EM_GRID,      NullPavilion::setup,    NullPavilion::loop, &NullPavilion::OVL_SIZE},
  {"NullEye",         "鏡の立方体と、うねる眼",   255,  96,  64, EM_CUBE,      NullEye::setup,         NullEye::loop, &NullEye::OVL_SIZE},
  {"NullHorn",        "赤と青の光のリング",       255,  56,  86, EM_RINGS,     NullHorn::setup,        NullHorn::loop, &NullHorn::OVL_SIZE},
  {"NullMirror",      "割れた鏡の部屋",            96, 168, 255, EM_SHARDS,    NullMirror::setup,      NullMirror::loop, &NullMirror::OVL_SIZE},
  {"NullLife Nuru",   "ぬるっと生きている膜",     200, 130, 255, EM_BLOB,      NullLife_Nuru::setup,   NullLife_Nuru::loop, &NullLife_Nuru::OVL_SIZE},
  {"NullLife Curl",   "見えない渦が流れる鏡面",   110, 200, 255, EM_CURL,      NullLife_Curl::setup,   NullLife_Curl::loop, &NullLife_Curl::OVL_SIZE},
  {"NullVortex",      "光の膜に空く渦",           255, 150, 230, EM_SWIRL,     NullVortex::setup,      NullVortex::loop, &NullVortex::OVL_SIZE},
  {"NullGoodbye",     "音で渦巻く液体と黒い穴",   255, 170,  60, EM_HOLE,      NullGoodbye::setup,     NullGoodbye::loop, &NullGoodbye::OVL_SIZE},
  {"NullBodhisattva", "null菩薩",                 255, 205, 120, EM_HALO,      NullBodhisattva::setup, NullBodhisattva::loop, &NullBodhisattva::OVL_SIZE},
  {"NullSutra",       "炎に照らされる経文の柱",   255, 120,  50, EM_SUTRA,     NullSutra::setup,       NullSutra::loop, &NullSutra::OVL_SIZE},
  {"NullButterfly",   "暗い空に舞う蝶",           140, 210, 255, EM_BUTTERFLY, NullButterfly::setup,   NullButterfly::loop, &NullButterfly::OVL_SIZE},
  {"NullFloat",       "音の節に浮かぶ光の粒",     170, 220, 255, EM_FLOAT,     NullFloat::setup,       NullFloat::loop, &NullFloat::OVL_SIZE},
  {"NullPond",        "計算機自然の池",            90, 200, 210, EM_POND,      NullPond::setup,        NullPond::loop, &NullPond::OVL_SIZE},
  {"NullPlasma",      "空中に描く光の点の立体",   120, 180, 255, EM_PLASMA,    NullPlasma::setup,      NullPlasma::loop, &NullPlasma::OVL_SIZE},
  {"NullKintsugi",    "ヒビに金が流れる器",       235, 185,  80, EM_KINTSUGI,  NullKintsugi::setup,    NullKintsugi::loop, &NullKintsugi::OVL_SIZE},
  {"NullInfinity",    "どこまでも続く合わせ鏡",   180, 160, 255, EM_INFINITY,  NullInfinity::setup,    NullInfinity::loop, &NullInfinity::OVL_SIZE},
  {"NullTwin",        "光の立方体でできた分身",    90, 230, 255, EM_TWIN,      NullTwin::setup,        NullTwin::loop, &NullTwin::OVL_SIZE},
  {"NullDrop",        "くっついて離れる鏡の雫",   200, 215, 235, EM_DROP,      NullDrop::setup,        NullDrop::loop, &NullDrop::OVL_SIZE},
  {"NullTiles",       "風にうねる鏡のタイル",     130, 190, 255, EM_TILES,     NullTiles::setup,       NullTiles::loop, &NullTiles::OVL_SIZE},
  {"NullZen",         "計算機の枯山水",           220, 215, 200, EM_ZEN,       NullZen::setup,         NullZen::loop, &NullZen::OVL_SIZE},
  {"NullBokeh",       "夜の光の玉ボケ",           255, 140, 180, EM_BOKEH,     NullBokeh::setup,       NullBokeh::loop, &NullBokeh::OVL_SIZE},
  {"NullChronicle",   "人類史と AI 史の年表",       150, 185, 255, EM_CHRON,     NullChronicle::setup,   NullChronicle::loop, &NullChronicle::OVL_SIZE},
  {"NullKaleido",     "ぬるぬる渦巻く光",         255, 140,  70, EM_KALEIDO,   NullKaleido::setup,     NullKaleido::loop, &NullKaleido::OVL_SIZE},
  {"NullGalaxy",      "立体の銀河と馬頭星雲",     235, 215, 180, EM_GALAXY,    NullGalaxy::setup,      NullGalaxy::loop, &NullGalaxy::OVL_SIZE},
  {"NullCloud",       "雲海に浮かぶ虹の光輪",     190, 215, 255, EM_CLOUD,     NullCloud::setup,       NullCloud::loop, &NullCloud::OVL_SIZE},
  {"NullSapiens",     "さようなら、ホモ・サピエンス", 255, 175, 120, EM_SAPIENS, NullSapiens::setup,     NullSapiens::loop, &NullSapiens::OVL_SIZE},
  {"NullSunset",      "夕凪の海と、沈む日",       255, 165, 110, EM_SUNSET,    NullSunset::setup,      NullSunset::loop, &NullSunset::OVL_SIZE},
  {"NullGif",         "GIF アニメの再生",          120, 225, 170, EM_GIF,       NullGif::setup,         NullGif::loop, &NullGif::OVL_SIZE},
  {"NullSlide",       "写真のスライドショー",     255, 150, 190, EM_SLIDE,     NullSlide::setup,       NullSlide::loop, &NullSlide::OVL_SIZE},
};
static constexpr int NITEM = sizeof(ITEMS) / sizeof(ITEMS[0]);

// ---------------------------------------------------------------- 状態
static int  sel = 0;              // 選んでいる番号
static int  running = -1;         // 動かしている作品(-1 = メニュー)
static bool waitRelease = true;   // 戻ってきた直後は、ボタンと指が離れるまで待つ
static uint32_t guardUntil = 0;   // 切り替え直後、しばらく入力を無視する
static float menuT = 0;
static int   SW, SH, CX, CY, RR;  // 画面の大きさと中心
static bool  roundScr = true;
static M5Canvas* cv = nullptr;    // メニューを描く紙(PSRAM)

// なぞって選ぶ用
static bool  swipeOn = false;
static int   swipeX0 = 0;
static int   touchX0 = 0, touchY0 = 0;        // 指を置いた所
static int   touchMove = 0;                   // 指を置いてから動いたいちばん大きな距離
static bool  swiped = false;                  // なぞって1つ以上切り替えたか
static uint32_t touchT0 = 0;                  // 指を置いた時刻
static uint32_t swipeEndMs = 0;               // 最後になぞり終えた時刻(すぐあとの軽いタッチは、タップにしない)
static float slide = 0;           // 切り替えのアニメーション(-1..1)
static bool  loadingNow = false;  // 作品を選んで、再起動する直前(「読み込み中…」を出す)
static uint32_t tSeen = 0;                    // 最後に指を読めた時刻
static int   tx = 0, ty = 0;                  // 最後に読めた指の位置
static bool  chipSwipe = false;               // タッチの部品が「はらった」と言ったか(この1回のさわりのあいだに)
static uint32_t launchAt = 0;                 // タップしたあと、作品を始める時刻(0 = なし)

// 指を1回読んだら、ここに知らせる(メニューの絵を描くあいだにも呼ぶ)
static void noteTouch(bool pressed, int x, int y, uint32_t ms) {
  if (!pressed) return;
  tSeen = ms; tx = x; ty = y;
  if (!swipeOn) {                             // さわり始め
    swipeOn = true; swipeX0 = x;
    touchX0 = x; touchY0 = y; touchMove = 0; swiped = false; touchT0 = ms; chipSwipe = false;
    launchAt = 0;                             // タップのあと、始まる前にまた触れたら取り消す
  } else {
    touchMove = max(touchMove, max(abs(x - touchX0), abs(y - touchY0)));
  }
}
// タッチの部品の「はらった」判定を読む(StopWatch の CST820。0x01 = 動きの番号: 1..4 = 上下左右にはらった)
static void readChipGesture() {
#if defined(ESP_PLATFORM)
  if (!roundScr || M5.getBoard() != m5::board_t::board_M5StopWatch) return;
  const uint8_t g = M5.In_I2C.readRegister8(0x15, 0x01, 400000);
  if (g >= 1 && g <= 4) chipSwipe = true;
#endif
}
// メニューの絵を描くあいだにも、指を読む(速くはらったときに、動きを取りこぼさないように)
static constexpr uint32_t FLICK_MS = 300;     // 触り始めから、この時間だけ指をこまめに読む(速くはらったかどうかを見る間)
static void sampleTouch() {
#if defined(ESP_PLATFORM)
  if (running >= 0 || waitRelease || millis() <= guardUntil) return;
  const uint32_t ms = millis();
  if (swipeOn && ms - touchT0 > FLICK_MS) return;   // なぞっているあいだは、1コマ1回(読み足すと、描くのが待たされてカクカクする)
  M5.Touch.update(ms);
  const auto d = M5.Touch.getDetail();
  if (d.isPressed()) { noteTouch(true, d.x, d.y, ms); readChipGesture(); }
#endif
}

static inline uint16_t rgb(int r, int g, int b) {
  r = r < 0 ? 0 : (r > 255 ? 255 : r); g = g < 0 ? 0 : (g > 255 ? 255 : g); b = b < 0 ? 0 : (b > 255 ? 255 : b);
  return M5.Display.color565(r, g, b);
}
static inline uint16_t dim(const Entry& e, float k) {
  return rgb((int)(e.r * k), (int)(e.g * k), (int)(e.b * k));
}
static inline float hash01(int i) {               // 0..1 の決まった乱数
  uint32_t h = (uint32_t)i * 2654435761u; h ^= h >> 15; h *= 2246822519u; h ^= h >> 13;
  return (h & 0xFFFF) / 65535.0f;
}

// ---------------------------------------------------------------- 絵柄
static void drawEmblem(M5Canvas& c, const Entry& e, int cx, int cy, int rad, float t) {
  const uint16_t col = rgb(e.r, e.g, e.b);
  switch (e.emblem) {
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
    case EM_CUBE: {                                  // 角を上にした立方体と眼
      float a = t * 0.5f;
      float pts[6][2];
      for (int i = 0; i < 6; ++i) {
        float th = a + i * 1.04719755f;
        pts[i][0] = cx + rad * cosf(th);
        pts[i][1] = cy + rad * sinf(th);
      }
      for (int i = 0; i < 6; ++i)
        c.drawLine((int)pts[i][0], (int)pts[i][1], (int)pts[(i + 1) % 6][0], (int)pts[(i + 1) % 6][1], col);
      for (int i = 0; i < 6; i += 2)
        c.drawLine(cx, cy, (int)pts[i][0], (int)pts[i][1], dim(e, 0.55f));
      for (int k = 0; k < 3; ++k)
        c.drawCircle(cx, cy, rad * (0.13f + 0.07f * k) * (1.0f + 0.06f * sinf(t * 2 + k)), col);
      c.fillCircle(cx, cy, rad * 0.06f, col);
      break;
    }
    case EM_RINGS: {                                 // 同心の光の輪
      for (int k = 0; k < 7; ++k) {
        float r = rad * (0.14f + k * 0.135f);
        float w = 0.45f + 0.55f * sinf(t * 1.4f - k * 0.7f);
        uint16_t cc = (k & 1) ? rgb((int)(70 * w), (int)(110 * w), (int)(255 * w)) : dim(e, 0.25f + 0.75f * w);
        c.drawCircle(cx, cy, (int)r, cc);
        c.drawCircle(cx, cy, (int)r + 1, cc);
      }
      break;
    }
    case EM_SHARDS: {                                // 割れた鏡
      for (int k = 0; k < 7; ++k) {
        float a0 = k * 0.897598f + 0.2f * sinf(t * 0.7f + k);
        float a1 = a0 + 0.72f;
        float r0 = rad * (0.32f + 0.12f * sinf(k * 2.1f));
        float r1 = rad * (0.95f + 0.05f * cosf(t + k));
        c.drawTriangle(cx + r0 * cosf(a0), cy + r0 * sinf(a0), cx + r1 * cosf(a0), cy + r1 * sinf(a0),
                       cx + r1 * cosf(a1), cy + r1 * sinf(a1),
                       dim(e, 0.35f + 0.65f * (0.5f + 0.5f * sinf(t + k * 1.7f))));
      }
      c.drawCircle(cx, cy, rad * 0.30f, col);
      break;
    }
    case EM_BLOB: {                                  // ぬるっとした膜
      const int N = 48;
      float px = 0, py = 0;
      for (int pass = 0; pass < 2; ++pass)
        for (int i = 0; i <= N; ++i) {
          float th = i * (6.2831853f / N);
          float r = rad * (0.74f + 0.16f * sinf(th * 3 + t * 1.1f) + 0.09f * sinf(th * 5 - t * 1.7f));
          float x = cx + r * cosf(th), y = cy + r * sinf(th);
          if (i) {
            if (pass == 0) c.fillTriangle(cx, cy, px, py, x, y, dim(e, 0.30f));
            else           c.drawLine(px, py, x, y, col);
          }
          px = x; py = y;
        }
      break;
    }
    case EM_CURL: {                                  // 渦を巻いて流れる線
      for (int k = -4; k <= 4; ++k) {
        float y0 = k * 0.21f;
        float px = 0, py = 0; bool has = false;
        for (int i = 0; i <= 40; ++i) {
          float u = -1.0f + i * 0.05f;
          float v = y0 + 0.10f * sinf(u * 4.0f + t * 1.3f + k) + 0.06f * sinf(u * 9.0f - t * 0.8f);
          bool in = (u * u + v * v) < 0.92f;
          float x = cx + u * rad, y = cy + v * rad;
          if (in && has) c.drawLine(px, py, x, y, dim(e, 0.35f + 0.65f * (0.5f + 0.5f * sinf(u * 3 + t * 2 + k))));
          px = x; py = y; has = in;
        }
      }
      break;
    }
    case EM_SPOTS: {                                 // 反応拡散の斑点
      for (int i = 0; i < 46; ++i) {
        float a = hash01(i) * 6.2831853f, r = sqrtf(hash01(i + 100)) * 0.88f;
        float s = 0.5f + 0.5f * sinf(t * 1.2f + i * 1.3f);
        int rr = (int)(rad * (0.035f + 0.07f * s * hash01(i + 300)));
        c.fillCircle(cx + rad * r * cosf(a), cy + rad * r * sinf(a), rr + 1, dim(e, 0.30f + 0.70f * s));
      }
      c.drawCircle(cx, cy, rad, dim(e, 0.25f));
      break;
    }
    case EM_SWIRL: {                                 // 渦
      for (int arm = 0; arm < 4; ++arm) {
        float px = cx, py = cy;
        for (int i = 1; i <= 30; ++i) {
          float r = i / 30.0f;
          float th = arm * 1.5707963f + r * 4.2f - t * 1.2f;
          float x = cx + rad * r * cosf(th), y = cy + rad * r * sinf(th);
          c.drawLine(px, py, x, y, dim(e, 0.25f + 0.75f * r));
          px = x; py = y;
        }
      }
      c.fillCircle(cx, cy, rad * 0.10f, rgb(0, 0, 0));
      c.drawCircle(cx, cy, rad * 0.11f, col);
      break;
    }
    case EM_HOLE: {                                  // 色の帯と、黒い穴
      static const uint8_t RB[6][3] = {{255, 60, 40}, {255, 150, 30}, {255, 230, 60}, {80, 220, 90}, {60, 210, 255}, {70, 90, 255}};
      for (int k = 0; k < 12; ++k) {
        float r = rad * (0.36f + k * 0.055f);
        const uint8_t* q = RB[(k + (int)(t * 2.0f)) % 6];
        int px = 0, py = 0;
        for (int i = 0; i <= 36; ++i) {
          float th = i * (6.2831853f / 36);
          float rr = r * (1.0f + 0.05f * sinf(th * 3 + t * 1.5f + k * 0.5f));
          int x = cx + rr * cosf(th + t * 0.3f), y = cy + rr * sinf(th + t * 0.3f);
          if (i) c.drawLine(px, py, x, y, rgb(q[0], q[1], q[2]));
          px = x; py = y;
        }
      }
      c.fillCircle(cx, cy, rad * 0.33f, rgb(0, 0, 0));
      c.drawCircle(cx, cy, rad * 0.34f, rgb(255, 240, 220));
      break;
    }
    case EM_HALO: {                                  // 光背の前に座る姿
      for (int k = 0; k < 3; ++k) c.drawCircle(cx, cy - rad * 0.30f, rad * (0.42f + k * 0.10f), dim(e, 0.30f + 0.2f * k));
      c.fillCircle(cx, cy - rad * 0.30f, rad * 0.16f, dim(e, 0.85f));                     // 頭
      c.fillTriangle(cx, cy - rad * 0.16f, cx - rad * 0.48f, cy + rad * 0.62f, cx + rad * 0.48f, cy + rad * 0.62f, dim(e, 0.55f));
      c.fillRect(cx - rad * 0.62f, cy + rad * 0.56f, rad * 1.24f, rad * 0.12f, dim(e, 0.70f)); // 台座
      for (int i = 0; i < 12; ++i) {                // まわりを回る光
        float a = i * 0.5235988f + t * 0.4f;
        float r = rad * (0.80f + 0.08f * sinf(t * 2 + i));
        c.fillCircle(cx + r * cosf(a), cy - rad * 0.2f + r * 0.8f * sinf(a), 2, col);
      }
      break;
    }
    case EM_SUTRA: {                                 // 文字の柱
      for (int k = 0; k < 5; ++k) {
        int x = cx + (k - 2) * rad * 0.36f;
        for (int j = 0; j < 12; ++j) {
          float yy = fmodf(j * 0.18f + t * 0.12f * (k & 1 ? 1 : -1) + 10.0f, 2.16f) - 1.08f;
          int y = cy + yy * rad;
          float lim = 1.0f - yy * yy; if (lim < 0.08f) continue;
          uint16_t cc = dim(e, 0.25f + 0.75f * lim);
          int h = (int)(rad * 0.06f), w = (int)(rad * 0.10f);
          int s = (int)(hash01(k * 31 + j) * 4);
          c.drawLine(x - w, y, x + w, y, cc);
          if (s & 1) c.drawLine(x, y - h, x, y + h, cc);
          if (s & 2) c.drawLine(x - w, y + h, x + w / 2, y - h, cc);
        }
      }
      break;
    }
    case EM_BUTTERFLY: {                             // ひらひら舞う蝶
      for (int k = 0; k < 4; ++k) {
        float a = t * 0.35f + k * 1.5707963f;
        int x = cx + rad * 0.52f * cosf(a), y = cy + rad * 0.40f * sinf(a) + rad * 0.08f * sinf(t * 1.7f + k);
        float f = fabsf(sinf(t * 5.0f + k * 1.1f));  // はばたき
        int s = (int)(rad * (0.20f + 0.06f * (k & 1)));
        int w = (int)(s * (0.25f + 0.75f * f));
        uint16_t cc = dim(e, 0.5f + 0.5f * f);
        c.fillTriangle(x, y, x - w, y - s, x - w, y + s / 3, cc);
        c.fillTriangle(x, y, x + w, y - s, x + w, y + s / 3, cc);
        c.fillTriangle(x, y, x - w * 2 / 3, y + s * 2 / 3, x - w / 4, y + s / 4, dim(e, 0.4f));
        c.fillTriangle(x, y, x + w * 2 / 3, y + s * 2 / 3, x + w / 4, y + s / 4, dim(e, 0.4f));
      }
      break;
    }
    case EM_FLOAT: {                                 // 音の節に浮く光の粒
      for (int j = -3; j <= 3; ++j) {
        float y0 = j * 0.26f;
        int px = 0, py = 0;
        for (int i = 0; i <= 30; ++i) {
          float u = -1 + i / 15.0f;
          if (u * u + y0 * y0 > 0.9f) { px = 0; continue; }
          int x = cx + u * rad, y = cy + (y0 + 0.04f * sinf(u * 6 + t * 2)) * rad;
          if (px) c.drawLine(px, py, x, y, dim(e, 0.18f));
          px = x; py = y;
        }
        for (int i = -3; i <= 3; ++i) {
          float u = i * 0.26f;
          if (u * u + y0 * y0 > 0.8f) continue;
          float b = 0.5f + 0.5f * sinf(t * 1.7f + i * 0.9f + j * 1.3f);
          int x = cx + u * rad, y = cy + (y0 + 0.03f * sinf(t * 2.2f + i)) * rad;
          c.fillCircle(x, y, (int)(rad * 0.03f + 2 * b), dim(e, 0.4f + 0.6f * b));
        }
      }
      break;
    }
    case EM_POND: {                                  // 波紋と、デジタルになる水面
      for (int k = 0; k < 4; ++k) {
        float f = fmodf(t * 0.25f + k * 0.25f, 1.0f);
        c.drawCircle(cx, cy, (int)(rad * (0.1f + 0.9f * f)), dim(e, 0.9f * (1 - f) + 0.1f));
      }
      for (int i = 0; i < 18; ++i) {
        float on = sinf(t * 1.1f + i * 2.3f);
        if (on < 0.3f) continue;
        float a = hash01(i + 7) * 6.2831853f, r = sqrtf(hash01(i + 70)) * 0.8f;
        int s = (int)(rad * 0.08f);
        int x = cx + rad * r * cosf(a), y = cy + rad * r * sinf(a);
        c.fillRect(x - x % s, y - y % s, s - 1, s - 1, dim(e, on));
      }
      break;
    }
    case EM_PLASMA: {                                // 光の点でできた、回る球
      const int N = 90;
      for (int i = 0; i < N; ++i) {
        float z = 1 - 2 * (i + 0.5f) / N, r = sqrtf(1 - z * z);
        float th = i * 2.3999632f + t * 0.6f;
        float x = r * cosf(th), y = z, d = r * sinf(th);
        float b = 0.35f + 0.65f * (d * 0.5f + 0.5f);
        c.fillCircle(cx + x * rad * 0.85f, cy + y * rad * 0.85f, d > 0 ? 2 : 1, dim(e, b));
      }
      break;
    }
    case EM_KINTSUGI: {                              // 金継ぎの器
      c.fillCircle(cx, cy, rad * 0.88f, rgb(16, 12, 10));
      c.drawCircle(cx, cy, rad * 0.88f, rgb(70, 50, 30));
      for (int k = 0; k < 5; ++k) {
        float a = k * 1.2566371f + 0.4f;
        float px = cx + rad * 0.08f * cosf(a), py = cy + rad * 0.08f * sinf(a);
        for (int i = 1; i <= 6; ++i) {
          float r = rad * 0.85f * i / 6.0f;
          float aa = a + (hash01(k * 10 + i) - 0.5f) * 0.45f;
          float x = cx + r * cosf(aa), y = cy + r * sinf(aa);
          float g = 0.5f + 0.5f * sinf(t * 2.0f - i * 0.8f + k);
          c.drawLine(px, py, x, y, rgb((int)(120 + 135 * g), (int)(80 + 110 * g), (int)(20 + 40 * g)));
          c.drawLine(px + 1, py, x + 1, y, rgb((int)(90 + 100 * g), (int)(60 + 80 * g), 20));
          px = x; py = y;
        }
      }
      break;
    }
    case EM_INFINITY: {                              // 奥へ続く合わせ鏡
      float ph = fmodf(t * 0.4f, 1.0f);
      for (int k = 0; k < 10; ++k) {
        float s = rad * 0.95f * powf(0.74f, k + ph);
        float a = (k + ph) * 0.13f + t * 0.1f;
        float x[4], y[4];
        for (int i = 0; i < 4; ++i) { float th = a + i * 1.5707963f + 0.7853982f; x[i] = cx + s * cosf(th); y[i] = cy + s * sinf(th); }
        uint16_t cc = dim(e, 0.15f + 0.85f * (1.0f - (k + ph) / 10.0f));
        for (int i = 0; i < 4; ++i) c.drawLine(x[i], y[i], x[(i + 1) & 3], y[(i + 1) & 3], cc);
        c.fillCircle(x[0], y[0], 2, cc);
      }
      break;
    }
    case EM_TWIN: {                                  // 光の立方体でできた人の姿
      static const char* FIG[] = {"..XXX..", "..XXX..", "...X...", ".XXXXX.", "X.XXX.X", "X.XXX.X", "..XXX..", "..X.X..", "..X.X..", "..X.X.."};
      const int R = 10;
      int s = rad * 2 / (R + 2);
      float scan = fmodf(t * 0.35f, 1.4f);         // 走査線
      for (int j = 0; j < R; ++j)
        for (int i = 0; i < 7; ++i) {
          if (FIG[j][i] != 'X') continue;
          float v = 1.0f - (float)j / R;           // 下から組み上がる
          if (v > scan) continue;
          int x = cx + (i - 3) * s, y = cy + (j - R / 2) * s + s / 2;
          float g = (scan - v < 0.08f) ? 1.0f : 0.55f;
          c.fillRect(x - s / 2 + 1, y - s / 2 + 1, s - 2, s - 2, dim(e, g));
        }
      if (scan < 1.0f) {
        int y = cy + (R / 2 - scan * R) * s + s / 2;
        c.drawLine(cx - rad * 0.8f, y, cx + rad * 0.8f, y, col);
      }
      break;
    }
    case EM_DROP: {                                  // 鏡の雫
      for (int k = 0; k < 3; ++k) {
        float a = t * (0.5f + 0.2f * k) + k * 2.1f;
        int x = cx + rad * 0.35f * cosf(a), y = cy + rad * 0.30f * sinf(a * 1.2f);
        int r = rad * (0.30f + 0.06f * k);
        c.fillCircle(x, y, r, dim(e, 0.35f));
      }
      for (int k = 0; k < 3; ++k) {
        float a = t * (0.5f + 0.2f * k) + k * 2.1f;
        int x = cx + rad * 0.35f * cosf(a), y = cy + rad * 0.30f * sinf(a * 1.2f);
        int r = rad * (0.30f + 0.06f * k);
        c.fillCircle(x - r * 0.35f, y - r * 0.35f, r * 0.22f, dim(e, 0.95f));
        c.drawCircle(x, y, r, dim(e, 0.6f));
      }
      break;
    }
    case EM_TILES: {                                 // うねる鏡のタイル(ひし形)
      const int N = 6;
      float s = rad * 1.7f / N;
      for (int j = 0; j < N; ++j)
        for (int i = 0; i < N; ++i) {
          float u = (i - (N - 1) * 0.5f) * s, v = (j - (N - 1) * 0.5f) * s;
          if (u * u + v * v > rad * rad * 0.85f) continue;
          float w = 0.5f + 0.5f * sinf(t * 1.8f - i * 0.7f - j * 0.4f);
          float h = s * 0.48f, k = h * (0.35f + 0.65f * w);
          int x = cx + u, y = cy + v;
          uint16_t cc = dim(e, 0.20f + 0.80f * w);
          c.fillTriangle(x - k, y, x, y - h, x + k, y, cc);
          c.fillTriangle(x - k, y, x, y + h, x + k, y, cc);
        }
      break;
    }
    case EM_ZEN: {                                   // 砂紋と石
      int sx = cx + rad * 0.25f, sy = cy - rad * 0.1f, sr = rad * 0.17f;
      for (int k = 1; k <= 4; ++k) c.drawCircle(sx, sy, sr + k * rad * 0.09f, dim(e, 0.55f));
      for (int j = -7; j <= 7; ++j) {
        float y0 = j * 0.12f;
        int px = 0, py = 0;
        for (int i = 0; i <= 40; ++i) {
          float u = -1 + i / 20.0f;
          int x = cx + u * rad, y = cy + (y0 + 0.012f * sinf(u * 8 + t)) * rad;
          float dx = x - sx, dy = y - sy;
          bool ok = (u * u + y0 * y0 < 0.9f) && (dx * dx + dy * dy > (sr + rad * 0.40f) * (sr + rad * 0.40f));
          if (ok && px) c.drawLine(px, py, x, y, dim(e, 0.45f));
          if (ok) { px = x; py = y; } else px = 0;
        }
      }
      c.fillCircle(sx, sy, sr, rgb(70, 66, 62));
      c.drawCircle(sx, sy, sr, rgb(120, 115, 108));
      break;
    }
    case EM_BOKEH: {                                 // 六角形の玉ボケ
      static const uint8_t BC[4][3] = {{255, 140, 180}, {255, 200, 110}, {120, 190, 255}, {200, 150, 255}};
      for (int i = 0; i < 11; ++i) {
        float a = hash01(i + 3) * 6.2831853f, r = sqrtf(hash01(i + 33)) * 0.70f;
        float br = 0.5f + 0.5f * sinf(t * 0.9f + i * 1.9f);
        float s = rad * (0.12f + 0.16f * hash01(i + 66)) * (0.85f + 0.3f * br);
        int x = cx + rad * r * cosf(a + t * 0.05f), y = cy + rad * r * sinf(a + t * 0.05f);
        const uint8_t* q = BC[i & 3];
        float k = 0.25f + 0.45f * br;
        uint16_t fill = rgb((int)(q[0] * k * 0.5f), (int)(q[1] * k * 0.5f), (int)(q[2] * k * 0.5f));
        uint16_t edge = rgb((int)(q[0] * k), (int)(q[1] * k), (int)(q[2] * k));
        float hx[6], hy[6];
        for (int v = 0; v < 6; ++v) { float th = v * 1.0471976f + 0.2f; hx[v] = x + s * cosf(th); hy[v] = y + s * sinf(th); }
        for (int v = 0; v < 6; ++v) c.fillTriangle(x, y, hx[v], hy[v], hx[(v + 1) % 6], hy[(v + 1) % 6], fill);
        for (int v = 0; v < 6; ++v) c.drawLine(hx[v], hy[v], hx[(v + 1) % 6], hy[(v + 1) % 6], edge);
      }
      break;
    }
    case EM_GIF: {                                   // 重なったコマと、その中ではねる玉
      const int w = (int)(rad * 1.20f), h = (int)(rad * 0.86f), off = (int)(rad * 0.17f);
      for (int k = 2; k >= 0; --k) {                 // 奥のコマから順に
        const int x0 = cx - w / 2 + (1 - k) * -off + off / 2, y0 = cy - h / 2 + (1 - k) * off - off / 2;
        const uint16_t edge = dim(e, k == 0 ? 0.95f : (k == 1 ? 0.45f : 0.25f));
        c.fillRect(x0, y0, w, h, rgb(8, 10, 16));
        c.drawLine(x0, y0, x0 + w - 1, y0, edge);         c.drawLine(x0, y0 + h - 1, x0 + w - 1, y0 + h - 1, edge);
        c.drawLine(x0, y0, x0, y0 + h - 1, edge);         c.drawLine(x0 + w - 1, y0, x0 + w - 1, y0 + h - 1, edge);
        if (k == 0) {                                // いちばん手前のコマ: 床と、はねる玉
          const int gy = y0 + h - h / 5;
          c.drawLine(x0 + w / 10, gy, x0 + w - w / 10, gy, dim(e, 0.35f));
          float ph = fmodf(t * 0.9f, 1.0f);
          const float bx = x0 + w * (0.18f + 0.64f * ph);
          const float by = gy - rad * 0.08f - (h * 0.52f) * fabsf(sinf(ph * 3.1415927f * 2.0f));
          c.fillCircle((int)bx, (int)by, (int)(rad * 0.08f), rgb(e.r, e.g, e.b));
        }
      }
      break;
    }
    case EM_KALEIDO: {                               // ばらばらの場所のうずと黒い穴、動きまわる光の粒
      static const uint8_t KC[4][3] = {{235, 40, 30}, {255, 150, 40}, {80, 210, 255}, {40, 90, 230}};
      static const float P[3][3] = {{-0.40f, -0.30f, 0.50f}, {0.38f, 0.22f, 0.58f}, {-0.12f, 0.52f, 0.32f}};   // x, y, 大きさ
      for (int q = 0; q < 3; ++q) {
        const float ox = rad * P[q][0], oy = rad * P[q][1], sz = rad * P[q][2];
        const float dir = (q & 1) ? -1.0f : 1.0f;
        for (int arm = 0; arm < 3; ++arm) {
          const uint8_t* k = KC[(arm + q) & 3];
          int px = 0, py = 0;
          for (int i = 0; i <= 22; ++i) {
            const float r = sz * (0.15f + 0.85f * i / 22.0f);
            const float th = dir * (arm * 2.0943951f + (i / 22.0f) * 3.6f + t * 0.9f);
            const int x = cx + (int)(ox + r * cosf(th)), y = cy + (int)(oy + r * sinf(th));
            if (i) c.drawLine(px, py, x, y, rgb(k[0] * (i + 6) / 28, k[1] * (i + 6) / 28, k[2] * (i + 6) / 28));
            px = x; py = y;
          }
        }
        c.fillCircle(cx + (int)ox, cy + (int)oy, sz * 0.22f, rgb(0, 0, 0));                 // 黒いつやつやの玉
        c.fillCircle(cx + (int)(ox - sz * 0.08f), cy + (int)(oy - sz * 0.08f), max(1, (int)(sz * 0.05f)), rgb(255, 255, 255));
      }
      for (int i = 0; i < 9; ++i) {                 // ふらふら動きまわって、またたく光の粒
        const float a = hash01(i * 3 + 1) * 6.2831853f + t * (0.3f + 0.4f * hash01(i * 3 + 4)) * ((i & 1) ? 1 : -1);
        const float r = rad * (0.2f + 0.6f * (0.5f + 0.5f * sinf(t * (0.5f + hash01(i * 3 + 2)) + i)));
        const float tw = 0.5f + 0.5f * sinf(t * (2.0f + hash01(i * 3 + 3) * 3.0f) + i * 1.7f);
        if (tw < 0.25f) continue;
        c.fillCircle(cx + (int)(r * cosf(a)), cy + (int)(r * sinf(a)), 1 + (int)(tw * 1.5f), rgb((int)(160 + 95 * tw), (int)(160 + 95 * tw), (int)(170 + 85 * tw)));
      }
      break;
    }
    case EM_GALAXY: {                                // 金色の中心と、青白い2本の腕(ゆっくり回る)
      for (int i = 0; i < 12; ++i) {                // まわりの星
        const float a = hash01(i * 5 + 1) * 6.2831853f, r = rad * (0.55f + 0.42f * hash01(i * 5 + 2));
        const float tw = 0.5f + 0.5f * sinf(t * (1.5f + hash01(i * 5 + 3) * 2.0f) + i);
        c.fillCircle(cx + (int)(r * cosf(a)), cy + (int)(r * sinf(a)), 1, rgb((int)(120 + 120 * tw), (int)(125 + 120 * tw), (int)(140 + 110 * tw)));
      }
      const float rot = t * 0.35f, inc = 0.62f, ca = cosf(0.5f), sa = sinf(0.5f);
      for (int arm = 0; arm < 2; ++arm)
        for (int i = 0; i < 70; ++i) {
          const float q = i / 69.0f, r = rad * (0.10f + 0.78f * q);
          const float th = rot + arm * 3.1415927f + logf(1.0f + q * 6.0f) * 1.35f;
          const float u = r * cosf(th), v = r * sinf(th) * inc;
          const int x = cx + (int)(u * ca - v * sa), y = cy + (int)(u * sa + v * ca);
          const float k = 1.0f - q * 0.75f;
          c.fillCircle(x, y, q < 0.4f ? 2 : 1, rgb((int)(150 * k + 60), (int)(170 * k + 60), (int)(220 * k + 35)));
          if ((i % 9) == 4) c.fillCircle(x + 1, y, 1, rgb(255, 110, 150));   // 赤い星雲
        }
      c.fillCircle(cx, cy, (int)(rad * 0.16f), rgb(120, 96, 60));
      c.fillCircle(cx, cy, (int)(rad * 0.10f), rgb(230, 200, 150));
      c.fillCircle(cx, cy, max(1, (int)(rad * 0.04f)), rgb(255, 250, 235));
      break;
    }
    case EM_CLOUD: {                                 // 地平線と、流れる雲のかたまり、雲の上の虹の輪(軽い絵)
      const int hy = cy - (int)(rad * 0.35f);
      c.drawLine(cx - (int)(rad * 0.93f), hy, cx + (int)(rad * 0.93f), hy, dim(e, 0.75f));   // 地平線
      const float drift = sinf(t * 0.6f) * rad * 0.04f;   // 雲が少しゆれる
      for (int row = 0; row < 2; ++row) {           // 雲の帯2本(奥は小さく、手前は大きく)。丸を重ねて、もくもくの帯にする
        const float r = rad * (row ? 0.17f : 0.10f), y = hy + rad * (row ? 0.62f : 0.20f);
        const int nb = row ? 5 : 7;
        const float span = rad * (row ? 1.50f : 1.55f), x0 = -span / 2;
        for (int i = 0; i < nb; ++i)
          c.fillCircle(cx + (int)(x0 + span * i / (nb - 1) + drift * (row ? 1.0f : 0.5f)), (int)y - ((i & 1) ? (int)(r * 0.3f) : 0), (int)r, dim(e, row ? 0.55f : 0.35f));
      }
      static const uint8_t RB[3][3] = {{110, 120, 255}, {110, 235, 150}, {255, 110, 90}};
      const int gy = cy + (int)(rad * 0.05f);
      for (int k = 0; k < 3; ++k) {                 // 虹の輪(ななめから見た丸)
        const float rx = rad * 0.34f + k * 2.0f, ry = rx * 0.5f;
        int px = cx + (int)rx, py = gy;
        for (int i = 1; i <= 16; ++i) {
          const float a = i * (6.2831853f / 16.0f);
          const int x = cx + (int)(rx * cosf(a)), y = gy + (int)(ry * sinf(a));
          c.drawLine(px, py, x, y, rgb(RB[k][0], RB[k][1], RB[k][2]));
          px = x; py = y;
        }
      }
      break;
    }
    case EM_SAPIENS: {                               // 輪切りの線でできた人。頭から糸がほどけて、右の光へ流れる(軽い絵)
      const int hy = cy + (int)(rad * 0.12f);
      c.drawLine(cx - (int)(rad * 0.93f), hy, cx + (int)(rad * 0.93f), hy, rgb(110, 120, 150));   // 地平線
      const int lx = cx + (int)(rad * 0.55f), ly = cy - (int)(rad * 0.25f);
      c.fillCircle(lx, ly, max(2, (int)(rad * 0.07f)), rgb(140, 125, 105));   // 光の入り口
      c.fillCircle(lx, ly, max(1, (int)(rad * 0.035f)), rgb(255, 240, 215));
      const float fx = cx - rad * 0.22f, top = cy - rad * 0.62f, bot = cy + rad * 0.62f, hh = bot - top;
      for (int k = 0; k <= 30; ++k) {               // 輪切りの線(高さごとの体の幅)
        const float q = k / 30.0f, y = top + hh * q;
        float w; float off = 0;
        if (q < 0.13f) w = 0.055f * sqrtf(fmaxf(0.0f, 1.0f - powf((q - 0.065f) / 0.07f, 2)));   // 頭
        else if (q < 0.17f) w = 0.025f;                                                       // 首
        else if (q < 0.50f) w = 0.10f - 0.02f * (q - 0.17f) / 0.33f;                          // 胴
        else if (q < 0.56f) w = 0.085f;                                                       // 腰
        else { w = 0.032f; off = 0.042f; }                                                    // 脚
        const int yi = (int)y, ww = (int)(w * hh), oo = (int)(off * hh);
        const bool gone = q < 0.11f && fmodf(t * 0.25f, 1.0f) > 0.25f;   // 頭の上は、ときどきほどけている
        if (gone || ww < 1) continue;
        const uint16_t col = rgb(170 + (k % 3) * 20, 175 + (k % 2) * 25, 215);
        if (oo) { c.drawLine((int)fx - oo - ww, yi, (int)fx - oo + ww, yi, col); c.drawLine((int)fx + oo - ww, yi, (int)fx + oo + ww, yi, col); }
        else c.drawLine((int)fx - ww, yi, (int)fx + ww, yi, col);
        if (q > 0.17f && q < 0.45f) {               // 腕
          const int ax = (int)(0.13f * hh);
          c.drawLine((int)fx - ax - 2, yi, (int)fx - ax + 1, yi, col); c.drawLine((int)fx + ax - 1, yi, (int)fx + ax + 2, yi, col);
        }
      }
      for (int k = 0; k < 9; ++k) {                 // ほどけた糸(ゆれながら光の方へ)
        const float sx = fx + (k - 4) * 1.2f, sy = top + hh * (0.02f + 0.01f * k);
        const float c1x = fx + rad * 0.25f, c1y = top - rad * (0.10f + 0.03f * k) + sinf(t * 1.3f + k) * rad * 0.05f;
        int px = (int)sx, py = (int)sy;
        for (int q = 1; q <= 14; ++q) {
          const float u = q / 14.0f, a = 1 - u;
          const int x = (int)(a * a * sx + 2 * a * u * c1x + u * u * lx), y = (int)(a * a * sy + 2 * a * u * c1y + u * u * ly);
          const int b = (int)(220 * (1.0f - 0.6f * u));
          c.drawLine(px, py, x, y, (k % 3 == 0) ? rgb(b * 3 / 4, b, b) : (k % 3 == 1 ? rgb(b, b * 3 / 4, b) : rgb(b, b, b * 7 / 8)));
          px = x; py = y;
        }
      }
      break;
    }
    case EM_SUNSET: {                                // 水平線に沈む日と、水面にくだける光の道、島のシルエット(軽い絵)
      const int hy = cy + (int)(rad * 0.08f);
      const int ytop = cy - (int)(rad * 0.85f);
      for (int y0 = ytop; y0 <= hy; ++y0) {         // 空(上ほど紫、水平線ほど金色。丸い窓の形に)
        const float q = (float)(y0 - ytop) / max(1, hy - ytop);
        const int w = (int)(sqrtf(fmaxf(0.0f, rad * rad * 0.80f - (float)((y0 - cy) * (y0 - cy)))));
        c.drawLine(cx - w, y0, cx + w, y0, rgb((int)(50 + 170 * q * q), (int)(30 + 100 * q * q * q), (int)(80 - 10 * q)));
      }
      const float sp = 0.5f + 0.5f * sinf(t * 0.25f);   // 日がゆっくり上下する
      const int sx = cx + (int)(rad * 0.15f), sy = hy - (int)(rad * (0.02f + 0.16f * sp)), sr = max(3, (int)(rad * 0.09f));
      c.fillCircle(sx, sy, sr + 3, rgb(150, 80, 50)); c.fillCircle(sx, sy, sr, rgb(255, 225, 170));
      for (int y0 = hy + 1; y0 < cy + (int)(rad * 0.85f); ++y0) { const int w = (int)(sqrtf(fmaxf(0.0f, rad * rad * 0.80f - (float)((y0 - cy) * (y0 - cy))))); c.drawLine(cx - w, y0, cx + w, y0, rgb(28, 18, 40)); }   // 海
      c.drawLine(cx - (int)(rad * 0.93f), hy, cx + (int)(rad * 0.93f), hy, rgb(255, 190, 120));             // 水平線
      for (int k = 0; k < 9; ++k) {                 // 光の道(ゆれる短い線)
        const int yy = hy + 3 + k * (int)(rad * 0.06f) + 1;
        const int w = (int)(rad * (0.04f + 0.02f * k) * (0.6f + 0.4f * sinf(t * 3.0f + k * 1.7f)));
        const int b = 230 - k * 18;
        c.drawLine(sx - w, yy, sx + w, yy, rgb(b, b * 3 / 4, b / 2));
      }
      for (int k = 0; k < 6; ++k) {                 // 島
        const int ix = cx - (int)(rad * 0.55f) + k * 3, ih = (int)(rad * (0.05f + 0.025f * sinf(k * 1.3f)));
        c.fillRect(ix - (int)(rad * 0.12f) + k * 4, hy - ih, (int)(rad * 0.10f), ih, rgb(20, 14, 22));
      }
      c.fillCircle(cx + (int)(rad * 0.62f), hy, (int)(rad * 0.06f), rgb(20, 14, 22));
      c.fillRect(cx + (int)(rad * 0.50f), hy + 1, (int)(rad * 0.24f), (int)(rad * 0.08f), rgb(28, 18, 40));
      break;
    }
    case EM_CHRON: {                                 // 床と天井を流れてくる年表の行と、まん中のモノリス
      c.drawLine(cx - (int)(rad * 0.95f), cy, cx + (int)(rad * 0.95f), cy, dim(e, 0.45f));   // 地平線
      for (int k = 0; k < 7; ++k) {
        const float p = fmodf(t * 0.30f + k / 7.0f, 1.0f);           // 0 = 奥 → 1 = 手前
        const int yo = (int)(rad * (0.06f + 0.78f * p * p));
        const float hw = rad * (0.25f + 0.65f * p);
        const uint16_t col = dim(e, 0.12f + 0.70f * p);
        for (int side = -1; side <= 1; side += 2) {                   // 床と天井(鏡)
          const int yy = cy + side * yo;
          float x = cx - hw;
          uint32_t h = (uint32_t)(k * 2654435761u) ^ (uint32_t)(int)(t * 0.30f + k / 7.0f);
          while (x < cx + hw) {                                       // 字の並びのような、とぎれとぎれの線
            h = h * 1103515245u + 12345u;
            const float seg = hw * (0.10f + ((h >> 16) & 15) / 60.0f);
            const float x2 = fminf(x + seg, cx + hw);
            c.drawLine((int)x, yy, (int)x2, yy, col);
            if (p > 0.55f) c.drawLine((int)x, yy + side, (int)x2, yy + side, col);
            x = x2 + hw * 0.06f;
          }
        }
      }
      const int mw = (int)(rad * 0.30f), mh = (int)(rad * 0.98f), my = cy - (int)(mh * 0.58f);
      c.fillRect(cx - mw / 2, my, mw, mh, rgb(8, 8, 13));
      c.drawLine(cx - mw / 2, my, cx - mw / 2, my + mh - 1, dim(e, 0.85f));
      c.drawLine(cx + mw / 2, my, cx + mw / 2, my + mh - 1, dim(e, 0.40f));
      c.drawLine(cx - mw / 2, my, cx + mw / 2, my, dim(e, 0.85f));
      c.setFont(&fonts::lgfxJapanMincho_20);
      c.setTextSize(rad >= 60 ? 2.0f : 1.0f);
      c.setTextDatum(middle_center);
      c.setTextColor(rgb(e.r, e.g, e.b));
      c.drawString("史", cx, my + (int)(mh * 0.32f));
      c.setTextSize(1.0f);
      break;
    }
  }
}

// ---------------------------------------------------------------- メニューを描く
static void drawMenu() {
  if (!cv) return;
  const Entry& e = ITEMS[sel];
  M5Canvas& c = *cv;
  const bool big = RR >= 200;

  c.fillScreen(rgb(5, 6, 10));
  const float breathe = 0.85f + 0.15f * sinf(menuT * 1.1f);
  for (int k = 9; k >= 1; --k) {                     // うっすら光る背景
    float f = k / 9.0f;
    float w = (1 - f) * (1 - f) * 0.085f * breathe;
    c.fillCircle(CX, CY - RR * 0.25f, (int)(RR * (0.22f + 0.78f * f)),
                 rgb((int)(e.r * w + 5), (int)(e.g * w + 6), (int)(e.b * w + 10)));
  }
  if (roundScr) {
    c.drawCircle(CX, CY, RR - 3, dim(e, 0.16f));     // 画面のふちを縁取る
    c.drawCircle(CX, CY, RR - 4, dim(e, 0.10f));
  }

  int emR = RR * 0.34f;
  int emY = CY - RR * (big ? 0.30f : 0.40f);
  int sx  = (int)(slide * SW * 0.55f);               // 切り替え時に横へ流れる
  float fade = 1.0f - fabsf(slide);
  if (fade < 0) fade = 0;

  drawEmblem(c, e, CX + sx, emY, emR, menuT);

  c.setTextDatum(middle_center);
  c.setTextColor(rgb((int)(230 * fade + 8), (int)(232 * fade + 8), (int)(240 * fade + 12)));
  c.setFont(big ? &fonts::FreeSansBold18pt7b : &fonts::FreeSansBold12pt7b);
  c.drawString(e.name, CX + sx, CY + RR * (big ? 0.20f : 0.10f));

  c.setTextColor(dim(e, 0.85f * fade + 0.02f));
  c.setFont(big ? &fonts::lgfxJapanGothicP_20 : &fonts::lgfxJapanGothicP_16);
  c.drawString(e.jp, CX + sx, CY + RR * (big ? 0.36f : 0.30f));

  char num[16];
  snprintf(num, sizeof(num), "%d / %d", sel + 1, NITEM);
  c.setTextColor(rgb(120, 126, 142));
  c.setFont(&fonts::lgfxJapanGothicP_16);
  c.drawString(num, CX, CY + RR * (big ? 0.50f : 0.47f));

  c.setTextColor(rgb(96, 102, 118));
  if (loadingNow) {                                  // 作品を選んだ: 操作の説明のかわりに「読み込み中…」
    c.setTextColor(rgb(e.r, e.g, e.b));
    c.setFont(&fonts::lgfxJapanGothicP_20);
    c.drawString("読み込み中...", CX, CY + RR * (roundScr ? 0.66f : 0.70f));
    c.setFont(&fonts::lgfxJapanGothicP_16);
  } else if (roundScr) {
    c.drawString("A: つぎへ    B: きめる", CX, CY + RR * 0.63f);
    c.drawString("もどる: B を長押し", CX, CY + RR * 0.73f);
  } else {
    c.drawString("なぞる:えらぶ  タップ:スタート", CX, CY + RR * 0.64f);
    c.drawString("もどる: 左上を長押し", CX, CY + RR * 0.80f);
  }

  if (roundScr) {                                    // 何番目か(ふちに並ぶ点)
    for (int i = 0; i < NITEM; ++i) {
      float a = i * (6.2831853f / NITEM) - 1.5707963f;   // 1番を真上に、時計回りに並べる
      int x = CX + (RR - 13) * cosf(a), y = CY + (RR - 13) * sinf(a);
      if (i == sel) c.fillCircle(x, y, 5, rgb(e.r, e.g, e.b));
      else          c.fillCircle(x, y, 2, rgb(58, 62, 74));
    }
  } else {                                           // 四角い画面: 下に1列
    for (int i = 0; i < NITEM; ++i) {
      int x = CX + (i - (NITEM - 1) * 0.5f) * 12, y = SH - 5;
      if (i == sel) c.fillCircle(x, y, 4, rgb(e.r, e.g, e.b));
      else          c.fillCircle(x, y, 2, rgb(58, 62, 74));
    }
  }

  // 版の名前(丸い画面は電池の上、四角い画面は左上)
  c.setFont(&fonts::lgfxJapanGothicP_16);
  c.setTextColor(rgb(110, 112, 122));
  if (roundScr) { c.setTextDatum(middle_center); c.drawString(MENU_VER, CX, CY - (int)(RR * 0.80f) - 12); }
  else          { c.setTextDatum(middle_left);   c.drawString(MENU_VER, 8, 12); }
  // 電池(丸い画面は上のまん中、四角い画面は右上)
  if (batLevel >= 0) {
    char bt[24];
    snprintf(bt, sizeof(bt), batCharging ? "%d%% 充電中" : "%d%%", batLevel);
    c.setFont(&fonts::lgfxJapanGothicP_16);
    const int tw = c.textWidth(bt), total = 24 + 6 + tw;
    const int bx = roundScr ? CX - total / 2 : SW - 8 - total;
    const int by = roundScr ? CY - (int)(RR * 0.80f) : 6;
    const uint16_t gray = rgb(150, 156, 170);
    drawBatIcon(c, bx, by, batLevel, gray, batCharging ? rgb(110, 190, 255) : batColor(batLevel));
    c.setTextDatum(middle_left);
    c.setTextColor(gray);
    c.drawString(bt, (bx & ~1) + 30, by + 6);
  }

  sampleTouch();                              // 送る前に、もう一度指を読む
  c.pushSprite(0, 0);
  sampleTouch();
}

// ---------------------------------------------------------------- 作品を始める(再起動して)
static void launch(int i) {
  nmSel = i;
  nmRun = i;
  Serial.printf("launch %s\n", ITEMS[i].name);
  // 「読み込み中…」の画面を出してから再起動する(画面は、作品が描き始めるまでこの絵のまま)
  sel = i; slide = 0; loadingNow = true;
  drawMenu();
  delay(40);
  esp_restart();
}

// ---------------------------------------------------------------- setup / loop
void setup() {
  const esp_reset_reason_t why = esp_reset_reason();
  if (nmMagic != NM_MAGIC) { nmMagic = NM_MAGIC; nmSel = 0; nmRun = NM_NONE; }
  if (nmSel >= (uint32_t)NITEM) nmSel = 0;
  sel = (int)nmSel;

  if (why == ESP_RST_SW && nmRun < (uint32_t)NITEM) {   // 選ばれた作品だけを動かす
    running = (int)nmRun;
    nmRun = NM_NONE;                                    // 途中で止まって再起動したら、メニューに戻るように
    size_t back = nmGiveBack(*ITEMS[running].ovl);
    ITEMS[running].setup();
    Serial.printf("NullMenu v97 -> %s  (arena %u used, %u returned to heap)\n", ITEMS[running].name,
                  (unsigned)*ITEMS[running].ovl, (unsigned)back);
    return;
  }
  nmRun = NM_NONE;
  nmGiveBack(0);                                        // メニューは置き場を使わない

  // ---- メニュー
  auto cfg = M5.config();
  M5.begin(cfg);
  Serial.begin(115200);
  Serial.println(MENU_VER);

  if (M5.Display.width() < M5.Display.height()) M5.Display.setRotation(1);
  SW = M5.Display.width(); SH = M5.Display.height();
  CX = SW / 2; CY = SH / 2;
  RR = min(SW, SH) / 2;
  roundScr = (SW == SH);
  size_t use = 0;
  for (int i = 0; i < NITEM; ++i) use = max(use, *ITEMS[i].ovl);
  Serial.printf("NullMenu  screen %dx%d  arena %u / %u bytes\n", SW, SH, (unsigned)use, (unsigned)NM_ARENA_SIZE);

  M5.Display.setBrightness(190);
  M5.Display.fillScreen(TFT_BLACK);
  cv = new M5Canvas(&M5.Display);
  cv->setPsram(true);
  cv->setColorDepth(16);
  if (!cv->createSprite(SW, SH)) { delete cv; cv = nullptr; }
  waitRelease = true;
  guardUntil = millis() + 300;
}

void loop() {
  if (running >= 0) {
    ITEMS[running].loop();                    // 中で M5.update() が呼ばれます
    batOverlay();                             // 電池が少ないときだけ、小さな赤い電池の絵
    if (nmWantsMenu()) nmBackToMenu();
    return;
  }

  // ---- メニュー
  M5.update();
  uint32_t now = millis();
  auto t = M5.Touch.getDetail();
  static uint32_t last = now;
  float dt = (now - last) / 1000.0f;
  last = now;
  if (dt > 0.1f) dt = 0.1f;
  menuT += dt;
  if (!swipeOn) slide *= expf(-dt * 12.0f);   // 流れをすっと止める(なぞっているあいだは指についてくる)

  if (waitRelease || now <= guardUntil) {     // 戻ってきたときに押していたボタン・指が離れるまで待つ
    M5.BtnA.wasClicked(); M5.BtnA.wasHold(); M5.BtnB.wasClicked();   // この間のボタンの出来事は捨てる
    if (waitRelease && !M5.BtnA.isPressed() && !M5.BtnB.isPressed() && !t.isPressed()) {
      waitRelease = false;
      guardUntil = now + 250;
    }
  } else {
    if (M5.BtnA.wasHold())         { sel = (sel + NITEM - 1) % NITEM; slide = -1.0f; }
    else if (M5.BtnA.wasClicked()) { sel = (sel + 1) % NITEM; slide = 1.0f; }
    if (M5.BtnB.wasClicked()) { launch(sel); return; }

    // タッチは一瞬とぎれることがある(タッチの部品から読めなかったとき)。0.15秒までのとぎれは、さわり続けているものとして扱う
    if (t.isPressed()) { noteTouch(true, t.x, t.y, now); if (now - touchT0 <= FLICK_MS) readChipGesture(); }
    const bool pressed = swipeOn && now - tSeen < 150;
    // タップのあと、0.2秒たって指が触れていなければ始める
    if (launchAt && !swipeOn && (int32_t)(now - launchAt) >= 0) { launchAt = 0; launch(sel); return; }
    if (pressed) {
      // なぞっているあいだは、絵が指についてくる(選んでいる作品は、まだ変えない)
      const int dxNow = tx - touchX0;
      if (abs(dxNow) > SW / 40) {
        float d = dxNow / (SW * 0.55f); d = d > 0.9f ? 0.9f : d < -0.9f ? -0.9f : d;
        slide += (d - slide) * (1.0f - expf(-dt * 22.0f));   // 指の位置へ、なめらかに追いつく
      }
    } else if (swipeOn) {
      swipeOn = false;
      readChipGesture();                      // 離した直後にも、タッチの部品の判定を見る
      const int dx = tx - touchX0, dy = ty - touchY0;
      const int upMove = max(abs(dx), abs(dy));                         // 離したときの位置(最後に読んだ位置)との差
      const int moved = max(touchMove, upMove);
      const uint32_t held = tSeen - touchT0;                            // 最後に指を読めたときまで
      const char* what = "none";
      // 1回なぞると1つだけ: 横に画面の 1/8 以上、縦より横が大きく動いたとき
      if (abs(dx) >= SW / 8 && abs(dx) > abs(dy) * 6 / 5) {
        if (dx < 0) { sel = (sel + 1) % NITEM; slide += 1.0f; what = "next"; }          // 左へなぞる → 次へ(右から入ってくる)
        else        { sel = (sel + NITEM - 1) % NITEM; slide -= 1.0f; what = "prev"; }  // 右へなぞる → 前へ
        swiped = true;
        slide = slide > 1.0f ? 1.0f : slide < -1.0f ? -1.0f : slide;
      }
      // タップ = なぞっていない・ほとんど動いていない(画面の 1/24 まで)・0.4秒以内に離した・なぞり終えて 0.5秒以上たっている
      //          ・まん中の絵の丸の中で触れて、丸の中で離した
      const int ey = CY - (int)(RR * (roundScr ? 0.30f : 0.40f)), er = (int)(RR * 0.48f);
      const auto inEmblem = [&](int x, int y) { const int a = x - CX, b = y - ey; return a * a + b * b < er * er; };
      const bool tapped = !swiped && !chipSwipe && moved < SW / 24 && held < 400 && now - swipeEndMs > 500 && inEmblem(touchX0, touchY0) && inEmblem(tx, ty);
      if (swiped || chipSwipe || moved >= SW / 24) swipeEndMs = now;
      if (tapped) what = "tap";
      else if (chipSwipe && !swiped) what = "none (chip: swipe)";
#if defined(ESP_PLATFORM)
      if (Serial.availableForWrite() >= 96)                // USB が受け取れるときだけ(受け取れないときに待たされないように)
#endif
        Serial.printf("menu touch: dx %d dy %d moved %d held %u ms -> %s\n", dx, dy, moved, (unsigned)held, what);
      if (tapped) launchAt = now + 200;      // まん中の絵をタップ → 0.2秒後にスタート(そのあいだに触れたら取り消す)
    }
  }
  nmSel = sel;

  batUpdate(5000);
  drawMenu();
  if (!swipeOn) for (int i = 0; i < 4; ++i) { delay(5); sampleTouch(); }   // 待つあいだも、指をこまめに読む
}
