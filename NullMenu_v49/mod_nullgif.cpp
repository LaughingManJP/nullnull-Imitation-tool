// NullGif  -  NullMenu 用モジュール(元のスケッチ NullGif_v6 を名前空間に入れたもの)
// 0 で始まる変数(配列も)は、ほかの作品と共通の置き場(NM_ARENA)に置いています。
// 一度に動くのは1つだけ(切り替えるときは再起動)なので、重なっていても問題ありません。
#include "nm_common.h"
#include <FFat.h>
#if defined(ESP_PLATFORM) || defined(NL_WIFI_TEST)
#include <WiFi.h>
#endif
#if defined(ESP_PLATFORM) || defined(NL_WIFI_TEST)
#include <WebServer.h>
#endif
#pragma GCC optimize("O3")

namespace NullGif {
static constexpr size_t nmOv0 = 0;

// NullGif v6  -  丸い画面で GIF を流す。GIF は本体の Wi-Fi か USB ケーブルで送る
//   M5Stack StopWatch 用(丸い 466x466 画面)。CoreS3 系でも動きます(書き込むときに FATFS のある Partition Scheme を選ぶ)。
//
// 画面の中:
//   ・本体にしまってある GIF を、画面の大きさに合わせて流します(丸い画面なので、四角い GIF の四隅は切れます)
//   ・GIF を切り替えたときなどに、下に「何番目か・名前・残りの容量」が少しのあいだ出ます
//     (設定の「名前の表示」で、手で替えたときだけ / いつも / 出さない を選べます)
//
// 操作:
//   StopWatch … ボタンA 短押し: 次の GIF   ボタンA 長押し: 前の GIF   ボタンB: 設定
//               (明るさ / 終わったら / 名前の表示 / 向き / この GIF を消す / Wi-Fi で受け取る / USB で受け取る)
//               設定の中: ボタンA 短押し: 次の項目   ボタンB 短押し: 変える   ボタンA 長押し: とじる
//               Wi-Fi の画面: ボタンA: QR コードを切り替え   ボタンB: Wi-Fi を切って戻る
//   CoreS3系  … 画面の右半分をタップ: 次の GIF   左半分: 前の GIF   電源ボタン短押し: 設定
//               設定の中: 項目をタップ: 変える   Wi-Fi の画面: タップ: QR を切り替え   電源ボタン: 戻る
//
// GIF の送り方(Wi-Fi):
//   1. ボタン B で設定を開いて「Wi-Fi で受け取る」を選ぶ
//   2. 画面の QR コードをスマホのカメラで読む → 本体の Wi-Fi「Null2-xxxx」に入る(パスワードは画面に出ている8けた)
//   3. A を押すと、ページの QR コードに変わる → 読む(または http://192.168.4.1 を開く)→ GIF を選ぶ
//   スマホは「インターネットなし」と出ますが、そのままで大丈夫です。ページが開かないときは、モバイルデータ通信をいったん切ってください。
//
// GIF の送り方(USB):
//   1. この作品を動かしたまま、USB ケーブルで PC につなぐ
//   2. PC の Chrome か Edge で「GIF を送るページ」を開く
//      (GitHub の …/docs/gif.html。zip の中の docs/gif.html をダブルクリックで開いても使えます)
//   3. 「本体につなぐ」→ 出てきた一覧から StopWatch を選ぶ → GIF をページに落とす
//   ページには、本体の GIF の一覧と、残りの容量 / 全体の容量 が出ます。いらない GIF はページから消せます。
//   (Arduino IDE のシリアルモニタを開いていると、ページからつなげません。閉じてからつないでください)
//
// しまう場所:
//   書き込むときの Partition Scheme「16M Flash (3MB APP/9.9MB FATFS)」の、9.9MB の FATFS の区画を使います。
//   プログラムとは別の場所なので、GIF を出し入れしてもプログラムは消えません。
//   最初に動かしたときだけ、その区画を GIF 用に整えるので、数秒〜数十秒かかります。
//
// 必要なもの:
//   ボードマネージャー M5Stack >= 3.3.7 / ボード: M5StopWatch(CoreS3 なら M5CoreS3)
//     ツール: Partition Scheme「16M Flash (3MB APP/9.9MB FATFS)」、USB CDC On Boot「Enabled」
//   ライブラリ M5Unified >= 0.2.15, M5GFX >= 0.2.21(GIF を読む部分は、このスケッチの中に入っています)
//
// しくみ:
//   GIF は、色の番号の並びを LZW という方法で縮めたものです。1コマずつほどいて、
//   GIF と同じ大きさの「絵の置き場」(PSRAM)に重ねていき、それを画面の大きさに合わせて 8 行ずつ送ります。
//   USB の送り受けは、1行の文字の約束ごと(「NMG …」で始まる行)と、2KB ずつのデータで行います。



// Arduino IDE は関数の宣言を自動でこのあたりに足すので、型の名前だけ先に宣言しておく
struct Input;

// ---- 調整用 ----------------------------------------------------------------
static constexpr int   MAX_GIF_SIDE = 1600;     // これより大きい GIF は流さない(絵の置き場が PSRAM に収まるように)
static constexpr int   INFO_MS      = 2500;     // 切り替えたとき、下に名前と容量を出しておく時間
static constexpr int   CHUNK        = 2048;     // USB で受け取る1回ぶん(ここまで受け取ったら「受け取った」と返す)
static constexpr int   MAX_FILES    = 64;       // しまっておける GIF の数
static constexpr int   GNAME_MAX     = 40;       // 名前の長さ(英数字と . _ -)
static constexpr uint32_t WIFI_IDLE_MS = 180000; // Wi-Fi は、これだけ何もしなければ自動で切る(3分)
static constexpr int   MENU_IDLE_MS  = 12000;    // 設定の画面を、さわらなければこの時間でとじる
static constexpr uint32_t NEXT_MIN_MS = 5000;    // 「終わったら: 次へ」のとき、短い GIF もこれだけはくり返して流す
// ---------------------------------------------------------------------------

static constexpr int BAND  = 8;
#if defined(ESP_PLATFORM) || defined(NL_WIFI_TEST)
#define NL_WIFI 1
#else
#define NL_WIFI 0                                 // ブラウザのエミュレータには Wi-Fi はない
#endif
static constexpr int RBUF  = 4096;
static constexpr int LZW_N = 4096;

static constexpr size_t nmOa0 = nmAlignTo(nmOv0, alignof(int)), nmOv1 = nmOa0 + sizeof(int); static int& W = *reinterpret_cast<int*>(NM_ARENA + nmOa0);
static constexpr size_t nmOa1 = nmAlignTo(nmOv1, alignof(int)), nmOv2 = nmOa1 + sizeof(int); static int& H = *reinterpret_cast<int*>(NM_ARENA + nmOa1);
static constexpr size_t nmOa2 = nmAlignTo(nmOv2, alignof(int)), nmOv3 = nmOa2 + sizeof(int); static int& S = *reinterpret_cast<int*>(NM_ARENA + nmOa2);
static constexpr size_t nmOa3 = nmAlignTo(nmOv3, alignof(int)), nmOv4 = nmOa3 + sizeof(int); static int& XOFF = *reinterpret_cast<int*>(NM_ARENA + nmOa3);
static constexpr size_t nmOa4 = nmAlignTo(nmOv4, alignof(int)), nmOv5 = nmOa4 + sizeof(int); static int& YOFF = *reinterpret_cast<int*>(NM_ARENA + nmOa4);
static bool roundScr = true;
static const uint8_t brightSteps[] = {60, 120, 190, 255};
static int brightIdx = 2;

// ---- しまってある GIF
static constexpr size_t nmOa5 = nmAlignTo(nmOv5, alignof(bool)), nmOv6 = nmOa5 + sizeof(bool); static bool& fsOk = *reinterpret_cast<bool*>(NM_ARENA + nmOa5);
static constexpr size_t nmOa6 = nmAlignTo(nmOv6, 16), nmOv7 = nmOa6 + sizeof(char[MAX_FILES][GNAME_MAX + 1]); static char (&names)[MAX_FILES][GNAME_MAX + 1] = *reinterpret_cast<char(*)[MAX_FILES][GNAME_MAX + 1]>(NM_ARENA + nmOa6);
static constexpr size_t nmOa7 = nmAlignTo(nmOv7, 16), nmOv8 = nmOa7 + sizeof(uint32_t[MAX_FILES]); static uint32_t (&sizes)[MAX_FILES] = *reinterpret_cast<uint32_t(*)[MAX_FILES]>(NM_ARENA + nmOa7);
static constexpr size_t nmOa8 = nmAlignTo(nmOv8, alignof(int)), nmOv9 = nmOa8 + sizeof(int); static int& nFiles = *reinterpret_cast<int*>(NM_ARENA + nmOa8);
static int cur = -1;  // いま流している番号
static constexpr size_t nmOa9 = nmAlignTo(nmOv9, alignof(size_t)), nmOv10 = nmOa9 + sizeof(size_t); static size_t& fsTotal = *reinterpret_cast<size_t*>(NM_ARENA + nmOa9);
static constexpr size_t nmOa10 = nmAlignTo(nmOv10, alignof(size_t)), nmOv11 = nmOa10 + sizeof(size_t); static size_t& fsUsed = *reinterpret_cast<size_t*>(NM_ARENA + nmOa10);

// ---- GIF を読む
static File*     gfile = nullptr;
static uint8_t*  rbuf = nullptr;            // 読み込みの置き場
static constexpr size_t nmOa11 = nmAlignTo(nmOv11, alignof(int)), nmOv12 = nmOa11 + sizeof(int); static int& rpos = *reinterpret_cast<int*>(NM_ARENA + nmOa11);
static constexpr size_t nmOa12 = nmAlignTo(nmOv12, alignof(int)), nmOv13 = nmOa12 + sizeof(int); static int& rlen = *reinterpret_cast<int*>(NM_ARENA + nmOa12);
static constexpr size_t nmOa13 = nmAlignTo(nmOv13, alignof(uint32_t)), nmOv14 = nmOa13 + sizeof(uint32_t); static uint32_t& rbase = *reinterpret_cast<uint32_t*>(NM_ARENA + nmOa13);  // rbuf の先頭が、ファイルの何バイト目か
static constexpr size_t nmOa14 = nmAlignTo(nmOv14, alignof(int)), nmOv15 = nmOa14 + sizeof(int); static int& gw = *reinterpret_cast<int*>(NM_ARENA + nmOa14);
static constexpr size_t nmOa15 = nmAlignTo(nmOv15, alignof(int)), nmOv16 = nmOa15 + sizeof(int); static int& gh = *reinterpret_cast<int*>(NM_ARENA + nmOa15);  // GIF の大きさ
static constexpr size_t nmOa16 = nmAlignTo(nmOv16, 16), nmOv17 = nmOa16 + sizeof(uint16_t[256]); static uint16_t (&gct)[256] = *reinterpret_cast<uint16_t(*)[256]>(NM_ARENA + nmOa16);
static constexpr size_t nmOa17 = nmAlignTo(nmOv17, 16), nmOv18 = nmOa17 + sizeof(uint16_t[256]); static uint16_t (&lct)[256] = *reinterpret_cast<uint16_t(*)[256]>(NM_ARENA + nmOa17);  // 色の表(画面へ送る形の色)
static constexpr size_t nmOa18 = nmAlignTo(nmOv18, alignof(int)), nmOv19 = nmOa18 + sizeof(int); static int& gctN = *reinterpret_cast<int*>(NM_ARENA + nmOa18);
static uint16_t* canvas = nullptr;          // 重ねていく絵(gw x gh)
static uint16_t* saved = nullptr;           // 「前に戻す」コマのための控え
static constexpr size_t nmOa19 = nmAlignTo(nmOv19, alignof(uint32_t)), nmOv20 = nmOa19 + sizeof(uint32_t); static uint32_t& firstPos = *reinterpret_cast<uint32_t*>(NM_ARENA + nmOa19);  // 1コマ目の始まり(くり返すときに戻る所)
static constexpr size_t nmOa20 = nmAlignTo(nmOv20, alignof(int)), nmOv21 = nmOa20 + sizeof(int); static int& prevDisp = *reinterpret_cast<int*>(NM_ARENA + nmOa20);
static constexpr size_t nmOa21 = nmAlignTo(nmOv21, alignof(int)), nmOv22 = nmOa21 + sizeof(int); static int& prevX = *reinterpret_cast<int*>(NM_ARENA + nmOa21);
static constexpr size_t nmOa22 = nmAlignTo(nmOv22, alignof(int)), nmOv23 = nmOa22 + sizeof(int); static int& prevY = *reinterpret_cast<int*>(NM_ARENA + nmOa22);
static constexpr size_t nmOa23 = nmAlignTo(nmOv23, alignof(int)), nmOv24 = nmOa23 + sizeof(int); static int& prevW = *reinterpret_cast<int*>(NM_ARENA + nmOa23);
static constexpr size_t nmOa24 = nmAlignTo(nmOv24, alignof(int)), nmOv25 = nmOa24 + sizeof(int); static int& prevH = *reinterpret_cast<int*>(NM_ARENA + nmOa24);
static uint16_t* lzwPre = nullptr;
static uint8_t*  lzwSuf = nullptr;
static uint8_t*  lzwStk = nullptr;
static constexpr size_t nmOa25 = nmAlignTo(nmOv25, alignof(bool)), nmOv26 = nmOa25 + sizeof(bool); static bool& gifOk = *reinterpret_cast<bool*>(NM_ARENA + nmOa25);
static constexpr size_t nmOa26 = nmAlignTo(nmOv26, alignof(uint32_t)), nmOv27 = nmOa26 + sizeof(uint32_t); static uint32_t& nextAt = *reinterpret_cast<uint32_t*>(NM_ARENA + nmOa26);  // 次のコマを出す時刻
static constexpr size_t nmOa27 = nmAlignTo(nmOv27, alignof(int)), nmOv28 = nmOa27 + sizeof(int); static int& framesThisLoop = *reinterpret_cast<int*>(NM_ARENA + nmOa27);

// ---- 画面へ
static uint16_t* band = nullptr;            // 8 行ぶん
static int16_t*  mapX = nullptr;            // 画面の x → GIF の x(-1 = 外)
static int16_t*  mapY = nullptr;
static int16_t*  spanS = nullptr;           // 丸い画面の、行ごとの左右
static int16_t*  spanE = nullptr;
static constexpr size_t nmOa28 = nmAlignTo(nmOv28, alignof(uint32_t)), nmOv29 = nmOa28 + sizeof(uint32_t); static uint32_t& infoUntil = *reinterpret_cast<uint32_t*>(NM_ARENA + nmOa28);
static constexpr size_t nmOa29 = nmAlignTo(nmOv29, 16), nmOv30 = nmOa29 + sizeof(char[64]); static char (&infoMsg)[64] = *reinterpret_cast<char(*)[64]>(NM_ARENA + nmOa29);
static M5Canvas* infoSpr = nullptr;           // 下に出す「名前と容量」(GIF の絵に重ねて送る)
static constexpr size_t nmOa30 = nmAlignTo(nmOv30, alignof(int)), nmOv31 = nmOa30 + sizeof(int); static int& infoX = *reinterpret_cast<int*>(NM_ARENA + nmOa30);
static constexpr size_t nmOa31 = nmAlignTo(nmOv31, alignof(int)), nmOv32 = nmOa31 + sizeof(int); static int& infoY = *reinterpret_cast<int*>(NM_ARENA + nmOa31);
static constexpr size_t nmOa32 = nmAlignTo(nmOv32, alignof(int)), nmOv33 = nmOa32 + sizeof(int); static int& infoW = *reinterpret_cast<int*>(NM_ARENA + nmOa32);
static constexpr size_t nmOa33 = nmAlignTo(nmOv33, alignof(int)), nmOv34 = nmOa33 + sizeof(int); static int& infoH = *reinterpret_cast<int*>(NM_ARENA + nmOa33);  // 置く場所(GIF の四角の中の位置)
static constexpr size_t nmOa34 = nmAlignTo(nmOv34, alignof(uint16_t)), nmOv35 = nmOa34 + sizeof(uint16_t); static uint16_t& infoKey = *reinterpret_cast<uint16_t*>(NM_ARENA + nmOa34);  // infoSpr の「何もしない」色
static constexpr size_t nmOa35 = nmAlignTo(nmOv35, alignof(bool)), nmOv36 = nmOa35 + sizeof(bool); static bool& infoShown = *reinterpret_cast<bool*>(NM_ARENA + nmOa35);  // いま画面に出ているか
static constexpr size_t nmOa36 = nmAlignTo(nmOv36, alignof(bool)), nmOv37 = nmOa36 + sizeof(bool); static bool& autoSwitch = *reinterpret_cast<bool*>(NM_ARENA + nmOa36);  // 自動で次の GIF へ進んだところか

// ---- USB
static constexpr size_t nmOa37 = nmAlignTo(nmOv37, 16), nmOv38 = nmOa37 + sizeof(char[160]); static char (&line)[160] = *reinterpret_cast<char(*)[160]>(NM_ARENA + nmOa37);
static constexpr size_t nmOa38 = nmAlignTo(nmOv38, alignof(int)), nmOv39 = nmOa38 + sizeof(int); static int& lineLen = *reinterpret_cast<int*>(NM_ARENA + nmOa38);
static File*     rxFile = nullptr;
static constexpr size_t nmOa39 = nmAlignTo(nmOv39, alignof(bool)), nmOv40 = nmOa39 + sizeof(bool); static bool& rxActive = *reinterpret_cast<bool*>(NM_ARENA + nmOa39);
static constexpr size_t nmOa40 = nmAlignTo(nmOv40, alignof(bool)), nmOv41 = nmOa40 + sizeof(bool); static bool& rxWaitDone = *reinterpret_cast<bool*>(NM_ARENA + nmOa40);
static constexpr size_t nmOa41 = nmAlignTo(nmOv41, alignof(uint32_t)), nmOv42 = nmOa41 + sizeof(uint32_t); static uint32_t& rxSize = *reinterpret_cast<uint32_t*>(NM_ARENA + nmOa41);
static constexpr size_t nmOa42 = nmAlignTo(nmOv42, alignof(uint32_t)), nmOv43 = nmOa42 + sizeof(uint32_t); static uint32_t& rxGot = *reinterpret_cast<uint32_t*>(NM_ARENA + nmOa42);
static constexpr size_t nmOa43 = nmAlignTo(nmOv43, alignof(uint32_t)), nmOv44 = nmOa43 + sizeof(uint32_t); static uint32_t& rxSinceAck = *reinterpret_cast<uint32_t*>(NM_ARENA + nmOa43);
static constexpr size_t nmOa44 = nmAlignTo(nmOv44, alignof(uint32_t)), nmOv45 = nmOa44 + sizeof(uint32_t); static uint32_t& rxCrc = *reinterpret_cast<uint32_t*>(NM_ARENA + nmOa44);
static constexpr size_t nmOa45 = nmAlignTo(nmOv45, alignof(uint32_t)), nmOv46 = nmOa45 + sizeof(uint32_t); static uint32_t& rxLast = *reinterpret_cast<uint32_t*>(NM_ARENA + nmOa45);
static constexpr size_t nmOa46 = nmAlignTo(nmOv46, 16), nmOv47 = nmOa46 + sizeof(char[GNAME_MAX + 1]); static char (&rxName)[GNAME_MAX + 1] = *reinterpret_cast<char(*)[GNAME_MAX + 1]>(NM_ARENA + nmOa46);
static uint8_t*  rxBuf = nullptr;
static int rxShownPct = -1;

// ---- 設定の画面と Wi-Fi
enum { G_PLAY = 0, G_MENU = 1, G_WIFI = 2, G_CONFIRM = 3, G_USBINFO = 4 };
static int gMode = G_PLAY;
static bool autoNext = true;  // 1回流し終わったら次の GIF へ(false = くり返す)
static constexpr size_t nmOa47 = nmAlignTo(nmOv47, alignof(int)), nmOv48 = nmOa47 + sizeof(int); static int& infoMode = *reinterpret_cast<int*>(NM_ARENA + nmOa47);  // 名前の表示: 0 手で替えたとき / 1 いつも / 2 出さない
static const char* const INFO_NAME[] = {"手で替えたとき", "いつも", "出さない"};
static constexpr size_t nmOa48 = nmAlignTo(nmOv48, alignof(bool)), nmOv49 = nmOa48 + sizeof(bool); static bool& flip180 = *reinterpret_cast<bool*>(NM_ARENA + nmOa48);  // 向き: true = さかさま(GIF を 180 度回す)
static constexpr size_t nmOa49 = nmAlignTo(nmOv49, alignof(uint32_t)), nmOv50 = nmOa49 + sizeof(uint32_t); static uint32_t& playStartMs = *reinterpret_cast<uint32_t*>(NM_ARENA + nmOa49);  // いまの GIF を流しはじめた時刻
static constexpr size_t nmOa50 = nmAlignTo(nmOv50, alignof(uint32_t)), nmOv51 = nmOa50 + sizeof(uint32_t); static uint32_t& confirmT = *reinterpret_cast<uint32_t*>(NM_ARENA + nmOa50);
static constexpr size_t nmOa51 = nmAlignTo(nmOv51, alignof(int)), nmOv52 = nmOa51 + sizeof(int); static int& menuSel = *reinterpret_cast<int*>(NM_ARENA + nmOa51);
static constexpr size_t nmOa52 = nmAlignTo(nmOv52, alignof(uint32_t)), nmOv53 = nmOa52 + sizeof(uint32_t); static uint32_t& menuT = *reinterpret_cast<uint32_t*>(NM_ARENA + nmOa52);
static constexpr int MENU_N = 8;                  // 明るさ / 終わったら / 名前の表示 / 向き / 消す / Wi-Fi / USB / とじる
#if NL_WIFI
static WebServer* web = nullptr;
#endif
static constexpr size_t nmOa53 = nmAlignTo(nmOv53, alignof(bool)), nmOv54 = nmOa53 + sizeof(bool); static bool& wifiOn = *reinterpret_cast<bool*>(NM_ARENA + nmOa53);
static constexpr size_t nmOa54 = nmAlignTo(nmOv54, alignof(uint32_t)), nmOv55 = nmOa54 + sizeof(uint32_t); static uint32_t& wifiLast = *reinterpret_cast<uint32_t*>(NM_ARENA + nmOa54);  // 最後に何かあった時刻(ここから3分で切る)
static constexpr size_t nmOa55 = nmAlignTo(nmOv55, alignof(int)), nmOv56 = nmOa55 + sizeof(int); static int& wifiQr = *reinterpret_cast<int*>(NM_ARENA + nmOa55);  // 0 = Wi-Fi に入る QR / 1 = ページの QR
static int wifiSta = -1;
static int wifiShownMin = -1;
static constexpr size_t nmOa56 = nmAlignTo(nmOv56, alignof(bool)), nmOv57 = nmOa56 + sizeof(bool); static bool& wifiDirty = *reinterpret_cast<bool*>(NM_ARENA + nmOa56);
static constexpr size_t nmOa57 = nmAlignTo(nmOv57, alignof(uint32_t)), nmOv58 = nmOa57 + sizeof(uint32_t); static uint32_t& wifiPeekUntil = *reinterpret_cast<uint32_t*>(NM_ARENA + nmOa57);  // 「流す」で GIF を少しのあいだ流している
static constexpr size_t nmOa58 = nmAlignTo(nmOv58, 16), nmOv59 = nmOa58 + sizeof(char[24]); static char (&wifiSsid)[24] = *reinterpret_cast<char(*)[24]>(NM_ARENA + nmOa58);
static constexpr size_t nmOa59 = nmAlignTo(nmOv59, 16), nmOv60 = nmOa59 + sizeof(char[12]); static char (&wifiPass)[12] = *reinterpret_cast<char(*)[12]>(NM_ARENA + nmOa59);
static constexpr size_t nmOa60 = nmAlignTo(nmOv60, 16), nmOv61 = nmOa60 + sizeof(char[64]); static char (&wifiMsg)[64] = *reinterpret_cast<char(*)[64]>(NM_ARENA + nmOa60);
static File*     upFile = nullptr;
static const char* upErr = nullptr;
static constexpr size_t nmOa61 = nmAlignTo(nmOv61, alignof(uint32_t)), nmOv62 = nmOa61 + sizeof(uint32_t); static uint32_t& upGot = *reinterpret_cast<uint32_t*>(NM_ARENA + nmOa61);
static constexpr size_t nmOa62 = nmAlignTo(nmOv62, alignof(uint32_t)), nmOv63 = nmOa62 + sizeof(uint32_t); static uint32_t& upSize = *reinterpret_cast<uint32_t*>(NM_ARENA + nmOa62);
static constexpr size_t nmOa63 = nmAlignTo(nmOv63, alignof(uint32_t)), nmOv64 = nmOa63 + sizeof(uint32_t); static uint32_t& upShownMs = *reinterpret_cast<uint32_t*>(NM_ARENA + nmOa63);
static constexpr size_t nmOa64 = nmAlignTo(nmOv64, 16), nmOv65 = nmOa64 + sizeof(char[GNAME_MAX + 1]); static char (&upName)[GNAME_MAX + 1] = *reinterpret_cast<char(*)[GNAME_MAX + 1]>(NM_ARENA + nmOa64);

// ---------------------------------------------------------------- 道具
static inline uint16_t toPix(int r, int g, int b) {
  return __builtin_bswap16((uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)));
}
static void fmtMB(char* o, size_t n, size_t bytes) { snprintf(o, n, "%.1fMB", bytes / 1048576.0); }

