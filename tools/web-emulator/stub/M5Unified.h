// ブラウザ用の M5Unified の代わり(NullMenu v3 を WebAssembly で動かすため)
#pragma once
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <cstdio>
#include <algorithm>
using std::min; using std::max;
#include "jpgstub.h"
inline void yield(){}

#define WEXP(n) extern "C" __attribute__((export_name(n)))
#define WIMP(n) extern "C" __attribute__((import_module("env"), import_name(n)))
WIMP("js_yield")   void js_yield();          // もう片方のコアへ(asyncify で中断)
WIMP("js_frame")   void js_frame();          // 次のフレームまで待つ(asyncify で中断)
WIMP("js_now")     double js_now();          // ミリ秒(小数)
WIMP("js_restart") void js_restart();        // 再起動(JS 側で作り直す)
WIMP("js_bright")  void js_bright(int v);

// ---- メモリ
#define MALLOC_CAP_INTERNAL 1
#define MALLOC_CAP_8BIT 2
#define MALLOC_CAP_DMA 4
#define MALLOC_CAP_SPIRAM 8
inline void* heap_caps_malloc(size_t n,int){return malloc(n);}
inline void* heap_caps_calloc(size_t n,size_t s,int){return calloc(n,s);}
inline void  heap_caps_free(void* p){ free(p); }
inline void* heap_caps_aligned_alloc(size_t a,size_t n,int){return aligned_alloc(a,(n+a-1)/a*a);}
inline void* ps_malloc(size_t n){return malloc(n);}
inline void* ps_calloc(size_t a,size_t b){return calloc(a,b);}
inline size_t heap_caps_get_free_size(int){ return 250 * 1024; }
inline size_t heap_caps_get_largest_free_block(int){ return 250 * 1024; }
static uint32_t g_rand = 0x12345678;
inline uint32_t esp_random(){ g_rand ^= g_rand << 13; g_rand ^= g_rand >> 17; g_rand ^= g_rand << 5; return g_rand; }

// ---- 時間
inline uint32_t millis(){ return (uint32_t)js_now(); }
inline uint32_t micros(){ return (uint32_t)(js_now() * 1000.0); }
inline void delay(int){}
inline void vTaskDelay(int){}

// ---- 2つ目のコア(1本の流れで、交互に動かす)
static void (*g_taskFn)(void*) = nullptr; static void* g_taskArg = nullptr;
static inline void nmYield(){ if (g_taskFn) js_yield(); }
static inline void nmWebFrame(){ js_frame(); }
WEXP("nm_task_entry") void nm_task_entry(){ if (g_taskFn) g_taskFn(g_taskArg); }
WEXP("nm_has_task") int nm_has_task(){ return g_taskFn != nullptr; }
struct WSem { volatile int c = 0; };
typedef WSem* SemaphoreHandle_t;
#define portMAX_DELAY 0
inline SemaphoreHandle_t xSemaphoreCreateBinary(){ return new WSem(); }
inline int xSemaphoreTake(SemaphoreHandle_t s,int){ while(!s->c) nmYield(); s->c = 0; return 1; }
inline int xSemaphoreGive(SemaphoreHandle_t s){ s->c = 1; return 1; }
inline void vSemaphoreDelete(SemaphoreHandle_t s){ delete s; }
typedef void* TaskHandle_t;
#ifndef pdPASS
#define pdPASS 1
#endif
inline int xTaskCreatePinnedToCore(void(*f)(void*),const char*,int,void* a,int,TaskHandle_t* h,int){
  g_taskFn = f; g_taskArg = a; if (h) *h = (TaskHandle_t)1; return 1; }
inline void vTaskDelete(TaskHandle_t){}

// ---- Serial(printf はブラウザのログへ)
struct Ser{ void begin(int){} template<class...A> void printf(const char* f, A... a){ ::printf(f, a...); fflush(stdout); }
  void println(const char* s){ puts(s); fflush(stdout); } void print(const char* s){ fputs(s, stdout); }
  void setRxBufferSize(size_t){} int available(){ return 0; } int read(){ return -1; } size_t read(uint8_t*, size_t){ return 0; } } Serial;   // ブラウザには USB の受け口はない
