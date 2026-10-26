// drawJpg の代わり(M5GFX と同じ TJpgDec = lgfx_tjpgd.c で読む)。出す先は「バイトを入れ替えた 565」の絵
#pragma once
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include "lgfx_tjpgd.h"
struct WJpgIn { const uint8_t* p; uint32_t n, pos; uint16_t* dst; int dw, dh, x, y; float sc; uint8_t* full; int fw; };
static uint32_t wjpg_in(void* dev, uint8_t* b, uint32_t len) {
  WJpgIn* s = (WJpgIn*)dev; uint32_t k = len < s->n - s->pos ? len : s->n - s->pos;
  if (b) memcpy(b, s->p + s->pos, k); s->pos += k; return k; }
static uint32_t wjpg_out(void* dev, void* bmp, JRECT* r) {
  WJpgIn* s = (WJpgIn*)dev; const uint8_t* c = (const uint8_t*)bmp;
  for (uint32_t yy = r->top; yy <= r->bottom; ++yy) for (uint32_t xx = r->left; xx <= r->right; ++xx, c += 3)
    memcpy(s->full + ((size_t)yy * s->fw + xx) * 3, c, 3);
  return 1; }
static bool wDrawJpg(uint16_t* dst, int dw, int dh, const uint8_t* d, uint32_t n, int x, int y, float sc) {
  if (!dst || !d || n < 4) return false;
  WJpgIn s{d, n, 0, dst, dw, dh, x, y, sc, nullptr, 0};
  lgfxJdec jd; static uint8_t work[4096];
  if (lgfx_jd_prepare(&jd, wjpg_in, work, sizeof(work), &s) != JDR_OK) return false;
  s.fw = jd.width; s.full = (uint8_t*)calloc((size_t)jd.width * jd.height, 3);
  if (!s.full) return false;
  const bool ok = lgfx_jd_decomp(&jd, wjpg_out, 0) == JDR_OK;
  if (ok) {
    if (sc <= 0) sc = 1;
    const int ow = (int)(jd.width * sc), oh = (int)(jd.height * sc);
    for (int j = 0; j < oh; ++j) { int Y = y + j; if (Y < 0 || Y >= dh) continue; int sy = (int)(j / sc); if (sy >= jd.height) sy = jd.height - 1;
      for (int i = 0; i < ow; ++i) { int X = x + i; if (X < 0 || X >= dw) continue; int sx = (int)(i / sc); if (sx >= jd.width) sx = jd.width - 1;
        const uint8_t* c = s.full + ((size_t)sy * jd.width + sx) * 3;
        uint16_t v = (uint16_t)(((c[0] & 0xF8) << 8) | ((c[1] & 0xFC) << 3) | (c[2] >> 3));
        dst[(size_t)Y * dw + X] = (uint16_t)((v << 8) | (v >> 8)); } }
  }
  free(s.full);
  return ok; }
