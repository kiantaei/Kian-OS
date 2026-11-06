// Kian OS GUI - window manager implementation.
#include "wm.h"
#include "desktop.h"

extern "C" char *strcpy(char *dst, const char *src);

int WM::open(int type, const char *title, int w, int h, int screen_w) {
    Window *ex = find_type(type);
    if (ex != 0) { // one instance per app: restore + raise
        ex->minimized = false;
        raise(ex->id);
        return ex->id;
    }
    Window nw;
    nw.id = next_id++;
    nw.type = type;
    nw.w = w;
    nw.h = h;
    int cascade = (wins.size() % 5) * 24;
    nw.x = 80 + cascade;
    nw.y = 48 + cascade;
    if (nw.x + nw.w > screen_w) nw.x = screen_w - nw.w - 8;
    for (int i = 0; i < 27 && title[i] != '\0'; i++) nw.title[i] = title[i];
    nw.title[27] = '\0';
    nw.minimized = false;
    nw.maximized = false;
    nw.prev_x = nw.x; nw.prev_y = nw.y; nw.prev_w = nw.w; nw.prev_h = nw.h;
    if (!wins.push(nw)) return -1;
    return nw.id;
}

Window *WM::find(int id) {
    for (int i = 0; i < wins.size(); i++) {
        if (wins[i].id == id) return &wins[i];
    }
    return 0;
}

Window *WM::find_type(int type) {
    for (int i = 0; i < wins.size(); i++) {
        if (wins[i].type == type) return &wins[i];
    }
    return 0;
}

void WM::close(int id) {
    for (int i = 0; i < wins.size(); i++) {
        if (wins[i].id == id) {
            wins.remove_at(i);
            return;
        }
    }
}

void WM::raise(int id) {
    for (int i = 0; i < wins.size(); i++) {
        if (wins[i].id == id && i != wins.size() - 1) {
            Window top = wins[i];
            wins.remove_at(i);
            wins.push(top);
            return;
        }
    }
}

void WM::restore(Window *w) {
    if (w->maximized) {
        w->maximized = false;
        w->x = w->prev_x; w->y = w->prev_y;
        w->w = w->prev_w; w->h = w->prev_h;
    }
    w->minimized = false;
}

Window *WM::topmost_at(int x, int y) {
    for (int i = wins.size() - 1; i >= 0; i--) {
        Window &w = wins[i];
        if (w.minimized) continue;
        if (x >= w.x && x < w.x + w.w && y >= w.y && y < w.y + w.h) return &w;
    }
    return 0;
}

Rect WM::content_rect(const Window &w, int screen_w, int screen_h) const {
    (void)screen_w;
    if (w.maximized) {
        Rect r = { 1, TITLE_H, 0, 0 };
        r.w = w.w - 2;
        r.h = w.h - TITLE_H - 1;
        return r;
    }
    Rect r = { w.x + 1, w.y + TITLE_H, w.w - 2, w.h - TITLE_H - 1 };
    (void)screen_h;
    return r;
}

int WM::button_at(const Window &w, int px, int py) const {
    if (py < w.y || py >= w.y + TITLE_H) return BTN_NONE;
    if (px >= w.x + w.w - 16 && px < w.x + w.w - 4) return BTN_CLOSE;
    if (px >= w.x + w.w - 30 && px < w.x + w.w - 18) return BTN_MAX;
    if (px >= w.x + w.w - 44 && px < w.x + w.w - 32) return BTN_MIN;
    return BTN_NONE;
}

bool WM::title_hit(const Window &w, int px, int py) const {
    if (py < w.y || py >= w.y + TITLE_H) return false;
    return px >= w.x && px < w.x + w.w - 46;
}

void WM::draw_one(Surface &s, const Theme &th, int idx, int screen_w, int screen_h) {
    if (idx < 0 || idx >= wins.size()) return;
    Window &w = wins[idx];
    if (w.minimized) return;
    Rect c = content_rect(w, screen_w, screen_h);

    // drop shadow (alpha blended over what is already on screen, offset 3,3)
    for (int sy = w.y + 3; sy < w.y + w.h + 3; sy++) {
        for (int sx = w.x + 3; sx < w.x + w.w + 3; sx++) {
            bool in_win = (sx < w.x + w.w && sy < w.y + w.h);
            if (!in_win && sx >= 0 && sy >= 0 && sx < s.w && sy < s.h) {
                s.buf[(size_t)sy * s.w + sx] = blend(s.buf[(size_t)sy * s.w + sx], th.shadow, 96);
            }
        }
    }

    // frame + body
    fill_round_rect(s, w.x, w.y, w.w, w.h, 4, th.win_frame);
    fill_rect(s, c.x, c.y, c.w, c.h, th.win_bg);

    // title bar
    fill_rect(s, w.x + 1, w.y + 1, w.w - 2, TITLE_H - 2, th.title_bg);
    draw_text(s, w.x + 6, w.y + 5, w.title, th.title_text, 1);

    // window buttons: minimize, maximize, close
    int by = w.y + 3;
    fill_round_rect(s, w.x + w.w - 32, by, 12, 12, 2, th.button_bg);
    hline(s, w.x + w.w - 30, by + 9, 8, th.button_fg);                    // minimize _
    fill_round_rect(s, w.x + w.w - 48, by, 12, 12, 2, th.button_bg);
    vline(s, w.x + w.w - 46, by + 3, 6, th.button_fg);                    // maximize []
    hline(s, w.x + w.w - 46, by + 3, 6, th.button_fg);
    vline(s, w.x + w.w - 41, by + 3, 6, th.button_fg);
    hline(s, w.x + w.w - 46, by + 8, 6, th.button_fg);
    fill_round_rect(s, w.x + w.w - 16, by, 12, 12, 2, th.close_btn_bg);
    draw_text(s, w.x + w.w - 15, by + 2, "X", th.button_fg, 1);           // close X
}

void WM::draw(Surface &s, const Theme &th, int screen_w, int screen_h) {
    for (int i = 0; i < wins.size(); i++) {
        draw_one(s, th, i, screen_w, screen_h);
    }
}
