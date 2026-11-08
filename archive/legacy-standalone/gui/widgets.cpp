// Kian OS GUI - widget implementations (all integer math, theme-driven).
#include "widgets.h"
#include "theme.h"
#include "icons.h"
#include "input.h"
#include "util.h"
#include "font.h"

// ================= Button =================

void btn_init(Btn *b, int x, int y, int w, int h, const char *label, int icon) {
    b->x = x; b->y = y; b->w = w; b->h = h;
    b->label = label;
    b->icon = icon;
    b->hover = b->armed = b->disabled = false;
}

void btn_draw(const Btn *b) {
    uint32_t bg = b->hover ? THEME->start_bg_active : THEME->button_bg;
    uint32_t fg = THEME->button_fg;
    if (b->disabled) { bg = THEME->icon_bg; fg = THEME->text_dim; }
    fill_round_rect(b->x, b->y, b->w, b->h, 6, bg);
    round_rect_outline(b->x, b->y, b->w, b->h, 6, THEME->menu_border);
    int tw = b->label ? text_width(b->label, 1) : 0;
    int iw = (b->icon != IC_NONE) ? 16 : 0;
    int total = tw + iw + ((iw && tw) ? 6 : 0);
    int tx = b->x + (b->w - total) / 2;
    int ty = text_center_y(b->y, b->h, 1);
    if (b->icon != IC_NONE) {
        icon_draw(b->icon, tx, b->y + (b->h - 16) / 2, 16, fg);
        tx += iw + 6;
    }
    if (b->label) draw_text(tx, ty, b->label, fg, 1);
}

bool btn_event(Btn *b, const kian_event_t *e) {
    b->hover = mouse_in(b->x, b->y, b->w, b->h) && !b->disabled;
    if (e == 0 || e->type == KIAN_EV_MOUSE_MOVE) return false;
    if (e->type == KIAN_EV_MOUSE_BTN) {
        bool inside = mouse_in(b->x, b->y, b->w, b->h);
        if (IN->press_l) b->armed = inside;
        if (IN->release_l && b->armed && inside && !b->disabled) {
            b->armed = false;
            return true;
        }
        if (IN->release_l && !inside) b->armed = false;
    }
    return false;
}

// ================= Label =================

void label_draw(int x, int y, const char *s, uint32_t color, int scale, bool bold) {
    if (bold) draw_text_bold(x, y, s, color, scale);
    else draw_text(x, y, s, color, scale);
}

// ================= TextBox =================

void tbox_init(TBox *t, int x, int y, int w, int h, bool password) {
    t->buf[0] = '\0';
    t->len = 0;
    t->caret = 0;
    t->x = x; t->y = y; t->w = w; t->h = h;
    t->focused = false;
    t->password = password;
    t->err_until = 0;
}

void tbox_set_text(TBox *t, const char *s) {
    int i = 0;
    for (; i < 127 && s[i] != '\0'; i++) t->buf[i] = s[i];
    t->buf[i] = '\0';
    t->len = i;
    t->caret = i;
}

const char *tbox_text(const TBox *t) { return t->buf; }

void tbox_draw(const TBox *t) {
    uint32_t bg = THEME->input_bg;
    uint32_t border = t->focused ? THEME->accent : THEME->menu_border;
    // shake on error: horizontal offset
    int shk = 0;
    unsigned now = now_ms();
    if (t->err_until > now) shk = ((now / 40) & 1) ? 3 : -3;
    int bx = t->x + shk;
    fill_round_rect(bx, t->y, t->w, t->h, 4, bg);
    round_rect_outline(bx, t->y, t->w, t->h, 4, border);
    int ty = text_center_y(t->y, t->h, 1);
    int tx = bx + 6;
    for (int i = 0; i < t->len; i++) {
        char ch = t->buf[i];
        char shown[2] = { ch, '\0' };
        draw_text(tx, ty, t->password ? "*" : shown, THEME->text, 1);
        tx += glyph_advance(t->password ? '*' : ch, 1);
    }
    if (t->focused && ((now / 400) & 1) == 0) { // caret blink
        int cx2 = bx + 6;
        for (int i = 0; i < t->caret && i < t->len; i++) {
            char ch = t->buf[i];
            cx2 += glyph_advance(t->password ? '*' : ch, 1);
        }
        vline(cx2 + 1, ty + FONT_UI_CAP_TOP - 1,
              (FONT_UI_CAP_BOT - FONT_UI_CAP_TOP + 1) + 2, THEME->accent);
    }
}

