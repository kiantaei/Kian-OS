// Kian OS GUI - main loop, apps, event handling. C++, freestanding, no STL.
// ESC or "Exit to console" always returns to ksh (M4).
#include "gui.h"
#include "runtime.h"
#include "gfx.h"
#include "wm.h"
#include "desktop.h"
#include "vec.h"

static inline void kpause() { __asm__ volatile("pause"); }

// ---------- app state (all created inside gui_main - no global ctors) ----------

struct CalcState {
    Str display;
    long long acc;
    int op;          // 0 none, 1 +, 2 -, 3 *, 4 /, 5 %
    bool fresh;      // start a new number on next digit
    bool err;

    CalcState() : acc(0), op(0), fresh(true), err(false) { display.append("0"); }

    void digit(char c) {
        if (err) clear_all();
        if (fresh) { display.clear(); fresh = false; }
        if (display.length() < 12) display.append(c);
    }
    void clear_all() { display.clear(); display.append("0"); acc = 0; op = 0; fresh = true; err = false; }
    long long parse_display() {
        const char *s = display.c_str();
        bool neg = false; int i = 0;
        if (s[0] == '-') { neg = true; i = 1; }
        long long v = 0;
        for (; s[i] >= '0' && s[i] <= '9'; i++) v = v * 10 + (s[i] - '0');
        return neg ? -v : v;
    }
    void set_display(long long a) {
        char buf[24];
        unsigned long long m;
        if (a < 0) { m = (unsigned long long)(-(a + 1)) + 1; } else { m = (unsigned long long)a; }
        int n = 0;
        if (m == 0) buf[n++] = '0';
        while (m > 0 && n < 22) { buf[n++] = (char)('0' + (m % 10)); m /= 10; }
        display.clear();
        if (a < 0) display.append('-');
        while (n > 0) display.append(buf[--n]);
    }
    void apply_op() {
        if (err) return;
        long long v = parse_display();
        if (op == 0) acc = v;
        else if (op == 1) acc += v;
        else if (op == 2) acc -= v;
        else if (op == 3) acc *= v;
        else if (op == 4) {
            if (v == 0) { err = true; display.clear(); display.append("ERR"); op = 0; fresh = true; return; }
            acc /= v;
        } else if (op == 5) {
            if (v == 0) { err = true; display.clear(); display.append("ERR"); op = 0; fresh = true; return; }
            acc %= v;
        }
        set_display(acc);
        fresh = true;
    }
    void set_op(int o) {
        apply_op();
        op = o;
    }
    void negate() {
        const char *src = display.c_str();
        bool had_minus = (src[0] == '-');
        Str t;
        if (!had_minus) t.append('-');
        for (int i = had_minus ? 1 : 0; src[i] != '\0'; i++) t.append(src[i]);
        display.clear();
        const char *ts = t.c_str();
        for (int i = 0; ts[i] != '\0'; i++) display.append(ts[i]);
    }
};

// ---------- cursor (drawn LAST) ----------

static const char *CURSOR[] = {
    "X.........",
    "XX........",
    "XOX.......",
    "XOOX......",
    "XOOOX.....",
    "XOOOOX....",
    "XOOOOOX...",
    "XOOOOOOX..",
    "XOOOOOOOX.",
    "XOOOOXXXXX",
    "XOXOOX....",
    "XOX.XOOX..",
    "XX..XOOX..",
    "....XOOX..",
    ".....XOOX.",
    ".....XOOX.",
    "......XOX.",
    "......XOX.",
    ".......X.."
};
static const int CUR_W = 10, CUR_H = 19;

static void draw_cursor(Surface &s, int cx, int cy) {
    for (int y = 0; y < CUR_H; y++) {
        const char *row = CURSOR[y];
        for (int x = 0; x < CUR_W; x++) {
            char c = row[x];
            if (c == 'X') put_pixel(s, cx + x, cy + y, 0x00000000);
            else if (c == 'O') put_pixel(s, cx + x, cy + y, 0x00FFFFFF);
        }
    }
}

// ---------- app content drawing ----------