// CRC32(PC から送られた GIF が、途中で化けていないか確かめる)
static uint32_t crcAdd(uint32_t crc, const uint8_t* p, size_t n) {
  static const uint32_t T[16] = {
    0x00000000, 0x1DB71064, 0x3B6E20C8, 0x26D930AC, 0x76DC4190, 0x6B6B51F4, 0x4DB26158, 0x5005713C,
    0xEDB88320, 0xF00F9344, 0xD6D6A3E8, 0xCB61B38C, 0x9B64C2B0, 0x86D3D2D4, 0xA00AE278, 0xBDBDF21C };
  crc = ~crc;
  for (size_t i = 0; i < n; ++i) {
    crc ^= p[i];
    crc = (crc >> 4) ^ T[crc & 15];
    crc = (crc >> 4) ^ T[crc & 15];
  }
  return ~crc;
}

// 名前は英数字と . _ - だけ、.gif で終わるもの
static bool goodName(const char* s) {
  int n = (int)strlen(s);
  if (n < 5 || n > GNAME_MAX) return false;
  for (int i = 0; i < n; ++i) {
    char c = s[i];
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-')) return false;
  }
  if (s[0] == '.' || s[0] == '~') return false;
  const char* e = s + n - 4;
  return (e[0] == '.') && (e[1] == 'g' || e[1] == 'G') && (e[2] == 'i' || e[2] == 'I') && (e[3] == 'f' || e[3] == 'F');
}

// ---------------------------------------------------------------- しまう場所
static void fsStats() {
  if (!fsOk) { fsTotal = fsUsed = 0; return; }
  fsTotal = FFat.totalBytes(); fsUsed = FFat.usedBytes();
}
static void scanFiles() {
  nFiles = 0;
  if (!fsOk) return;
  File dir = FFat.open("/gif");
  if (!dir || !dir.isDirectory()) return;
  for (File f = dir.openNextFile(); f && nFiles < MAX_FILES; f = dir.openNextFile()) {
    if (f.isDirectory()) continue;
    const char* nm = f.name();
    const char* slash = strrchr(nm, '/');
    if (slash) nm = slash + 1;
    if (!goodName(nm)) continue;
    strncpy(names[nFiles], nm, GNAME_MAX); names[nFiles][GNAME_MAX] = 0;
    sizes[nFiles] = (uint32_t)f.size();
    ++nFiles;
  }
  // 名前の順に並べる
  for (int i = 1; i < nFiles; ++i)
    for (int j = i; j > 0 && strcmp(names[j - 1], names[j]) > 0; --j) {
      char t[GNAME_MAX + 1]; strcpy(t, names[j]); strcpy(names[j], names[j - 1]); strcpy(names[j - 1], t);
      uint32_t z = sizes[j]; sizes[j] = sizes[j - 1]; sizes[j - 1] = z;
    }
  fsStats();
}
static int findFile(const char* nm) {
  for (int i = 0; i < nFiles; ++i) if (!strcmp(names[i], nm)) return i;
  return -1;
}

// ---------------------------------------------------------------- GIF を読む
static void rSeek(uint32_t pos) { gfile->seek(pos); rbase = pos; rpos = rlen = 0; }
static inline uint32_t rTell() { return rbase + rpos; }
static inline int rByte() {
  if (rpos >= rlen) {
    rbase += rlen;
    rlen = gfile->read(rbuf, RBUF);
    rpos = 0;
    if (rlen <= 0) { rlen = 0; return -1; }
  }
  return rbuf[rpos++];
}
static inline int rU16() { int a = rByte(), b = rByte(); return (a < 0 || b < 0) ? -1 : (a | (b << 8)); }
static void skipBlocks() { for (;;) { int n = rByte(); if (n <= 0) return; for (int i = 0; i < n; ++i) if (rByte() < 0) return; } }
static void readPal(uint16_t* pal, int n) {
  for (int i = 0; i < n; ++i) { int r = rByte(), g = rByte(), b = rByte(); pal[i] = toPix(r & 255, g & 255, b & 255); }
}

static void closeGif() {
  if (gfile) { gfile->close(); delete gfile; gfile = nullptr; }
  free(canvas); canvas = nullptr;
  free(saved); saved = nullptr;
  gifOk = false;
}

// 画面の点 → GIF の点(画面に収まるように拡大・縮小して、まん中に置く)
static void buildMaps() {
  const float k = fminf((float)S / gw, (float)S / gh);
  const float dw = gw * k, dh = gh * k, ox = (S - dw) * 0.5f, oy = (S - dh) * 0.5f;
  for (int x = 0; x < S; ++x) { int sx = (int)floorf((x + 0.5f - ox) / k); mapX[x] = (sx >= 0 && sx < gw) ? sx : -1; }
  for (int y = 0; y < S; ++y) { int sy = (int)floorf((y + 0.5f - oy) / k); mapY[y] = (sy >= 0 && sy < gh) ? sy : -1; }
  if (flip180) {                                           // さかさま: 左右と上下を逆に読む
    for (int i = 0, j = S - 1; i < j; ++i, --j) { int16_t t = mapX[i]; mapX[i] = mapX[j]; mapX[j] = t; t = mapY[i]; mapY[i] = mapY[j]; mapY[j] = t; }
  }
}

