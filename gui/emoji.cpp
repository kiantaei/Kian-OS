// Kian OS GUI - emoji implementations: flat "Windows emoji" style, every
// glyph drawn with rects/circles/lines in integer code. All sizes derive
// from the requested box size s (>= 4).
#include "emoji.h"
#include "gfx.h"
#include "util.h"

// ---- local primitive helpers (independent from icons.cpp) ----

static void em_line(int x0, int y0, int x1, int y1, uint32_t c) {
    int dx = iabs(x1 - x0), dy = iabs(y1 - y0);
    int sx = (x0 < x1) ? 1 : -1, sy = (y0 < y1) ? 1 : -1;
    int err = dx - dy;
    for (int guard = 0; guard < 256; guard++) {
        gp(x0, y0, c);
        if (x0 == x1 && y0 == y1) break;
        int e2 = err * 2;
        if (e2 > -dy) { err -= dy; x0 += sx; }
        if (e2 < dx) { err += dx; y0 += sy; }
    }
}

// ring segment: qmask bits 1=NE 2=SE 4=SW 8=NW
static void em_arc(int cx, int cy, int r, int qmask, uint32_t c) {
    for (int dy = -r; dy <= r; dy++) {
        for (int dx = -r; dx <= r; dx++) {
            int dd = dx * dx + dy * dy;
            if (dd > r * r || dd < (r - 1) * (r - 1)) continue;
            if (dx >= 0 && dy < 0) { if (qmask & 1) gp(cx + dx, cy + dy, c); }
            else if (dx >= 0 && dy >= 0) { if (qmask & 2) gp(cx + dx, cy + dy, c); }
            else if (dx < 0 && dy >= 0) { if (qmask & 4) gp(cx + dx, cy + dy, c); }
            else { if (qmask & 8) gp(cx + dx, cy + dy, c); }
        }
    }
}

// filled triangle (integer baryon test, bounded)
static void em_tri(int ax, int ay, int bx, int by, int cx2, int cy2, uint32_t c) {
    int minx = imin(imin(ax, bx), cx2), maxx = imax(imax(ax, bx), cx2);
    int miny = imin(imin(ay, by), cy2), maxy = imax(imax(ay, by), cy2);
    for (int py = miny; py <= maxy; py++) {
        for (int px = minx; px <= maxx; px++) {
            long long d1 = (long long)(px - bx) * (cy2 - by) - (long long)(py - by) * (cx2 - bx);
            long long d2 = (long long)(px - cx2) * (ay - cy2) - (long long)(py - cy2) * (ax - cx2);
            long long d3 = (long long)(px - ax) * (by - ay) - (long long)(py - ay) * (bx - ax);
            bool neg = (d1 < 0) || (d2 < 0) || (d3 < 0);
            bool pos = (d1 > 0) || (d2 > 0) || (d3 > 0);
            if (!(neg && pos)) gp(px, py, c);
        }
    }
}

// ---- palette (Windows-flat) ----
static const uint32_t EM_YELLOW = 0x00FFD34D;
static const uint32_t EM_YELLOWS = 0x00E6A817;
static const uint32_t EM_RED    = 0x00E8483B;
static const uint32_t EM_GREEN  = 0x0024A148;
static const uint32_t EM_BLUE   = 0x00267CD9;
static const uint32_t EM_GRAY   = 0x009AA6B2;
static const uint32_t EM_GRAYD  = 0x005B6670;
static const uint32_t EM_AMBER  = 0x00F0A830;
static const uint32_t EM_WHITE  = 0x00FFFFFF;
static const uint32_t EM_DARK   = 0x00303030;
static const uint32_t EM_PALE   = 0x00E8ECF4;
static const uint32_t EM_ORANGE = 0x00F2793C;

// ---- glyphs (coordinates relative to x,y box of size s) ----