void tbox_focus(TBox *t, bool on) { t->focused = on; }

bool tbox_key(TBox *t, const kian_event_t *e) {
    if (!t->focused || e->type != KIAN_EV_KEY_DOWN) return false;
    int a = e->a, b = e->b;
    if (a == KIAN_KEY_ENTER || b == '\n') return true;
    if (a == KIAN_KEY_BACKSPACE || b == 8) {
        if (t->caret > 0) {
            for (int i = t->caret - 1; i < t->len - 1; i++) t->buf[i] = t->buf[i + 1];
            t->len--; t->caret--;
            t->buf[t->len] = '\0';
        }
        return false;
    }
    if (a == KIAN_KEY_DEL) {
        if (t->caret < t->len) {
            for (int i = t->caret; i < t->len - 1; i++) t->buf[i] = t->buf[i + 1];
            t->len--;
            t->buf[t->len] = '\0';
        }
        return false;
    }
    if (a == KIAN_KEY_LEFT)  { if (t->caret > 0) t->caret--; return false; }
    if (a == KIAN_KEY_RIGHT) { if (t->caret < t->len) t->caret++; return false; }
    if (a == KIAN_KEY_HOME)  { t->caret = 0; return false; }
    if (a == KIAN_KEY_END)   { t->caret = t->len; return false; }
    if (!(e->c & 2) && b >= 32 && b < 127 && t->len < 127) {
        // plain printable only: Ctrl+letter (e.g. Ctrl+C) must never type a
        // character - this box has no clipboard/selection support
        for (int i = t->len; i > t->caret; i--) t->buf[i] = t->buf[i - 1];
        t->buf[t->caret++] = (char)b;
        t->len++;
        t->buf[t->len] = '\0';
    }
    return false;
}

bool tbox_mouse(TBox *t, const kian_event_t *e) {
    if (e->type != KIAN_EV_MOUSE_BTN || !IN->press_l) return false;
    if (!mouse_in(t->x, t->y, t->w, t->h)) return false;
    t->focused = true;
    // place caret near the clicked x
    int cx2 = t->x + 6;
    int old = t->caret;
    t->caret = 0;
    for (int i = 0; i < t->len; i++) {
        int adv = glyph_advance(t->password ? '*' : t->buf[i], 1);
        if (IN->mx < cx2 + adv / 2) break;
        cx2 += adv;
        t->caret = i + 1;
    }
    (void)old;
    return true;
}

// ================= Checkbox =================

void chk_init(Chk *c, int x, int y, const char *label) {
    c->x = x; c->y = y; c->w = 220;
    c->label = label;
    c->checked = false;
    c->hover = false;
}

void chk_draw(const Chk *c) {
    uint32_t box = c->checked ? THEME->accent : THEME->input_bg;
    uint32_t border = c->hover ? THEME->accent : THEME->menu_border;
    round_rect_outline(c->x, c->y, 18, 18, 4, border);
    if (c->checked) fill_round_rect(c->x + 2, c->y + 2, 14, 14, 3, box);
    if (c->checked) icon_draw(IC_CHECK, c->x + 3, c->y + 3, 12, 0x00FFFFFF);
    draw_text(c->x + 26, text_center_y(c->y, 18, 1), c->label, THEME->text, 1);
}