static bool openGif(int i) {
  closeGif();
  if (i < 0 || i >= nFiles) return false;
  char path[64]; snprintf(path, sizeof(path), "/gif/%s", names[i]);
  File f = FFat.open(path, "r");
  if (!f) return false;
  gfile = new File(f);
  rSeek(0);
  char sig[6];
  for (int k = 0; k < 6; ++k) sig[k] = (char)rByte();
  if (memcmp(sig, "GIF87a", 6) && memcmp(sig, "GIF89a", 6)) { closeGif(); return false; }
  gw = rU16(); gh = rU16();
  const int pk = rByte(); rByte(); rByte();              // 背景の色の番号、縦横比(使わない)
  if (gw <= 0 || gh <= 0 || gw > MAX_GIF_SIDE || gh > MAX_GIF_SIDE) { closeGif(); return false; }
  gctN = 0;
  if (pk & 0x80) { gctN = 2 << (pk & 7); readPal(gct, gctN); }
  firstPos = rTell();
  canvas = (uint16_t*)ps_malloc((size_t)gw * gh * 2);
  if (!canvas) canvas = (uint16_t*)malloc((size_t)gw * gh * 2);
  if (!canvas) { closeGif(); return false; }
  memset(canvas, 0, (size_t)gw * gh * 2);
  prevDisp = 0; framesThisLoop = 0;
  buildMaps();
  gifOk = true;
  return true;
}

// LZW をほどいて、コマの四角(ix, iy, iw, ih)に色を置く
static bool decodeImage(int ix, int iy, int iw, int ih, bool inter, const uint16_t* pal, int palN, bool hasT, int tIdx) {
  const int minCode = rByte();
  if (minCode < 2 || minCode > 11) return false;
  const int clear = 1 << minCode, eoi = clear + 1;
  int codeSize = minCode + 1, next = clear + 2, old = -1, first = 0;
  uint32_t bits = 0; int nbits = 0, left = 0;
  bool ended = false;
  // 置く位置(飛び飛びの行の GIF にも対応)
  int col = 0, row = 0, pass = 0, rstep = inter ? 8 : 1;
  bool full = false;
  auto put = [&](int v) {
    if (full) return;
    const int X = ix + col, Y = iy + row;
    if (X >= 0 && X < gw && Y >= 0 && Y < gh && !(hasT && v == tIdx))
      canvas[Y * gw + X] = v < palN ? pal[v] : 0;
    if (++col >= iw) {
      col = 0; row += rstep;
      while (row >= ih && inter && pass < 3) {
        ++pass;
        row = pass == 1 ? 4 : pass == 2 ? 2 : 1;
        rstep = pass == 1 ? 8 : pass == 2 ? 4 : 2;
      }
      if (row >= ih) full = true;
    }
  };
  for (int k = 0; k < clear; ++k) { lzwPre[k] = 0xFFFF; lzwSuf[k] = (uint8_t)k; }
  for (;;) {
    while (nbits < codeSize) {                             // 小分けの塊から、ビットを足していく
      if (left == 0) { left = rByte(); if (left <= 0) { ended = true; break; } }
      const int b = rByte(); if (b < 0) { ended = true; break; }
      bits |= (uint32_t)b << nbits; nbits += 8; --left;
    }
    if (ended) break;
    int code = bits & ((1 << codeSize) - 1);
    bits >>= codeSize; nbits -= codeSize;
    if (code == clear) { codeSize = minCode + 1; next = clear + 2; old = -1; continue; }
    if (code == eoi) break;
    if (old < 0) {
      if (code >= clear) return false;
      put(code); old = first = code; continue;
    }
    const int in = code;
    int sp = 0;
    if (code >= next) {
      if (code > next) return false;                       // 壊れたデータ
      lzwStk[sp++] = (uint8_t)first; code = old;
    }
    while (code >= clear) {
      if (sp >= LZW_N) return false;
      lzwStk[sp++] = lzwSuf[code]; code = lzwPre[code];
    }
    first = code;
    lzwStk[sp++] = (uint8_t)first;
    while (sp > 0) put(lzwStk[--sp]);
    if (next < LZW_N) {
      lzwPre[next] = (uint16_t)old; lzwSuf[next] = (uint8_t)first; ++next;
      if (next == (1 << codeSize) && codeSize < 12) ++codeSize;
    }
    old = in;
  }
  if (!ended) {                                            // 終わりの印のあとに残った塊を読み飛ばす
    if (left > 0) for (int i = 0; i < left; ++i) rByte();
    skipBlocks();
  }
  return true;
}

// 次のコマを絵の置き場に重ねる。戻り値: 次のコマまでの時間(ms)。-1 = 読めない
static int nextFrame() {
  // 前のコマの「片付け方」
  if (prevDisp == 2) {
    for (int y = prevY; y < prevY + prevH; ++y) memset(canvas + (size_t)y * gw + prevX, 0, prevW * 2);
  } else if (prevDisp == 3 && saved) {
    for (int y = prevY; y < prevY + prevH; ++y) memcpy(canvas + (size_t)y * gw + prevX, saved + (size_t)y * gw + prevX, prevW * 2);
  }
  prevDisp = 0;
  int delay = 0, disp = 0, tIdx = 0; bool hasT = false;
  for (int guard = 0; guard < 4096; ++guard) {
    const int b = rByte();
    if (b < 0 || b == 0x3B) {                              // 終わり → 1コマ目へ戻る
      if (framesThisLoop == 0) return -1;
      if (autoNext && nFiles > 1 && gMode == G_PLAY && millis() - playStartMs >= NEXT_MIN_MS) return -2;   // 流し終わった → 次の GIF へ
      framesThisLoop = 0;
      rSeek(firstPos);
      memset(canvas, 0, (size_t)gw * gh * 2);
      continue;
    }
    if (b == 0x21) {                                       // 拡張
      const int label = rByte();
      if (label == 0xF9) {                                 // 次のコマの決まり(待ち時間・透明・片付け方)
        const int sz = rByte(), p = rByte(), d = rU16(), t = rByte();
        for (int i = 4; i < sz; ++i) rByte();
        skipBlocks();
        disp = (p >> 2) & 7; hasT = p & 1; tIdx = t; delay = d * 10;
      } else skipBlocks();
      continue;
    }
    if (b == 0x2C) {                                       // コマの絵
      int ix = rU16(), iy = rU16(), iw = rU16(), ih = rU16();
      const int p = rByte();
      if (ix < 0 || iy < 0 || iw <= 0 || ih <= 0) return -1;
      const uint16_t* pal = gct; int palN = gctN;
      if (p & 0x80) { palN = 2 << (p & 7); readPal(lct, palN); pal = lct; }
      // 片付けに使う四角(画面からはみ出す分は切る)
      const int cx = min(ix, gw), cy = min(iy, gh);
      const int cw = max(0, min(ix + iw, gw) - cx), ch = max(0, min(iy + ih, gh) - cy);
      if (disp == 3) {
        if (!saved) saved = (uint16_t*)ps_malloc((size_t)gw * gh * 2);
        if (saved) for (int y = cy; y < cy + ch; ++y) memcpy(saved + (size_t)y * gw + cx, canvas + (size_t)y * gw + cx, cw * 2);
      }
      if (!decodeImage(ix, iy, iw, ih, p & 0x40, pal, palN, hasT, tIdx)) return -1;
      prevDisp = disp; prevX = cx; prevY = cy; prevW = cw; prevH = ch;
      ++framesThisLoop;
      return delay <= 10 ? 100 : delay;                    // 0 や 10ms は、ブラウザと同じく 100ms に
    }
    return -1;                                             // 知らない印
  }
  return -1;
}

// ---------------------------------------------------------------- 画面へ
static void drawCanvas() {
  const bool info = infoSpr && (int32_t)(millis() - infoUntil) < 0;
  for (int y0 = 0; y0 < S; y0 += BAND) {
    const int h = min(BAND, S - y0);
    int bx0 = S, bx1 = -1;
    for (int y = y0; y < y0 + h; ++y) if (spanS[y] <= spanE[y]) { bx0 = min(bx0, (int)spanS[y]); bx1 = max(bx1, (int)spanE[y]); }
    if (bx1 < bx0) continue;
    bx0 &= ~1; if (((bx1 - bx0 + 1) & 1) && bx1 < S - 1) ++bx1;
    const int bw = bx1 - bx0 + 1;
    for (int y = y0; y < y0 + h; ++y) {
      uint16_t* o = band + (y - y0) * bw;
      const int sy = mapY[y];
      if (sy < 0) { memset(o, 0, bw * 2); continue; }
      const uint16_t* src = canvas + (size_t)sy * gw;
      for (int x = bx0; x <= bx1; ++x) { const int sx = mapX[x]; o[x - bx0] = sx < 0 ? 0 : src[sx]; }
    }
    if (info && y0 < infoY + infoH && y0 + h > infoY) {   // 名前と容量を重ねる(うしろの絵は暗くする)
      const uint16_t* ib = (const uint16_t*)infoSpr->getBuffer();
      for (int y = max(y0, infoY); y < min(y0 + h, infoY + infoH); ++y) {
        uint16_t* o = band + (y - y0) * bw;
        const uint16_t* ir = ib + (y - infoY) * infoW;
        const int xa = max(bx0, infoX), xb = min(bx1, infoX + infoW - 1);
        for (int x = xa; x <= xb; ++x) {
          const uint16_t v = ir[x - infoX];
          if (v == infoKey) continue;
          if (v == 0) { const uint16_t p = __builtin_bswap16(o[x - bx0]); o[x - bx0] = __builtin_bswap16((uint16_t)(((p >> 2) & 0x39E7) + ((p >> 3) & 0x18E3))); }
          else o[x - bx0] = v;
        }
      }
    }
    M5.Display.pushImage(XOFF + bx0, YOFF + y0, bw, h, band);
  }
  infoShown = info;
}

// まん中に何行かの文字(GIF がないときなど)
static void message(const char* const* lines, int n) {
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setFont(&fonts::lgfxJapanGothicP_20);
  M5.Display.setTextDatum(middle_center);
  for (int i = 0; i < n; ++i) {
    M5.Display.setTextColor(i == 0 ? TFT_WHITE : 0xAD55);
    M5.Display.drawString(lines[i], W / 2, H / 2 + (i - (n - 1) * 0.5f) * 30);
  }
}
static void showEmpty() {
  if (gMode == G_WIFI) { wifiDirty = true; return; }
  if (!fsOk) {
    const char* L[] = {"GIF の置き場がありません", "Partition Scheme を", "16M Flash (3MB APP/9.9MB FATFS)", "にして書き込んでください"};
    message(L, 4); return;
  }
  char cap[48], a[16], b[16];
  fmtMB(a, sizeof(a), fsTotal > fsUsed ? fsTotal - fsUsed : 0); fmtMB(b, sizeof(b), fsTotal);
  snprintf(cap, sizeof(cap), "残り %s / %s", a, b);
  const char* L[] = {"GIF がありません", roundScr ? "設定(ボタン B)から" : "設定(電源ボタン)から", "Wi-Fi か USB で送れます", "(GIF は 64 個まで。容量しだい)", cap};
  message(L, 5);
}

static void setInfo(const char* fmt, const char* s) {
  if (!infoSpr || infoMode == 2 || (infoMode == 0 && autoSwitch)) { infoUntil = millis(); return; }
  snprintf(infoMsg, sizeof(infoMsg), fmt, cur + 1, nFiles, s);
  char cap[48], a[16], b[16];
  fmtMB(a, sizeof(a), fsTotal > fsUsed ? fsTotal - fsUsed : 0); fmtMB(b, sizeof(b), fsTotal);
  snprintf(cap, sizeof(cap), "残り %s / %s", a, b);
  infoSpr->fillSprite((uint16_t)0xF81F);   // 何もしないところの色
  infoKey = ((const uint16_t*)infoSpr->getBuffer())[0];
  infoSpr->fillRoundRect(0, 0, infoW, infoH, 14, TFT_BLACK);
  infoSpr->setFont(infoW >= 250 ? &fonts::lgfxJapanGothicP_20 : &fonts::lgfxJapanGothicP_16);
  infoSpr->setTextDatum(middle_center);
  // 長い名前は、収まるように後ろを「…」にする
  int len = strlen(infoMsg);
  while (len > 4 && infoSpr->textWidth(infoMsg) > infoW - 16) { infoMsg[--len] = 0; infoMsg[len - 1] = '.'; infoMsg[len - 2] = '.'; }
  infoSpr->setTextColor(TFT_WHITE);
  infoSpr->drawString(infoMsg, infoW / 2, infoH / 2 - 12);
  infoSpr->setTextColor(0xAD55);
  infoSpr->drawString(cap, infoW / 2, infoH / 2 + 12);
  infoUntil = millis() + INFO_MS;
}
static void playIndex(int i) {
  if (nFiles <= 0) { closeGif(); cur = -1; showEmpty(); return; }
  for (int tries = 0; tries < nFiles; ++tries) {
    cur = ((i + tries) % nFiles + nFiles) % nFiles;
    if (openGif(cur)) {
      M5.Display.fillScreen(TFT_BLACK);
      setInfo("%d / %d  %s", names[cur]);
      nextAt = millis(); playStartMs = millis();
      if (gMode == G_WIFI) wifiPeekUntil = (millis() + 5000) | 1;   // Wi-Fi の画面のあいだは、5秒流したら戻る
      Serial.printf("# play %s  %dx%d\n", names[cur], gw, gh);
      return;
    }
    Serial.printf("# cannot play %s\n", names[cur]);
  }
  closeGif(); cur = -1;
  if (gMode == G_WIFI) { wifiDirty = true; return; }
  const char* L[] = {"GIF を読めませんでした", "大きすぎるか、壊れています"};
  message(L, 2);
}

// ---------------------------------------------------------------- USB で受け取る
static void reply(const char* s) { Serial.print(s); Serial.print("\n"); }
static void replyOK() { char o[64]; fsStats(); snprintf(o, sizeof(o), "NMG OK %u %u", (unsigned)fsTotal, (unsigned)fsUsed); reply(o); }
static void rxAbort(const char* why) {
  if (rxFile) { rxFile->close(); delete rxFile; rxFile = nullptr; }
  if (fsOk) FFat.remove("/gif/~up.tmp");
  rxActive = rxWaitDone = false;
  char o[48]; snprintf(o, sizeof(o), "NMG ERR %s", why); reply(o);
  playIndex(cur < 0 ? 0 : cur);                            // 流していた GIF に戻る
}
static void showProgress() {
  const int pct = rxSize ? (int)((uint64_t)rxGot * 100 / rxSize) : 100;
  if (pct == rxShownPct) return;
  rxShownPct = pct;
  M5.Display.fillRect(W / 2 - 150, H / 2 - 50, 300, 100, TFT_BLACK);
  M5.Display.setFont(&fonts::lgfxJapanGothicP_20);
  M5.Display.setTextDatum(middle_center);
  M5.Display.setTextColor(TFT_WHITE);
  char s[64]; snprintf(s, sizeof(s), "受け取り中  %d%%", pct);
  M5.Display.drawString(s, W / 2, H / 2 - 22);
  M5.Display.setTextColor(0xAD55);
  M5.Display.drawString(rxName, W / 2, H / 2 + 6);
  M5.Display.fillRect(W / 2 - 120, H / 2 + 30, 240, 6, 0x4208);
  M5.Display.fillRect(W / 2 - 120, H / 2 + 30, 240 * pct / 100, 6, 0x07FF);
}