static void g_smile(int x, int y, int s) {
    int r = s / 2 - 1, cx = x + s / 2, cy = y + s / 2;
    fill_circle(cx, cy, r, EM_YELLOW);
    fill_circle(cx, cy, imax(1, r - 1), EM_YELLOW);
    int er = imax(1, s / 10);
    fill_circle(cx - r / 2, cy - r / 3, er, EM_DARK);
    fill_circle(cx + r / 2, cy - r / 3, er, EM_DARK);
    // mouth: bottom arc
    em_arc(cx, cy - r / 4, r / 2, 2 | 4, EM_DARK);
    for (int dy = 0; dy < imax(1, s / 9); dy++) {
        int rr = r / 2 - 1;
        for (int dx = -rr; dx <= rr; dx++) {
            int dd = dx * dx + dy * dy;
            if (dd <= rr * rr && dx * dx + (dy + 1) * (dy + 1) > rr * rr) {
                gp(cx + dx, cy - r / 4 + rr - dy + 1, EM_DARK);
            }
        }
    }
}

static void g_heart(int x, int y, int s) {
    int r = s / 5 + 1;
    fill_circle(x + s / 4 + 1, y + s / 3 + 1, r, EM_RED);
    fill_circle(x + s * 3 / 4 - 1, y + s / 3 + 1, r, EM_RED);
    em_tri(x + s / 8 + 1, y + s / 3 + 2, x + s * 7 / 8 - 1, y + s / 3 + 2, x + s / 2, y + s - 2, EM_RED);
}

static void g_star(int x, int y, int s) {
    int cx = x + s / 2, cy = y + s / 2 + 1, r = s / 2 - 1;
    // five spikes: thick cross + diagonals (recognizable star at small size)
    for (int i = 0; i < 4; i++) {
        int qx = (i == 0) ? 0 : (i == 1) ? 1 : (i == 2) ? 0 : -1;
        int qy = (i == 0) ? -1 : (i == 1) ? 0 : (i == 2) ? 1 : 0;
        em_line(cx, cy, cx + qx * r, cy + qy * r, EM_YELLOW);
        em_line(cx + 1, cy, cx + 1 + qx * r, cy + qy * r, EM_YELLOW);
    }
    for (int i = -1; i <= 1; i += 2) {
        em_line(cx - 1, cy - 1, cx - 1 + i * r * 3 / 4, cy - 1 - i * r * 3 / 4, EM_YELLOW);
        em_line(cx, cy - 1, cx + i * r * 3 / 4, cy - 1 - i * r * 3 / 4, EM_YELLOW);
    }
    fill_circle(cx, cy, imax(2, s / 5), EM_YELLOW);
    fill_circle(cx, cy, imax(1, s / 5 - 1), EM_YELLOWS);
}

static void g_bolt(int x, int y, int s) {
    int mx = x + s / 2;
    em_tri(mx + s / 4, y + 1, x + s / 4, y + s / 2 + s / 5, mx, y + s / 2, EM_YELLOW);
    em_tri(mx - s / 4, y + s - 1, mx + s / 3, y + s / 2 - s / 6, mx, y + s / 2, EM_YELLOWS);
}

static void g_sun(int x, int y, int s) {
    int cx = x + s / 2, cy = y + s / 2, r = s / 3;
    static const int ux[8] = { 1, 1, 0, -1, -1, -1, 0, 1 };
    static const int uy[8] = { 0, 1, 1, 1, 0, -1, -1, -1 };
    int dl = imax(2, s / 6);
    for (int i = 0; i < 8; i++) {
        int fx = ux[i], fy = uy[i];
        if (fx != 0 && fy != 0) {
            int x0 = cx + fx * (r * 2 / 3), y0 = cy + fy * (r * 2 / 3);
            int x1 = cx + fx * (r * 2 / 3 + dl), y1 = cy + fy * (r * 2 / 3 + dl);
            em_line(x0, y0, x1, y1, EM_YELLOWS);
        } else {
            em_line(cx + fx * r, cy + fy * r, cx + fx * (r + dl), cy + fy * (r + dl), EM_YELLOWS);
        }
    }
    fill_circle(cx, cy, r, EM_YELLOW);
    fill_circle(cx, cy, r - 1, EM_YELLOW);
}

