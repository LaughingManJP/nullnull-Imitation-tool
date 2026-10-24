// NullGalaxy  -  NullMenu 用モジュール(元のスケッチ NullGalaxy_v13 を名前空間に入れたもの)
// 0 で始まる変数(配列も)は、ほかの作品と共通の置き場(NM_ARENA)に置いています。
// 一度に動くのは1つだけ(切り替えるときは再起動)なので、重なっていても問題ありません。
#include "nm_common.h"
#if defined(ESP_PLATFORM) && __has_include(<lgfx/v1/panel/Panel_AMOLED.hpp>)
#include <lgfx/v1/panel/Panel_AMOLED.hpp>
#endif
#pragma GCC optimize("O3")

namespace NullGalaxy {
static constexpr size_t nmPre = 4368;   // 共通の置き場のいちばん前に、内蔵メモリを節約するための表を置く(下の nmOv0 から後ろがいつもの分)
static constexpr size_t nmOv0 = nmPre;

// NullGalaxy v13  -  立体の渦巻き銀河と、馬頭星雲。なぞると立体のまま回したり倒したりでき、タップした所に星が生まれる。銀と黒の見た目にも切りかえられる
//   大阪・関西万博のパビリオン「null²」を見た印象をもとにした、非公式の習作です。写真や他の作品の画像は使っていません。
//   M5Stack StopWatch 用(丸い 466x466 画面)。CoreS3 系でも真ん中に正方形で動きます。
//
// 画面の中:
//   ・金色に光る中心のふくらみと、青白い若い星が集まる渦巻きの腕。腕の内側には、黒〜茶色のちりの帯と、赤い星雲のかたまり
//   ・銀河は、ゆっくり回る(StopWatch はボタンA 長押しで 1倍 / 4倍 / 16倍)
//   ・ときどき、どこかの星が超新星になって、まぶしく光ってから消えていく
//   ・まわりの星は、奥行きがばらばら。傾けると、近い星ほど大きくずれる(のぞき込んでいる感じ)
//   ・立体: 近くから見ているので、手前のふちは大きく、奥は小さく見える(回すと、手前と奥で動く速さがちがう)。
//          円盤の星には上下の厚みがあり、手前の星ほど大きく明るい。赤い星雲のかたまりと黒いちりの粒も、円盤から少し浮いている。
//          真ん中のふくらみは横から見てもつぶれない玉。横向きに近いと、円盤の向こう側の星はちりで暗くなり、
//          ふくらみの前を、ちりの帯が横切る
//   ・種類は4つ: 2本腕の渦巻き / 棒のある渦巻き / 腕の多いもじゃもじゃ渦巻き / 馬頭星雲
//   ・馬頭星雲: 赤く光る星雲の壁の前に、黒いちりの雲から馬の頭の形の影が突き出ている。上には壁を照らす明るい星。
//          左下には、オレンジの炎のような星雲と、青白く光る星雲。後ろの壁・手前の雲・さらに手前に浮かぶ黒いちりの塊・星が
//          別々の層で、なぞったり傾けたりすると層が大きくずれて、馬の頭が壁から浮き出て見える(手前ほど大きくずれる)。
//          雲の表面は、上の明るい星に照らされて、でこぼこが浮かぶ
//
// 見た目は6つ:
//   ・写真   … 上のとおりの、色のある銀河・星雲
//   ・銀     … 色を抜いた、強い明暗の銀と黒。フィルムのような粒がちらつき、明るさのさかい目に、細い虹の線がゆらめく。
//              まわりの星は、ふわふわ浮かぶ光の粒になる
//   ・にじ   … NullKaleido と同じ6色の表で、明るさに合わせて色をつける。色は、時間とともに暗い所から明るい所へ流れていく
//   ・ネオン … 深い紫 → マゼンタ → ピンク → 水色
//   ・オーロラ … 深い青緑 → 緑 → 水色 → すみれ色
//   ・ほのお … 深い赤 → オレンジ → 金 → 白
//   (いちばん明るい所は、どの色でも白く光る)
//   馬頭星雲では、色の表で塗るのは後ろの壁だけ。壁は少しかすませ、手前の雲は暗い影として重ね、
//   雲のふちと照り返しは、壁と反対側の色で光らせる(どの色でも、明るい壁・黒い影・光るふちで立体に見える)
//
// 触ると:
//   タップ … 触った所に、新しい星が生まれる(光の輪がひろがって、星雲と若い星のかたまりが残り、銀河と一緒に回りながら、だんだん消える)
//   なぞる … 銀河を立体のまま動かせる。横になぞると回り(はじくと、その速さでずっと回り続ける)、縦になぞると倒れる(真上から 〜 横から)。
//            指をはなすと、倒した角度は、本体の傾きで決まる角度へゆっくり戻る。馬頭星雲では、のぞく向きが変わる
//   指を当てたまま待つ … はじいて回っているのが止まる(もとの、ゆっくりした回転にもどる)
// 傾けると:
//   銀河を見る角度が変わる(手前・奥に傾けると、真上から見たり、横から見たり。左右で銀河の向きが回る)
//
// 操作:
//   StopWatch … ボタンA: 見た目(写真 / 銀 / にじ / ネオン / オーロラ / ほのお)  ボタンA 長押し: 回る速さ  ボタンB: 種類(3つの銀河 / 馬頭星雲)
//   CoreS3系  … 電源ボタン短押し: 見た目  ダブルタップ: 種類
//
// 必要なもの:
//   ボードマネージャー M5Stack >= 3.3.7 / ボード: M5StopWatch(CoreS3 なら M5CoreS3)。StopWatch では PSRAM を「OPI PSRAM」に
//   ライブラリ M5Unified >= 0.2.15, M5GFX >= 0.2.21
//
// しくみ:
//   起動時(と種類を変えたとき)に、真上から見た銀河の絵(512x512)を作っておく。
//     ・なめらかな光(円盤の古い星・腕の若い星・中心のふくらみ)に、数万個の星の粒と、星雲のかたまりを足す
//     ・ちりは、腕の内側にそったすじと、細かいもやもや(ノイズ)。青い光ほどよく吸われる
//     ・光のにじみは、小さくした絵をぼかして足す
//     ・絵を作るときの exp・pow・atan2 などは、速い近似の計算にする(ESP32 ではとても遅いため)。ノイズは 32x32 の格子(内蔵メモリ)から
//       なめらかに引く。銀河の腕・円盤や、馬頭星雲のゆっくり変わる模様は、半分の細かさで作ってから引き伸ばす
//   コマごとには、画面の点から「銀河の絵のどこか」を、回転と傾きと遠近から求めて引く。
//     銀河の絵は 4行ずつの帯にして縦に4点ずつ並べておく(銀河がどの向きに回っていても、PSRAM から読む所がとびとびにならない)。
//     遠近の割り算は 8 点ごとに1回だけして、あいだは足し算でつなぐ。
//   生まれた星・超新星・まわりの星は、そのあとに光を足して描く。帯(16行ずつ)は、2つのコアで1本おきに描く。
//   写真以外の見た目: 絵を作るときに、明るさだけの絵(1点 1バイト)も作っておく。コマごとに、明るさ → 色 の表を作りなおして引く
//     (銀は虹の線つき。鮮やかな色は、6色の表を時間でずらして、色が流れる)。
//     粒は、4096 個のゆらぎの表を、コマごとにずらして足す。
//   立体: 銀河の絵からは、真ん中のふくらみを抜いておく。ふくらみは、少しつぶれた玉として画面の上で光を足す(見る角度で形が変わる)。
//     ふくらみの光の向こう側の分は、その画素の下の円盤のちり(絵を作るときに 256x256 で取っておく)で暗くする(横から見ると、ちりの帯になる)。
//     星は 8000 個を、円盤からの高さも持たせて置き、コマごとに立体の回転・傾き・遠近で画面に写して、帯ごとに分けて描く。
//     星雲のかたまり(光を足す)と、ちりの粒(暗くする。横から見るほど濃い)も、同じように立体に置く。
//   馬頭星雲: 後ろの壁(色)と、手前の雲(暗さと、照らされた明るさ)を 512x512 の絵2枚で作る。馬の頭は、丸や棒の形をなめらかにつないで、
//     ふちをノイズでぼかした形。2枚は、のぞく向きに合わせて、ちがう量だけずらして重ねる。

#ifndef IRAM_ATTR
#define IRAM_ATTR
#endif


// Arduino IDE は関数の宣言を自動で先頭近くに足すので、型の名前だけ先に宣言しておく
struct Birth;
struct BgStar;
struct Nova;
struct GalParam;
struct Star3;
struct NebPrim;

// ---- 調整用 ----------------------------------------------------------------
static constexpr int   TYPE_START  = 0;       // 始めの種類(0〜2: 銀河 / 3: 馬頭星雲)
static constexpr int   STYLE_START = 0;       // 始めの見た目(0: 写真 / 1: 銀 / 2: にじ / 3: ネオン / 4: オーロラ / 5: ほのお)
static constexpr float COLOR_FLOW  = 0.03f;   // 鮮やかな色が流れる速さ(0 で止まる)
static constexpr float FLING_KEEP  = 0.0f;    // はじいた回転が弱まる速さ(0 で、ずっと回り続ける)
static constexpr int   STARS_3D    = 8000;    // 立体に置く星の数(重いときは減らす)
static constexpr int   NEB_3D      = 140;     // 立体に置く、赤い星雲のかたまりの数
static constexpr int   DUST_3D     = 260;     // 立体に置く、ちりの粒の数
static constexpr float CAM_DIST    = 3.2f;    // 遠近: 見ている所までの距離(銀河の半径 = 1。小さいほど遠近が強い)
static constexpr float TILT_GAIN   = 1.0f;    // 傾けたときの動きの大きさ(大きいほどよく動く)
static constexpr float NEB_BRIGHT  = 1.0f;    // 馬頭星雲の明るさ
static constexpr float BULGE_FLAT  = 0.62f;   // 真ん中のふくらみのつぶれ具合(1 で まんまるの玉)
static constexpr float DRAG_TURN   = 1.0f;    // なぞったときの回りやすさ
static constexpr float DRAG_TILT   = 1.0f;    // なぞったときの倒れやすさ
static constexpr float INC_MIN     = 0.10f;   // いちばん横から見たときの、円盤の見かけの太さ(短軸 / 長軸)
static constexpr float SPIN        = 0.035f;  // 回る速さ(1倍のとき。ラジアン / 秒)
static constexpr float GAL_SIZE    = 0.88f;   // 画面の半径に対する、銀河の大きさ
static constexpr bool  TILT        = true;    // 傾きで見る角度を変えるか
static constexpr bool  TILT_FLIP_X = false;   // 左右が逆に動くときは true
static constexpr bool  TILT_FLIP_Y = false;   // 上下が逆に動くときは true
static constexpr bool  TILT_SWAP   = false;   // 左右と上下が入れかわっているときは true
static constexpr bool  TILT_STOPWATCH_FIX = true;   // StopWatch はセンサーが 90 度回ってついているので、自動で読みかえる
static constexpr float TILT_RELEVEL = 8.0f;   // 同じ角度で持ち続けると、何秒くらいでそこを「水平」にするか(0 = 起動したときの姿勢のまま)
static constexpr float TILT_RELEVEL_MAX = 0.5f;   // これより大きく傾けているあいだは、水平を合わせなおさない
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

static constexpr int BAND = 16, NSLOT = 4;
static constexpr int T = 512;                     // 銀河の絵の大きさ
static constexpr float TEXR = 1.15f;              // 絵の端 = 銀河の半径の何倍か
static constexpr int GB = 128;                    // 光のにじみ用の小さな絵
static constexpr int MAXBIRTH = 28, NBG = 360, NFG = 7, MAXNOVA = 3;
static constexpr int NTYPE = 4, NGAL = 3, NEBULA = 3;   // 0〜2: 銀河 / 3: 馬頭星雲
static constexpr int NS3 = STARS_3D + NEB_3D + DUST_3D;
static constexpr float NEB_R = 1.25f;             // 馬頭星雲の絵の端(画面の半径 = 0.90)
static constexpr int NEB_FG = 40;                 // 馬頭星雲: 手前に浮かぶ黒いちりの塊
static constexpr float PX_WALL = 0.05f, PX_CLOUD = 0.40f;   // 馬頭星雲: のぞく向きで、壁と雲がずれる量
static const char* const TYPE_NAME[NTYPE] = {"grand design spiral", "barred spiral", "flocculent spiral", "horsehead nebula"};
static const float SPEEDS[3] = {1.0f, 4.0f, 16.0f};
static constexpr int NSTYLE = 6;
static const char* const STYLE_NAME[NSTYLE] = {"photo", "silver", "rainbow", "neon", "aurora", "fire"};
static const float VIVID[4][6][3] = {             // 鮮やかな色の表(6色を順にまわす)
  {{0.10f, 0.03f, 0.02f}, {0.88f, 0.08f, 0.05f}, {1.00f, 0.55f, 0.10f}, {1.00f, 0.92f, 0.72f}, {0.30f, 0.85f, 1.00f}, {0.10f, 0.28f, 0.85f}},   // にじ(NullKaleido と同じ)
  {{0.05f, 0.00f, 0.16f}, {0.40f, 0.02f, 0.80f}, {1.00f, 0.05f, 0.65f}, {1.00f, 0.50f, 0.85f}, {0.10f, 0.95f, 1.00f}, {0.50f, 0.25f, 1.00f}},   // ネオン
  {{0.00f, 0.08f, 0.10f}, {0.00f, 0.50f, 0.42f}, {0.20f, 1.00f, 0.55f}, {0.55f, 1.00f, 0.92f}, {0.35f, 0.50f, 1.00f}, {0.75f, 0.28f, 0.95f}},   // オーロラ
  {{0.10f, 0.00f, 0.00f}, {0.75f, 0.06f, 0.02f}, {1.00f, 0.42f, 0.02f}, {1.00f, 0.80f, 0.20f}, {1.00f, 0.97f, 0.78f}, {0.95f, 0.30f, 0.12f}},   // ほのお
};

static constexpr size_t nmOa0 = nmAlignTo(nmOv0, alignof(int)), nmOv1 = nmOa0 + sizeof(int); static int& W = *reinterpret_cast<int*>(NM_ARENA + nmOa0);
static constexpr size_t nmOa1 = nmAlignTo(nmOv1, alignof(int)), nmOv2 = nmOa1 + sizeof(int); static int& H = *reinterpret_cast<int*>(NM_ARENA + nmOa1);
static constexpr size_t nmOa2 = nmAlignTo(nmOv2, alignof(int)), nmOv3 = nmOa2 + sizeof(int); static int& S = *reinterpret_cast<int*>(NM_ARENA + nmOa2);
static constexpr size_t nmOa3 = nmAlignTo(nmOv3, alignof(int)), nmOv4 = nmOa3 + sizeof(int); static int& XOFF = *reinterpret_cast<int*>(NM_ARENA + nmOa3);
static constexpr size_t nmOa4 = nmAlignTo(nmOv4, alignof(int)), nmOv5 = nmOa4 + sizeof(int); static int& YOFF = *reinterpret_cast<int*>(NM_ARENA + nmOa4);
static constexpr size_t nmOa5 = nmAlignTo(nmOv5, alignof(int)), nmOv6 = nmOa5 + sizeof(int); static int& NB = *reinterpret_cast<int*>(NM_ARENA + nmOa5);
static bool roundScr = true;
static constexpr size_t nmOa6 = nmAlignTo(nmOv6, alignof(bool)), nmOv7 = nmOa6 + sizeof(bool); static bool& fbMode = *reinterpret_cast<bool*>(NM_ARENA + nmOa6);
static constexpr size_t nmOa7 = nmAlignTo(nmOv7, alignof(bool)), nmOv8 = nmOa7 + sizeof(bool); static bool& directSend = *reinterpret_cast<bool*>(NM_ARENA + nmOa7);
static int16_t* spanA = nullptr;
static int16_t* spanB = nullptr;
static int16_t* bandX = nullptr;
static int16_t* bandW = nullptr;
static uint16_t* band[NSLOT] = {nullptr, nullptr, nullptr, nullptr};
static uint16_t* tex = nullptr;                   // 銀河の絵(画面へ送る形) [T * T]
static uint8_t*  texL = nullptr;                  // 銀の見た目用: 明るさだけの絵 [T * T]
static uint16_t* frT = nullptr;                   // 馬頭星雲の手前の雲: 暗さ(上の8ビット)とふちの光(下の8ビット) [T * T]
static int accR = 255, accG = 105, accB = 100;  // 鮮やかな色の馬頭星雲: 雲のふちを光らせる色
static constexpr size_t nmOa8 = nmAlignTo(nmOv8, 16), nmOv9 = nmOa8 + sizeof(uint16_t[256]); static uint16_t (&palS)[256] = *reinterpret_cast<uint16_t(*)[256]>(NM_ARENA + nmOa8);  // 明るさ → 銀の色(虹の線つき。コマごとに作りなおす)
static constexpr size_t nmOa9 = nmAlignTo(nmOv9, 16), nmOv10 = nmOa9 + sizeof(int8_t[4096]); static int8_t (&grainT)[4096] = *reinterpret_cast<int8_t(*)[4096]>(NM_ARENA + nmOa9);  // フィルムの粒
static constexpr size_t nmOa10 = nmAlignTo(nmOv10, alignof(uint32_t)), nmOv11 = nmOa10 + sizeof(uint32_t); static uint32_t& grainOff = *reinterpret_cast<uint32_t*>(NM_ARENA + nmOa10);
static constexpr size_t nmOa11 = nmAlignTo(nmOv11, 16), nmOv12 = nmOa11 + sizeof(uint8_t[21 * 21]); static uint8_t (&glowA)[21 * 21] = *reinterpret_cast<uint8_t(*)[21 * 21]>(NM_ARENA + nmOa11);  // 星雲の光の形
static constexpr size_t nmOa12 = nmAlignTo(nmOv12, 16), nmOv13 = nmOa12 + sizeof(uint8_t[11 * 11]); static uint8_t (&glowS)[11 * 11] = *reinterpret_cast<uint8_t(*)[11 * 11]>(NM_ARENA + nmOa12);  // 小さな光・ちりの形
static constexpr size_t nmOa13 = nmAlignTo(nmOv13, 16), nmOv14 = nmOa13 + sizeof(uint8_t[41 * 41]); static uint8_t (&glowB)[41 * 41] = *reinterpret_cast<uint8_t(*)[41 * 41]>(NM_ARENA + nmOa13);  // 大きな光・ちりの塊の形
static constexpr size_t nmOa14 = nmAlignTo(nmOv14, 16), nmOv15 = nmOa14 + sizeof(uint8_t[15 * 15]); static uint8_t (&starA)[15 * 15] = *reinterpret_cast<uint8_t(*)[15 * 15]>(NM_ARENA + nmOa14);  // 星の形(十字の光の筋つき)
static constexpr size_t nmOa15 = nmAlignTo(nmOv15, 16), nmOv16 = nmOa15 + sizeof(float[1024]); static float (&sinT)[1024] = *reinterpret_cast<float(*)[1024]>(NM_ARENA + nmOa15);

struct Birth { float u, v, age, life, k; uint32_t seed; };   // 生まれた星(銀河の面の上の場所)
static constexpr size_t nmOa16 = nmAlignTo(nmOv16, 16), nmOv17 = nmOa16 + sizeof(Birth[MAXBIRTH]); static Birth (&birth)[MAXBIRTH] = *reinterpret_cast<Birth(*)[MAXBIRTH]>(NM_ARENA + nmOa16);
static constexpr size_t nmOa17 = nmAlignTo(nmOv17, alignof(int)), nmOv18 = nmOa17 + sizeof(int); static int& birthNext = *reinterpret_cast<int*>(NM_ARENA + nmOa17);
struct BgStar { float x, y, depth, b, tw; uint8_t col; };  // まわりの星(画面の上。depth: 近いほど大きい)
static constexpr size_t nmOa18 = nmAlignTo(nmOv18, 16), nmOv19 = nmOa18 + sizeof(BgStar[NBG + NFG]); static BgStar (&bg)[NBG + NFG] = *reinterpret_cast<BgStar(*)[NBG + NFG]>(NM_ARENA + nmOa18);
struct Nova { float u, v, age, life; bool on; };
static constexpr size_t nmOa19 = nmAlignTo(nmOv19, 16), nmOv20 = nmOa19 + sizeof(Nova[MAXNOVA]); static Nova (&nova)[MAXNOVA] = *reinterpret_cast<Nova(*)[MAXNOVA]>(NM_ARENA + nmOa19);
static int style = STYLE_START;
static constexpr size_t nmOa20 = nmAlignTo(nmOv20, alignof(bool)), nmOv21 = nmOa20 + sizeof(bool); static bool& grabbing = *reinterpret_cast<bool*>(NM_ARENA + nmOa20);  // 指で銀河をつかんでいる(なぞっている)
static constexpr size_t nmOa21 = nmAlignTo(nmOv21, alignof(float)), nmOv22 = nmOa21 + sizeof(float); static float& incOff = *reinterpret_cast<float*>(NM_ARENA + nmOa21);  // なぞって倒した分
struct Star3 { float u, v, z; int16_t sx, sy; uint8_t r, g, b, bk, a, big; };   // 立体に置いた星・星雲・ちり(銀河の面の上の場所と、面からの高さ)
static Star3*    st3 = nullptr;
static uint16_t* st3Idx = nullptr;                // 帯ごとに並べた星の番号
static uint16_t* st3Band = nullptr;               // 帯 k の星は st3Idx[st3Band[k] .. st3Band[k+1]-1]
static uint8_t*  tT = nullptr;                    // 円盤のちりを通る光の割合(255 = 全部通る) [256 * 256]
static constexpr size_t nmOa22 = nmAlignTo(nmOv22, 16), nmOv23 = nmOa22 + sizeof(uint8_t[512]); static uint8_t (&bulR)[512] = *reinterpret_cast<uint8_t(*)[512]>(NM_ARENA + nmOa22);
static constexpr size_t nmOa23 = nmAlignTo(nmOv23, 16), nmOv24 = nmOa23 + sizeof(uint8_t[512]); static uint8_t (&bulG)[512] = *reinterpret_cast<uint8_t(*)[512]>(NM_ARENA + nmOa23);
static constexpr size_t nmOa24 = nmAlignTo(nmOv24, 16), nmOv25 = nmOa24 + sizeof(uint8_t[512]); static uint8_t (&bulB)[512] = *reinterpret_cast<uint8_t(*)[512]>(NM_ARENA + nmOa24);
static constexpr size_t nmOa25 = nmAlignTo(nmOv25, 16), nmOv26 = nmOa25 + sizeof(uint8_t[512]); static uint8_t (&bulL)[512] = *reinterpret_cast<uint8_t(*)[512]>(NM_ARENA + nmOa25);  // ふくらみの光(中心からの距離の2乗で引く)
static float bulC = 1, bulIQ2 = 1;
static constexpr size_t nmOa26 = nmAlignTo(nmOv26, alignof(float)), nmOv27 = nmOa26 + sizeof(float); static float& bulRpx = *reinterpret_cast<float*>(NM_ARENA + nmOa26);
static constexpr size_t nmOa27 = nmAlignTo(nmOv27, alignof(float)), nmOv28 = nmOa27 + sizeof(float); static float& bulK = *reinterpret_cast<float*>(NM_ARENA + nmOa27);
static constexpr size_t nmOa28 = nmAlignTo(nmOv28, alignof(float)), nmOv29 = nmOa28 + sizeof(float); static float& bulS = *reinterpret_cast<float*>(NM_ARENA + nmOa28);
static constexpr size_t nmOa29 = nmAlignTo(nmOv29, alignof(float)), nmOv30 = nmOa29 + sizeof(float); static float& spinV = *reinterpret_cast<float*>(NM_ARENA + nmOa29);  // 指で回した・はじいた分の回る速さ(ラジアン / 秒)
static float novaNext = 5.0f;
static constexpr size_t nmOa30 = nmAlignTo(nmOv30, alignof(float)), nmOv31 = nmOa30 + sizeof(float); static float& simT = *reinterpret_cast<float*>(NM_ARENA + nmOa30);
static constexpr size_t nmOa31 = nmAlignTo(nmOv31, alignof(float)), nmOv32 = nmOa31 + sizeof(float); static float& phi = *reinterpret_cast<float*>(NM_ARENA + nmOa31);
static int galType = TYPE_START;
static constexpr size_t nmOa32 = nmAlignTo(nmOv32, alignof(int)), nmOv33 = nmOa32 + sizeof(int); static int& speedIdx = *reinterpret_cast<int*>(NM_ARENA + nmOa32);
static constexpr size_t nmOa33 = nmAlignTo(nmOv33, alignof(float)), nmOv34 = nmOa33 + sizeof(float); static float& tiltX = *reinterpret_cast<float*>(NM_ARENA + nmOa33);
static constexpr size_t nmOa34 = nmAlignTo(nmOv34, alignof(float)), nmOv35 = nmOa34 + sizeof(float); static float& tiltY = *reinterpret_cast<float*>(NM_ARENA + nmOa34);
static constexpr size_t nmOa35 = nmAlignTo(nmOv35, alignof(float)), nmOv36 = nmOa35 + sizeof(float); static float& levelX = *reinterpret_cast<float*>(NM_ARENA + nmOa35);
static constexpr size_t nmOa36 = nmAlignTo(nmOv36, alignof(float)), nmOv37 = nmOa36 + sizeof(float); static float& levelY = *reinterpret_cast<float*>(NM_ARENA + nmOa36);
static constexpr size_t nmOa37 = nmAlignTo(nmOv37, alignof(int)), nmOv38 = nmOa37 + sizeof(int); static int& levelN = *reinterpret_cast<int*>(NM_ARENA + nmOa37);
static constexpr size_t nmOa38 = nmAlignTo(nmOv38, alignof(bool)), nmOv39 = nmOa38 + sizeof(bool); static bool& imuOK = *reinterpret_cast<bool*>(NM_ARENA + nmOa38);
static float inc = 0.55f, pa = 0.55f;  // 見え方: 傾き(短軸 / 長軸)と向き
static uint32_t rnd = 12345;
static inline float frand() { rnd = rnd * 1664525u + 1013904223u; return (rnd >> 8) * (1.0f / 16777216.0f); }   // 0..1

// コマごとの、画面 → 銀河の絵 の1次式(16.16 の固定小数)
static float vC = 1, vCf = 1, vK = 1, vIK = 1, vIncD = 1, vTS = 1;
static constexpr size_t nmOa39 = nmAlignTo(nmOv39, alignof(float)), nmOv40 = nmOa39 + sizeof(float); static float& vS = *reinterpret_cast<float*>(NM_ARENA + nmOa39);
static constexpr size_t nmOa40 = nmAlignTo(nmOv40, alignof(float)), nmOv41 = nmOa40 + sizeof(float); static float& vSf = *reinterpret_cast<float*>(NM_ARENA + nmOa40);
static constexpr size_t nmOa41 = nmAlignTo(nmOv41, alignof(float)), nmOv42 = nmOa41 + sizeof(float); static float& vSI = *reinterpret_cast<float*>(NM_ARENA + nmOa41);  // コマごとの見え方(向き・回転・傾き・遠近)
static constexpr size_t nmOa42 = nmAlignTo(nmOv42, alignof(float)), nmOv43 = nmOa42 + sizeof(float); static float& camX = *reinterpret_cast<float*>(NM_ARENA + nmOa42);
static constexpr size_t nmOa43 = nmAlignTo(nmOv43, alignof(float)), nmOv44 = nmOa43 + sizeof(float); static float& camY = *reinterpret_cast<float*>(NM_ARENA + nmOa43);
static constexpr size_t nmOa44 = nmAlignTo(nmOv44, alignof(float)), nmOv45 = nmOa44 + sizeof(float); static float& camDX = *reinterpret_cast<float*>(NM_ARENA + nmOa44);
static constexpr size_t nmOa45 = nmAlignTo(nmOv45, alignof(float)), nmOv46 = nmOa45 + sizeof(float); static float& camDY = *reinterpret_cast<float*>(NM_ARENA + nmOa45);  // 馬頭星雲: のぞく向き(なぞった分は camD)
static constexpr size_t nmOa46 = nmAlignTo(nmOv46, alignof(int32_t)), nmOv47 = nmOa46 + sizeof(int32_t); static int32_t& nbU = *reinterpret_cast<int32_t*>(NM_ARENA + nmOa46);
static constexpr size_t nmOa47 = nmAlignTo(nmOv47, alignof(int32_t)), nmOv48 = nmOa47 + sizeof(int32_t); static int32_t& nbV = *reinterpret_cast<int32_t*>(NM_ARENA + nmOa47);
static constexpr size_t nmOa48 = nmAlignTo(nmOv48, alignof(int32_t)), nmOv49 = nmOa48 + sizeof(int32_t); static int32_t& nbStep = *reinterpret_cast<int32_t*>(NM_ARENA + nmOa48);
static constexpr size_t nmOa49 = nmAlignTo(nmOv49, alignof(int32_t)), nmOv50 = nmOa49 + sizeof(int32_t); static int32_t& nfDU = *reinterpret_cast<int32_t*>(NM_ARENA + nmOa49);
static constexpr size_t nmOa50 = nmAlignTo(nmOv50, alignof(int32_t)), nmOv51 = nmOa50 + sizeof(int32_t); static int32_t& nfDV = *reinterpret_cast<int32_t*>(NM_ARENA + nmOa50);  // 馬頭星雲: 後ろの壁の引き方(16.16)と、手前の雲のずれ
static constexpr size_t nmOa51 = nmAlignTo(nmOv51, 16), nmOv52 = nmOa51 + sizeof(float[2]); static float (&spX)[2] = *reinterpret_cast<float(*)[2]>(NM_ARENA + nmOa51);
static constexpr size_t nmOa52 = nmAlignTo(nmOv52, 16), nmOv53 = nmOa52 + sizeof(float[2]); static float (&spY)[2] = *reinterpret_cast<float(*)[2]>(NM_ARENA + nmOa52);  // 馬頭星雲: 明るい星2つの画面の場所

// 2つのコアで描く
static SemaphoreHandle_t semGo = nullptr, semDone = nullptr;
static volatile int readyW = -1, doneUpTo = -1;
static uint32_t busyUs[2] = {0, 0}, sendUs = 0;
static volatile int job = 0;                      // コア0 にたのむ仕事: 0 = 帯を描く / 1 = 銀河の絵の奇数行 / 2 = 星雲の絵の奇数行

// ---------------------------------------------------------------- 小さな道具
static inline uint16_t toPix(float r, float g, float b) {
  int R = (int)(r * 255.0f + 0.5f), G = (int)(g * 255.0f + 0.5f), B = (int)(b * 255.0f + 0.5f);
  R = R < 0 ? 0 : R > 255 ? 255 : R; G = G < 0 ? 0 : G > 255 ? 255 : G; B = B < 0 ? 0 : B > 255 ? 255 : B;
  return __builtin_bswap16((uint16_t)(((R & 0xF8) << 8) | ((G & 0xFC) << 3) | (B >> 3)));
}
// 光を足す(r, g, b は 0..255 くらい。a: 0..32 の濃さ)
static inline __attribute__((always_inline)) uint16_t addLight(uint16_t p, int r, int g, int b, uint32_t a) {
  const uint32_t c = __builtin_bswap16(p);
  int R = (int)(c >> 11) + ((r * (int)a) >> 8), G = (int)((c >> 5) & 63) + ((g * (int)a) >> 7), B = (int)(c & 31) + ((b * (int)a) >> 8);
  R = R > 31 ? 31 : R; G = G > 63 ? 63 : G; B = B > 31 ? 31 : B;
  return __builtin_bswap16((uint16_t)((R << 11) | (G << 5) | B));
}
static inline float fsin(float x) { return sinT[(int)(x * (1024.0f / 6.2831853f)) & 1023]; }
static inline float clamp01(float v) { return v < 0 ? 0 : (v > 1 ? 1 : v); }
// 銀河・星雲の絵(512x512)の点の場所: 4行ずつの帯にして、帯の中は縦に4点ずつ並べる
// (PSRAM から一度に読む 32 バイトに、たて横となりの点がまとめて入る。銀河が回っても、読む所がとびとびにならない)
static constexpr uint32_t GSTRIP = 2048 + 48;     // 帯1本の長さ(少しすき間をあけて、キャッシュの同じ所にばかり入らないように)
static inline uint32_t gIdx(uint32_t x, uint32_t y) { return (y >> 2) * GSTRIP + (x << 2) + (y & 3); }
static inline uint32_t tIdx256(uint32_t x, uint32_t y) { return ((y >> 2) << 10) | (x << 2) | (y & 3); }   // 256x256 の表(ちりの透け具合)
static inline uint32_t hash32(uint32_t x) { x ^= x >> 16; x *= 0x7feb352d; x ^= x >> 15; x *= 0x846ca68b; x ^= x >> 16; return x; }
static inline float hashf(int x, int y, uint32_t seed) { return (hash32((uint32_t)x * 73856093u ^ (uint32_t)y * 19349663u ^ seed) & 0xFFFF) / 32767.5f - 1.0f; }   // -1..1
// ---- 起動時に絵を作るときだけ使う、速い近似の計算(ESP32 では expf・powf・atan2f などがとても遅いため)
static inline int ffloorI(float x) { const int i = (int)x; return (x < (float)i) ? i - 1 : i; }
static __attribute__((noinline)) float fexp2(float x) {               // 2^x(相対誤差 1e-4 くらい)
  if (x < -126.0f) return 0.0f;
  if (x > 126.0f) x = 126.0f;
  const int i = ffloorI(x); const float f = x - (float)i;
  const float p = 1.0f + f * (0.69314718f + f * (0.24022650f + f * (0.05550411f + f * (0.00961813f + f * 0.00133336f))));
  union { float f; int32_t i; } u; u.f = p; u.i += i << 23; return u.f;
}
static inline float fexp(float x) { return fexp2(x * 1.44269504f); }
static __attribute__((noinline)) float flog2(float x) {               // log2(x)(x > 0。誤差 1e-4 くらい)
  union { float f; uint32_t i; } u; u.f = x;
  const int e = (int)((u.i >> 23) & 255) - 127;
  u.i = (u.i & 0x7FFFFFu) | 0x3F800000u;         // 1..2
  const float m = u.f, y = (m - 1.0f) / (m + 1.0f), y2 = y * y;   // log2(m) = 2/ln2 · atanh(y)
  return (float)e + 2.88539008f * y * (1.0f + y2 * (0.33333333f + y2 * (0.2f + y2 * 0.14285714f)));
}
static inline float flog(float x) { return flog2(x) * 0.69314718f; }
static inline float fpow(float a, float p) { return a <= 0.0f ? 0.0f : fexp2(p * flog2(a)); }
static __attribute__((noinline)) float fatan2(float y, float x) {    // 誤差 2e-4 ラジアンくらい
  const float ax = fabsf(x), ay = fabsf(y), mx = ax > ay ? ax : ay, mn = ax > ay ? ay : ax;
  if (mx == 0.0f) return 0.0f;
  const float a = mn / mx, s = a * a;
  float r = ((-0.0464964749f * s + 0.15931422f) * s - 0.327622764f) * s * a + a;
  if (ay > ax) r = 1.57079637f - r;
  if (x < 0) r = 3.14159265f - r;
  return y < 0 ? -r : r;
}
static __attribute__((noinline)) float fcos(float x) {                // 表を引いて、あいだを直線でつなぐ(誤差 5e-6 くらい)
  const float t = x * (1024.0f / 6.2831853f) + 256.0f;   // cos = sin(x + π/2)
  const int i = ffloorI(t); const float f = t - (float)i;
  const float a = sinT[i & 1023], b = sinT[(i + 1) & 1023];
  return a + (b - a) * f;
}
static inline float fsinL(float x) { return fcos(x - 1.5707963f); }
static float vnoise(float x, float y, uint32_t seed) {
  const int xi = (int)floorf(x), yi = (int)floorf(y);
  float fx = x - xi, fy = y - yi;
  fx = fx * fx * (3 - 2 * fx); fy = fy * fy * (3 - 2 * fy);
  const float a = hashf(xi, yi, seed), b = hashf(xi + 1, yi, seed), c = hashf(xi, yi + 1, seed), d = hashf(xi + 1, yi + 1, seed);
  return (a * (1 - fx) + b * fx) * (1 - fy) + (c * (1 - fx) + d * fx) * fy;
}
static float (&latN)[32 * 32] = *reinterpret_cast<float(*)[32 * 32]>(NM_ARENA + 0);   // ノイズの格子(32x32 でくり返す。共通の置き場 = 内蔵メモリ)
static void buildNoise() {
  for (int y = 0; y < 32; ++y) for (int x = 0; x < 32; ++x) latN[y * 32 + x] = hashf(x, y, 777u);
}
static inline float nT(float x, float y) {         // ノイズをなめらかに引く(x, y は格子の点。はみ出たら、くり返す)
  const int xi = ffloorI(x), yi = ffloorI(y);
  float ax = x - (float)xi, ay = y - (float)yi; ax = ax * ax * (3 - 2 * ax); ay = ay * ay * (3 - 2 * ay);
  const int x0 = xi & 31, y0 = yi & 31, x1 = (xi + 1) & 31, y1 = (yi + 1) & 31;
  const float a = latN[y0 * 32 + x0], b = latN[y0 * 32 + x1], c = latN[y1 * 32 + x0], d = latN[y1 * 32 + x1];
  return (a + (b - a) * ax) * (1 - ay) + (c + (d - c) * ax) * ay;
}
static float fbm(float x, float y, int oct, uint32_t seed) {   // -1..1 くらい(x, y: ノイズのます 1 = 1)
  x = x + (seed % 97) * (13.7f / 8.0f); y = y + (seed % 89) * (17.3f / 8.0f);
  float s = 0, a = 1, n = 0;
  for (int o = 0; o < oct; ++o) { s += a * nT(x, y); n += a; x = x * 2.03f + (31.7f / 8.0f); y = y * 2.03f + (11.3f / 8.0f); a *= 0.55f; }
  return s / n * 1.6f;
}

// ---------------------------------------------------------------- 銀河の絵を作る
struct GalParam { int arms; float pitch, armPow, warp, spur, bar, barLen, dust; };
static const GalParam GP[NGAL] = {
  {2, 0.40f, 3.0f, 2.2f, 0.45f, 0.0f, 0.00f, 1.0f},   // 2本腕の渦巻き
  {2, 0.33f, 3.5f, 1.6f, 0.30f, 1.0f, 0.30f, 1.0f},   // 棒のある渦巻き
  {4, 0.55f, 2.0f, 3.2f, 0.80f, 0.0f, 0.00f, 0.8f},   // 腕の多いもじゃもじゃ渦巻き
};
static inline float armPhase(const GalParam& P, float r, float th) { return P.arms * (th - flog(r) / P.pitch); }
// 絵を作るあいだだけ使う表: exp(-x)(x = 0..16、1/64 きざみ)と、x^1.45(x = 0..1)
static float* gExpL = nullptr;
static float* gPowL = nullptr;
static inline float expNeg(float x) {              // exp(-x)(x >= 0)。表のあいだは直線でつなぐ
  if (x >= 15.98f) return 0.0f;
  const float t = x * 64.0f; const int i = (int)t; const float f = t - (float)i;
  return gExpL[i] + (gExpL[i + 1] - gExpL[i]) * f;
}
static inline float pow145(float x) {              // x^1.45(0..1)
  const float t = clamp01(x) * 1023.0f; const int i = (int)t; const float f = t - (float)i;
  return i >= 1023 ? gPowL[1023] : gPowL[i] + (gPowL[i + 1] - gPowL[i]) * f;
}
static void splat(float* buf, float u, float v, float rr, float gg, float bb, float rad) {   // 銀河の面の (u, v) に、ぼんやりした光の点
  const float k = T * 0.5f / TEXR;
  const float cx = u * k + T * 0.5f, cy = v * k + T * 0.5f;
  const int R = (int)ceilf(rad * 2.0f);
  const int x0 = max(0, (int)cx - R), x1 = min(T - 1, (int)cx + R), y0 = max(0, (int)cy - R), y1 = min(T - 1, (int)cy + R);
  const float i2 = 1.0f / (rad * rad);
  for (int y = y0; y <= y1; ++y)
    for (int x = x0; x <= x1; ++x) {
      const float dx = x - cx, dy = y - cy, w = expNeg((dx * dx + dy * dy) * i2);
      float* q = buf + ((size_t)y * T + x) * 3;
      q[0] += rr * w; q[1] += gg * w; q[2] += bb * w;
    }
}
static void showMsg(const char* msg, float prog);
static void nebRows(int y0, int step, bool show);
static const GalParam* gP = nullptr;
static constexpr size_t nmOa53 = nmAlignTo(nmOv53, alignof(float*)), nmOv54 = nmOa53 + sizeof(float*); static float*& gBuf = *reinterpret_cast<float**>(NM_ARENA + nmOa53);
static constexpr size_t nmOa54 = nmAlignTo(nmOv54, alignof(float*)), nmOv55 = nmOa54 + sizeof(float*); static float*& gTau = *reinterpret_cast<float**>(NM_ARENA + nmOa54);
static constexpr size_t nmOa55 = nmAlignTo(nmOv55, alignof(uint32_t)), nmOv56 = nmOa55 + sizeof(uint32_t); static uint32_t& gSd = *reinterpret_cast<uint32_t*>(NM_ARENA + nmOa55);
// 銀河の、ゆっくり変わる所(腕の形・円盤の明るさ・星のかたまり・ちりのすじ)は、半分の細かさの格子で先に作っておく
static constexpr int GLG = T / 2 + 1;
static constexpr int GLF = 5;                      // 古い星・若い星・棒・ちりのすじ・円盤
static float* galLow = nullptr;                    // [GLG * GLG * GLF]
static void galLowRows(int j0, int step) {
  const GalParam& P = *gP;
  const uint32_t sd = gSd;
  const float k = TEXR * 2.0f / T;
  for (int j = j0; j < GLG; j += step) {
    if (j != j0 && ((j - j0) & 7) == 0) vTaskDelay(1);   // ときどき休む(コア0 は、5秒休まないと見張りに止められて再起動になる)
    const float v = (2 * j + 0.5f) * k - TEXR;
    for (int i = 0; i < GLG; ++i) {
      const float u = (2 * i + 0.5f) * k - TEXR;
      const float r = sqrtf(u * u + v * v) + 1e-4f, th = fatan2(v, u);
      const float F = fbm(u * 2.4f, v * 2.4f, 5, sd), F2 = fbm(u * 3.1f + 7, v * 3.1f, 3, sd + 77);
      const float ph0 = armPhase(P, r, th), ph = ph0 + F * P.warp;
      float arm = fpow(0.5f + 0.5f * fcos(ph), P.armPow);
      const float sp = 0.5f + 0.5f * fcos(2.0f * ph0 + F2 * 3.0f);
      arm += P.spur * sp * sp * sp * sp * sp * sp;
      arm = arm > 1.0f ? 1.0f : arm;
      if (P.bar > 0) arm *= clamp01((r - P.barLen * 0.6f) / (P.barLen * 0.7f));   // 棒の先から腕
      const float disk = fexp(-r / 0.30f) * (1.0f - clamp01((r - 0.70f) / 0.40f) * clamp01((r - 0.70f) / 0.40f));   // 外はなめらかに消える
      const float cl = fmaxf(0.0f, fbm(u * 6.0f, v * 6.0f, 3, sd + 13) * 1.8f + 0.55f), clump = cl * sqrtf(cl);
      float bulge = 0.0f;                          // 真ん中のふくらみは、絵に入れずに、立体の玉として描く
      if (P.bar > 0) bulge += 0.55f * fexp(-(u / P.barLen) * (u / P.barLen) - (v / 0.065f) * (v / 0.065f));   // 棒
      bulge = bulge > 6.0f ? 6.0f : bulge;
      float d = 0.5f + 0.5f * fcos(ph + 0.8f); d = d * d * d; d *= d;   // ちり: 腕の内側にそったすじ
      d *= fexp(-r / 0.45f) * clamp01((r - 0.04f) / 0.08f);
      if (P.bar > 0) d *= clamp01((r - P.barLen * 0.5f) / (P.barLen * 0.5f)) + 0.3f;
      float* q = galLow + ((size_t)j * GLG + i) * GLF;
      q[0] = disk * (0.70f + 0.30f * arm);                              // 古い星
      q[1] = disk * arm * clamp01((r - 0.06f) / 0.12f) * clump * 1.2f;   // 若い星
      q[2] = bulge; q[3] = d; q[4] = disk;
    }
  }
}
// 1. なめらかな光とちり(y0 行から、step 行おきに。2つのコアで分ける)。細かいちりのもやもやだけ、1点ずつ計算する
static void galRows(int y0, int step, bool show) {
  const GalParam& P = *gP;
  float* buf = gBuf; float* tau = gTau;
  const uint32_t sd = gSd;
  const float k = TEXR * 2.0f / T;
  for (int y = y0; y < T; y += step) {
    if (y != y0 && ((y - y0) & 15) == 0) vTaskDelay(1);   // ときどき休む(コア0 は、5秒休まないと見張りに止められて再起動になる)
    if (show && (y & 31) == 0) showMsg(nullptr, 0.05f + 0.45f * y / T);
    const int j0 = y >> 1; const float fy = (y & 1) * 0.5f;
    for (int x = 0; x < T; ++x) {
      const float u = (x + 0.5f) * k - TEXR, v = (y + 0.5f) * k - TEXR;
      float L[GLF];                                // ゆっくり変わる所(半分の細かさの格子から、なめらかに)
      {
        const int i0 = x >> 1; const float fx = (x & 1) * 0.5f;
        const float* q00 = galLow + ((size_t)j0 * GLG + i0) * GLF; const float* q01 = q00 + GLF; const float* q10 = q00 + GLG * GLF; const float* q11 = q10 + GLF;
        for (int c = 0; c < GLF; ++c) L[c] = (q00[c] + (q01[c] - q00[c]) * fx) * (1 - fy) + (q10[c] + (q11[c] - q10[c]) * fx) * fy;
      }
      const float old = L[0], young = L[1], bulge = L[2], disk = L[4];
      {                                            // ちり: 腕の内側にそったすじ(格子から)と、細かいもやもや
        const float fil = fmaxf(0.0f, fbm(u * 8.0f, v * 8.0f, 4, sd + 31) * 1.6f + 0.4f);
        const float mist = fmaxf(0.0f, fbm(u * 14.0f, v * 14.0f, 3, sd + 57) * 2.2f - 0.6f) * disk * 1.4f;
        tau[(size_t)y * T + x] = (L[3] * fil * 4.5f + mist) * P.dust;
      }
      float* q = buf + ((size_t)y * T + x) * 3;
      q[0] = old * 0.90f + young * 0.60f + bulge * 1.00f;
      q[1] = old * 0.77f + young * 0.79f + bulge * 0.83f;
      q[2] = old * 0.61f + young * 1.10f + bulge * 0.58f;
    }
  }
}
static float* gGl = nullptr;                       // 光のにじみの小さな絵(絵を作るあいだだけ)
static void galDust(int y0, int step) {            // 3. ちり(青い光ほどよく吸われる)
  float* buf = gBuf; const float* tau = gTau;
  for (int y = y0; y < T; y += step) {
    if (y != y0 && ((y - y0) & 31) == 0) vTaskDelay(1);
    for (int x = 0; x < T; ++x) {
      const size_t n = (size_t)y * T + x;
      const float t = tau[n];
      if (t <= 0.001f) continue;
      float* q = buf + n * 3;
      q[0] *= expNeg(t); q[1] *= expNeg(t * 1.15f); q[2] *= expNeg(t * 1.35f);
    }
  }
}
static void galFinal(int y0, int step) {           // 5. 画面の色に(やわらかく頭打ち)
  const float* buf = gBuf; const float* gl = gGl;
  const int f = T / GB;
  const float k = TEXR * 2.0f / T;
  const float EXPO = 1.7f;
  for (int y = y0; y < T; y += step) {
    if (y != y0 && ((y - y0) & 31) == 0) vTaskDelay(1);
    for (int x = 0; x < T; ++x) {
      const float* q = buf + ((size_t)y * T + x) * 3;
      const float gx = (x + 0.5f) / f - 0.5f, gy = (y + 0.5f) / f - 0.5f;   // にじみの絵を、なめらかに引き伸ばして読む
      const int ix = min(GB - 2, max(0, (int)gx)), iy = min(GB - 2, max(0, (int)gy));
      const float fx = clamp01(gx - ix), fy = clamp01(gy - iy);
      float g[3];
      for (int ch = 0; ch < 3; ++ch) {
        const float a = gl[(iy * GB + ix) * 3 + ch], b = gl[(iy * GB + ix + 1) * 3 + ch], c = gl[((iy + 1) * GB + ix) * 3 + ch], d = gl[((iy + 1) * GB + ix + 1) * 3 + ch];
        g[ch] = (a * (1 - fx) + b * fx) * (1 - fy) + (c * (1 - fx) + d * fx) * fy;
      }
      const float u = (x + 0.5f) * k - TEXR, v = (y + 0.5f) * k - TEXR, edge = clamp01((TEXR - sqrtf(u * u + v * v)) / 0.10f);   // 絵の端は黒に
      float rr = (q[0] * 1.0f + g[0] * 0.55f) * edge, gg = (q[1] * 1.0f + g[1] * 0.55f) * edge, bb = (q[2] * 1.0f + g[2] * 0.55f) * edge;
      const float L = rr * 0.30f + gg * 0.55f + bb * 0.15f;   // 銀: 明暗を強く(ちりは黒く沈む)
      float sl = 1.0f - expNeg(L * EXPO * 1.3f);
      sl = pow145(sl) * 1.02f;
      texL[gIdx(x, y)] = (uint8_t)(clamp01(sl) * 238.0f);
      rr = 1.0f - expNeg(rr * EXPO); gg = 1.0f - expNeg(gg * EXPO); bb = 1.0f - expNeg(bb * EXPO);
      tex[gIdx(x, y)] = toPix(rr, gg, bb);
    }
  }
}
static bool buildGalaxy(int type) {
  const GalParam& P = GP[type];
  float* buf = (float*)ps_calloc((size_t)T * T * 3, sizeof(float));
  float* gl = (float*)calloc((size_t)GB * GB * 3, sizeof(float));
  float* tmp = (float*)malloc((size_t)GB * 3 * sizeof(float));
  float* tau = (float*)ps_malloc((size_t)T * T * sizeof(float));      // ちりの吸われ具合
  gExpL = (float*)malloc(1025 * sizeof(float)); gPowL = (float*)malloc(1025 * sizeof(float));
  if (!buf || !gl || !tmp || !tau || !gExpL || !gPowL) { free(buf); free(gl); free(tmp); free(tau); free(gExpL); free(gPowL); gExpL = gPowL = nullptr; return false; }
  for (int i = 0; i <= 1024; ++i) { gExpL[i] = fexp(-i / 64.0f); gPowL[i] = fpow(i / 1023.0f, 1.45f); }
  const uint32_t sd = 1000u + type * 7919u;
  const float k = TEXR * 2.0f / T;
  // 1. なめらかな光とちり(2つのコアで、1行おきに)
  gP = &P; gBuf = buf; gTau = tau; gSd = sd;
  galLow = (float*)ps_malloc((size_t)GLG * GLG * GLF * sizeof(float));
  if (!galLow) { free(buf); free(gl); free(tmp); free(tau); free(gExpL); free(gPowL); gExpL = gPowL = nullptr; return false; }
  if (semGo) {
    job = 4; xSemaphoreGive(semGo); galLowRows(0, 2); xSemaphoreTake(semDone, portMAX_DELAY);
    job = 1; xSemaphoreGive(semGo); galRows(0, 2, true); xSemaphoreTake(semDone, portMAX_DELAY); job = 0;
  } else { galLowRows(0, 1); galRows(0, 1, true); }
  free(galLow); galLow = nullptr;
  // 2. 星の粒(腕にそって多く)と、赤い星雲のかたまり
  rnd = sd;
  const int nStar = 26000;
  for (int i = 0, got = 0; got < nStar && i < nStar * 4; ++i) {
    const float rr = -0.30f * flog(1.0f - frand() * 0.98f), tt = frand() * 6.2831853f - 3.1415927f;
    const bool young = (got & 1) == 0;
    if (young) {
      const float a = 0.5f + 0.5f * fcos(armPhase(P, rr + 1e-4f, tt));
      if (frand() > a * a * a * a) continue;
      if (P.bar > 0 && rr < P.barLen * 0.6f) continue;
    }
    const float b = frand(); const float bb = b * b * (young ? 0.55f : 0.25f);
    const float u = rr * fcos(tt), v = rr * fsinL(tt);
    if (young) splat(buf, u, v, bb * 0.6f, bb * 0.75f, bb * 1.0f, 0.7f);
    else splat(buf, u, v, bb * 1.0f, bb * 0.9f, bb * 0.75f, 0.6f);
    ++got;
    if ((got & 4095) == 0) showMsg(nullptr, 0.50f + 0.20f * got / nStar);
  }
  for (int c = 0, tries = 0; c < (type == 2 ? 300 : 220) && tries < 20000; ++tries) {
    const float rr = -0.30f * flog(1.0f - frand() * 0.95f), tt = frand() * 6.2831853f - 3.1415927f;
    if (rr < 0.10f || rr > 0.90f) continue;
    const float a = 0.5f + 0.5f * fcos(armPhase(P, rr, tt));
    if (frand() > fpow(a, 8.0f)) continue;
    ++c;
    const int m = 3 + (int)(frand() * 6);
    for (int j = 0; j < m; ++j) {
      const float r2 = rr + (frand() - 0.5f) * 0.016f, t2 = tt + (frand() - 0.5f) * 0.04f, w = 0.5f + frand();
      splat(buf, r2 * fcos(t2), r2 * fsinL(t2), 0.55f * w, 0.17f * w, 0.26f * w, 1.0f);
    }
  }
  showMsg(nullptr, 0.72f);
  // 3. ちり(青い光ほどよく吸われる。2つのコアで1行おきに)
  if (semGo) { job = 5; xSemaphoreGive(semGo); galDust(0, 2); xSemaphoreTake(semDone, portMAX_DELAY); job = 0; }
  else galDust(0, 1);
  showMsg(nullptr, 0.85f);
  for (int y = 0; y < 256; ++y)                    // ふくらみの、円盤の向こう側の光を暗くする分
    for (int x = 0; x < 256; ++x) {
      const float* q = tau + (size_t)(2 * y) * T + 2 * x;
      tT[tIdx256(x, y)] = (uint8_t)(expNeg((q[0] + q[1] + q[T] + q[T + 1]) * 0.25f) * 255.0f);
    }
  // 4. 光のにじみ: 小さくした絵を、箱ぼかし3回(広いにじみと、せまいにじみ)
  const int f = T / GB;
  for (int y = 0; y < GB; ++y)
    for (int x = 0; x < GB; ++x) {
      float s0 = 0, s1 = 0, s2 = 0;
      for (int j = 0; j < f; ++j) for (int i = 0; i < f; ++i) { const float* q = buf + ((size_t)(y * f + j) * T + (x * f + i)) * 3; s0 += q[0]; s1 += q[1]; s2 += q[2]; }
      float* g = gl + (y * GB + x) * 3; g[0] = s0 / (f * f); g[1] = s1 / (f * f); g[2] = s2 / (f * f);
    }
  for (int pass = 0; pass < 3; ++pass)
    for (int dir = 0; dir < 2; ++dir)
      for (int j = 0; j < GB; ++j) {
        const int R = 3;
        for (int ch = 0; ch < 3; ++ch) {
          float acc = 0;
          auto at = [&](int i) -> float& { i = i < 0 ? 0 : (i > GB - 1 ? GB - 1 : i); return dir == 0 ? gl[(j * GB + i) * 3 + ch] : gl[(i * GB + j) * 3 + ch]; };
          for (int i = -R; i <= R; ++i) acc += at(i);
          for (int i = 0; i < GB; ++i) { tmp[i * 3 + ch] = acc / (2 * R + 1); acc += at(i + R + 1) - at(i - R); }
        }
        for (int i = 0; i < GB; ++i) for (int ch = 0; ch < 3; ++ch) (dir == 0 ? gl[(j * GB + i) * 3 + ch] : gl[(i * GB + j) * 3 + ch]) = tmp[i * 3 + ch];
      }
  showMsg(nullptr, 0.95f);
  // 5. 画面の色に(やわらかく頭打ち。2つのコアで1行おきに)
  gGl = gl;
  if (semGo) { job = 6; xSemaphoreGive(semGo); galFinal(0, 2); xSemaphoreTake(semDone, portMAX_DELAY); job = 0; }
  else galFinal(0, 1);
  free(buf); free(gl); free(tmp); free(tau); free(gExpL); free(gPowL); gExpL = gPowL = nullptr;
  return true;
}
// ---------------------------------------------------------------- 馬頭星雲を作る
struct NebPrim { float ax, ay, bx, by, r; };      // 棒(両はしが丸い)。a = b なら丸
static const NebPrim HORSE[] = {                  // 馬の頭(左を向いて、下の雲から首が立ち上がる)
  { 0.17f,  0.42f,  0.12f,  0.00f, 0.120f},         // 首(下ほど太い)
  { 0.25f,  0.38f,  0.19f, -0.06f, 0.080f},         // たてがみの側(首の後ろのふくらみ)
  { 0.09f, -0.19f,  0.09f, -0.19f, 0.090f},         // 頭のてっぺん
  { 0.07f, -0.14f, -0.13f,  0.07f, 0.082f},         // 頭(左下へ、うつむくようにのびる)
  {-0.17f,  0.12f, -0.17f,  0.12f, 0.068f},         // 鼻先
  { 0.06f, -0.03f,  0.06f, -0.03f, 0.085f},         // ほお
  { 0.12f, -0.27f,  0.16f, -0.37f, 0.030f},         // 耳
};
static inline float capsule(float px, float py, const NebPrim& P) {
  const float bax = P.bx - P.ax, bay = P.by - P.ay, pax = px - P.ax, pay = py - P.ay;
  const float l2 = bax * bax + bay * bay, h = l2 > 0 ? clamp01((pax * bax + pay * bay) / l2) : 0.0f;
  const float dx = pax - bax * h, dy = pay - bay * h;
  return sqrtf(dx * dx + dy * dy) - P.r;
}
static inline float smin(float a, float b, float k) { const float h = clamp01(0.5f + 0.5f * (b - a) / k); return b + (a - b) * h - k * h * (1.0f - h); }
static inline float nebEdge(float px, uint32_t sd) {   // 下の雲の上のふちの高さ(横の位置だけで決まる)
  return 0.30f + 0.05f * fsinL(px * 2.3f + 0.4f) + 0.10f * fbm(px * 2.2f + 11.0f, 3.0f, 4, sd + 21);
}
static float nebSdf(float px, float py, uint32_t sd, float edge) {   // 雲の形(内側がマイナス): 下の雲の本体 + 馬の頭
  float d = edge - py;
  // 馬の頭: 形のまわりの四角(-0.24〜0.33, -0.40〜0.54)から遠ければ、計算しても結果は変わらないので飛ばす
  const float bx = px < -0.24f ? -0.24f - px : (px > 0.33f ? px - 0.33f : 0.0f), by = py < -0.40f ? -0.40f - py : (py > 0.54f ? py - 0.54f : 0.0f);
  if (bx + by < 0.0001f || sqrtf(bx * bx + by * by) - 0.012f < d + 0.06f) {
    float hd = capsule(px, py, HORSE[0]);
    for (size_t i = 1; i < sizeof(HORSE) / sizeof(HORSE[0]); ++i) hd = smin(hd, capsule(px, py, HORSE[i]), 0.045f);
    d = smin(d, hd, 0.06f);
  }
  if (d > 0.12f || d < -0.12f) return d;          // ふちから遠い所では、ふちのぎざぎざ(±0.03 まで)は見た目を変えないので計算しない
  return d + fbm(px * 9.0f, py * 9.0f, 3, sd + 31) * 0.022f + fbm(px * 22.0f, py * 22.0f, 2, sd + 33) * 0.008f;
}
// 馬頭星雲の、ゆっくり変わる模様(3つ)は、半分の細かさの格子で先に作っておき、なめらかに引く
static constexpr int NLG = T / 2 + 1;
static constexpr int NLF = 5;                      // うねり・明るさのうねり・雲の濃さ・雲の表面の傾き・照り返しのむら
static float* nebLow = nullptr;                    // [NLG * NLG * NLF]
static void nebLowRows(int j0, int step) {
  const uint32_t sd = 4321u;
  const float k = NEB_R * 2.0f / T;
  for (int j = j0; j < NLG; j += step) {
    if (j != j0 && ((j - j0) & 15) == 0) vTaskDelay(1);
    const float py = (2 * j + 0.5f) * k - NEB_R;
    for (int i = 0; i < NLG; ++i) {
      const float px = (2 * i + 0.5f) * k - NEB_R;
      float* q = nebLow + ((size_t)j * NLG + i) * NLF;
      q[0] = fbm(px * 3.0f + 7.0f, py * 3.0f, 4, sd + 13);
      q[1] = fbm(px * 1.5f + 4.0f, py * 0.8f, 2, sd + 9);
      q[2] = fbm(px * 4.0f, py * 4.0f, 3, sd + 51);
      const float F0 = fbm(px * 4.5f, py * 4.5f, 3, sd + 81), Fu = fbm(px * 4.5f, (py - 0.012f) * 4.5f, 3, sd + 81), Fl = fbm((px - 0.012f) * 4.5f, py * 4.5f, 3, sd + 81);
      q[3] = (F0 - Fu) * 7.0f + (F0 - Fl) * 2.5f;   // 上(と少し左)へ下る面は、光の方を向いている
      q[4] = fbm(px * 6.0f, py * 6.0f, 2, sd + 71);
    }
  }
}
static void nebRows(int y0, int step, bool show) {   // y0 行から step 行おきに(2つのコアで分ける)
  const uint32_t sd = 4321u;
  const float k = NEB_R * 2.0f / T;
  const float flX = -0.60f, flY = 0.72f, rfX = -0.30f, rfY = 0.48f;   // 炎の星雲 / 青白い反射星雲
  float* edgeX = (float*)malloc(T * sizeof(float));   // 下の雲のふちの高さ(列ごとに1回だけ計算)
  for (int x = 0; x < T; ++x) edgeX[x] = nebEdge((x + 0.5f) * k - NEB_R, sd);
  for (int y = y0; y < T; y += step) {
    if (y != y0 && ((y - y0) & 15) == 0) vTaskDelay(1);   // ときどき休む(コア0 は、5秒休まないと見張りに止められて再起動になる)
    if (show && (y & 31) == 0) showMsg(nullptr, 0.05f + 0.85f * y / T);
    for (int x = 0; x < T; ++x) {
      const float px = (x + 0.5f) * k - NEB_R, py = (y + 0.5f) * k - NEB_R;
      // 後ろの壁: 上ほど明るい赤。縦のすじ
      float E = (1.9f * fexp(-(py + 1.0f) * 0.70f) + 0.30f) * NEB_BRIGHT;
      const float st = fbm(px * 6.0f, py * 1.2f, 4, sd), fi = fbm(px * 14.0f, py * 5.0f, 3, sd + 5);
      float low[NLF];                               // ゆっくり変わる模様(半分の細かさの格子から)
      {
        const int i0 = x >> 1, j0 = y >> 1; const float fx = (x & 1) * 0.5f, fy = (y & 1) * 0.5f;
        const float* q00 = nebLow + ((size_t)j0 * NLG + i0) * NLF; const float* q01 = q00 + NLF; const float* q10 = q00 + NLG * NLF; const float* q11 = q10 + NLF;
        for (int c = 0; c < NLF; ++c) low[c] = (q00[c] + (q01[c] - q00[c]) * fx) * (1 - fy) + (q10[c] + (q11[c] - q10[c]) * fx) * fy;
      }
      const float cl = low[0];
      E *= fmaxf(0.0f, 0.62f + 0.30f * st + 0.32f * cl + 0.12f * fi) * (0.80f + 0.30f * low[1]);
      float r = E * 1.00f, g = E * 0.11f + E * E * 0.035f, b = E * 0.19f + E * E * 0.045f;   // 深い赤のまま明るく
      {                                             // 炎の星雲(オレンジ。黒いすじが通る)
        const float dx = px - flX, dy = py - flY, q = dx * dx + dy * dy;
        float fl = 0.0f;
        if (q < 0.36f) {                            // これより遠いと、炎の星雲は見えない(1e-5 未満)
          fl = fexp(-q / 0.030f) * fmaxf(0.0f, 0.6f + 0.7f * fbm(px * 8.0f, py * 8.0f, 3, sd + 41));
          const float lane = dx * 0.6f - dy * 0.8f + 0.03f * fbm(px * 10.0f, py * 10.0f, 2, sd + 43);
          fl *= 1.0f - 0.85f * fexp(-(lane / 0.025f) * (lane / 0.025f));
        }
        r += fl * 1.70f * NEB_BRIGHT; g += fl * 0.85f * NEB_BRIGHT; b += fl * 0.25f * NEB_BRIGHT;
      }
      {                                             // 青白い反射星雲
        const float dx = px - rfX, dy = py - rfY, q = dx * dx + dy * dy;
        const float rf = fexp(-q / 0.012f) * 0.75f + fexp(-q / 0.05f) * 0.15f;
        r += rf * 0.85f * NEB_BRIGHT; g += rf * 1.05f * NEB_BRIGHT; b += rf * 1.50f * NEB_BRIGHT;
      }
      r = 1.0f - fexp(-r * 2.1f); g = 1.0f - fexp(-g * 2.1f); b = 1.0f - fexp(-b * 2.1f);
      const size_t n = gIdx(x, y);
      tex[n] = toPix(r, g, b);
      const float L = r * 0.30f + g * 0.55f + b * 0.15f;
      texL[n] = (uint8_t)(clamp01(fpow(clamp01(L * 1.25f), 1.15f)) * 238.0f);
      // 手前の雲: 暗さとふちの光(上を向いたふちほど、上の明るい星に照らされる)
      const float d = nebSdf(px, py, sd, edgeX[x]);
      float al = clamp01(0.5f - d / 0.040f) * 0.93f * (0.80f + 0.20f * low[2]);
      if (d > 0 && d < 0.25f) {                     // 雲のふちの上に、うすい細いすじ
        const float w = fmaxf(0.0f, fbm(px * 5.0f, py * 12.0f, 3, sd + 61) * 1.3f - 0.35f) * fexp(-d / 0.10f) * 0.45f;
        al = fmaxf(al, w);
      }
      { const float dx = px - flX, dy = py - flY; al *= 1.0f - 0.85f * fexp(-(dx * dx + dy * dy) / 0.040f); }
      { const float dx = px - rfX, dy = py - rfY; al *= 1.0f - 0.75f * fexp(-(dx * dx + dy * dy) / 0.015f); }
      float lit = 0;                                // 照らされた明るさ: 上を向いたふち + 雲の表面のでこぼこ
      if (d < 0.04f) {
        const float up = (nebSdf(px, py - 0.010f, sd, edgeX[x]) - d) / 0.010f;   // 上へ行くと外に出る所ほど、上を向いたふち
        const float hi = clamp01(1.1f - (py + 0.2f) * 0.7f);   // 上の星に近いほど明るく
        if (d > -0.015f) lit += fexp(-(d / 0.016f) * (d / 0.016f)) * clamp01(up) * hi * (0.55f + 0.45f * clamp01(low[4] + 0.5f));
        if (d < 0.0f) {
          lit += fexp(d / 0.04f) * clamp01(up * 0.8f + 0.2f) * hi * 0.40f;   // ふちに近い所ほど照らされる(雲の丸み)
          const float slope = low[3];               // 上(と少し左)へ下る面は、光の方を向いている(格子から)
          lit += clamp01(slope) * (0.10f + 0.90f * fexp(d / 0.10f)) * hi * 0.32f;
        }
      }
      frT[n] = (uint16_t)(((int)(al * 255.0f) << 8) | (int)(clamp01(lit) * 255.0f));
    }
  }
  free(edgeX);
}
static void buildNebula() {
  nebLow = (float*)ps_malloc((size_t)NLG * NLG * NLF * sizeof(float));
  if (!nebLow) { Serial.println("Memory error (nebula)"); return; }
  if (semGo) {
    job = 3; xSemaphoreGive(semGo); nebLowRows(0, 2); xSemaphoreTake(semDone, portMAX_DELAY);
    job = 2; xSemaphoreGive(semGo); nebRows(0, 2, true); xSemaphoreTake(semDone, portMAX_DELAY); job = 0;
  } else { nebLowRows(0, 1); nebRows(0, 1, true); }
  free(nebLow); nebLow = nullptr;
  rnd = 4321u;                                     // 後ろの壁の、細かい星
  for (int i = 0; i < 2500; ++i) {
    const int x = (int)(frand() * T), y = (int)(frand() * T);
    const float q = frand(), v = q * q * q * 0.9f + 0.05f;
    const size_t n = gIdx(x, y);
    tex[n] = addLight(tex[n], 230, 230, 255, (uint32_t)(v * 32.0f));
    texL[n] = (uint8_t)min(255, texL[n] + (int)(v * 200.0f));
  }
}
static void genStarsNeb() {                        // 馬頭星雲の星: 奥行き(z: 0 = 壁のあたり 〜 1 = いちばん手前)をばらばらに
  rnd = 777u;
  for (int i = 0; i < NS3; ++i) {
    Star3& Q = st3[i];
    Q.sx = -1; Q.a = 0; Q.big = 0;
    if (i >= STARS_3D + NEB_FG) { Q.bk = 0; continue; }
    if (i >= STARS_3D) {                           // 手前に浮かぶ黒いちりの塊(下の雲のあたりに多い)
      Q.u = (frand() * 2.0f - 1.0f) * 1.3f; Q.v = -0.2f + frand() * 1.3f; Q.z = 0.65f + 0.35f * frand();
      Q.bk = (uint8_t)(110 + frand() * 110); Q.big = 3;
      continue;
    }
    Q.u = (frand() * 2.0f - 1.0f) * 1.5f; Q.v = (frand() * 2.0f - 1.0f) * 1.5f; Q.z = frand();
    const float q = frand();
    const float br = 0.06f + q * q * q * q * q * 0.9f;
    if (frand() < 0.6f) { Q.r = 210; Q.g = 225; Q.b = 255; } else { Q.r = 255; Q.g = 232; Q.b = 205; }
    Q.bk = (uint8_t)(clamp01(br) * 255.0f);
  }
}
static void buildSprites() {
  for (int y = -20; y <= 20; ++y)
    for (int x = -20; x <= 20; ++x) {
      const float d2 = (float)(x * x + y * y);
      const float a = 32.0f * expf(-d2 / 90.0f);
      glowB[(y + 20) * 41 + (x + 20)] = (uint8_t)(a > 32 ? 32 : a);
    }
  for (int y = -5; y <= 5; ++y)
    for (int x = -5; x <= 5; ++x) {
      const float d2 = (float)(x * x + y * y);
      const float a = 32.0f * expf(-d2 / 6.0f);
      glowS[(y + 5) * 11 + (x + 5)] = (uint8_t)(a > 32 ? 32 : a);
    }
  for (int y = -10; y <= 10; ++y)
    for (int x = -10; x <= 10; ++x) {
      const float d2 = (float)(x * x + y * y);
      const float a = 32.0f * expf(-d2 / 18.0f) + 10.0f * expf(-d2 / 60.0f);
      glowA[(y + 10) * 21 + (x + 10)] = (uint8_t)(a > 32 ? 32 : a);
    }
  for (int y = -7; y <= 7; ++y)
    for (int x = -7; x <= 7; ++x) {
      const float d2 = (float)(x * x + y * y);
      float a = 32.0f * expf(-d2 / 1.2f) + 8.0f * expf(-d2 / 6.0f);
      if (x == 0 || y == 0) { const float t = 1.0f - (float)(abs(x) + abs(y)) / 8.0f; a += 18.0f * t * t; }
      starA[(y + 7) * 15 + (x + 7)] = (uint8_t)(a > 32 ? 32 : a);
    }
}
static inline float gauss() { return (frand() + frand() + frand() + frand() - 2.0f) * 1.732f; }   // だいたい 平均 0・ばらつき 1
static void genStars(int type) {                  // 立体の星: ふくらみ・まわりに広がる古い星・円盤(若い星は腕にそって、うすく)。そのあと星雲のかたまりとちりの粒
  if (type == NEBULA) { genStarsNeb(); return; }
  const GalParam& P = GP[type];
  rnd = 9000u + type * 131u;
  for (int i = 0; i < STARS_3D; ++i) {
    Star3& Q = st3[i];
    const float kind = frand();
    float br;
    if (kind < 0.20f) {
      Q.u = gauss() * 0.10f; Q.v = gauss() * 0.10f; Q.z = gauss() * 0.10f * BULGE_FLAT;
      if (P.bar > 0 && frand() < 0.5f) { Q.u = (frand() * 2.0f - 1.0f) * P.barLen; Q.v = gauss() * 0.03f; Q.z = gauss() * 0.03f; }
      Q.r = 255; Q.g = 215; Q.b = 160; br = 0.35f + 0.65f * frand();
    } else if (kind < 0.23f) {
      const float rr = 0.25f + 0.95f * frand(), ct = frand() * 2.0f - 1.0f, st = sqrtf(1.0f - ct * ct), ph = frand() * 6.2831853f;
      Q.u = rr * st * cosf(ph); Q.v = rr * st * sinf(ph); Q.z = rr * ct;
      Q.r = 255; Q.g = 225; Q.b = 190; br = 0.15f + 0.25f * frand();
    } else {
      const bool young = frand() < 0.55f;
      float rr = 0, tt = 0;
      for (int t2 = 0; t2 < 24; ++t2) {
        rr = -0.30f * logf(1.0f - frand() * 0.95f) + 0.02f; tt = frand() * 6.2831853f - 3.1415927f;
        if (!young) break;
        const float a = 0.5f + 0.5f * cosf(armPhase(P, rr, tt));
        if (frand() < a * a * a * a && !(P.bar > 0 && rr < P.barLen * 0.6f)) break;
      }
      Q.u = rr * cosf(tt); Q.v = rr * sinf(tt);
      Q.z = gauss() * (young ? 0.014f + 0.016f * rr : 0.032f + 0.040f * rr);
      if (young) { Q.r = 185; Q.g = 210; Q.b = 255; } else { Q.r = 255; Q.g = 232; Q.b = 200; }
      const float q = frand(); br = (young ? 0.34f : 0.24f) + q * q * q * 0.8f;
    }
    Q.bk = (uint8_t)(clamp01(br) * 255.0f); Q.sx = -1; Q.big = 0;
  }
  for (int i = STARS_3D; i < STARS_3D + NEB_3D; ++i) {   // 赤い星雲のかたまり: 腕にそって、円盤から少し浮く
    Star3& Q = st3[i];
    float rr = 0.3f, tt = 0;
    for (int t2 = 0; t2 < 40; ++t2) {
      rr = 0.12f + frand() * 0.78f; tt = frand() * 6.2831853f - 3.1415927f;
      const float a = 0.5f + 0.5f * cosf(armPhase(P, rr, tt));
      if (frand() < powf(a, 6.0f) && !(P.bar > 0 && rr < P.barLen * 0.6f)) break;
    }
    Q.u = rr * cosf(tt); Q.v = rr * sinf(tt); Q.z = gauss() * 0.025f;
    Q.r = 255; Q.g = 95; Q.b = 135; Q.bk = (uint8_t)(90 + frand() * 120); Q.sx = -1; Q.big = 1;
  }
  for (int i = STARS_3D + NEB_3D; i < NS3; ++i) {   // ちりの粒: ちりの多い所に、円盤から少し浮く
    Star3& Q = st3[i];
    float u = 0, v = 0, tr = 1;
    for (int t2 = 0; t2 < 40; ++t2) {
      const float rr = 0.08f + frand() * 0.88f, tt = frand() * 6.2831853f;
      u = rr * cosf(tt); v = rr * sinf(tt);
      const int ix = (int)((u / TEXR * 0.5f + 0.5f) * 256), iy = (int)((v / TEXR * 0.5f + 0.5f) * 256);
      tr = (ix >= 0 && iy >= 0 && ix < 256 && iy < 256) ? tT[tIdx256(ix, iy)] / 255.0f : 1.0f;
      if (frand() < (1.0f - tr) * 1.6f) break;
    }
    Q.u = u; Q.v = v; Q.z = gauss() * 0.012f;
    Q.bk = (uint8_t)(clamp01(0.45f + (1.0f - tr)) * 255.0f); Q.r = Q.g = Q.b = 0; Q.sx = -1; Q.big = 2;
  }
}
// 銀河の面の上の点(と高さ z)→ 画面。f: 遠近の倍率(手前ほど大きい)
static inline bool viewToScreen(float u, float v, float z, float& sx, float& sy, float& f) {
  const float x = vCf * u + vSf * v, y = -vSf * u + vCf * v;
  const float Y = y * inc - z * vSI, dep = y * vSI + z * inc;
  if (dep > CAM_DIST - 0.4f) return false;
  f = CAM_DIST / (CAM_DIST - dep);
  sx = (vC * x - vS * Y) * vK * f + S * 0.5f; sy = (vS * x + vC * Y) * vK * f + S * 0.5f;
  return true;
}static void buildBulge() {                        // ふくらみの光: 芯と、まわりのにじみ(中心からの距離 → 色)
  const float K = S * 0.5f * GAL_SIZE, R = 0.42f, sg = 0.088f, sg2 = 0.15f;
  float F = 0;
  for (float r = 0.0005f; r < 0.5f; r += 0.001f) F += fminf(6.0f, 0.003f * expf(-7.67f * (powf(r / 0.075f, 0.40f) - 1.0f))) * 6.2831853f * r * 0.001f;
  bulRpx = R * K; bulK = 512.0f / (bulRpx * bulRpx);
  for (int i = 0; i < 512; ++i) {
    const float r = R * sqrtf((i + 0.5f) / 512.0f) + 0.002f;
    const float B = fminf(6.0f, 0.003f * expf(-7.67f * (powf(r / 0.075f, 0.40f) - 1.0f)));
    const float G = F / (6.2831853f * sg * sg) * expf(-r * r / (2.0f * sg * sg));
    const float G2 = expf(-r * r / (2.0f * sg2 * sg2));   // ふくらみの外側の、うすい光
    const float V = (B + 0.85f * G + 0.30f * G2) * clamp01((R - r) / 0.08f);
    bulR[i] = (uint8_t)((1.0f - expf(-V * 1.00f * 1.7f)) * 255.0f);
    bulG[i] = (uint8_t)((1.0f - expf(-V * 0.83f * 1.7f)) * 255.0f);
    bulB[i] = (uint8_t)((1.0f - expf(-V * 0.58f * 1.7f)) * 255.0f);
    const float L = V * (0.30f + 0.55f * 0.83f + 0.15f * 0.58f);
    bulL[i] = (uint8_t)(clamp01(powf(1.0f - expf(-L * 1.7f * 1.3f), 1.45f) * 1.02f) * 255.0f);
  }
}
// 立体の星を画面に写して、帯ごとに分ける
// 立体の星を画面に写す(i0〜i1 の星。帯ごとの数は cnt に数える)。2つのコアで半分ずつ
static uint16_t (&projCnt)[2][66] = *reinterpret_cast<uint16_t(*)[2][66]>(NM_ARENA + 4096);   // 共通の置き場
static void projPart(int i0, int i1, uint16_t* cnt) {
  const bool neb = galType == NEBULA;
  const float NK = S * 0.5f / 0.90f, hh = S * 0.5f;
  for (int i = i0; i < i1; ++i) {
    Star3& Q = st3[i];
    if (neb) {                                     // 馬頭星雲: 手前の星ほど、のぞく向きで大きくずれる。壁より奥の星は、手前の雲で隠れる
      if (i >= STARS_3D + NEB_FG) { Q.sx = -1; continue; }
      const float p = 0.02f + 0.90f * Q.z, fx = Q.u - camX * p, fy = Q.v - camY * p;
      const int sx = (int)(hh + fx * NK), sy = (int)(hh + fy * NK);
      if (i >= STARS_3D) {                         // 手前のちりの塊(帯をまたぐので、帯ごとには分けない)
        Q.sx = (sx < -30 || sy < -30 || sx >= S + 30 || sy >= S + 30) ? -1 : (int16_t)sx; Q.sy = (int16_t)sy; Q.a = (uint8_t)(Q.bk * 34 / 255);
        continue;
      }
      if (sx < 0 || sy < 0 || sx >= S - 1 || sy >= S) { Q.sx = -1; continue; }
      float a = Q.bk * (26.0f / 255.0f) * (0.5f + 0.8f * Q.z);
      if (p < PX_CLOUD) {
        const int ix = (int)(((fx + camX * PX_CLOUD) / NEB_R * 0.5f + 0.5f) * T), iy = (int)(((fy + camY * PX_CLOUD) / NEB_R * 0.5f + 0.5f) * T);
        if (ix >= 0 && iy >= 0 && ix < T && iy < T) a *= (255 - (frT[gIdx(ix, iy)] >> 8)) * (1.0f / 255.0f);
      }
      Q.a = (uint8_t)(a > 48 ? 48 : a); Q.big = (Q.z > 0.85f && Q.bk > 170) ? 1 : 0;
      Q.sx = (int16_t)sx; Q.sy = (int16_t)sy;
      ++cnt[sy / BAND + 1];
      continue;
    }
    float sx, sy, f;
    if (!viewToScreen(Q.u, Q.v, Q.z, sx, sy, f) || sx < 0 || sy < 0 || sx >= S - 1 || sy >= S) { Q.sx = -1; continue; }
    Q.sx = (int16_t)sx; Q.sy = (int16_t)sy;
    if (i >= STARS_3D) {                           // 星雲のかたまり・ちりの粒(帯をまたぐので、帯ごとには分けない)
      float a = (i < STARS_3D + NEB_3D) ? Q.bk * (12.0f / 255.0f) * f * f : Q.bk * (24.0f / 255.0f) * (0.03f + 0.97f * vSI * vSI) * f;
      if (i < STARS_3D + NEB_3D && Q.z < 0) a *= 0.45f;
      Q.a = (uint8_t)(a > 48 ? 48 : a);
      continue;
    }
    float a = Q.bk * (30.0f / 255.0f) * f * f;
    if (Q.z < 0 && Q.u * Q.u + Q.v * Q.v < 1.0f) a *= 0.40f;   // 円盤の向こう側: ちりで暗い
    Q.a = (uint8_t)(a > 48 ? 48 : a); Q.big = (f > 1.10f && Q.bk > 150) ? 1 : 0;
    ++cnt[(int)sy / BAND + 1];
  }
}
static void projStars() {
  for (int c = 0; c < 2; ++c) for (int k = 0; k <= NB; ++k) projCnt[c][k] = 0;
  if (semGo) { job = 7; xSemaphoreGive(semGo); projPart(0, NS3 / 2, projCnt[1]); xSemaphoreTake(semDone, portMAX_DELAY); job = 0; }
  else { projPart(0, NS3 / 2, projCnt[1]); projPart(NS3 / 2, NS3, projCnt[0]); }
  for (int k = 0; k <= NB; ++k) st3Band[k] = (uint16_t)(projCnt[0][k] + projCnt[1][k]);
  for (int k = 0; k < NB; ++k) st3Band[k + 1] += st3Band[k];
  static uint16_t fill[64];
  for (int k = 0; k < NB && k < 64; ++k) fill[k] = st3Band[k];
  for (int i = 0; i < STARS_3D; ++i) if (st3[i].sx >= 0) st3Idx[fill[st3[i].sy / BAND]++] = (uint16_t)i;
}static void buildGrain() {
  uint32_t h = 777;
  for (int i = 0; i < 4096; ++i) {
    h = hash32(h + i);
    const int a = (int)(h & 15) + (int)((h >> 4) & 15) - 15;   // -15..15(まん中が多い)
    grainT[i] = (int8_t)(a * 3 / 15);
  }
}
// 銀の色の表: 明るさの段のさかい目に、細い虹の線(時間で色がまわる)
static void buildPal() {
  if (style >= 2) {                                // 鮮やかな色: 明るさで6色の表を引き、時間でずらす。暗い所は暗く、いちばん明るい所は白へ
    const float (*C)[3] = VIVID[style - 2];
    for (int i = 0; i < 256; ++i) {
      const float v = clamp01(i / 238.0f);
      const bool nb = galType == NEBULA;           // 星雲は色の幅をせまく(形が色にうもれないように)
      float t = v * (nb ? 0.55f : 1.35f) + simT * COLOR_FLOW;
      t -= floorf(t);
      const float q = t * 6.0f; const int a = (int)q % 6, c = (a + 1) % 6; float f = q - (int)q;
      f = f * f * (3.0f - 2.0f * f);
      const float env = powf(v, nb ? 0.95f : 0.75f) * (nb ? 1.70f : 1.30f), w = clamp01((v - 0.82f) / 0.18f) * 0.65f;
      const float ma = 1.0f / fmaxf(0.05f, fmaxf(C[a][0], fmaxf(C[a][1], C[a][2]))), mc = 1.0f / fmaxf(0.05f, fmaxf(C[c][0], fmaxf(C[c][1], C[c][2])));
      float r = (C[a][0] * ma + (C[c][0] * mc - C[a][0] * ma) * f) * env * 0.80f, g = (C[a][1] * ma + (C[c][1] * mc - C[a][1] * ma) * f) * env * 0.80f, b = (C[a][2] * ma + (C[c][2] * mc - C[a][2] * ma) * f) * env * 0.80f;
      const float gr = (r + g + b) * (1.0f / 3.0f);   // 色を濃く(星雲の壁は奥なので、かすませる)
      const float sat = nb ? 0.85f : 1.3f, dk = nb ? 0.82f : 1.0f;
      r = (gr + (r - gr) * sat) * dk; g = (gr + (g - gr) * sat) * dk; b = (gr + (b - gr) * sat) * dk;
      r = r * (1 - w) + w; g = g * (1 - w) + w; b = b * (1 - w) + w;
      palS[i] = toPix(r, g, b);
    }
    {                                              // 雲のふちの色: 表の反対側の色を、明るく濃く
      float t = 0.35f + 0.50f + simT * COLOR_FLOW;
      t -= floorf(t);
      const float q = t * 6.0f; const int a = (int)q % 6, c = (a + 1) % 6; float f = q - (int)q;
      f = f * f * (3.0f - 2.0f * f);
      float r = C[a][0] + (C[c][0] - C[a][0]) * f, g = C[a][1] + (C[c][1] - C[a][1]) * f, b = C[a][2] + (C[c][2] - C[a][2]) * f;
      const float mx = fmaxf(0.05f, fmaxf(r, fmaxf(g, b)));
      r /= mx; g /= mx; b /= mx;
      accR = (int)((r * 0.85f + 0.15f) * 255.0f); accG = (int)((g * 0.85f + 0.15f) * 255.0f); accB = (int)((b * 0.85f + 0.15f) * 255.0f);
    }
    return;
  }
  for (int i = 0; i < 256; ++i) {
    const float v = i / 238.0f;
    float r = v * 0.93f, g = v * 0.96f, b = v * 1.00f + 0.004f;
    const float f = v * 5.0f + 0.35f, d = fabsf(f - floorf(f + 0.5f));   // 段のさかい目からの近さ
    const float w = clamp01(1.0f - d / 0.034f) * clamp01((v - 0.10f) / 0.10f) * 0.18f;
    if (w > 0) {
      const float h = v * 1.7f + simT * 0.07f;
      const float cr = 0.5f + 0.5f * cosf(6.2831853f * h), cg = 0.5f + 0.5f * cosf(6.2831853f * (h - 0.333f)), cb = 0.5f + 0.5f * cosf(6.2831853f * (h - 0.667f));
      r = r * (1 - w) + (cr * 0.45f + 0.55f) * w; g = g * (1 - w) + (cg * 0.45f + 0.55f) * w; b = b * (1 - w) + (cb * 0.45f + 0.55f) * w;
    }
    palS[i] = toPix(r, g, b);
  }
}
static void initBg() {
  rnd = 4242;
  for (int i = 0; i < NBG + NFG; ++i) {
    BgStar& b = bg[i];
    b.x = frand() * S; b.y = frand() * S;
    b.depth = i < NBG ? 0.2f + frand() * 0.8f : 1.5f + frand();
    const float q = frand();
    b.b = i < NBG ? q * q * q * 0.9f + 0.08f : 0.7f + 0.3f * frand();
    b.tw = frand() * 6.28f; b.col = (uint8_t)(frand() * 4);
  }
}

// ---------------------------------------------------------------- 動き
static void setView(float dt) {
  // 傾きで見る角度: 手前・奥で傾き(真上から 〜 横から)、左右で向き
  if (!grabbing) { incOff *= expf(-dt * 0.30f); camDX *= expf(-dt * 0.30f); camDY *= expf(-dt * 0.30f); }   // はなすと、倒した分・のぞいた分はゆっくり戻る
  float incGoal = clamp01(0.55f + tiltY * 0.85f * TILT_GAIN) * 0.82f + 0.18f + incOff;
  incGoal = incGoal < INC_MIN ? INC_MIN : (incGoal > 1.0f ? 1.0f : incGoal);
  const float paGoal = 0.55f + tiltX * 1.4f * TILT_GAIN;
  const float a = 1.0f - expf(-dt * (grabbing ? 14.0f : 3.0f));
  inc += (incGoal - inc) * a; pa += (paGoal - pa) * a;
  vC = cosf(pa); vS = sinf(pa); vCf = cosf(phi); vSf = sinf(phi); vSI = sqrtf(fmaxf(0.0f, 1.0f - inc * inc));
  vK = S * 0.5f * GAL_SIZE; vIK = 1.0f / vK; vIncD = inc * CAM_DIST; vTS = T * 0.5f / TEXR;
  {                                                // ふくらみ: 少しつぶれた玉を、この角度から見た形
    const float q2 = inc * inc + BULGE_FLAT * BULGE_FLAT * (1.0f - inc * inc);
    bulC = vC; bulS = vS; bulIQ2 = 1.0f / q2;
  }
  // 馬頭星雲: のぞく向き(傾き + なぞった分 + ゆっくりただよう)。後ろの壁は少し、手前の雲は大きくずれる
  // 傾きは、小さなゆれ(0.03 まで)を無視し、端に近づくほどゆっくり止まるようにする(ぴたっと止まらない)
  const float dzx = fabsf(tiltX) < 0.03f ? 0.0f : tiltX - copysignf(0.03f, tiltX), dzy = fabsf(tiltY) < 0.03f ? 0.0f : tiltY - copysignf(0.03f, tiltY);
  float gx = dzx * 1.4f * TILT_GAIN + camDX + 0.12f * sinf(simT * 0.05f), gy = dzy * 1.4f * TILT_GAIN + camDY + 0.08f * sinf(simT * 0.037f);
  gx = gx / sqrtf(1.0f + gx * gx); gy = gy / sqrtf(1.0f + gy * gy);
  camX += (gx - camX) * a; camY += (gy - camY) * a;
  const float tpu = T * 0.5f / NEB_R;              // 星雲の 1 = 絵の何点か
  nbStep = (int32_t)(0.90f / (S * 0.5f) * tpu * 65536.0f);
  nbU = (int32_t)((T * 0.5f + camX * PX_WALL * tpu) * 65536.0f); nbV = (int32_t)((T * 0.5f + camY * PX_WALL * tpu) * 65536.0f);
  nfDU = (int32_t)(camX * (PX_CLOUD - PX_WALL) * tpu * 65536.0f); nfDV = (int32_t)(camY * (PX_CLOUD - PX_WALL) * tpu * 65536.0f);
  const float NK = S * 0.5f / 0.90f;
  spX[0] = S * 0.5f + (0.10f - camX * 0.08f) * NK; spY[0] = S * 0.5f + (-0.80f - camY * 0.08f) * NK;   // 壁を照らす明るい星
  spX[1] = S * 0.5f + (-0.74f - camX * 0.60f) * NK; spY[1] = S * 0.5f + (0.52f - camY * 0.60f) * NK;   // 炎の星雲のそばの明るい星(手前)
}static void diskToScreen(float tu, float tv, float& sx, float& sy) {   // 銀河の面 → 画面(点)。馬頭星雲では、手前の雲の面
  if (galType == NEBULA) { const float NK = S * 0.5f / 0.90f; sx = S * 0.5f + (tu - camX * PX_CLOUD) * NK; sy = S * 0.5f + (tv - camY * PX_CLOUD) * NK; return; }
  float f; if (!viewToScreen(tu, tv, 0.0f, sx, sy, f)) { sx = sy = -1000.0f; }
}static void screenToDisk(float sx, float sy, float& tu, float& tv) {   // 画面(点)→ 銀河の面(遠近の逆)
  if (galType == NEBULA) { const float NK = S * 0.5f / 0.90f; tu = (sx - S * 0.5f) / NK + camX * PX_CLOUD; tv = (sy - S * 0.5f) / NK + camY * PX_CLOUD; return; }
  const float dx = sx - S * 0.5f, dy = sy - S * 0.5f, a = (vC * dx + vS * dy) * vIK, b = (-vS * dx + vC * dy) * vIK;
  float den = vIncD + b * vSI; if (den < 0.05f) den = 0.05f;
  const float x = a * vIncD / den, y = b * CAM_DIST / den;
  tu = vCf * x - vSf * y; tv = vSf * x + vCf * y;
}static void addBirth(float tu, float tv, float k) {
  Birth& b = birth[birthNext]; birthNext = (birthNext + 1) % MAXBIRTH;
  b.u = tu; b.v = tv; b.age = 0; b.life = 22.0f + frand() * 10.0f; b.k = k; b.seed = (uint32_t)(frand() * 1e9f);
}
static void simulate(float dt) {
  simT += dt;
  const float sp = SPEEDS[speedIdx];
  if (!grabbing) {                                 // 指でつかんでいる間は、指が回す
    phi += dt * (SPIN * sp + spinV);
    if (FLING_KEEP > 0) spinV *= expf(-dt * FLING_KEEP);
  }
  while (phi > 6.2831853f) phi -= 6.2831853f;
  while (phi < 0) phi += 6.2831853f;
  for (int i = 0; i < MAXBIRTH; ++i) if (birth[i].k > 0) { birth[i].age += dt * (0.6f + 0.4f * sp); if (birth[i].age > birth[i].life) birth[i].k = 0; }
  novaNext -= dt;
  if (novaNext <= 0 && galType != NEBULA) {         // 超新星: 腕のあたりのどこか
    for (int i = 0; i < MAXNOVA; ++i) if (!nova[i].on) {
      const float rr = 0.18f + frand() * 0.65f, tt = frand() * 6.2831853f;
      nova[i] = {rr * cosf(tt), rr * sinf(tt), 0, 5.0f + frand() * 3.0f, true};
      break;
    }
    novaNext = 6.0f + frand() * 8.0f;
  }
  for (int i = 0; i < MAXNOVA; ++i) if (nova[i].on) { nova[i].age += dt; if (nova[i].age > nova[i].life) nova[i].on = false; }
  if (style >= 1) buildPal();                      // 銀は虹の線、鮮やかな色は表そのものが、時間でまわる
  grainOff = (grainOff + 1237u) & 4095u;
  setView(dt);
  projStars();
}

// ---------------------------------------------------------------- 描く
static const uint8_t STARC[4][3] = {{255, 225, 190}, {200, 220, 255}, {255, 255, 255}, {255, 190, 140}};
static inline __attribute__((always_inline)) void sprite(uint16_t* out, int bx, int bw, int y0, int h, const uint8_t* A, int R, int px, int py, int r, int g, int b, uint32_t k) {
  if (py + R < y0 || py - R >= y0 + h || k == 0) return;
  const int D = 2 * R + 1;
  for (int yy = max(y0, py - R); yy <= min(y0 + h - 1, py + R); ++yy) {
    uint16_t* o = out + (yy - y0) * bw - bx;
    const uint8_t* a = A + (yy - py + R) * D + R;
    const int xa = max(max(bx, (int)spanA[yy]), px - R), xb = min(min(bx + bw - 1, (int)spanB[yy]), px + R);
    for (int x = xa; x <= xb; ++x) { const uint32_t q = (a[x - px] * k) >> 5; if (q) o[x] = addLight(o[x], r, g, b, q > 48 ? 48 : q); }
  }
}
static inline __attribute__((always_inline)) uint16_t dimPix(uint16_t p, uint32_t m) {   // 暗くする(m: 0..256)
  const uint32_t c = __builtin_bswap16(p);
  const uint32_t R = ((c >> 11) * m) >> 8, G = (((c >> 5) & 63) * m) >> 8, B = ((c & 31) * m) >> 8;
  return __builtin_bswap16((uint16_t)((R << 11) | (G << 5) | B));
}
static inline __attribute__((always_inline)) void darkSprite(uint16_t* out, int bx, int bw, int y0, int h, const uint8_t* A, int R, int px, int py, uint32_t k) {
  if (py + R < y0 || py - R >= y0 + h || k == 0) return;
  const int D = 2 * R + 1;
  for (int yy = max(y0, py - R); yy <= min(y0 + h - 1, py + R); ++yy) {
    uint16_t* o = out + (yy - y0) * bw - bx;
    const uint8_t* a = A + (yy - py + R) * D + R;
    const int xa = max(max(bx, (int)spanA[yy]), px - R), xb = min(min(bx + bw - 1, (int)spanB[yy]), px + R);
    for (int x = xa; x <= xb; ++x) { uint32_t q = (a[x - px] * k) >> 3; if (q) o[x] = dimPix(o[x], 256 - (q > 200 ? 200 : q)); }
  }
}
static void IRAM_ATTR renderBand(int kb, uint16_t* out, int bx, int bw) {
  const int y0 = kb * BAND, h = min(BAND, S - y0);
  const float hh = S * 0.5f;
  const bool neb = galType == NEBULA;
  for (int rr = 0; rr < h; ++rr) {
    const int y = y0 + rr;
    uint16_t* o = out + rr * bw - bx;
    const int sa = max((int)spanA[y], bx), sb = min((int)spanB[y], bx + bw - 1);
    for (int x = bx; x < sa; ++x) o[x] = 0;
    for (int x = sb + 1 < bx ? bx : sb + 1; x < bx + bw; ++x) o[x] = 0;
    if (sa > sb) continue;
    const uint32_t g0 = (uint32_t)y * 131u + grainOff;
    if (neb) {                                     // 馬頭星雲: 後ろの壁に、手前の雲(暗さ・ふちの光)を重ねる。雲はずれ方がちがう
      int32_t U = nbU + nbStep * (sa - (int)hh), V = nbV + nbStep * (y - (int)hh);
      for (int x = sa; x <= sb; ++x) {
        int ix = U >> 16, iy = V >> 16, jx = (U + nfDU) >> 16, jy = (V + nfDV) >> 16;
        ix = ix < 0 ? 0 : (ix > T - 1 ? T - 1 : ix); iy = iy < 0 ? 0 : (iy > T - 1 ? T - 1 : iy);
        jx = jx < 0 ? 0 : (jx > T - 1 ? T - 1 : jx); jy = jy < 0 ? 0 : (jy > T - 1 ? T - 1 : jy);
        const uint32_t fr = frT[gIdx(jx, jy)];
        const uint32_t al = fr >> 8, lit = fr & 255;
        if (style == 0) {
          uint16_t p = tex[gIdx(ix, iy)];
          if (al) p = dimPix(p, 256 - al);
          if (lit) p = addLight(p, 255, 105, 100, lit >> 3);
          o[x] = p;
        } else if (style >= 2) {                   // 鮮やかな色: 壁だけを色の表で。雲は影、ふちは反対側の色で光る
          const int l = texL[gIdx(ix, iy)] + grainT[(g0 + (uint32_t)x * 7u) & 4095u];
          uint16_t p = palS[l < 0 ? 0 : (l > 255 ? 255 : l)];
          if (al) p = dimPix(p, 256 - al);
          if (lit) p = addLight(p, accR, accG, accB, lit >> 3);
          o[x] = p;
        } else {
          int l = ((texL[gIdx(ix, iy)] * (256 - al)) >> 8) + (lit >> 1) + grainT[(g0 + (uint32_t)x * 7u) & 4095u];
          o[x] = palS[l < 0 ? 0 : (l > 255 ? 255 : l)];
        }
        U += nbStep;
      }
      continue;
    }
    // 銀河: 画面の点 → 銀河の面(遠近つき)。割り算は 8 点ごとに1回で、あいだは足し算でつなぐ
    const float dy = y - hh, rowA = vS * dy * vIK, rowB = vC * dy * vIK, stA = vC * vIK, stB = -vS * vIK;
    auto texAt = [&](int x, int32_t& U, int32_t& V) -> bool {
      const float dx = x - hh, a = rowA + stA * dx, b = rowB + stB * dx, den = vIncD + b * vSI;
      if (den < 0.02f) return false;               // 地平線の向こう
      const float id = CAM_DIST / den, xx = a * inc * id, yy = b * id;
      const float fu = (vCf * xx - vSf * yy) * vTS + T * 0.5f, fv = (vSf * xx + vCf * yy) * vTS + T * 0.5f;
      if (!(fu > -2048.0f && fu < 2048.0f && fv > -2048.0f && fv < 2048.0f)) return false;
      U = (int32_t)(fu * 65536.0f); V = (int32_t)(fv * 65536.0f);
      return true;
    };
    auto put = [&](int x, int32_t U, int32_t V, bool ok) {
      const uint32_t ix = (uint32_t)(U >> 16), iy = (uint32_t)(V >> 16);
      const bool in = ok && ix < (uint32_t)T && iy < (uint32_t)T;
      if (style == 0) o[x] = in ? tex[gIdx(ix, iy)] : 0;
      else { int l = (in ? texL[gIdx(ix, iy)] : 0) + grainT[(g0 + (uint32_t)x * 7u) & 4095u]; o[x] = palS[l < 0 ? 0 : (l > 255 ? 255 : l)]; }
    };
    int32_t U0 = 0, V0 = 0;
    bool ok0 = texAt(sa, U0, V0);
    for (int x0 = sa; x0 <= sb; x0 += 8) {
      const int x1 = min(x0 + 8, sb + 1), n = x1 - x0;
      int32_t U1 = 0, V1 = 0;
      const bool ok1 = texAt(x1, U1, V1);
      if (ok0 && ok1) {
        const int32_t dU = (U1 - U0) / n, dV = (V1 - V0) / n;
        int32_t U = U0, V = V0;
        const int32_t Ue = U0 + dU * (n - 1), Ve = V0 + dV * (n - 1);
        if ((uint32_t)(U0 >> 16) < (uint32_t)T && (uint32_t)(V0 >> 16) < (uint32_t)T && (uint32_t)(Ue >> 16) < (uint32_t)T && (uint32_t)(Ve >> 16) < (uint32_t)T) {
          // 区間の両はしが絵の中なら、あいだも絵の中(はみ出しの確かめを省く)
          if (style == 0) { for (int x = x0; x < x1; ++x) { o[x] = tex[gIdx((uint32_t)(U >> 16), (uint32_t)(V >> 16))]; U += dU; V += dV; } }
          else {
            for (int x = x0; x < x1; ++x) {
              const int l = texL[gIdx((uint32_t)(U >> 16), (uint32_t)(V >> 16))] + grainT[(g0 + (uint32_t)x * 7u) & 4095u];
              o[x] = palS[l < 0 ? 0 : (l > 255 ? 255 : l)]; U += dU; V += dV;
            }
          }
        } else for (int x = x0; x < x1; ++x) { put(x, U, V, true); U += dU; V += dV; }
      } else {
        for (int x = x0; x < x1; ++x) { int32_t U = 0, V = 0; const bool ok = texAt(x, U, V); put(x, U, V, ok); }
      }
      U0 = U1; V0 = V1; ok0 = ok1;
    }
    {                                              // 真ん中のふくらみ: 向こう側の分は、その画素の下のちりで暗く
      if (fabsf(dy) < bulRpx) {
        const float hw = sqrtf(bulRpx * bulRpx - dy * dy);
        const int xa = max(sa, (int)(hh - hw)), xb = min(sb, (int)(hh + hw) + 1);
        float a = bulC * (xa - hh) + bulS * dy, b = -bulS * (xa - hh) + bulC * dy;
        for (int x = xa; x <= xb; ++x) {
          const int idx = (int)((a * a + b * b * bulIQ2) * bulK);
          if (idx < 512) {
            int32_t U = 0, V = 0;
            uint32_t tr = 255;
            if (texAt(x, U, V)) { const uint32_t ix = (uint32_t)(U >> 16), iy = (uint32_t)(V >> 16); if (ix < (uint32_t)T && iy < (uint32_t)T) tr = tT[tIdx256(ix >> 1, iy >> 1)]; }
            const uint32_t al = (77 + ((tr * 179) >> 8)) >> 3;
            if (style == 0) o[x] = addLight(o[x], bulR[idx], bulG[idx], bulB[idx], al);
            else { const uint32_t p = __builtin_bswap16(palS[bulL[idx]]); o[x] = addLight(o[x], (p >> 8) & 0xF8, (p >> 3) & 0xFC, (p << 3) & 0xF8, al); }
          }
          a += bulC; b -= bulS;
        }
      }
    }
  }
  if (!neb) {                                      // ちりの粒(円盤から少し浮いた、暗い雲。横から見るほど濃い)
    for (int i = STARS_3D + NEB_3D; i < NS3; ++i) {
      const Star3& Q = st3[i];
      if (Q.sx >= 0 && Q.a) darkSprite(out, bx, bw, y0, h, glowS, 5, Q.sx, Q.sy, Q.a);
    }
  } else {                                         // 馬頭星雲: 手前に浮かぶ黒いちりの塊(いちばん大きくずれる)
    for (int i = STARS_3D; i < STARS_3D + NEB_FG; ++i) {
      const Star3& Q = st3[i];
      if (Q.sx > -30 && Q.a) darkSprite(out, bx, bw, y0, h, glowB, 20, Q.sx, Q.sy, Q.a);
    }
  }
  // まわりの星(奥行きで、傾けたときのずれが変わる)
  for (int i = 0; i < NBG + NFG; ++i) {
    const BgStar& b = bg[i];
    const float px = b.x - tiltX * b.depth * S * 0.10f * TILT_GAIN;
    const float py = b.y - tiltY * b.depth * S * 0.10f * TILT_GAIN + (style == 1 ? fsin(simT * (0.35f + 0.25f * b.depth) + b.tw * 3.0f) * 7.0f * b.depth : 0.0f);
    const int ix = (int)px, iy = (int)py;
    if (iy < y0 - 7 || iy >= y0 + h + 7) continue;
    const float tw = 0.75f + 0.25f * fsin(simT * (1.5f + b.depth) + b.tw);
    const uint8_t* C = STARC[style == 1 ? 2 : b.col];
    if (i < NBG) {
      if (iy < y0 || iy >= y0 + h || ix < max(bx, (int)spanA[iy]) || ix > min(bx + bw - 1, (int)spanB[iy])) continue;
      uint16_t* o = out + (iy - y0) * bw - bx;
      o[ix] = addLight(o[ix], C[0], C[1], C[2], (uint32_t)(b.b * tw * 32.0f));
    } else sprite(out, bx, bw, y0, h, starA, 7, ix, iy, C[0], C[1], C[2], (uint32_t)(b.b * tw * 32.0f));
  }
  // 立体の星(この帯の星だけ)
  for (int n = st3Band[kb]; n < st3Band[kb + 1]; ++n) {
    const Star3& Q = st3[st3Idx[n]];
    const int x = Q.sx, yy = Q.sy;
    if (yy < y0 || yy >= y0 + h || Q.a == 0) continue;
    const int xa = max(bx, (int)spanA[yy]), xb = min(bx + bw - 1, (int)spanB[yy]);
    if (x < xa || x > xb) continue;
    uint16_t* o = out + (yy - y0) * bw - bx;
    int r = Q.r, g = Q.g, b = Q.b;
    if (style == 1) { r = g = 170 + (Q.bk >> 2); b = r + 15 > 255 ? 255 : r + 15; }
    o[x] = addLight(o[x], r, g, b, Q.a);
    if ((Q.big || Q.bk > 190) && x + 1 <= xb) o[x + 1] = addLight(o[x + 1], r, g, b, Q.big ? Q.a : Q.a >> 1);
    if (Q.big) {                                   // 手前の星: 上下にも少し
      if (x - 1 >= xa) o[x - 1] = addLight(o[x - 1], r, g, b, Q.a >> 1);
      if (yy + 1 < y0 + h && x <= min(bx + bw - 1, (int)spanB[yy + 1]) && x >= max(bx, (int)spanA[yy + 1])) o[x + bw] = addLight(o[x + bw], r, g, b, Q.a >> 1);
      if (yy - 1 >= y0 && x <= min(bx + bw - 1, (int)spanB[yy - 1]) && x >= max(bx, (int)spanA[yy - 1])) o[x - bw] = addLight(o[x - bw], r, g, b, Q.a >> 1);
    }
  }
  if (!neb) {                                      // 赤い星雲のかたまり(円盤から少し浮く)
    for (int i = STARS_3D; i < STARS_3D + NEB_3D; ++i) {
      const Star3& Q = st3[i];
      if (Q.sx < 0 || !Q.a) continue;
      if (style == 1) sprite(out, bx, bw, y0, h, glowS, 5, Q.sx, Q.sy, 200, 205, 215, (uint32_t)(Q.a >> 1));
      else sprite(out, bx, bw, y0, h, glowS, 5, Q.sx, Q.sy, Q.r, Q.g, Q.b, Q.a);
    }
  } else {                                         // 馬頭星雲の、明るい星2つ
    sprite(out, bx, bw, y0, h, glowB, 20, (int)spX[0], (int)spY[0], 255, 200, 210, 22);
    for (int i = 0; i < 2; ++i) {
      const int r = i == 0 ? 225 : 210, g = i == 0 ? 235 : 225, b = 255;
      sprite(out, bx, bw, y0, h, glowA, 10, (int)spX[i], (int)spY[i], r, g, b, i == 0 ? 34 : 26);
      sprite(out, bx, bw, y0, h, starA, 7, (int)spX[i], (int)spY[i], 255, 255, 255, 48);
    }
  }
  // 生まれた星: 光の輪がひろがって、赤い星雲と青い星のかたまりが残る(銀河と一緒に回る)
  for (int i = 0; i < MAXBIRTH; ++i) {
    const Birth& B = birth[i];
    if (B.k <= 0) continue;
    float sx, sy; diskToScreen(B.u, B.v, sx, sy);
    const float a = B.age, fade = clamp01(1.0f - (a - B.life * 0.6f) / (B.life * 0.4f));
    if (a < 1.2f) {                                // 光の輪
      const float R = 6.0f + a * 60.0f, w = (1.0f - a / 1.2f) * B.k;
      const int iy0 = max(y0, (int)(sy - R - 2)), iy1 = min(y0 + h - 1, (int)(sy + R + 2));
      for (int yy = iy0; yy <= iy1; ++yy) {
        uint16_t* o = out + (yy - y0) * bw - bx;
        const float dy = yy - sy, wq = R * R - dy * dy;
        if (wq < -6 * R) continue;
        const float hw = sqrtf(fmaxf(0.0f, wq));
        for (int side = -1; side <= 1; side += 2) {
          const int xc = (int)(sx + side * hw);
          for (int x = xc - 2; x <= xc + 2; ++x) {
            if (x < max(bx, (int)spanA[yy]) || x > min(bx + bw - 1, (int)spanB[yy])) continue;
            const float d = fabsf(sqrtf((x - sx) * (x - sx) + dy * dy) - R);
            if (d < 2.0f) {
              if (style == 1) o[x] = addLight(o[x], 225, 232, 245, (uint32_t)((1.0f - d / 2.0f) * w * 26.0f));
              else o[x] = addLight(o[x], 200, 220, 255, (uint32_t)((1.0f - d / 2.0f) * w * 24.0f));
            }
          }
        }
      }
    }
    const float bloom = clamp01(a / 1.5f) * fade * B.k;
    if (style == 1) sprite(out, bx, bw, y0, h, glowA, 10, (int)sx, (int)sy, 200, 210, 230, (uint32_t)(bloom * 18.0f));
    else sprite(out, bx, bw, y0, h, glowA, 10, (int)sx, (int)sy, 255, 80, 130, (uint32_t)(bloom * 26.0f));
    uint32_t hs = B.seed;
    for (int j = 0; j < 5; ++j) {                  // 青い若い星
      hs = hash32(hs + j);
      const float ox = ((hs & 255) / 255.0f - 0.5f) * 16.0f, oy = (((hs >> 8) & 255) / 255.0f - 0.5f) * 16.0f;
      const float tw = 0.7f + 0.3f * fsin(simT * 3.0f + j * 1.7f);
      if (style == 1) sprite(out, bx, bw, y0, h, starA, 7, (int)(sx + ox), (int)(sy + oy), 240, 245, 255, (uint32_t)(bloom * tw * 30.0f));
      else sprite(out, bx, bw, y0, h, starA, 7, (int)(sx + ox), (int)(sy + oy), 170, 200, 255, (uint32_t)(bloom * tw * 30.0f));
    }
    if (a < 0.6f) sprite(out, bx, bw, y0, h, glowA, 10, (int)sx, (int)sy, 255, 255, 255, (uint32_t)((1.0f - a / 0.6f) * 40.0f * B.k));
  }
  // 超新星: すっと明るくなって、ゆっくり消える
  for (int i = 0; i < MAXNOVA; ++i) {
    const Nova& N = nova[i];
    if (!N.on) continue;
    float sx, sy; diskToScreen(N.u, N.v, sx, sy);
    const float a = N.age, k = a < 0.4f ? a / 0.4f : expf(-(a - 0.4f) * 0.7f);
    sprite(out, bx, bw, y0, h, glowA, 10, (int)sx, (int)sy, 255, 240, 220, (uint32_t)(k * 22.0f));
    sprite(out, bx, bw, y0, h, starA, 7, (int)sx, (int)sy, 255, 255, 255, (uint32_t)(k * 48.0f));
  }
}
static void renderFrame() {
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
static void workerTask(void*) {                   // コア0: 奇数番目の帯
  for (;;) {
    xSemaphoreTake(semGo, portMAX_DELAY);
    if (job == 1) { galRows(1, 2, false); xSemaphoreGive(semDone); continue; }   // 銀河の絵を作る手伝い
    if (job == 2) { nebRows(1, 2, false); xSemaphoreGive(semDone); continue; }   // 星雲の絵を作る手伝い
    if (job == 3) { nebLowRows(1, 2); xSemaphoreGive(semDone); continue; }
    if (job == 4) { galLowRows(1, 2); xSemaphoreGive(semDone); continue; }
    if (job == 5) { galDust(1, 2); xSemaphoreGive(semDone); continue; }
    if (job == 6) { galFinal(1, 2); xSemaphoreGive(semDone); continue; }
    if (job == 7) { projPart(NS3 / 2, NS3, projCnt[0]); xSemaphoreGive(semDone); continue; }   // 立体の星の後ろ半分を画面に写す
    uint32_t busy = 0;
    for (int k = 1; k < NB; k += 2) {
      while (__atomic_load_n(&doneUpTo, __ATOMIC_ACQUIRE) < k - NSLOT) { }
      if (bandW[k] > 0) { const uint32_t t0 = micros(); renderBand(k, band[k % NSLOT], bandX[k], bandW[k]); busy += micros() - t0; }
      __atomic_store_n(&readyW, k, __ATOMIC_RELEASE);
    }
    busyUs[0] = busy;
    xSemaphoreGive(semDone);
  }
}

// ---------------------------------------------------------------- 「銀河を作っています」の表示
// 画面外の絵に描いてから、まとめて送る(画面へ直接送るときは、横の位置と幅を2の倍数にそろえないと崩れる)
static void showMsg(const char* msg, float prog) {
  static M5Canvas* cv = nullptr;
  static const char* text = "";
  const int w = 280, h = 64, x0 = ((W - w) / 2) & ~1, y0 = ((H - h) / 2) & ~1;
  if (msg) {
    text = msg;
    M5.Display.fillScreen(TFT_BLACK);
    if (!cv) { cv = new M5Canvas(&M5.Display); cv->setPsram(true); cv->setColorDepth(16); if (!cv->createSprite(w, h)) { delete cv; cv = nullptr; } }
  }
  if (!cv) return;
  cv->fillScreen(TFT_BLACK);
  cv->setTextDatum(middle_center);
  cv->setFont(&fonts::lgfxJapanGothicP_16);
  cv->setTextColor(M5.Display.color565(170, 180, 205));
  cv->drawString(text, w / 2, 20);
  cv->fillRect(40, 44, w - 80, 4, M5.Display.color565(40, 42, 54));
  cv->fillRect(40, 44, (int)((w - 80) * clamp01(prog)), 4, M5.Display.color565(255, 210, 150));
  cv->pushSprite(x0, y0);
  if (prog >= 1.0f) { delete cv; cv = nullptr; }
}
static void setType(int t) {
  galType = t;
  const uint32_t t0 = millis();
  if (galType == NEBULA) { showMsg("星雲を作っています…", 0.0f); buildNebula(); }
  else { showMsg("銀河を作っています…", 0.0f); if (!buildGalaxy(galType)) { Serial.println("Memory error (galaxy)"); } }
  genStars(galType);
  showMsg(nullptr, 1.0f);
  for (int i = 0; i < MAXBIRTH; ++i) birth[i].k = 0;
  for (int i = 0; i < MAXNOVA; ++i) nova[i].on = false;
  Serial.printf("galaxy: %s  (%u ms)\n", TYPE_NAME[galType], (unsigned)(millis() - t0));
}static void setStyle(int st) {
  style = st;
  buildPal();
  Serial.printf("style: %s\n", STYLE_NAME[style]);
}
static void fail(const char* msg) {
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setTextColor(0xF800);
  M5.Display.setTextDatum(middle_center);
  M5.Display.drawString(msg, W / 2, H / 2);
  Serial.println(msg);
  while (true) nmIdle();
}

// ---------------------------------------------------------------- setup / loop
void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);
  Serial.begin(115200);
  Serial.println("NullGalaxy v13");
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
  tex = (uint16_t*)ps_malloc((size_t)(T / 4) * GSTRIP * 2);
  texL = (uint8_t*)ps_malloc((size_t)(T / 4) * GSTRIP);
  tT = (uint8_t*)ps_malloc(256 * 256);
  st3 = (Star3*)ps_calloc(NS3, sizeof(Star3));
  st3Idx = (uint16_t*)malloc(STARS_3D * 2); st3Band = (uint16_t*)calloc(NB + 2, 2);
  frT = (uint16_t*)ps_malloc((size_t)(T / 4) * GSTRIP * 2);
  if (!tT || !st3 || !st3Idx || !st3Band || !frT || NB > 64) fail("Memory error (stars)");
  buildNoise();
  if (!spanA || !spanB || !bandX || !bandW || !band[0] || !band[1] || !band[2] || !band[3] || !tex || !texL) fail("Memory error");
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
  for (int i = 0; i < 1024; ++i) sinT[i] = sinf(i * 6.2831853f / 1024.0f);
  buildSprites();
  buildGrain();
  buildBulge();
  initBg();
  for (int i = 0; i < MAXBIRTH; ++i) birth[i].k = 0;
  for (int i = 0; i < MAXNOVA; ++i) nova[i].on = false;
  imuOK = TILT && M5.Imu.isEnabled();
  semGo = xSemaphoreCreateBinary();
  semDone = xSemaphoreCreateBinary();
  if (!semGo || !semDone) fail("Memory error (task)");
  xTaskCreatePinnedToCore(workerTask, "galaxy", 6144, nullptr, 1, nullptr, 0);   // コア0 で帯を描く(銀河の絵を作るのも手伝う)
  setType(TYPE_START);
  setStyle(STYLE_START);
  rnd = (uint32_t)esp_random() | 1u;
  setView(1.0f);
}