static void handleLine(char* s) {
  if (strncmp(s, "NMG ", 4)) return;
  char* cmd = s + 4;
  if (!strcmp(cmd, "HELLO")) {
    fsStats();
    char o[96]; snprintf(o, sizeof(o), "NMG HELLO 1 %u %u %d %d", (unsigned)fsTotal, (unsigned)fsUsed, nFiles, fsOk ? 1 : 0); reply(o);
  } else if (!strcmp(cmd, "LIST")) {
    char o[96];
    for (int i = 0; i < nFiles; ++i) { snprintf(o, sizeof(o), "NMG FILE %u %s", (unsigned)sizes[i], names[i]); reply(o); }
    snprintf(o, sizeof(o), "NMG END %d", nFiles); reply(o);
  } else if (!strncmp(cmd, "PUT ", 4)) {
    unsigned sz = 0; char nm[64] = {0};
    if (sscanf(cmd + 4, "%u %63s", &sz, nm) != 2 || !goodName(nm)) { reply("NMG ERR NAME"); return; }
    if (!fsOk) { reply("NMG ERR NOFS"); return; }
    if (upFile) { reply("NMG ERR BUSY"); return; }   // Wi-Fi で受け取っている最中
    fsStats();
    const int idx = findFile(nm);
    const size_t freeNow = (fsTotal > fsUsed ? fsTotal - fsUsed : 0) + (idx >= 0 ? sizes[idx] : 0);
    if (sz == 0 || (size_t)sz + 32768 > freeNow) { reply("NMG ERR FULL"); return; }
    if (idx < 0 && nFiles >= MAX_FILES) { reply("NMG ERR MANY"); return; }
    if (gifOk && idx == cur) closeGif();                  // 同じ名前を流していたら、いったん止める
    FFat.remove("/gif/~up.tmp");
    File f = FFat.open("/gif/~up.tmp", "w");
    if (!f) { reply("NMG ERR OPEN"); return; }
    rxFile = new File(f);
    strcpy(rxName, nm); rxSize = sz; rxGot = 0; rxSinceAck = 0; rxCrc = 0; rxLast = millis();
    rxActive = true; rxWaitDone = false; rxShownPct = -1;
    M5.Display.fillScreen(TFT_BLACK);
    showProgress();
    char o[32]; snprintf(o, sizeof(o), "NMG READY %d", CHUNK); reply(o);
  } else if (!strncmp(cmd, "DONE ", 5)) {
    if (!rxWaitDone) { reply("NMG ERR STATE"); return; }
    const uint32_t want = (uint32_t)strtoul(cmd + 5, nullptr, 16);
    rxFile->close(); delete rxFile; rxFile = nullptr;
    rxActive = rxWaitDone = false;
    if (want != rxCrc) { FFat.remove("/gif/~up.tmp"); reply("NMG ERR CRC"); playIndex(cur < 0 ? 0 : cur); return; }
    char path[64]; snprintf(path, sizeof(path), "/gif/%s", rxName);
    FFat.remove(path);
    if (!FFat.rename("/gif/~up.tmp", path)) { FFat.remove("/gif/~up.tmp"); reply("NMG ERR SAVE"); return; }
    scanFiles();
    replyOK();
    playIndex(findFile(rxName));                           // 受け取った GIF を流す
  } else if (!strncmp(cmd, "DEL ", 4)) {
    const char* nm = cmd + 4;
    const int idx = findFile(nm);
    if (idx < 0) { reply("NMG ERR NOFILE"); return; }
    char playing[GNAME_MAX + 1] = {0};
    if (gifOk && cur >= 0 && idx != cur) strcpy(playing, names[cur]);
    if (gifOk && idx == cur) closeGif();
    char path[64]; snprintf(path, sizeof(path), "/gif/%s", nm);
    FFat.remove(path);
    scanFiles();
    replyOK();
    if (!gifOk) playIndex(nFiles ? min(idx, nFiles - 1) : 0);   // 流していたものを消した → 次へ
    else cur = findFile(playing);                                // 番号がずれるので、名前で探し直す
  } else if (!strncmp(cmd, "PLAY ", 5)) {
    const int idx = findFile(cmd + 5);
    if (idx < 0) { reply("NMG ERR NOFILE"); return; }
    playIndex(idx); replyOK();
  } else {
    reply("NMG ERR CMD");
  }
}

static void pollUsb() {
  for (int guard = 0; guard < 64; ++guard) {
    int av = Serial.available();
    if (av <= 0) break;
    if (rxActive && !rxWaitDone) {                         // データを受け取っている最中
      const uint32_t want = min((uint32_t)min(av, CHUNK), rxSize - rxGot);
      const int n = (int)Serial.read(rxBuf, want);
      if (n <= 0) break;
      rxFile->write(rxBuf, n);
      rxCrc = crcAdd(rxCrc, rxBuf, n);
      rxGot += n; rxSinceAck += n; rxLast = millis();
      if (rxSinceAck >= (uint32_t)CHUNK || rxGot >= rxSize) {
        rxSinceAck = 0;
        char o[32]; snprintf(o, sizeof(o), "NMG ACK %u", (unsigned)rxGot); reply(o);
      }
      if (rxGot >= rxSize) rxWaitDone = true;
      showProgress();
      continue;
    }
    const int c = Serial.read();
    if (c < 0) break;
    if (c == '\r') continue;
    if (c == '\n') { line[lineLen] = 0; handleLine(line); lineLen = 0; continue; }
    if (lineLen < (int)sizeof(line) - 1) line[lineLen++] = (char)c;
  }
  if (rxActive && millis() - rxLast > 8000) rxAbort("TIMEOUT");
}

// GIF を1コマ進める(時間が来ていれば)
static void stepGif() {
  if (!gifOk) { delay(10); return; }
  if (infoShown && (int32_t)(millis() - infoUntil) >= 0) drawCanvas();   // 名前と容量を消す(ゆっくりの GIF でも時間どおりに)
  const uint32_t now = millis();
  if ((int32_t)(now - nextAt) < 0) { delay(1); return; }
  const int d = nextFrame();
  if (d == -2) { autoSwitch = true; playIndex(cur + 1); autoSwitch = false; return; }   // 1回流し終わった → 次の GIF へ
  if (d < 0) {                                             // 読めなくなった → 次の GIF へ
    Serial.printf("# broken %s\n", names[cur]);
    closeGif();
    if (nFiles > 1) { autoSwitch = true; playIndex(cur + 1); autoSwitch = false; } else if (gMode != G_WIFI) { const char* L[] = {"GIF を読めませんでした"}; message(L, 1); }
    return;
  }
  drawCanvas();
  nextAt = ((int32_t)(millis() - (now + d)) > 200) ? millis() : now + d;   // 遅れすぎたら、そこから数え直す
}

// ---------------------------------------------------------------- 設定の画面(明るさ / Wi-Fi で受け取る)
static const char* const MENU_LABEL[MENU_N] = {"明るさ", "終わったら", "名前の表示", "向き", "この GIF を消す", "Wi-Fi で受け取る", "USB で受け取る", "とじる"};
static int menuRowH() { return roundScr ? 33 : 22; }
// 設定をしまう / 読む(/gif.cfg)
static void saveCfg() {
  if (!fsOk) return;
  File f = FFat.open("/gif.cfg", "w");
  if (!f) return;
  char s[32]; snprintf(s, sizeof(s), "NGCFG 3 %d %d %d %d\n", brightIdx, autoNext ? 1 : 0, infoMode, flip180 ? 1 : 0);
  f.write((const uint8_t*)s, strlen(s));
  f.close();
}
static void loadCfg() {
  if (!fsOk) return;
  File f = FFat.open("/gif.cfg", "r");
  if (!f) return;
  char s[32] = {0};
  f.read((uint8_t*)s, sizeof(s) - 1);
  f.close();
  int v, b, n, im = 0, fl = 0;
  const int got = sscanf(s, "NGCFG %d %d %d %d %d", &v, &b, &n, &im, &fl);
  if (got >= 3 && v >= 1 && v <= 3) {
    if (b >= 0 && b < (int)sizeof(brightSteps)) brightIdx = b;
    autoNext = n != 0;
    if (v >= 2 && got >= 4 && im >= 0 && im < 3) infoMode = im;
    if (v >= 3 && got >= 5) flip180 = fl != 0;
  }
}
static int menuY0()   { return H / 2 - (MENU_N * menuRowH()) / 2 + menuRowH() / 2 + 8; }
static void drawMenu() {
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setTextDatum(middle_center);
  M5.Display.setFont(&fonts::lgfxJapanGothicP_24);
  M5.Display.setTextColor(0x7F38);
  M5.Display.drawString("設定", W / 2, menuY0() - menuRowH() - 6);
  M5.Display.setFont(roundScr ? &fonts::lgfxJapanGothicP_20 : &fonts::lgfxJapanGothicP_16);
  const int rw = roundScr ? 300 : 240, rh = menuRowH(), x0 = W / 2 - rw / 2;
  for (int i = 0; i < MENU_N; ++i) {
    const int y = menuY0() + i * rh;
    const bool on = i == menuSel;
    if (on) M5.Display.fillRect(x0, y - rh / 2 + 4, rw, rh - 8, 0x1A49);
    M5.Display.setTextColor(on ? TFT_WHITE : 0xAD55);
    if (i == MENU_N - 1) { M5.Display.setTextDatum(middle_center); M5.Display.drawString(MENU_LABEL[i], W / 2, y); continue; }
    M5.Display.setTextDatum(middle_left);
    M5.Display.drawString(MENU_LABEL[i], x0 + 12, y);
    char v[32]; snprintf(v, sizeof(v), "%s", i == 0 ? (brightIdx == 0 ? "1" : brightIdx == 1 ? "2" : brightIdx == 2 ? "3" : "4") : i == 1 ? (autoNext ? "次の GIF へ" : "くり返す") : i == 2 ? INFO_NAME[infoMode] : i == 3 ? (flip180 ? "さかさま" : "そのまま") : "→");
    M5.Display.setTextColor(on ? 0x7F38 : TFT_WHITE);
    M5.Display.drawString(v, x0 + rw - 12 - M5.Display.textWidth(v), y);
  }
  M5.Display.setFont(&fonts::lgfxJapanGothicP_16);
  M5.Display.setTextDatum(middle_center);
  M5.Display.setTextColor(0x7BEF);
  const int hy = menuY0() + MENU_N * rh - rh / 2 + (roundScr ? 20 : 12);
  if (roundScr) { M5.Display.drawString("A: 次の項目   B: 変える", W / 2, hy); M5.Display.drawString("A 長押し: とじる", W / 2, hy + 20); }
  else M5.Display.drawString("項目をタップで変える", W / 2, hy);
}

// ---- この GIF を消す(たしかめる画面)
static void openConfirm() {
  if (nFiles <= 0 || cur < 0 || cur >= nFiles) return;
  gMode = G_CONFIRM; confirmT = millis();
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setTextDatum(middle_center);
  M5.Display.setFont(&fonts::lgfxJapanGothicP_24);
  M5.Display.setTextColor(TFT_WHITE);
  M5.Display.drawString("この GIF を消しますか?", W / 2, H / 2 - 30);
  M5.Display.setFont(&fonts::lgfxJapanGothicP_16);
  M5.Display.setTextColor(0xAD55);
  M5.Display.drawString(names[cur], W / 2, H / 2 + 4);
  M5.Display.setTextColor(0x7F38);
  M5.Display.drawString(roundScr ? "B: 消す      A: やめる" : "右をタップ: 消す   左: やめる", W / 2, H / 2 + 36);
}
static void doDelete() {
  char path[64]; snprintf(path, sizeof(path), "/gif/%s", names[cur]);
  Serial.printf("# delete %s (on device)\n", names[cur]);
  closeGif();
  FFat.remove(path);
  const int at = cur;
  scanFiles();
  gMode = G_PLAY;
  playIndex(nFiles ? min(at, nFiles - 1) : 0);  // 消したところの次の GIF を流す
}
// ---- USB の送り方
static void openUsbInfo() {
  gMode = G_USBINFO; confirmT = millis();
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setTextDatum(middle_center);
  const bool big = min(W, H) >= 400;
  const int lh = big ? 27 : 19;
  int y = H / 2 - lh * 5 + (big ? 6 : 8);
  M5.Display.setFont(big ? &fonts::lgfxJapanGothicP_24 : &fonts::lgfxJapanGothicP_20);
  M5.Display.setTextColor(0x7F38);
  M5.Display.drawString("USB で受け取る", W / 2, y); y += lh + (big ? 10 : 4);
  M5.Display.setFont(&fonts::lgfxJapanGothicP_16);
  static const char* const L[] = {
    "① USB ケーブルで PC につなぐ", "② PC の Chrome か Edge で", "gif.html を開く", "(zip なら docs の中)",
    "③「本体につなぐ」を押す", "④ GIF をページに落とす" };
  for (int i = 0; i < 6; ++i) {
    M5.Display.setTextColor(i == 2 || i == 3 ? 0xAD55 : TFT_WHITE);
    M5.Display.drawString(L[i], W / 2, y); y += lh;
  }
  M5.Display.setTextColor(0x7BEF);
  M5.Display.drawString("シリアルモニタは閉じておく", W / 2, y); y += lh + (big ? 6 : 2);
  M5.Display.drawString(roundScr ? "A か B: もどる" : "タップ: もどる", W / 2, y);
}

