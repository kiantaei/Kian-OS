// Kian OS GUI - pixel drawing primitives (integers only).
#include "gfx.h"

uint32_t blend(uint32_t dst, uint32_t src, int alpha) {
    if (alpha < 0) alpha = 0;
    if (alpha > 255) alpha = 255;
    uint32_t out = 0;
    for (int shift = 0; shift < 32; shift += 8) {
        uint32_t d = (dst >> shift) & 0xFF;
        uint32_t s = (src >> shift) & 0xFF;
        uint32_t v = (d * (255 - alpha) + s * alpha + 127) / 255;
        out |= (v & 0xFF) << shift;
    }
    return out;
}

void put_pixel(Surface &s, int x, int y, uint32_t c) {
    if (x < 0 || y < 0 || x >= s.w || y >= s.h) return;
    s.buf[(size_t)y * s.w + x] = c;
}

void fill_rect(Surface &s, int x, int y, int w, int h, uint32_t c) {
    for (int ry = y; ry < y + h; ry++) {
        if (ry < 0 || ry >= s.h) continue;
        for (int rx = x; rx < x + w; rx++) {
            if (rx < 0 || rx >= s.w) continue;
            s.buf[(size_t)ry * s.w + rx] = c;
        }
    }
}

void hline(Surface &s, int x, int y, int len, uint32_t c) {
    fill_rect(s, x, y, len, 1, c);
}

void vline(Surface &s, int x, int y, int len, uint32_t c) {
    fill_rect(s, x, y, 1, len, c);
}

void fill_round_rect(Surface &s, int x, int y, int w, int h, int r, uint32_t c) {
    if (r <= 0) { fill_rect(s, x, y, w, h, c); return; }
    if (r * 2 > w) r = w / 2;
    if (r * 2 > h) r = h / 2;
    fill_rect(s, x + r, y, w - 2 * r, h, c);
    fill_rect(s, x, y + r, r, h - 2 * r, c);
    fill_rect(s, x + w - r, y + r, r, h - 2 * r, c);
    // quarter circles (integer radius test)
    for (int dy = 0; dy <= r; dy++) {
        for (int dx = 0; dx <= r; dx++) {
            if (dx * dx + dy * dy <= r * r) {
                put_pixel(s, x + r - dx, y + r - dy, c);         // top-left
                put_pixel(s, x + w - 1 - r + dx, y + r - dy, c); // top-right
                put_pixel(s, x + r - dx, y + h - 1 - r + dy, c); // bottom-left
                put_pixel(s, x + w - 1 - r + dx, y + h - 1 - r + dy, c); // bottom-right
            }
        }
    }
}

void draw_text(Surface &s, int x, int y, const char *str, uint32_t c, int scale) {
    if (scale < 1) scale = 1;
    int ox = x;
    for (int i = 0; str[i] != '\0'; i++) {
        char ch = str[i];
        if (ch == '\n') { ox = x; y += 8 * scale; continue; }
        const uint8_t *g = kian_font_glyph(ch);
        for (int row = 0; row < 8; row++) {
            uint8_t bits = g[row];
            for (int col = 0; col < 8; col++) {
                if (bits & (0x80u >> col)) {
                    fill_rect(s, ox + col * scale, y + row * scale, scale, scale, c);
                }
            }
        }
        ox += 8 * scale;
    }
}

int text_width(const char *str, int scale) {
    int len = 0;
    while (str[len] != '\0') len++;
    return len * 8 * scale;
}
