// Kian OS GUI - Paint (bonus): canvas, 8 colors, 3 brush sizes, eraser, clear.
#include "../apps.h"
#include "../gui.h"
#include "../input.h"
#include "../widgets.h"
#include "../util.h"

static const uint32_t PAINT_COLORS[8] = {
    0x00000000, 0x00FFFFFF, 0x00E03030, 0x00F0A020,
    0x00F0E040, 0x0030B050, 0x003070D0, 0x00A040C0
};

const int P_CW = 480, P_CH = 280;

struct PaintApp {
    uint32_t *canvas;
    int color;    // index into PAINT_COLORS (7 = eraser means use white)
    bool erase;
    int size;     // 2 / 4 / 8
    bool drawing;
    bool dirty;
};

static void paint_dot(PaintApp *p, int x, int y, int r, uint32_t c) {
    for (int dy = -r; dy <= r; dy++) {
        for (int dx = -r; dx <= r; dx++) {
            if (dx * dx + dy * dy <= r * r) {
                int px = x + dx, py = y + dy;
                if (px >= 0 && px < P_CW && py >= 0 && py < P_CH) {
                    p->canvas[(size_t)py * P_CW + px] = c;
                }
            }
        }
    }
}

void *paint_create() {
    PaintApp *p = new PaintApp();
    p->canvas = (uint32_t *)kian_alloc((size_t)P_CW * (size_t)P_CH * 4, 16);
    if (p->canvas) {
        for (int i = 0; i < P_CW * P_CH; i++) p->canvas[i] = 0x00FFFFFF;
    }
    p->color = 6;
    p->erase = false;
    p->size = 4;
    p->drawing = false;
    p->dirty = true;
    return p;
}

void paint_destroy(void *inst) {
    PaintApp *p = (PaintApp *)inst;
    if (p->canvas) kian_free(p->canvas);
    delete p;
}

void paint_draw(void *inst, Rect r) {
    PaintApp *p = (PaintApp *)inst;
    fill_rect(r.x, r.y, r.w, r.h, THEME->win_bg);

    // toolbar: 8 colors, eraser, 3 sizes, clear
    int tx = r.x + 8, ty = r.y + 8;
    for (int i = 0; i < 8; i++) {
        fill_round_rect(tx, ty, 22, 22, 4, PAINT_COLORS[i]);
        if (i == p->color && !p->erase) round_rect_outline(tx - 2, ty - 2, 26, 26, 4, THEME->accent);
        tx += 28;
    }
    tx += 8;
    bool hov = mouse_in(tx, ty, 58, 22);
    fill_round_rect(tx, ty, 58, 22, 4, p->erase ? THEME->accent : (hov ? THEME->start_bg_active : THEME->button_bg));
    draw_text(tx + 12, ty + 7, "Eraser", p->erase ? 0x00FFFFFF : THEME->button_fg, 1);
    tx += 66;
    static const char *sz[3] = { "S", "M", "L" };
    static const int szv[3] = { 2, 4, 8 };
    for (int i = 0; i < 3; i++) {
        bool sh = mouse_in(tx, ty, 30, 22);
        fill_round_rect(tx, ty, 30, 22, 4, (p->size == szv[i]) ? THEME->accent : (sh ? THEME->start_bg_active : THEME->button_bg));
        draw_text(tx + 12, ty + 7, sz[i], (p->size == szv[i]) ? 0x00FFFFFF : THEME->button_fg, 1);
        tx += 34;
    }
    fill_round_rect(tx, ty, 50, 22, 4, mouse_in(tx, ty, 50, 22) ? THEME->start_bg_active : THEME->button_bg);
    draw_text(tx + 12, ty + 7, "Clear", THEME->button_fg, 1);

    // canvas (blit with clipping)
    Rect cv = { r.x + 8, r.y + 40, imin(P_CW, r.w - 16), imin(P_CH, r.h - 48) };
    fill_round_rect(cv.x, cv.y, cv.w, cv.h, 2, 0x00FFFFFF);
    blit(p->canvas, P_CW, 0, 0, cv.x, cv.y, cv.w, cv.h);
    rect_outline(cv.x, cv.y, cv.w, cv.h, THEME->menu_border);
}

static Rect paint_canvas_rect(Rect r) {
    Rect cv = { r.x + 8, r.y + 40, imin(P_CW, r.w - 16), imin(P_CH, r.h - 48) };
    return cv;
}

void paint_event(void *inst, const kian_event_t *e, Rect r) {
    PaintApp *p = (PaintApp *)inst;
    Rect cv = paint_canvas_rect(r);
    int tx = r.x + 8, ty = r.y + 8;

    if (e->type == KIAN_EV_MOUSE_BTN && IN->press_l) {
        // toolbar hits
        for (int i = 0; i < 8; i++) {
            if (mouse_in(tx + i * 28, ty, 22, 22)) { p->color = i; p->erase = false; return; }
        }
        int tx2 = tx + 8 * 28 + 8;
        if (mouse_in(tx2, ty, 58, 22)) { p->erase = !p->erase; return; }
        tx2 += 66;
        static const int szv[3] = { 2, 4, 8 };
        for (int i = 0; i < 3; i++) {
            if (mouse_in(tx2 + i * 34, ty, 30, 22)) { p->size = szv[i]; return; }
        }
        tx2 += 3 * 34;
        if (mouse_in(tx2, ty, 50, 22)) {
            if (p->canvas) {
                for (int i = 0; i < P_CW * P_CH; i++) p->canvas[i] = 0x00FFFFFF;
            }
            p->dirty = true;
            return;
        }
        if (rect_hit(IN->mx, IN->my, cv)) p->drawing = true;
    }
    if (e->type == KIAN_EV_MOUSE_BTN && IN->release_l) p->drawing = false;

    if (p->drawing && p->canvas && (e->type == KIAN_EV_MOUSE_MOVE || e->type == KIAN_EV_MOUSE_BTN)) {
        if (rect_hit(IN->mx, IN->my, cv)) {
            uint32_t c = p->erase ? 0x00FFFFFF : PAINT_COLORS[p->color];
            paint_dot(p, IN->mx - cv.x, IN->my - cv.y, p->size, c);
            shell_mark_content(); // repaint canvas region
        }
    }
}