enum { top_left = 0, middle_center = 4, middle_left = 6 };

// ---- 文字(PC で作った字形の表)
struct WGlyph { uint32_t cp; uint16_t font; uint8_t w, h; uint32_t off; };
#include "glyphs_web.inc"   // WG_TAB[], WG_N, WG_BITS[]
static const WGlyph* wglyph(uint32_t cp, int font){
  int lo = 0, hi = WG_N - 1; uint64_t key = ((uint64_t)cp << 16) | (uint32_t)font;
  while (lo <= hi){ int m = (lo + hi) / 2; uint64_t k = ((uint64_t)WG_TAB[m].cp << 16) | WG_TAB[m].font;
    if (k == key) return &WG_TAB[m]; if (k < key) lo = m + 1; else hi = m - 1; }
  return nullptr; }

// ---- 描画(画面とスプライトで共通)。中身は本体と同じ「バイトを入れ替えた 565」
static inline uint16_t wswap(uint16_t c){ return (uint16_t)((c << 8) | (c >> 8)); }
struct Gfx {
  bool drawJpg(const uint8_t* d, uint32_t n, int x=0, int y=0, int=0, int=0, int=0, int=0, float sx=1.0f, float=0.0f){ return wDrawJpg(buf, w, h, d, n, x, y, sx); }
  uint16_t* buf=nullptr; int w=0,h=0; uint16_t tcol=0xFFFF; int datum=0; int font=16; float tsize=1;
  inline void px(int x,int y,uint16_t c){ if(!buf||x<0||y<0||x>=w||y>=h) return; buf[y*w+x]=wswap(c); }
  void fillScreen(uint16_t c){ if(!buf) return; uint16_t s=wswap(c); for(int i=0;i<w*h;++i) buf[i]=s; }
  void fillRect(int x,int y,int ww,int hh,uint16_t c){ for(int j=0;j<hh;++j) for(int i=0;i<ww;++i) px(x+i,y+j,c); }
  void fillRoundRect(int x,int y,int ww,int hh,int r,uint16_t c){ for(int j=0;j<hh;++j){ int d=0; int dy=j<r?r-j:(j>=hh-r?j-(hh-r-1):0); if(dy){ float q=r*r-(float)dy*dy; d=r-(q>0?(int)__builtin_sqrtf(q):0);} for(int i=d;i<ww-d;++i) px(x+i,y+j,c);} }
  void drawFastHLine(int x,int y,int ww,uint16_t c){ for(int i=0;i<ww;++i) px(x+i,y,c); }
  void drawPixel(int x,int y,uint16_t c){ px(x,y,c); }
  void drawLine(int x0,int y0,int x1,int y1,uint16_t c){
    int dx=abs(x1-x0), dy=-abs(y1-y0), sx=x0<x1?1:-1, sy=y0<y1?1:-1, err=dx+dy;
    for(;;){ px(x0,y0,c); if(x0==x1&&y0==y1) break; int e2=2*err;
      if(e2>=dy){err+=dy;x0+=sx;} if(e2<=dx){err+=dx;y0+=sy;} } }
  void drawCircle(int cx,int cy,int r,uint16_t c){          // 中点円
    if(r<0) return; int x=r, y=0, e=1-r;
    while(x>=y){ px(cx+x,cy+y,c);px(cx+y,cy+x,c);px(cx-y,cy+x,c);px(cx-x,cy+y,c);
                 px(cx-x,cy-y,c);px(cx-y,cy-x,c);px(cx+y,cy-x,c);px(cx+x,cy-y,c);
                 ++y; if(e<0) e+=2*y+1; else { --x; e+=2*(y-x)+1; } } }
  void fillCircle(int cx,int cy,int r,uint16_t c){
    for(int y=-r;y<=r;++y){ int dx=(int)sqrtf((float)(r*r-y*y)); for(int x=-dx;x<=dx;++x) px(cx+x,cy+y,c); } }
  void drawTriangle(int x0,int y0,int x1,int y1,int x2,int y2,uint16_t c){
    drawLine(x0,y0,x1,y1,c); drawLine(x1,y1,x2,y2,c); drawLine(x2,y2,x0,y0,c); }
  void fillTriangle(int x0,int y0,int x1,int y1,int x2,int y2,uint16_t c){
    int mny=std::min({y0,y1,y2}), mxy=std::max({y0,y1,y2});
    if(mny<0) mny=0; if(mxy>=h) mxy=h-1;
    for(int y=mny;y<=mxy;++y){ float xa=1e9f, xb=-1e9f; int P[3][2]={{x0,y0},{x1,y1},{x2,y2}};
      for(int e=0;e<3;++e){ int*A=P[e],*B=P[(e+1)%3]; if((A[1]<=y&&B[1]>=y)||(B[1]<=y&&A[1]>=y)){
          if(A[1]==B[1]){ xa=std::min(xa,(float)std::min(A[0],B[0])); xb=std::max(xb,(float)std::max(A[0],B[0])); }
          else { float x=A[0]+(float)(y-A[1])*(B[0]-A[0])/(B[1]-A[1]); xa=std::min(xa,x); xb=std::max(xb,x); } } }
      for(int x=(int)ceilf(xa-0.5f); x<=(int)floorf(xb+0.5f); ++x) px(x,y,c); } }
  void setTextDatum(int d){ datum=d; }
  void setTextColor(uint16_t c){ tcol=c; }
  void setTextColor(uint16_t c, uint16_t){ tcol=c; }
  void setFont(const int* f){ font = f ? *f : 16; }
  void setTextSize(float k){ tsize = k<=0 ? 1 : k; }
  int fontPx() const { return font % 1000; }
  int textWidth(const char* s){
    int tw=0;
    for(const unsigned char* p=(const unsigned char*)s;*p;){ uint32_t c=*p;
      if(c<0x80){++p;} else if(c<0xE0){ c=((c&0x1F)<<6)|(p[1]&0x3F); p+=2; }
      else if(c<0xF0){ c=((c&0x0F)<<12)|((p[1]&0x3F)<<6)|(p[2]&0x3F); p+=3; } else { c=((c&7)<<18)|((p[1]&0x3F)<<12)|((p[2]&0x3F)<<6)|(p[3]&0x3F); p+=4; }
      const WGlyph* g=wglyph(c,font); tw += (int)((g? g->w : fontPx()/2)*tsize); }
    return tw; }
  void drawString(const char* s,int x,int y){
    uint32_t cps[256]; int n=0;
    for(const unsigned char* p=(const unsigned char*)s;*p && n<256;){ uint32_t c=*p;
      if(c<0x80){++p;} else if(c<0xE0){ c=((c&0x1F)<<6)|(p[1]&0x3F); p+=2; }
      else if(c<0xF0){ c=((c&0x0F)<<12)|((p[1]&0x3F)<<6)|(p[2]&0x3F); p+=3; } else { c=((c&7)<<18)|((p[1]&0x3F)<<12)|((p[2]&0x3F)<<6)|(p[3]&0x3F); p+=4; }
      cps[n++]=c; }
    int tw=0; for(int i=0;i<n;++i){ const WGlyph* g=wglyph(cps[i],font); tw += (int)((g? g->w : fontPx()/2)*tsize); }
    const int ch=(int)(fontPx()*tsize+0.5f);
    int x0=(datum==4? x-tw/2 : x), y0=((datum==4||datum==6)? y-ch/2 : y);
    for(int i=0;i<n;++i){ const WGlyph* g=wglyph(cps[i],font); if(!g){ x0+=(int)(fontPx()/2*tsize); continue; }
      int gw=(int)(g->w*tsize);
      for(int j=0;j<ch;++j) for(int ii=0;ii<gw;++ii){ int sj=(int)(j/tsize), si=(int)(ii/tsize);
        if(sj<g->h && si<g->w){ uint32_t bit=g->off + sj*g->w + si; if(WG_BITS[bit>>3] & (1u<<(bit&7))) px(x0+ii, y0+j, tcol); } }
      x0+=gw; }
  }
};
namespace fonts { static const int lgfxJapanGothicP_16=16, lgfxJapanGothicP_20=20, lgfxJapanGothicP_24=24,
  lgfxJapanMincho_20=1020, FreeSansBold18pt7b=3034, FreeSansBold12pt7b=3022; }

