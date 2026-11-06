// Kian OS GUI - window manager implementation.
#include "wm.h"
#include "theme.h"
#include "icons.h"
#include "input.h"
#include "util.h"

static const int BTN_CLOSE_W = 34, BTN_OTHER_W = 30;

void wm_init(WM *wm) {
    for (int i = 0; i < WM_MAX_WINDOWS; i++) {
        wm->win[i].used = false;
        wm->win[i].id = 0;
        wm->win[i].inst = 0;
    }
    wm->count = 0;
    wm->order_n = 0;
    wm->next_id = 1;
    wm->focused = 0;
    wm->cur_desk = 0;
    wm->dragging = false;
    wm->drag_id = wm->drag_offx = wm->drag_offy = 0;
    wm->resize_id = 0;
    wm->resize_mode = RS_NONE;
    wm->rs_x = wm->rs_y = wm->rs_w = wm->rs_h = 0;
    wm->snap_zone = 0;
    wm->did_move = false;
}

static void order_remove(WM *wm, int idx) {
    for (int i = idx; i < wm->order_n - 1; i++) wm->order[i] = wm->order[i + 1];
    wm->order_n--;
}

static void order_push(WM *wm, int idx) {
    if (wm->order_n < WM_MAX_WINDOWS) wm->order[wm->order_n++] = idx;
}

Window *wm_open(WM *wm, int app, const char *title, int icon, int w, int h, int desk_id) {
    int slot = -1;
    for (int i = 0; i < WM_MAX_WINDOWS; i++) {
        if (!wm->win[i].used) { slot = i; break; }
    }
    if (slot < 0) return 0; // pool exhausted
    Window *nw = &wm->win[slot];
    int tw = imin(31, (int)strlen(title));
    for (int i = 0; i < tw; i++) nw->title[i] = title[i];
    nw->title[tw] = '\0';
    nw->used = true;
    nw->id = wm->next_id++;
    nw->app = app;
    nw->w = imax(WM_MIN_W, w);
    nw->h = imax(WM_MIN_H, h);
    int cascade = (wm->count % 5) * 24;
    nw->x = imax(8, 70 + cascade);
    nw->y = imax(8, 40 + cascade);
    if (GFX && nw->x + nw->w > GFX->w - 8) nw->x = imax(0, GFX->w - nw->w - 8);
    if (GFX && nw->y + nw->h > GFX->h - 48 - 8) nw->y = imax(0, GFX->h - 48 - nw->h - 8);
    nw->icon = icon;
    nw->state = WS_NORM;
    nw->rx = nw->x; nw->ry = nw->y; nw->rw = nw->w; nw->rh = nw->h;
    nw->desk_id = desk_id;
    nw->dirty = true;
    nw->born_ms = now_ms();
    nw->inst = 0;
    nw->draw = 0;
    nw->event = 0;
    nw->destroy = 0;
    wm->count++;
    order_push(wm, slot);
    wm->focused = nw->id;
    return nw;
}

void wm_close_id(WM *wm, int id) {
    int idx = -1;
    for (int i = 0; i < WM_MAX_WINDOWS; i++) {
        if (wm->win[i].used && wm->win[i].id == id) { idx = i; break; }
    }
    if (idx < 0) return;
    Window *w = &wm->win[idx];
    if (w->destroy && w->inst) w->destroy(w->inst);
    w->inst = 0;
    w->used = false;
    wm->count--;
    for (int i = 0; i < wm->order_n; i++) {
        if (wm->order[i] == idx) { order_remove(wm, i); break; }
    }
    if (wm->focused == id) {
        wm->focused = 0;
        Window *t = wm_topmost(wm, w->desk_id);
        if (t) wm->focused = t->id;
    }
    wm->dragging = false;
    wm->resize_id = 0;
    wm->resize_mode = RS_NONE;
}

void wm_close_app(WM *wm, int app) {
    for (int i = 0; i < WM_MAX_WINDOWS; i++) {
        if (wm->win[i].used && wm->win[i].app == app) wm_close_id(wm, wm->win[i].id);
    }
}

Window *wm_get(WM *wm, int id) {
    for (int i = 0; i < WM_MAX_WINDOWS; i++) {
        if (wm->win[i].used && wm->win[i].id == id) return &wm->win[i];
    }
    return 0;
}

