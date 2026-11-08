// Kian OS GUI - every icon drawn with rects/lines/circles in code.
#include "icons.h"
#include "gfx.h"
#include "util.h"

// arc helper: draws a ring segment. qmask bits: 1=NE 2=SE 4=SW 8=NW
static void arc_px(int cx, int cy, int r, int qmask, uint32_t c) {
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

static void line(int x0, int y0, int x1, int y1, uint32_t c) {
    // integer Bresenham-ish walk
    int dx = iabs(x1 - x0), dy = iabs(y1 - y0);
    int sx = (x0 < x1) ? 1 : -1, sy = (y0 < y1) ? 1 : -1;
    int err = dx - dy;
    for (int guard = 0; guard < 512; guard++) {
        gp(x0, y0, c);
        if (x0 == x1 && y0 == y1) break;
        int e2 = err * 2;
        if (e2 > -dy) { err -= dy; x0 += sx; }
        if (e2 < dx) { err += dx; y0 += sy; }
    }
}

static void draw_start(int x, int y, int s, uint32_t c) {
    int q = s / 2 - 1, g = s / 8 + 1;
    fill_rect(x, y, q, q, c);
    fill_rect(x + q + g, y, q, q, c);
    fill_rect(x, y + q + g, q, q, c);
    fill_rect(x + q + g, y + q + g, q, q, c);
}

static void draw_search(int x, int y, int s, uint32_t c) {
    int r = s / 4;
    circle_outline(x + s / 2 - 1, y + s / 2 - 1, r, c);
    line(x + s / 2 + r - 2, y + s / 2 + r - 2, x + s - 2, y + s - 2, c);
}

static void draw_taskview(int x, int y, int s, uint32_t c) {
    int q = s * 3 / 5;
    rect_outline(x, y, q, q, c);
    rect_outline(x + s / 4, y + s / 4, q, q, c);
}

static void draw_browser(int x, int y, int s, uint32_t c) {
    int r = s / 2 - 2;
    circle_outline(x + s / 2, y + s / 2, r, c);
    hline(x + s / 2 - r, y + s / 2, 2 * r, c);
    vline(x + s / 2, y + s / 2 - r, 2 * r, c);
    // two vertical ellipse guides
    line(x + s / 2 - r / 2, y + s / 2 - r, x + s / 2 - r / 2, y + s / 2 + r, c);
    line(x + s / 2 + r / 2, y + s / 2 - r, x + s / 2 + r / 2, y + s / 2 + r, c);
}

static void draw_terminal(int x, int y, int s, uint32_t c) {
    rect_outline(x, y, s, s, c);
    line(x + 2, y + s / 4, x + s / 3, y + s / 2, c);
    line(x + s / 3, y + s / 2, x + 2, y + s * 3 / 4, c);
    hline(x + s / 2, y + s * 3 / 4, s / 3, c);
}

static void draw_files(int x, int y, int s, uint32_t c) {
    int tabw = s / 3;
    hline(x, y, tabw, c);
    line(x + tabw, y, x + tabw + 2, y + 3, c);
    hline(x + tabw + 2, y + 3, s - tabw - 2, c);
    vline(x, y, s, c);
    vline(x + s - 1, y + 3, s - 3, c);
    hline(x, y + s - 1, s, c);
    hline(x, y + 4, 2, c);
    vline(x, y + 4, s - 4, c);
}

static void draw_notes(int x, int y, int s, uint32_t c) {
    rect_outline(x + 1, y, s - 2, s, c);
    hline(x + 3, y + s / 5, s - 6, c);
    hline(x + 3, y + s / 2, s - 6, c);
    hline(x + 3, y + s * 4 / 5, s - 8, c);
}

static void draw_settings(int x, int y, int s, uint32_t c) {
    int cx = x + s / 2, cy = y + s / 2, r = s / 2 - 2;
    // teeth: 8 small rects around the ring
    static const int tx[8] = { 0, 6, 6, 0, -6, -6, 0, 0 };
    static const int ty[8] = { -6, -3, 3, 6, 3, -3, -6, 6 };
    for (int i = 0; i < 8; i++) {
        int px = cx + tx[i] * s / 16, py = cy + ty[i] * s / 16;
        fill_rect(px - 1, py - 1, 3, 3, c);
    }
    circle_outline(cx, cy, r, c);
    circle_outline(cx, cy, r / 2, c);
}

static void draw_taskmgr(int x, int y, int s, uint32_t c) {
    int bw = s / 5;
    fill_rect(x + 2, y + s * 3 / 5, bw, s * 2 / 5, c);
    fill_rect(x + 2 + bw + 1, y + s * 2 / 5, bw, s * 3 / 5, c);
    fill_rect(x + 2 + 2 * (bw + 1), y + s / 5, bw, s * 4 / 5, c);
    hline(x, y + s - 1, s, c);
}

static void draw_calc(int x, int y, int s, uint32_t c) {
    rect_outline(x + 1, y, s - 2, s, c);
    hline(x + 3, y + 2, s - 6, c);
    for (int r = 0; r < 2; r++) {
        for (int q = 0; q < 3; q++) {
            fill_rect(x + 3 + q * (s - 6) / 3, y + s / 2 + r * (s / 4), 2, 2, c);
        }
    }
}

static void draw_paint(int x, int y, int s, uint32_t c) {
    circle_outline(x + s / 2, y + s / 2, s / 2 - 2, c);
    fill_rect(x + s / 2 - 1, y + s / 4, 2, 2, c);
    fill_rect(x + s / 4, y + s / 2, 2, 2, c);
    fill_rect(x + s * 3 / 4, y + s / 2, 2, 2, c);
}

static void draw_trash(int x, int y, int s, uint32_t c) {
    hline(x + 1, y + 2, s - 2, c);                    // lid
    rect_outline(x + 3, y + 4, s - 6, s - 5, c);      // body
    vline(x + s / 2 - 3, y + 6, s / 2, c);
    vline(x + s / 2 + 2, y + 6, s / 2, c);
}

static void draw_pc(int x, int y, int s, uint32_t c) {
    rect_outline(x, y + 1, s, s * 3 / 5, c);
    vline(x + s / 2, y + 1 + s * 3 / 5, s / 5, c);
    hline(x + s / 4, y + s - 2, s / 2, c);
}

static void draw_power(int x, int y, int s, uint32_t c) {
    int cx = x + s / 2, cy = y + s / 2 + 1, r = s / 2 - 2;
    // ring with a gap at the top
    for (int dy = -r; dy <= r; dy++) {
        for (int dx = -r; dx <= r; dx++) {
            int dd = dx * dx + dy * dy;
            if (dd > r * r || dd < (r - 1) * (r - 1)) continue;
            if (dy < -r * 2 / 3 && iabs(dx) < r * 2 / 3) continue; // gap
            gp(cx + dx, cy + dy, c);
        }
    }
    vline(cx, y + 1, s / 2, c);
}

static void draw_user(int x, int y, int s, uint32_t c) {
    int cx = x + s / 2;
    fill_circle(cx, y + s / 3, s / 5, c);              // head
    // shoulders: half disc
    for (int dy = 0; dy <= s / 3; dy++) {
        for (int dx = -(s / 2 - 2); dx <= (s / 2 - 2); dx++) {
            int k = (s / 2 - 2);
            if (dx * dx + dy * dy <= k * k) gp(cx + dx, y + s - 2 - dy, c);
        }
    }
}

static void draw_wifi(int x, int y, int s, uint32_t c) {
    int cx = x + s / 2, cy = y + s - 3;
    arc_px(cx, cy, s / 2, 1, c);                       // outer arc (upper right half)
    arc_px(cx, cy, s / 2, 8, c);                       // upper left half
    arc_px(cx, cy, s / 3 + 1, 1, c);
    arc_px(cx, cy, s / 3 + 1, 8, c);
    fill_rect(cx - 1, cy - 2, 2, 2, c);
}

static void draw_speaker(int x, int y, int s, uint32_t c) {
    int cy = y + s / 2;
    fill_rect(x + 2, cy - 2, 3, 5, c);
    for (int i = 0; i < 4; i++) {
        vline(x + 5 + i, cy - 2 - i, 5 + 2 * i, c);
    }
    arc_px(x + 6, cy, s / 3, 1 | 2, c);
}

static void draw_battery(int x, int y, int s, uint32_t c) {
    rect_outline(x, y + s / 4, s - 4, s / 2, c);
    fill_rect(x + s - 3, y + s / 2 - 2, 2, 4, c);      // tip
    fill_rect(x + 2, y + s / 4 + 2, (s - 8) * 4 / 5, s / 2 - 4, c); // charge
}

static void draw_chevup(int x, int y, int s, uint32_t c) {
    line(x + 2, y + s * 2 / 3, x + s / 2, y + s / 3, c);
    line(x + s / 2, y + s / 3, x + s - 2, y + s * 2 / 3, c);
}

static void draw_sync(int x, int y, int s, uint32_t c) {
    int cx = x + s / 2, cy = y + s / 2, r = s / 2 - 2;
    arc_px(cx, cy, r, 1 | 8, c);                       // left+top half
    arc_px(cx, cy, r, 2 | 4, c);                       // right+bottom half
    fill_rect(cx - r - 1, cy - 2, 3, 3, c);
    fill_rect(cx + r - 1, cy, 3, 3, c);
}

static void draw_back(int x, int y, int s, uint32_t c) {
    line(x + s * 2 / 3, y + 2, x + s / 3, y + s / 2, c);
    line(x + s / 3, y + s / 2, x + s * 2 / 3, y + s - 2, c);
}

static void draw_updir(int x, int y, int s, uint32_t c) {
    line(x + 2, y + s * 2 / 3, x + s / 2, y + s / 3, c);
    line(x + s / 2, y + s / 3, x + s - 2, y + s * 2 / 3, c);
    hline(x + s / 4, y + s * 3 / 4, s / 2, c);
}

static void draw_close(int x, int y, int s, uint32_t c) {
    line(x + 2, y + 2, x + s - 2, y + s - 2, c);
    line(x + s - 2, y + 2, x + 2, y + s - 2, c);
}

static void draw_min(int x, int y, int s, uint32_t c) {
    hline(x + 2, y + s - 3, s - 4, c);
}

static void draw_max(int x, int y, int s, uint32_t c) {
    rect_outline(x + 2, y + 2, s - 4, s - 4, c);
}

static void draw_restore(int x, int y, int s, uint32_t c) {
    rect_outline(x + s / 3, y + 1, s * 2 / 3 - 1, s * 2 / 3 - 1, c);
    rect_outline(x + 1, y + s / 3, s * 2 / 3 - 1, s * 2 / 3 - 1, c);
}

static void draw_plus(int x, int y, int s, uint32_t c) {
    vline(x + s / 2, y + 2, s - 4, c);
    hline(x + 2, y + s / 2, s - 4, c);
}

static void draw_check(int x, int y, int s, uint32_t c) {
    line(x + 2, y + s / 2, x + s / 3, y + s - 3, c);
    line(x + s / 3, y + s - 3, x + s - 2, y + 2, c);
}

static void draw_bookmark(int x, int y, int s, uint32_t c) {
    vline(x + 3, y + 1, s - 2, c);
    vline(x + s - 4, y + 1, s - 2, c);
    hline(x + 3, y + 1, s - 7, c);
    line(x + 3, y + s - 2, x + s / 2, y + s * 2 / 3, c);
    line(x + s / 2, y + s * 2 / 3, x + s - 4, y + s - 2, c);
}

static void draw_grid(int x, int y, int s, uint32_t c) {
    int q = (s - 5) / 2;
    rect_outline(x, y, q, q, c);
    rect_outline(x + q + 3, y, q, q, c);
    rect_outline(x, y + q + 3, q, q, c);
    rect_outline(x + q + 3, y + q + 3, q, q, c);
}

// Xenox logo: bold ring with an X inside (drawn in code only).
static void draw_xenox(int x, int y, int s, uint32_t c) {
    int cx = x + s / 2, cy = y + s / 2;
    int r = s / 2 - 1;
    for (int t = 0; t < 2; t++) {
        for (int dy = -(r - t); dy <= (r - t); dy++) {
            for (int dx = -(r - t); dx <= (r - t); dx++) {
                int dd = dx * dx + dy * dy;
                if (dd > (r - t) * (r - t) || dd < (r - t - 1) * (r - t - 1)) continue;
                gp(cx + dx, cy + dy, c);
            }
        }
    }
    int xr = r - 4;
    if (xr < 2) xr = 2;
    line(cx - xr, cy - xr, cx + xr, cy + xr, c);
    line(cx - xr + 1, cy - xr, cx + xr + 1, cy + xr, c);
    line(cx + xr, cy - xr, cx - xr, cy + xr, c);
    line(cx + xr - 1, cy - xr, cx - xr - 1, cy + xr, c);
}

static void draw_forward(int x, int y, int s, uint32_t c) {
    line(x + s / 3, y + 2, x + s * 2 / 3, y + s / 2, c);
    line(x + s * 2 / 3, y + s / 2, x + s / 3, y + s - 2, c);
}

static void draw_reload(int x, int y, int s, uint32_t c) {
    int cx = x + s / 2, cy = y + s / 2, r = s / 2 - 3;
    // ring with a gap at the top-right, plus an arrowhead into the gap
    for (int dy = -r; dy <= r; dy++) {
        for (int dx = -r; dx <= r; dx++) {
            int dd = dx * dx + dy * dy;
            if (dd > r * r || dd < (r - 1) * (r - 1)) continue;
            if (dx > r / 2 && dy < -r / 2) continue; // gap
            gp(cx + dx, cy + dy, c);
        }
    }
    fill_rect(cx + r - 3, cy - r - 1, 4, 4, c);
    fill_rect(cx + r - 1, cy - r + 1, 3, 3, c);
}

static void draw_home(int x, int y, int s, uint32_t c) {
    // roof: two slanted thick bars
    line(x + 1, y + s / 2, x + s / 2, y + 1, c);
    line(x + 1 + 1, y + s / 2, x + s / 2 + 1, y + 1, c);
    line(x + s / 2, y + 1, x + s - 2, y + s / 2, c);
    line(x + s / 2, y + 1, x + s - 1, y + s / 2, c);
    fill_rect(x + s / 4, y + s / 2, s / 2, s / 2 - 2, c);
}

static void draw_lock(int x, int y, int s, uint32_t c) {
    int bw = s * 3 / 5, bx = x + (s - bw) / 2;
    int by = y + s / 2;
    int r = bw / 2;
    int cx = x + s / 2;
    for (int dy = -r; dy <= 0; dy++) {
        for (int dx = -r; dx <= r; dx++) {
            int dd = dx * dx + dy * dy;
            if (dd <= r * r && dd >= (r - 2) * (r - 2)) gp(cx + dx, by + dy, c);
        }
    }
    fill_rect(bx, by - 1, bw, s / 2, c);
}

static void draw_dots(int x, int y, int s, uint32_t c) {
    fill_circle(x + s / 2, y + s / 5, 2, c);
    fill_circle(x + s / 2, y + s / 2, 2, c);
    fill_circle(x + s / 2, y + s * 4 / 5, 2, c);
}

static void draw_clock(int x, int y, int s, uint32_t c) {
    circle_outline(x + s / 2, y + s / 2, s / 2 - 1, c);
    line(x + s / 2, y + s / 3, x + s / 2, y + s / 2, c);
    line(x + s / 2, y + s / 2, x + s * 2 / 3, y + s * 2 / 3, c);
}

static void draw_download(int x, int y, int s, uint32_t c) {
    line(x + s / 2, y + 2, x + s / 2, y + s * 2 / 3, c);
    line(x + s / 3, y + s / 2, x + s / 2, y + s * 2 / 3, c);
    line(x + s / 2, y + s * 2 / 3, x + s * 2 / 3, y + s / 2, c);
    hline(x + 2, y + s - 2, s - 4, c);
}

// native globe (browser start page): circle ring + meridian ellipse + equator
static void draw_globe(int x, int y, int s, uint32_t c) {
    int cx = x + s / 2, cy = y + s / 2;
    int r = s / 2 - 1;
    if (r < 3) r = 3;
    for (int dy = -r; dy <= r; dy++) {
        for (int dx = -r; dx <= r; dx++) {
            int dd = dx * dx + dy * dy;
            if (dd <= r * r && dd >= (r - 1) * (r - 1)) gp(cx + dx, cy + dy, c); // ring only
        }
    }
    // vertical meridian (ellipse ring): ry = r, rx = r/2, dx = rx*sqrt(1-(dy/r)^2)
    int rx = (r > 4) ? r / 2 : 2;
    for (int dy = -r; dy <= r; dy++) {
        int want = 0;   // largest want with want^2 * r^2 <= rx^2 * (r^2 - dy^2)
        while ((want + 1) * (want + 1) * (int64_t)r * r <=
               (int64_t)rx * rx * (r * r - dy * dy)) want++;
        gp(cx + want, cy + dy, c);
        gp(cx - want, cy + dy, c);
    }
    hline(cx - r, cy, r * 2 + 1, c);   // full equator
}

// native warning triangle (browser error page): outline + exclamation mark
static void draw_warn(int x, int y, int s, uint32_t c) {
    int cx = x + s / 2;
    int top = y + 1, bot = y + s - 2, half = s / 2 - 1;
    if (half < 2) half = 2;
    for (int i = 0; i <= half; i++) {
        // apex at the top (cx, top), base at the bottom (cx +/- half, bot)
        int ly = top + (i * (bot - top)) / half;
        gp(cx - i, ly, c);
        gp(cx + i, ly, c);
    }
    hline(cx - half, bot, half * 2 + 1, c);
    // exclamation mark
    int w = (s >= 16) ? 2 : 1;
    fill_rect(cx - w / 2, y + s * 2 / 5, w, s / 4, c);
    fill_rect(cx - w / 2, bot - s / 5, w, w, c);
}

void icon_draw(int id, int x, int y, int size, uint32_t color) {
    if (size <= 0) return;
    switch (id) {
    case IC_START:    draw_start(x, y, size, color); break;
    case IC_SEARCH:   draw_search(x, y, size, color); break;
    case IC_TASKVIEW: draw_taskview(x, y, size, color); break;
    case IC_BROWSER:  draw_browser(x, y, size, color); break;
    case IC_TERMINAL: draw_terminal(x, y, size, color); break;
    case IC_FILES:    draw_files(x, y, size, color); break;
    case IC_NOTES:    draw_notes(x, y, size, color); break;
    case IC_SETTINGS: draw_settings(x, y, size, color); break;
    case IC_TASKMGR:  draw_taskmgr(x, y, size, color); break;
    case IC_CALC:     draw_calc(x, y, size, color); break;
    case IC_PAINT:    draw_paint(x, y, size, color); break;
    case IC_TRASH:    draw_trash(x, y, size, color); break;
    case IC_PC:       draw_pc(x, y, size, color); break;
    case IC_POWER:    draw_power(x, y, size, color); break;
    case IC_USER:     draw_user(x, y, size, color); break;
    case IC_WIFI:     draw_wifi(x, y, size, color); break;
    case IC_SPEAKER:  draw_speaker(x, y, size, color); break;
    case IC_BATTERY:  draw_battery(x, y, size, color); break;
    case IC_CHEVUP:   draw_chevup(x, y, size, color); break;
    case IC_SYNC:     draw_sync(x, y, size, color); break;
    case IC_BACK:     draw_back(x, y, size, color); break;
    case IC_UPDIR:    draw_updir(x, y, size, color); break;
    case IC_CLOSE:    draw_close(x, y, size, color); break;
    case IC_MIN:      draw_min(x, y, size, color); break;
    case IC_MAX:      draw_max(x, y, size, color); break;
    case IC_RESTORE:  draw_restore(x, y, size, color); break;
    case IC_PLUS:     draw_plus(x, y, size, color); break;
    case IC_CHECK:    draw_check(x, y, size, color); break;
    case IC_BOOKMARK: draw_bookmark(x, y, size, color); break;
    case IC_GRID:     draw_grid(x, y, size, color); break;
    case IC_XENOX:    draw_xenox(x, y, size, color); break;
    case IC_FORWARD:  draw_forward(x, y, size, color); break;
    case IC_RELOAD:   draw_reload(x, y, size, color); break;
    case IC_HOME:     draw_home(x, y, size, color); break;
    case IC_LOCK:     draw_lock(x, y, size, color); break;
    case IC_DOTS:     draw_dots(x, y, size, color); break;
    case IC_CLOCK:    draw_clock(x, y, size, color); break;
    case IC_DOWNLOAD: draw_download(x, y, size, color); break;
    case IC_HISTORY:  draw_clock(x, y, size, color); break;
    case IC_GLOBE:    draw_globe(x, y, size, color); break;
    case IC_WARN:     draw_warn(x, y, size, color); break;
    default: break;
    }
}