static int g_w=468, g_h=468;
static uint16_t* g_fb=nullptr;
namespace lgfx { typedef ::Gfx LGFXBase; }   // 本物の M5GFX と同じ名前で、画面と紙(Canvas)の共通の親
struct Disp : Gfx {
  int width(){return g_w;} int height(){return g_h;}
  void ensure(){ if(!g_fb) g_fb=(uint16_t*)calloc((size_t)g_w*g_h,2); buf=g_fb; w=g_w; h=g_h; }
  void setRotation(int){ if(g_w<g_h){int t=g_w;g_w=g_h;g_h=t;} }
  void fillScreen(int c){ ensure(); Gfx::fillScreen((uint16_t)c); }
  void drawString(const char* s,int x,int y){ ensure(); Gfx::drawString(s,x,y); }
  void setBrightness(int v){ js_bright(v); }
  uint16_t color565(int r,int g,int b){return ((r&0xF8)<<8)|((g&0xFC)<<3)|(b>>3);}
  void startWrite(){} void endWrite(){} void waitDMA(){} void display(){}
  void pushImageDMA(int x,int y,int ww,int hh,uint16_t* p){ pushImage(x,y,ww,hh,p); }
  void pushImage(int x,int y,int ww,int hh,const uint16_t* p){ ensure();
    for(int j=0;j<hh;++j){ int yy=y+j; if(yy<0||yy>=g_h) continue;
      for(int i=0;i<ww;++i){ int xx=x+i; if(xx<0||xx>=g_w) continue; g_fb[yy*g_w+xx]=p[j*ww+i]; } } }
  struct Pnl{ bool ad=false; bool getAutoDisplay(){return ad;} };
  Pnl pnl; Pnl* getPanel(){ pnl.ad = (g_w==g_h); return &pnl; }    // StopWatch と同じく、丸い画面は「置き場にためて送る」側
};
struct M5Canvas : Gfx {
  M5Canvas(void*){}
  void setPsram(bool){} void setColorDepth(int){}
  void* getBuffer(){ return buf; }
  void fillSprite(uint16_t c){ Gfx::fillScreen(c); }
  bool createSprite(int ww,int hh){ w=ww;h=hh; buf=(uint16_t*)calloc((size_t)ww*hh,2); return buf!=nullptr; }
  void deleteSprite(){ ::free(buf); buf=nullptr; }
  void pushSprite(int x,int y){ if(!g_fb) g_fb=(uint16_t*)calloc((size_t)g_w*g_h,2);
    for(int j=0;j<h;++j){ int yy=y+j; if(yy<0||yy>=g_h) continue;
      for(int i=0;i<w;++i){ int xx=x+i; if(xx<0||xx>=g_w) continue; g_fb[yy*g_w+xx]=buf[j*w+i]; } } }
  ~M5Canvas(){ if(buf) ::free(buf); }
};
#define TFT_BLACK 0
#define TFT_WHITE 0xFFFF