static void g_moon(int x, int y, int s) {
    int cx = x + s / 2, cy = y + s / 2, r = s / 2 - 1;
    for (int t = 0; t < imax(2, s / 5); t++) {
        em_arc(cx, cy, r - t, 1 | 2 | 8, EM_PALE);
    }
    em_line(cx - r / 3, cy - r / 2, cx - r / 3, cy + r / 2, EM_PALE);
    em_line(cx - r / 3 + 1, cy - r / 3, cx - r / 3 + 1, cy + r / 3, EM_PALE);
}

static void g_cloud(int x, int y, int s) {
    int base = y + s * 2 / 3;
    fill_circle(x + s / 3, y + s / 2, s / 4, EM_PALE);
    fill_circle(x + s * 3 / 5, y + s * 2 / 5, s / 3, EM_PALE);
    fill_circle(x + s * 4 / 5, y + s / 2, s / 5, EM_PALE);
    fill_rect(x + s / 4, base - 1, s - s / 2, s / 4, EM_PALE);
}

static void g_folder(int x, int y, int s) {
    int tab = s / 3;
    fill_rect(x + 1, y + 2, tab + 2, 3, EM_AMBER);
    fill_rect(x + 1, y + 4, s - 2, s - 6, EM_AMBER);
    fill_rect(x + 1, y + 4, s - 2, 2, 0x00C08820);
    fill_rect(x + 2, y + 6, s - 4, s - 9, 0x00FFC85C);
}

static void g_gear(int x, int y, int s) {
    int cx = x + s / 2, cy = y + s / 2, r = s / 2 - 2;
    static const int tx[8] = { 0, 6, 6, 0, -6, -6, 0, 0 };
    static const int ty[8] = { -6, -3, 3, 6, 3, -3, -6, 6 };
    for (int i = 0; i < 8; i++) {
        int px = cx + tx[i] * s / 16, py = cy + ty[i] * s / 16;
        fill_rect(px - 1, py - 1, 3, 3, EM_GRAY);
    }
    for (int t = 0; t < imax(1, s / 8); t++) {
        em_arc(cx, cy, r - t, 15, EM_GRAY);
    }
    fill_circle(cx, cy, imax(1, r / 2), EM_GRAYD);
}

static void g_globe(int x, int y, int s) {
    int cx = x + s / 2, cy = y + s / 2, r = s / 2 - 1;
    fill_circle(cx, cy, r, EM_BLUE);
    fill_circle(cx - r / 3, cy - r / 4, r / 3, EM_GREEN);
    fill_circle(cx + r / 3, cy + r / 3, r / 4, EM_GREEN);
    // meridians + equator
    hline(cx - r + 1, cy, 2 * r - 2, 0x00BFE0FF);
    em_arc(cx, cy, r - 1, 1 | 2 | 4 | 8, 0x00BFE0FF);
    for (int t = 0; t < imax(1, s / 10); t++) {
        em_arc(cx + (r - 1 - t) / 2, cy, (r - 1 - t) / 2 + t / 2, 1 | 2, 0x00BFE0FF);
        em_arc(cx - (r - 1 - t) / 2, cy, (r - 1 - t) / 2 + t / 2, 4 | 8, 0x00BFE0FF);
    }
}

static void g_rocket(int x, int y, int s) {
    int cx = x + s / 2;
    em_tri(cx, y + 1, cx - s / 4, y + s / 2, cx + s / 4, y + s / 2, EM_PALE);
    fill_rect(cx - s / 4, y + s / 2 - 1, s / 2 + 1, s / 3, EM_PALE);
    fill_circle(cx, y + s / 3, imax(1, s / 8), EM_BLUE);
    em_tri(cx - s / 4, y + s * 5 / 6, cx - s / 4, y + s / 2, cx - s / 3, y + s / 2, EM_RED);
    em_tri(cx + s / 4, y + s * 5 / 6, cx + s / 4, y + s / 2, cx + s / 3, y + s / 2, EM_RED);
    em_tri(cx - s / 6, y + s - 1, cx + s / 6, y + s - 1, cx, y + s * 2 / 3, EM_ORANGE);
}

