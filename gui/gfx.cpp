// Kian OS GUI - gfx implementation. Integers only (mgeneral-regs-only: the
// compiler would reject float/double anyway). 16.16 fixed point where needed.
#include "gfx.h"
#include "util.h"

Gfx *GFX = 0;
static Gfx g_state;          // POD, no constructor -> allowed as static storage

static Rect CLIP = { 0, 0, 0, 0 };

static inline bool clip_ok(int x, int y) {
    return x >= CLIP.x && x < CLIP.x + CLIP.w && y >= CLIP.y && y < CLIP.y + CLIP.h;
}

bool gfx_init(const kian_fb_t *fb) {
    g_state.w = (int)fb->width;
    g_state.h = (int)fb->height;
    g_state.fb = (uint32_t *)(uintptr_t)fb->addr;
    g_state.pitch = (int)fb->pitch;
    g_state.frame = (uint32_t *)kian_alloc((size_t)g_state.w * (size_t)g_state.h * 4, 16);
    g_state.scene = (uint32_t *)kian_alloc((size_t)g_state.w * (size_t)g_state.h * 4, 16);
    if (g_state.frame == 0 || g_state.scene == 0) {
        if (g_state.frame) { kian_free(g_state.frame); g_state.frame = 0; }
        if (g_state.scene) { kian_free(g_state.scene); g_state.scene = 0; }
        GFX = 0;
        return false;
    }
    CLIP.x = 0; CLIP.y = 0; CLIP.w = g_state.w; CLIP.h = g_state.h;
    GFX = &g_state;
    return true;
}

void gfx_free() {
    if (g_state.frame) { kian_free(g_state.frame); g_state.frame = 0; }
    if (g_state.scene) { kian_free(g_state.scene); g_state.scene = 0; }
    GFX = 0;
}

void clip_set(Rect r) {
    if (!GFX) return;
    int x0 = imax(0, r.x), y0 = imax(0, r.y);
    int x1 = imin(GFX->w, r.x + r.w), y1 = imin(GFX->h, r.y + r.h);
    CLIP.x = x0; CLIP.y = y0;
    CLIP.w = (x1 > x0) ? x1 - x0 : 0;
    CLIP.h = (y1 > y0) ? y1 - y0 : 0;
}
void clip_reset() {
    if (!GFX) return;
    CLIP.x = 0; CLIP.y = 0; CLIP.w = GFX->w; CLIP.h = GFX->h;
}
Rect clip_get() { return CLIP; }

void gp(int x, int y, uint32_t c) {
    if (!GFX || !clip_ok(x, y)) return;
    GFX->frame[(size_t)y * GFX->w + x] = c;
}

void fill_rect(int x, int y, int w, int h, uint32_t c) {
    if (!GFX || w <= 0 || h <= 0) return;
    int x0 = imax(x, CLIP.x), y0 = imax(y, CLIP.y);
    int x1 = imin(x + w, CLIP.x + CLIP.w), y1 = imin(y + h, CLIP.y + CLIP.h);
    if (x1 <= x0 || y1 <= y0) return;
    for (int ry = y0; ry < y1; ry++) {
        uint32_t *row = GFX->frame + (size_t)ry * GFX->w + x0;
        for (int rx = x0; rx < x1; rx++) row[rx - x0] = c;
    }
}

void hline(int x, int y, int len, uint32_t c) { fill_rect(x, y, len, 1, c); }
void vline(int x, int y, int len, uint32_t c) { fill_rect(x, y, 1, len, c); }

void rect_outline(int x, int y, int w, int h, uint32_t c) {
    if (w <= 0 || h <= 0) return;
    hline(x, y, w, c); hline(x, y + h - 1, w, c);
    vline(x, y, h, c); vline(x + w - 1, y, h, c);
}

void fill_round_rect(int x, int y, int w, int h, int r, uint32_t c) {
    if (!GFX || w <= 0 || h <= 0) return;
    if (r <= 0) { fill_rect(x, y, w, h, c); return; }
    if (r * 2 > w) r = w / 2;
    if (r * 2 > h) r = h / 2;
    fill_rect(x + r, y, w - 2 * r, h, c);
    fill_rect(x, y + r, r, h - 2 * r, c);
    fill_rect(x + w - r, y + r, r, h - 2 * r, c);
    for (int dy = 0; dy <= r; dy++) {
        for (int dx = 0; dx <= r; dx++) {
            if (dx * dx + dy * dy <= r * r) {
                gp(x + r - dx, y + r - dy, c);                  // top-left
                gp(x + w - 1 - r + dx, y + r - dy, c);          // top-right
                gp(x + r - dx, y + h - 1 - r + dy, c);          // bottom-left
                gp(x + w - 1 - r + dx, y + h - 1 - r + dy, c);  // bottom-right
            }
        }
    }
}