// ---- マイク: JS が 16kHz で書きこむ輪(声が無いときは 0)
static const int WMIC_N = 1 << 15;
static int16_t g_micRing[WMIC_N]; static volatile uint32_t g_micW = 0; static bool g_micOn = false;
static double g_micT0 = 0;
WEXP("web_mic_ring") int16_t* web_mic_ring(){ return g_micRing; }
WEXP("web_mic_set_w") void web_mic_set_w(uint32_t w){ g_micW = w; }
struct MicC {
  bool en=false; uint32_t pos=0;
  bool begin(){ en = g_micOn; pos = g_micW; g_micT0 = js_now(); return en; }
  bool isEnabled(){ return en; }
  size_t isRecording(){ if(!en) return 0; long ahead=(long)pos-(long)g_micW; return ahead>=2*512? 2 : (ahead>=512?1:0); }
  bool record(int16_t* d, size_t n, uint32_t){ if(!en || isRecording()>=2) return false;
    if ((long)g_micW - (long)pos > WMIC_N/2) pos = g_micW - n;      // 遅れすぎたら追いつく
    for(size_t i=0;i<n;++i){ uint32_t k=pos+i; d[i] = (k < g_micW) ? g_micRing[k & (WMIC_N-1)] : 0; } pos+=n; return true; }
};
struct SpkC { void end(){} bool begin(){return true;} };