void loop() {
  static uint32_t last = millis(), fpsT = millis(), lastTapMs = 0, lastBirthMs = 0;
  static int frames = 0;
  static bool was = false;
  M5.update();
  auto t = M5.Touch.getDetail();
  uint32_t now = millis();
  const bool corner = !roundScr && t.x < 56 && t.y < 56;   // 左上のすみはメニューに戻る所なので使わない
  bool changed = false;
  if (roundScr) {
    if (M5.BtnA.wasHold()) { speedIdx = (speedIdx + 1) % 3; spinV = 0; Serial.printf("speed: x%d\n", (int)SPEEDS[speedIdx]); }
    else if (M5.BtnA.wasClicked()) setStyle((style + 1) % NSTYLE);
    if (M5.BtnB.wasClicked()) { setType((galType + 1) % NTYPE); changed = true; }
  } else {
    if (M5.BtnPWR.wasClicked()) setStyle((style + 1) % NSTYLE);
    if (t.wasPressed() && !corner) {
      if (now - lastTapMs < 350) { setType((galType + 1) % NTYPE); changed = true; lastTapMs = 0; } else lastTapMs = now;
    }
  }
  if (changed) { now = millis(); last = now; }
  float dt = (now - last) / 1000.0f; last = now;
  if (dt > 0.1f) dt = 0.1f;

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
      if (levelN < 20) { ++levelN; levelX += (sx - levelX) / levelN; levelY += (sy - levelY) / levelN; }   // はじめの姿勢を「水平」に
      else if (TILT_RELEVEL > 0 && fabsf(sx - levelX) < TILT_RELEVEL_MAX && fabsf(sy - levelY) < TILT_RELEVEL_MAX) {   // 同じ角度で持ち続けたら、そこを少しずつ「水平」にする
        const float b = 1.0f - expf(-dt / TILT_RELEVEL); levelX += (sx - levelX) * b; levelY += (sy - levelY) * b;
      }
      sx -= levelX; sy -= levelY;
      sx = sx < -1 ? -1 : sx > 1 ? 1 : sx; sy = sy < -1 ? -1 : sy > 1 ? 1 : sy;
      const float a = 1.0f - expf(-dt * 4.0f);
      tiltX += (sx - tiltX) * a; tiltY += (sy - tiltY) * a;
    }
  }

  // 触る: タップで星が生まれる。なぞると、立体のまま動かす(横: 回る / 縦: 倒す)。はなすと、回る勢いはそのまま残り、倒した分はゆっくり戻る
  // 指を当てたまま待つと、はじいて回っているのが止まる
  static int pressX = 0, pressY = 0, lastX = 0, lastY = 0;
  static uint32_t pressMs = 0;
  static bool moved = false, skipTap = false;
  const int tx = t.x - XOFF, ty = t.y - YOFF;
  const bool touching = t.isPressed() && !corner && tx >= 0 && ty >= 0 && tx < S && ty < S;
  if (touching && !was) { pressX = lastX = tx; pressY = lastY = ty; pressMs = now; moved = false; skipTap = changed; }
  if (touching && was) {
    if (!moved) {
      const float mx = (float)(tx - pressX), my = (float)(ty - pressY);
      if (mx * mx + my * my > (S * 0.03f) * (S * 0.03f)) {   // 動かしはじめた: なぞる
        moved = true; grabbing = true; spinV = 0; lastX = pressX; lastY = pressY;
      }
    }
    if (moved && galType == NEBULA) {              // 馬頭星雲: なぞった向きに、のぞく向きが変わる(手前の雲ほど大きくずれる)
      camDX -= (tx - lastX) * (2.4f / S); camDY -= (ty - lastY) * (2.4f / S);
      camDX = camDX < -0.9f ? -0.9f : (camDX > 0.9f ? 0.9f : camDX); camDY = camDY < -0.9f ? -0.9f : (camDY > 0.9f ? 0.9f : camDY);
      lastX = tx; lastY = ty;
    } else if (moved) {                            // 手前のふちをつかんで動かす感じ(横: 回る / 縦: 下へなぞると真上から見る向きへ)
      const float dphi = (tx - lastX) * (DRAG_TURN / (S * 0.30f));
      phi += dphi;
      incOff += (ty - lastY) * (DRAG_TILT * 1.8f / S);
      incOff = incOff < -1.0f ? -1.0f : (incOff > 1.0f ? 1.0f : incOff);
      if (dt > 0.001f) spinV += (dphi / dt - spinV) * 0.35f;
      lastX = tx; lastY = ty;
    } else if (now - pressMs > 350) {              // 動かさずに指を当てたまま: はじいた回転が止まる
      spinV *= expf(-dt * 8.0f);
      if (fabsf(spinV) < 0.03f) spinV = 0;
    }
  }
  if (!touching && was) {
    if (!moved && !skipTap && now - pressMs <= 350) {   // タップ: 星が生まれる
      float tu, tv; screenToDisk((float)pressX, (float)pressY, tu, tv);
      addBirth(tu, tv, 1.0f);
      lastBirthMs = now;
    }
    if (grabbing) { spinV = spinV > 3.0f ? 3.0f : (spinV < -3.0f ? -3.0f : spinV); if (fabsf(spinV) < 0.05f) spinV = 0; }
    grabbing = false;
  }
  was = touching;

  simulate(dt);
  renderFrame();
  if (++frames, now - fpsT >= 3000) {
    Serial.printf("fps: %.1f  (%s, %s, x%d)  core0 %.1f ms  core1 %.1f ms  send %.1f ms\n", frames * 1000.0f / (now - fpsT), TYPE_NAME[galType], STYLE_NAME[style], (int)SPEEDS[speedIdx],
                  busyUs[0] / 1000.0f, busyUs[1] / 1000.0f, sendUs / 1000.0f);
    if (imuOK) Serial.printf("tilt: x %+.2f  y %+.2f  (incl %.2f)\n", tiltX, tiltY, inc);
    frames = 0; fpsT = now;
  }
}