static void g_fire(int x, int y, int s) {
    int cx = x + s / 2;
    em_tri(cx, y + 1, x + s / 6, y + s * 2 / 3, x + s * 5 / 6, y + s * 2 / 3, EM_ORANGE);
    em_tri(cx, y + s / 3, x + s / 3, y + s - 1, x + s * 2 / 3, y + s - 1, EM_YELLOW);
    fill_circle(cx, y + s * 2 / 3, imax(1, s / 6), EM_YELLOW);
}

static void g_ok(int x, int y, int s) {
    int cx = x + s / 2, cy = y + s / 2, r = s / 2 - 1;
    fill_circle(cx, cy, r, EM_GREEN);
    em_line(cx - r / 2, cy, cx - r / 6, cy + r / 2, EM_WHITE);
    em_line(cx - r / 6 - 1, cy + r / 2, cx + r / 2, cy - r / 3, EM_WHITE);
    em_line(cx - r / 6 + 1, cy + r / 2, cx + r / 2 + 1, cy - r / 3, EM_WHITE);
}

static void g_warn(int x, int y, int s) {
    int cx = x + s / 2;
    em_tri(cx, y + 1, x + 1, y + s - 2, x + s - 2, y + s - 2, EM_YELLOW);
    em_tri(cx, y + 3, x + 3, y + s - 4, x + s - 4, y + s - 4, EM_YELLOW);
    fill_rect(cx - 1, y + s / 3, 2, s / 3, EM_DARK);
    fill_rect(cx - 1, y + s * 2 / 3, 2, 2, EM_DARK);
}

static void g_lock(int x, int y, int s) {
    int bw = s * 3 / 5, bx = x + (s - bw) / 2;
    int by = y + s / 2;
    em_arc(x + s / 2, by, bw / 2, 1 | 8, EM_GRAY);
    em_line(x + s / 2 - bw / 2, by, x + s / 2 - bw / 2, by - bw / 4, EM_GRAY);
    em_line(x + s / 2 + bw / 2, by, x + s / 2 + bw / 2, by - bw / 4, EM_GRAY);
    fill_round_rect(bx, by - 1, bw, s / 2, 2, 0x00FFC85C);
    fill_rect(x + s / 2 - 1, by + s / 6, 2, s / 8, EM_DARK);
}

static void g_music(int x, int y, int s) {
    int nx = x + s / 3, ny = y + s * 3 / 4;
    fill_circle(nx, ny, imax(2, s / 5), EM_BLUE);
    int stem = s * 2 / 3;
    fill_rect(nx + s / 5 - 1, ny - stem, 2, stem, EM_BLUE);
    fill_rect(nx + s / 5, ny - stem, s / 3, 2, EM_BLUE);
}

// ---- dispatcher ----

void emoji_draw(int id, int x, int y, int size) {
    if (size < 4) size = 4;
    switch (id) {
    case EMOJI_SMILE:  g_smile(x, y, size); break;
    case EMOJI_HEART:  g_heart(x, y, size); break;
    case EMOJI_STAR:   g_star(x, y, size); break;
    case EMOJI_BOLT:   g_bolt(x, y, size); break;
    case EMOJI_SUN:    g_sun(x, y, size); break;
    case EMOJI_MOON:   g_moon(x, y, size); break;
    case EMOJI_CLOUD:  g_cloud(x, y, size); break;
    case EMOJI_FOLDER: g_folder(x, y, size); break;
    case EMOJI_GEAR:   g_gear(x, y, size); break;
    case EMOJI_GLOBE:  g_globe(x, y, size); break;
    case EMOJI_ROCKET: g_rocket(x, y, size); break;
    case EMOJI_FIRE:   g_fire(x, y, size); break;
    case EMOJI_OK:     g_ok(x, y, size); break;
    case EMOJI_WARN:   g_warn(x, y, size); break;
    case EMOJI_LOCK:   g_lock(x, y, size); break;
    case EMOJI_MUSIC:  g_music(x, y, size); break;
    default: break;
    }
}

int emoji_size_valid(int id) { return id >= 1 && id < EMOJI_N; }

int emoji_size_ui(int scale) { return 12 * scale; }
int emoji_advance_ui(int scale) { return 14 * scale; }
int emoji_size_mono(int scale) { return 12 * scale; }
int emoji_advance_mono(int scale) { return 2 * 8 * scale; }  // two 8px cells