// ---------------------------------------------------------------- Wi-Fi で受け取る
// 本体が Wi-Fi(アクセスポイント)を出して、スマホや PC のブラウザに http://192.168.4.1 のページを出す。
// 名前とパスワードは本体ごとに決まっていて(毎回同じ。NullSlide と同じ)、画面の QR コードを読めば入れる。
// ==== PAGE BEGIN ====
// ページ(gif_wifi.html を gzip したもの。tools/wifi-pages/genpage.py で作り直せます)
static const uint8_t PAGE_GZ[3476] = {
  0x1f,0x8b,0x08,0x00,0x00,0x00,0x00,0x00,0x02,0x03,0xa5,0x19,0x6b,0x77,0xd4,0xc6,0xf5,0xbb,0x7f,0xc5,
  0xb0,0x38,0x91,0x14,0x76,0xb5,0x0f,0x93,0xb5,0xd1,0x7a,0x9d,0x83,0x29,0x04,0x52,0x0c,0x1c,0x0c,0x6d,
  0x52,0x4a,0xb0,0x76,0x35,0xda,0x1d,0x5b,0x2b,0xa9,0xd2,0x2c,0xb6,0xe3,0xb8,0xc7,0xbb,0x21,0x09,0x09,
  0xa6,0xe4,0xd1,0x84,0xd2,0x24,0x27,0xf9,0x90,0x16,0x9a,0xb6,0xd0,0x73,0x9a,0xe6,0xd0,0x9e,0x24,0xfd,
  0xd0,0x7f,0x12,0xb1,0x24,0xf9,0x17,0xbd,0x77,0x46,0xd2,0xbe,0x6c,0x03,0xed,0x17,0x79,0x75,0xe7,0xce,
  0x7d,0x3f,0xe5,0xd9,0x7d,0x96,0x57,0xe7,0xeb,0x3e,0x25,0x4d,0xde,0x72,0xe6,0x26,0x66,0xf1,0x0f,0x71,
  0x4c,0xb7,0x51,0xcd,0x2c,0x9b,0x19,0x04,0x50,0xd3,0x82,0x3f,0x2d,0xca,0x4d,0x52,0x6f,0x9a,0x41,0x48,
  0x79,0x35,0xd3,0xe6,0x76,0x6e,0x26,0x93,0x80,0x5d,0xb3,0x45,0xab,0x99,0xcb,0x8c,0xae,0xfa,0x5e,0xc0,
  0x33,0xa4,0xee,0xb9,0x9c,0xba,0x80,0xb6,0xca,0x2c,0xde,0xac,0x5a,0xf4,0x32,0xab,0xd3,0x9c,0x78,0xc9,
  0x12,0xe6,0x32,0xce,0x4c,0x27,0x17,0xd6,0x4d,0x87,0x56,0x8b,0x59,0x92,0xdc,0xcb,0xd9,0x8c,0x57,0xeb,
  0xde,0x65,0x1a,0x20,0x61,0xce,0xb8,0x43,0xe7,0x4e,0xb5,0x1d,0xe7,0x79,0x66,0xcf,0xe6,0xe5,0xeb,0xc4,
  0x6c,0xc8,0xd7,0xf1,0xaf,0x11,0x78,0x1e,0x27,0x1b,0x24,0x97,0xab,0x35,0x8c,0xfd,0x85,0x5a,0xc1,0x2a,
  0x96,0x2a,0xf0,0xe6,0x9b,0x2e,0x75,0x8c,0xfd,0xc5,0xa9,0x62,0xb9,0x48,0x11,0xe0,0x30,0x97,0x1a,0xfb,
  0x4b,0xe5,0x52,0x6d,0x6a,0x1a,0xdf,0x6d,0x40,0xa7,0x65,0x3a,0x43,0xc5,0x69,0xab,0xcd,0xa9,0x65,0xec,
  0x9f,0x31,0x0f,0x15,0xcd,0x29,0x04,0x98,0xf5,0x3a,0x08,0x6e,0xec,0x9f,0xb6,0xad,0x83,0xf5,0x22,0x42,
  0x98,0xbb,0x02,0x0c,0xca,0xa5,0x42,0xf1,0x10,0xbe,0xd6,0x4c,0xc0,0xb7,0x0b,0xd3,0x66,0xd9,0xac,0x4c,
  0xc0,0xab,0x67,0xad,0x1b,0x99,0xe3,0x2c,0x30,0x1b,0xcc,0xf5,0xc8,0xa2,0xe9,0x86,0x99,0x6c,0xe6,0x94,
  0xc7,0xe5,0x6f,0xf2,0xc2,0x99,0x4c,0x36,0x5c,0x0f,0x39,0x6d,0xe5,0xda,0x2c,0x1b,0x02,0x28,0x17,0xd2,
  0x80,0xd9,0x82,0xb9,0xe7,0x7a,0x46,0x9b,0x89,0xbf,0xa1,0x6f,0xd6,0x69,0x76,0x81,0xba,0x8e,0x97,0x4d,
  0xdf,0x2b,0x60,0x48,0xc7,0x0b,0xc0,0x52,0x4d,0xda,0xa2,0x86,0x65,0x06,0x2b,0x15,0xb2,0x39,0x81,0x3c,
  0x41,0xf5,0x96,0x19,0x00,0x4f,0xa3,0x50,0x21,0x35,0xb3,0xbe,0xd2,0x08,0xbc,0xb6,0x6b,0x19,0x97,0xcd,
  0x40,0x45,0x9b,0x68,0xf1,0xdd,0x18,0x60,0x23,0xc0,0x06,0xaf,0x18,0xc5,0x83,0xfe,0x5a,0xbe,0xa8,0x97,
  0x49,0x8c,0x09,0xb4,0x34,0x24,0xaa,0xaf,0x06,0xa6,0x2f,0xa8,0xae,0x49,0x47,0x19,0xe5,0x83,0x05,0x7f,
  0xad,0x92,0xb2,0x21,0x66,0x9b,0x7b,0x15,0xe2,0x9b,0x96,0xc5,0xdc,0x86,0x51,0x9c,0xf1,0xd7,0x48,0xb1,
  0x0c,0x0f,0x89,0x66,0xb1,0xd0,0x77,0xcc,0x75,0xc3,0x76,0x28,0xbc,0xe1,0x33,0x67,0xb1,0x80,0xd6,0x39,
  0xf3,0x5c,0x03,0x44,0x69,0xb7,0xdc,0x0a,0x69,0x98,0xbe,0x10,0x00,0x19,0x36,0x8b,0xc0,0x0d,0x45,0xca,
  0x85,0xec,0x15,0x6a,0x94,0x86,0x98,0x01,0x02,0xd1,0xc3,0x76,0x0d,0x50,0x06,0xd5,0x10,0xee,0x8a,0x35,
  0x91,0xd7,0x8a,0x25,0xfd,0x59,0x49,0x4f,0xaf,0x79,0x6b,0x80,0x3e,0x66,0x0b,0x11,0x11,0x70,0xa7,0xe6,
  0x05,0x16,0x0d,0x8c,0x22,0x48,0x1c,0x7a,0x0e,0xb3,0x62,0x03,0x60,0x7c,0xa4,0xa7,0xb9,0xc0,0xb4,0x58,
  0x3b,0x34,0x8a,0x42,0x98,0x54,0xd5,0x12,0xaa,0x7a,0xf0,0xc9,0xb4,0x2c,0xc4,0x5a,0x96,0x86,0xb4,0x2c,
  0x16,0x11,0xec,0x50,0xce,0x81,0x19,0xfa,0x18,0xe9,0xeb,0xc5,0x12,0x6d,0x55,0x76,0x54,0x74,0xc0,0x1e,
  0xa0,0xa0,0x19,0x00,0xb1,0x26,0x65,0x8d,0x26,0x37,0x66,0x90,0xd0,0xb0,0xd4,0xc2,0x10,0x03,0xfa,0xef,
  0x2f,0x98,0x85,0x7a,0xb1,0xf0,0x28,0xd5,0x31,0xe1,0x6c,0xc7,0x5b,0x35,0x9a,0xcc,0xb2,0xa8,0x9b,0xb0,
  0x9a,0x63,0xc0,0x2c,0xd1,0xb7,0xe6,0x78,0x75,0x88,0xbd,0x98,0x77,0xb1,0x50,0x78,0xaa,0x42,0x64,0x94,
  0xec,0x14,0x7e,0x32,0x89,0x80,0x34,0x0f,0x20,0xe0,0x99,0x30,0x8e,0xc0,0x26,0xfa,0x54,0x28,0xe8,0xbb,
  0xed,0x56,0x6c,0x97,0xd8,0x83,0xb1,0x48,0x18,0xfa,0xda,0xce,0xa6,0x80,0x80,0xc0,0x5b,0xa3,0x21,0x21,
  0x22,0x1b,0x28,0x06,0xde,0xea,0x80,0xbc,0xd2,0x3f,0xe8,0x08,0x61,0x27,0xe1,0x28,0x0c,0x70,0x03,0x1f,
  0x15,0x62,0x3a,0xac,0xe1,0xe6,0x18,0x64,0x65,0x68,0xa0,0xa8,0x34,0x90,0x5a,0x73,0x17,0x68,0x98,0xbe,
  0x4f,0x4d,0x10,0xbc,0x4e,0x0d,0xd7,0x73,0xe9,0xb0,0x49,0x8b,0xf5,0x52,0xa1,0x74,0x68,0x87,0xe4,0x7a,
  0x92,0xf0,0x9a,0x19,0x8c,0xae,0x43,0x69,0x70,0x09,0x73,0x94,0x0b,0x05,0xf1,0x3a,0x96,0x9f,0x20,0x9c,
  0xee,0x07,0x0c,0x22,0x62,0x7d,0xa7,0x30,0x4f,0x6d,0x3e,0x28,0x19,0xd4,0xae,0x3e,0xf3,0xc1,0x83,0x14,
  0x3b,0x26,0x1c,0xb6,0x4c,0xc7,0x01,0xb2,0x89,0x50,0x28,0x80,0x0c,0xe1,0xf1,0x54,0x13,0x90,0x55,0x19,
  0x09,0xcf,0x16,0x44,0x68,0xd6,0xda,0x9c,0x83,0x8f,0x6d,0xaf,0xde,0x0e,0x73,0x97,0x59,0xc8,0x6a,0x0e,
  0xcd,0x42,0x13,0xa9,0x41,0x2d,0x96,0xc0,0x55,0xc6,0x9b,0x0c,0xad,0xeb,0xb5,0xb9,0x28,0xc9,0xa5,0x11,
  0x43,0xa5,0x12,0xc5,0x08,0x39,0xcf,0xb6,0xa1,0xd3,0x20,0x9e,0x90,0x72,0xd9,0xab,0x85,0x63,0x0e,0xde,
  0x23,0x01,0xcb,0xfd,0x7b,0x4f,0x72,0x6d,0x6a,0x67,0x9d,0x63,0x42,0xc3,0x29,0x28,0x4f,0x08,0x9e,0xe8,
  0xde,0xca,0x48,0x5c,0xf6,0x2d,0x2c,0x11,0x68,0x10,0x8c,0x60,0x40,0x27,0x11,0x0e,0x68,0xa3,0xe1,0x1d,
  0x16,0x02,0x4b,0xec,0x6d,0x71,0xd0,0xf5,0x33,0x3f,0xf1,0x89,0xb0,0xb4,0xc3,0xc6,0xb4,0xd9,0x29,0x98,
  0xd3,0xc8,0x4f,0x2e,0x4f,0x83,0xb9,0xd3,0x4a,0x90,0xe3,0x9e,0xbf,0x5b,0xa4,0xb6,0x98,0x9b,0x4b,0x33,
  0x1b,0xf9,0x19,0x36,0x0b,0x40,0xb6,0x7a,0x93,0x39,0x16,0x06,0x5e,0x9f,0x82,0xac,0x4a,0x36,0x7a,0x15,
  0x25,0x31,0x8a,0x23,0xb7,0xc7,0x0a,0x0b,0xa7,0x6b,0x3c,0x97,0x42,0xa9,0xe3,0x30,0x3f,0x64,0x50,0x10,
  0x56,0x9b,0x20,0xbd,0x28,0x87,0xa8,0xbd,0x4c,0x51,0x59,0x1b,0xa6,0x46,0x2b,0x03,0x32,0x0c,0xf7,0xe8,
  0x0a,0xb2,0x58,0x0f,0x5d,0x11,0xb2,0x49,0xa3,0x62,0xe5,0xf1,0x38,0x7d,0x8c,0xae,0x32,0xd6,0x8c,0x76,
  0xf7,0xdf,0x6c,0x3e,0x9e,0x49,0x66,0xf3,0xf1,0xa8,0x84,0x49,0x8b,0xa3,0x91,0x09,0x11,0x5f,0x77,0xcc,
  0x30,0x84,0x41,0x08,0x94,0xc2,0xa9,0xc6,0x62,0x97,0xe7,0x66,0x9b,0xc5,0xfe,0x5c,0x03,0xbf,0x11,0x98,
  0xe0,0x41,0xcf,0xcb,0xcc,0x3d,0xfc,0xf8,0x2f,0x0f,0xbe,0x79,0x3f,0xea,0xdc,0x25,0x3f,0x67,0xb9,0x63,
  0x8c,0x44,0x9d,0xdb,0xe4,0xf9,0x13,0xc7,0x48,0xd4,0x7d,0xef,0xc7,0xad,0x4e,0xd4,0xbd,0xa6,0x4e,0xf5,
  0xae,0xbe,0x11,0x75,0x3e,0x88,0xba,0x37,0xa2,0xee,0x5b,0x51,0xe7,0x8b,0xa8,0x73,0x25,0xea,0xdc,0x89,
  0xb6,0x3a,0xe3,0x57,0xef,0xfd,0xf0,0xe6,0x17,0xbd,0x6b,0x1f,0x00,0x8d,0xde,0xd5,0x37,0xa3,0xee,0x76,
  0xd4,0xf9,0x36,0xea,0xdc,0xd2,0x66,0xf3,0x42,0x14,0xf1,0x9c,0x98,0xf5,0x13,0xfe,0xc2,0x3c,0xa0,0x6b,
  0x86,0x30,0xab,0x9a,0x69,0x85,0x8d,0x0c,0x91,0xde,0x03,0x54,0x1f,0xc7,0x2f,0x99,0x33,0x09,0x3a,0xf4,
  0x5c,0x31,0x22,0x96,0xe6,0x7a,0x77,0xff,0xf9,0xe3,0x9b,0x37,0x40,0x9f,0x92,0xd4,0x32,0xc5,0x30,0x61,
  0x9a,0x9b,0x65,0x82,0x5c,0xdd,0xf4,0xe7,0xc5,0x6b,0x9e,0xc5,0x9c,0x07,0x31,0xa1,0xba,0x67,0x12,0xb4,
  0x73,0x10,0x29,0x99,0xb9,0xef,0xb6,0x7e,0xbb,0xa3,0x80,0x99,0x39,0x61,0x8d,0xce,0x3d,0x52,0x3e,0x48,
  0x7a,0x5b,0xd7,0x84,0x4a,0xb7,0x7b,0xaf,0xff,0x21,0xea,0xbe,0x2d,0xd5,0x8b,0xb6,0xba,0x51,0xe7,0xd3,
  0xa8,0xf3,0x59,0xd4,0xb9,0x29,0x25,0x53,0xbf,0xff,0xf2,0x0a,0x39,0xa4,0x4f,0x2f,0xcc,0x6b,0x70,0xb1,
  0xf7,0xc6,0xad,0xef,0x3f,0x86,0xf3,0x3b,0xc2,0x8e,0xef,0xf6,0xde,0xb9,0x1a,0x75,0xe0,0xc7,0x5d,0x20,
  0x03,0x46,0xec,0x7d,0x7e,0x3b,0xea,0x5c,0x07,0xb3,0x4a,0xab,0x77,0xb6,0x7b,0x9f,0xff,0x3e,0xb6,0x30,
  0x80,0xbb,0x57,0x10,0x25,0x26,0x30,0x7c,0xd4,0xb9,0x07,0x47,0x51,0xe7,0x13,0x34,0x73,0xf7,0x2a,0x48,
  0xd3,0xfb,0xdb,0xbb,0xc2,0x3b,0x37,0xf0,0x39,0x20,0x9c,0x34,0x66,0x3e,0xb6,0xe6,0x5e,0x76,0x1d,0xf2,
  0xfb,0xb8,0x79,0xa1,0xed,0x81,0x3d,0x45,0xa5,0x4d,0xef,0x42,0x17,0x8b,0x1b,0x45,0x06,0x62,0x3a,0xa8,
  0x66,0x7c,0x56,0x5f,0xc9,0xa4,0x94,0x3a,0xf7,0xa3,0xce,0x57,0xb3,0x79,0x71,0x07,0x3c,0xe3,0xfa,0x6d,
  0x4e,0x70,0x01,0xa8,0x66,0x6c,0xe6,0x50,0xe9,0x02,0x71,0x83,0x60,0x0d,0xf3,0x61,0x8a,0x07,0x5a,0x0d,
  0x9a,0x6f,0x30,0x3b,0xab,0xc3,0x23,0x43,0x5a,0x6d,0x87,0x33,0xdf,0xa1,0xfd,0xd0,0x90,0x4e,0x1a,0x90,
  0x0b,0xcb,0xb5,0x24,0x25,0x7e,0xcd,0xed,0xe2,0xc7,0xde,0x3b,0x10,0x8f,0xbf,0xeb,0xbd,0x73,0xbd,0xf7,
  0xd6,0x75,0x0c,0xda,0xd8,0xaf,0x0f,0xee,0xbf,0xfd,0xf0,0xa3,0xfb,0xc2,0xa2,0x37,0x53,0x9b,0x1d,0x2c,
  0x97,0xff,0x73,0x13,0x1e,0x04,0xcd,0x89,0x51,0x7f,0xa5,0x7f,0x63,0x5b,0x38,0x66,0x5b,0xc0,0x6e,0xff,
  0xb8,0x05,0x8e,0xb9,0x21,0x62,0xfe,0xfa,0xff,0x60,0xf1,0x7e,0x0a,0x01,0xed,0xd8,0xe2,0x50,0xa3,0x51,
  0x17,0x34,0x90,0x50,0xa6,0xed,0x0c,0x13,0xcb,0x63,0xae,0x23,0xd1,0x7a,0xc0,0x7c,0x3e,0x37,0x01,0x0b,
  0x50,0xc8,0xc9,0x24,0xa9,0x12,0x95,0x59,0x1a,0xa9,0xce,0x11,0xd8,0xb2,0xda,0x2d,0xa8,0xce,0x7a,0x83,
  0xf2,0xa3,0x0e,0xc5,0x9f,0xf3,0xeb,0x27,0x2c,0x3c,0xae,0xc4,0xe8,0x47,0xcf,0x9e,0x85,0x0b,0x1b,0xe4,
  0xd8,0xf9,0x93,0x27,0x0d,0x45,0x86,0x2d,0xa8,0xf6,0xc3,0x57,0x7f,0x4f,0x42,0xe7,0xa3,0xa8,0xfb,0xbe,
  0x92,0x25,0xa7,0x0e,0x2f,0x1c,0x35,0x94,0xa8,0x83,0x42,0x26,0xb6,0xbb,0xf7,0xe0,0x9b,0x7f,0x47,0x9d,
  0xab,0x83,0x68,0x0b,0x87,0x4f,0xbd,0x64,0x28,0x83,0x21,0x7c,0x2b,0xea,0xfc,0x46,0x5a,0x44,0x1d,0xcc,
  0x1c,0x0d,0x69,0x9e,0x3e,0xb6,0x68,0x28,0x89,0xee,0x7f,0x8e,0xed,0x7a,0xf7,0xfb,0x6f,0x20,0x27,0xae,
  0xf7,0x3e,0xfb,0x52,0xd8,0xb8,0x3b,0x2c,0xc8,0xc4,0xe9,0x33,0x47,0x4f,0x0d,0x5c,0x92,0x3e,0xfb,0xe1,
  0xdb,0xaf,0xa3,0x6e,0x27,0x45,0xc3,0x94,0x42,0x2f,0x7e,0x0a,0x4c,0x16,0x0f,0xff,0xec,0xe8,0x93,0xe0,
  0x83,0x50,0x27,0x4e,0x0a,0x55,0x3f,0x19,0x88,0x8e,0xf4,0xfe,0x98,0x40,0x64,0xfe,0xfc,0xe2,0x4b,0x7d,
  0x06,0xdb,0xe4,0xfc,0xe2,0x3c,0x56,0xcf,0xde,0x8d,0x9b,0x98,0xe5,0x37,0x3e,0x04,0xf4,0x07,0xf7,0xff,
  0x2a,0x58,0xdc,0x52,0xc8,0x66,0x62,0xf9,0x56,0x0d,0x3d,0xe5,0x0a,0x47,0xa9,0x2e,0xc9,0xc3,0xec,0x73,
  0x70,0xe6,0xd9,0xe9,0xb2,0xa6,0x73,0xef,0x18,0x5b,0xa3,0x96,0x5a,0xd4,0xc8,0x01,0xa2,0x2c,0xcc,0x03,
  0x8f,0x95,0x01,0x64,0x97,0xcc,0x55,0x13,0x64,0xf2,0x1c,0xd0,0x41,0xb8,0x41,0x16,0x4c,0xde,0xd4,0x61,
  0x93,0x52,0x61,0xbb,0x15,0xbf,0xc5,0xa4,0x16,0x53,0x2e,0x1d,0xd4,0x04,0xb1,0x9f,0xce,0x2b,0x95,0x09,
  0x58,0x05,0x60,0x21,0xb6,0x3d,0x20,0xe9,0x42,0x4b,0xa8,0x4c,0xd8,0x6d,0x57,0xc6,0x65,0xd8,0xf4,0x56,
  0x8f,0x06,0x81,0xca,0x35,0x08,0x89,0x49,0x55,0x81,0x5a,0xac,0x68,0xba,0xcc,0x38,0xc0,0xde,0xc7,0x2b,
  0x7d,0x28,0x36,0xd6,0x23,0x72,0xe7,0x86,0x23,0x4e,0x5e,0x7d,0x95,0x28,0x0a,0xf6,0x26,0x33,0x5c,0x77,
  0xeb,0x24,0xa5,0x69,0xfa,0x4c,0xf5,0x41,0x1e,0x20,0x19,0x6b,0x1e,0x00,0xbe,0xb9,0x6a,0x32,0x4e,0x6c,
  0xca,0xeb,0x4d,0x71,0x9a,0xc5,0x46,0x67,0xc2,0xce,0x69,0x10,0xc5,0xf5,0x60,0x2a,0xf1,0x02,0x0a,0xc6,
  0x4a,0xe3,0x74,0x39,0xbd,0x13,0xe8,0xcb,0xa1,0xe7,0xaa,0x9a,0x5e,0x37,0xf1,0xb2,0x2a,0x0d,0xb8,0x81,
  0x0d,0x04,0xee,0x1e,0x3f,0x77,0xee,0x0c,0x51,0x40,0xd5,0x40,0x0f,0xb9,0xc9,0xdb,0x21,0xd0,0x00,0x22,
  0xcc,0x26,0xea,0xbe,0x00,0x27,0x26,0x10,0x73,0x19,0x1b,0xab,0x46,0x78,0x13,0xe7,0x77,0x97,0xae,0x12,
  0x50,0xd9,0x0b,0x54,0x48,0x85,0x0b,0xe2,0xe8,0x22,0x22,0xa9,0xa9,0x47,0xef,0x46,0xdd,0x3b,0xd1,0x6b,
  0x7f,0x8a,0x5e,0xfb,0xda,0x10,0x94,0x55,0x81,0x84,0x38,0x09,0x0f,0x0d,0x59,0x04,0x94,0xb7,0x03,0x97,
  0x2c,0x57,0x26,0x36,0xfb,0x06,0x6d,0xfb,0x2a,0x7e,0xa7,0xc8,0x92,0xda,0x3a,0xa7,0x61,0x96,0x78,0xae,
  0x8f,0x76,0x88,0x71,0x91,0xf7,0x99,0xc0,0x6b,0xb1,0x90,0xaa,0xaa,0xb7,0x92,0x25,0x6e,0x43,0x68,0x93,
  0x18,0x6a,0x0d,0x3d,0x04,0x38,0x2f,0x2e,0x9c,0x3c,0xce,0xb9,0x7f,0x96,0xfe,0xaa,0x4d,0x43,0xae,0xc2,
  0x04,0xb0,0xa6,0x7b,0x3e,0x75,0x55,0xe5,0xcc,0xe9,0xc5,0x73,0x10,0x1e,0x4a,0x1e,0xac,0x9c,0x6f,0xfb,
  0x8a,0x38,0x82,0x11,0x36,0x46,0x3d,0x0e,0xb3,0x01,0x0d,0x54,0xe5,0xc5,0xdc,0x29,0x90,0x02,0x10,0x51,
  0x98,0x5d,0x70,0x62,0x5f,0xe6,0xce,0x41,0x59,0x46,0x92,0xb0,0x92,0x38,0x0c,0x6c,0x0c,0x6a,0xe4,0xbd,
  0x3a,0xa7,0x38,0x2a,0x06,0xd4,0x6c,0x01,0x8f,0x89,0x35,0xbd,0xed,0x3b,0x9e,0x69,0xe9,0xa0,0x4f,0xe0,
  0x35,0x02,0x1a,0x86,0x18,0x9f,0x54,0x4a,0x4f,0xd0,0xda,0x54,0x77,0xa8,0xdb,0xe0,0xcd,0x23,0x5e,0x0b,
  0x8a,0xbd,0x09,0xc3,0x39,0x79,0xfa,0x69,0xa9,0x3f,0x3c,0xf0,0x18,0xee,0x53,0x0b,0xa2,0x93,0x42,0xc0,
  0x73,0x13,0xd7,0xe5,0x4d,0xa4,0xec,0xb9,0x78,0x82,0xe4,0x62,0x6a,0x18,0xae,0xe8,0xfe,0x8d,0x4d,0x5c,
  0xee,0x70,0x11,0xc1,0xb7,0x17,0x16,0x4f,0x9f,0xd2,0x7d,0xfc,0x32,0xa4,0xae,0xe9,0x20,0x80,0x0f,0x16,
  0xa3,0xd8,0xc2,0xc5,0xf8,0x2b,0x62,0x43,0x08,0x24,0xb1,0xc7,0xc2,0x63,0x2d,0x0d,0x0f,0x44,0x47,0x81,
  0x53,0xc8,0xbe,0x6a,0x95,0x94,0x60,0x23,0xea,0x47,0x8a,0xdb,0x50,0xff,0x9f,0x30,0x59,0xeb,0x87,0x09,
  0x08,0x47,0x9d,0x90,0x12,0x6f,0x45,0x5d,0xee,0x2b,0x4c,0x91,0x70,0xaa,0xf1,0x10,0x37,0x65,0xa0,0xf0,
  0x7c,0x2e,0xda,0xfa,0xf6,0x60,0xf9,0x51,0xc7,0x67,0xaf,0xed,0x64,0xe4,0x82,0x86,0x74,0x2d,0xea,0x76,
  0xb1,0xb0,0xc5,0x13,0x18,0xde,0xd0,0x14,0x4d,0x38,0x30,0xa4,0x50,0x1c,0x44,0x54,0xc2,0x2b,0xe6,0xd9,
  0x40,0xd4,0x86,0xa6,0x4d,0x31,0x60,0x44,0xec,0x62,0xbc,0xa2,0x0b,0xb0,0x02,0x2d,0xf2,0x80,0xa1,0x78,
  0x08,0x06,0xa3,0xc3,0x42,0x50,0xa7,0x6a,0xfe,0x97,0xfa,0x85,0x97,0xf5,0x8b,0xcf,0x4c,0xe6,0x21,0x6a,
  0x94,0x01,0xf8,0x85,0x97,0x0f,0xe7,0x7e,0x61,0xe6,0x5e,0x29,0xe4,0x0e,0xe9,0x97,0x72,0x17,0xf3,0x0d,
  0x38,0xbf,0x34,0x88,0x70,0xe9,0xc0,0x38,0xec,0xe5,0x0b,0xfa,0xa5,0x5f,0x5f,0x3c,0x20,0x69,0xc9,0xc4,
  0x55,0x6b,0x50,0xd7,0x30,0xd7,0xf3,0x17,0x52,0x82,0x40,0x4d,0x43,0xdb,0x5e,0xb8,0xa8,0xc5,0x81,0x46,
  0x66,0x49,0x49,0x13,0xe3,0x32,0xa6,0x8e,0x15,0xa7,0xce,0x4f,0x4c,0x4e,0x55,0x2d,0x4b,0xfc,0x7e,0xfd,
  0x4c,0x94,0xd0,0x20,0x7c,0xac,0x45,0x6e,0x06,0x5c,0x2d,0x01,0xb7,0x02,0xb2,0x43,0x25,0x97,0x60,0xde,
  0xb8,0x34,0xb9,0x61,0x61,0xd7,0x3c,0x06,0xf5,0xf1,0x25,0xd8,0xcc,0x55,0x6d,0x73,0x72,0xc3,0x57,0x05,
  0x6c,0x01,0x12,0xa5,0xa9,0x62,0x39,0x2d,0x0e,0x40,0x25,0x23,0x6d,0xf3,0x52,0x0a,0x39,0xee,0xb5,0x83,
  0x10,0x41,0xfd,0x9b,0xcc,0x85,0xb1,0x7f,0x18,0xb6,0x48,0x41,0x60,0x4b,0xc0,0x96,0xb0,0x7a,0xc6,0xa5,
  0xa1,0xa6,0x87,0x90,0x7d,0x54,0x2d,0x64,0xc9,0x54,0x59,0x94,0x6e,0x1c,0x83,0x14,0x74,0xd3,0x48,0x7d,
  0x0d,0xa8,0x0d,0xe1,0x8f,0x02,0x6d,0x4c,0xc4,0x55,0x5d,0x16,0x4a,0x2c,0xbc,0xb2,0x30,0x20,0x18,0xb5,
  0x83,0xda,0x2d,0xc7,0x61,0x30,0xb8,0xd8,0x1f,0x74,0xf9,0x85,0x04,0x27,0x06,0x40,0x91,0x79,0x08,0x2d,
  0x45,0xb6,0x12,0xe6,0xaa,0xc5,0x42,0x21,0x2b,0x5a,0x85,0xde,0x0e,0x45,0xae,0x0e,0xa0,0x3d,0x03,0x6d,
  0xa5,0x80,0x8d,0xa7,0x30,0xda,0xb1,0x9e,0x52,0x12,0x56,0x98,0x8f,0xc0,0x8b,0xb9,0x10,0xe2,0xc7,0xcf,
  0x2d,0x9c,0x44,0xeb,0x3e,0xbc,0x0b,0x51,0xf9,0x36,0x99,0xad,0xcd,0x4d,0x6e,0x40,0xef,0x4a,0xdb,0x56,
  0xc2,0x49,0x52,0xcf,0xf5,0xd9,0x82,0x65,0x66,0xf3,0xb5,0x39,0xe0,0xde,0x7b,0xfd,0x0e,0x04,0x3b,0x11,
  0xf7,0xfa,0xb8,0xda,0xa6,0x8a,0x4d,0x7a,0x72,0x43,0x80,0xea,0xd0,0xf5,0xf8,0x26,0xce,0x19,0xda,0x52,
  0xd2,0x3c,0x70,0xd1,0xdd,0xc1,0x2c,0x08,0x56,0x20,0x36,0x60,0xd0,0xaa,0x62,0x5f,0x13,0x93,0x16,0x96,
  0xd0,0xb6,0x33,0xd2,0xdd,0xa0,0xb1,0xc9,0x16,0x82,0x57,0x74,0x81,0x17,0x47,0x5d,0x3f,0xe2,0x60,0x4b,
  0xae,0xf6,0x27,0xae,0x3a,0x94,0x4a,0x4e,0xe3,0xa1,0x4b,0x55,0x1c,0x86,0x74,0x1d,0xa6,0x8b,0xc9,0x0f,
  0x73,0x0b,0xa9,0xe2,0x3c,0xaa,0x08,0xf0,0x08,0x3b,0x91,0xad,0x9f,0xf5,0xe7,0xcc,0xe1,0x91,0x43,0x08,
  0x88,0x5f,0x8c,0x20,0x83,0x1d,0x26,0xf6,0x41,0x18,0xb9,0x89,0x2a,0xe5,0xb0,0x89,0x67,0x93,0xbe,0x9c,
  0xfd,0xb6,0xfb,0x68,0x01,0x63,0x44,0x77,0x0f,0x3c,0x58,0x98,0x5d,0x54,0xc5,0x1d,0xd6,0xc4,0x76,0x15,
  0x84,0x0d,0xab,0x61,0xeb,0x6e,0x42,0x32,0x7c,0x0c,0x92,0xe1,0x08,0xc9,0x50,0x41,0xd8,0x30,0xc9,0x95,
  0x9a,0x6a,0xeb,0x61,0x2a,0xa9,0xbf,0x07,0x59,0xf9,0x7d,0x08,0x09,0xfb,0xc3,0x84,0x71,0x57,0x11,0xdf,
  0x9e,0x14,0x3c,0x1a,0xb1,0xfc,0xc3,0x7f,0x74,0x70,0x4c,0xab,0x4c,0xf8,0x50,0x97,0xeb,0x90,0x82,0x2b,
  0x18,0x38,0x22,0xe3,0x92,0x7e,0x24,0x7b,0xd0,0x68,0x30,0xe1,0xc7,0x91,0xe7,0xdc,0x2a,0x56,0x7e,0xea,
  0xd6,0x3d,0x8b,0x9e,0x3f,0x7b,0x02,0x7b,0x1f,0x54,0x77,0x90,0x06,0x4c,0xa1,0x8d,0x35,0xa6,0x64,0xda,
  0xa2,0x7a,0x0b,0x1a,0x28,0xac,0x3b,0x02,0x23,0x1d,0x10,0xad,0xc7,0x52,0xce,0xda,0x5d,0x39,0x6b,0x4c,
  0xb9,0xaf,0xae,0x4a,0xe5,0xac,0x5d,0x95,0x13,0x51,0x0e,0xfc,0x6d,0x16,0xb4,0xd4,0xa5,0x68,0x6b,0x7b,
  0x72,0x03,0x64,0xdf,0x8c,0xb6,0x60,0xd5,0x79,0x2f,0x69,0x36,0x90,0xc0,0x6f,0x09,0x5a,0xc9,0xa2,0xd4,
  0xb9,0xf6,0xdc,0x12,0x0c,0x98,0xb2,0x76,0x55,0x76,0xb1,0x90,0x45,0x9d,0x47,0x1a,0x28,0x1e,0xf1,0x92,
  0x92,0xf6,0xd8,0x16,0x83,0x14,0x8a,0x13,0xc2,0xcd,0x12,0x98,0xb3,0xfc,0x2c,0xb1,0xb4,0xd1,0x3c,0x81,
  0xda,0xb9,0x89,0x85,0x09,0x17,0x4d,0xa8,0x4a,0x60,0x82,0xa6,0xe9,0x36,0xe8,0xa8,0x0d,0x62,0xf3,0x8b,
  0xec,0x81,0xb3,0x0b,0xba,0xae,0xf7,0x2f,0x09,0xe8,0x45,0x31,0x06,0xc7,0x90,0xcb,0xa6,0xd3,0xa6,0x71,
  0x8d,0x18,0x4d,0xc3,0x91,0x0c,0xc4,0xcf,0x79,0xbb,0xfb,0x14,0x96,0x55,0x74,0x28,0x7e,0xb8,0x1b,0x72,
  0x29,0x00,0x94,0x24,0x26,0xf8,0xa3,0xef,0x4b,0x44,0xfc,0x68,0xf8,0x48,0x54,0x40,0x1a,0x89,0x1e,0x68,
  0x0d,0x09,0x05,0x90,0xdd,0xd9,0x83,0x04,0x4b,0x08,0xc4,0x06,0x46,0x74,0x30,0x31,0x4a,0x1f,0x43,0x78,
  0x16,0xcf,0x35,0x61,0x2b,0x5c,0xc8,0xc1,0x56,0x3e,0xb4,0x79,0x3c,0x82,0xd7,0x7e,0xb5,0x91,0xbc,0xd3,
  0x99,0x03,0x22,0x41,0x0e,0xa9,0x7c,0x24,0x7c,0x05,0x22,0xb4,0x17,0x42,0x1e,0xfe,0xeb,0xc3,0x5e,0xf7,
  0x16,0x2c,0x55,0xdf,0x6d,0xfd,0x11,0x4c,0x23,0xc2,0x2d,0xa6,0x26,0x26,0x9a,0xb8,0xf7,0x9f,0x67,0x2e,
  0x9f,0x39,0x1c,0x04,0xe6,0xba,0x1a,0xaf,0x1b,0xba,0x89,0x6f,0xf3,0x6d,0xdb,0x86,0x89,0x57,0x4b,0x45,
  0x08,0x59,0x23,0x1d,0x6e,0x74,0x1b,0x06,0xf2,0x23,0x4d,0x33,0x38,0x02,0x21,0xaa,0x82,0xef,0x05,0x45,
  0xfc,0xbf,0x90,0xb8,0x8b,0x6d,0xaa,0x9c,0x6c,0x14,0x78,0x0f,0xa7,0x45,0xdc,0x77,0x67,0xa6,0x4d,0x05,
  0x27,0xdc,0x21,0xd8,0x21,0x53,0x19,0xdf,0x33,0xe2,0xed,0xf8,0x36,0x7e,0xb9,0x19,0xa9,0xec,0x3b,0xac,
  0x3b,0xe3,0x5d,0x1c,0x39,0x4b,0xa1,0xe2,0xc1,0xe7,0x00,0x99,0x2a,0x4d,0x97,0x67,0xc8,0x1c,0xcc,0xab,
  0x49,0xf7,0x5c,0x96,0xad,0x73,0x8c,0xf9,0xd2,0x6e,0x2b,0xbe,0x3a,0xb9,0x01,0xc5,0x75,0x90,0xae,0xb6,
  0xa9,0x2d,0x01,0x3b,0x29,0xc6,0xe8,0x22,0xa3,0xae,0xc4,0x05,0x03,0xfd,0x3e,0x3a,0x4c,0xac,0xc4,0xc3,
  0x41,0x3a,0x17,0x14,0x92,0xb9,0x60,0xcc,0xa9,0x4b,0x93,0x1b,0x48,0x77,0x93,0x40,0xff,0x1e,0xd8,0x5a,
  0x13,0x0a,0x9b,0x4f,0xe1,0x50,0x04,0x52,0xec,0xc0,0x46,0xc1,0x7f,0x09,0xed,0x49,0x52,0x7c,0xc0,0x92,
  0x2a,0xe2,0x72,0xbf,0xb3,0x8a,0x03,0x19,0x77,0x12,0xbb,0xa6,0x69,0x59,0xaa,0xe2,0xad,0xa0,0xa5,0x87,
  0x8b,0xcf,0x5e,0xa2,0xa7,0x05,0x69,0x73,0x67,0x7a,0x30,0xe1,0x2b,0xa2,0x51,0xe3,0x80,0x17,0x17,0xb7,
  0x64,0x81,0x95,0x5b,0xd3,0x78,0x69,0x43,0x01,0x2a,0x4f,0x84,0x3e,0x9b,0x4f,0xbe,0x05,0xc1,0xc0,0x24,
  0xbf,0x07,0xe7,0xe5,0x7f,0xd8,0xff,0x0b,0x2e,0x1c,0x3e,0x37,0x72,0x1f,0x00,0x00,
};
// ==== PAGE END ====