// 共通の置き場に置いたものは、0 で始まる単純な型だけ(コンストラクタが要らないもの)
static_assert(std::is_trivially_default_constructible<BgStar>::value, "NM_ARENA: BgStar");
static_assert(std::is_trivially_default_constructible<Birth>::value, "NM_ARENA: Birth");
static_assert(std::is_trivially_default_constructible<Nova>::value, "NM_ARENA: Nova");
static_assert(std::is_trivially_default_constructible<bool>::value, "NM_ARENA: bool");
static_assert(std::is_trivially_default_constructible<float>::value, "NM_ARENA: float");
static_assert(std::is_trivially_default_constructible<float*>::value, "NM_ARENA: float*");
static_assert(std::is_trivially_default_constructible<int>::value, "NM_ARENA: int");
static_assert(std::is_trivially_default_constructible<int32_t>::value, "NM_ARENA: int32_t");
static_assert(std::is_trivially_default_constructible<int8_t>::value, "NM_ARENA: int8_t");
static_assert(std::is_trivially_default_constructible<uint16_t>::value, "NM_ARENA: uint16_t");
static_assert(std::is_trivially_default_constructible<uint32_t>::value, "NM_ARENA: uint32_t");
static_assert(std::is_trivially_default_constructible<uint8_t>::value, "NM_ARENA: uint8_t");
static constexpr size_t OVL_END = nmAlignTo(nmOv56, 16);
static_assert(OVL_END <= NM_ARENA_SIZE, "NM_ARENA_SIZE (nm_common.h) を大きくしてください");
extern const size_t OVL_SIZE = OVL_END;   // 起動時の確認用
}  // namespace NullGalaxy
