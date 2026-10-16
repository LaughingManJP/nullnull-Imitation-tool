// NullMenu v5  -  作品(mod_*.cpp)とメニュー(NullMenu_v5.ino)で共通のもの
#pragma once
#include <Arduino.h>
#include <M5Unified.h>
#include <math.h>
#include <algorithm>
#include <type_traits>

// 1 にすると、描画の内側を高速メモリ(IRAM)に載せます。
// 22本ぶんは入らないので 0 のままにしてください("iram0_0_seg overflowed" と出ます)。
#define USE_IRAM 0
#if USE_IRAM
  #define MOD_IRAM IRAM_ATTR
#else
  #define MOD_IRAM
#endif

// 作品どうしで重ねて使う置き場(大きな配列用)。一度に動くのは1つだけなので、いちばん大きい作品のぶんだけ。
// 足りないと、書き込むときに「NM_ARENA_SIZE を大きくしてください」と出ます。
static constexpr size_t NM_ARENA_SIZE = 143104;
alignas(16) extern uint8_t NM_ARENA[NM_ARENA_SIZE];
static constexpr size_t nmAlignTo(size_t x, size_t a) { return (x + a - 1) / a * a; }

// 作品の中で「メモリが足りません」と出て止まったとき、メニューへ戻れるようにする
void nmIdle();

// 作品ごとの入口(mod_*.cpp の中にあります)
#define NM_DECL(ns) namespace ns { void setup(); void loop(); extern const size_t OVL_SIZE; }
NM_DECL(NullPavilion)  NM_DECL(NullEye)       NM_DECL(NullHorn)        NM_DECL(NullMirror)
NM_DECL(NullLife_Nuru) NM_DECL(NullLife_Curl) NM_DECL(NullVortex)
NM_DECL(NullGoodbye)   NM_DECL(NullBodhisattva) NM_DECL(NullSutra)     NM_DECL(NullButterfly)
NM_DECL(NullFloat)     NM_DECL(NullPond)      NM_DECL(NullPlasma)      NM_DECL(NullKintsugi)
NM_DECL(NullInfinity)  NM_DECL(NullTwin)      NM_DECL(NullDrop)        NM_DECL(NullTiles)
NM_DECL(NullZen)       NM_DECL(NullBokeh)     NM_DECL(NullGif)       NM_DECL(NullSlide)     NM_DECL(NullChronicle) NM_DECL(NullKaleido)
#undef NM_DECL
