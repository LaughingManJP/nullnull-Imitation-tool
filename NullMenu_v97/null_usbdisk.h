// null_usbdisk.h  -  GIF・写真の区画(ffat)を、PC から USB メモリとして見せる(NullGif / NullSlide で使う)
//   使い方: nudRun("GIF") を呼ぶと、画面に説明を出して USB メモリになる。PC で「取り出し」をするか、
//   本体のボタンを2回押すと、再起動して元の作品に戻る(この関数からは戻らない)。
//   書き込みの設定(USB Mode「Hardware CDC and JTAG」)はそのままで動くように、選んだときだけ USB の使い方を切りかえる。
//   USB メモリになっているあいだは、本体は区画を使わない(同時に書きかえると中身がこわれるので)。
#pragma once
#include <Arduino.h>
#include <M5Unified.h>
#include <FFat.h>
#if defined(ESP_PLATFORM)
#include <soc/soc_caps.h>
#include <sdkconfig.h>
#endif

#if defined(ESP_PLATFORM) && SOC_USB_OTG_SUPPORTED && CONFIG_TINYUSB_MSC_ENABLED
#include <USB.h>
#include <USBMSC.h>
#include <esp_partition.h>
#include <wear_levelling.h>
#define NUD_AVAILABLE 1

namespace nud {
static wl_handle_t wl = WL_INVALID_HANDLE;
static uint32_t secSz = 4096;                      // 1ブロックの大きさ(区画の作り = 4096)
static uint32_t total = 0;                         // 区画の大きさ(バイト)
static uint8_t* rmw = nullptr;                     // ブロックの一部だけ書くときの置き場
static volatile bool ejected = false;
static volatile uint32_t lastIo = 0;
static USBMSC msc;

static int32_t onRead(uint32_t lba, uint32_t off, void* buf, uint32_t n) {
  lastIo = millis();
  const uint64_t a = (uint64_t)lba * secSz + off;
  if (a + n > total) return -1;                    // 区画の外は読ませない
  return wl_read(wl, (size_t)a, buf, n) == ESP_OK ? (int32_t)n : -1;
}
static int32_t onWrite(uint32_t lba, uint32_t off, uint8_t* buf, uint32_t n) {
  lastIo = millis();
  const uint64_t base = (uint64_t)lba * secSz;
  if (base + off + n > total || off + n > secSz) return -1;   // 区画の外へは書かせない
  if (off == 0 && n == secSz) {                    // ブロックまるごと
    if (wl_erase_range(wl, (size_t)base, secSz) != ESP_OK) return -1;
    return wl_write(wl, (size_t)base, buf, n) == ESP_OK ? (int32_t)n : -1;
  }
  if (!rmw) return -1;                             // ブロックの一部: 読んで、まぜて、書き直す
  if (wl_read(wl, (size_t)base, rmw, secSz) != ESP_OK) return -1;
  memcpy(rmw + off, buf, n);
  if (wl_erase_range(wl, (size_t)base, secSz) != ESP_OK) return -1;
  return wl_write(wl, (size_t)base, rmw, secSz) == ESP_OK ? (int32_t)n : -1;
}
static bool onStartStop(uint8_t power, bool start, bool loadEject) {
  (void)power;
  if (loadEject && !start) ejected = true;         // PC で「取り出し」をした
  return true;
}

static void drawTo(lgfx::LovyanGFX& d, int state) {   // 説明の画面を描く(画面か、画面外の絵に)
  const int W = d.width(), H = d.height();
  const bool big = min(W, H) >= 400;
  const int lh = big ? 27 : 19;
  d.fillScreen(TFT_BLACK);
  d.setTextDatum(middle_center);
  int y = H / 2 - lh * 5 - lh / 2 - (big ? 6 : 2);
  d.setFont(big ? &fonts::lgfxJapanGothicP_24 : &fonts::lgfxJapanGothicP_20);
  d.setTextColor(0x7F38);
  d.drawString("USB メモリになっています", W / 2, y); y += lh + (big ? 10 : 4);
  d.setFont(&fonts::lgfxJapanGothicP_16);
  d.setTextColor(TFT_WHITE);
  d.drawString("PC に USB メモリが出てきます", W / 2, y); y += lh;
  d.drawString("gif フォルダに GIF、", W / 2, y); y += lh;
  d.drawString("slide フォルダに写真を入れる", W / 2, y); y += lh;
  d.setTextColor(0x7BEF);
  d.drawString("写真は JPG だけ(600KB まで)", W / 2, y); y += lh;
  d.drawString("PNG・iPhone の写真は Wi-Fi で送る", W / 2, y); y += lh;
  d.drawString("*.cfg は設定(消すと元にもどる)", W / 2, y); y += lh;
  d.setTextColor(0xAD55);
  d.drawString("終わったら PC で「取り出し」", W / 2, y); y += lh;
  d.drawString("(本体が再起動して、もどります)", W / 2, y); y += lh + (big ? 6 : 2);
  d.setTextColor(state == 1 ? 0xFE60 : 0x7BEF);
  static const char* const ST[4] = {"", "PC が読み書きしています…", "もう一度押すと、もどります", "もどります…"};
  const bool round = W == H;
  d.drawString(state ? ST[state] : (round ? "ボタン2回: やめる" : "2回タップ: やめる"), W / 2, y);
}
static void draw(const char* what, int state) {   // state: 0 = 待っている / 1 = 読み書き中 / 2 = もう一度押すと戻る / 3 = 戻ります
  (void)what;
  // 画面外の絵(PSRAM)に描いてから、画面ぜんぶをまとめて送る。
  // StopWatch で画面へ直接描く作品(NullSlide)は、横2点ずつそろえた書き込みしか出ないので、文字を直接描くと消えてしまうため
  M5Canvas cv(&M5.Display);
  cv.setPsram(true);
  cv.setColorDepth(16);
  if (cv.createSprite(M5.Display.width(), M5.Display.height())) {
    drawTo(cv, state);
    M5.Display.startWrite();
    cv.pushSprite(0, 0);
    M5.Display.endWrite();
    cv.deleteSprite();
    return;
  }
  M5.Display.startWrite(); drawTo(M5.Display, state); M5.Display.endWrite();   // 置き場がないときは、直接
}

// USB メモリになる(戻らない)。区画が見つからないときだけ false で戻る
static bool run(const char* what) {
  FFat.end();                                      // 本体からは使わない
  const esp_partition_t* p = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_FAT, "ffat");
  if (!p || wl_mount(p, &wl) != ESP_OK) { FFat.begin(true); return false; }
  secSz = (uint32_t)wl_sector_size(wl); total = (uint32_t)wl_size(wl);
  rmw = (uint8_t*)malloc(secSz);
  Serial.printf("# usb disk: %u blocks x %u B\n", (unsigned)(total / secSz), (unsigned)secSz);
  Serial.flush();
  draw(what, 0);
  msc.vendorID("NullMenu"); msc.productID("GIF and Photos"); msc.productRevision("1.0");
  msc.onRead(onRead); msc.onWrite(onWrite); msc.onStartStop(onStartStop);
  msc.mediaPresent(true);
  msc.begin(total / secSz, (uint16_t)secSz);
  USB.begin();                                     // ここから USB は「USB メモリ」になる(シリアルモニタは切れる)
  int state = 0; uint32_t armT = 0;
  for (;;) {
    M5.update();
    const uint32_t now = millis();
    bool press = M5.BtnA.wasClicked() || M5.BtnB.wasClicked() || M5.BtnPWR.wasClicked();
    if (M5.Display.width() != M5.Display.height()) press = press || M5.Touch.getDetail().wasPressed();
    const int busy = (lastIo && now - lastIo < 800) ? 1 : 0;
    int want = state == 2 ? 2 : busy;
    if (press) {
      if (state == 2 && now - armT < 4000) want = 3;
      else { want = 2; armT = now; }
    }
    if (state == 2 && now - armT >= 4000) want = busy;
    if (ejected) want = 3;
    if (want != state) { state = want; draw(what, state); }
    if (state == 3) break;
    delay(20);
  }
  const uint32_t t0 = millis();                    // 書きかけがあれば、少し待ってから
  while (lastIo && millis() - lastIo < 600 && millis() - t0 < 3000) delay(20);
  wl_unmount(wl);
  delay(300);
  M5.Display.setBrightness(0);
  esp_restart();                                   // 元の作品で起動し直す(メニューからなら、同じ作品に戻る)
  return true;
}
}  // namespace nud

#else
// この環境(ブラウザのエミュレータなど)では USB メモリになれないので、説明だけ出す
#define NUD_AVAILABLE 0
namespace nud {
static bool run(const char* what) {
  (void)what;
  const int W = M5.Display.width(), H = M5.Display.height();
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setTextDatum(middle_center);
  M5.Display.setFont(&fonts::lgfxJapanGothicP_16);
  M5.Display.setTextColor(TFT_WHITE);
  M5.Display.drawString("USB メモリになるのは", W / 2, H / 2 - 20);
  M5.Display.drawString("本体(StopWatch / CoreS3)だけです", W / 2, H / 2 + 8);
  delay(2000);
  return false;
}
}  // namespace nud
#endif