void round_rect_outline(int x, int y, int w, int h, int r, uint32_t c) {
    if (w <= 2 || h <= 2) return;
    if (r <= 0) { rect_outline(x, y, w, h, c); return; }
    if (r * 2 > w) r = w / 2;
    if (r * 2 > h) r = h / 2;
    hline(x + r, y, w - 2 * r, c);
    hline(x + r, y + h - 1, w - 2 * r, c);
    vline(x, y + r, h - 2 * r, c);
    vline(x + w - 1, y + r, h - 2 * r, c);
    for (int d = 0; d <= r; d++) {
        for (int e = 0; e <= r; e++) {
            int dd = d * d + e * e;
            if (dd <= r * r && dd >= (r - 1) * (r - 1)) {
                gp(x + r - d, y + r - e, c);
                gp(x + w - 1 - r + d, y + r - e, c);
                gp(x + r - d, y + h - 1 - r + e, c);
                gp(x + w - 1 - r + d, y + h - 1 - r + e, c);
            }
        }
    }
}

void fill_circle(int cx, int cy, int r, uint32_t c) {
    if (!GFX || r <= 0) return;
    for (int dy = -r; dy <= r; dy++) {
        for (int dx = -r; dx <= r; dx++) {
            if (dx * dx + dy * dy <= r * r) gp(cx + dx, cy + dy, c);
        }
    }
}

void circle_outline(int cx, int cy, int r, uint32_t c) {
    if (!GFX || r <= 0) return;
    for (int dy = -r; dy <= r; dy++) {
        for (int dx = -r; dx <= r; dx++) {
            int dd = dx * dx + dy * dy;
            if (dd <= r * r && dd >= (r - 1) * (r - 1)) gp(cx + dx, cy + dy, c);
        }
    }
}

static inline uint32_t mix(uint32_t d, uint32_t s, int a) {
    uint32_t out = 0;
    for (int shift = 0; shift < 32; shift += 8) {
        uint32_t dc = (d >> shift) & 0xFF, sc = (s >> shift) & 0xFF;
        uint32_t v = (dc * (255 - a) + sc * a + 127) / 255;
        out |= (v & 0xFF) << shift;
    }
    return out;
}

void blend_pixel(int x, int y, uint32_t c, int alpha) {
    if (!GFX || !clip_ok(x, y)) return;
    if (alpha <= 0) return;
    if (alpha > 255) alpha = 255;
    uint32_t *p = &GFX->frame[(size_t)y * GFX->w + x];
    *p = mix(*p, c, alpha);
}

void blend_rect(int x, int y, int w, int h, uint32_t c, int alpha) {
    if (!GFX || alpha <= 0 || w <= 0 || h <= 0) return;
    int x0 = imax(x, CLIP.x), y0 = imax(y, CLIP.y);
    int x1 = imin(x + w, CLIP.x + CLIP.w), y1 = imin(y + h, CLIP.y + CLIP.h);
    for (int ry = y0; ry < y1; ry++) {
        for (int rx = x0; rx < x1; rx++) {
            uint32_t *p = &GFX->frame[(size_t)ry * GFX->w + rx];
            *p = mix(*p, c, alpha);
        }
    }
}

void gradient_v(int x, int y, int w, int h, uint32_t top, uint32_t bottom) {
    if (!GFX || w <= 0 || h <= 0) return;
    int denom = (h > 1) ? h - 1 : 1;
    for (int ry = 0; ry < h; ry++) {
        int t = (int)(((int64_t)ry * (1 << 16)) / denom); // 16.16 position
        for (int rx = 0; rx < w; rx++) {
            uint32_t out = 0;
            for (int shift = 0; shift < 32; shift += 8) {
                int32_t a = (int32_t)((top >> shift) & 0xFF);
                int32_t b = (int32_t)((bottom >> shift) & 0xFF);
                int32_t v = a + (int32_t)(((int64_t)(b - a) * t) >> 16);
                out |= ((uint32_t)(v & 0xFF)) << shift;
            }
            gp(x + rx, y + ry, out);
        }
    }
}

void blit(const uint32_t *src, int src_stride, int sx, int sy, int dx, int dy, int w, int h) {
    if (!GFX || w <= 0 || h <= 0) return;
    for (int ry = 0; ry < h; ry++) {
        for (int rx = 0; rx < w; rx++) {
            int px = sx + rx, py = sy + ry;
            if (px < 0 || py < 0) continue;
            uint32_t c = src[(size_t)py * src_stride + px];
            gp(dx + rx, dy + ry, c);
        }
    }
}

void shadow_rect(int x, int y, int w, int h) {
    // three stacked alpha layers = soft shadow (Win11 look)
    blend_rect(x - 4, y - 2, w + 8, h + 8, 0x00000000, 36);
    blend_rect(x - 2, y + 1, w + 4, h + 4, 0x00000000, 44);
    blend_rect(x, y + 3, w, h + 2, 0x00000000, 52);
}

int glyph_advance(char c, int scale) {
    // nicer spacing than a flat 8px grid (still monospace-friendly)
    const char *narrow = " .,:;!'`|il";
    const char *medium = "ftjr[]()-I";
    if (c == '\0') return 0;
    for (const char *p = narrow; *p; p++) if (*p == c) return 4 * scale;
    for (const char *p = medium; *p; p++) if (*p == c) return 6 * scale;
    return 8 * scale;
}