static bool win_visible(const Window *w, int cur_desk) {
    return w->used && w->desk_id == cur_desk && w->state != WS_MIN;
}

Window *wm_top_at(WM *wm, int x, int y, int cur_desk) {
    for (int i = wm->order_n - 1; i >= 0; i--) {
        Window *w = &wm->win[wm->order[i]];
        if (!win_visible(w, cur_desk)) continue;
        if (rect_hit(x, y, Rect{ w->x, w->y, w->w, w->h })) return w;
    }
    return 0;
}

Window *wm_topmost(WM *wm, int cur_desk) {
    for (int i = wm->order_n - 1; i >= 0; i--) {
        Window *w = &wm->win[wm->order[i]];
        if (win_visible(w, cur_desk)) return w;
    }
    return 0;
}

Window *wm_focused(WM *wm) {
    return wm_get(wm, wm->focused);
}

void wm_raise(WM *wm, Window *w) {
    if (!w || !w->used) return;
    int idx = (int)(w - wm->win);
    for (int i = 0; i < wm->order_n; i++) {
        if (wm->order[i] == idx) {
            for (int j = i; j < wm->order_n - 1; j++) wm->order[j] = wm->order[j + 1];
            wm->order[wm->order_n - 1] = idx;
            return;
        }
    }
}

void wm_focus(WM *wm, Window *w) {
    if (!w || !w->used) return;
    wm_raise(wm, w);
    wm->focused = w->id;
}

Rect wm_content(const Window *w) {
    Rect r = { w->x + 1, w->y + WM_TITLE_H, w->w - 2, w->h - WM_TITLE_H - 1 };
    return r;
}

int wm_button_at(const Window *w, int px, int py) {
    if (py < w->y || py >= w->y + WM_TITLE_H) return 0;
    int bx = w->x + w->w - BTN_CLOSE_W;
    if (px >= bx && px < w->x + w->w) return 3;                    // close
    bx -= BTN_OTHER_W;
    if (px >= bx && px < bx + BTN_OTHER_W) return 2;               // max
    bx -= BTN_OTHER_W;
    if (px >= bx && px < bx + BTN_OTHER_W) return 1;               // min
    return 0;
}

int wm_resize_at(const Window *w, int px, int py) {
    if (w->state == WS_MAX) return RS_NONE;
    int m = RS_NONE;
    if (px >= w->x && px < w->x + WM_EDGE) m |= RS_L;
    if (px >= w->x + w->w - WM_EDGE && px < w->x + w->w) m |= RS_R;
    if (py >= w->y && py < w->y + WM_EDGE) m |= RS_T;
    if (py >= w->y + w->h - WM_EDGE && py < w->y + w->h) m |= RS_B;
    return m;
}

static bool wm_title_hit(const Window *w, int px, int py) {
    if (py < w->y || py >= w->y + WM_TITLE_H) return false;
    return px >= w->x && px < w->x + w->w - (BTN_CLOSE_W + 2 * BTN_OTHER_W + 4);
}

void wm_toggle_max(Window *w, int screen_w, int screen_h) {
    if (!w) return;
    if (w->state == WS_MAX) {
        w->state = WS_NORM;
        w->x = w->rx; w->y = w->ry; w->w = w->rw; w->h = w->rh;
    } else {
        w->rx = w->x; w->ry = w->y; w->rw = w->w; w->rh = w->h;
        w->state = WS_MAX;
        w->x = 0; w->y = 0;
        w->w = screen_w; w->h = screen_h - 48;
    }
    w->dirty = true;
}

Rect wm_snap_rect(int zone, int screen_w, int screen_h) {
    Rect r = { 0, 0, 0, 0 };
    if (zone == 1) { r.x = 0; r.y = 0; r.w = screen_w / 2; r.h = screen_h - 48; }
    else if (zone == 2) { r.x = screen_w / 2; r.y = 0; r.w = screen_w - screen_w / 2; r.h = screen_h - 48; }
    else if (zone == 3) { r.x = 0; r.y = 0; r.w = screen_w; r.h = screen_h - 48; }
    return r;
}

void wm_minimize_all(WM *wm, int cur_desk) {
    for (int i = 0; i < WM_MAX_WINDOWS; i++) {
        Window *w = &wm->win[i];
        if (w->used && w->desk_id == cur_desk && w->state != WS_MIN) {
            w->rx = w->x; w->ry = w->y; w->rw = w->w; w->rh = w->h;
            w->state = WS_MIN;
        }
    }
    wm->focused = 0;
}

