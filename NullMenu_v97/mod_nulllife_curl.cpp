// NullLife_Curl  -  NullMenu 用モジュール(元のスケッチ NullLife_Curl_v7 を名前空間に入れたもの)
// 0 で始まる変数(配列も)は、ほかの作品と共通の置き場(NM_ARENA)に置いています。
// 一度に動くのは1つだけ(切り替えるときは再起動)なので、重なっていても問題ありません。
#include "nm_common.h"
#if defined(ESP_PLATFORM) && __has_include(<lgfx/v1/panel/Panel_AMOLED.hpp>)
#include <lgfx/v1/panel/Panel_AMOLED.hpp>
#endif
#pragma GCC optimize("O3")

namespace NullLife_Curl {
static constexpr size_t nmOv0 = 0;

// NullLife_Curl v7  -  null²風「生きている鏡面」に、見えない渦の流れ(カールノイズ)を
//   M5Stack StopWatch(丸い画面)/ CoreS3・CoreS3 Lite・CoreS3 SE(四角い画面)両対応
//   画面の形を見て自動で切り替わります。
//
// カールノイズって?
//   なめらかなノイズ ψ を「流れの関数」とみなして、その傾きを90度回したものを流れにします。
//     流れ = ( ∂ψ/∂y , -∂ψ/∂x )
//   こうして作った流れは、どこにも「湧き出し」も「吸い込み」もありません(発散ゼロ)。
//   だから膜の模様は1か所に溜まったり薄まったりせず、ただ渦を巻いて、糸のように
//   引き伸ばされていきます。墨流し(マーブリング)や、液体の中の粒の動きそのものです。
//   画面のふちでは ψ をゼロに落としているので、流れはふちに沿って回り、外へは漏れません。
//
// この版で増えたこと:
//   ・膜の表面のぬめりが、見えない渦に乗って流れ、墨流しのように巻き込まれていく
//   ・触った跡(指で伸ばした膜)も流れに運ばれて、くるくる巻かれながら薄れていく
//   ・膜の下の生き物も、流れに少し押し流される
//   ・表面に小さな粒がいくつか浮かんでいて、渦に乗ってくるくる回る(流れが目で見える)
//   ・なぞると、指の動きで流れが押されて、左右に渦が2つできる
//   ・押さえたままでいると、指のまわりに渦が巻きはじめる
//
// 触るとぬるっと:
//   ・触る          : 膜が指に吸いつき、まわりがくぼんで、ぬちゃっと持ち上がる
//   ・生き物を押す  : ぬるっと指の下からすり抜けて、ぷるんと震える
//   ・なぞる        : 膜が指についてきて伸び、ぬめった跡がテカテカ光る
//   ・触り続ける    : 中の生き物が腕を伸ばして指にくっつく
//   ・指を離す      : 腕が糸を引いて細く伸び…ぷつっと切れて、ぷるんと縮む
//   ・放っておく    : 表面のぬめりが渦に乗って流れ、ときどき何かがぬるっと浮かぶ
//
// 操作:
//   StopWatch  … ボタンA 短押し: 景色(銀/夕暮れ/ネオン/金)  ボタンA 長押し: 流れの強さ(穏やか/ふつう/嵐)
//                ボタンB: 明るさ
//   CoreS3系   … ダブルタップ: 景色               電源ボタン短押し: 明るさ
//
// 必要なもの:
//   ボードマネージャー M5Stack >= 3.3.7
//   ボード: StopWatch なら「M5StopWatch」、CoreS3系なら「M5CoreS3」
//   ライブラリ M5Unified >= 0.2.15, M5GFX >= 0.2.21
//
// しくみ:
//   画面の半分の細かさの格子で「高さ」を計算 → 傾きから鏡に映る方向を求め、
//   景色テクスチャを引いて 2倍に拡大して描画。画面は8行ずつの帯に分けて、2つのコアで交互に描く。
//   高さは帯ごとにその場で作るので、大きな表は持たない。
//   高さ = ふくらんだ膜 + ねっとり変形 + 流れるぬめり + 生き物 + 腕(糸) + 浮かぶ粒
//   流れは 4マスおきの粗い格子で ψ を作り、差分をとって速さにします(1フレーム1回)。



// Arduino IDE は関数の宣言を自動でファイルの先頭近くに足すので、型の名前を先に宣言しておく
struct Col; struct Input; struct Scratch; struct UV; struct Lump; struct Blob; struct Mote; struct Bubble; struct Theme;

// ---- ぬるぬる具合の調整用 --------------------------------------------------
static constexpr float GOO_STICK   = 15.0f;   // 指に吸いつく高さ
static constexpr float GOO_GRIP    = 3.0f;    // 吸いつく速さ
static constexpr float GOO_SUCK    = 0.35f;   // 吸いついたまわりのくぼみ
static constexpr float GOO_DIFFUSE = 0.10f;   // 粘りの広がり(0.24以下。小さいほど跡が長く残る)
static constexpr float GOO_RELAX   = 0.09f;   // 元に戻る速さ(小さいほどゆっくり)
static constexpr float HEART_DOME  = 6.0f;    // 鼓動で膜がふくらむ量
static constexpr float HEART_GLOW  = 0.22f;   // 鼓動で生き物が強まる量
static constexpr float HEART_SWELL = 0.12f;   // 鼓動で生き物がふくらむ量
static constexpr float LUMP_FORCE  = 6.0f;    // 生き物の動く力
static constexpr float LUMP_DRAG   = 2.4f;    // 生き物の重たさ(大きいほど粘る)
static constexpr float SLIP        = 26.0f;   // 押したときにすり抜ける強さ
static constexpr float JIGGLE      = 0.35f;   // ぷるんと震える大きさ
static constexpr float TENDRIL_IN  = 0.8f;    // 腕が伸びる速さ
static constexpr float STRING_TIME = 1.0f;    // 離したとき糸を引く長さ(秒)
static constexpr float MARBLE      = 0.90f;   // 表面のぬめり(流れて巻き込まれる模様)の強さ
static constexpr float MARBLE_T    = 7.0f;    // ぬめりの模様が入れ替わる周期(秒。長いほど細く長く巻き込まれる)
static constexpr float MARBLE_FINE = 1.0f;    // ぬめりの模様の細かさ
static constexpr float FLOW        = 13.0f;   // 流れの速さ(おおよその速さ、マス/秒)
static constexpr float FLOW_SIZE   = 1.0f;    // 渦の大きさ(大きいほど大きな渦)
static constexpr float FLOW_CHANGE = 0.05f;   // 渦の形が移り変わる速さ
static constexpr float CARRY       = 0.55f;   // 生き物が流れに押される割合
static constexpr float STIR        = 0.70f;   // なぞったとき、指が流れを押す強さ
static constexpr float SPIN        = 2.2f;    // 押さえたままのとき、指のまわりに渦が巻く強さ
static constexpr float STIR_KEEP   = 0.35f;   // 指で起こした渦が消えていく速さ(小さいほど長く残る)
static constexpr int   NMOTE       = 26;      // 渦に乗って回る小さな粒の数
static constexpr float REFLECT     = 16.0f;   // 映り込みのゆがみ(大きいほどギラッと)
// ---------------------------------------------------------------------------

static constexpr int ENV   = 128;             // 景色テクスチャのサイズ
static constexpr int NLUMP = 4;               // 膜の下の「生き物」の数
static constexpr int NOISE = 64;              // ぬめり用ノイズのサイズ

// 画面に合わせて起動時に決まる値
static constexpr size_t nmOa0 = nmAlignTo(nmOv0, alignof(int)), nmOv1 = nmOa0 + sizeof(int); static int& W = *reinterpret_cast<int*>(NM_ARENA + nmOa0);
static constexpr size_t nmOa1 = nmAlignTo(nmOv1, alignof(int)), nmOv2 = nmOa1 + sizeof(int); static int& H = *reinterpret_cast<int*>(NM_ARENA + nmOa1);
static constexpr size_t nmOa2 = nmAlignTo(nmOv2, alignof(int)), nmOv3 = nmOa2 + sizeof(int); static int& CXN = *reinterpret_cast<int*>(NM_ARENA + nmOa2);
static constexpr size_t nmOa3 = nmAlignTo(nmOv3, alignof(int)), nmOv4 = nmOa3 + sizeof(int); static int& CYN = *reinterpret_cast<int*>(NM_ARENA + nmOa3);
static constexpr size_t nmOa4 = nmAlignTo(nmOv4, alignof(int)), nmOv5 = nmOa4 + sizeof(int); static int& GX = *reinterpret_cast<int*>(NM_ARENA + nmOa4);
static constexpr size_t nmOa5 = nmAlignTo(nmOv5, alignof(int)), nmOv6 = nmOa5 + sizeof(int); static int& GY = *reinterpret_cast<int*>(NM_ARENA + nmOa5);
static constexpr size_t nmOa6 = nmAlignTo(nmOv6, alignof(int)), nmOv7 = nmOa6 + sizeof(int); static int& GN = *reinterpret_cast<int*>(NM_ARENA + nmOa6);
static constexpr size_t nmOa7 = nmAlignTo(nmOv7, alignof(float)), nmOv8 = nmOa7 + sizeof(float); static float& MX = *reinterpret_cast<float*>(NM_ARENA + nmOa7);
static constexpr size_t nmOa8 = nmAlignTo(nmOv8, alignof(float)), nmOv9 = nmOa8 + sizeof(float); static float& MY = *reinterpret_cast<float*>(NM_ARENA + nmOa8);
static constexpr size_t nmOa9 = nmAlignTo(nmOv9, alignof(float)), nmOv10 = nmOa9 + sizeof(float); static float& RR = *reinterpret_cast<float*>(NM_ARENA + nmOa9);
static constexpr size_t nmOa10 = nmAlignTo(nmOv10, alignof(float)), nmOv11 = nmOa10 + sizeof(float); static float& RD = *reinterpret_cast<float*>(NM_ARENA + nmOa10);
static constexpr size_t nmOa11 = nmAlignTo(nmOv11, alignof(float)), nmOv12 = nmOa11 + sizeof(float); static float& SC = *reinterpret_cast<float*>(NM_ARENA + nmOa11);
static constexpr size_t nmOa12 = nmAlignTo(nmOv12, alignof(bool)), nmOv13 = nmOa12 + sizeof(bool); static bool& ROUND = *reinterpret_cast<bool*>(NM_ARENA + nmOa12);

struct UV { uint16_t u, v; };                 // 8.8 固定小数点

static uint16_t* env;          // 景色テクスチャ (内部RAM)
static int16_t*  gooA;         // ねっとりした変形(半分の細かさの格子。256 倍した整数で持つ。内部RAM)
static int16_t*  gooB;         // 作業用
static constexpr float GOO_Q = 256.0f, GOO_IQ = 1.0f / 256.0f;
static inline int16_t gooQ(float v) { v *= GOO_Q; v = v > 32000.0f ? 32000.0f : (v < -32000.0f ? -32000.0f : v); return (int16_t)(v + (v >= 0 ? 0.5f : -0.5f)); }
static uint8_t*  mask2;        // 膜のある場所=1(半分の細かさ)
static constexpr size_t nmOa13 = nmAlignTo(nmOv13, alignof(int)), nmOv14 = nmOa13 + sizeof(int); static int& G2X = *reinterpret_cast<int*>(NM_ARENA + nmOa13);
static constexpr size_t nmOa14 = nmAlignTo(nmOv14, alignof(int)), nmOv15 = nmOa14 + sizeof(int); static int& G2Y = *reinterpret_cast<int*>(NM_ARENA + nmOa14);
static constexpr size_t nmOa15 = nmAlignTo(nmOv15, alignof(int)), nmOv16 = nmOa15 + sizeof(int); static int& G2N = *reinterpret_cast<int*>(NM_ARENA + nmOa15);  // 半分の細かさの格子の大きさ
static float*    noiseT;       // ぬめり用ノイズ
static float*    psiS;         // 指で起こした流れ(4マスおきの ψ。ゆっくり消える)
static float*    psiT;         // このフレームの ψ(ノイズ + 指)
static float*    flowU;        // 流れ(4マスおき、マス/秒)
static float*    flowV;
static float*    flowRowU;     // 流れ(1行ぶん × コア2つ)
static float*    flowRowV;
static int16_t*  marA;         // 流れるぬめり(2マスおき。毎フレーム作り直す。16384 倍した整数。内部RAM)
// ぬめりの「値」ではなく「元の模様のどこから来たか(座標)」を流れで運ぶ。値を運ぶと、
// 補間のたびに少しずつぼやけて、10秒ほどで灰色一色になってしまうため。
// 座標はなめらかなので、補間してもほとんどぼやけません。
// 2組を半周期ずらして交互に作り直し、重ねて見せます(引き伸ばされすぎないように)。
static float*    crd[2][2];    // [組][x/y] 元の模様の座標
static float*    crdTmp[2][2]; // 書き込み用(書き終わったら入れ替える)
static constexpr size_t nmOa16 = nmAlignTo(nmOv16, 16), nmOv17 = nmOa16 + sizeof(float[2]); static float (&crdAge)[2] = *reinterpret_cast<float(*)[2]>(NM_ARENA + nmOa16);  // それぞれの組が作り直されてからの時間
static constexpr size_t nmOa17 = nmAlignTo(nmOv17, 16), nmOv18 = nmOa17 + sizeof(float[2][2]); static float (&crdOfs)[2][2] = *reinterpret_cast<float(*)[2][2]>(NM_ARENA + nmOa17);  // 作り直すたびに、元の模様の別の場所を使う
static constexpr size_t nmOa18 = nmAlignTo(nmOv18, alignof(int)), nmOv19 = nmOa18 + sizeof(int); static int& MGX = *reinterpret_cast<int*>(NM_ARENA + nmOa18);
static constexpr size_t nmOa19 = nmAlignTo(nmOv19, alignof(int)), nmOv20 = nmOa19 + sizeof(int); static int& MGY = *reinterpret_cast<int*>(NM_ARENA + nmOa19);
static const float FLOW_LEVELS[3] = {0.45f, 1.0f, 1.9f};
static int flowLevel = 1;
struct Mote { float x, y, age, life; };
static constexpr size_t nmOa20 = nmAlignTo(nmOv20, 16), nmOv21 = nmOa20 + sizeof(Mote[NMOTE]); static Mote (&motes)[NMOTE] = *reinterpret_cast<Mote(*)[NMOTE]>(NM_ARENA + nmOa20);
static constexpr size_t nmOa21 = nmAlignTo(nmOv21, alignof(float)), nmOv22 = nmOa21 + sizeof(float); static float& stillT = *reinterpret_cast<float*>(NM_ARENA + nmOa21);  // 指が止まっている時間
static constexpr size_t nmOa22 = nmAlignTo(nmOv22, alignof(int)), nmOv23 = nmOa22 + sizeof(int); static int& gooFlip = *reinterpret_cast<int*>(NM_ARENA + nmOa22);
static constexpr size_t nmOa23 = nmAlignTo(nmOv23, alignof(int)), nmOv24 = nmOa23 + sizeof(int); static int& CGX = *reinterpret_cast<int*>(NM_ARENA + nmOa23);
static constexpr size_t nmOa24 = nmAlignTo(nmOv24, alignof(int)), nmOv25 = nmOa24 + sizeof(int); static int& CGY = *reinterpret_cast<int*>(NM_ARENA + nmOa24);
static int16_t*  rowS;         // 高さを計算する範囲(行ごと)
static int16_t*  rowE;
static int16_t*  gradS;        // 傾きを計算する範囲(行ごと)
static int16_t*  gradE;
static constexpr size_t nmOa25 = nmAlignTo(nmOv25, 16), nmOv26 = nmOa25 + sizeof(float[257]); static float (&expLut)[257] = *reinterpret_cast<float(*)[257]>(NM_ARENA + nmOa25);

struct Lump { float x, y, vx, vy, baseAmp, sig, phase, theta, jig, jigPh; };
static constexpr size_t nmOa26 = nmAlignTo(nmOv26, 16), nmOv27 = nmOa26 + sizeof(Lump[NLUMP]); static Lump (&lumps)[NLUMP] = *reinterpret_cast<Lump(*)[NLUMP]>(NM_ARENA + nmOa26);

struct Input { bool touching; bool tapped; float tx, ty, px, py; float dt; };
static constexpr size_t nmOa27 = nmAlignTo(nmOv27, alignof(Input)), nmOv28 = nmOa27 + sizeof(Input); static Input& simIn = *reinterpret_cast<Input*>(NM_ARENA + nmOa27);

static SemaphoreHandle_t semGo, semDone;

// 高さに足すふくらみ(1フレームぶんの一覧。帯を描くときに、その場で足す)
struct Blob { float cx, cy, amp, ia, ip, dirX, dirY; int16_t x0, x1, y0, y1; bool str; };
static constexpr int MAXBLOB = 72;
static constexpr size_t nmOa28 = nmAlignTo(nmOv28, 16), nmOv29 = nmOa28 + sizeof(Blob[MAXBLOB]); static Blob (&blobs)[MAXBLOB] = *reinterpret_cast<Blob(*)[MAXBLOB]>(NM_ARENA + nmOa28);  // 細いふくらみ(帯で1マスずつ足す)
static constexpr size_t nmOa29 = nmAlignTo(nmOv29, alignof(int)), nmOv30 = nmOa29 + sizeof(int); static int& nBlob = *reinterpret_cast<int*>(NM_ARENA + nmOa29);
static constexpr size_t nmOa30 = nmAlignTo(nmOv30, 16), nmOv31 = nmOa30 + sizeof(Blob[MAXBLOB]); static Blob (&bigBlobs)[MAXBLOB] = *reinterpret_cast<Blob(*)[MAXBLOB]>(NM_ARENA + nmOa30);  // 太いふくらみ(半分の細かさの高さに足す)
static constexpr size_t nmOa31 = nmAlignTo(nmOv31, alignof(int)), nmOv32 = nmOa31 + sizeof(int); static int& nBig = *reinterpret_cast<int*>(NM_ARENA + nmOa31);
static constexpr float SMALL_SIG = 4.0f;    // これより細い(マス)ふくらみは、1マスずつ足す
static int16_t* H2;                         // 高さ(半分の細かさ。256 倍した整数。2つのコアで半分ずつ作る)
static constexpr float H2_Q = 256.0f, H2_IQ = 1.0f / 256.0f;
static volatile int32_t phaseCnt[2] = {-1, -1};   // 2つのコアの足並みそろえ
static constexpr size_t nmOa32 = nmAlignTo(nmOv32, alignof(int32_t)), nmOv33 = nmOa32 + sizeof(int32_t); static int32_t& phaseBase = *reinterpret_cast<int32_t*>(NM_ARENA + nmOa32);
static constexpr int32_t PHASES = 8;           // 1フレームで使う「足並みそろえ」の回数の上限
static constexpr size_t nmOa33 = nmAlignTo(nmOv33, alignof(int)), nmOv34 = nmOa33 + sizeof(int); static int& gooPasses = *reinterpret_cast<int*>(NM_ARENA + nmOa33);  // このフレームに2つのコアで進める「指の跡を広げる」回数(0 = 休み)
static constexpr size_t nmOa34 = nmAlignTo(nmOv34, 16), nmOv35 = nmOa34 + sizeof(int32_t[2]); static int32_t (&gooK0)[2] = *reinterpret_cast<int32_t(*)[2]>(NM_ARENA + nmOa34);
static constexpr size_t nmOa35 = nmAlignTo(nmOv35, 16), nmOv36 = nmOa35 + sizeof(int32_t[2]); static int32_t (&gooKd)[2] = *reinterpret_cast<int32_t(*)[2]>(NM_ARENA + nmOa35);  // その係数(14bit 固定小数点)
static constexpr size_t nmOa36 = nmAlignTo(nmOv36, alignof(bool)), nmOv37 = nmOa36 + sizeof(bool); static bool& gooAdvNow = *reinterpret_cast<bool*>(NM_ARENA + nmOa36);  // このフレームに、指の跡を流れに乗せるか
static constexpr size_t nmOa37 = nmAlignTo(nmOv37, alignof(float)), nmOv38 = nmOa37 + sizeof(float); static float& gooAdvDt = *reinterpret_cast<float*>(NM_ARENA + nmOa37);
static uint32_t prepUs[2] = {0, 0};
static constexpr size_t nmOa38 = nmAlignTo(nmOv38, alignof(float)), nmOv39 = nmOa38 + sizeof(float); static float& domeDepth = *reinterpret_cast<float*>(NM_ARENA + nmOa38);
static constexpr size_t nmOa39 = nmAlignTo(nmOv39, alignof(float)), nmOv40 = nmOa39 + sizeof(float); static float& wetK = *reinterpret_cast<float*>(NM_ARENA + nmOa39);
static constexpr size_t nmOa40 = nmAlignTo(nmOv40, alignof(float)), nmOv41 = nmOa40 + sizeof(float); static float& marK = *reinterpret_cast<float*>(NM_ARENA + nmOa40);  // このフレームの、ふくらみの深さ、ぬれたテカり、ぬめりの強さ
static constexpr float MAR_Q = 16384.0f, MAR_IQ = 1.0f / 16384.0f;

// 帯(8行ずつ)
static constexpr int BAND = 8, NSLOT = 4;
static int SPLIT = 2;
static constexpr size_t nmOa41 = nmAlignTo(nmOv41, alignof(int)), nmOv42 = nmOa41 + sizeof(int); static int& NB = *reinterpret_cast<int*>(NM_ARENA + nmOa41);
static constexpr size_t nmOa42 = nmAlignTo(nmOv42, alignof(bool)), nmOv43 = nmOa42 + sizeof(bool); static bool& fbMode = *reinterpret_cast<bool*>(NM_ARENA + nmOa42);
static constexpr size_t nmOa43 = nmAlignTo(nmOv43, alignof(bool)), nmOv44 = nmOa43 + sizeof(bool); static bool& directSend = *reinterpret_cast<bool*>(NM_ARENA + nmOa43);  // StopWatch で、画面へ直接送っているか

// ---- StopWatch では、描いた帯を画面へ「直接」送る(M5GFX の PSRAM の置き場を通さない)
//   置き場を通すと「置き場に書く → 読み直す → 送り終わるまで待つ」で、1コマに約20ms かかっていた。
//   直接なら DMA で送るので、送っているあいだも次の帯を計算できる。画面が乱れるときは false に
static constexpr bool DIRECT_SEND = true;
#if defined(ESP_PLATFORM) && defined(LGFX_USE_QSPI) && __has_include(<lgfx/v1/panel/Panel_AMOLED.hpp>)
#define NL_DIRECT_AMOLED 1
// 置き場(Panel_AMOLED_Framebuffer)が中に持っている、本物の画面(Panel_AMOLED)を取り出す
struct NlFbPeek : public lgfx::Panel_AMOLED_Framebuffer {
  static lgfx::Panel_AMOLED* inner(lgfx::Panel_Device* p) {
    return static_cast<lgfx::Panel_AMOLED_Framebuffer*>(p)->*(&NlFbPeek::_panel);
  }
};
#endif
static uint16_t* slotBuf[NSLOT];
static constexpr size_t nmOa44 = nmAlignTo(nmOv44, alignof(int16_t*)), nmOv45 = nmOa44 + sizeof(int16_t*); static int16_t*& bandC0 = *reinterpret_cast<int16_t**>(NM_ARENA + nmOa44);
static constexpr size_t nmOa45 = nmAlignTo(nmOv45, alignof(int16_t*)), nmOv46 = nmOa45 + sizeof(int16_t*); static int16_t*& bandC1 = *reinterpret_cast<int16_t**>(NM_ARENA + nmOa45);  // 帯の、丸にかかる範囲(2ピクセル単位)
static constexpr size_t nmOa46 = nmAlignTo(nmOv46, alignof(int16_t*)), nmOv47 = nmOa46 + sizeof(int16_t*); static int16_t*& rowC0 = *reinterpret_cast<int16_t**>(NM_ARENA + nmOa46);
static constexpr size_t nmOa47 = nmAlignTo(nmOv47, alignof(int16_t*)), nmOv48 = nmOa47 + sizeof(int16_t*); static int16_t*& rowC1 = *reinterpret_cast<int16_t**>(NM_ARENA + nmOa47);  // 2行ずつの、丸にかかる範囲(前の版と同じ。外側は黒のまま)
static constexpr size_t nmOa48 = nmAlignTo(nmOv48, alignof(int16_t*)), nmOv49 = nmOa48 + sizeof(int16_t*); static int16_t*& bandHX0 = *reinterpret_cast<int16_t**>(NM_ARENA + nmOa48);
static constexpr size_t nmOa49 = nmAlignTo(nmOv49, alignof(int16_t*)), nmOv50 = nmOa49 + sizeof(int16_t*); static int16_t*& bandHX1 = *reinterpret_cast<int16_t**>(NM_ARENA + nmOa49);  // 帯で高さを計算する範囲(格子)
static volatile int readyW = -1, doneUpTo = -1;
static uint32_t busyUs[2] = {0, 0}, sendUs = 0;
struct Scratch { float* h; UV* uv; float* row; };   // 帯ごとの作業場(コアごとに1つ)
static constexpr size_t nmOa50 = nmAlignTo(nmOv50, 16), nmOv51 = nmOa50 + sizeof(Scratch[2]); static Scratch (&scr)[2] = *reinterpret_cast<Scratch(*)[2]>(NM_ARENA + nmOa50);

// 生き物の状態
static constexpr size_t nmOa51 = nmAlignTo(nmOv51, alignof(float)), nmOv52 = nmOa51 + sizeof(float); static float& simT = *reinterpret_cast<float*>(NM_ARENA + nmOa51);
static constexpr size_t nmOa52 = nmAlignTo(nmOv52, alignof(float)), nmOv53 = nmOa52 + sizeof(float); static float& activity = *reinterpret_cast<float*>(NM_ARENA + nmOa52);
static constexpr size_t nmOa53 = nmAlignTo(nmOv53, alignof(float)), nmOv54 = nmOa53 + sizeof(float); static float& fear = *reinterpret_cast<float*>(NM_ARENA + nmOa53);
static constexpr size_t nmOa54 = nmAlignTo(nmOv54, alignof(float)), nmOv55 = nmOa54 + sizeof(float); static float& wake = *reinterpret_cast<float*>(NM_ARENA + nmOa54);
static constexpr size_t nmOa55 = nmAlignTo(nmOv55, alignof(float)), nmOv56 = nmOa55 + sizeof(float); static float& breathPhase = *reinterpret_cast<float*>(NM_ARENA + nmOa55);
static constexpr size_t nmOa56 = nmAlignTo(nmOv56, alignof(float)), nmOv57 = nmOa56 + sizeof(float); static float& hbPhase = *reinterpret_cast<float*>(NM_ARENA + nmOa56);
static constexpr size_t nmOa57 = nmAlignTo(nmOv57, alignof(float)), nmOv58 = nmOa57 + sizeof(float); static float& gooQuiet = *reinterpret_cast<float*>(NM_ARENA + nmOa57);
static constexpr size_t nmOa58 = nmAlignTo(nmOv58, alignof(bool)), nmOv59 = nmOa58 + sizeof(bool); static bool& gooCleared = *reinterpret_cast<bool*>(NM_ARENA + nmOa58);

// 腕(触手)と糸
enum TState { T_NONE, T_REACH, T_STRING, T_SNAP };
static TState tState = T_NONE;
static int tLump = -1;
static float strDur = 1;
static constexpr size_t nmOa59 = nmAlignTo(nmOv59, alignof(float)), nmOv60 = nmOa59 + sizeof(float); static float& tS = *reinterpret_cast<float*>(NM_ARENA + nmOa59);
static constexpr size_t nmOa60 = nmAlignTo(nmOv60, alignof(float)), nmOv61 = nmOa60 + sizeof(float); static float& tV = *reinterpret_cast<float*>(NM_ARENA + nmOa60);
static constexpr size_t nmOa61 = nmAlignTo(nmOv61, alignof(float)), nmOv62 = nmOa61 + sizeof(float); static float& tTx = *reinterpret_cast<float*>(NM_ARENA + nmOa61);
static constexpr size_t nmOa62 = nmAlignTo(nmOv62, alignof(float)), nmOv63 = nmOa62 + sizeof(float); static float& tTy = *reinterpret_cast<float*>(NM_ARENA + nmOa62);
static constexpr size_t nmOa63 = nmAlignTo(nmOv63, alignof(float)), nmOv64 = nmOa63 + sizeof(float); static float& strT = *reinterpret_cast<float*>(NM_ARENA + nmOa63);

// 表面にぬるっと浮かぶもの
struct Bubble { float x, y, t, T, amp, sig; bool on; };
static Bubble bubble = {0, 0, 0, 0, 0, 0, false};
static float bubbleTimer = 4;

static constexpr size_t nmOa64 = nmAlignTo(nmOv64, alignof(int)), nmOv65 = nmOa64 + sizeof(int); static int& theme = *reinterpret_cast<int*>(NM_ARENA + nmOa64);
static const uint8_t brightSteps[] = {60, 120, 190, 255};
static int brightIdx = 2;

// ---------------------------------------------------------------- 景色
struct Col { float r, g, b; };
struct Theme { Col skyTop, horizon, ground, band, sun, glow; };
static const Theme THEMES[] = {
  // 銀 (null²っぽい昼の鏡面)
  {{0.55f,0.72f,0.95f},{0.95f,0.97f,1.00f},{0.07f,0.08f,0.10f},{0.38f,0.40f,0.44f},{1.0f,1.0f,1.0f},{0.85f,0.92f,1.0f}},
  // 夕暮れ
  {{0.16f,0.10f,0.38f},{1.00f,0.55f,0.30f},{0.05f,0.03f,0.08f},{0.42f,0.18f,0.26f},{1.0f,0.85f,0.5f},{1.0f,0.55f,0.30f}},
  // ネオン
  {{0.01f,0.01f,0.04f},{0.12f,0.05f,0.28f},{0.00f,0.00f,0.02f},{0.00f,0.75f,0.90f},{1.0f,0.2f,0.8f},{0.30f,0.90f,1.0f}},
  // 金
  {{0.95f,0.82f,0.50f},{1.00f,0.97f,0.82f},{0.16f,0.09f,0.02f},{0.62f,0.42f,0.12f},{1.0f,1.0f,0.9f},{1.0f,0.90f,0.60f}},
};
static constexpr int NTHEME = sizeof(THEMES) / sizeof(THEMES[0]);

static inline float clamp01(float v) { return v < 0 ? 0 : (v > 1 ? 1 : v); }
static inline Col lerpC(Col a, Col b, float t) {
  return {a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t};
}

static void buildEnv(int th) {
  const Theme& T = THEMES[th];
  for (int j = 0; j < ENV; ++j) {
    for (int i = 0; i < ENV; ++i) {
      float x = (i - 63.5f) / 64.0f;
      float y = (j - 63.5f) / 64.0f;               // 下向きが正
      float hz = 0.08f + 0.06f * sinf(x * 2.2f);   // ゆるく波打つ地平線
      Col c;
      if (y < hz) {                                // 空
        float s = clamp01((hz - y) / (hz + 1.0f));
        c = lerpC(T.horizon, T.skyTop, powf(s, 0.6f));
        float dx = x - 0.35f, dy = y + 0.55f;
        float sun = expf(-(dx * dx + dy * dy) / 0.02f);
        c.r += T.sun.r * sun; c.g += T.sun.g * sun; c.b += T.sun.b * sun;
      } else {                                     // 地面と映り込む構造物
        float gnd = clamp01((y - hz) / (1.0f - hz));
        float bands = 0.5f + 0.5f * sinf(x * 9.0f + sinf(x * 3.0f) * 2.0f);
        bands = powf(bands, 6.0f) * (1.0f - gnd);
        Col base = lerpC(T.band, T.ground, powf(gnd, 0.45f));
        c = lerpC(base, T.band, bands);
      }
      float g = expf(-(y - hz) * (y - hz) / 0.006f);   // 地平線の光
      c.r += T.glow.r * g * 0.8f; c.g += T.glow.g * g * 0.8f; c.b += T.glow.b * g * 0.8f;

      // ぬれたテカり: ふくらみの肩に、小さく鋭いハイライトが乗るように
      float s1x = x + 0.13f, s1y = y + 0.17f, s2x = x + 0.27f, s2y = y + 0.36f;
      float glint = 1.4f * expf(-(s1x * s1x + s1y * s1y) / (2 * 0.04f * 0.04f))
                  + 0.6f * expf(-(s2x * s2x + s2y * s2y) / (2 * 0.07f * 0.07f));
      c.r += glint; c.g += glint; c.b += glint;

      uint16_t col = M5.Display.color565(clamp01(c.r) * 255, clamp01(c.g) * 255, clamp01(c.b) * 255);
      env[j * ENV + i] = __builtin_bswap16(col);   // 色がおかしい場合はこの bswap を外す
    }
  }
}

// ---------------------------------------------------------------- 道具
static inline float frand() { return (esp_random() & 0xFFFF) / 65535.0f; }
static inline float smooth3(float t) { t = clamp01(t); return t * t * (3 - 2 * t); }

static void buildNoise() {
  float* tmp = (float*)malloc(NOISE * NOISE * sizeof(float));
  for (int i = 0; i < NOISE * NOISE; ++i) noiseT[i] = frand() * 2 - 1;
  for (int pass = 0; pass < 4; ++pass) {            // ぼかして、なめらかなうねりに
    for (int y = 0; y < NOISE; ++y)
      for (int x = 0; x < NOISE; ++x) {
        float s = 0;
        for (int d = -2; d <= 2; ++d) s += noiseT[y * NOISE + ((x + d) & (NOISE - 1))];
        tmp[y * NOISE + x] = s / 5;
      }
    for (int y = 0; y < NOISE; ++y)
      for (int x = 0; x < NOISE; ++x) {
        float s = 0;
        for (int d = -2; d <= 2; ++d) s += tmp[((y + d) & (NOISE - 1)) * NOISE + x];
        noiseT[y * NOISE + x] = s / 5;
      }
  }
  float mx = 0;
  for (int i = 0; i < NOISE * NOISE; ++i) mx = fmaxf(mx, fabsf(noiseT[i]));
  for (int i = 0; i < NOISE * NOISE; ++i) noiseT[i] /= mx;
  free(tmp);
}

static inline float noiseAt(float x, float y) {
  // floorf は関数呼び出しで重いので、整数への切り捨て + 負のときの補正で床関数にする(値は同じ)
  int ix = (int)x, iy = (int)y;
  if ((float)ix > x) --ix;
  if ((float)iy > y) --iy;
  float fx = x - (float)ix, fy = y - (float)iy;
  int x0 = ix & (NOISE - 1), y0 = iy & (NOISE - 1);
  int x1 = (x0 + 1) & (NOISE - 1),  y1 = (y0 + 1) & (NOISE - 1);
  float a = noiseT[y0 * NOISE + x0], b = noiseT[y0 * NOISE + x1];
  float c = noiseT[y1 * NOISE + x0], d = noiseT[y1 * NOISE + x1];
  return (a + (b - a) * fx) + ((c + (d - c) * fx) - (a + (b - a) * fx)) * fy;
}

// 丸いふくらみを、ねっとり変形(半分の細かさ)に直接足す。cx, cy, sig は高さの格子の単位
static void addGooBlob(float cx, float cy, float amp, float sig) {
  cx *= 0.5f; cy *= 0.5f; sig *= 0.5f;
  if (sig < 1.0f) sig = 1.0f;
  float inv = 32.0f / (sig * sig);
  float rad = sig * 2.4f;
  int x0 = max(0, (int)(cx - rad)), x1 = min(G2X - 1, (int)(cx + rad));
  int y0 = max(0, (int)(cy - rad)), y1 = min(G2Y - 1, (int)(cy + rad));
  for (int y = y0; y <= y1; ++y) {
    float dy2 = (y - cy) * (y - cy);
    int16_t* row = gooA + y * G2X;
    for (int x = x0; x <= x1; ++x) {
      float dx = x - cx;
      float q = (dx * dx + dy2) * inv;
      if (q < 255.0f) { int i = (int)q; row[x] = gooQ(row[x] * GOO_IQ + amp * (expLut[i] + (expLut[i + 1] - expLut[i]) * (q - i))); }
    }
  }
}

// 丸いふくらみを、高さの一覧に積む
static void addBlob(float cx, float cy, float amp, float sig) {
  if (sig < 1.0f) sig = 1.0f;
  const bool big = sig >= SMALL_SIG;
  int& n = big ? nBig : nBlob;
  if (n >= MAXBLOB) return;
  float rad = sig * 2.4f;
  Blob& b = (big ? bigBlobs : blobs)[n];
  b.cx = cx; b.cy = cy; b.amp = amp; b.ia = 32.0f / (sig * sig); b.ip = 0; b.dirX = 1; b.dirY = 0; b.str = false;
  b.x0 = max(0, (int)(cx - rad)); b.x1 = min(GX - 1, (int)(cx + rad));
  b.y0 = max(0, (int)(cy - rad)); b.y1 = min(GY - 1, (int)(cy + rad));
  if (b.x0 <= b.x1 && b.y0 <= b.y1) ++n;
}

// 向きをもって伸びた(縮んだ)ふくらみを、高さの一覧に積む
static void addStretchedBlob(float cx, float cy, float amp, float sa, float sp, float dirX, float dirY) {
  const bool big = fminf(sa, sp) >= SMALL_SIG;
  int& n = big ? nBig : nBlob;
  if (n >= MAXBLOB) return;
  float rad = fmaxf(sa, sp) * 2.4f;
  Blob& b = (big ? bigBlobs : blobs)[n];
  b.cx = cx; b.cy = cy; b.amp = amp; b.ia = 32.0f / (sa * sa); b.ip = 32.0f / (sp * sp); b.dirX = dirX; b.dirY = dirY; b.str = true;
  b.x0 = max(0, (int)(cx - rad)); b.x1 = min(GX - 1, (int)(cx + rad));
  b.y0 = max(0, (int)(cy - rad)); b.y1 = min(GY - 1, (int)(cy + rad));
  if (b.x0 <= b.x1 && b.y0 <= b.y1) ++n;
}

static inline float sampleGoo(const int16_t* g, float x, float y) {
  x = x < 0 ? 0 : (x > G2X - 1.001f ? G2X - 1.001f : x);
  y = y < 0 ? 0 : (y > G2Y - 1.001f ? G2Y - 1.001f : y);
  int x0 = (int)x, y0 = (int)y;
  float fx = x - x0, fy = y - y0;
  const int16_t* p = g + y0 * G2X + x0;
  return ((p[0] * (1 - fx) + p[1] * fx) * (1 - fy) + (p[G2X] * (1 - fx) + p[G2X + 1] * fx) * fy) * GOO_IQ;
}

// 指で膜を引っぱる(指のまわりの膜を、指の動きにあわせて引きずる)
static void dragGoo(float px, float py, float qx, float qy) {
  px *= 0.5f; py *= 0.5f; qx *= 0.5f; qy *= 0.5f;   // 半分の細かさの格子で
  float mx = px - qx, my = py - qy;
  float m2 = mx * mx + my * my;
  float jump = 15.0f * SC;
  if (m2 < 0.000625f || m2 > jump * jump) return;  // 動いていない or 指が飛んだ
  int rad = (int)(9 * SC);
  float s2 = 2.0f * (4 * SC) * (4 * SC);
  int x0 = max(1, (int)px - rad), x1 = min(G2X - 2, (int)px + rad);
  int y0 = max(1, (int)py - rad), y1 = min(G2Y - 2, (int)py + rad);
  if (x1 < x0 || y1 < y0) return;
  for (int y = y0 - 1; y <= y1 + 1; ++y)            // 作業用に範囲だけ写す
    memcpy(gooB + y * G2X + x0 - 1, gooA + y * G2X + x0 - 1, (x1 - x0 + 3) * sizeof(int16_t));
  for (int y = y0; y <= y1; ++y)
    for (int x = x0; x <= x1; ++x) {
      float dx = x - px, dy = y - py;
      float w = expf(-(dx * dx + dy * dy) / s2) * 0.95f;
      if (w < 0.01f) continue;
      float sx = x - w * mx, sy = y - w * my;
      if (sx < x0 - 1 || sx >= x1 || sy < y0 - 1 || sy >= y1) continue;
      gooA[y * G2X + x] = gooQ(sampleGoo(gooB, sx, sy));
    }
}

static inline float softPulse(float x) { return expf(-(x * x) / (2 * 0.07f * 0.07f)); }

// 映り込みのずれが、景色の絵の端に張りつかないようにする。
// ふつうの傾きはそのまま(6割まで)、急すぎるところだけ、なめらかに端へ寄せる。
// 張りつくと、そこだけ端の色がべったり出て、四角いまだらに見えてしまうため
static inline float softEdge(float o) {
  const float M = 63.0f * 256.0f, knee = 0.6f * M, room = M - knee;
  float a = fabsf(o);
  if (a <= knee) return o;
  float t = (a - knee) / room;
  float r = knee + room * t / (1.0f + t);
  return o < 0 ? -r : r;
}

// ---------------------------------------------------------------- カールノイズの流れ
// 格子(マス)の座標 (gx, gy) での流れの速さ(マス/秒)。4マスおきの値を補間します
static inline void flowAt(float gx, float gy, float& u, float& v) {
  float x = gx * 0.25f, y = gy * 0.25f;
  x = x < 0 ? 0 : (x > CGX - 1.001f ? CGX - 1.001f : x);
  y = y < 0 ? 0 : (y > CGY - 1.001f ? CGY - 1.001f : y);
  int x0 = (int)x, y0 = (int)y;
  float fx = x - x0, fy = y - y0;
  int i = y0 * CGX + x0;
  float a = flowU[i] + (flowU[i + 1] - flowU[i]) * fx, b = flowU[i + CGX] + (flowU[i + CGX + 1] - flowU[i + CGX]) * fx;
  u = a + (b - a) * fy;
  a = flowV[i] + (flowV[i + 1] - flowV[i]) * fx; b = flowV[i + CGX] + (flowV[i + CGX + 1] - flowV[i + CGX]) * fx;
  v = a + (b - a) * fy;
}

// 画面のふちに近づくほど 1 → 0。ψ にこれを掛けると、ふちが「流線」になって流れが漏れない
static inline float rimFade(float x, float y) {
  float r;
  if (ROUND) { float dx = (x - MX) / RR, dy = (y - MY) / RR; r = sqrtf(dx * dx + dy * dy); }
  else       { r = fmaxf(fabsf(x - MX) / MX, fabsf(y - MY) / MY); }
  return 1.0f - smooth3((r - 0.60f) / 0.42f);    // 急に落とすと、ふちに沿って速い流れができてしまう
}

// ψ = なめらかなノイズ(ゆっくり形を変える) + 指で起こした流れ。その傾きを90度回して流れにする
static void buildFlow(const Input& in, float dt) {
  const float lvl = FLOW_LEVELS[flowLevel];
  const float s1 = 0.055f / (SC * FLOW_SIZE), s2 = s1 * 2.3f;   // 渦の大きさ(画面に数個おさまるくらい)
  // 流れの速さ ≒ 振幅 × 周波数 × 0.33(このノイズで実測)。ここから、狙った速さになる振幅を決める
  const float amp = FLOW * SC * lvl / (s1 * 0.33f);
  const float t = simT * FLOW_CHANGE * 20.0f;

  // ---- 指: なぞる → 指の向きに押す(左右に渦が2つできる)/ 止めて押さえる → 渦を巻く
  float fvx = 0, fvy = 0;
  if (in.touching && !in.tapped && dt > 0.001f) {
    fvx = (in.tx - in.px) / dt; fvy = (in.ty - in.py) / dt;
    float sp = sqrtf(fvx * fvx + fvy * fvy), lim = 90.0f * SC;
    if (sp > lim) { fvx *= lim / sp; fvy *= lim / sp; }
    stillT = (sp < 6.0f * SC) ? stillT + dt : 0.0f;
  } else if (!in.touching) stillT = 0;
  const float keep = expf(-dt * STIR_KEEP);
  for (int i = 0; i < CGX * CGY; ++i) psiS[i] *= keep;
  if (in.touching) {
    const float sig = 12.0f * SC, i2 = 1.0f / (2 * sig * sig);
    const float grip = fminf(1.0f, dt * 6.0f);
    const float spin = SPIN * sig * sig * smooth3((stillT - 0.35f) / 0.8f);  // 押さえて少したってから巻く
    int c0x = max(0, (int)((in.tx - 3 * sig) * 0.25f)), c1x = min(CGX - 1, (int)((in.tx + 3 * sig) * 0.25f) + 1);
    int c0y = max(0, (int)((in.ty - 3 * sig) * 0.25f)), c1y = min(CGY - 1, (int)((in.ty + 3 * sig) * 0.25f) + 1);
    for (int cy = c0y; cy <= c1y; ++cy)
      for (int cx = c0x; cx <= c1x; ++cx) {
        float dx = cx * 4.0f - in.tx, dy = cy * 4.0f - in.ty;
        float q = (dx * dx + dy * dy) * i2 * 32.0f;
        if (q >= 255.0f) continue;
        int k = (int)q; float g = expLut[k] + (expLut[k + 1] - expLut[k]) * (q - k);
        float& p = psiS[cy * CGX + cx];
        float target = STIR * (fvx * dy - fvy * dx) * g;           // 指の動く向きに押す流れ
        p += (target - p) * grip * g;
        p += spin * g * dt;                                          // 渦
        float lim = 60.0f * sig * SC;
        p = p > lim ? lim : (p < -lim ? -lim : p);
      }
  }

  for (int cy = 0; cy < CGY; ++cy)
    for (int cx = 0; cx < CGX; ++cx) {
      float x = cx * 4.0f, y = cy * 4.0f;
      float n = noiseAt(x * s1 + t * 0.7f, y * s1 - t * 0.4f) + 0.45f * noiseAt(x * s2 - t * 0.9f + 31.0f, y * s2 + t * 0.6f);
      psiT[cy * CGX + cx] = (amp * n + psiS[cy * CGX + cx]) * rimFade(x, y);
    }
  // 流れ = (∂ψ/∂y, -∂ψ/∂x)。4マスおきなので差は 8マスぶん
  for (int cy = 0; cy < CGY; ++cy)
    for (int cx = 0; cx < CGX; ++cx) {
      int i = cy * CGX + cx;
      int xm = cx > 0 ? i - 1 : i, xp = cx < CGX - 1 ? i + 1 : i;
      int ym = cy > 0 ? i - CGX : i, yp = cy < CGY - 1 ? i + CGX : i;
      float wx = (cx > 0 && cx < CGX - 1) ? 0.125f : 0.25f;
      float wy = (cy > 0 && cy < CGY - 1) ? 0.125f : 0.25f;
      flowU[i] =  (psiT[yp] - psiT[ym]) * wy;
      flowV[i] = -(psiT[xp] - psiT[xm]) * wx;
    }
}

// ある組の座標を「今いる場所 = 元の模様の場所」に戻す(読み用・書き用の両方)
static void resetCoords(int k) {
  const float ms = 0.50f * MARBLE_FINE / SC;
  crdOfs[k][0] = frand() * 64.0f; crdOfs[k][1] = frand() * 64.0f;
  for (int my = 0; my < MGY; ++my)
    for (int mx = 0; mx < MGX; ++mx) {
      const int i = my * MGX + mx;
      crd[k][0][i] = crdTmp[k][0][i] = mx * 2.0f * ms + crdOfs[k][0];
      crd[k][1][i] = crdTmp[k][1][i] = my * 2.0f * ms + crdOfs[k][1];
    }
}

// 流れるぬめり: 座標を流れに沿って運び(ひとつ前の位置から読んでくる)、元の模様を引く
static constexpr size_t nmOa65 = nmAlignTo(nmOv65, alignof(float)), nmOv66 = nmOa65 + sizeof(float); static float& marW0 = *reinterpret_cast<float*>(NM_ARENA + nmOa65);
static constexpr size_t nmOa66 = nmAlignTo(nmOv66, alignof(float)), nmOv67 = nmOa66 + sizeof(float); static float& marW1 = *reinterpret_cast<float*>(NM_ARENA + nmOa66);
static constexpr size_t nmOa67 = nmAlignTo(nmOv67, alignof(float)), nmOv68 = nmOa67 + sizeof(float); static float& marDt = *reinterpret_cast<float*>(NM_ARENA + nmOa67);
static constexpr size_t nmOa68 = nmAlignTo(nmOv68, alignof(int)), nmOv69 = nmOa68 + sizeof(int); static int& marPh = *reinterpret_cast<int*>(NM_ARENA + nmOa68);  // このコマで運ぶ行(0: 偶数の行 / 1: 奇数の行)。座標の入れかえは、両方そろってから
// 準備(1つのコアで): 古くなった組を作り直し、2組の重みを決める
static void marblePrep(float dt) {
  for (int k = 0; k < 2; ++k) {
    crdAge[k] += dt;
    // 作り直すのは、偶数の行から始めるコマだけ(読む座標を、2コマのあいだ変えないため)
    if (marPh == 0 && crdAge[k] >= MARBLE_T) { crdAge[k] -= MARBLE_T; resetCoords(k); }
  }
  // 2組の重み: 作り直した瞬間は 0、半周期で 1(三角形)。2組の和はいつも 1
  marW0 = 1.0f - fabsf(2.0f * fminf(crdAge[0], MARBLE_T) / MARBLE_T - 1.0f); marW1 = 1.0f - marW0;
  marDt = dt * 2.0f;                                // それぞれの行は2コマに1回なので、2コマぶん進める
}

// 行 myA..myB-1 を運ぶ(2つのコアで半分ずつ)
static void marbleRows(int myA, int myB) {
  const float w0 = marW0, w1 = marW1, dt = marDt;
  const float *ax0 = crd[0][0], *ay0 = crd[0][1], *ax1 = crd[1][0], *ay1 = crd[1][1];
  float *bx0 = crdTmp[0][0], *by0 = crdTmp[0][1], *bx1 = crdTmp[1][0], *by1 = crdTmp[1][1];
  for (int my = max(1, myA); my < min(MGY - 1, myB); ++my) {
    if ((my & 1) != marPh) continue;                // このコマの番ではない行
    const int gy = min(GY - 1, my * 2);
    int16_t* mrow = marA + my * MGX;
    if (rowS[gy] > rowE[gy]) { memset(mrow, 0, MGX * sizeof(int16_t)); continue; }   // 画面の外
    const int mx0 = max(1, rowS[gy] / 2 - 2), mx1 = min(MGX - 2, rowE[gy] / 2 + 2);
    for (int mx = 1; mx < mx0; ++mx) mrow[mx] = 0;
    for (int mx = mx1 + 1; mx < MGX - 1; ++mx) mrow[mx] = 0;
    for (int mx = mx0; mx <= mx1; ++mx) {
      const float gx = mx * 2.0f, gyf = my * 2.0f;
      float u, v;
      flowAt(gx, gyf, u, v);                                        // 中点法: 半分戻った所の流れで戻る
      flowAt(gx - u * dt * 0.5f, gyf - v * dt * 0.5f, u, v);
      float bx = mx - u * dt * 0.5f, by = my - v * dt * 0.5f;       // 2マスおきの座標へ
      bx = bx < 0 ? 0 : (bx > MGX - 1.001f ? MGX - 1.001f : bx);
      by = by < 0 ? 0 : (by > MGY - 1.001f ? MGY - 1.001f : by);
      const int x0 = (int)bx, y0 = (int)by, j = y0 * MGX + x0;
      const float fx = bx - x0, fy = by - y0;
      const float c00 = (1 - fx) * (1 - fy), c10 = fx * (1 - fy), c01 = (1 - fx) * fy, c11 = fx * fy;
      const int i = my * MGX + mx;
      // 2組とも、同じ「ひとつ前の位置」から座標を読んでくる
      const float px0 = ax0[j] * c00 + ax0[j + 1] * c10 + ax0[j + MGX] * c01 + ax0[j + MGX + 1] * c11;
      const float py0 = ay0[j] * c00 + ay0[j + 1] * c10 + ay0[j + MGX] * c01 + ay0[j + MGX + 1] * c11;
      const float px1 = ax1[j] * c00 + ax1[j + 1] * c10 + ax1[j + MGX] * c01 + ax1[j + MGX + 1] * c11;
      const float py1 = ay1[j] * c00 + ay1[j + 1] * c10 + ay1[j + MGX] * c01 + ay1[j + MGX + 1] * c11;
      bx0[i] = px0; by0[i] = py0; bx1[i] = px1; by1[i] = py1;
      const float mv = (w0 * noiseAt(px0, py0) + w1 * noiseAt(px1, py1)) * MAR_Q;
      mrow[mx] = (int16_t)(mv + (mv >= 0 ? 0.5f : -0.5f));   // 元の模様を、運ばれた座標で引く
    }
  }
}

// 書き終わった座標を、次の読み用にする(2つのコアが両方終わってから)
static void marbleSwap() {
  marPh ^= 1;
  if (marPh == 1) return;                           // 偶数の行だけ終わった。奇数の行は次のコマで
  for (int k = 0; k < 2; ++k)
    for (int c = 0; c < 2; ++c) { float* t = crd[k][c]; crd[k][c] = crdTmp[k][c]; crdTmp[k][c] = t; }
}

// 触った跡を流れに乗せて運ぶ
// 行 ya..yb-1 だけ(src → dst)。rowU / rowV はコアごとの作業場
static void advectGooRows(const int16_t* src, int16_t* dst, int ya, int yb, float dt, float* rowU, float* rowV) {   // 半分の細かさの格子で(流れは高さの格子のマス/秒なので半分に)
  for (int y = max(1, ya); y < min(G2Y - 1, yb); ++y) {
    const int gy = min(GY - 1, y * 2);
    const float* u0 = flowU + (gy >> 2) * CGX; const float* u1 = u0 + CGX;
    const float* v0 = flowV + (gy >> 2) * CGX; const float* v1 = v0 + CGX;
    float fy = (gy & 3) * 0.25f;
    for (int cx = 0; cx < CGX; ++cx) {
      rowU[cx] = (u0[cx] + (u1[cx] - u0[cx]) * fy) * dt * 0.5f;
      rowV[cx] = (v0[cx] + (v1[cx] - v0[cx]) * fy) * dt * 0.5f;
    }
    int i = y * G2X + 1;
    for (int x = 1; x < G2X - 1; ++x, ++i) {
      if (!mask2[i]) { dst[i] = 0; continue; }
      const int gx = min(GX - 1, x * 2);
      float fx = (gx & 3) * 0.25f;
      int c = gx >> 2;
      float du = rowU[c] + (rowU[c + 1] - rowU[c]) * fx;
      float dv = rowV[c] + (rowV[c + 1] - rowV[c]) * fx;
      dst[i] = gooQ(sampleGoo(src, x - du, y - dv));
    }
  }
}

// 表面に浮かぶ小さな粒: 流れに乗って運ばれ、しばらくすると消えて、別の場所にまた浮かぶ
static void spawnMote(Mote& m) {
  float a = frand() * 6.2832f, r = sqrtf(frand()) * 0.85f;
  m.x = MX + cosf(a) * r * (ROUND ? RR : MX);
  m.y = MY + sinf(a) * r * (ROUND ? RR : MY);
  m.age = 0; m.life = 6.0f + frand() * 8.0f;
}
static void updateMotes(float dt) {
  for (auto& m : motes) {
    float u, v;
    flowAt(m.x, m.y, u, v);
    flowAt(m.x + u * dt * 0.5f, m.y + v * dt * 0.5f, u, v);
    m.x += u * dt; m.y += v * dt; m.age += dt;
    float dx = (m.x - MX) / (ROUND ? RR : MX), dy = (m.y - MY) / (ROUND ? RR : MY);
    if (m.age > m.life || dx * dx + dy * dy > 0.92f) spawnMote(m);
  }
}

// 腕が切れたときの、ぷつっ → ぷるん
static void snapString() {
  if (tLump < 0) return;
  Lump& L = lumps[tLump];
  addGooBlob(tTx, tTy, L.baseAmp * 0.55f, L.sig * 0.45f);   // 指先に残ったしずく
  float dx = L.x - tTx, dy = L.y - tTy, d = sqrtf(dx * dx + dy * dy) + 0.001f;
  L.vx += dx / d * 20.0f * SC; L.vy += dy / d * 20.0f * SC;   // 反動で少し戻る
  L.jig = 1.0f;
  tState = T_SNAP; tV = -2.5f;
}

// ---------------------------------------------------------------- シミュレーション
static void simulate(const Input& in) {
  float dt = in.dt > 0.05f ? 0.05f : in.dt;
  simT += dt;

  // 気分: 触られると興奮し、放っておくと落ち着く
  if (in.touching) activity = fminf(1.0f, activity + dt * 0.8f);
  else             activity = fmaxf(0.0f, activity - dt / 25.0f);
  fear *= expf(-dt * 0.8f);
  wake  = fminf(1.0f, wake + dt / 4.0f);             // 起動時にゆっくり目覚める

  // ---- 触った瞬間: ぬちゃっと沈み、押された生き物はすり抜けて震える
  if (in.tapped) {
    fear = fminf(1.0f, fear + 0.6f);
    addGooBlob(in.tx, in.ty, -6.0f * SC, 8.0f * SC);
    for (auto& L : lumps) {
      float dx = L.x - in.tx, dy = L.y - in.ty;
      float d = fmaxf(sqrtf(dx * dx + dy * dy), 1.0f);
      if (d < L.sig * 1.6f) {                        // 押された子: ぬるっと逃げて、ぷるん
        L.vx += (dx / d * 0.8f - dy / d * 0.6f) * 45.0f * SC;
        L.vy += (dy / d * 0.8f + dx / d * 0.6f) * 45.0f * SC;
        L.jig = 1.0f;
      } else {
        float k = 20.0f * SC / (1.0f + d / (30.0f * SC));
        L.vx += dx / d * k; L.vy += dy / d * k;
      }
    }
  }

  // ---- 触っている間: 吸いつき + 引っぱり
  if (in.touching) {
    dragGoo(in.tx, in.ty, in.px, in.py);
    const float hx = in.tx * 0.5f, hy = in.ty * 0.5f;   // 半分の細かさの格子で
    float s1 = 3.5f * SC, s2 = s1 * 2.2f;
    float i1 = 1.0f / (2 * s1 * s1), i2 = 1.0f / (2 * s2 * s2);
    float grip = fminf(1.0f, dt * GOO_GRIP);
    float stick = GOO_STICK * SC;
    int rad = (int)(s2 * 2.3f);
    int x0 = max(1, (int)hx - rad), x1 = min(G2X - 2, (int)hx + rad);
    int y0 = max(1, (int)hy - rad), y1 = min(G2Y - 2, (int)hy + rad);
    for (int y = y0; y <= y1; ++y)
      for (int x = x0; x <= x1; ++x) {
        float dx = x - hx, dy = y - hy, d2 = dx * dx + dy * dy;
        float q1 = d2 * i1 * 32.0f, q2 = d2 * i2 * 32.0f;         // exp(-x) は表引き(軽くするため)
        if (q2 >= 255.0f) continue;
        int k2 = (int)q2; float w2 = expLut[k2] + (expLut[k2 + 1] - expLut[k2]) * (q2 - k2);
        float w1 = 0;
        if (q1 < 255.0f) { int k1 = (int)q1; w1 = expLut[k1] + (expLut[k1 + 1] - expLut[k1]) * (q1 - k1); }
        float target = stick * (w1 - GOO_SUCK * w2);   // 真ん中は持ち上がり、まわりはくぼむ
        int16_t& gq = gooA[y * G2X + x];
        const float g = gq * GOO_IQ;
        gq = gooQ(g + (target - g) * grip * w2);
      }
  }

  // ---- 見えない渦の流れ(カールノイズ)と、それに乗って流れるぬめり・粒
  buildFlow(in, dt);
  marblePrep(dt);                                  // 運ぶのは、描くときに2つのコアで
  updateMotes(dt);

  // ---- 粘りで広がりながら、流れに運ばれて、ゆっくりたれて戻る(振動しない)
  //      しばらく触らないと膜は平らに戻りきるので、計算を休んで軽くする
  if (in.touching || in.tapped || tState != T_NONE) gooQuiet = 0;
  else gooQuiet += dt;
  gooPasses = 0;
  if (gooQuiet > 50.0f) {
    if (!gooCleared) { memset(gooA, 0, G2N * sizeof(int16_t)); gooCleared = true; }
  } else {
    gooCleared = false;
    float relax = expf(-dt * GOO_RELAX);
    static float advAcc = 0;  // 流れに乗せるのは1フレームおき(軽くするため)
    advAcc += dt;
    gooAdvNow = (gooFlip ^= 1) != 0;
    if (gooAdvNow) { gooAdvDt = advAcc; advAcc = 0; }
    const float diff = GOO_DIFFUSE * 0.25f;          // 格子が倍の粗さなので、1回に広がる量は 1/4
    gooK0[0] = (int32_t)(relax * 16384.0f + 0.5f); gooKd[0] = (int32_t)(diff * relax * 16384.0f + 0.5f);   // 整数で(14bit 固定小数点)
    gooPasses = 1;                                   // 計算そのものは、描く前に2つのコアで(prepareHalf)
  }

  // ---- 呼吸と鼓動(やわらかく)
  float period = fmaxf(3.0f, 6.0f - 2.0f * activity - 1.0f * fear);
  breathPhase += dt * 6.2832f / period;
  float breath = sinf(breathPhase);
  hbPhase += dt / (1.6f - 0.5f * activity - 0.3f * fear);
  if (hbPhase >= 1.0f) hbPhase -= 1.0f;
  float hb = softPulse(hbPhase - 0.10f) + 0.5f * softPulse(hbPhase - 0.32f);

  // ---- 腕: 伸ばす → (離すと)糸を引く → ぷつっ → ぷるんと戻る
  if (in.touching) {
    if (tState != T_REACH) {
      if (tState == T_STRING) snapString();
      float best = 1e9f;
      for (int n = 0; n < NLUMP; ++n) {
        float dx = lumps[n].x - in.tx, dy = lumps[n].y - in.ty, d = dx * dx + dy * dy;
        if (d < best) { best = d; tLump = n; }
      }
      if (tState == T_NONE || tState == T_SNAP) tS = fmaxf(tS, 0.0f);
      tState = T_REACH;
    }
    tTx = in.tx; tTy = in.ty;
    tS = fminf(1.0f, tS + dt * TENDRIL_IN * (0.5f + activity));
  } else if (tState == T_REACH) {
    if (tS > 0.25f) { tState = T_STRING; strT = 0; strDur = STRING_TIME * (0.6f + 0.8f * tS); }
    else            { tState = T_SNAP; tV = 0; }
  }
  if (tState == T_STRING) {
    strT += dt;
    if (strT >= strDur) snapString();
  } else if (tState == T_SNAP) {                     // ばねのように、少し行き過ぎて戻る
    tV += (-55.0f * tS - 7.0f * tV) * dt;
    tS += tV * dt;
    if (fabsf(tS) < 0.01f && fabsf(tV) < 0.05f) { tState = T_NONE; tS = 0; tV = 0; }
  }

  // ---- 生き物: 重たく、のたうつように動く
  float speed = 0.5f + 0.8f * activity + 0.6f * fear;
  for (int n = 0; n < NLUMP; ++n) {
    Lump& L = lumps[n];
    L.theta += dt * speed * (0.6f * sinf(simT * 0.23f + L.phase) + 0.5f * sinf(simT * 0.51f + L.phase * 1.7f));
    float fx = cosf(L.theta) * LUMP_FORCE * SC * speed;
    float fy = sinf(L.theta) * LUMP_FORCE * SC * speed;

    float dx = L.x - MX, dy = L.y - MY;               // 画面の内側にとどまる
    if (ROUND) {
      float r = sqrtf(dx * dx + dy * dy) + 0.001f, lim = 0.55f * RR;
      if (r > lim) { fx -= dx / r * (r - lim) * 2.0f; fy -= dy / r * (r - lim) * 2.0f; }
    } else {
      float limX = 0.70f * MX, limY = 0.62f * MY;
      if (dx >  limX) fx -= (dx - limX) * 2.0f;
      if (dx < -limX) fx -= (dx + limX) * 2.0f;
      if (dy >  limY) fy -= (dy - limY) * 2.0f;
      if (dy < -limY) fy -= (dy + limY) * 2.0f;
    }

    for (int m = 0; m < NLUMP; ++m) {                // くっつきすぎない(少し重なるのはOK)
      if (m == n) continue;
      float ox = L.x - lumps[m].x, oy = L.y - lumps[m].y;
      float d = sqrtf(ox * ox + oy * oy) + 0.001f;
      float want = (L.sig + lumps[m].sig) * 0.7f;
      if (d < want) { fx += ox / d * (want - d) * 0.8f; fy += oy / d * (want - d) * 0.8f; }
    }

    if (in.touching) {
      float tx = L.x - in.tx, ty = L.y - in.ty;
      float d = sqrtf(tx * tx + ty * ty) + 0.001f;
      float reach = L.sig * 1.3f;
      if (d < reach) {                               // 指の下からぬるっとすり抜ける
        float p = (reach - d) / reach;
        float side = (n & 1) ? 1.0f : -1.0f;
        fx += (tx / d - side * ty / d * 0.7f) * SLIP * SC * p;
        fy += (ty / d + side * tx / d * 0.7f) * SLIP * SC * p;
        L.jig = fmaxf(L.jig, 0.4f * p);
      } else if (n == tLump && tState == T_REACH && d > 35 * SC) {
        fx -= tx / d * 5.0f * SC * tS; fy -= ty / d * 5.0f * SC * tS;   // 腕を伸ばした子は体ごと寄る
      }
    }
    if (n == tLump && tState == T_STRING) {          // 離されたら体を引いて、糸がのびる
      float tx = L.x - tTx, ty = L.y - tTy;
      float d = sqrtf(tx * tx + ty * ty) + 0.001f;
      fx += tx / d * 14.0f * SC; fy += ty / d * 14.0f * SC;
    }

    float damp = expf(-dt * LUMP_DRAG);
    L.vx = (L.vx + fx * dt) * damp;
    L.vy = (L.vy + fy * dt) * damp;
    L.x += L.vx * dt; L.y += L.vy * dt;
    { float fu, fv; flowAt(L.x, L.y, fu, fv); L.x += fu * CARRY * dt; L.y += fv * CARRY * dt; }   // 流れに押される
    L.jig *= expf(-dt * 2.5f);
    L.jigPh += dt * 13.0f;
  }

  // ---- 表面にときどき何かがぬるっと浮かんで、沈む
  bubbleTimer -= dt;
  if (!bubble.on && bubbleTimer < 0) {
    bubble.on = true; bubble.t = 0;
    bubble.T = 3.5f + frand() * 3.0f;
    float a = frand() * 6.2832f, r = sqrtf(frand()) * 0.6f;
    bubble.x = MX + cosf(a) * r * (ROUND ? RR : MX);
    bubble.y = MY + sinf(a) * r * (ROUND ? RR : MY);
    bubble.amp = (5.0f + frand() * 5.0f) * SC;
    bubble.sig = (7.0f + frand() * 6.0f) * SC;
    bubbleTimer = (6.0f + frand() * 8.0f) * (1.0f - 0.4f * activity);
  }

  // ---- 高さ = ふくらんだ膜 + ねっとり変形 + 流れるぬめり(帯を描くときに作る)。ここでは、このフレームの値だけ決める
  domeDepth = ((ROUND ? 9.0f : 14.0f) + 2.0f * breath + HEART_DOME * hb) * SC;
  wetK = 0.15f / SC;
  marK = MARBLE * SC;
  nBlob = 0; nBig = 0;

  // ---- 生き物
  float gain = wake * (1.0f + 0.10f * breath + HEART_GLOW * hb) * (1.0f - 0.35f * fear);
  for (int n = 0; n < NLUMP; ++n) {
    Lump& L = lumps[n];
    float wob = L.jig * JIGGLE * sinf(L.jigPh);      // ぷるぷる
    float amp = L.baseAmp * gain * (0.85f + 0.15f * sinf(simT * 0.7f + L.phase)) * (1.0f + wob);
    float sig = L.sig * (1.0f + 0.07f * breath + HEART_SWELL * hb);
    float v = sqrtf(L.vx * L.vx + L.vy * L.vy);
    float stretch = fminf(1.3f, v * 0.06f / SC);     // 速く動くほど、ねばっと伸びる
    float dirX = v > 0.01f ? L.vx / v : 1.0f, dirY = v > 0.01f ? L.vy / v : 0.0f;
    float sa = sig * (1.0f + stretch) * (1.0f - 0.5f * wob);
    float sp = sig / (1.0f + 0.5f * stretch) * (1.0f + 0.5f * wob);
    addStretchedBlob(L.x, L.y, amp, sa, sp, dirX, dirY);
    addBlob(L.x - L.vx * 0.35f, L.y - L.vy * 0.35f, amp * 0.35f, sig * 0.8f);   // ぬめっとした余韻
  }

  // ---- 腕 / 糸
  if (tState != T_NONE && tLump >= 0) {
    Lump& L = lumps[tLump];
    float e;
    float neck = 0;                                  // 糸の細さ(0=ふつう → 1=切れる寸前)
    float tipX = tTx, tipY = tTy;
    if (tState == T_REACH)       e = smooth3(tS);
    else if (tState == T_STRING) {
      e = 1.0f;
      neck = strT / strDur;
      float ax = tTx - L.x, ay = tTy - L.y, al = sqrtf(ax * ax + ay * ay) + 0.001f;
      float sag = 6.0f * SC * neck * neck;           // 先のしずくが、重みで少したれる
      tipX += ax / al * sag; tipY += ay / al * sag;
    } else                       e = tS;             // T_SNAP: 少し行き過ぎてもOK
    float ax = (tipX - L.x) * e, ay = (tipY - L.y) * e;
    float len = sqrtf(ax * ax + ay * ay);
    if (len > 3.0f * SC) {
      float nx = -ay / len, ny = ax / len;
      int beads = (int)fminf(22.0f, fmaxf(5.0f, len / (3.5f * SC)));
      float wob = (5.0f * sinf(simT * 1.1f) + 3.0f * sinf(simT * 2.3f + 1.0f)) * SC * (1.0f - neck);
      for (int b = 1; b <= beads; ++b) {
        float t = (float)b / beads;
        float bend = sinf(t * 3.1416f) * wob;
        float bx = L.x + ax * t + nx * bend;
        float by = L.y + ay * t + ny * bend;
        float thick = L.sig * (0.50f - 0.28f * t) + 3.5f * SC;
        float a = L.baseAmp * gain * (0.75f - 0.35f * t);
        if (neck > 0) {                              // 真ん中から細く、糸のように
          float pinch = smooth3(neck * 1.3f) * expf(-(t - 0.55f) * (t - 0.55f) / (2 * 0.26f * 0.26f));
          thick *= 1.0f - 0.80f * pinch;
          a     *= 1.0f - 0.88f * pinch;
        }
        if (b == beads) { thick *= 1.3f; a *= 1.2f; }    // 先っぽは少しふくらむ(しずく)
        addBlob(bx, by, a, fmaxf(thick, 1.5f * SC));
      }
    }
  }

  if (bubble.on) {
    bubble.t += dt;
    if (bubble.t >= bubble.T) bubble.on = false;
    else {
      float s = sinf(3.1416f * bubble.t / bubble.T);
      addBlob(bubble.x, bubble.y, bubble.amp * s * s * wake, bubble.sig * (0.7f + 0.3f * s));
    }
  }

  // ---- 渦に乗って回る小さな粒(ふっと浮かんで、ふっと消える)
  for (const auto& m : motes) {
    float e = sinf(3.1416f * fminf(1.0f, m.age / m.life));
    addBlob(m.x, m.y, 2.2f * SC * wake * e * e, 2.6f * SC);
  }
}

// ---------------------------------------------------------------- 描画
static inline uint16_t envAt(uint32_t u, uint32_t v) { return env[(v >> 8) * ENV + (u >> 8)]; }

// ふくらみ B を、行(縦の差 ddy)の上に足す。点の位置は x = j*step(j = ja..jb)
//   高さの式 q = A*dx^2 + Bq*dx + C は dx の2次式なので、q < 255(見える)範囲を先に求めて、
//   その中だけを「足し算」で進める(1点ごとのかけ算が要らない)
static inline __attribute__((always_inline)) void addBlobRow(const Blob& B, float ddy, int ja, int jb, int step, float* row) {
  float A, Bq, C;
  if (!B.str) { A = B.ia; Bq = 0.0f; C = ddy * ddy * B.ia; }
  else {
    const float dX = B.dirX, dY = B.dirY;
    A  = B.ia * dX * dX + B.ip * dY * dY;
    Bq = 2.0f * ddy * dX * dY * (B.ia - B.ip);
    C  = ddy * ddy * (B.ia * dY * dY + B.ip * dX * dX);
  }
  const float disc = Bq * Bq - 4.0f * A * (C - 255.0f);
  if (disc <= 0.0f) return;
  const float r = sqrtf(disc), i2A = 0.5f / A;
  const float lo = (-Bq - r) * i2A + B.cx, hi = (-Bq + r) * i2A + B.cx;   // この行で見える x の範囲
  const float istep = 1.0f / step;
  const int j0 = max(ja, (int)ceilf(lo * istep)), j1 = min(jb, (int)floorf(hi * istep));
  if (j0 > j1) return;
  const float dx0 = j0 * step - B.cx, fs = (float)step;
  float q = (A * dx0 + Bq) * dx0 + C;
  float dq = A * (2.0f * dx0 * fs + fs * fs) + Bq * fs;
  const float ddq = 2.0f * A * fs * fs;
  const float amp = B.amp;
  for (int j = j0; j <= j1; ++j) {
    const float qq = q < 0.0f ? 0.0f : q;
    if (qq < 255.0f) { const int i = (int)qq; row[j] += amp * (expLut[i] + (expLut[i + 1] - expLut[i]) * (qq - i)); }
    q += dq; dq += ddq;
  }
}

// 半分の細かさの高さ H2 の行 y2a..y2b-1 を作る(2つのコアで半分ずつ)
//   高さ = ふくらんだ膜 + ねっとり変形 + ぬめり + 太いふくらみ。前の版と同じ式を、2マスおきの点で計算する
static void heightHalfRows(int y2a, int y2b, float* row, const int16_t* goo) {
  const float iRD = 1.0f / RD;
  for (int y2 = y2a; y2 < y2b; ++y2) {
    const int y = y2 * 2;
    int16_t* out = H2 + y2 * G2X;
    const int ys = min(GY - 1, y);
    if (rowS[ys] > rowE[ys] && (y2 == 0 || rowS[min(GY - 1, y - 2)] > rowE[min(GY - 1, y - 2)]) && (y + 2 > GY - 1 || rowS[min(GY - 1, y + 2)] > rowE[min(GY - 1, y + 2)])) {
      memset(out, 0, G2X * sizeof(int16_t)); continue;  // 画面の外
    }
    const float dy = (y - MY) * iRD, dy2 = dy * dy;
    const int16_t* g = goo + y2 * G2X;
    const int16_t* mr = marA + min(MGY - 2, y2) * MGX;          // 流れるぬめり(2マスおき = 同じ細かさ)
    const float mk = marK * MAR_IQ;
    for (int x2 = 0; x2 < G2X; ++x2) {
      const int x = min(GX - 1, x2 * 2);
      const float n = mr[x2] * mk;
      const float gg = g[x2] * GOO_IQ;
      const float dx = (x - MX) * iRD;
      row[x2] = -domeDepth * (dx * dx + dy2) + gg + n * (1.0f + wetK * fabsf(gg));   // 触った跡ほどテカテカ
    }
    for (int k = 0; k < nBig; ++k) {
      const Blob& B = bigBlobs[k];
      if (y < B.y0 || y > B.y1) continue;
      const int xa = (B.x0 + 1) >> 1, xb = min(G2X - 1, B.x1 >> 1);
      addBlobRow(B, y - B.cy, xa, xb, 2, row);      // 2マスおきの点
    }
    for (int x2 = 0; x2 < G2X; ++x2) {
      float v = row[x2] * H2_Q;
      v = v > 32000.0f ? 32000.0f : (v < -32000.0f ? -32000.0f : v);
      out[x2] = (int16_t)(v + (v >= 0 ? 0.5f : -0.5f));
    }
  }
}

// 高さの1行(格子 y 行目の xa..xb)を作る: H2 をなめらかにつないで、細いふくらみを足す
static inline void heightRow(int y, int xa, int xb, float* h) {
  const int y2 = y >> 1;
  const int16_t* r0 = H2 + y2 * G2X;
  const int16_t* r1 = (y & 1) ? r0 + G2X : r0;
  for (int x = xa; x <= xb; ++x) {
    const int x2 = x >> 1;
    int32_t v;
    if (x & 1) v = r0[x2] + r0[x2 + 1] + r1[x2] + r1[x2 + 1];
    else       v = (r0[x2] + r1[x2]) * 2;
    h[x] = v * (H2_IQ * 0.25f);
  }
  for (int k = 0; k < nBlob; ++k) {                  // 細いふくらみ(q < 255 になる範囲だけ、足し算で進める)
    const Blob& B = blobs[k];
    if (y < B.y0 || y > B.y1) continue;
    addBlobRow(B, y - B.cy, max((int)B.x0, xa), min((int)B.x1, xb), 1, h);
  }
}

// 帯 k(画面の 8 行)を描く: 高さ → 映る向き → 景色を引いて 2 倍に広げる
static void renderBand(int k, uint16_t* buf, Scratch& s) {
  const int c0 = bandC0[k], c1 = bandC1[k];
  const int gyA = k * (BAND / 2), gyB = min(CYN, gyA + BAND / 2) - 1;   // この帯の格子の行
  const int hy0 = max(0, gyA - 1), hy1 = min(GY - 1, gyB + 2);
  const int hxa = bandHX0[k], hxb = bandHX1[k];
  for (int y = hy0; y <= hy1; ++y) heightRow(y, hxa, hxb, s.h + (y - hy0) * GX);
  const float kk = REFLECT * 256.0f;
  const float maxUV = (ENV - 1) * 256.0f + 255.0f;
  const UV ctrUV = {64 * 256, 64 * 256};
  for (int y = gyA; y <= gyB + 1; ++y) {
    UV* out = s.uv + (y - gyA) * GX;
    for (int x = c0; x <= c1; ++x) out[x] = ctrUV;             // 計算しない場所は景色の中心
    const int ym = y > 0 ? y - 1 : 0, yp = y < GY - 1 ? y + 1 : GY - 1;
    const float* hm = s.h + (ym - hy0) * GX;
    const float* hc = s.h + (y - hy0) * GX;
    const float* hp = s.h + (yp - hy0) * GX;
    const int xs = max((int)gradS[y], c0), xe = min((int)gradE[y], c1);
    for (int x = xs; x <= xe; ++x) {
      const int xm = x > 0 ? x - 1 : 0, xp = x < GX - 1 ? x + 1 : GX - 1;
      float u = 64.0f * 256.0f + softEdge((hc[xp] - hc[xm]) * kk);
      float v = 64.0f * 256.0f + softEdge((hp[x] - hm[x]) * kk);
      u = u < 0 ? 0 : (u > maxUV ? maxUV : u);
      v = v < 0 ? 0 : (v > maxUV ? maxUV : v);
      out[x] = {(uint16_t)u, (uint16_t)v};
    }
  }
  const int bw = (c1 - c0) * 2;
  for (int gy = gyA; gy <= gyB; ++gy) {
    const UV* r0 = s.uv + (gy - gyA) * GX;
    const UV* r1 = r0 + GX;
    uint16_t* row0 = buf + (gy - gyA) * 2 * bw;
    uint16_t* row1 = row0 + bw;
    const int rc0 = rowC0[gy], rc1 = rowC1[gy];
    for (int cx = c0; cx < c1; ++cx) {
      const int o = (cx - c0) * 2;
      if (cx < rc0 || cx >= rc1) { row0[o] = row0[o + 1] = row1[o] = row1[o + 1] = 0; continue; }   // 丸の外
      const UV a = r0[cx], b = r0[cx + 1], c = r1[cx], d = r1[cx + 1];
      row0[o]     = envAt(a.u, a.v);
      row0[o + 1] = envAt((a.u + b.u) >> 1, (a.v + b.v) >> 1);
      row1[o]     = envAt((a.u + c.u) >> 1, (a.v + c.v) >> 1);
      row1[o + 1] = envAt((a.u + b.u + c.u + d.u) >> 2, (a.v + b.v + c.v + d.v) >> 2);
    }
  }
}
static void __attribute__((noinline)) MOD_IRAM renderBandIram(int k, uint16_t* b, Scratch& s) { renderBand(k, b, s); }

// 指の跡を広げて、ゆっくり戻す(行 ya..yb-1。src → dst)。2つのコアで行を分ける
static void gooDiffuseRows(const int16_t* __restrict src, int16_t* __restrict dst, int ya, int yb, int32_t k0, int32_t kd) {
  for (int y = max(1, ya); y < min(G2Y - 1, yb); ++y) {
    int i = y * G2X + 1;
    for (int x = 1; x < G2X - 1; ++x, ++i) {
      if (!mask2[i]) { dst[i] = 0; continue; }
      const int32_t lap = src[i - 1] + src[i + 1] + src[i - G2X] + src[i + G2X] - 4 * src[i];
      const int32_t nb = (src[i] * k0 + lap * kd + 8192) >> 14;
      dst[i] = (int16_t)(nb > 32000 ? 32000 : (nb < -32000 ? -32000 : nb));
    }
  }
}

// コア0: コア1が描かない帯
// もう片方のコアが「target 番目」まで終わるのを待つ
static inline void barrier(int me, int32_t target) {
  __atomic_store_n(&phaseCnt[me], target, __ATOMIC_RELEASE);
  while (__atomic_load_n(&phaseCnt[1 - me], __ATOMIC_ACQUIRE) < target) { }
}

// 指の跡の計算(2つのコアで行を半分ずつ)。できあがった跡の置き場を返す
static const int16_t* gooStep(int me, int ya, int yb, int32_t& ph) {
  int16_t* src = gooA; int16_t* dst = gooB;
  if (gooPasses > 0 && gooAdvNow) {               // 流れに乗せる
    advectGooRows(src, dst, ya, yb, gooAdvDt, flowRowU + me * CGX, flowRowV + me * CGX);
    barrier(me, ++ph); int16_t* t = src; src = dst; dst = t;
  }
  for (int p = 0; p < gooPasses; ++p) {             // 粘りで広げて、ゆっくり戻す
    gooDiffuseRows(src, dst, ya, yb, gooK0[p], gooKd[p]);
    barrier(me, ++ph); int16_t* t = src; src = dst; dst = t;
  }
  return src;
}

// このフレームの下ごしらえ(2つのコアで半分ずつ)
static void prepareHalf(int me) {
  const uint32_t t0 = micros();
  int32_t ph = phaseBase;
  const int mid = G2Y / 2, ya = me == 0 ? 0 : mid, yb = me == 0 ? mid : G2Y;
  const int16_t* goo = gooStep(me, ya, yb, ph);
  { const int mid = MGY / 2; if (me == 0) marbleRows(1, mid); else marbleRows(mid, MGY - 1); }
  barrier(me, ++ph);                               // ぬめりがそろってから高さを作る
  heightHalfRows(ya, yb, scr[me].row, goo);
  barrier(me, ++ph);
  prepUs[me] = micros() - t0;
}

static void workerTask(void*) {
  for (;;) {
    xSemaphoreTake(semGo, portMAX_DELAY);
    prepareHalf(0);
    uint32_t busy = 0;
    for (int k = 0; k < NB; ++k) {
      if (k % SPLIT == 0) continue;
      while (__atomic_load_n(&doneUpTo, __ATOMIC_ACQUIRE) < k - NSLOT) { }
      if (bandC1[k] > bandC0[k]) {
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
  prepareHalf(1);
  uint32_t busy = 0, send = 0;
  if (!fbMode) M5.Display.startWrite();
  for (int k = 0; k < NB; ++k) {
    const bool vis = bandC1[k] > bandC0[k];
    if (k % SPLIT == 0) {
      if (vis) {
        uint32_t t0 = micros();
        renderBandIram(k, slotBuf[k % NSLOT], scr[1]);
        busy += micros() - t0;
      }
    } else {
      while (__atomic_load_n(&readyW, __ATOMIC_ACQUIRE) < k) { }
    }
    if (!fbMode) { const uint32_t tw = micros(); M5.Display.waitDMA(); send += micros() - tw; }   // 前の帯を送り終わるまで待つ
    __atomic_store_n(&doneUpTo, k - 1, __ATOMIC_RELEASE);
    if (vis) {
      const int y0 = k * BAND, h = min(BAND, H - y0);
      const int x0 = bandC0[k] * 2, w = (bandC1[k] - bandC0[k]) * 2;
      if (fbMode) {
        const uint32_t t0 = micros();
        M5.Display.pushImage(x0, y0, w, h, slotBuf[k % NSLOT]);
        send += micros() - t0;
      } else {
        M5.Display.pushImageDMA(x0, y0, w, h, slotBuf[k % NSLOT]);
      }
    }
  }
  if (!fbMode) M5.Display.waitDMA();
  __atomic_store_n(&doneUpTo, NB, __ATOMIC_RELEASE);
  if (!fbMode) M5.Display.endWrite();
  xSemaphoreTake(semDone, portMAX_DELAY);
  phaseBase += PHASES;
  { const int n = gooPasses + ((gooPasses > 0 && gooAdvNow) ? 1 : 0); if (n & 1) { int16_t* t = gooA; gooA = gooB; gooB = t; } }
  marbleSwap();
  busyUs[1] = busy;
  sendUs = send;
}

// 内蔵メモリを優先、足りなければ PSRAM。内蔵メモリは、画面の転送やタスクのために 48KB は残しておく
static void* allocFast(size_t n) {
  if (heap_caps_get_free_size(MALLOC_CAP_INTERNAL) >= n + 48 * 1024) {
    void* p = heap_caps_malloc(n, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (p) return p;
  }
  return ps_malloc(n);
}

static void fail(const char* msg) {
  M5.Display.setTextDatum(middle_center);
  M5.Display.drawString(msg, M5.Display.width() / 2, M5.Display.height() / 2);
  Serial.println(msg);
  while (true) nmIdle();
}

__attribute__((cold)) void setup() {   // cold: 1回だけ動くので、小さく作る
  auto cfg = M5.config();
  M5.begin(cfg);
  Serial.println("NullLife_Curl v7");
  Serial.begin(115200);
#if defined(ESP_PLATFORM)
  if (getCpuFrequencyMhz() < 240) setCpuFrequencyMhz(240);   // 計算が多いので、いちばん速い設定で
  Serial.printf("cpu %d MHz\n", (int)getCpuFrequencyMhz());
#endif

  // 画面の形で機種を判断: 正方形 = StopWatch(丸)、横長 = CoreS3系
  ROUND = (M5.Display.width() == M5.Display.height());
  if (!ROUND && M5.Display.width() < M5.Display.height()) M5.Display.setRotation(1);
  W = M5.Display.width();
  H = M5.Display.height();
  CXN = W / 2; CYN = H / 2;
  GX = CXN + 1; GY = CYN + 1; GN = GX * GY;
  MX = (GX - 1) * 0.5f; MY = (GY - 1) * 0.5f;
  RR = fminf(MX, MY);
  RD = ROUND ? RR : 100.0f;
  SC = fminf(GX, GY) / 121.0f;                      // CoreS3 を基準(1.0)にした大きさ
  Serial.printf("screen %dx%d  %s  grid %dx%d\n", W, H, ROUND ? "round" : "rect", GX, GY);

  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setBrightness(brightSteps[brightIdx]);

  G2X = CXN / 2 + 2; G2Y = CYN / 2 + 2; G2N = G2X * G2Y;
  NB = (H + BAND - 1) / BAND;
  CGX = (GX + 3) / 4 + 2; CGY = (GY + 3) / 4 + 2;
  MGX = GX / 2 + 3; MGY = GY / 2 + 3;
  env      = (uint16_t*)heap_caps_malloc(ENV * ENV * 2, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  noiseT   = (float*)heap_caps_malloc(NOISE * NOISE * sizeof(float), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  for (int i = 0; i < NSLOT; ++i) slotBuf[i] = (uint16_t*)heap_caps_malloc(W * BAND * 2, MALLOC_CAP_DMA);
  // 流れ(4マスおき)は毎フレーム何度も引くので内蔵メモリ。ψ は1フレーム1回、順番に読むだけなので PSRAM
  psiS     = (float*)ps_calloc(CGX * CGY, sizeof(float));
  psiT     = (float*)ps_calloc(CGX * CGY, sizeof(float));
  flowU    = (float*)heap_caps_calloc(CGX * CGY, sizeof(float), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  flowV    = (float*)heap_caps_calloc(CGX * CGY, sizeof(float), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  flowRowU = (float*)heap_caps_malloc(2 * CGX * sizeof(float), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);   // コアごとに1行ぶん
  flowRowV = (float*)heap_caps_malloc(2 * CGX * sizeof(float), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  // 流れるぬめりの座標(2マスおき)は大きいので PSRAM。1フレーム1回、順番に読み書きするだけ
  for (int k = 0; k < 2; ++k) {
    crd[k][0] = (float*)ps_calloc(MGX * MGY, sizeof(float));
    crd[k][1] = (float*)ps_calloc(MGX * MGY, sizeof(float));
    crdTmp[k][0] = (float*)ps_calloc(MGX * MGY, sizeof(float));
    crdTmp[k][1] = (float*)ps_calloc(MGX * MGY, sizeof(float));
  }
  rowS  = (int16_t*)malloc(GY * sizeof(int16_t)); rowE  = (int16_t*)malloc(GY * sizeof(int16_t));
  gradS = (int16_t*)malloc(GY * sizeof(int16_t)); gradE = (int16_t*)malloc(GY * sizeof(int16_t));
  bandC0 = (int16_t*)malloc(NB * sizeof(int16_t)); bandC1 = (int16_t*)malloc(NB * sizeof(int16_t));
  bandHX0 = (int16_t*)malloc(NB * sizeof(int16_t)); bandHX1 = (int16_t*)malloc(NB * sizeof(int16_t));
  rowC0 = (int16_t*)malloc(GY * sizeof(int16_t)); rowC1 = (int16_t*)malloc(GY * sizeof(int16_t));
  for (int c = 0; c < 2; ++c) {
    scr[c].h  = (float*)heap_caps_malloc((BAND / 2 + 3) * GX * sizeof(float), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    scr[c].uv = (UV*)heap_caps_malloc((BAND / 2 + 1) * GX * sizeof(UV), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    scr[c].row = (float*)heap_caps_malloc(G2X * sizeof(float), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  }
  H2 = (int16_t*)heap_caps_calloc(G2N, sizeof(int16_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  if (!H2) H2 = (int16_t*)ps_calloc(G2N, sizeof(int16_t));
  mask2 = (uint8_t*)heap_caps_malloc(G2N, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  gooA  = (int16_t*)allocFast(G2N * sizeof(int16_t));
  gooB  = (int16_t*)allocFast(G2N * sizeof(int16_t));
  marA  = (int16_t*)allocFast(MGX * MGY * sizeof(int16_t));
  bool ok = env && noiseT && psiS && psiT && flowU && flowV && flowRowU && flowRowV && rowS && rowE && gradS && gradE &&
            bandC0 && bandC1 && bandHX0 && bandHX1 && rowC0 && rowC1 && mask2 && gooA && gooB && marA;
  for (int k = 0; k < 2; ++k) ok = ok && crd[k][0] && crd[k][1] && crdTmp[k][0] && crdTmp[k][1];
  for (int i = 0; i < NSLOT; ++i) ok = ok && slotBuf[i];
  for (int c = 0; c < 2; ++c) ok = ok && scr[c].h && scr[c].uv && scr[c].row;
  ok = ok && H2;
  if (!ok) fail("Memory error (PSRAM?)");
  memset(gooA, 0, G2N * sizeof(int16_t)); memset(gooB, 0, G2N * sizeof(int16_t)); memset(marA, 0, MGX * MGY * sizeof(int16_t));
  for (int y = 0; y < G2Y; ++y)                      // 膜のある場所(半分の細かさ)
    for (int x = 0; x < G2X; ++x) {
      float dx = (x * 2 - MX) / RD, dy = (y * 2 - MY) / RD;
      mask2[y * G2X + x] = ROUND ? (dx * dx + dy * dy < 0.98f) : 1;
    }

  // 計算する範囲: 四角い画面は全部。丸い画面は円の内側だけ(少し余裕をもたせる)
  for (int y = 0; y < GY; ++y) {
    if (!ROUND) { rowS[y] = 0; rowE[y] = GX - 1; gradS[y] = 0; gradE[y] = GX - 1; continue; }
    float dy = y - MY;
    float rh = RR + 4.0f, rg = RR + 2.0f;
    float wh = rh * rh - dy * dy, wg = rg * rg - dy * dy;
    int hs = 0, he = -1, gs = 1, ge = 0;
    if (wh > 0) { float s = sqrtf(wh); hs = max(0, (int)floorf(MX - s)); he = min(GX - 1, (int)ceilf(MX + s)); }
    if (wg > 0 && y > 0 && y < GY - 1) {
      float s = sqrtf(wg); gs = max(1, (int)floorf(MX - s)); ge = min(GX - 2, (int)ceilf(MX + s));
    }
    rowS[y] = hs; rowE[y] = he; gradS[y] = gs; gradE[y] = ge;
  }
  // 帯ごとの範囲: 描く幅(前の版と同じく、2行ずつ丸にかかる幅)と、高さを計算する幅
  {
    const float rr = W * 0.5f, ctr = (W - 1) * 0.5f;
    for (int k = 0; k < NB; ++k) {
      int a = CXN, b = 0, ha = GX - 1, hb = 0;
      const int gyA = k * (BAND / 2), gyB = min(CYN, gyA + BAND / 2) - 1;
      for (int gy = gyA; gy <= gyB; ++gy) {
        int c0 = 0, c1 = CXN;
        if (ROUND) {
          const float dy = fminf(fabsf(gy * 2 - ctr), fabsf(gy * 2 + 1 - ctr)), w2 = rr * rr - dy * dy;
          if (w2 <= 0) { c0 = c1 = 0; }
          else {
            const float hw = sqrtf(w2) + 1.0f;
            c0 = max(0, (int)floorf((ctr - hw) * 0.5f)); c1 = min(CXN, (int)ceilf((ctr + hw + 1.0f) * 0.5f));
          }
        }
        rowC0[gy] = c0; rowC1[gy] = c1;
        if (c1 > c0) { a = min(a, c0); b = max(b, c1); }
      }
      for (int y = gyA; y <= gyB + 1 && y < GY; ++y)
        if (gradS[y] <= gradE[y]) { ha = min(ha, gradS[y] - 1); hb = max(hb, gradE[y] + 1); }
      if (b <= a) { a = b = 0; }
      if (hb < ha) { ha = 0; hb = 0; }
      bandC0[k] = a; bandC1[k] = b;
      bandHX0[k] = max(0, ha); bandHX1[k] = min(GX - 1, hb);
    }
  }
  for (int i = 0; i < 257; ++i) expLut[i] = expf(-i / 32.0f);
  buildNoise();

  // 流れるぬめり: 2組の座標を半周期ずらして用意する
  resetCoords(0); resetCoords(1);
  crdAge[0] = 0; crdAge[1] = MARBLE_T * 0.5f;
  for (auto& m : motes) { spawnMote(m); m.age = frand() * m.life; }   // ばらばらの時期から始める

  for (auto& L : lumps) {
    L.x = MX + (frand() - 0.5f) * (ROUND ? RR : MX);
    L.y = MY + (frand() - 0.5f) * (ROUND ? RR : MY);
    L.vx = L.vy = 0;
    L.baseAmp = (8.0f + frand() * 5.0f) * SC;
    L.sig     = (13.0f + frand() * 8.0f) * SC;       // 大きめで、ぼてっと
    L.phase   = frand() * 6.2832f;
    L.theta   = frand() * 6.2832f;
    L.jig = 0; L.jigPh = frand() * 6.2832f;
  }

  buildEnv(theme);

  // 最初の1枚
  simIn = {false, false, 0, 0, 0, 0, 0.016f};
  simulate(simIn);

  // StopWatch の M5GFX は、画面の手前に PSRAM の置き場を持ち、描き終わるたびに自動で画面へ送る
  { auto* pnl = M5.Display.getPanel(); fbMode = pnl && pnl->getAutoDisplay();
#if defined(NL_DIRECT_AMOLED)
    if (fbMode && DIRECT_SEND && M5.getBoard() == m5::board_t::board_M5StopWatch) {
      lgfx::Panel_AMOLED* inner = NlFbPeek::inner(pnl);
      if (inner) { M5.Display.setPanel(inner); fbMode = false; directSend = true; }   // ここからは本物の画面へ直接
    }
#endif
  }
  SPLIT = fbMode ? 4 : 2;                          // 送る係のコア1は、帯4本に1本だけ描く
  Serial.printf("display: %s\n", fbMode ? "framebuffer (send each band)" : directSend ? "direct DMA (StopWatch, no framebuffer)" : "direct DMA");
  semGo   = xSemaphoreCreateBinary();
  semDone = xSemaphoreCreateBinary();
  xTaskCreatePinnedToCore(workerTask, "band", 8192, nullptr, 1, nullptr, 0);
}

void loop() {
  static uint32_t last = millis();
  static uint32_t fpsTimer = millis();
  static uint32_t lastTapMs = 0;
  static float prevX = 0, prevY = 0;
  static int frames = 0;

  M5.update();

  auto t = M5.Touch.getDetail();
  uint32_t now = millis();
  bool pressed = t.wasPressed();

  if (ROUND) {                                       // StopWatch: ボタンで操作
    if (M5.BtnA.wasHold())         { flowLevel = (flowLevel + 1) % 3; }
    else if (M5.BtnA.wasClicked()) { theme = (theme + 1) % NTHEME; buildEnv(theme); }
    if (M5.BtnB.wasPressed()) {
      brightIdx = (brightIdx + 1) % sizeof(brightSteps);
      M5.Display.setBrightness(brightSteps[brightIdx]);
    }
  } else {                                           // CoreS3: ダブルタップと電源ボタン
    if (M5.BtnPWR.wasClicked()) {
      brightIdx = (brightIdx + 1) % sizeof(brightSteps);
      M5.Display.setBrightness(brightSteps[brightIdx]);
    }
    if (pressed) {
      if (now - lastTapMs < 350) { theme = (theme + 1) % NTHEME; buildEnv(theme); lastTapMs = 0; }
      else lastTapMs = now;
    }
  }

  float tx = t.x * 0.5f, ty = t.y * 0.5f;
  if (pressed) { prevX = tx; prevY = ty; }

  simIn.touching = t.isPressed();
  simIn.tapped   = pressed;
  simIn.tx = tx;    simIn.ty = ty;
  simIn.px = prevX; simIn.py = prevY;
  simIn.dt = (now - last) / 1000.0f;
  last = now;
  prevX = tx; prevY = ty;

  const uint32_t ts = micros();
  simulate(simIn);                    // 動きを進める(軽い)
  const uint32_t simUs = micros() - ts;
  renderFrame();                      // 高さ・映る向き・色を帯ごとに作って描く(2つのコア)

  if (++frames, millis() - fpsTimer >= 3000) {
    Serial.printf("fps: %.1f  sim %.1f ms  prep %.1f/%.1f ms  core0 %.1f ms  core1 %.1f ms  send %.1f ms\n", frames * 1000.0f / (millis() - fpsTimer),
                  simUs / 1000.0f, prepUs[0] / 1000.0f, prepUs[1] / 1000.0f, busyUs[0] / 1000.0f, busyUs[1] / 1000.0f, sendUs / 1000.0f);
    frames = 0; fpsTimer = millis();
  }
}


// 共通の置き場に置いたものは、0 で始まる単純な型だけ(コンストラクタが要らないもの)
static_assert(std::is_trivially_default_constructible<Blob>::value, "NM_ARENA: Blob");
static_assert(std::is_trivially_default_constructible<Input>::value, "NM_ARENA: Input");
static_assert(std::is_trivially_default_constructible<Lump>::value, "NM_ARENA: Lump");
static_assert(std::is_trivially_default_constructible<Mote>::value, "NM_ARENA: Mote");
static_assert(std::is_trivially_default_constructible<Scratch>::value, "NM_ARENA: Scratch");
static_assert(std::is_trivially_default_constructible<bool>::value, "NM_ARENA: bool");
static_assert(std::is_trivially_default_constructible<float>::value, "NM_ARENA: float");
static_assert(std::is_trivially_default_constructible<int>::value, "NM_ARENA: int");
static_assert(std::is_trivially_default_constructible<int16_t*>::value, "NM_ARENA: int16_t*");
static_assert(std::is_trivially_default_constructible<int32_t>::value, "NM_ARENA: int32_t");
static constexpr size_t OVL_END = nmAlignTo(nmOv69, 16);
static_assert(OVL_END <= NM_ARENA_SIZE, "NM_ARENA_SIZE (nm_common.h) を大きくしてください");
extern const size_t OVL_SIZE = OVL_END;   // 起動時の確認用
}  // namespace NullLife_Curl