static void wifiIds() {
#if NL_WIFI
  const uint64_t mac = ESP.getEfuseMac();
#else
  const uint64_t mac = 0x0000A1B2C3D4E5F6ull;
#endif
  snprintf(wifiSsid, sizeof(wifiSsid), "Null2-%04X", (unsigned)((mac >> 32) & 0xFFFF));
  uint32_t h = (uint32_t)(mac ^ (mac >> 21) ^ (mac >> 40)) * 2654435761u;
  h ^= h >> 15; h *= 2246822519u; h ^= h >> 13;
  snprintf(wifiPass, sizeof(wifiPass), "%08u", (unsigned)(h % 100000000u));
}

static void wifiDraw() {
  const bool big = min(W, H) >= 400;
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setTextDatum(middle_center);
  M5.Display.setFont(big ? &fonts::lgfxJapanGothicP_24 : &fonts::lgfxJapanGothicP_16);
  M5.Display.setTextColor(0x7F38);
  M5.Display.drawString("Wi-Fi で受け取る", W / 2, YOFF + (big ? 66 : 10));
#if NL_WIFI
  if (!wifiOn) {
    M5.Display.setFont(&fonts::lgfxJapanGothicP_20);
    M5.Display.setTextColor(TFT_WHITE);
    M5.Display.drawString("Wi-Fi を始められませんでした", W / 2, H / 2);
    return;
  }
  char q[96];
  if (wifiQr == 0) snprintf(q, sizeof(q), "WIFI:T:WPA;S:%s;P:%s;;", wifiSsid, wifiPass);
  else             snprintf(q, sizeof(q), "http://192.168.4.1/");
  const int qs = big ? 172 : 92, qy = YOFF + (big ? 88 : 26);
  M5.Display.fillRect(W / 2 - qs / 2 - 6, qy - 6, qs + 12, qs + 12, TFT_WHITE);
  M5.Display.qrcode(q, W / 2 - qs / 2, qy, qs);
  int y = qy + qs + (big ? 22 : 14);
  const int lh = big ? 26 : 17;
  M5.Display.setFont(big ? &fonts::lgfxJapanGothicP_20 : &fonts::lgfxJapanGothicP_16);
  M5.Display.setTextColor(TFT_WHITE);
  M5.Display.drawString(wifiQr == 0 ? "① カメラで読んで Wi-Fi に入る" : "② カメラで読んでページを開く", W / 2, y); y += lh + 4;
  M5.Display.setTextColor(0xCE59);
  char line[64];
  if (wifiQr == 0) {
    snprintf(line, sizeof(line), "Wi-Fi  %s", wifiSsid);  M5.Display.drawString(line, W / 2, y); y += lh;
    snprintf(line, sizeof(line), "パスワード  %s", wifiPass); M5.Display.drawString(line, W / 2, y); y += lh;
  } else {
    M5.Display.drawString("http://192.168.4.1", W / 2, y); y += lh;
    snprintf(line, sizeof(line), "つながっている  %d 台", wifiSta < 0 ? 0 : wifiSta); M5.Display.drawString(line, W / 2, y); y += lh;
  }
  M5.Display.setFont(&fonts::lgfxJapanGothicP_16);
  if (wifiMsg[0]) { M5.Display.setTextColor(0x7F38); M5.Display.drawString(wifiMsg, W / 2, y); }
  y += big ? 22 : 16;
  M5.Display.setTextColor(0x7BEF);
  snprintf(line, sizeof(line), "あと%d分で自動で切れます", wifiShownMin < 1 ? 1 : wifiShownMin);
  M5.Display.drawString(line, W / 2, y); y += 18;
  M5.Display.drawString(roundScr ? "A: QR を切り替え   B: おわる" : "タップ: QR切替  電源: おわる", W / 2, y);
#else
  M5.Display.setFont(&fonts::lgfxJapanGothicP_20);
  M5.Display.setTextColor(TFT_WHITE);
  M5.Display.drawString("ここでは Wi-Fi は使えません", W / 2, H / 2 - 14);
  M5.Display.setTextColor(0xAD55);
  M5.Display.drawString("(本物の本体で使えます)", W / 2, H / 2 + 14);
#endif
}

