// NullChronicle  -  NullMenu 用モジュール(元のスケッチ NullChronicle_v7 を名前空間に入れたもの)
// 0 で始まる変数(配列も)は、ほかの作品と共通の置き場(NM_ARENA)に置いています。
// 一度に動くのは1つだけ(切り替えるときは再起動)なので、重なっていても問題ありません。
#include "nm_common.h"
#if defined(ESP_PLATFORM) && __has_include(<lgfx/v1/panel/Panel_AMOLED.hpp>)
#include <lgfx/v1/panel/Panel_AMOLED.hpp>
#endif
#pragma GCC optimize("O3")

namespace NullChronicle {
static constexpr size_t nmOv0 = 0;

// NullChronicle v7  -  鏡の部屋を、人類の歴史とこれからの AI の年表が流れていく。見え方を2つから選べる
//   null² の「ダイアログモード」(鏡の部屋で、床と天井を年表のような文字が流れ、まん中にモノリスが立つ)から受けたイメージで作った、
//   非公式の習作です。パビリオンの映像・音・文章・データは使っていません。年表の言葉は、このスケッチのために書いたものです。
//   M5Stack StopWatch 用(丸い画面)。CoreS3 系でも動きます(自動で切り替わります)。
//
// 画面の中:
//   ・まん中で黒いモノリスがゆっくり回り、いまの出来事を一字で光らせる。下に、いまの年と出来事が出る
//   ・約700万年前 → 火 → ことば → 文字 → 印刷 → 蒸気 → 電気 → 計算機 → ウェブ → 深層学習 → AI と話す → これから…
//     と、時代が進むほど流れが速くなる。最後は「記号を手放して、ヌルへ」。文字が粒になってほどけ、静かになって、またはじめから
//   ・「これから」の行は、どれも想像です(予言ではありません)
//
// 触ると:
//   ・触った所の文字が、粒になって散る。指でなぞると、なぞった所から散っていく
//   ・鏡の回廊では、床や壁の字に穴があく(しばらくすると戻る)。文字の塔では、字がはじかれて、ゆっくり元の場所に戻る
//
// 傾けると(本体の傾きセンサー):
//   ・鏡の回廊: のぞき込む角度が変わる。傾けた側(低い方)の壁が近づき、奥の出口がずれて見える
//   ・文字の塔: 塔が低い方へかたむき、回る速さも変わる
//   ・向きが逆のときは、調整用の TILT_FLIP_X / TILT_FLIP_Y / TILT_SWAP を変える。Serial に tilt: の値が出る
//
// 操作:
//   StopWatch … ボタンA 短押し: 次の出来事へ   ボタンA 長押し: 前の出来事へ   ボタンB: 見え方(鏡の回廊 / 文字の塔 / おまかせ)
//   CoreS3系  … 画面の右半分をダブルタップ: 次へ   左半分をダブルタップ: 前へ   電源ボタン短押し: 見え方
//
// 必要なもの:
//   ボードマネージャー M5Stack >= 3.3.7 / ボード: M5StopWatch(PSRAM は OPI)、CoreS3 なら M5CoreS3
//   ライブラリ M5Unified >= 0.2.16, M5GFX >= 0.2.21
//
// しくみ:
//   年表の行と塔の字は、起動したときに明朝体で絵にしておきます(PSRAM)。
//   鏡の回廊: 画面の点ごとに「目から見て、床(天井)と壁のどちらに先に当たるか」を出して、当たった所の奥行きで年表の絵を引きます。
//     床と天井は行ごとに奥行きが同じ、壁は列ごとに奥行きが同じなので、計算は軽くすみます。左右は鏡のように折り返して暗くしていきます。
//   文字の塔: 字を1つずつ、らせんの上の 3D の位置から画面に写して、大きさを変えて描きます(奥の字 → モノリス → 手前の字の順)。
//   モノリスは、上から見た四角を回して、見えている面の左右と上下を、画面の列ごとに出しておきます(床の鏡にも、それを逆さに使う)。
//   16 行ずつの帯にして、画面へ送ります。

#ifndef IRAM_ATTR
#define IRAM_ATTR
#endif


// ---- 調整用 ----------------------------------------------------------------
static constexpr int   NULL_MS    = 6000;      // 最後に「ヌル」へほどけていく時間
static constexpr int   QUIET_MS   = 1800;      // ほどけたあと、まっくらな時間
static constexpr float SPIN       = 0.45f;     // モノリス(と文字の塔)の回る速さ(rad/秒)
static constexpr int   START_VIEW = 0;         // はじめの見え方: 0 = 鏡の回廊 / 1 = 文字の塔 / 2 = おまかせ(1周ごとに入れかわる)
// ---- 傾き ----
static constexpr bool  TILT        = true;    // 傾きで動かすか(false にすると、傾きを使わない)
static constexpr float TILT_GAIN   = 1.0f;    // 傾きの効き方
static constexpr bool  TILT_FLIP_X = false;   // 左右が逆に動くときは true
static constexpr bool  TILT_FLIP_Y = false;   // 上下が逆に動くときは true
static constexpr bool  TILT_SWAP   = false;   // 左右と上下が入れかわっているときは true
static constexpr bool  TILT_STOPWATCH_FIX = true;   // StopWatch はセンサーが 90 度回ってついているので、自動で読みかえる
static constexpr float TILT_DEAD   = 0.08f;   // これより小さい傾きは無視する
static constexpr float TILT_RELEVEL = 15.0f;   // 同じ角度で持ち続けると、何秒くらいでそこを「水平」にするか(0 = 起動したときの姿勢のまま)
static constexpr float TILT_RELEVEL_MAX = 0.25f;   // これより大きく傾けているあいだは、水平を合わせなおさない(大きく傾けて戻すと逆に動くのを防ぐ)
// ---- 触ると散る ----
static constexpr int   SCATTER_N   = 26;      // 触ったときに飛び散る粒(字)の数
static constexpr float SCATTER_R   = 0.17f;   // 散る範囲(画面の大きさに対して)
// ---------------------------------------------------------------------------

// StopWatch では、画面へ「直接」送る(M5GFX の PSRAM の置き場を通さない)。画面が乱れるときは false に
static constexpr bool DIRECT_SEND = true;
#if defined(ESP_PLATFORM) && defined(LGFX_USE_QSPI) && __has_include(<lgfx/v1/panel/Panel_AMOLED.hpp>)
#define NL_DIRECT_AMOLED 1
struct NlFbPeek : public lgfx::Panel_AMOLED_Framebuffer {
  static lgfx::Panel_AMOLED* inner(lgfx::Panel_Device* p) {
    return static_cast<lgfx::Panel_AMOLED_Framebuffer*>(p)->*(&NlFbPeek::_panel);
  }
};
#endif

// ---- 年表(時代: 0 太古 / 1 古代 / 2 近代 / 3 計算機 / 4 AI / 5 これから / 6 ヌル)
struct Ent { const char* year; const char* text; const char* key; uint8_t era; };
static const Ent ENT[] = {
  {"約700万年前",  "人類の祖先が、二本の足で立つ", "立", 0},
  {"約260万年前",  "石を割って、道具にする",       "石", 0},
  {"約100万年前",  "火を使う",                     "火", 0},
  {"約30万年前",   "ホモ・サピエンスが現れる",     "人", 0},
  {"約7万年前",    "ことばで物語を語る",           "語", 0},
  {"約4万年前",    "洞窟に絵を描く",               "絵", 0},
  {"約1万年前",    "種をまき、村をつくる",         "農", 1},
  {"前3200年ごろ", "文字が生まれる",               "文", 1},
  {"前3000年ごろ", "車輪がまわる",                 "輪", 1},
  {"前500年ごろ",  "「なぜ」を問う",               "問", 1},
  {"105年",        "紙が広まる",                   "紙", 1},
  {"1450年ごろ",   "活版印刷",                     "印", 2},
  {"1543年",       "地球が太陽のまわりを回る",     "天", 2},
  {"1687年",       "万有引力",                     "力", 2},
  {"1769年",       "蒸気機関",                     "蒸", 2},
  {"1837年",       "電信で文字を送る",             "電", 2},
  {"1876年",       "電話で声を送る",               "声", 2},
  {"1879年",       "電球が夜を照らす",             "光", 2},
  {"1903年",       "人が空を飛ぶ",                 "翔", 2},
  {"1936年",       "計算する機械の理論",           "算", 3},
  {"1946年",       "電子計算機",                   "機", 3},
  {"1950年",       "機械は考えられるか",           "考", 3},
  {"1956年",       "「人工知能」ということば",     "知", 3},
  {"1969年",       "月に立つ・計算機がつながる",   "月", 3},
  {"1971年",       "マイクロプロセッサ",           "芯", 3},
  {"1991年",       "ウェブが世界をつなぐ",         "網", 3},
  {"1997年",       "機械がチェスで勝つ",           "駒", 3},
  {"2007年",       "手のひらの計算機",             "掌", 3},
  {"2012年",       "深層学習",                     "層", 4},
  {"2016年",       "機械が囲碁で勝つ",             "碁", 4},
  {"2017年",       "注意だけで学ぶモデル",         "注", 4},
  {"2022年",       "AI と話す",                    "話", 4},
  {"これから",     "AI が AI をつくる",            "創", 5},
  {"これから",     "ことばの壁が消える",           "通", 5},
  {"これから",     "身体をデジタルに写す",         "写", 5},
  {"これから",     "自分ではない自分と話す",       "鏡", 5},
  {"これから",     "記号を手放す",                 "捨", 5},
  {"そして",       "ヌルへ",                       "無", 6},
};
static constexpr int NENT = sizeof(ENT) / sizeof(ENT[0]);
static const uint16_t ERA_MS[7] = {3600, 3000, 2300, 1800, 1600, 3000, 3200};   // 1つの出来事にかける時間(時代が進むほど速く)
static const uint8_t  ERA_RGB[7][3] = {
  {255, 168,  90}, {250, 210, 130}, {235, 232, 240}, { 90, 215, 255}, {120, 160, 255}, {205, 140, 255}, {255, 255, 255} };
static const char* const VIEW_NAME[] = {"鏡の回廊", "文字の塔", "おまかせ"};
static const float SPEED_MUL[] = {0.55f, 1.0f, 1.7f};

// ---- 画面
static constexpr size_t nmOa0 = nmAlignTo(nmOv0, alignof(int)), nmOv1 = nmOa0 + sizeof(int); static int& W = *reinterpret_cast<int*>(NM_ARENA + nmOa0);
static constexpr size_t nmOa1 = nmAlignTo(nmOv1, alignof(int)), nmOv2 = nmOa1 + sizeof(int); static int& H = *reinterpret_cast<int*>(NM_ARENA + nmOa1);
static constexpr size_t nmOa2 = nmAlignTo(nmOv2, alignof(int)), nmOv3 = nmOa2 + sizeof(int); static int& S = *reinterpret_cast<int*>(NM_ARENA + nmOa2);
static constexpr size_t nmOa3 = nmAlignTo(nmOv3, alignof(int)), nmOv4 = nmOa3 + sizeof(int); static int& XOFF = *reinterpret_cast<int*>(NM_ARENA + nmOa3);
static constexpr size_t nmOa4 = nmAlignTo(nmOv4, alignof(int)), nmOv5 = nmOa4 + sizeof(int); static int& YOFF = *reinterpret_cast<int*>(NM_ARENA + nmOa4);
static constexpr size_t nmOa5 = nmAlignTo(nmOv5, alignof(int)), nmOv6 = nmOa5 + sizeof(int); static int& NB = *reinterpret_cast<int*>(NM_ARENA + nmOa5);
static constexpr size_t nmOa6 = nmAlignTo(nmOv6, alignof(int)), nmOv7 = nmOa6 + sizeof(int); static int& CX = *reinterpret_cast<int*>(NM_ARENA + nmOa6);
static constexpr size_t nmOa7 = nmAlignTo(nmOv7, alignof(int)), nmOv8 = nmOa7 + sizeof(int); static int& CY = *reinterpret_cast<int*>(NM_ARENA + nmOa7);
static bool roundScr = true;
static constexpr size_t nmOa8 = nmAlignTo(nmOv8, alignof(bool)), nmOv9 = nmOa8 + sizeof(bool); static bool& fbMode = *reinterpret_cast<bool*>(NM_ARENA + nmOa8);
static constexpr size_t nmOa9 = nmAlignTo(nmOv9, alignof(bool)), nmOv10 = nmOa9 + sizeof(bool); static bool& directSend = *reinterpret_cast<bool*>(NM_ARENA + nmOa9);
static constexpr int BAND = 16;
static int16_t* spanA = nullptr;                  // 行ごとの見える左右(丸い画面)
static int16_t* spanB = nullptr;
static int16_t* bandX = nullptr;                  // 帯ごとの左はし(偶数)と幅(偶数)
static int16_t* bandW = nullptr;
static constexpr int NSLOT = 4;                   // 帯の置き場の数(2つのコアで、送るのを待たずに先へ描けるように)
static uint16_t* band[NSLOT] = {nullptr, nullptr, nullptr, nullptr};
static SemaphoreHandle_t semGo = nullptr, semDone = nullptr;
static volatile int readyW = -1, doneUpTo = -1;   // コア0が描き終えた帯 / 画面へ送り終えた帯
static uint32_t busyUs[2] = {0, 0}, sendUs = 0;

// 内蔵メモリに置く(足りなければ、どこでも)
static void* imalloc(size_t n) {
  void* p = heap_caps_malloc(n, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  return p ? p : malloc(n);
}

// ---- 年表の絵(明朝体で、1行 RH 点の高さ)
static constexpr int TW = 512, RH = 26;
static uint8_t* tex = nullptr;                    // [NENT * RH][TW]  0..15 の濃さ(上4ビット: にじみ / 下4ビット: 字)
static constexpr size_t nmOa10 = nmAlignTo(nmOv10, alignof(int)), nmOv11 = nmOa10 + sizeof(int); static int& texRows = *reinterpret_cast<int*>(NM_ARENA + nmOa10);

// ---- モノリスの字と、下の説明
static constexpr int KG = 64;                     // モノリスの字の大きさ
static uint8_t* keyTex = nullptr;                 // [KG][KG] 0..15
static int CAPW = 320, CAPH = 64;
static uint8_t* capTex = nullptr;                 // [CAPH][CAPW] 上4ビット: 影 / 下4ビット: 字
static constexpr size_t nmOa11 = nmAlignTo(nmOv11, alignof(int)), nmOv12 = nmOa11 + sizeof(int); static int& capX = *reinterpret_cast<int*>(NM_ARENA + nmOa11);
static constexpr size_t nmOa12 = nmAlignTo(nmOv12, alignof(int)), nmOv13 = nmOa12 + sizeof(int); static int& capY = *reinterpret_cast<int*>(NM_ARENA + nmOa12);

// ---- 色の表: [明るさ 0..31][濃さ 0..15](時代ごとに作りなおす)
static constexpr size_t nmOa13 = nmAlignTo(nmOv13, 16), nmOv14 = nmOa13 + sizeof(uint16_t[7][32][16]); static uint16_t (&lutEra)[7][32][16] = *reinterpret_cast<uint16_t(*)[7][32][16]>(NM_ARENA + nmOa13);
static constexpr size_t nmOa14 = nmAlignTo(nmOv14, 16), nmOv15 = nmOa14 + sizeof(float[256]); static float (&keyK)[256] = *reinterpret_cast<float(*)[256]>(NM_ARENA + nmOa14);  // モノリスの字の1点 → 明るさ
static constexpr size_t nmOa15 = nmAlignTo(nmOv15, 16), nmOv16 = nmOa15 + sizeof(uint8_t[256]); static uint8_t (&aIdx)[256] = *reinterpret_cast<uint8_t(*)[256]>(NM_ARENA + nmOa15);  // 字の絵の1点(上4ビット: にじみ / 下4ビット: 字)→ 色の番号 0..15
static constexpr size_t nmOa16 = nmAlignTo(nmOv16, 16), nmOv17 = nmOa16 + sizeof(uint8_t[64]); static uint8_t (&entEraT)[64] = *reinterpret_cast<uint8_t(*)[64]>(NM_ARENA + nmOa16);  // 出来事の番号 → 時代(内蔵メモリに写したもの)
static constexpr size_t nmOa17 = nmAlignTo(nmOv17, alignof(int)), nmOv18 = nmOa17 + sizeof(int); static int& colPeak = *reinterpret_cast<int*>(NM_ARENA + nmOa17);  // 壁までの奥行きがいちばん深い列(まん中あたり)

// ---- 進み方
static constexpr size_t nmOa18 = nmAlignTo(nmOv18, alignof(int)), nmOv19 = nmOa18 + sizeof(int); static int& cur = *reinterpret_cast<int*>(NM_ARENA + nmOa18);  // いまの出来事
static constexpr size_t nmOa19 = nmAlignTo(nmOv19, alignof(float)), nmOv20 = nmOa19 + sizeof(float); static float& tIn = *reinterpret_cast<float*>(NM_ARENA + nmOa19);  // その出来事に入ってからの時間(ms)
static constexpr size_t nmOa20 = nmAlignTo(nmOv20, alignof(int)), nmOv21 = nmOa20 + sizeof(int); static int& phase = *reinterpret_cast<int*>(NM_ARENA + nmOa20);  // 0 = 年表 / 1 = ほどける / 2 = まっくら
static constexpr size_t nmOa21 = nmAlignTo(nmOv21, alignof(float)), nmOv22 = nmOa21 + sizeof(float); static float& phaseT = *reinterpret_cast<float*>(NM_ARENA + nmOa21);
static int speedIdx = 1;  // 流れの速さ(SPEED_MUL の番号)
static constexpr size_t nmOa22 = nmAlignTo(nmOv22, alignof(float)), nmOv23 = nmOa22 + sizeof(float); static float& capFade = *reinterpret_cast<float*>(NM_ARENA + nmOa22);  // 下の説明の明るさ(0..1)
static int shownCap = -1, shownKey = -1;
static constexpr size_t nmOa23 = nmAlignTo(nmOv23, alignof(uint32_t)), nmOv24 = nmOa23 + sizeof(uint32_t); static uint32_t& frameNo = *reinterpret_cast<uint32_t*>(NM_ARENA + nmOa23);
static constexpr size_t nmOa24 = nmAlignTo(nmOv24, 16), nmOv25 = nmOa24 + sizeof(char[24]); static char (&flashMsg)[24] = *reinterpret_cast<char(*)[24]>(NM_ARENA + nmOa24);
static constexpr size_t nmOa25 = nmAlignTo(nmOv25, alignof(uint32_t)), nmOv26 = nmOa25 + sizeof(uint32_t); static uint32_t& flashUntil = *reinterpret_cast<uint32_t*>(NM_ARENA + nmOa25);

// ---------------------------------------------------------------- 道具
static inline __attribute__((always_inline)) uint16_t sw565(int r, int g, int b) {
  r = r < 0 ? 0 : r > 255 ? 255 : r; g = g < 0 ? 0 : g > 255 ? 255 : g; b = b < 0 ? 0 : b > 255 ? 255 : b;
  return (uint16_t)__builtin_bswap16((uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)));
}
static inline __attribute__((always_inline)) uint16_t mix565(uint16_t p, uint16_t q, uint32_t a) {   // a: 0..32
  uint32_t x = __builtin_bswap16(p), y = __builtin_bswap16(q);
  x = (x | (x << 16)) & 0x07E0F81Fu; y = (y | (y << 16)) & 0x07E0F81Fu;
  uint32_t r = ((x * (32 - a) + y * a) >> 5) & 0x07E0F81Fu;
  return __builtin_bswap16((uint16_t)(r | (r >> 16)));
}
static inline __attribute__((always_inline)) uint32_t hash32(uint32_t x) { x ^= x >> 16; x *= 0x7feb352d; x ^= x >> 15; x *= 0x846ca68b; x ^= x >> 16; return x; }
static int entMs(int e) { return (int)(ERA_MS[ENT[e].era] / SPEED_MUL[speedIdx]); }

static void fail(const char* msg) {
  M5.Display.setTextDatum(middle_center);
  M5.Display.drawString(msg, M5.Display.width() / 2, M5.Display.height() / 2);
  Serial.println(msg);
  while (true) nmIdle();
}

// ---------------------------------------------------------------- 字を絵にする
// canvas(16ビット)に描いた字を、0..15 の濃さにする(2x2 をまとめて、なめらかに)
static void grabAA(M5Canvas& cv, int N2w, int N2h, uint8_t* dst, int dw, int dh, int dstride, bool flipY) {
  const uint16_t* b = (const uint16_t*)cv.getBuffer();
  for (int y = 0; y < dh; ++y)
    for (int x = 0; x < dw; ++x) {
      int k = 0;
      if (2 * y + 1 < N2h && 2 * x + 1 < N2w)
        k = (b[(2 * y) * N2w + 2 * x] != 0) + (b[(2 * y) * N2w + 2 * x + 1] != 0)
          + (b[(2 * y + 1) * N2w + 2 * x] != 0) + (b[(2 * y + 1) * N2w + 2 * x + 1] != 0);
      dst[(flipY ? dh - 1 - y : y) * dstride + x] = (uint8_t)(k * 15 / 4);
    }
}
// 字のまわりに、うすいにじみ(上4ビット)を足す
static void addGlow(uint8_t* p, int w, int h, int stride, int rad) {
  uint8_t* tmp = (uint8_t*)malloc((size_t)w * h);
  if (!tmp) return;
  for (int y = 0; y < h; ++y)
    for (int x = 0; x < w; ++x) {
      int a = 0;
      for (int d = -rad; d <= rad; ++d) { int xx = x + d; if (xx >= 0 && xx < w) a += p[y * stride + xx] & 15; }
      tmp[y * w + x] = (uint8_t)min(255, a);
    }
  for (int y = 0; y < h; ++y)
    for (int x = 0; x < w; ++x) {
      int a = 0;
      for (int d = -rad; d <= rad; ++d) { int yy = y + d; if (yy >= 0 && yy < h) a += tmp[yy * w + x]; }
      int g = a * 3 / ((2 * rad + 1) * (2 * rad + 1));
      if (g > 15) g = 15;
      p[y * stride + x] = (uint8_t)((p[y * stride + x] & 15) | (g << 4));
    }
  free(tmp);
}

// 年表の行を、明朝体で1枚の絵に描く
static bool buildTimeline() {
  texRows = NENT * RH;
  tex = (uint8_t*)ps_malloc((size_t)TW * texRows);
  if (!tex) tex = (uint8_t*)malloc((size_t)TW * texRows);
  if (!tex) return false;
  memset(tex, 0, (size_t)TW * texRows);
  M5Canvas cv(&M5.Display);
  cv.setColorDepth(16);
  cv.setPsram(true);
  if (!cv.createSprite(TW * 2, RH * 2)) return false;
  for (int e = 0; e < NENT; ++e) {
    char line[128];
    snprintf(line, sizeof(line), "%s  %s", ENT[e].year, ENT[e].text);
    cv.fillSprite(0);
    cv.setFont(&fonts::lgfxJapanMincho_20);
    cv.setTextSize(2.0f);
    cv.setTextColor(0xFFFF);
    cv.setTextDatum(middle_center);
    cv.drawString(line, TW, RH);
    grabAA(cv, TW * 2, RH * 2, tex + (size_t)e * RH * TW, TW, RH, TW, false);
  }
  addGlow(tex, TW, texRows, TW, 2);
  cv.deleteSprite();
  return true;
}

// モノリスの字(出来事が変わったときに描きなおす)
static M5Canvas* work = nullptr;                  // 字を描く紙(PSRAM)
static void buildKey(int e) {
  work->fillSprite(0);
  work->setFont(&fonts::lgfxJapanMincho_20);
  work->setTextSize(KG * 2 / 22.0f);
  work->setTextColor(0xFFFF);
  work->setTextDatum(middle_center);
  work->drawString(ENT[e].key, KG, KG + 2);
  grabAA(*work, KG * 2, KG * 2, keyTex, KG, KG, KG, false);
  addGlow(keyTex, KG, KG, KG, 3);
  shownKey = e;
}
// 下の説明(年と出来事)
static M5Canvas* capCv = nullptr;
static void buildCap(int e) {
  capCv->fillSprite(0);
  capCv->setTextColor(0xFFFF);
  capCv->setTextDatum(middle_center);
  capCv->setTextSize(2.0f);
  capCv->setFont(&fonts::lgfxJapanGothicP_16);
  capCv->drawString(ENT[e].year, CAPW, 20);
  capCv->setFont(S >= 400 ? &fonts::lgfxJapanGothicP_20 : &fonts::lgfxJapanGothicP_16);
  const char* t = (flashUntil && (int32_t)(millis() - flashUntil) < 0) ? flashMsg : ENT[e].text;
  capCv->drawString(t, CAPW, 82);
  grabAA(*capCv, CAPW * 2, CAPH * 2, capTex, CAPW, CAPH, CAPW, false);
  addGlow(capTex, CAPW, CAPH, CAPW, 3);
  shownCap = e;
}

// 色の表(時代ごと): 明るさ b(0..31)、濃さ a(上: にじみ / 下: 字)
static void buildLuts() {
  for (int er = 0; er < 7; ++er) {
    const float r = ERA_RGB[er][0], g = ERA_RGB[er][1], bl = ERA_RGB[er][2];
    for (int b = 0; b < 32; ++b)
      for (int a = 0; a < 16; ++a) {
        const float k = b / 31.0f;
        const float core = (a & 15) / 15.0f;
        // 字は色、白に少し寄せる。ない所は、うっすら床の色
        const float base = 0.035f * k;
        const float lr = r * core * k + 255 * core * core * k * 0.25f + 40 * base;
        const float lg = g * core * k + 255 * core * core * k * 0.25f + 34 * base;
        const float lb = bl * core * k + 255 * core * core * k * 0.25f + 60 * base;
        lutEra[er][b][a] = sw565((int)lr, (int)lg, (int)lb);
      }
  }
}

// ---------------------------------------------------------------- 1フレーム(共通)
static constexpr size_t nmOa26 = nmAlignTo(nmOv26, alignof(float)), nmOv27 = nmOa26 + sizeof(float); static float& scrollZ = *reinterpret_cast<float*>(NM_ARENA + nmOa26);  // 年表がどこまで流れてきたか(奥行きの単位)
static constexpr size_t nmOa27 = nmAlignTo(nmOv27, alignof(float)), nmOv28 = nmOa27 + sizeof(float); static float& dissolve = *reinterpret_cast<float*>(NM_ARENA + nmOa27);  // 0..1(ほどける)
static float worldFade = 1;  // 全体の明るさ
static constexpr size_t nmOa28 = nmAlignTo(nmOv28, alignof(int)), nmOv29 = nmOa28 + sizeof(int); static int& viewMode = *reinterpret_cast<int*>(NM_ARENA + nmOa28);  // 見え方: 0 = 鏡の回廊 / 1 = 文字の塔
static constexpr size_t nmOa29 = nmAlignTo(nmOv29, alignof(int)), nmOv30 = nmOa29 + sizeof(int); static int& viewSel = *reinterpret_cast<int*>(NM_ARENA + nmOa29);  // ボタンで選んだもの: 0 回廊 / 1 塔 / 2 おまかせ(1周ごとに入れかわる)
static float FOC = 1;  // 画面の点 / (横 ÷ 奥行き)
static constexpr size_t nmOa30 = nmAlignTo(nmOv30, alignof(float)), nmOv31 = nmOa30 + sizeof(float); static float& camY = *reinterpret_cast<float*>(NM_ARENA + nmOa30);  // 目の高さ
static constexpr size_t nmOa31 = nmAlignTo(nmOv31, alignof(float)), nmOv32 = nmOa31 + sizeof(float); static float& camX = *reinterpret_cast<float*>(NM_ARENA + nmOa31);  // 目の横の位置(傾けるとずれる)
static constexpr size_t nmOa32 = nmAlignTo(nmOv32, alignof(float)), nmOv33 = nmOa32 + sizeof(float); static float& VX = *reinterpret_cast<float*>(NM_ARENA + nmOa32);
static constexpr size_t nmOa33 = nmAlignTo(nmOv33, alignof(float)), nmOv34 = nmOa33 + sizeof(float); static float& VY = *reinterpret_cast<float*>(NM_ARENA + nmOa33);  // 奥の消える点(傾けるとずれる)
static constexpr size_t nmOa34 = nmAlignTo(nmOv34, alignof(bool)), nmOv35 = nmOa34 + sizeof(bool); static bool& imuOK = *reinterpret_cast<bool*>(NM_ARENA + nmOa34);
static constexpr size_t nmOa35 = nmAlignTo(nmOv35, alignof(float)), nmOv36 = nmOa35 + sizeof(float); static float& tiltX = *reinterpret_cast<float*>(NM_ARENA + nmOa35);
static constexpr size_t nmOa36 = nmAlignTo(nmOv36, alignof(float)), nmOv37 = nmOa36 + sizeof(float); static float& tiltY = *reinterpret_cast<float*>(NM_ARENA + nmOa36);
static constexpr size_t nmOa37 = nmAlignTo(nmOv37, alignof(float)), nmOv38 = nmOa37 + sizeof(float); static float& levelX = *reinterpret_cast<float*>(NM_ARENA + nmOa37);
static constexpr size_t nmOa38 = nmAlignTo(nmOv38, alignof(float)), nmOv39 = nmOa38 + sizeof(float); static float& levelY = *reinterpret_cast<float*>(NM_ARENA + nmOa38);
static constexpr size_t nmOa39 = nmAlignTo(nmOv39, alignof(int)), nmOv40 = nmOa39 + sizeof(int); static int& levelN = *reinterpret_cast<int*>(NM_ARENA + nmOa39);
static float frameDt = 0.033f;  // 1フレームの時間(秒)
// 触った所
static constexpr size_t nmOa40 = nmAlignTo(nmOv40, alignof(bool)), nmOv41 = nmOa40 + sizeof(bool); static bool& touching = *reinterpret_cast<bool*>(NM_ARENA + nmOa40);
static constexpr size_t nmOa41 = nmAlignTo(nmOv41, alignof(float)), nmOv42 = nmOa41 + sizeof(float); static float& touchX = *reinterpret_cast<float*>(NM_ARENA + nmOa41);
static constexpr size_t nmOa42 = nmAlignTo(nmOv42, alignof(float)), nmOv43 = nmOa42 + sizeof(float); static float& touchY = *reinterpret_cast<float*>(NM_ARENA + nmOa42);
static constexpr size_t nmOa43 = nmAlignTo(nmOv43, alignof(float)), nmOv44 = nmOa43 + sizeof(float); static float& holeA = *reinterpret_cast<float*>(NM_ARENA + nmOa43);  // 回廊の「穴」の大きさ(0..1)
static constexpr size_t nmOa44 = nmAlignTo(nmOv44, alignof(float)), nmOv45 = nmOa44 + sizeof(float); static float& holeX = *reinterpret_cast<float*>(NM_ARENA + nmOa44);
static constexpr size_t nmOa45 = nmAlignTo(nmOv45, alignof(float)), nmOv46 = nmOa45 + sizeof(float); static float& holeY = *reinterpret_cast<float*>(NM_ARENA + nmOa45);
// 飛び散る字
struct Part { float x, y, vx, vy, life, size; uint8_t g, era; };
static constexpr int MAXPART = 160;
static Part* part = nullptr;
static constexpr size_t nmOa46 = nmAlignTo(nmOv46, alignof(int)), nmOv47 = nmOa46 + sizeof(int); static int& nPart = *reinterpret_cast<int*>(NM_ARENA + nmOa46);

static inline __attribute__((always_inline)) uint16_t add565(uint16_t p, uint16_t q) {     // 明るさを足す(あふれたら白で止める)
  uint32_t a = __builtin_bswap16(p), b = __builtin_bswap16(q);
  uint32_t r = min<uint32_t>(31, (a >> 11) + (b >> 11)), g = min<uint32_t>(63, ((a >> 5) & 63) + ((b >> 5) & 63)), bl = min<uint32_t>(31, (a & 31) + (b & 31));
  return __builtin_bswap16((uint16_t)((r << 11) | (g << 5) | bl));
}
static inline __attribute__((always_inline)) uint16_t scale565(uint16_t p, uint32_t k) {   // k: 0..32
  return mix565(0, p, k);
}
// 出来事 e の明るさ(いま = 明るい / これから = うすい / 過ぎたもの = だんだん暗く)
static inline float entBright(int e) {
  if (e == cur) return 1.0f;
  if (e > cur) return 0.30f;
  return 0.48f * powf(0.62f, (float)(cur - e - 1));
}

// ---- モノリス(黒い石の板)。上から見た四角を回して、見えている面を、画面の列ごとに出しておく
struct MonoCol { int16_t yTop, yBot; uint8_t face; uint8_t on; float u, lit, spec, ih;     // ih = 1 / (yBot - yTop)
                 float r0, g0, b0, dr, dg, db, kf; int16_t gx; };                          // 列ごとに先に計算しておく色(上の端 / 下へ1あたり)
static MonoCol* mcol = nullptr;                   // [S]
static int monoX1 = -1;
static constexpr size_t nmOa47 = nmAlignTo(nmOv47, alignof(int)), nmOv48 = nmOa47 + sizeof(int); static int& monoX0 = *reinterpret_cast<int*>(NM_ARENA + nmOa47);
static constexpr size_t nmOa48 = nmAlignTo(nmOv48, alignof(float)), nmOv49 = nmOa48 + sizeof(float); static float& monoAng = *reinterpret_cast<float*>(NM_ARENA + nmOa48);
static void prepMonolith(float cxW, float cyW, float czW, float hw, float hh, float hd) {
  monoX0 = S; monoX1 = -1;
  for (int x = 0; x < S; ++x) mcol[x].on = 0;
  if (phase == 2) return;
  const float c = cosf(monoAng), s = sinf(monoAng);
  const float lx[4] = {-hw, hw, hw, -hw}, lz[4] = {-hd, -hd, hd, hd};
  float X[4], Z[4];
  for (int i = 0; i < 4; ++i) { X[i] = cxW + lx[i] * c + lz[i] * s; Z[i] = czW - lx[i] * s + lz[i] * c; }
  const float yT = cyW + hh, yB = cyW - hh;
  for (int f = 0; f < 4; ++f) {
    const int i0 = f, i1 = (f + 1) & 3;
    // 外向きの法線と、目から面への向き
    const float ex = X[i1] - X[i0], ez = Z[i1] - Z[i0];
    const float nx = ez, nz = -ex;                // (回り方の向きで外向き)
    const float mx = (X[i0] + X[i1]) * 0.5f - camX, mz = (Z[i0] + Z[i1]) * 0.5f;
    if (nx * mx + nz * mz >= 0) continue;         // 裏を向いている
    const float sx0 = VX + (X[i0] - camX) / Z[i0] * FOC, sx1 = VX + (X[i1] - camX) / Z[i1] * FOC;
    const bool rev = sx1 < sx0;
    const float xa = rev ? sx1 : sx0, xb = rev ? sx0 : sx1;
    const float iza = 1.0f / (rev ? Z[i1] : Z[i0]), izb = 1.0f / (rev ? Z[i0] : Z[i1]);
    const float ua = rev ? 1.0f : 0.0f, ub = rev ? 0.0f : 1.0f;
    const float len = sqrtf(nx * nx + nz * nz), cosi = fabsf(nx * mx + nz * mz) / (len * sqrtf(mx * mx + mz * mz));
    const bool wide = (f == 0 || f == 2);
    for (int x = max(0, (int)ceilf(xa)); x <= min(S - 1, (int)floorf(xb)); ++x) {
      const float t = (xb > xa) ? (x - xa) / (xb - xa) : 0;
      const float iz = iza + (izb - iza) * t;
      const float u = (ua * iza + (ub * izb - ua * iza) * t) / iz;
      MonoCol& m = mcol[x];
      m.on = 1; m.face = wide ? (f == 0 ? 0 : 2) : 1;
      m.u = u; m.lit = 0.25f + 0.75f * cosi;
      // つやの光の帯(回ると面の上をすべる)
      const float sp = u - (0.5f + 0.9f * sinf(monoAng * 1.3f + f));
      m.spec = expf(-sp * sp * 30.0f) * (1.0f - cosi) * 0.9f + (wide ? 0 : 0.05f);
      m.yTop = (int16_t)(VY - (yT - camY) * iz * FOC);
      m.yBot = (int16_t)(VY - (yB - camY) * iz * FOC);
      m.ih = 1.0f / max(1, (int)(m.yBot - m.yTop));
      // 色: shade = (0.035 + 0.05 * (1 - vy)) * lit = lit * 0.085 - lit * 0.05 * vy
      {
        const float s0 = 0.085f * m.lit, ds = -0.05f * m.lit;
        const float ue = min(m.u, 1.0f - m.u);
        float add = ue < 0.025f ? 1.0f : 0.0f;                       // ふちの光(縦のふち)
        float w = 0;
        if (phase == 1) w = dissolve * dissolve * 255;
        m.r0 = s0 * 120 + m.spec * 70 + add * 80 + w;  m.dr = ds * 120;
        m.g0 = s0 * 120 + m.spec * 75 + add * 85 + w;  m.dg = ds * 120;
        m.b0 = s0 * 150 + m.spec * 95 + add * 100 + w; m.db = ds * 150;
        m.kf = m.lit * (1.0f + dissolve * 2.0f);
        const int gx = (int)(m.u * KG);
        m.gx = (int16_t)((m.face != 1 && gx >= 0 && gx < KG) ? gx : -1);
      }
      monoX0 = min(monoX0, x); monoX1 = max(monoX1, x);
    }
  }
}
// モノリスの点の色(なければ 0 を返して false)
static inline __attribute__((always_inline)) bool monoPixel(int x, int y, uint16_t* out) {
  const MonoCol& m = mcol[x];
  if (!m.on || y < m.yTop || y > m.yBot) return false;
  const float vy = (float)(y - m.yTop) * m.ih;
  float R = m.r0 + m.dr * vy, G = m.g0 + m.dg * vy, B = m.b0 + m.db * vy;
  if (m.gx >= 0) {
    const int gy = (int)((vy - 0.18f) * 2.2f * KG);
    if (gy >= 0 && gy < KG) {
      const float k = keyK[keyTex[gy * KG + m.gx]] * m.kf;
      const uint8_t* e = ERA_RGB[ENT[cur].era];
      R += e[0] * k; G += e[1] * k; B += e[2] * k;
    }
  }
  if (y - m.yTop < 2) { R += 80; G += 85; B += 100; }   // ふちの光(上のふち)
  *out = sw565((int)(R * worldFade), (int)(G * worldFade), (int)(B * worldFade));
  return true;
}

// ---------------------------------------------------------------- 見え方A: 鏡の回廊
// 床 y=-1、天井 y=+1、左右の壁 x=±WALL。どの面も年表が流れる。床に映るモノリスも描く
static constexpr float WALL = 1.7f;
static float* colZ = nullptr;                     // [S] 列ごとの「壁までの奥行き」
static uint8_t* colFog = nullptr;                 // [S] その奥行きの明るさ(0..31)
static uint8_t* colSeam = nullptr;                // [S] 壁の、奥行きのつなぎ目の光(0..15)
static int16_t* colU = nullptr;                   // [S] 壁の年表の、横の位置(列ごとに同じ)
static int16_t* colEnt = nullptr;                 // [S] 壁の年表の、出来事の番号のずれ
static constexpr float CORR_MONO_Z = 3.5f;
static void prepCorridor() {
  for (int x = 0; x < S; ++x) {
    const float px = (x + 0.5f - VX) / FOC;
    const float z = px > 1e-4f ? (WALL - camX) / px : px < -1e-4f ? (WALL + camX) / -px : 1e9f;
    colZ[x] = z;
    colFog[x] = (uint8_t)min(31.0f, 31.0f * expf(-z * 0.10f) * 0.55f * worldFade);
    const float g = fabsf(fmodf(z + 0.25f, 0.5f) - 0.25f);
    colSeam[x] = (uint8_t)(15.0f * expf(-(g / 0.012f) * (g / 0.012f)) * expf(-z * 0.10f) * worldFade);
    const float wz = (fminf(z, 1000.0f) + scrollZ) * 52.0f;
    const int wi = (int)floorf(wz);
    colU[x] = (int16_t)(wi & (TW - 1));
    colEnt[x] = (int16_t)((((wi >> 9) * 3) % NENT + NENT) % NENT);      // TW = 512 = 2^9
  }
  colPeak = 0;
  for (int x = 1; x < S; ++x) if (colZ[x] > colZ[colPeak]) colPeak = x;
}
static void IRAM_ATTR corridorRowRef(int y, uint16_t* o, int bx, int sa, int sb) {
  const float py = (VY - (y + 0.5f)) / FOC;      // 上が +
  const bool floorRow = py < 0;
  float zr = 1e9f;
  if (py < -1e-4f) zr = (-1.0f - camY) / py;
  else if (py > 1e-4f) zr = (1.0f - camY) / py;
  // 床・天井の行: 出来事と行の中の高さ
  const float zz = zr + scrollZ;
  const float rowf = zz / 0.62f;
  const int ent = (int)floorf(rowf);
  const float fr = rowf - ent;
  const bool entOk = zr < 60 && ent >= 0 && ent < NENT;
  const uint8_t* src = entOk ? tex + ((size_t)ent * RH + (int)((1.0f - fr) * (RH - 1))) * TW : nullptr;
  const int era = entOk ? ENT[ent].era : 0;
  const float fogR = expf(-zr * 0.10f);
  int lvl[6];
  { float k = entOk ? entBright(ent) * 1.25f * fogR * worldFade : 0; for (int i = 0; i < 6; ++i) { lvl[i] = (int)min(31.0f, k * 31.0f); k *= 0.70f; } }
  const float zg = fabsf(fmodf(zr + 0.25f, 0.5f) - 0.25f);
  const int rowSeam = (int)(15.0f * expf(-(zg / 0.012f) * (zg / 0.012f)) * fogR * worldFade);
  // 横: 世界の x(16.16)
  const float zrc = fminf(zr, 1000.0f);
  const int32_t xw0 = (int32_t)((((sa + 0.5f - VX) / FOC) * zrc + camX) * 65536.0f), dxw = (int32_t)(zrc / FOC * 65536.0f);
  const bool glitch = era == 5 && (hash32((uint32_t)y * 977u + (frameNo / 3) * 131u) & 15) == 0;
  const uint32_t th2 = (uint32_t)(dissolve * dissolve * 255.0f);
  const uint16_t (*L)[16] = lutEra[era];
  const uint16_t seamC[2] = {sw565(18, 22, 34), sw565(36, 44, 68)};
  int32_t xw = xw0;
  int32_t uf = xw0 * 100 + ((TW * 3 / 10 + (glitch ? 40 : 0)) << 16);   // 年表の横の位置(16.16)
  const int32_t duf = dxw * 100;
  const float pyw = py, camW = camY + 1.0f;
  for (int x = sa; x <= sb; ++x, xw += dxw, uf += duf) {
    uint16_t p;
    const float zw = colZ[x];
    if (zr <= zw) {                               // 床か天井
      if (!src) { p = 0; }
      else {
        const int32_t u = uf >> 16;
        const int32_t m = u >> 9;                 // TW = 512 → 何枚目の鏡か(左はマイナス)
        int uu = u & (TW - 1); if (m & 1) uu = TW - 1 - uu;
        const int am = m < 0 ? -m : m;
        uint8_t a = src[uu];
        if (th2 && (hash32((uint32_t)(x * 7919 + y * 104729) ^ frameNo) & 255) < th2) a = 0;
        const int core = a & 15, gl = a >> 4;
        p = L[am < 6 ? lvl[am] : 0][core > (gl >> 1) ? core : (gl >> 1)];
      }
      // パネルのつなぎ目(横方向)
      const uint32_t fx = (uint32_t)(xw + 0x4000) & 0x7FFF;   // 0.5 ごと
      const uint32_t dx = fx < 0x4000 ? fx : 0x8000 - fx;
      int seam = rowSeam;
      if (dx < 900) seam += (int)(12 * fogR * worldFade);
      if (seam > 0) p = add565(p, seamC[seam > 10]);
      // 床に映るモノリス
      if (floorRow && x >= monoX0 && x <= monoX1 && mcol[x].on && y > mcol[x].yBot) {
        uint16_t mp;
        const int ym = 2 * mcol[x].yBot - y;
        if (ym >= mcol[x].yTop && monoPixel(x, ym, &mp)) p = add565(p, scale565(mp, 13));
      }
    } else {                                      // 左右の壁(鏡)
      const float yw1 = pyw * zw + camW;          // 床からの高さ
      const float wy = yw1 * (1.0f / 0.34f) + 64.0f;
      const int wi = (int)wy;
      const float wfr = wy - wi;
      int went = (wi - 64) * 5 + colEnt[x];
      while (went >= NENT) went -= NENT;
      while (went < 0) went += NENT;
      const float yw = yw1 - 1.0f;
      uint8_t a = tex[((size_t)went * RH + (int)((1.0f - wfr) * (RH - 1))) * TW + colU[x]];
      if (th2 && (hash32((uint32_t)(x * 7919 + y * 104729) ^ frameNo) & 255) < th2) a = 0;
      const int core = a & 15, gl = a >> 4;
      p = lutEra[ENT[went].era][colFog[x]][core > (gl >> 1) ? core : (gl >> 1)];
      int seam = colSeam[x];
      const float g2 = yw * 2.0f + 64.5f, gy = fabsf(g2 - (int)g2 - 0.5f);   // 0.5 ごとのつなぎ目までの距離(×2)
      if (gy < 0.024f) seam += colSeam[x] > 0 ? 12 : (int)(12 * colFog[x] / 17);
      if (seam > 0) p = add565(p, seamC[seam > 10]);
    }
    o[x - bx] = p;
  }
}

// 速い版: 1行を「左の壁 / 床か天井 / 右の壁」に分ける。床は鏡1枚ぶんずつまとめて描く。
// 消えていくとき(粒つぶに消える)だけは、上のゆっくり版を使う
static void IRAM_ATTR corridorRow(int y, uint16_t* o, int bx, int sa, int sb) {
  if (dissolve > 0.0f && (uint32_t)(dissolve * dissolve * 255.0f) != 0) { corridorRowRef(y, o, bx, sa, sb); return; }
  const float py = (VY - (y + 0.5f)) / FOC;      // 上が +
  const bool floorRow = py < 0;
  float zr = 1e9f;
  if (py < -1e-4f) zr = (-1.0f - camY) / py;
  else if (py > 1e-4f) zr = (1.0f - camY) / py;
  // 床か天井になる列の範囲 [fa, fb](壁までの奥行きは、まん中がいちばん深く、外へいくほど浅い)
  int fa = sb + 1, fb = sb;
  {
    const int pk = colPeak;
    if (colZ[pk] >= zr && zr < 1e8f) {
      int lo = sa, hi = min(sb, pk);              // 左側: x が増えると深くなる。zr <= colZ になる最初の列
      if (lo <= hi) {
        if (colZ[hi] < zr) lo = hi + 1;
        else { while (lo < hi) { const int mid = (lo + hi) >> 1; if (colZ[mid] >= zr) hi = mid; else lo = mid + 1; } }
        fa = lo;
      } else fa = sa;
      lo = max(sa, pk); hi = sb;                  // 右側: x が増えると浅くなる。zr <= colZ になる最後の列
      if (lo <= hi) {
        if (colZ[lo] < zr) hi = lo - 1;
        else { while (lo < hi) { const int mid = (lo + hi + 1) >> 1; if (colZ[mid] >= zr) lo = mid; else hi = mid - 1; } }
        fb = hi;
      } else fb = sb;
      if (fa < sa) fa = sa;
      if (fb > sb) fb = sb;
    }
  }
  // ---- 左右の壁(鏡)
  {
    const uint16_t seamC[2] = {sw565(18, 22, 34), sw565(36, 44, 68)};
    const float pyw = py, camW = camY + 1.0f;
    for (int side = 0; side < 2; ++side) {
      const int xa = side == 0 ? sa : max(fb + 1, sa), xb = side == 0 ? min(fa - 1, sb) : sb;
      for (int x = xa; x <= xb; ++x) {
        const float zw = colZ[x];
        const float yw1 = pyw * zw + camW;
        const float wy = yw1 * (1.0f / 0.34f) + 64.0f;
        const int wi = (int)wy;
        const float wfr = wy - wi;
        int went = (wi - 64) * 5 + colEnt[x];
        while (went >= NENT) went -= NENT;
        while (went < 0) went += NENT;
        const float yw = yw1 - 1.0f;
        const uint8_t a = tex[((size_t)went * RH + (int)((1.0f - wfr) * (RH - 1))) * TW + colU[x]];
        uint16_t p = lutEra[entEraT[went]][colFog[x]][aIdx[a]];
        int seam = colSeam[x];
        const float g2 = yw * 2.0f + 64.5f, gy = fabsf(g2 - (int)g2 - 0.5f);
        if (gy < 0.024f) seam += colSeam[x] > 0 ? 12 : (int)(12 * colFog[x] / 17);
        if (seam > 0) p = add565(p, seamC[seam > 10]);
        o[x - bx] = p;
      }
    }
  }
  if (fa > fb) return;
  // ---- 床か天井
  const float zz = zr + scrollZ;
  const float rowf = zz / 0.62f;
  const int ent = (int)floorf(rowf);
  const float fr = rowf - ent;
  const bool entOk = zr < 60 && ent >= 0 && ent < NENT;
  const uint8_t* src = entOk ? tex + ((size_t)ent * RH + (int)((1.0f - fr) * (RH - 1))) * TW : nullptr;
  const int era = entOk ? ENT[ent].era : 0;
  const float fogR = expf(-zr * 0.10f);
  int lvl[6];
  { float k = entOk ? entBright(ent) * 1.25f * fogR * worldFade : 0; for (int i = 0; i < 6; ++i) { lvl[i] = (int)min(31.0f, k * 31.0f); k *= 0.70f; } }
  const float zg = fabsf(fmodf(zr + 0.25f, 0.5f) - 0.25f);
  const int rowSeam = (int)(15.0f * expf(-(zg / 0.012f) * (zg / 0.012f)) * fogR * worldFade);
  const float zrc = fminf(zr, 1000.0f);
  const int32_t xw0 = (int32_t)((((sa + 0.5f - VX) / FOC) * zrc + camX) * 65536.0f), dxw = (int32_t)(zrc / FOC * 65536.0f);
  const bool glitch = era == 5 && (hash32((uint32_t)y * 977u + (frameNo / 3) * 131u) & 15) == 0;
  const uint16_t (*L)[16] = lutEra[era];
  const uint16_t seamC[2] = {sw565(18, 22, 34), sw565(36, 44, 68)};
  const int32_t uf0 = xw0 * 100 + ((TW * 3 / 10 + (glitch ? 40 : 0)) << 16);
  const int32_t duf = dxw * 100;
  uint16_t* const ob = o - bx;
  if (!src) { for (int x = fa; x <= fb; ++x) ob[x] = 0; }
  else {
    int64_t uf = (int64_t)uf0 + (int64_t)(fa - sa) * duf;
    int x = fa;
    while (x <= fb) {
      const int32_t u = (int32_t)(uf >> 16);
      const int32_t m = u >> 9;                   // 何枚目の鏡か(左はマイナス)
      // この鏡が終わるまでの点の数
      int n = fb - x + 1;
      if (duf > 0) {
        const int64_t edge = (int64_t)(m + 1) << 25;   // u が (m+1)*512 になる uf
        const int64_t k = (edge - uf + duf - 1) / duf;
        if (k < n) n = (int)k;
        if (n < 1) n = 1;
      }
      const int am = m < 0 ? -m : m;
      const uint16_t* Lm = L[am < 6 ? lvl[am] : 0];
      const int base = (int)(m << 9);
      uint16_t* d = ob + x;
      if (m & 1) { for (int i = 0; i < n; ++i, uf += duf) d[i] = Lm[aIdx[src[TW - 1 - ((int32_t)(uf >> 16) - base)]]]; }
      else       { for (int i = 0; i < n; ++i, uf += duf) d[i] = Lm[aIdx[src[(int32_t)(uf >> 16) - base]]]; }
      x += n;
    }
  }
  // パネルのつなぎ目: 横(この行ぜんぶ = rowSeam)と、縦(0.5 ごと = vs)。縦の上は「横 + 縦」の明るさで1回だけ足す
  const int vs = (int)(12 * fogR * worldFade);
  if (rowSeam > 0 || vs > 0) {
    auto cdiv = [](int64_t a, int64_t b) { int64_t q = a / b; if ((a % b != 0) && ((a > 0) == (b > 0))) ++q; return q; };
    auto fdiv = [](int64_t a, int64_t b) { int64_t q = a / b; if ((a % b != 0) && ((a > 0) != (b > 0))) --q; return q; };
    const uint16_t cRow = seamC[rowSeam > 10], cBoth = seamC[rowSeam + vs > 10];
    int x = fa;
    if (dxw > 0) {
      // xw(x) = xw0 + (x - sa) * dxw。縦のつなぎ目は xw = 0x4000 + k * 0x8000 から 899 以内
      const int64_t xwA = (int64_t)xw0 + (int64_t)(fa - sa) * dxw, xwB = (int64_t)xw0 + (int64_t)(fb - sa) * dxw;
      const int64_t k0 = fdiv(xwA - 0x4000 - 899, 0x8000), k1 = cdiv(xwB - 0x4000 + 899, 0x8000);
      for (int64_t k = k0; k <= k1; ++k) {
        const int64_t c = 0x4000 + k * 0x8000;
        int xl = sa + (int)cdiv(c - 899 - xw0, dxw), xh = sa + (int)fdiv(c + 899 - xw0, dxw);
        if (xl < x) xl = x;
        if (xh > fb) xh = fb;
        if (xl > xh) continue;
        if (rowSeam > 0) for (; x < xl; ++x) ob[x] = add565(ob[x], cRow);   // ここまでは横だけ
        x = xl;
        for (; x <= xh; ++x) ob[x] = add565(ob[x], cBoth);                  // 縦(と横)
      }
    }
    if (rowSeam > 0) for (; x <= fb; ++x) ob[x] = add565(ob[x], cRow);
  }
  // 床に映るモノリス
  if (floorRow && monoX1 >= monoX0) {
    const int xa = max(fa, monoX0), xb = min(fb, monoX1);
    for (int x = xa; x <= xb; ++x) {
      if (!mcol[x].on || y <= mcol[x].yBot) continue;
      uint16_t mp;
      const int ym = 2 * mcol[x].yBot - y;
      if (ym >= mcol[x].yTop && monoPixel(x, ym, &mp)) ob[x] = add565(ob[x], scale565(mp, 13));
    }
  }
}

// ---------------------------------------------------------------- 見え方B: 文字の塔
// 年表の文字が、モノリスのまわりをらせんになって回りながら昇る。床は鏡
static constexpr int GA = 32;                     // 字の絵の大きさ
static uint8_t*  glyphAtlas = nullptr;            // [NG][GA][GA](上4ビット: にじみ / 下4ビット: 字)
static uint32_t* glyphCp = nullptr;               // その字の文字コード
static constexpr size_t nmOa49 = nmAlignTo(nmOv49, alignof(int)), nmOv50 = nmOa49 + sizeof(int); static int& NG = *reinterpret_cast<int*>(NM_ARENA + nmOa49);
static uint16_t* tChar = nullptr;                 // 塔に並べる字(字の番号)
static uint8_t*  tEnt = nullptr;                  // その字の出来事
static constexpr size_t nmOa50 = nmAlignTo(nmOv50, alignof(int)), nmOv51 = nmOa50 + sizeof(int); static int& NT = *reinterpret_cast<int*>(NM_ARENA + nmOa50);
static int*      entFirst = nullptr;              // 出来事 e の最初の字の番号
static float*    tOffX = nullptr;                 // 字ごとの、はじかれたずれ(画面の点)と速さ
static float*    tOffY = nullptr;
static float*    tVelX = nullptr;
static float*    tVelY = nullptr;
struct Sprite { int16_t x, y, s; uint8_t g, era, lvl, back; };
static Sprite*   spr = nullptr;
static constexpr size_t nmOa51 = nmAlignTo(nmOv51, alignof(int)), nmOv52 = nmOa51 + sizeof(int); static int& NS = *reinterpret_cast<int*>(NM_ARENA + nmOa51);
static constexpr int MAXSPR = 420;
static constexpr size_t nmOa52 = nmAlignTo(nmOv52, alignof(float)), nmOv53 = nmOa52 + sizeof(float); static float& towerRot = *reinterpret_cast<float*>(NM_ARENA + nmOa52);
static constexpr size_t nmOa53 = nmAlignTo(nmOv53, alignof(float)), nmOv54 = nmOa53 + sizeof(float); static float& towerRise = *reinterpret_cast<float*>(NM_ARENA + nmOa53);
static constexpr float TOWER_R = 1.55f, TOWER_Z = 3.7f, TOWER_DY = 0.0118f, TOWER_DA = 0.24f, TOWER_FLOOR = -1.25f, TOWER_CH = 0.165f;

static uint32_t utf8Next(const char*& p) {
  const unsigned char c = (unsigned char)*p;
  if (c < 0x80) { ++p; return c; }
  if (c < 0xE0) { uint32_t v = ((c & 0x1F) << 6) | (p[1] & 0x3F); p += 2; return v; }
  if (c < 0xF0) { uint32_t v = ((c & 0x0F) << 12) | ((p[1] & 0x3F) << 6) | (p[2] & 0x3F); p += 3; return v; }
  uint32_t v = ((c & 0x07) << 18) | ((p[1] & 0x3F) << 12) | ((p[2] & 0x3F) << 6) | (p[3] & 0x3F); p += 4; return v;
}
static bool buildTower() {
  // 並べる字: 「年 + 出来事 + ・」を全部つなげたもの
  static const int MAXT = 600, MAXG = 260;
  tChar = (uint16_t*)imalloc(MAXT * 2); tEnt = (uint8_t*)imalloc(MAXT);
  glyphCp = (uint32_t*)malloc(MAXG * 4); entFirst = (int*)malloc(NENT * sizeof(int));
  spr = (Sprite*)imalloc(MAXSPR * sizeof(Sprite));
  tOffX = (float*)calloc(MAXT, sizeof(float)); tOffY = (float*)calloc(MAXT, sizeof(float));
  tVelX = (float*)calloc(MAXT, sizeof(float)); tVelY = (float*)calloc(MAXT, sizeof(float));
  if (!tChar || !tEnt || !glyphCp || !entFirst || !spr || !tOffX || !tOffY || !tVelX || !tVelY) return false;
  NT = 0; NG = 0;
  for (int e = 0; e < NENT; ++e) {
    entFirst[e] = NT;
    char line[128]; snprintf(line, sizeof(line), "%s%s・", ENT[e].year, ENT[e].text);
    for (const char* p = line; *p && NT < MAXT;) {
      const uint32_t cp = utf8Next(p);
      if (cp == ' ' || cp == 0x3000) continue;
      int g = -1;
      for (int i = 0; i < NG; ++i) if (glyphCp[i] == cp) { g = i; break; }
      if (g < 0) { if (NG >= MAXG) continue; glyphCp[NG] = cp; g = NG++; }
      tChar[NT] = (uint16_t)g; tEnt[NT] = (uint8_t)e; ++NT;
    }
  }
  glyphAtlas = (uint8_t*)ps_malloc((size_t)NG * GA * GA);
  if (!glyphAtlas) glyphAtlas = (uint8_t*)malloc((size_t)NG * GA * GA);
  if (!glyphAtlas) return false;
  M5Canvas cv(&M5.Display);
  cv.setColorDepth(16); cv.setPsram(true);
  if (!cv.createSprite(GA * 2, GA * 2)) return false;
  for (int g = 0; g < NG; ++g) {
    char s[8] = {0}; const uint32_t c = glyphCp[g];
    if (c < 0x80) s[0] = (char)c;
    else if (c < 0x800) { s[0] = (char)(0xC0 | (c >> 6)); s[1] = (char)(0x80 | (c & 0x3F)); }
    else { s[0] = (char)(0xE0 | (c >> 12)); s[1] = (char)(0x80 | ((c >> 6) & 0x3F)); s[2] = (char)(0x80 | (c & 0x3F)); }
    cv.fillSprite(0);
    cv.setFont(&fonts::lgfxJapanMincho_20);
    cv.setTextSize(GA * 2 * 0.80f / 20.0f);
    cv.setTextColor(0xFFFF); cv.setTextDatum(middle_center);
    cv.drawString(s, GA, GA);
    uint8_t* d = glyphAtlas + (size_t)g * GA * GA;
    grabAA(cv, GA * 2, GA * 2, d, GA, GA, GA, false);
    addGlow(d, GA, GA, GA, 2);
  }
  cv.deleteSprite();
  return true;
}
// 字をどこに置くか(毎フレーム)。奥から順に並べる
static void prepTower() {
  NS = 0;
  const float kc = entFirst[cur] + (float)(entFirst[min(cur + 1, NENT - 1)] - entFirst[cur]) * min(1.0f, tIn / (float)entMs(cur));
  towerRise = kc;
  const uint32_t th2 = (uint32_t)(dissolve * dissolve * 255.0f);
  for (int k = 0; k < NT && NS < MAXSPR; ++k) {
    const float yv = (k - kc) * TOWER_DY - 0.10f;
    if (yv < TOWER_FLOOR + 0.02f || yv > 2.4f) continue;
    if (th2 && (hash32((uint32_t)k * 2654435761u ^ (frameNo / 2)) & 255) < th2) continue;   // ほどけて消える
    const float a = k * TOWER_DA + towerRot;
    const float x = TOWER_R * sinf(a), z = TOWER_Z - TOWER_R * cosf(a);
    const float iz = 1.0f / z;
    const float lean = (yv + 0.1f) * tiltX * 0.35f;     // 傾けた方へ、上ほどずれる
    float fsx = VX + (x + lean - camX) * iz * FOC, fsy = VY - (yv - camY) * iz * FOC;
    // 触られた字は、はじかれて、ゆっくり戻る
    if (touching) {
      const float dx = fsx + tOffX[k] - touchX, dy = fsy + tOffY[k] - touchY, d2 = dx * dx + dy * dy, R = SCATTER_R * S;
      if (d2 < R * R) { const float d = sqrtf(d2) + 1.0f, f = (1.0f - d / R) * 2600.0f * frameDt; tVelX[k] += dx / d * f; tVelY[k] += dy / d * f; }
    }
    if (tOffX[k] != 0 || tOffY[k] != 0 || tVelX[k] != 0 || tVelY[k] != 0) {
      tVelX[k] = (tVelX[k] - tOffX[k] * 9.0f * frameDt) * (1.0f - 2.2f * frameDt);
      tVelY[k] = (tVelY[k] - tOffY[k] * 9.0f * frameDt) * (1.0f - 2.2f * frameDt);
      tOffX[k] += tVelX[k] * frameDt; tOffY[k] += tVelY[k] * frameDt;
      if (fabsf(tOffX[k]) + fabsf(tOffY[k]) + fabsf(tVelX[k]) + fabsf(tVelY[k]) < 0.3f) tOffX[k] = tOffY[k] = tVelX[k] = tVelY[k] = 0;
    }
    const int sx = (int)(fsx + tOffX[k]), sy = (int)(fsy + tOffY[k]);
    const int s = (int)(TOWER_CH * iz * FOC);
    if (s < 4) continue;
    const float facing = fabsf(cosf(a));          // 横を向いている字は、うすく
    const int te = tEnt[k];
    const float eb = te == cur ? 1.35f : te > cur ? 0.42f : fmaxf(0.30f, 0.85f * powf(0.90f, (float)(cur - te - 1)));   // 塔は、過ぎた出来事もしばらく見えている
    float k2 = expf(-(z - 2.3f) * 0.22f) * (0.35f + 0.65f * facing) * eb * worldFade;
    const int lv = (int)min(31.0f, k2 * 31.0f);
    if (lv <= 0) continue;
    Sprite& q = spr[NS++];
    q.x = (int16_t)sx; q.y = (int16_t)sy; q.s = (int16_t)s; q.g = (uint8_t)tChar[k]; q.era = ENT[tEnt[k]].era; q.lvl = (uint8_t)lv;
    q.back = z >= TOWER_Z ? 1 : 0;
    // 床の鏡に映る字
    if (NS < MAXSPR) {
      const float yr = 2 * TOWER_FLOOR - yv;
      Sprite& r = spr[NS++];
      r = q; r.x = (int16_t)(VX + (x - lean - camX) * iz * FOC); r.y = (int16_t)(VY - (yr - camY) * iz * FOC); r.lvl = (uint8_t)(lv * 0.30f); r.back = 2;
      if (r.lvl == 0) --NS;
    }
  }
  // 奥から(z が大きい = 小さい字から)描くように、大きさで並べる
  for (int i = 1; i < NS; ++i) { Sprite t = spr[i]; int j = i; while (j > 0 && spr[j - 1].s > t.s) { spr[j] = spr[j - 1]; --j; } spr[j] = t; }
}
static void IRAM_ATTR drawSprites(int pass, int y0, int h, uint16_t* out, int bx, int bw) {
  for (int i = 0; i < NS; ++i) {
    const Sprite& q = spr[i];
    if ((pass == 0 && q.back == 0) || (pass == 1 && q.back != 0)) continue;   // 0: 奥と映り / 1: 手前
    const int half = q.s / 2;
    const int ya = max(y0, q.y - half), yb = min(y0 + h - 1, q.y - half + q.s - 1);
    if (ya > yb) continue;
    const int xa = max(bx, q.x - half), xb = min(bx + bw - 1, q.x - half + q.s - 1);
    if (xa > xb) continue;
    const uint8_t* g = glyphAtlas + (size_t)q.g * GA * GA;
    const uint16_t* L = lutEra[q.era][q.lvl];
    const int step = (GA << 16) / q.s;
    const bool flip = q.back == 2;                // 映りは上下が逆
    for (int y = ya; y <= yb; ++y) {
      int gy = ((y - (q.y - half)) * step) >> 16; if (flip) gy = GA - 1 - gy;
      const uint8_t* gr = g + gy * GA;
      uint16_t* o = out + (y - y0) * bw - bx;
      int gx16 = (xa - (q.x - half)) * step;
      for (int x = xa; x <= xb; ++x, gx16 += step) {
        const uint8_t a = gr[gx16 >> 16];
        if (!a) continue;
        const int core = a & 15, gl = a >> 4;
        o[x] = add565(o[x], L[core > (gl >> 1) ? core : (gl >> 1)]);
      }
    }
  }
}
static void IRAM_ATTR towerFloorRow(int y, uint16_t* o, int bx, int sa, int sb) {
  const float py = (VY - (y + 0.5f)) / FOC;
  if (py >= -1e-4f) { for (int x = sa; x <= sb; ++x) o[x - bx] = 0; return; }
  const float z = (TOWER_FLOOR - camY) / py;
  const float fog = expf(-z * 0.12f) * worldFade;
  const float zg = fabsf(fmodf(z + 0.25f, 0.5f) - 0.25f);
  const bool rowSeam = zg < 0.012f;
  const uint16_t sc = sw565((int)(36 * fog), (int)(44 * fog), (int)(68 * fog));
  int32_t xw = (int32_t)((((sa + 0.5f - VX) / FOC) * z + camX) * 65536.0f);
  const int32_t dxw = (int32_t)(z / FOC * 65536.0f);
  for (int x = sa; x <= sb; ++x, xw += dxw) {
    const uint32_t fx = (uint32_t)(xw + 0x4000) & 0x7FFF;
    const uint32_t dx = fx < 0x4000 ? fx : 0x8000 - fx;
    uint16_t p = (rowSeam || dx < 800) ? sc : 0;
    if (x >= monoX0 && x <= monoX1 && mcol[x].on && y > mcol[x].yBot) {          // 床に映るモノリス
      uint16_t mp; const int ym = 2 * mcol[x].yBot - y;
      if (ym >= mcol[x].yTop && monoPixel(x, ym, &mp)) p = add565(p, scale565(mp, 11));
    }
    o[x - bx] = p;
  }
}

// ---------------------------------------------------------------- 触ると散る
static void spawnParts(float x, float y, int n) {
  if (!NT) return;
  const int a = entFirst[cur], b = cur + 1 < NENT ? entFirst[cur + 1] : NT;
  for (int i = 0; i < n; ++i) {
    if (nPart >= MAXPART) { // いちばん古いものと入れかえ
      for (int j = 1; j < nPart; ++j) part[j - 1] = part[j];
      --nPart;
    }
    Part& p = part[nPart++];
    const float ang = (hash32(frameNo * 131u + i * 977u) & 1023) / 1023.0f * 6.2831853f;
    const float sp = (90.0f + (hash32(frameNo * 71u + i * 313u) & 255)) * S / 466.0f;
    p.x = x; p.y = y; p.vx = cosf(ang) * sp; p.vy = sinf(ang) * sp - 40.0f;
    p.life = 1.0f; p.size = (18.0f + (hash32(i * 17u + frameNo) & 15)) * S / 466.0f;
    const int k = a + (int)(hash32(frameNo * 7u + i) % (uint32_t)max(1, b - a));
    p.g = (uint8_t)tChar[min(k, NT - 1)]; p.era = ENT[cur].era;
  }
}
static void stepParts(float dt) {
  int j = 0;
  for (int i = 0; i < nPart; ++i) {
    Part p = part[i];
    p.life -= dt / 1.4f;
    if (p.life <= 0) continue;
    p.x += p.vx * dt; p.y += p.vy * dt;
    p.vx *= 1.0f - 1.2f * dt; p.vy = p.vy * (1.0f - 1.2f * dt) + 30.0f * dt;   // ゆっくり止まって、少し落ちる
    p.size *= 1.0f - 0.35f * dt;
    part[j++] = p;
  }
  nPart = j;
}
static void IRAM_ATTR drawParts(int y0, int h, uint16_t* out, int bx, int bw) {
  for (int i = 0; i < nPart; ++i) {
    const Part& p = part[i];
    const int s = (int)p.size, half = s / 2;
    if (s < 3) continue;
    const int ya = max(y0, (int)p.y - half), yb = min(y0 + h - 1, (int)p.y - half + s - 1);
    if (ya > yb) continue;
    const int xa = max(bx, (int)p.x - half), xb = min(bx + bw - 1, (int)p.x - half + s - 1);
    if (xa > xb) continue;
    const uint8_t* g = glyphAtlas + (size_t)p.g * GA * GA;
    const int lv = (int)min(31.0f, 31.0f * p.life * 1.3f * worldFade);
    const uint16_t* L = lutEra[p.era][lv];
    const int step = (GA << 16) / s;
    for (int y = ya; y <= yb; ++y) {
      const uint8_t* gr = g + (((y - ((int)p.y - half)) * step) >> 16) * GA;
      uint16_t* o = out + (y - y0) * bw - bx;
      int gx16 = (xa - ((int)p.x - half)) * step;
      for (int x = xa; x <= xb; ++x, gx16 += step) {
        const uint8_t a = gr[gx16 >> 16];
        if (!a) continue;
        const int core = a & 15, gl = a >> 4;
        o[x] = add565(o[x], L[core > (gl >> 1) ? core : (gl >> 1)]);
      }
    }
  }
}
// 回廊: 触った所の字が散って、穴があいたように暗くなる
static void IRAM_ATTR holeRow(int y, uint16_t* o, int bx, int sa, int sb) {
  if (holeA < 0.02f) return;
  const float R = SCATTER_R * S * (0.6f + 0.4f * holeA);
  const float dy = y - holeY;
  if (fabsf(dy) >= R) return;
  const float hw = sqrtf(R * R - dy * dy);
  const int xa = max(sa, (int)(holeX - hw)), xb = min(sb, (int)(holeX + hw));
  const float iR2 = 1.0f / (R * R), dy2 = dy * dy;
  for (int x = xa; x <= xb; ++x) {
    const float dx = x - holeX, d2 = (dx * dx + dy2) * iR2;   // (まん中からの距離 / R)² … 0(まん中)..1(ふち)
    const float k = holeA * (1.0f - d2);
    o[x - bx] = mix565(o[x - bx], 0, (uint32_t)(k * 30.0f));
  }
}

// ---------------------------------------------------------------- 帯を描く
static void IRAM_ATTR renderBand(int b, uint16_t* out, int bx, int bw) {
  const int y0 = b * BAND, h = min(BAND, S - y0);
  const int era = ENT[cur].era;
  for (int y = y0; y < y0 + h; ++y) {
    uint16_t* o = out + (y - y0) * bw;
    const int sa = max((int)spanA[y], bx), sb = min((int)spanB[y], bx + bw - 1);
    if (sa > sb) { memset(o, 0, bw * 2); continue; }
    if (sa > bx) memset(o, 0, (sa - bx) * 2);
    if (sb < bx + bw - 1) memset(o + (sb - bx + 1), 0, (bx + bw - 1 - sb) * 2);
    if (viewMode == 0) { corridorRow(y, o, bx, sa, sb); holeRow(y, o, bx, sa, sb); }
    else towerFloorRow(y, o, bx, sa, sb);
  }
  if (viewMode == 1) drawSprites(0, y0, h, out, bx, bw);   // 奥の字と、床に映った字
  // モノリス
  for (int y = y0; y < y0 + h; ++y) {
    uint16_t* o = out + (y - y0) * bw;
    const int sa = max((int)spanA[y], max(bx, monoX0)), sb = min((int)spanB[y], min(bx + bw - 1, monoX1));
    for (int x = sa; x <= sb; ++x) { uint16_t mp; if (monoPixel(x, y, &mp)) o[x - bx] = mp; }
  }
  if (viewMode == 1) drawSprites(1, y0, h, out, bx, bw);   // 手前の字
  drawParts(y0, h, out, bx, bw);                           // 飛び散る字
  // 下の説明
  if (capFade > 0.01f && y0 + h > capY && y0 < capY + CAPH) {
    const uint16_t tc = sw565(ERA_RGB[era][0], ERA_RGB[era][1], ERA_RGB[era][2]);
    const uint32_t under = (uint32_t)(18 * capFade);
    for (int y = max(y0, capY); y < min(y0 + h, capY + CAPH); ++y) {
      uint16_t* o = out + (y - y0) * bw;
      const uint8_t* cp = capTex + (y - capY) * CAPW;
      const int sa = max((int)spanA[y], max(bx, capX)), sb = min((int)spanB[y], min(bx + bw - 1, capX + CAPW - 1));
      for (int x = sa; x <= sb; ++x) {
        const uint8_t a = cp[x - capX];
        uint16_t p = mix565(o[x - bx], 0, under);
        if (a) {
          const int shadow = (int)((a >> 4) * 1.6f * capFade), core = (int)((a & 15) * 2.1f * capFade);
          if (shadow) p = mix565(p, 0, min(28, shadow));
          if (core) p = mix565(p, tc, min(32, core));
        }
        o[x - bx] = p;
      }
    }
  }
}

static void renderFrame() {
  // 見え方ごとの準備
  if (viewMode == 0) {
    camX = -tiltX * 0.75f;                        // 低い方の壁へ寄る
    camY = -0.05f - tiltY * 0.35f;
    VX = CX - tiltX * S * 0.16f; VY = CY - tiltY * S * 0.12f;
    prepCorridor();
    prepMonolith(0.0f, -1.0f + 0.95f, CORR_MONO_Z, 0.40f, 0.95f, 0.10f);
  } else {
    camX = -tiltX * 0.35f; camY = 0.25f - tiltY * 0.25f;
    VX = CX - tiltX * S * 0.06f; VY = CY - tiltY * S * 0.06f;
    prepTower();
    prepMonolith(0.0f, TOWER_FLOOR + 1.1f, TOWER_Z, 0.33f, 1.1f, 0.08f);
  }
  // コア0 に奇数番目の帯をまかせて、こちら(コア1)は偶数番目を描きながら、順番に画面へ送る
  __atomic_store_n(&readyW, -1, __ATOMIC_RELEASE);
  __atomic_store_n(&doneUpTo, -1, __ATOMIC_RELEASE);
  xSemaphoreGive(semGo);
  uint32_t busy = 0, send = 0;
  if (!fbMode) M5.Display.startWrite();
  for (int k = 0; k < NB; ++k) {
    if ((k & 1) == 0) {
      if (bandW[k] > 0) { const uint32_t t0 = micros(); renderBand(k, band[k % NSLOT], bandX[k], bandW[k]); busy += micros() - t0; }
    } else {
      while (__atomic_load_n(&readyW, __ATOMIC_ACQUIRE) < k) { }
    }
    if (!fbMode) { const uint32_t tw = micros(); M5.Display.waitDMA(); send += micros() - tw; }   // 前の帯を送り終わるまで待つ
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
// コア0: 奇数番目の帯
static void workerTask(void*) {
  for (;;) {
    xSemaphoreTake(semGo, portMAX_DELAY);
    uint32_t busy = 0;
    for (int k = 1; k < NB; k += 2) {
      while (__atomic_load_n(&doneUpTo, __ATOMIC_ACQUIRE) < k - NSLOT) { }   // 同じ置き場の前の帯を送り終わるまで待つ
      if (bandW[k] > 0) { const uint32_t t0 = micros(); renderBand(k, band[k % NSLOT], bandX[k], bandW[k]); busy += micros() - t0; }
      __atomic_store_n(&readyW, k, __ATOMIC_RELEASE);
    }
    busyUs[0] = busy;
    xSemaphoreGive(semDone);
  }
}

// ---------------------------------------------------------------- 進み方
static void gotoEnt(int e) {
  if (e < 0) e = 0;
  if (e >= NENT) e = NENT - 1;
  cur = e; tIn = 0; phase = 0; phaseT = 0; dissolve = 0; worldFade = 1;
  capFade = 0;
  Serial.printf("# %s  %s\n", ENT[cur].year, ENT[cur].text);
}
static void setView(int v) {
  viewSel = v;
  viewMode = v == 2 ? viewMode : v;
  snprintf(flashMsg, sizeof(flashMsg), "見え方: %s", VIEW_NAME[viewSel]);
  flashUntil = (millis() + 1600) | 1;
  capFade = 1;
}
static void update(float dtMs) {
  monoAng += SPIN * dtMs * 0.001f * (phase == 1 ? 1.0f + dissolve * 6.0f : 1.0f);
  towerRot -= SPIN * (0.55f + tiltX * 0.9f) * dtMs * 0.001f;   // 傾けると、回る速さも変わる
  if (phase == 0) {
    tIn += dtMs;
    const float d = (float)entMs(cur);
    if (tIn >= d) {
      if (cur + 1 < NENT) { cur++; tIn -= d; capFade = 0; Serial.printf("# %s  %s\n", ENT[cur].year, ENT[cur].text); }
      else { phase = 1; phaseT = 0; tIn = d; }
    }
    capFade = min(1.0f, capFade + dtMs / 450.0f);
    if (tIn > d - 250) capFade = max(0.0f, min(capFade, (d - tIn) / 250.0f));
  } else if (phase == 1) {                        // ほどけて、ヌルへ
    phaseT += dtMs;
    dissolve = min(1.0f, phaseT / NULL_MS);
    worldFade = 1.0f - max(0.0f, (dissolve - 0.7f) / 0.3f);
    capFade = max(0.0f, 1.0f - dissolve * 2.0f);
    if (phaseT >= NULL_MS) { phase = 2; phaseT = 0; worldFade = 0; }
  } else {                                        // まっくら → はじめから
    phaseT += dtMs;
    worldFade = 0;
    if (phaseT >= QUIET_MS) {
      if (viewSel == 2) viewMode ^= 1;            // おまかせ: 1周ごとに入れかわる
      gotoEnt(0); worldFade = 0; phase = 0;
    }
  }
  if (phase == 0 && worldFade < 1) worldFade = min(1.0f, worldFade + dtMs / 1500.0f);
  const float prog = phase == 0 ? min(1.0f, tIn / (float)entMs(cur)) : 1.0f;
  scrollZ = (cur + prog) * 0.62f - CORR_MONO_Z;
  if (shownKey != cur) buildKey(cur);
  const bool flashing = flashUntil && (int32_t)(millis() - flashUntil) < 0;
  static bool wasFlash = false;
  if (shownCap != cur || flashing != wasFlash) { buildCap(cur); wasFlash = flashing; }
}

// ---------------------------------------------------------------- setup / loop
void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);
  Serial.begin(115200);
  Serial.println("NullChronicle v7");
#if defined(ESP_PLATFORM)
  if (getCpuFrequencyMhz() < 240) setCpuFrequencyMhz(240);
  Serial.printf("cpu %d MHz\n", (int)getCpuFrequencyMhz());
#endif
  if (M5.Display.width() < M5.Display.height()) M5.Display.setRotation(1);
  W = M5.Display.width(); H = M5.Display.height();
  roundScr = (W == H);
  S = min(W, H) & ~1;
  XOFF = ((W - S) / 2) & ~1; YOFF = (H - S) / 2;
  CX = S / 2; CY = S / 2;
  NB = (S + BAND - 1) / BAND;
  FOC = (S * 0.5f) / 0.85f;
  VX = CX; VY = CY;
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setBrightness(200);
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
  for (int i = 0; i < NSLOT; ++i) band[i] = (uint16_t*)heap_caps_malloc((size_t)S * BAND * 2, MALLOC_CAP_DMA);
  CAPW = (roundScr ? min(320, S - 40) : S) & ~1; CAPH = 64;
  capTex = (uint8_t*)imalloc((size_t)CAPW * CAPH);
  keyTex = (uint8_t*)imalloc(KG * KG);
  mcol = (MonoCol*)imalloc(S * sizeof(MonoCol));
  colZ = (float*)imalloc(S * sizeof(float)); colFog = (uint8_t*)imalloc(S); colSeam = (uint8_t*)imalloc(S);
  colU = (int16_t*)imalloc(S * 2); colEnt = (int16_t*)imalloc(S * 2);
  if (!spanA || !spanB || !bandX || !bandW || !band[0] || !band[1] || !band[2] || !band[3] || !capTex || !keyTex || !mcol || !colZ || !colFog || !colSeam || !colU || !colEnt) fail("Memory error");
  const float c = (S - 1) * 0.5f, rr = S * 0.5f + 1.0f;
  for (int y = 0; y < S; ++y) {
    if (!roundScr) { spanA[y] = 0; spanB[y] = S - 1; continue; }
    const float dy = y - c, w = rr * rr - dy * dy;
    if (w <= 0) { spanA[y] = 1; spanB[y] = 0; continue; }
    const float s = sqrtf(w);
    spanA[y] = (int16_t)max(0, (int)floorf(c - s)); spanB[y] = (int16_t)min(S - 1, (int)ceilf(c + s));
  }
  for (int b = 0; b < NB; ++b) {
    int x0 = S, x1 = -1;
    for (int y = b * BAND; y < min(S, (b + 1) * BAND); ++y) if (spanA[y] <= spanB[y]) { x0 = min(x0, (int)spanA[y]); x1 = max(x1, (int)spanB[y]); }
    if (x1 < x0) { bandX[b] = 0; bandW[b] = 0; continue; }
    x0 &= ~1; x1 |= 1; if (x1 > S - 1) x1 = S - 1;
    bandX[b] = (int16_t)x0; bandW[b] = (int16_t)((x1 - x0 + 1) & ~1);
  }
  capX = (S - CAPW) / 2;
  capY = roundScr ? (int)(S * 0.76f) : S - CAPH - 2;

  M5.Display.setTextDatum(middle_center);
  M5.Display.setFont(&fonts::lgfxJapanGothicP_16);
  M5.Display.setTextColor(0x8410);
  M5.Display.drawString("年表を用意しています…", W / 2, H / 2);
  const uint32_t t0 = millis();
  for (int a = 0; a < 256; ++a) { const int core = a & 15, gl = a >> 4; aIdx[a] = (uint8_t)(core > (gl >> 1) ? core : (gl >> 1)); }
  for (int e = 0; e < NENT && e < 64; ++e) entEraT[e] = ENT[e].era;
  for (int a = 0; a < 256; ++a) keyK[a] = (a & 15) / 15.0f * 1.25f + (a >> 4) / 15.0f * 0.55f;
  if (!buildTimeline()) fail("Memory error (timeline)");
  if (!buildTower()) fail("Memory error (tower)");
  part = (Part*)imalloc(MAXPART * sizeof(Part));
  if (!part) fail("Memory error (parts)");
  imuOK = TILT && M5.Imu.isEnabled();
  Serial.printf("tilt sensor: %s\n", imuOK ? "ok" : "not used");
  work = new M5Canvas(&M5.Display);
  work->setColorDepth(16); work->setPsram(true);
  capCv = new M5Canvas(&M5.Display);
  capCv->setColorDepth(16); capCv->setPsram(true);
  if (!work->createSprite(KG * 2, KG * 2) || !capCv->createSprite(CAPW * 2, CAPH * 2)) fail("Memory error (canvas)");
  buildLuts();
  Serial.printf("timeline: %d events, tower %d chars / %d kinds, %u ms\n", NENT, NT, NG, (unsigned)(millis() - t0));
  viewSel = START_VIEW; viewMode = START_VIEW == 1 ? 1 : 0;
  gotoEnt(0);
  worldFade = 0;
  M5.Display.fillScreen(TFT_BLACK);
  semGo = xSemaphoreCreateBinary();
  semDone = xSemaphoreCreateBinary();
  if (!semGo || !semDone) fail("Memory error (task)");
  xTaskCreatePinnedToCore(workerTask, "chron", 6144, nullptr, 1, nullptr, 0);   // コア0 で帯を描く
}

void loop() {
  static uint32_t last = millis(), fpsT = millis(), lastTapMs = 0;
  static int frames = 0;
  M5.update();
  int go = 0; bool sw = false;
  auto t = M5.Touch.getDetail();
  const bool corner = !roundScr && t.x < 56 && t.y < 56;   // 左上のすみはメニューに戻る所なので使わない
  if (roundScr) {
    if (M5.BtnA.wasHold()) go = -1;
    else if (M5.BtnA.wasClicked()) go = 1;
    if (M5.BtnB.wasClicked()) sw = true;
  } else {
    if (t.wasPressed() && !corner) {                    // ダブルタップ: 右半分 = 次へ / 左半分 = 前へ
      if (millis() - lastTapMs < 350) { go = t.x < W / 2 ? -1 : 1; lastTapMs = 0; } else lastTapMs = millis();
    }
    if (M5.BtnPWR.wasClicked()) sw = true;
  }
  // 触ると散る
  {
    const bool pr = t.isPressed() && !corner;
    const float tx = (float)(t.x - XOFF), ty = (float)(t.y - YOFF);
    if (pr) {
      const float mv = touching ? fabsf(tx - touchX) + fabsf(ty - touchY) : 999.0f;
      if (t.wasPressed() || !touching) spawnParts(tx, ty, SCATTER_N);
      else if (mv > 6) spawnParts(tx, ty, 3);     // なぞると、なぞった所から散る
      touchX = tx; touchY = ty; holeX = tx; holeY = ty;
    }
    touching = pr;
  }
  // 傾き: 本体を傾けたとき、画面の上で「下」になる向き
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
      const float dts = frameDt;
      if (levelN < 20) { ++levelN; levelX += (sx - levelX) / levelN; levelY += (sy - levelY) / levelN; }
      else if (TILT_RELEVEL > 0 && fabsf(sx - levelX) < TILT_RELEVEL_MAX && fabsf(sy - levelY) < TILT_RELEVEL_MAX) {   // 少しの傾きだけ「水平」に合わせなおす
        const float b = 1.0f - expf(-dts / TILT_RELEVEL); levelX += (sx - levelX) * b; levelY += (sy - levelY) * b; }
      sx -= levelX; sy -= levelY;
      sx *= TILT_GAIN; sy *= TILT_GAIN;
      const float m = sqrtf(sx * sx + sy * sy);
      if (m < TILT_DEAD) { sx = sy = 0; } else { const float k = (m - TILT_DEAD) / m; sx *= k; sy *= k; }
      sx = sx < -1 ? -1 : sx > 1 ? 1 : sx; sy = sy < -1 ? -1 : sy > 1 ? 1 : sy;
      const float a = 1.0f - expf(-dts * 6.0f);
      tiltX += (sx - tiltX) * a; tiltY += (sy - tiltY) * a;
    }
  }
  if (go) {
    if (phase != 0) gotoEnt(go > 0 ? 0 : NENT - 1);
    else if (go > 0 && cur + 1 >= NENT) { phase = 1; phaseT = 0; }
    else gotoEnt(cur + go);
  }
  if (sw) setView((viewSel + 1) % 3);
  const uint32_t now = millis();
  float dt = (float)(now - last); last = now;
  if (dt > 100) dt = 100;
  frameDt = dt * 0.001f;
  holeA = touching ? min(1.0f, holeA + frameDt * 6.0f) : max(0.0f, holeA - frameDt * 0.9f);
  stepParts(frameDt);
  update(dt);
  ++frameNo;
  renderFrame();
  if (++frames, now - fpsT >= 3000) {
    Serial.printf("fps: %.1f  (%s)  core0 %.1f ms  core1 %.1f ms  send %.1f ms\n", frames * 1000.0f / (now - fpsT), viewMode == 0 ? "corridor" : "tower",
                  busyUs[0] / 1000.0f, busyUs[1] / 1000.0f, sendUs / 1000.0f);
    if (imuOK) Serial.printf("tilt: x %+.2f  y %+.2f  (水平の基準 x %+.2f  y %+.2f)  右を下げると x が +、手前(画面の下)を下げると y が + なら正しい向き\n", tiltX, tiltY, levelX, levelY);
    frames = 0; fpsT = now;
  }
}


// 共通の置き場に置いたものは、0 で始まる単純な型だけ(コンストラクタが要らないもの)
static_assert(std::is_trivially_default_constructible<bool>::value, "NM_ARENA: bool");
static_assert(std::is_trivially_default_constructible<char>::value, "NM_ARENA: char");
static_assert(std::is_trivially_default_constructible<float>::value, "NM_ARENA: float");
static_assert(std::is_trivially_default_constructible<int>::value, "NM_ARENA: int");
static_assert(std::is_trivially_default_constructible<uint16_t>::value, "NM_ARENA: uint16_t");
static_assert(std::is_trivially_default_constructible<uint32_t>::value, "NM_ARENA: uint32_t");
static_assert(std::is_trivially_default_constructible<uint8_t>::value, "NM_ARENA: uint8_t");
static constexpr size_t OVL_END = nmAlignTo(nmOv54, 16);
static_assert(OVL_END <= NM_ARENA_SIZE, "NM_ARENA_SIZE (nm_common.h) を大きくしてください");
extern const size_t OVL_SIZE = OVL_END;   // 起動時の確認用
}  // namespace NullChronicle