static void draw_glyph(int x, int y, char ch, uint32_t c, int scale) {
    const uint8_t *g = kian_font_glyph(ch);
    if (g == 0) return;
    for (int row = 0; row < 8; row++) {
        uint8_t bits = g[row];
        for (int col = 0; col < 8; col++) {
            if (bits & (0x80u >> col)) {
                if (scale == 1) {
                    gp(x + col, y + row, c);
                } else {
                    fill_rect(x + col * scale, y + row * scale, scale, scale, c);
                }
            }
        }
    }
}

void draw_text(int x, int y, const char *s, uint32_t c, int scale) {
    if (scale < 1) scale = 1;
    int ox = x;
    for (int i = 0; s[i] != '\0'; i++) {
        char ch = s[i];
        if (ch == '\n') { ox = x; y += text_line_height(scale); continue; }
        draw_glyph(ox, y, ch, c, scale);
        ox += glyph_advance(ch, scale);
    }
}

void draw_text_bold(int x, int y, const char *s, uint32_t c, int scale) {
    draw_text(x, y, s, c, scale);
    draw_text(x + scale, y, s, c, scale); // 1px offset -> bold look
}

int text_width(const char *s, int scale) {
    if (scale < 1) scale = 1;
    int w = 0;
    for (int i = 0; s[i] != '\0'; i++) w += glyph_advance(s[i], scale);
    return w;
}

int text_width_n(const char *s, int n, int scale) {
    if (scale < 1) scale = 1;
    int w = 0;
    for (int i = 0; i < n && s[i] != '\0'; i++) w += glyph_advance(s[i], scale);
    return w;
}

int text_line_height(int scale) {
    return 8 * scale + (scale > 1 ? 4 : 0);
}

bool rect_hit(int px, int py, const Rect &r) {
    return px >= r.x && px < r.x + r.w && py >= r.y && py < r.y + r.h;
}

bool rect_overlap(const Rect &a, const Rect &b) {
    return a.x < b.x + b.w && a.x + a.w > b.x && a.y < b.y + b.h && a.y + a.h > b.y;
}

Rect rect_union(const Rect &a, const Rect &b) {
    Rect r;
    r.x = imin(a.x, b.x); r.y = imin(a.y, b.y);
    r.w = imax(a.x + a.w, b.x + b.w) - r.x;
    r.h = imax(a.y + a.h, b.y + b.h) - r.y;
    return r;
}

// rep movsd for one row of the present blit (ESI=src, EDI=dst, ECX=count)
static inline void rep_movsd(uint32_t *dst, const uint32_t *src, int cnt) {
    __asm__ volatile ("rep movsd"
                      : "+S"(src), "+D"(dst), "+c"(cnt)
                      :
                      : "memory");
}

void present(int x, int y, int w, int h) {
    if (!GFX || w <= 0 || h <= 0) return;
    int x0 = imax(0, x), y0 = imax(0, y);
    int x1 = imin(GFX->w, x + w), y1 = imin(GFX->h, y + h);
    if (x1 <= x0 || y1 <= y0) return;
    for (int ry = y0; ry < y1; ry++) {
        uint32_t *dst = (uint32_t *)((uint8_t *)GFX->fb + (size_t)ry * (size_t)GFX->pitch) + x0;
        const uint32_t *src = GFX->frame + (size_t)ry * (size_t)GFX->w + x0;
        int cnt = x1 - x0;
        rep_movsd(dst, src, cnt);
    }
}

void scene_copy(int x, int y, int w, int h) {
    if (!GFX || w <= 0 || h <= 0) return;
    int x0 = imax(0, x), y0 = imax(0, y);
    int x1 = imin(GFX->w, x + w), y1 = imin(GFX->h, y + h);
    for (int ry = y0; ry < y1; ry++) {
        const uint32_t *s = GFX->scene + (size_t)ry * (size_t)GFX->w + x0;
        uint32_t *d = GFX->frame + (size_t)ry * (size_t)GFX->w + x0;
        int cnt = x1 - x0;
        rep_movsd(d, s, cnt);
    }
}

void scene_snap_all() {
    if (!GFX) return;
    int cnt = GFX->w;
    for (int ry = 0; ry < GFX->h; ry++) {
        const uint32_t *s = GFX->frame + (size_t)ry * (size_t)GFX->w;
        uint32_t *d = GFX->scene + (size_t)ry * (size_t)GFX->w;
        rep_movsd(d, s, cnt);
    }
}

void scene_snap_rect(int x, int y, int w, int h) {
    if (!GFX || w <= 0 || h <= 0) return;
    int x0 = imax(0, x), y0 = imax(0, y);
    int x1 = imin(GFX->w, x + w), y1 = imin(GFX->h, y + h);
    for (int ry = y0; ry < y1; ry++) {
        const uint32_t *s = GFX->frame + (size_t)ry * (size_t)GFX->w + x0;
        uint32_t *d = GFX->scene + (size_t)ry * (size_t)GFX->w + x0;
        int cnt = x1 - x0;
        rep_movsd(d, s, cnt);
    }
}