bool chk_event(Chk *c, const kian_event_t *e) {
    c->hover = mouse_in(c->x, c->y, c->w, 20);
    if (e->type == KIAN_EV_MOUSE_BTN && IN->press_l && mouse_in(c->x, c->y, c->w, 20)) {
        c->checked = !c->checked;
        return true;
    }
    return false;
}

// ================= Slider =================

void slider_init(Slider *s, int x, int y, int w, int value) {
    s->x = x; s->y = y; s->w = w;
    s->value = iclamp(value, 0, 100);
    s->dragging = false;
}

void slider_draw(const Slider *s) {
    int ty = s->y + 7;
    fill_round_rect(s->x, ty, s->w, 6, 3, THEME->icon_bg);
    int kx = s->x + (s->w - 14) * s->value / 100;
    fill_round_rect(s->x, ty, kx - s->x + 14, 6, 3, THEME->accent);
    fill_circle(kx + 7, ty + 3, 7, THEME->accent);
    if (!s->dragging) {
        fill_circle(kx + 7, ty + 3, 6, 0x00FFFFFF);
        fill_circle(kx + 7, ty + 3, 4, THEME->accent);
    }
}

bool slider_event(Slider *s, const kian_event_t *e) {
    if (e->type == KIAN_EV_MOUSE_BTN) {
        if (IN->press_l && mouse_in(s->x - 8, s->y - 4, s->w + 16, 24)) s->dragging = true;
        if (IN->release_l) s->dragging = false;
    }
    if (s->dragging && (e->type == KIAN_EV_MOUSE_MOVE || e->type == KIAN_EV_MOUSE_BTN)) {
        int v = ((IN->mx - s->x) * 100) / (s->w - 1);
        v = iclamp(v, 0, 100);
        if (v != s->value) { s->value = v; return true; }
    }
    return false;
}

// ================= ListView =================

void listv_init(ListView *lv, int x, int y, int w, int h, int row_h) {
    lv->x = x; lv->y = y; lv->w = w; lv->h = h;
    lv->row_h = row_h > 8 ? row_h : 8;
    lv->n = 0;
    lv->offset = 0;
    lv->selected = -1;
    lv->wheel_lines = 3;
}

void listv_set_count(ListView *lv, int n) {
    lv->n = n;
    int vis = lv->h / lv->row_h;
    if (lv->offset > n - vis) lv->offset = n - vis;
    if (lv->offset < 0) lv->offset = 0;
    if (lv->selected >= n) lv->selected = n - 1;
}

bool listv_event(ListView *lv, const kian_event_t *e) {
    int vis = lv->h / lv->row_h;
    bool changed = false;
    if (e->type == KIAN_EV_MOUSE_WHEEL) {
        int dir = (e->a > 0) ? -lv->wheel_lines : lv->wheel_lines;
        int no = lv->offset + dir;
        if (no > lv->n - vis) no = lv->n - vis;
        if (no < 0) no = 0;
        if (no != lv->offset) { lv->offset = no; changed = true; }
    } else if (e->type == KIAN_EV_MOUSE_BTN && IN->press_l) {
        if (mouse_in(lv->x, lv->y, lv->w, lv->h)) {
            int row = (IN->my - lv->y) / lv->row_h + lv->offset;
            if (row >= 0 && row < lv->n) {
                lv->selected = row;
                changed = true;
            }
        }
    }
    return changed;
}

void listv_draw(const ListView *lv, const char *const *labels) {
    fill_rect(lv->x, lv->y, lv->w, lv->h, THEME->win_bg);
    int vis = lv->h / lv->row_h;
    for (int i = 0; i < vis && lv->offset + i < lv->n; i++) {
        int row = lv->offset + i;
        int ry = lv->y + i * lv->row_h;
        if (row == lv->selected) fill_rect(lv->x, ry, lv->w, lv->row_h, THEME->select_bg);
        if (labels && labels[row]) {
            draw_text(lv->x + 6, text_center_y(ry, lv->row_h, 1), labels[row], THEME->text, 1);
        }
    }
    // mini scrollbar
    if (lv->n > vis) {
        int bh = imax(8, lv->h * vis / lv->n);
        int by = lv->y + (lv->h - bh) * lv->offset / (lv->n - vis);
        fill_rect(lv->x + lv->w - 4, lv->y, 3, lv->h, THEME->icon_bg);
        fill_rect(lv->x + lv->w - 4, by, 3, bh, THEME->menu_border);
    }
}