#if NL_WIFI
static void webJson(int code, const char* body) {
  web->sendHeader("Cache-Control", "no-store");
  web->send(code, "application/json", body);
}
static void webErr(const char* e) { char o[48]; snprintf(o, sizeof(o), "{\"err\":\"%s\"}", e); webJson(strcmp(e, "NOFILE") ? 400 : 404, o); }
static void webInfo() {
  fsStats();
  char o[120];
  snprintf(o, sizeof(o), "{\"app\":\"gif\",\"total\":%u,\"used\":%u,\"count\":%d}", (unsigned)fsTotal, (unsigned)fsUsed, nFiles);
  webJson(200, o);
}
static void webTouch() { wifiLast = millis(); }
static void webList() {
  webTouch();
  web->sendHeader("Cache-Control", "no-store");
  web->setContentLength(CONTENT_LENGTH_UNKNOWN);
  web->send(200, "application/json", "");
  char buf[1024]; int n = snprintf(buf, sizeof(buf), "{\"files\":[");
  for (int i = 0; i < nFiles; ++i) {
    if (n > (int)sizeof(buf) - 80) { web->sendContent(buf, n); n = 0; }
    n += snprintf(buf + n, sizeof(buf) - n, "%s{\"n\":\"%s\",\"s\":%u}", i ? "," : "", names[i], (unsigned)sizes[i]);
  }
  n += snprintf(buf + n, sizeof(buf) - n, "]}");
  web->sendContent(buf, n);
  web->sendContent("");
}
static void upClose(bool keep) {
  if (upFile) { upFile->close(); delete upFile; upFile = nullptr; }
  if (!keep && fsOk) FFat.remove("/gif/~web.tmp");
}
static void upProgress() {
  const int pct = upSize ? (int)((uint64_t)upGot * 100 / upSize) : 100;
  M5.Display.fillRect(W / 2 - 150, H / 2 - 50, 300, 100, TFT_BLACK);
  M5.Display.setFont(&fonts::lgfxJapanGothicP_20);
  M5.Display.setTextDatum(middle_center);
  M5.Display.setTextColor(TFT_WHITE);
  char s[64]; snprintf(s, sizeof(s), "受け取り中  %d%%", pct);
  M5.Display.drawString(s, W / 2, H / 2 - 22);
  M5.Display.setTextColor(0xAD55);
  M5.Display.drawString(upName, W / 2, H / 2 + 6);
  M5.Display.fillRect(W / 2 - 120, H / 2 + 30, 240, 6, 0x4208);
  M5.Display.fillRect(W / 2 - 120, H / 2 + 30, 240 * pct / 100, 6, 0x07FF);
}
// GIF の受け取り(本文がそのまま GIF。名前は X-Name の見出しで)
static void webUpload() {
  HTTPRaw& r = web->raw();
  if (r.status == RAW_START) {
    webTouch();
    upErr = nullptr; upGot = 0; upShownMs = 0;
    upSize = (uint32_t)web->clientContentLength();
    String nm = web->header("X-Name");
    strncpy(upName, nm.c_str(), GNAME_MAX); upName[GNAME_MAX] = 0;
    if (rxActive) upErr = "BUSY";
    else if (!goodName(nm.c_str()) || nm.length() > GNAME_MAX) upErr = "NAME";
    else if (!fsOk) upErr = "NOFS";
    else {
      fsStats();
      const int idx = findFile(upName);
      const size_t freeNow = (fsTotal > fsUsed ? fsTotal - fsUsed : 0) + (idx >= 0 ? sizes[idx] : 0);
      if (upSize == 0 || (size_t)upSize + 32768 > freeNow) upErr = "FULL";
      else if (idx < 0 && nFiles >= MAX_FILES) upErr = "MANY";
    }
    if (!upErr) {
      if (gifOk && cur >= 0 && !strcmp(names[cur], upName)) closeGif();   // 同じ名前を流していたら止める
      FFat.remove("/gif/~web.tmp");
      File f = FFat.open("/gif/~web.tmp", "w");
      if (!f) upErr = "OPEN"; else upFile = new File(f);
    }
    if (!upErr) { wifiPeekUntil = 0; M5.Display.fillScreen(TFT_BLACK); upProgress(); }
  } else if (r.status == RAW_WRITE) {
    if (upFile && !upErr && r.currentSize) {
      if (upFile->write(r.buf, r.currentSize) != r.currentSize) upErr = "SAVE";
      upGot += r.currentSize;
      if (millis() - upShownMs > 250) { upShownMs = millis(); upProgress(); }
    }
  } else if (r.status == RAW_END) {
    webTouch();
    if (upErr || !upFile) { upClose(false); if (!upErr) upErr = "OPEN"; return; }
    upClose(true);
    if (upGot != upSize) { upErr = "SAVE"; FFat.remove("/gif/~web.tmp"); return; }
    char path[64]; snprintf(path, sizeof(path), "/gif/%s", upName);
    FFat.remove(path);
    if (!FFat.rename("/gif/~web.tmp", path)) { FFat.remove("/gif/~web.tmp"); upErr = "SAVE"; return; }
    char playing[GNAME_MAX + 1] = {0};
    if (gifOk && cur >= 0) strcpy(playing, names[cur]);
    scanFiles();
    cur = playing[0] ? findFile(playing) : -1;
    snprintf(wifiMsg, sizeof(wifiMsg), "受け取りました(GIF %d 個)", nFiles);
    Serial.printf("# wifi got %s  %u bytes\n", upName, (unsigned)upGot);
  } else {
    upClose(false);
    upErr = "ABORT";
  }
}
static void webUploadDone() {
  wifiDirty = true;
  if (upErr) webErr(upErr); else webInfo();
  upErr = nullptr;
}
#endif