// Draws a text buffer bottom-aligned, 8px line height (terminal / notes).
static void draw_text_buffer(Surface &s, const Rect &c, const Theme &th, const Str &text) {
    int total = 1;
    for (int i = 0; i < text.n; i++) {
        if (text.d[i] == '\n') total++;
    }
    int visible = c.h / 8;
    if (visible < 1) return;
    int skip = (total > visible) ? (total - visible) : 0;
    int shown = total - skip;
    int y = c.y + (c.h > shown * 8 ? c.h - shown * 8 : 0);
    int line = 0, x = c.x + 2;
    for (int i = 0; i < text.n; i++) {
        char ch = text.d[i];
        if (ch == '\n') {
            line++;
            if (line > skip) y += 8;
            x = c.x + 2;
            continue;
        }
        if (line >= skip && y + 8 <= c.y + c.h) {
            char b[2] = { ch, '\0' };
            draw_text(s, x, y, b, th.text, 1);
            x += 8;
        }
    }
}

static void draw_calc(Surface &s, const Rect &c, const Theme &th, CalcState &cs) {
    fill_rect(s, c.x + 4, c.y + 4, c.w - 8, 24, th.button_bg);
    draw_text(s, c.x + 8, c.y + 12, cs.display.c_str(), th.text, 1);
    static const char *L[5][4] = {
        { "C", "+-", "%", "/" },
        { "7", "8", "9", "*" },
        { "4", "5", "6", "-" },
        { "1", "2", "3", "+" },
        { "0", "=", "=", "=" }
    };
    for (int r = 0; r < 5; r++) {
        for (int col = 0; col < 4; col++) {
            if (r == 4 && col >= 1) continue; // last row: only "0" button drawn
            int bx = c.x + 4 + col * ((c.w - 8) / 4);
            int by = c.y + 32 + r * 26;
            int bw = (c.w - 8) / 4 - 3;
            int bh = 23;
            fill_round_rect(s, bx, by, bw, bh, 3, th.button_bg);
            draw_text(s, bx + bw / 2 - 4, by + 8, L[r][col], th.button_fg, 1);
        }
    }
}

static int calc_button_at(const Rect &c, int x, int y) {
    for (int r = 0; r < 5; r++) {
        for (int col = 0; col < 4; col++) {
            if (r == 4 && col >= 1) continue;
            int bx = c.x + 4 + col * ((c.w - 8) / 4);
            int by = c.y + 32 + r * 26;
            int bw = (c.w - 8) / 4 - 3;
            int bh = 23;
            if (x >= bx && x < bx + bw && y >= by && y < by + bh) return r * 4 + col;
        }
    }
    return -1;
}

static void calc_press(CalcState &cs, int idx) {
    static const char *L[5][4] = {
        { "C", "+-", "%", "/" },
        { "7", "8", "9", "*" },
        { "4", "5", "6", "-" },
        { "1", "2", "3", "+" },
        { "0", "=", "=", "=" }
    };
    int r = idx / 4, col = idx % 4;
    const char *lbl = L[r][col];
    if (lbl[0] >= '0' && lbl[0] <= '9') cs.digit(lbl[0]);
    else if (lbl[0] == 'C') cs.clear_all();
    else if (lbl[0] == '+') cs.set_op(1);
    else if (lbl[0] == '-') cs.set_op(2);
    else if (lbl[0] == '*') cs.set_op(3);
    else if (lbl[0] == '/') cs.set_op(4);
    else if (lbl[0] == '%') cs.set_op(5);
    else if (lbl[0] == '+' && lbl[1] == '-') cs.negate();
    else if (lbl[0] == '=') { cs.apply_op(); cs.op = 0; }
}

// settings window: remembers the theme toggle button rect (screen coords)
static Rect settings_toggle_rect(const Rect &c) {
    Rect r = { c.x + 8, c.y + 34, 96, 22 };
    return r;
}

// ---------- gui_main ----------