// ---- ボタンとタッチ(M5Unified と同じく、update() 1回ごとに出来事を決める)
struct Btn {
  bool raw=false, down=false, prs=false, rel=false, clk=false, hld=false, held=false; uint32_t downT=0;
  void tick(uint32_t now){ prs=rel=clk=hld=false;
    if(raw && !down){ down=true; prs=true; downT=now; held=false; }
    else if(!raw && down){ down=false; rel=true; if(!held) clk=true; }
    if(down && !held && now-downT>=500){ held=true; hld=true; } }
  bool isPressed(){return down;} bool isReleased(){return !down;}
  bool pressedFor(uint32_t ms){ return down && millis()-downT>=ms; }
  bool wasPressed(){return prs;} bool wasReleased(){return rel;} bool wasClicked(){return clk;} bool wasHold(){return hld;}
};
struct TD{ bool pr=false, wp=false, wr=false; int x=0, y=0;
  bool isPressed(){return pr;} bool wasPressed(){return wp;} bool wasReleased(){return wr;} };
static bool g_touchRaw=false; static int g_touchX=0, g_touchY=0;
struct Tch{ TD d; TD getDetail(){return d;}
  void tick(){ bool was=d.pr; d.pr=g_touchRaw; d.wp=d.pr&&!was; d.wr=!d.pr&&was; if(d.pr){ d.x=g_touchX; d.y=g_touchY; } } };
struct Cfg{};
static bool g_imuOn = true; static float g_ax = 0, g_ay = 0, g_az = 1;
struct ImuC { bool isEnabled() const { return g_imuOn; } int update(){ return 1; }
  bool getAccel(float* x, float* y, float* z){ *x = g_ax; *y = g_ay; *z = g_az; return true; } };
static int g_batt = 80, g_charging = 0;      // 電池(ページの「電池」から変えられる)
struct PwrC { int getBatteryLevel(){ return g_batt; } int isCharging(){ return g_charging; } };
WEXP("web_battery") void web_battery(int level, int charging){ g_batt = level; g_charging = charging; }
struct M5c{ Disp Display; Btn BtnPWR, BtnA, BtnB; Tch Touch; ImuC Imu; MicC Mic; SpkC Speaker; PwrC Power;
  Cfg config(){return{};} void begin(Cfg){ Display.ensure(); }
  void update(){ uint32_t n=millis(); BtnA.tick(n); BtnB.tick(n); BtnPWR.tick(n); Touch.tick(); } } M5;
static bool* g_btnRaw[3] = { &M5.BtnA.raw, &M5.BtnB.raw, &M5.BtnPWR.raw };

// ---- 再起動・RTC
#define RTC_NOINIT_ATTR
typedef int esp_reset_reason_t; enum { ESP_RST_POWERON=1, ESP_RST_SW=3, ESP_RST_PANIC=4 };
static int g_resetReason = ESP_RST_POWERON;
inline esp_reset_reason_t esp_reset_reason(){ return g_resetReason; }
[[noreturn]] inline void esp_restart(){ js_restart(); __builtin_trap(); }
typedef int esp_err_t; enum { ESP_OK=0 };
inline esp_err_t heap_caps_add_region(intptr_t, intptr_t){ return ESP_OK; }
#define IRAM_ATTR