static void wifiStop();
static void wifiStart() {
  gMode = G_WIFI;
  closeGif();
  wifiQr = 0; wifiSta = -1; wifiMsg[0] = 0; wifiPeekUntil = 0; wifiShownMin = -1;
  wifiLast = millis();
  wifiIds();
#if NL_WIFI
  { const char* L[] = {"Wi-Fi を始めています…"}; message(L, 1); }
  WiFi.mode(WIFI_AP);
  if (!WiFi.softAP(wifiSsid, wifiPass, 6, 0, 3)) {
    Serial.println("# wifi: softAP failed");
    WiFi.mode(WIFI_OFF);
    wifiOn = false; wifiDirty = true;
    return;
  }
  WiFi.setTxPower(WIFI_POWER_11dBm);              // すぐ近くで使うので、電波は弱めに(電池のため)
  web = new WebServer(80);
  static const char* hk[] = {"X-Name"};
  web->collectHeaders(hk, 1);
  web->on("/", HTTP_GET, []() {
    webTouch();
    web->sendHeader("Content-Encoding", "gzip");
    web->sendHeader("Cache-Control", "no-store");
    web->send_P(200, "text/html", (const char*)PAGE_GZ, sizeof(PAGE_GZ));
  });
  web->on("/api/info", HTTP_GET, []() { webTouch(); webInfo(); });
  web->on("/api/list", HTTP_GET, []() { webList(); });
  web->on("/api/up", HTTP_POST, []() { webUploadDone(); }, []() { webUpload(); });
  web->on("/api/del", HTTP_GET, []() {
    webTouch();
    const String an = web->arg("n");               // (一時的な String の中身を指したままにしない)
    const char* nm = an.c_str();
    const int idx = findFile(nm);
    if (idx < 0) { webErr("NOFILE"); return; }
    if (gifOk && idx == cur) { closeGif(); wifiPeekUntil = 0; wifiDirty = true; }
    char playing[GNAME_MAX + 1] = {0};
    if (gifOk && cur >= 0) strcpy(playing, names[cur]);
    char path[64]; snprintf(path, sizeof(path), "/gif/%s", nm);
    FFat.remove(path);
    scanFiles();
    cur = playing[0] ? findFile(playing) : -1;
    webInfo();
  });
  web->on("/api/play", HTTP_GET, []() {
    webTouch();
    const int idx = findFile(web->arg("n").c_str());
    if (idx < 0) { webErr("NOFILE"); return; }
    playIndex(idx);
    webInfo();
  });
  web->onNotFound([]() { webErr("NOFILE"); });
  web->begin();
  wifiOn = true;
  Serial.printf("# wifi on  %s  pass %s  http://192.168.4.1\n", wifiSsid, wifiPass);
#endif
  wifiDirty = true;
}
// Wi-Fi を切って、GIF に戻る
static void wifiStop() {
#if NL_WIFI
  if (web) { web->stop(); delete web; web = nullptr; }
  upClose(false);
  if (wifiOn) { WiFi.softAPdisconnect(true); WiFi.mode(WIFI_OFF); Serial.println("# wifi off"); }
#endif
  wifiOn = false; wifiPeekUntil = 0;
  gMode = G_PLAY;
  playIndex(cur < 0 ? 0 : cur);
}
static void wifiLoop(bool tapped) {
#if NL_WIFI
  if (web) web->handleClient();
  if (wifiOn) {
    const int sta = WiFi.softAPgetStationNum();
    if (sta != wifiSta) {
      if (sta > 0 && wifiSta <= 0) wifiQr = 1;    // スマホが入ってきた → ページの QR へ
      wifiSta = sta; wifiDirty = true; wifiLast = millis();
    }
  }
#endif
  if (gMode != G_WIFI) return;
  const uint32_t idle = millis() - wifiLast;
  if (idle >= WIFI_IDLE_MS) { wifiStop(); return; }  // しばらく何もなかった → 自動で切る
  const int left = (int)((WIFI_IDLE_MS - idle + 59999) / 60000);
  if (left != wifiShownMin) { wifiShownMin = left; if (!wifiPeekUntil) wifiDirty = true; }
  bool flip = false, quit = false;
  if (roundScr) { flip = M5.BtnA.wasClicked(); quit = M5.BtnB.wasClicked(); }
  else          { flip = tapped; quit = M5.BtnPWR.wasClicked(); }
  if (quit) { wifiStop(); return; }
  if (flip) { wifiQr ^= 1; wifiLast = millis(); if (wifiPeekUntil) { wifiPeekUntil = 0; closeGif(); } wifiDirty = true; }
  if (wifiPeekUntil) {                             // 「流す」で、少しのあいだ GIF を流している
    if ((int32_t)(millis() - wifiPeekUntil) >= 0) { wifiPeekUntil = 0; closeGif(); wifiDirty = true; }
    else { stepGif(); return; }
  }
  if (wifiDirty) { wifiDirty = false; wifiDraw(); }
  delay(2);
}

// ---------------------------------------------------------------- setup / loop
static void fail(const char* msg) {
  M5.Display.setTextDatum(middle_center);
  M5.Display.drawString(msg, M5.Display.width() / 2, M5.Display.height() / 2);
  Serial.println(msg);
  while (true) nmIdle();
}

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);
  Serial.setRxBufferSize(8192);                            // USB で GIF を受け取るので、受け口を大きく
  Serial.begin(115200);
  Serial.println("NullGif v6");

  if (M5.Display.width() < M5.Display.height()) M5.Display.setRotation(1);
  W = M5.Display.width(); H = M5.Display.height();
  roundScr = (W == H);
  S = min(W, H) & ~1;                                      // GIF は短いほうの辺に合わせる(四角い画面は左右が黒)
  XOFF = (W - S) / 2; YOFF = (H - S) / 2;
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setBrightness(brightSteps[brightIdx]);

  rbuf   = (uint8_t*)malloc(RBUF);
  rxBuf  = (uint8_t*)malloc(CHUNK);
  band   = (uint16_t*)heap_caps_malloc(S * BAND * 2, MALLOC_CAP_DMA);
  mapX   = (int16_t*)malloc(S * 2); mapY = (int16_t*)malloc(S * 2);
  spanS  = (int16_t*)malloc(S * 2); spanE = (int16_t*)malloc(S * 2);
  lzwPre = (uint16_t*)malloc(LZW_N * 2); lzwSuf = (uint8_t*)malloc(LZW_N); lzwStk = (uint8_t*)malloc(LZW_N + 1);
  if (!rbuf || !rxBuf || !band || !mapX || !mapY || !spanS || !spanE || !lzwPre || !lzwSuf || !lzwStk) fail("Memory error");
  infoW = (roundScr ? 260 : min(300, S - 16)) & ~1; infoH = roundScr ? 52 : 48;
  infoX = (S - infoW) / 2; infoY = roundScr ? S - 96 : S - infoH - 6;
  infoSpr = new M5Canvas(&M5.Display);
  infoSpr->setColorDepth(16);
  infoSpr->setPsram(true);
  if (!infoSpr->createSprite(infoW, infoH)) { delete infoSpr; infoSpr = nullptr; }   // 作れなくても、表示なしで動く
  const float c = (S - 1) * 0.5f, rr = S * 0.5f + 1.0f;
  for (int y = 0; y < S; ++y) {
    if (!roundScr) { spanS[y] = 0; spanE[y] = S - 1; continue; }
    const float dy = y - c, w = rr * rr - dy * dy;
    if (w <= 0) { spanS[y] = 1; spanE[y] = 0; continue; }
    const float s = sqrtf(w);
    spanS[y] = max(0, (int)floorf(c - s)); spanE[y] = min(S - 1, (int)ceilf(c + s));
  }

  // GIF の置き場(初めてのときは整えるので時間がかかる)
  {
    const char* L[] = {"GIF の置き場を準備しています", "(初めてのときは少し時間がかかります)"};
    message(L, 2);
  }
  fsOk = FFat.begin(true);
  if (fsOk && !FFat.exists("/gif")) FFat.mkdir("/gif");
  if (fsOk) FFat.remove("/gif/~up.tmp");
  loadCfg();
  M5.Display.setBrightness(brightSteps[brightIdx]);
  scanFiles();
  Serial.printf("# gif: %s  %d files  used %u / %u bytes\n", fsOk ? "ok" : "NO FATFS", nFiles, (unsigned)fsUsed, (unsigned)fsTotal);
  playIndex(0);
}

void loop() {
  static uint32_t lastTapMs = 0;
  M5.update();
  pollUsb();
  if (rxActive) { delay(1); return; }                      // 受け取っているあいだは、流すのを休む

  auto t = M5.Touch.getDetail();
  if (gMode == G_WIFI) { wifiLoop(t.wasPressed() && !(t.x < 56 && t.y < 56)); return; }
  if (gMode == G_CONFIRM || gMode == G_USBINFO) {  // 消すかどうか / USB の説明
    int act = 0;                                   // 1 = 消す / -1 = もどる
    if (roundScr) {
      if (gMode == G_CONFIRM && M5.BtnB.wasClicked()) act = 1;
      else if (M5.BtnA.wasClicked() || M5.BtnA.wasHold() || M5.BtnB.wasClicked()) act = -1;
    } else {
      if (M5.BtnPWR.wasClicked()) act = -1;
      if (t.wasPressed() && millis() - lastTapMs > 200 && !(t.x < 56 && t.y < 56)) {
        lastTapMs = millis();
        act = (gMode == G_CONFIRM && t.x >= W / 2) ? 1 : -1;
      }
    }
    if (!act && millis() - confirmT > (uint32_t)MENU_IDLE_MS * 2) act = -1;
    if (act == 1) { doDelete(); return; }
    if (act == -1) { gMode = G_MENU; menuT = millis(); drawMenu(); return; }
    delay(5);
    return;
  }
  if (gMode == G_MENU) {                           // 設定の画面
    int act = -2;                                  // -1 = とじる / 0.. = その項目を変える
    if (roundScr) {
      if (M5.BtnA.wasHold()) act = -1;
      else if (M5.BtnA.wasClicked()) { menuSel = (menuSel + 1) % MENU_N; menuT = millis(); drawMenu(); }
      if (M5.BtnB.wasClicked()) act = menuSel;
    } else {
      if (M5.BtnPWR.wasClicked()) act = -1;
      if (t.wasPressed() && millis() - lastTapMs > 200 && !(t.x < 56 && t.y < 56)) {
        lastTapMs = millis();
        const int i = (int)floorf((t.y - (menuY0() - menuRowH() / 2)) / (float)menuRowH());
        if (i >= 0 && i < MENU_N) { menuSel = i; act = i; }
      }
    }
    if (act == -2 && millis() - menuT > (uint32_t)MENU_IDLE_MS) act = -1;
    if (act == 0) { brightIdx = (brightIdx + 1) % (int)sizeof(brightSteps); M5.Display.setBrightness(brightSteps[brightIdx]); saveCfg(); menuT = millis(); drawMenu(); }
    else if (act == 1) { autoNext = !autoNext; saveCfg(); menuT = millis(); drawMenu(); }
    else if (act == 2) { infoMode = (infoMode + 1) % 3; saveCfg(); menuT = millis(); drawMenu(); }
    else if (act == 3) { flip180 = !flip180; saveCfg(); menuT = millis(); drawMenu(); }   // 設定をとじると、その向きで流しなおす
    else if (act == 4) { if (nFiles > 0 && cur >= 0) { openConfirm(); return; } }
    else if (act == 5) { wifiStart(); return; }
    else if (act == 6) { openUsbInfo(); return; }
    else if (act == -1 || act == 7) { gMode = G_PLAY; playIndex(cur < 0 ? 0 : cur); return; }
    delay(5);
    return;
  }
  if (roundScr) {
    if (M5.BtnA.wasHold())         { if (nFiles) playIndex(cur - 1); }
    else if (M5.BtnA.wasClicked()) { if (nFiles) playIndex(cur + 1); }
    if (M5.BtnB.wasClicked() && fsOk) { closeGif(); gMode = G_MENU; menuSel = 0; menuT = millis(); drawMenu(); return; }
  } else {
    if (M5.BtnPWR.wasClicked() && fsOk) { closeGif(); gMode = G_MENU; menuSel = 0; menuT = millis(); drawMenu(); return; }
    if (t.wasPressed() && millis() - lastTapMs > 250 && !(t.x < 56 && t.y < 56)) {
      lastTapMs = millis();
      if (nFiles) playIndex(t.x < W / 2 ? cur - 1 : cur + 1);
    }
  }

  stepGif();
}


// 共通の置き場に置いたものは、0 で始まる単純な型だけ(コンストラクタが要らないもの)
static_assert(std::is_trivially_default_constructible<bool>::value, "NM_ARENA: bool");
static_assert(std::is_trivially_default_constructible<char>::value, "NM_ARENA: char");
static_assert(std::is_trivially_default_constructible<int>::value, "NM_ARENA: int");
static_assert(std::is_trivially_default_constructible<size_t>::value, "NM_ARENA: size_t");
static_assert(std::is_trivially_default_constructible<uint16_t>::value, "NM_ARENA: uint16_t");
static_assert(std::is_trivially_default_constructible<uint32_t>::value, "NM_ARENA: uint32_t");
static constexpr size_t OVL_END = nmAlignTo(nmOv65, 16);
static_assert(OVL_END <= NM_ARENA_SIZE, "NM_ARENA_SIZE (nm_common.h) を大きくしてください");
extern const size_t OVL_SIZE = OVL_END;   // 起動時の確認用
}  // namespace NullGif