void wm_restore_all(WM *wm, int cur_desk) {
    for (int i = 0; i < WM_MAX_WINDOWS; i++) {
        Window *w = &wm->win[i];
        if (w->used && w->desk_id == cur_desk && w->state == WS_MIN) {
            w->state = WS_NORM;
            w->x = w->rx; w->y = w->ry; w->w = w->rw; w->h = w->rh;
        }
    }
}

int wm_count_on_desk(WM *wm, int desk) {
    int n = 0;
    for (int i = 0; i < WM_MAX_WINDOWS; i++) {
        if (wm->win[i].used && wm->win[i].desk_id == desk) n++;
    }
    return n;
}

// ---------------- drawing ----------------

void wm_draw_one(WM *wm, Window *w) {
    if (!GFX || !w->used || w->state == WS_MIN) return;
    (void)wm;
    clip_reset();

    // shadow + frame
    shadow_rect(w->x, w->y, w->w, w->h);
    fill_round_rect(w->x, w->y, w->w, w->h, 8, THEME->win_frame);
    if (w->id == wm->focused) {
        round_rect_outline(w->x, w->y, w->w, w->h, 8, THEME->accent);
    }

    // title bar
    fill_rect(w->x + 1, w->y + 1, w->w - 2, WM_TITLE_H - 1, THEME->title_bg);
    icon_draw(w->icon, w->x + 8, w->y + (WM_TITLE_H - 16) / 2, 16, THEME->accent);
    draw_text_bold(w->x + 30, w->y + (WM_TITLE_H - 8) / 2, w->title, THEME->title_text, 1);

    // caption buttons (close red on hover)
    int by = w->y + (WM_TITLE_H - 16) / 2;
    icon_draw(IC_MIN, w->x + w->w - BTN_CLOSE_W - BTN_OTHER_W * 2 + 7, by, 16, THEME->title_text);
    icon_draw(w->state == WS_MAX ? IC_RESTORE : IC_MAX, w->x + w->w - BTN_CLOSE_W - BTN_OTHER_W + 7, by, 16, THEME->title_text);
    bool close_hover = mouse_in(w->x + w->w - BTN_CLOSE_W, w->y, BTN_CLOSE_W, WM_TITLE_H);
    fill_rect(w->x + w->w - BTN_CLOSE_W, w->y + 1, BTN_CLOSE_W - 1, WM_TITLE_H - 1,
              close_hover ? THEME->close_btn_bg : THEME->title_bg);
    icon_draw(IC_CLOSE, w->x + w->w - BTN_CLOSE_W + 9, by, 16, close_hover ? 0x00FFFFFF : THEME->title_text);

    // content: app draws inside its rect (clipped)
    Rect c = wm_content(w);
    fill_rect(c.x, c.y, c.w, c.h, THEME->win_bg);
    if (w->draw) {
        clip_set(c);
        w->draw(w->inst, c);
        clip_reset();
    }

    // open animation: fade the whole window in over ~150 ms
    unsigned dt = now_ms() - w->born_ms;
    if (dt < 150) {
        int alpha = 160 - (int)(((int64_t)dt * 160) / 150);
        blend_rect(w->x, w->y, w->w, w->h, 0x00000000, iclamp(alpha, 0, 160));
    }
}

void wm_draw_all(WM *wm, int cur_desk) {
    for (int i = 0; i < wm->order_n; i++) {
        Window *w = &wm->win[wm->order[i]];
        if (win_visible(w, cur_desk)) wm_draw_one(wm, w);
    }
}

// ---------------- mouse routing ----------------