bool listv_dbl_hit(const ListView *lv, int mx, int my) {
    if (!rect_hit(mx, my, Rect{ lv->x, lv->y, lv->w, lv->h })) return false;
    int row = (my - lv->y) / lv->row_h + lv->offset;
    return row >= 0 && row < lv->n;
}

// ================= ScrollBar =================

int sbar_track_len(const SBar *sb) {
    int vis = imax(1, imin(sb->visible, sb->total));
    return imax(12, sb->h * vis / imax(1, sb->total));
}

void sbar_set(SBar *sb, int total, int visible, int offset) {
    sb->total = total;
    sb->visible = visible;
    sb->offset = iclamp(offset, 0, imax(0, total - visible));
}

void sbar_draw(const SBar *sb) {
    fill_rect(sb->x, sb->y, 10, sb->h, THEME->icon_bg);
    if (sb->total > sb->visible && sb->total > 0) {
        int bh = sbar_track_len(sb);
        int track = sb->h - bh;
        int by = sb->y + track * sb->offset / imax(1, sb->total - sb->visible);
        fill_rect(sb->x + 2, by, 6, bh, THEME->menu_border);
    }
}

bool sbar_event(SBar *sb, const kian_event_t *e) {
    if (e->type == KIAN_EV_MOUSE_BTN) {
        if (IN->press_l && mouse_in(sb->x, sb->y, 10, sb->h)) {
            sb->dragging = true;
            sb->grab_dy = IN->my - sb->y;
        }
        if (IN->release_l) sb->dragging = false;
    }
    if (sb->dragging && e->type == KIAN_EV_MOUSE_MOVE) {
        if (sb->total <= sb->visible) return false;
        int bh = sbar_track_len(sb);
        int track = sb->h - bh;
        int off = ((IN->my - sb->grab_dy - sb->y) * imax(1, sb->total - sb->visible)) / imax(1, track);
        int old = sb->offset;
        sbar_set(sb, sb->total, sb->visible, off);
        return sb->offset != old;
    }
    return false;
}

// ================= Menu =================

static int menu_item_h(int i, const Menu *m) {
    return (m->items[i].id < 0) ? 9 : 28;
}

static int menu_height(const Menu *m) {
    int h = 8;
    for (int i = 0; i < m->n; i++) h += menu_item_h(i, m);
    return h;
}

void menu_open(Menu *m, const MenuItem *items, int n, void (*cb)(void *, int), void *ud, int x, int y) {
    if (!GFX) return;
    m->open = true;
    m->n = imin(n, 12);
    for (int i = 0; i < m->n; i++) m->items[i] = items[i];
    m->cb = cb;
    m->ud = ud;
    m->w = 160;
    for (int i = 0; i < m->n; i++) {
        if (m->items[i].id < 0) continue;
        int tw = text_width(m->items[i].label, 1) + 24;
        if (tw > m->w) m->w = tw;
    }
    m->x = iclamp(x, 0, GFX->w - m->w - 1);
    m->y = y;
    int mh = menu_height(m);
    if (m->y + mh > GFX->h) m->y = imax(0, GFX->h - mh - 1);
    m->hover = -1;
}

void menu_close(Menu *m) { m->open = false; m->hover = -1; }

