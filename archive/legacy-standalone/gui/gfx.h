// Kian OS GUI - gfx core: frame + scene buffers, drawing primitives,
// dirty-rect presentation with `rep movsd`. Integers only, no SSE.
#pragma once
#include <kian_abi.h>

struct Rect { int x, y, w, h; };

struct Gfx {
    uint32_t *frame;      // full composition buffer (w*h pixels)
    uint32_t *scene;      // frame content WITHOUT the cursor (cursor trick)
    int w, h;
    uint32_t *fb;         // real framebuffer
    int pitch;            // real framebuffer pitch in BYTES
};

extern Gfx *GFX;          // set by gfx_init, cleared by gfx_free

bool gfx_init(const kian_fb_t *fb);   // false on OOM (frame+scene already freed)
void gfx_free();

void clip_set(Rect r);    // drawing is clipped to this rect (default: screen)
void clip_reset();
Rect clip_get();

void gp(int x, int y, uint32_t c);                       // bounds+clip checked put_pixel
void fill_rect(int x, int y, int w, int h, uint32_t c);
void hline(int x, int y, int len, uint32_t c);
void vline(int x, int y, int len, uint32_t c);
void rect_outline(int x, int y, int w, int h, uint32_t c);
void fill_round_rect(int x, int y, int w, int h, int r, uint32_t c);
void round_rect_outline(int x, int y, int w, int h, int r, uint32_t c);
void fill_circle(int cx, int cy, int r, uint32_t c);
void circle_outline(int cx, int cy, int r, uint32_t c);
void blend_pixel(int x, int y, uint32_t c, int alpha);   // alpha 0..255 over frame
void blend_rect(int x, int y, int w, int h, uint32_t c, int alpha);
void gradient_v(int x, int y, int w, int h, uint32_t top, uint32_t bottom);
void blit(const uint32_t *src, int src_stride, int sx, int sy, int dx, int dy, int w, int h);
void shadow_rect(int x, int y, int w, int h);            // soft 3-layer shadow

int  glyph_advance(char c, int scale);              // UI font (proportional)
void draw_text(int x, int y, const char *s, uint32_t c, int scale);
void draw_text_bold(int x, int y, const char *s, uint32_t c, int scale);
int  text_width(const char *s, int scale);
int  text_width_n(const char *s, int n, int scale);  // width of the first n chars
int  text_line_height(int scale);
// vertical centering: y to pass to draw_text() so the glyph ink (cap band)
// sits centered inside a box of the given height
int  text_center_y(int box_y, int box_h, int scale);

// ---- mono font (8x16 cell) for the terminal / text editors ----
int  glyph_advance_mono(int scale);                 // FONT_MONO_CELL * scale
int  text_line_height_mono(int scale);              // FONT_MONO_ROWS * scale
void draw_text_mono(int x, int y, const char *s, uint32_t c, int scale);
void draw_text_mono_bold(int x, int y, const char *s, uint32_t c, int scale);
int  text_width_mono(const char *s, int scale);     // len * cell * scale
int  mono_caret_y(int line_y);                      // y/height of the caret bar
int  mono_caret_h();

bool rect_hit(int px, int py, const Rect &r);
bool rect_overlap(const Rect &a, const Rect &b);
Rect rect_union(const Rect &a, const Rect &b);

void present(int x, int y, int w, int h);    // frame -> framebuffer (rep movsd)
void scene_copy(int x, int y, int w, int h); // scene -> frame (erase cursor)
void scene_snap_all();                       // frame -> scene (after full redraws)
void scene_snap_rect(int x, int y, int w, int h); // frame -> scene for a region