bool wm_mouse(WM *wm, const kian_event_t *e) {
    if (!GFX || !IN) return false;
    int cur_desk = wm->cur_desk;

    if (e->type == KIAN_EV_MOUSE_MOVE) {
        if (wm->dragging) {
            Window *w = wm_get(wm, wm->drag_id);
            if (!w) { wm->dragging = false; return true; }
            w->x = IN->mx - wm->drag_offx;
            w->y = IN->my - wm->drag_offy;
            if (w->x < -w->w + 40) w->x = -w->w + 40;
            if (w->x > GFX->w - 40) w->x = GFX->w - 40;
            if (w->y < 0) w->y = 0;
            if (w->y > GFX->h - 48 - WM_TITLE_H) w->y = GFX->h - 48 - WM_TITLE_H;
            // snap zones
            wm->snap_zone = 0;
            if (IN->mx <= 2) wm->snap_zone = 1;
            else if (IN->mx >= GFX->w - 3) wm->snap_zone = 2;
            else if (IN->my <= 2) wm->snap_zone = 3;
            wm->did_move = true;
            return true;
        }
        if (wm->resize_mode != RS_NONE) {
            Window *w = wm_get(wm, wm->resize_id);
            if (!w) { wm->resize_mode = RS_NONE; return true; }
            int m = wm->resize_mode;
            if (m & RS_L) {
                int nx = w->x + (IN->mx - wm->drag_offx);
                int nw2 = w->w - (nx - w->x);
                if (nw2 >= WM_MIN_W) { w->x = nx; w->w = nw2; }
            }
            if (m & RS_R) w->w = imax(WM_MIN_W, IN->mx - w->x + 1);
            if (m & RS_T) {
                int ny = w->y + (IN->my - wm->drag_offy);
                int nh2 = w->h - (ny - w->y);
                if (nh2 >= WM_MIN_H) { w->y = ny; w->h = nh2; }
            }
            if (m & RS_B) w->h = imax(WM_MIN_H, IN->my - w->y + 1);
            w->dirty = true;
            wm->did_move = true;
            return true;
        }
        return false;
    }

    if (e->type == KIAN_EV_MOUSE_BTN && IN->press_l) {
        Window *w = wm_top_at(wm, IN->mx, IN->my, cur_desk);
        if (!w) return false;
        wm_focus(wm, w);
        int btn = wm_button_at(w, IN->mx, IN->my);
        if (btn == 1) { w->state = WS_MIN; wm->focused = 0; return true; }
        if (btn == 2) { wm_toggle_max(w, GFX->w, GFX->h); return true; }
        if (btn == 3) { wm_close_id(wm, w->id); return true; }
        if (wm_title_hit(w, IN->mx, IN->my)) {
            if (w->state == WS_MAX) {
                // pull out of maximized, centered under cursor
                w->state = WS_NORM;
                w->x = IN->mx - w->rw / 2;
                w->y = imax(0, IN->my - WM_TITLE_H / 2);
                w->w = w->rw; w->h = w->rh;
                if (w->x < 0) w->x = 0;
            }
            wm->dragging = true;
            wm->drag_id = w->id;
            wm->drag_offx = IN->mx - w->x;
            wm->drag_offy = IN->my - w->y;
            wm->snap_zone = 0;
            return true;
        }
        int rs = wm_resize_at(w, IN->mx, IN->my);
        if (rs != RS_NONE) {
            wm->resize_id = w->id;
            wm->resize_mode = rs;
            wm->drag_offx = IN->mx;
            wm->drag_offy = IN->my;
            return true;
        }
        return false; // content click -> app
    }

    if (e->type == KIAN_EV_MOUSE_BTN && IN->release_l) {
        bool was = wm->dragging || wm->resize_mode != RS_NONE;
        if (wm->dragging) {
            Window *w = wm_get(wm, wm->drag_id);
            if (w && wm->snap_zone != 0) {
                Rect r = wm_snap_rect(wm->snap_zone, GFX->w, GFX->h);
                w->rx = w->x; w->ry = w->y; w->rw = w->w; w->rh = w->h;
                w->x = r.x; w->y = r.y; w->w = r.w; w->h = r.h;
                if (wm->snap_zone == 3) { w->state = WS_MAX; }
                w->dirty = true;
            }
            wm->dragging = false;
            wm->drag_id = 0;
            wm->snap_zone = 0;
            wm->did_move = true;
        }
        wm->resize_mode = RS_NONE;
        wm->resize_id = 0;
        return was;
    }

    if (e->type == KIAN_EV_MOUSE_BTN && IN->press_r) {
        // right-click: consume only on decorations; content clicks go to the app
        Window *w = wm_top_at(wm, IN->mx, IN->my, cur_desk);
        if (!w) return false;
        wm_focus(wm, w);
        return wm_button_at(w, IN->mx, IN->my) != 0 || wm_title_hit(w, IN->mx, IN->my);
    }
    return false;
}
