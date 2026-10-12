// NullMenu v3 をブラウザで動かすための入口
#include "src/NullMenu_v3.ino"
#include "src/mod_nullpavilion.cpp"
#include "src/mod_nulleye.cpp"
#include "src/mod_nullhorn.cpp"
#include "src/mod_nullmirror.cpp"
#include "src/mod_nulllife_nuru.cpp"
#include "src/mod_nulllife_curl.cpp"
#include "src/mod_nullvortex.cpp"
#include "src/mod_nullgoodbye.cpp"
#include "src/mod_nullbodhisattva.cpp"
#include "src/mod_nullsutra.cpp"
#include "src/mod_nullbutterfly.cpp"
#include "src/mod_nullfloat.cpp"
#include "src/mod_nullpond.cpp"
#include "src/mod_nullplasma.cpp"
#include "src/mod_nullkintsugi.cpp"
#include "src/mod_nullinfinity.cpp"
#include "src/mod_nulltwin.cpp"
#include "src/mod_nulldrop.cpp"
#include "src/mod_nulltiles.cpp"
#include "src/mod_nullzen.cpp"
#include "src/mod_nullbokeh.cpp"
#include "src/mod_nullgif.cpp"
#include "src/mod_nullslide.cpp"
#include "src/mod_nullchronicle.cpp"

WEXP("web_setup") void web_setup(){ setup(); }
WEXP("web_loop")  void web_loop(){ loop(); }
WEXP("web_rtc_get") uint32_t web_rtc_get(int i){ return i==0 ? nmMagic : i==1 ? nmRun : nmSel; }
WEXP("web_rtc_set") void web_rtc_set(int i, uint32_t v){ if(i==0) nmMagic=v; else if(i==1) nmRun=v; else nmSel=v; }
WEXP("web_boot") void web_boot(int reason, int w, int h, int imu, int mic, uint32_t seed){
  g_resetReason=reason; g_w=w; g_h=h; g_imuOn=imu; g_micOn=mic; g_rand = seed | 1; srand(seed); }
WEXP("web_fb") uint16_t* web_fb(){ return g_fb; }
WEXP("web_w") int web_w(){ return g_w; }
WEXP("web_h") int web_h(){ return g_h; }
WEXP("web_running") int web_running(){ return running; }
WEXP("web_nitem") int web_nitem(){ return NITEM; }
WEXP("web_item_name") const char* web_item_name(int i){ return ITEMS[i].name; }
WEXP("web_touch") void web_touch(int pr, int x, int y){ g_touchRaw=pr; g_touchX=x; g_touchY=y; }
WEXP("web_btn") void web_btn(int id, int down){ *g_btnRaw[id] = down; }
WEXP("web_accel") void web_accel(float x, float y, float z){ g_ax=x; g_ay=y; g_az=z; }
WEXP("web_alloc") void* web_alloc(int n){ return malloc(n); }
WEXP("web_fs_put") void web_fs_put(const char* path, const uint8_t* d, int n){ fsPut(path, d, (size_t)n); }   // NullGif / NullSlide 用: GIF や写真を本体の置き場へ