extern "C" int gui_main(const kian_fb_t *fb) {
    if (fb->bpp != 32) return 1; // GUI needs 32bpp; console stays usable

    Surface s;
    s.w = (int)fb->width;
    s.h = (int)fb->height;
    s.buf = (uint32_t *)kian_alloc((size_t)s.w * (size_t)s.h * 4, 16);
    if (s.buf == 0) return 1;

    Theme *th = new Theme();
    theme_dark(*th);

    WM *wm = new WM();
    CalcState *calc = new CalcState();
    Str *term = new Str();
    Str *notes = new Str();

    int cur_x = s.w / 2, cur_y = s.h / 2;
    bool menu_open = false;
    int drag_id = -1, drag_offx = 0, drag_offy = 0;
    bool pressed = false;
    int last_second = -1;
    bool running = true;

    term->append("Kian OS terminal - type, ESC exits GUI\n");
    term->append("kian@kian-os:~$ ");

    while (running) {
        bool activity = false;

        // ---- events ----
        kian_event_t ev;
        while (kian_poll_event(&ev)) {
            activity = true;
            if (ev.type == KIAN_EV_KEY_DOWN) {
                if (ev.b == 27) { running = false; break; } // ESC always exits
                if (wm->count() == 0) continue;
                Window &top = wm->at(wm->count() - 1);
                if (top.minimized) continue;
                if (top.type == APP_TERM) {
                    if (ev.b == '\b') term->backspace();
                    else if (ev.b == '\n') term->append("\nkian@kian-os:~$ ");
                    else if (ev.b >= 32 && ev.b < 127) term->append((char)ev.b);
                } else if (top.type == APP_NOTES) {
                    if (ev.b == '\b') notes->backspace();
                    else if (ev.b == '\n' || (ev.b >= 32 && ev.b < 127)) notes->append((char)ev.b);
                } else if (top.type == APP_CALC) {
                    if (ev.b >= '0' && ev.b <= '9') calc->digit((char)ev.b);
                    else if (ev.b == '+') calc->set_op(1);
                    else if (ev.b == '-') calc->set_op(2);
                    else if (ev.b == '*') calc->set_op(3);
                    else if (ev.b == '/') calc->set_op(4);
                    else if (ev.b == '\n' || ev.b == '=') { calc->apply_op(); calc->op = 0; }
                }
            } else if (ev.type == KIAN_EV_MOUSE_MOVE) {
                // 2x acceleration via 16.16 fixed point (integer only)
                int32_t dx = (int32_t)fx_mul(fx_from_int(ev.a), fx_from_int(2)) >> 16;
                int32_t dy = (int32_t)fx_mul(fx_from_int(ev.b), fx_from_int(2)) >> 16;
                cur_x += dx;
                cur_y += dy;
                if (cur_x < 0) cur_x = 0;
                if (cur_y < 0) cur_y = 0;
                if (cur_x >= s.w) cur_x = s.w - 1;
                if (cur_y >= s.h) cur_y = s.h - 1;
                if (drag_id >= 0) {
                    Window *w = wm->find(drag_id);
                    if (w != 0) {
                        w->x = cur_x - drag_offx;
                        w->y = cur_y - drag_offy;
                        if (w->x < -w->w + 40) w->x = -w->w + 40;
                        if (w->x > s.w - 40) w->x = s.w - 40;
                        if (w->y < 0) w->y = 0;
                        if (w->y > s.h - TB_H - TITLE_H) w->y = s.h - TB_H - TITLE_H;
                    }
                }
            } else if (ev.type == KIAN_EV_MOUSE_BTN) {
                bool left = (ev.a & 1) != 0;
                if (left && !pressed) {
                    pressed = true;
                    activity = true;
                    if (menu_open) {
                        int item = start_menu_item_at(cur_x, cur_y);
                        if (item == 0) { wm->open(APP_TERM, "Terminal", 420, 260, s.w); menu_open = false; }
                        else if (item == 1) { wm->open(APP_FILES, "Files", 380, 240, s.w); menu_open = false; }
                        else if (item == 2) { wm->open(APP_SETTINGS, "Settings", 360, 220, s.w); menu_open = false; }
                        else if (item == 3) { wm->open(APP_CALC, "Calculator", 200, 220, s.w); menu_open = false; }
                        else if (item == 4) { running = false; break; } // Exit to console
                        else if (item == 5) { kian_reboot(); }
                        else if (item == 6) { kian_shutdown(); }
                        else if (item < 0) menu_open = false;
                    } else {
                        Rect sb = start_button_rect();
                        if (cur_x >= sb.x && cur_x < sb.x + sb.w && cur_y >= sb.y && cur_y < sb.y + sb.h) {
                            menu_open = !menu_open;
                        } else if (cur_y >= s.h - TB_H) {
                            for (int i = 0; i < PIN_COUNT; i++) {
                                Rect r = taskbar_icon_rect(i);
                                if (cur_x >= r.x && cur_x < r.x + r.w && cur_y >= r.y && cur_y < r.y + r.h) {
                                    Window *w = wm->find_type(PIN_TYPE[i]);
                                    if (w != 0 && w->minimized) { wm->restore(w); wm->raise(w->id); }
                                    else if (w != 0) wm->raise(w->id);
                                    break;
                                }
                            }
                        } else {
                            Window *w = wm->topmost_at(cur_x, cur_y);
                            if (w != 0) {
                                wm->raise(w->id);
                                int btn = wm->button_at(*w, cur_x, cur_y);
                                if (btn == BTN_CLOSE) wm->close(w->id);
                                else if (btn == BTN_MIN) w->minimized = true;
                                else if (btn == BTN_MAX) {
                                    if (w->maximized) {
                                        w->maximized = false;
                                        w->x = w->prev_x; w->y = w->prev_y;
                                        w->w = w->prev_w; w->h = w->prev_h;
                                    } else {
                                        w->prev_x = w->x; w->prev_y = w->y;
                                        w->prev_w = w->w; w->prev_h = w->h;
                                        w->maximized = true;
                                        w->x = 0; w->y = 0;
                                        w->w = s.w; w->h = s.h - TB_H;
                                    }
                                } else if (wm->title_hit(*w, cur_x, cur_y)) {
                                    drag_id = w->id;
                                    drag_offx = cur_x - w->x;
                                    drag_offy = cur_y - w->y;
                                } else if (w->type == APP_SETTINGS) {
                                    Rect c = wm->content_rect(*w, s.w, s.h);
                                    Rect tr = settings_toggle_rect(c);
                                    if (cur_x >= tr.x && cur_x < tr.x + tr.w && cur_y >= tr.y && cur_y < tr.y + tr.h) {
                                        Theme old = *th;
                                        if (th->taskbar_bg == 0x00101828) theme_light(*th); else theme_dark(*th);
                                        (void)old;
                                    }
                                } else if (w->type == APP_CALC) {
                                    Rect c = wm->content_rect(*w, s.w, s.h);
                                    int bidx = calc_button_at(c, cur_x, cur_y);
                                    if (bidx >= 0) calc_press(*calc, bidx);
                                }
                            }
                        }
                    }
                } else if (!left && pressed) {
                    pressed = false;
                    drag_id = -1;
                }
            }
        }
        if (!running) break;

        // ---- clock: redraw once per second even without input ----
        kian_time_t now;
        kian_time_now(&now);
        bool new_second = (now.second != last_second);
        if (new_second) last_second = now.second;

        // ---- draw ----
        if (activity || new_second) {
            draw_wallpaper(s, *th);
            for (int i = 0; i < wm->wins.size(); i++) {
                wm->draw_one(s, *th, i, s.w, s.h);
                Window &w = wm->at(i);
                if (w.minimized) continue;
                Rect c = wm->content_rect(w, s.w, s.h);
                if (w.type == APP_TERM) {
                    draw_text_buffer(s, c, *th, *term);
                } else if (w.type == APP_NOTES) {
                    draw_text_buffer(s, c, *th, *notes);
                } else if (w.type == APP_FILES) {
                    draw_text(s, c.x + 8, c.y + 8, "Files: no filesystem mounted yet (partly working)", th->text, 1);
                } else if (w.type == APP_SETTINGS) {
                    draw_text(s, c.x + 8, c.y + 8, "Time zone: Asia/Tehran (UTC+03:30), no DST", th->text, 1);
                    draw_text(s, c.x + 8, c.y + 20, "Theme toggle:", th->text, 1);
                    Rect tr = settings_toggle_rect(c);
                    fill_round_rect(s, tr.x, tr.y, tr.w, tr.h, 3, th->button_bg);
                    draw_text(s, tr.x + 6, tr.y + 7, "Dark / Light", th->button_fg, 1);
                } else if (w.type == APP_CALC) {
                    draw_calc(s, c, *th, *calc);
                } else if (w.type == APP_ABOUT) {
                    draw_text(s, c.x + 8, c.y + 8, "Kian OS 0.3.0 \"Horizon\"", th->text, 1);
                    draw_text(s, c.x + 8, c.y + 20, "Rust kernel + C console + C++ GUI", th->text, 1);
                    draw_text(s, c.x + 8, c.y + 32, "(c) kiantaei", th->text, 1);
                }
            }

            draw_taskbar(s, *th, *wm, menu_open);
            if (menu_open) draw_start_menu(s, *th, -1);
            draw_cursor(s, cur_x, cur_y);

            // ---- present: copy back buffer to the real framebuffer ----
            uint8_t *dst = (uint8_t *)(uintptr_t)fb->addr;
            for (int y = 0; y < s.h; y++) {
                memcpy(dst + (size_t)y * fb->pitch, s.buf + (size_t)y * s.w, (size_t)s.w * 4);
            }
        } else {
            // idle: cap the work so we do not busy-flood
            for (int i = 0; i < 200; i++) kpause();
        }
    }

    delete calc;
    delete notes;
    delete term;
    delete wm;
    delete th;
    kian_free(s.buf);
    return 0;
}
