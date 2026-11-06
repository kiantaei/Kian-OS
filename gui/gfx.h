// Kian OS GUI - pixel drawing primitives (integers only, 16.16 fixed point
// where fractional math is needed - NO float/double).
#pragma once
#include <kian_abi.h>

struct Surface {
    uint32_t *buf;   // back buffer, stride == w
    int w, h;
};

struct Rect { int x, y, w, h; };

// 16.16 fixed point helpers (integer only)
inline int32_t fx_from_int(int32_t v) { return v << 16; }
inline int32_t fx_mul(int32_t a, int32_t b) { return (int32_t)(((int64_t)a * (int64_t)b) >> 16); }
inline int32_t fx_div(int32_t a, int32_t b) { return (int32_t)(((int64_t)a << 16) / b); }

uint32_t blend(uint32_t dst, uint32_t src, int alpha); // alpha 0..255, integer math

void put_pixel(Surface &s, int x, int y, uint32_t c);
void fill_rect(Surface &s, int x, int y, int w, int h, uint32_t c);
void hline(Surface &s, int x, int y, int len, uint32_t c);
void vline(Surface &s, int x, int y, int len, uint32_t c);
void fill_round_rect(Surface &s, int x, int y, int w, int h, int r, uint32_t c);
void draw_text(Surface &s, int x, int y, const char *str, uint32_t c, int scale);
int  text_width(const char *str, int scale);