void menu_draw(const Menu *m) {
    if (!m->open) return;
    int mh = menu_height(m);
    Rect r = { m->x, m->y, m->w, mh };
    clip_set(r);
    fill_round_rect(m->x, m->y, m->w, mh, 6, THEME->menu_border);
    fill_round_rect(m->x + 1, m->y + 1, m->w - 2, mh - 2, 5, THEME->menu_bg);
    int y = m->y + 4;
    for (int i = 0; i < m->n; i++) {
        int h = menu_item_h(i, m);
        if (m->items[i].id < 0) {
            hline(m->x + 8, y + 4, m->w - 16, THEME->menu_border);
        } else {
            if (i == m->hover) fill_round_rect(m->x + 4, y, m->w - 8, h, 4, THEME->start_bg_active);
            draw_text(m->x + 12, text_center_y(y, h, 1), m->items[i].label, THEME->text, 1);
        }
        y += h;
    }
    clip_reset();
}

bool menu_event(Menu *m, const kian_event_t *e) {
    if (!m->open || !GFX) return false;
    int mh = menu_height(m);
    Rect r = { m->x, m->y, m->w, mh };
    if (e->type == KIAN_EV_MOUSE_MOVE) {
        m->hover = -1;
        if (rect_hit(IN->mx, IN->my, r)) {
            int y = m->y + 4;
            for (int i = 0; i < m->n; i++) {
                int h = menu_item_h(i, m);
                if (IN->my >= y && IN->my < y + h && m->items[i].id >= 0) { m->hover = i; break; }
                y += h;
            }
        }
        return false;
    }
    if (e->type == KIAN_EV_MOUSE_BTN && (IN->press_l || IN->press_r)) {
        if (!rect_hit(IN->mx, IN->my, r)) {
            menu_close(m);
            return true; // click outside closes, swallow it
        }
        int y = m->y + 4;
        for (int i = 0; i < m->n; i++) {
            int h = menu_item_h(i, m);
            if (rect_hit(IN->mx, IN->my, Rect{ m->x, y, m->w, h }) && m->items[i].id >= 0) {
                void (*cb)(void *, int) = m->cb;
                void *ud = m->ud;
                int id = m->items[i].id;
                menu_close(m);
                if (cb) cb(ud, id);
                return true;
            }
            y += h;
        }
        menu_close(m);
        return true;
    }
    if (e->type == KIAN_EV_KEY_DOWN && (e->a == KIAN_KEY_ESC || e->b == 27)) {
        menu_close(m);
        return true;
    }
    return false;
}

// ================= Tooltip =================

void tooltip_draw(int x, int y, const char *text) {
    int w = text_width(text, 1) + 12;
    int h = 24;
    x = iclamp(x, 0, (GFX ? GFX->w - w - 1 : 0));
    y = iclamp(y, 0, (GFX ? GFX->h - h - 1 : 0));
    fill_round_rect(x, y, w, h, 4, THEME->menu_border);
    fill_round_rect(x + 1, y + 1, w - 2, h - 2, 4, THEME->menu_bg);
    draw_text(x + 6, text_center_y(y, h, 1), text, THEME->text, 1);
}

// ================= TabBar =================

void tabbar_draw(const TabBar *tb) {
    int x = tb->x;
    for (int i = 0; i < tb->n; i++) {
        int tw = text_width(tb->tabs[i], 1) + 20;
        bool act = (i == tb->active);
        if (act) fill_round_rect(x, tb->y, tw, 26, 6, THEME->select_bg);
        draw_text(x + 10, text_center_y(tb->y, 26, 1), tb->tabs[i], act ? THEME->accent : THEME->text, 1);
        if (act) hline(x + 4, tb->y + 25, tw - 8, THEME->accent);
        x += tw + 4;
    }
}

int tabbar_hit(const TabBar *tb, int mx, int my) {
    if (my < tb->y || my >= tb->y + 26) return -1;
    int x = tb->x;
    for (int i = 0; i < tb->n; i++) {
        int tw = text_width(tb->tabs[i], 1) + 20;
        if (mx >= x && mx < x + tw) return i;
        x += tw + 4;
    }
    return -1;
}
