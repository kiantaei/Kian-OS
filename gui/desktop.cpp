// Kian OS GUI - desktop shell implementation (G5). Win11-style, integers only.
#include "desktop.h"
#include "theme.h"
#include "icons.h"
#include "input.h"
#include "util.h"
#include "apps.h"
#include "ramfs.h"
#include "gui.h"

static const int TB_H = 48;
static const int PIN_N = 6;
static const int PIN_APP[PIN_N] = { APP_BROWSER, APP_TERM, APP_FILES, APP_NOTES, APP_SETTINGS, APP_TASKMGR };
static const int PIN_ICON[PIN_N] = { IC_BROWSER, IC_TERMINAL, IC_FILES, IC_NOTES, IC_SETTINGS, IC_TASKMGR };
static const char *PIN_NAME[PIN_N] = { "Kian Browser", "Terminal", "File Manager", "Notepad", "Settings", "Task Manager" };

// ---------------- theme ----------------

void theme_set(Theme *t, bool dark, uint32_t accent) {
    if (dark) {
        t->wallpaper_top    = 0x001A2B4A;
        t->wallpaper_bottom = 0x003B0A45;
        t->taskbar_bg       = 0x00101828;
        t->taskbar_fg       = 0x00E8E8F0;
        t->start_bg         = 0x00283560;
        t->start_bg_active  = 0x00405090;
        t->menu_bg          = 0x001C2440;
        t->menu_border      = 0x00405090;
        t->win_frame        = 0x00303A58;
        t->win_bg           = 0x00141C30;
        t->title_bg         = 0x00283560;
        t->title_text       = 0x00F0F0F8;
        t->text             = 0x00E8E8F0;
        t->text_dim         = 0x0090A0B8;
        t->button_bg        = 0x00283048;
        t->button_fg        = 0x00F0F0F8;
        t->close_btn_bg     = 0x00A03030;
        t->icon_bg          = 0x00283560;
        t->input_bg         = 0x00101A2C;
        t->select_bg        = 0x00283A60;
        t->shadow           = 0x00000010;
    } else {
        t->wallpaper_top    = 0x0088AACC;
        t->wallpaper_bottom = 0x00AA88CC;
        t->taskbar_bg       = 0x00D0D8E8;
        t->taskbar_fg       = 0x00101828;
        t->start_bg         = 0x00B8C4E0;
        t->start_bg_active  = 0x0090A0D0;
        t->menu_bg          = 0x00F0F2FA;
        t->menu_border      = 0x008090B8;
        t->win_frame        = 0x008890A8;
        t->win_bg           = 0x00F4F6FC;
        t->title_bg         = 0x00C0CAF0;
        t->title_text       = 0x00101828;
        t->text             = 0x00101828;
        t->text_dim         = 0x00506078;
        t->button_bg        = 0x00D8DDE8;
        t->button_fg        = 0x00101828;
        t->close_btn_bg     = 0x00E08080;
        t->icon_bg          = 0x00C0CCE8;
        t->input_bg         = 0x00FFFFFF;
        t->select_bg        = 0x00C8DCF0;
        t->shadow           = 0x008090A0;
    }
    t->accent = accent;
}

// ---------------- date helpers (integer only) ----------------

int days_from_civil(int y, int m, int d) {
    y -= (m <= 2) ? 1 : 0;
    int era = (y >= 0 ? y : y - 399) / 400;
    int yoe = y - era * 400;
    int doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    int doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe;
}

static int month_days(int y, int m) {
    static const int md[13] = { 0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    if (m < 1 || m > 12) return 30;
    if (m == 2) {
        bool leap = (y % 4 == 0 && y % 100 != 0) || (y % 400 == 0);
        return leap ? 29 : 28;
    }
    return md[m];
}

static const char *month_name(int m) {
    static const char *mn[13] = { "", "January", "February", "March", "April", "May", "June",
                                  "July", "August", "September", "October", "November", "December" };
    return (m >= 1 && m <= 12) ? mn[m] : "?";
}

// put_num / put_pad2 come from util.h (inline, shared with apps + login)

// ---------------- init ----------------

void desk_init(Desktop *d, WM *wm) {
    d->wm = wm;
    d->wallpaper = 0;
    d->cur_desk = 0;
    d->sel_icon = -1;
    d->taskview = false;
    d->start_open = false;
    d->start_ms = 0;
    d->quick_open = false;
    d->wifi = true;
    d->bt = false;
    d->night = false;
    d->saver = false;
    d->brightness = 100;
    d->volume = 40;
    slider_init(&d->sld_bright, 0, 0, 200, d->brightness);
    slider_init(&d->sld_vol, 0, 0, 200, d->volume);
    d->lang_fa = false;
    d->clock24 = false;
    d->cal_open = false;
    d->toast_on = false;
    d->toast_ms = 0;
    d->toast_text[0] = '\0';
    d->hover_btn = -1;
    d->hover_since = 0;
    d->tooltip_on = false;
    d->exit_gui = false;
    tbox_init(&d->sbox, 0, 0, 100, 30, false);
}

// ---------------- wallpaper + desktop icons ----------------

void desk_draw_wallpaper(Desktop *d) {
    if (!GFX) return;
    switch (d->wallpaper % 3) {
    case 0: gradient_v(0, 0, GFX->w, GFX->h, THEME->wallpaper_top, THEME->wallpaper_bottom); break;
    case 1: gradient_v(0, 0, GFX->w, GFX->h, 0x000E2A1E, 0x00A05030); break; // green->amber
    case 2: gradient_v(0, 0, GFX->w, GFX->h, 0x00102030, 0x006070A0); break; // steel blue
    default: gradient_v(0, 0, GFX->w, GFX->h, THEME->wallpaper_top, THEME->wallpaper_bottom); break;
    }
    // subtle horizon line
    blend_rect(0, GFX->h * 2 / 3, GFX->w, 1, 0x00FFFFFF, 24);
}

static const int DICON_N = 4;
static const int DICON_APP[DICON_N] = { APP_FILES, 0, APP_NOTES, APP_TERM };
static const int DICON_IC[DICON_N] = { IC_PC, IC_TRASH, IC_NOTES, IC_TERMINAL };
static const char *DICON_NAME[DICON_N] = { "This PC", "Recycle Bin", "Notepad", "Terminal" };

static Rect dicon_rect(int i) {
    Rect r = { 12, 12 + i * 96, 76, 88 };
    return r;
}

void desk_draw_icons(Desktop *d) {
    for (int i = 0; i < DICON_N; i++) {
        Rect r = dicon_rect(i);
        if (i == d->sel_icon) {
            fill_round_rect(r.x, r.y, r.w, r.h, 4, THEME->select_bg);
        }
        icon_draw(DICON_IC[i], r.x + (r.w - 28) / 2, r.y + 6, 28, THEME->text);
        draw_text(r.x + (r.w - text_width(DICON_NAME[i], 1)) / 2, r.y + 66, DICON_NAME[i], THEME->text, 1);
    }
}

int desk_icon_at_impl(int x, int y) {
    for (int i = 0; i < DICON_N; i++) {
        if (rect_hit(x, y, dicon_rect(i))) return i;
    }
    return -1;
}

bool desk_event(Desktop *d, const kian_event_t *e) {
    if (e->type != KIAN_EV_MOUSE_BTN) return false;
    int idx = desk_icon_at_impl(IN->mx, IN->my);
    if (IN->press_l) {
        if (idx >= 0) {
            d->sel_icon = idx;
            if (IN->dbl_click) {
                d->sel_icon = -1;
                if (DICON_APP[idx] != 0) app_open(DICON_APP[idx]);
                else toast_show(d, "Recycle Bin is empty");
            }
            return true;
        }
        d->sel_icon = -1;
    }
    return false;
}

// ---------------- taskbar ----------------

static Rect tb_start_rect() { Rect r = { 4, 0, 44, TB_H }; return r; }
static Rect tb_search_rect() { Rect r = { 54, 0, 200, TB_H }; return r; }
static Rect tb_tv_rect() { Rect r = { 260, 0, 36, TB_H }; return r; }
static Rect tb_pin_rect(int i) { Rect r = { 304 + i * 44, 0, 40, TB_H }; return r; }

static Rect tb_clock_rect(int screen_w) { Rect r = { screen_w - 8 - 84, 0, 84, TB_H }; return r; }
static Rect tb_tray_rect(int screen_w) {
    // everything between the last pin and the clock: treated as one hit zone
    Rect r = { screen_w - 8 - 84 - 6 - 150, 0, 150, TB_H };
    return r;
}

static void fmt_clock(char *out, int maxn, const kian_time_t &t, bool h24) {
    int i = 0;
    out[0] = '\0';
    int hour = t.hour;
    const char *ap = "";
    if (!h24) {
        ap = (t.hour < 12) ? " AM" : " PM";
        hour = ((t.hour + 11) % 12) + 1;
    }
    put_num(out, i, hour);
    out[i++] = ':';
    put_pad2(out, i, t.minute);
    for (const char *p = ap; *p && i < maxn - 1; p++) out[i++] = *p;
    out[i] = '\0';
}

static void fmt_date(char *out, int maxn, const kian_time_t &t) {
    int i = 0;
    out[0] = '\0';
    put_num(out, i, t.month);
    out[i++] = '/';
    put_num(out, i, t.day);
    out[i++] = '/';
    put_num(out, i, t.year);
    out[i] = '\0';
    (void)maxn;
}

static bool pin_running(Desktop *d, int app) {
    for (int i = 0; i < WM_MAX_WINDOWS; i++) {
        if (d->wm->win[i].used && d->wm->win[i].app == app && d->wm->win[i].desk_id == d->cur_desk) return true;
    }
    return false;
}

static Window *pin_active(Desktop *d, int app) {
    Window *f = wm_focused(d->wm);
    if (f && f->app == app && f->desk_id == d->cur_desk) return f;
    return 0;
}

void taskbar_draw(Desktop *d) {
    if (!GFX) return;
    int W = GFX->w, H = GFX->h;
    // acrylic: blend the taskbar color over what is behind it
    blend_rect(0, H - TB_H, W, TB_H, THEME->taskbar_bg, 200);
    hline(0, H - TB_H, W, THEME->menu_border);

    uint32_t fg = THEME->taskbar_fg;
    // start button
    Rect sr = tb_start_rect();
    bool sh = mouse_in(sr.x, H - TB_H, sr.w, TB_H);
    if (sh) fill_round_rect(sr.x + 2, H - TB_H + 6, sr.w - 4, TB_H - 12, 6, THEME->start_bg_active);
    icon_draw(IC_START, sr.x + 10, H - TB_H + 12, 24, THEME->accent);

    // search box
    Rect se = tb_search_rect();
    fill_round_rect(se.x, H - TB_H + 8, se.w, 32, 16, THEME->input_bg);
    icon_draw(IC_SEARCH, se.x + 10, H - TB_H + 14, 16, THEME->text_dim);
    draw_text(se.x + 32, H - TB_H + 18, "Search", THEME->text_dim, 1);

    // task view button
    Rect tv = tb_tv_rect();
    bool tvh = mouse_in(tv.x, H - TB_H, tv.w, TB_H);
    if (tvh) fill_round_rect(tv.x + 2, H - TB_H + 6, tv.w - 4, TB_H - 12, 6, THEME->start_bg_active);
    icon_draw(IC_TASKVIEW, tv.x + 6, H - TB_H + 12, 24, fg);

    // pinned apps
    for (int i = 0; i < PIN_N; i++) {
        Rect pr = tb_pin_rect(i);
        bool hov = mouse_in(pr.x, H - TB_H, pr.w, TB_H);
        if (hov) fill_round_rect(pr.x + 2, H - TB_H + 4, pr.w - 4, TB_H - 8, 6, THEME->start_bg_active);
        icon_draw(PIN_ICON[i], pr.x + 8, H - TB_H + 12, 24, fg);
        bool running = pin_running(d, PIN_APP[i]);
        Window *act = pin_active(d, PIN_APP[i]);
        int ux = pr.x + 14, uw = 12;
        if (act) { ux = pr.x + 10; uw = 20; }
        if (running || act) hline(ux, H - 5, uw, act ? THEME->accent : THEME->text_dim);
    }

    // tray: chev, sync, ENG/FA, wifi, speaker, battery
    int x = W - 8 - 84 - 6;
    x -= 24; icon_draw(IC_BATTERY, x + 4, H - TB_H + 15, 18, fg);
    x -= 24; icon_draw(IC_SPEAKER, x + 3, H - TB_H + 15, 18, fg);
    x -= 24; icon_draw(IC_WIFI, x + 3, H - TB_H + 15, 18, fg);
    x -= 6;
    const char *lang = d->lang_fa ? "FA" : "ENG";
    int lw = text_width(lang, 1);
    x -= lw + 4;
    draw_text(x + 2, H - TB_H + 20, lang, fg, 1);
    x -= 20; icon_draw(IC_SYNC, x, H - TB_H + 15, 18, fg);
    x -= 16; icon_draw(IC_CHEVUP, x, H - TB_H + 15, 18, fg);

    // clock (two lines)
    kian_time_t t;
    kian_time_now(&t);
    char l1[24], l2[24];
    fmt_clock(l1, sizeof(l1), t, d->clock24);
    fmt_date(l2, sizeof(l2), t);
    draw_text(W - 8 - 76, H - TB_H + 8, l1, fg, 1);
    draw_text(W - 8 - 76, H - TB_H + 24, l2, fg, 1);

    // tooltip after 600 ms hover
    if (d->tooltip_on && d->hover_btn >= 0 && mouse_idle_ms() >= 600 && IN->btns == 0) {
        const char *name = (d->hover_btn == 0) ? "Start" : (d->hover_btn == 1) ? "Search"
                         : (d->hover_btn == 2) ? "Task view" : PIN_NAME[d->hover_btn - 3];
        tooltip_draw(IN->mx + 8, H - TB_H - 26, name);
    }
}

static void pin_menu_cb(void *ud, int id) {
    int app = (int)(intptr_t)ud;
    if (id == 1) app_open(app);
    else if (id == 2 && SH) wm_close_app(&SH->wm, app);
}

static void power_menu_cb(void *ud, int id) {
    Desktop *d = (Desktop *)ud;
    if (id == 1) toast_show(d, "Sleep is not implemented yet");
    else if (id == 2) kian_shutdown();
    else if (id == 3) kian_reboot();
    else if (id == 4) d->exit_gui = true;
}

bool taskbar_event(Desktop *d, const kian_event_t *e) {
    if (!GFX || !IN) return false;
    int W = GFX->w, H = GFX->h;
    if (IN->my < H - TB_H) {
        // mouse left the taskbar: reset tooltip
        if (e->type == KIAN_EV_MOUSE_MOVE && d->hover_btn >= 0) {
            d->hover_btn = -1;
            d->tooltip_on = false;
        }
        return false;
    }
    if (e->type == KIAN_EV_MOUSE_MOVE) {
        int hb = -1;
        if (mouse_in(tb_start_rect().x, H - TB_H, tb_start_rect().w, TB_H)) hb = 0;
        else if (mouse_in(tb_search_rect().x, H - TB_H, tb_search_rect().w, TB_H)) hb = 1;
        else if (mouse_in(tb_tv_rect().x, H - TB_H, tb_tv_rect().w, TB_H)) hb = 2;
        else {
            for (int i = 0; i < PIN_N; i++) {
                Rect r = tb_pin_rect(i);
                if (mouse_in(r.x, H - TB_H, r.w, TB_H)) { hb = 3 + i; break; }
            }
        }
        if (hb != d->hover_btn) {
            d->hover_btn = hb;
            d->hover_since = now_ms();
            d->tooltip_on = (hb >= 0);
        }
        return true; // taskbar owns move events on itself (tooltip redraw)
    }
    if (e->type == KIAN_EV_MOUSE_BTN && IN->press_l) {
        d->tooltip_on = false;
        if (rect_hit(IN->mx, IN->my, tb_start_rect())) { start_menu_toggle(d); return true; }
        if (rect_hit(IN->mx, IN->my, tb_search_rect())) {
            if (!d->start_open) start_menu_toggle(d);
            tbox_focus(&d->sbox, true);
            return true;
        }
        if (rect_hit(IN->mx, IN->my, tb_tv_rect())) { d->taskview = !d->taskview; return true; }
        for (int i = 0; i < PIN_N; i++) {
            Rect r = tb_pin_rect(i);
            if (rect_hit(IN->mx, IN->my, r)) {
                Window *act = pin_active(d, PIN_APP[i]);
                Window *ex = 0;
                for (int k = 0; k < WM_MAX_WINDOWS; k++) {
                    if (d->wm->win[k].used && d->wm->win[k].app == PIN_APP[i] &&
                        d->wm->win[k].desk_id == d->cur_desk) { ex = &d->wm->win[k]; break; }
                }
                if (ex) {
                    if (act) {
                        ex->state = WS_MIN;      // click on active icon minimizes
                    } else {
                        if (ex->state == WS_MIN) ex->state = WS_NORM;
                        wm_focus(d->wm, ex);
                    }
                } else {
                    app_open(PIN_APP[i]);
                }
                return true;
            }
        }
        if (rect_hit(IN->mx, IN->my, tb_clock_rect(W))) { d->cal_open = !d->cal_open; d->quick_open = false; return true; }
        if (rect_hit(IN->mx, IN->my, tb_tray_rect(W))) { d->quick_open = !d->quick_open; d->cal_open = false; return true; }
        return true; // clicks on the empty taskbar are consumed
    }
    if (e->type == KIAN_EV_MOUSE_BTN && IN->press_r) {
        for (int i = 0; i < PIN_N; i++) {
            Rect r = tb_pin_rect(i);
            if (rect_hit(IN->mx, IN->my, r)) {
                static const MenuItem items[2] = { { "Open", 1 }, { "Close window", 2 } };
                menu_open(&SH->menu, items, 2, pin_menu_cb, (void *)(intptr_t)PIN_APP[i], IN->mx, IN->my - 60);
                return true;
            }
        }
        return true;
    }
    return false;
}

// ---------------- start menu ----------------

struct TileDef { const char *name; int icon; int app; };
static const TileDef TILES[8] = {
    { "Browser", IC_BROWSER, APP_BROWSER },
    { "Terminal", IC_TERMINAL, APP_TERM },
    { "Files", IC_FILES, APP_FILES },
    { "Notepad", IC_NOTES, APP_NOTES },
    { "Settings", IC_SETTINGS, APP_SETTINGS },
    { "Task Mgr", IC_TASKMGR, APP_TASKMGR },
    { "Calculator", IC_CALC, APP_CALC },
    { "Paint", IC_PAINT, APP_PAINT }
};

static bool ci_contains(const char *hay, const char *needle) {
    char h[64], n[32];
    int i = 0;
    for (; hay[i] != '\0' && i < 63; i++) h[i] = (hay[i] >= 'A' && hay[i] <= 'Z') ? hay[i] + 32 : hay[i];
    h[i] = '\0';
    int j = 0;
    for (; needle[j] != '\0' && j < 31; j++) n[j] = (needle[j] >= 'A' && needle[j] <= 'Z') ? needle[j] + 32 : needle[j];
    n[j] = '\0';
    if (n[0] == '\0') return true;
    for (int k = 0; h[k] != '\0'; k++) {
        int m = 0;
        while (n[m] != '\0' && h[k + m] == n[m]) m++;
        if (n[m] == '\0') return true;
    }
    return false;
}

static Rect start_panel_rect(Desktop *d) {
    int mh = imin(560, GFX->h - TB_H - 8);
    Rect r = { 4, GFX->h - TB_H - mh, 480, mh };
    unsigned dt = now_ms() - d->start_ms;
    if (dt < 120) {
        int off = (1 << 16) - fx_ease_out((int)(((int64_t)dt * (1 << 16)) / 120));
        r.y += (off * 40) >> 16;
    }
    return r;
}

void start_menu_toggle(Desktop *d) {
    if (!d->start_open) {
        d->start_open = true;
        d->start_ms = now_ms();
        tbox_init(&d->sbox, 0, 0, 100, 30, false);
        tbox_focus(&d->sbox, true);
    } else {
        d->start_open = false;
    }
}

static void draw_tile(int x, int y, int w, int h, const TileDef *td, bool hover) {
    if (hover) fill_round_rect(x, y, w, h, 6, THEME->start_bg_active);
    icon_draw(td->icon, x + (w - 24) / 2, y + 10, 24, THEME->text);
    draw_text(x + (w - text_width(td->name, 1)) / 2, y + 42, td->name, THEME->text_dim, 1);
}

void start_menu_draw(Desktop *d) {
    if (!GFX || !d->start_open) return;
    Rect p = start_panel_rect(d);
    // acrylic panel
    blend_rect(p.x, p.y, p.w, p.h, THEME->menu_bg, 220);
    round_rect_outline(p.x, p.y, p.w, p.h, 8, THEME->menu_border);
    Rect inner = { p.x + 1, p.y + 1, p.w - 2, p.h - 2 };
    clip_set(inner);

    // search box
    d->sbox.x = p.x + 12; d->sbox.y = p.y + 12; d->sbox.w = p.w - 24; d->sbox.h = 32;
    tbox_draw(&d->sbox);

    // pinned grid (filtered)
    draw_text_bold(p.x + 12, p.y + 56, "Pinned", THEME->text, 2);
    const char *q = tbox_text(&d->sbox);
    int tw = (p.w - 24) / 6;
    int shown = 0;
    for (int i = 0; i < 8; i++) {
        if (!ci_contains(TILES[i].name, q)) continue;
        int col = shown % 6, row = shown / 6;
        int tx = p.x + 12 + col * tw, ty = p.y + 92 + row * 72;
        bool hov = mouse_in(tx, ty, tw, 68);
        draw_tile(tx, ty, tw - 4, 68, &TILES[i], hov);
        shown++;
        if (shown >= 12) break;
    }
    if (shown == 0) draw_text(p.x + 12, p.y + 100, "No matching apps", THEME->text_dim, 1);

    // recommended
    int ry = p.y + 92 + 2 * 72 + 10;
    draw_text_bold(p.x + 12, ry, "Recommended", THEME->text, 2);
    const char *rec[2] = { "readme.txt", "welcome.txt" };
    const char *recp[2] = { "/Documents/readme.txt", "/Desktop/welcome.txt" };
    for (int i = 0; i < 2; i++) {
        if (*q != '\0' && !ci_contains(rec[i], q)) continue;
        int iy = ry + 30 + i * 26;
        bool hov = mouse_in(p.x + 12, iy, 240, 24);
        if (hov) fill_round_rect(p.x + 12, iy, 240, 24, 4, THEME->start_bg_active);
        icon_draw(IC_NOTES, p.x + 18, iy + 4, 16, THEME->accent);
        draw_text(p.x + 42, iy + 8, rec[i], THEME->text, 1);
        (void)recp;
    }

    // bottom: user + power
    int by = p.y + p.h - 44;
    hline(p.x + 8, by, p.w - 16, THEME->menu_border);
    fill_circle(p.x + 26, by + 20, 12, THEME->start_bg_active);
    draw_text_bold(p.x + 22, by + 16, "K", 0x00FFFFFF, 1);
    draw_text(p.x + 44, by + 16, "kian", THEME->text, 1);
    Rect pw = { p.x + p.w - 40, by + 6, 32, 32 };
    bool phov = mouse_in(pw.x, pw.y, pw.w, pw.h);
    if (phov) fill_round_rect(pw.x, pw.y, pw.w, pw.h, 6, THEME->start_bg_active);
    icon_draw(IC_POWER, pw.x + 8, pw.y + 8, 16, THEME->text);
    clip_reset();
}

bool start_menu_event(Desktop *d, const kian_event_t *e) {
    if (!d->start_open || !GFX) return false;
    Rect p = start_panel_rect(d);
    if (e->type == KIAN_EV_KEY_DOWN) {
        if (e->a == KIAN_KEY_ESC || e->b == 27) { d->start_open = false; return true; }
        bool enter = tbox_key(&d->sbox, e);
        shell_full_redraw();
        if (enter) {
            // open the first matching tile
            const char *q = tbox_text(&d->sbox);
            for (int i = 0; i < 8; i++) {
                if (ci_contains(TILES[i].name, q)) {
                    d->start_open = false;
                    app_open(TILES[i].app);
                    return true;
                }
            }
        }
        return true; // start menu owns keys while open
    }
    if (e->type == KIAN_EV_MOUSE_WHEEL) return false;
    if (e->type != KIAN_EV_MOUSE_BTN || !IN->press_l) return false;
    if (!rect_hit(IN->mx, IN->my, p)) { d->start_open = false; return true; }
    if (tbox_mouse(&d->sbox, e)) return true;
    // tiles
    const char *q = tbox_text(&d->sbox);
    int tw = (p.w - 24) / 6;
    int shown = 0;
    for (int i = 0; i < 8; i++) {
        if (!ci_contains(TILES[i].name, q)) continue;
        int col = shown % 6, row = shown / 6;
        int tx = p.x + 12 + col * tw, ty = p.y + 92 + row * 72;
        if (rect_hit(IN->mx, IN->my, Rect{ tx, ty, tw - 4, 68 })) {
            d->start_open = false;
            app_open(TILES[i].app);
            return true;
        }
        shown++;
        if (shown >= 12) break;
    }
    // recommended
    int ry = p.y + 92 + 2 * 72 + 10;
    for (int i = 0; i < 2; i++) {
        int iy = ry + 30 + i * 26;
        if (rect_hit(IN->mx, IN->my, Rect{ p.x + 12, iy, 240, 24 })) {
            d->start_open = false;
            app_open_path(APP_NOTES, (i == 0) ? "/Documents/readme.txt" : "/Desktop/welcome.txt");
            return true;
        }
    }
    // power button
    Rect pw = { p.x + p.w - 40, p.y + p.h - 44 + 6, 32, 32 };
    if (rect_hit(IN->mx, IN->my, pw)) {
        static const MenuItem items[4] = {
            { "Sleep", 1 }, { "Shut down", 2 }, { "Restart", 3 }, { "Exit to console", 4 }
        };
        menu_open(&SH->menu, items, 4, power_menu_cb, d, pw.x - 120, pw.y - 120);
        return true;
    }
    return true; // clicks inside the panel are consumed
}

// ---------------- quick settings ----------------

static Rect quick_panel_rect() {
    Rect r = { GFX->w - 320 - 8, GFX->h - TB_H - 340 - 8, 320, 340 };
    return r;
}

static bool pill(int x, int y, int w, int h, const char *label, int icon, bool on) {
    bool hov = mouse_in(x, y, w, h);
    uint32_t bg = on ? THEME->accent : (hov ? THEME->start_bg_active : THEME->button_bg);
    fill_round_rect(x, y, w, h, 6, bg);
    icon_draw(icon, x + 10, y + (h - 16) / 2, 16, 0x00FFFFFF);
    draw_text(x + 34, y + (h - 8) / 2, label, 0x00FFFFFF, 1);
    return hov;
}

void quick_draw(Desktop *d) {
    if (!GFX || !d->quick_open) return;
    Rect p = quick_panel_rect();
    blend_rect(p.x, p.y, p.w, p.h, THEME->menu_bg, 230);
    round_rect_outline(p.x, p.y, p.w, p.h, 8, THEME->menu_border);
    draw_text_bold(p.x + 12, p.y + 10, "Quick settings", THEME->text, 1);
    int gx = p.x + 12, gy = p.y + 30;
    pill(gx, gy, 142, 40, "Wi-Fi", IC_WIFI, d->wifi);
    pill(gx + 154, gy, 142, 40, "Bluetooth", IC_SYNC, d->bt);
    pill(gx, gy + 48, 142, 40, "Night light", IC_CHECK, d->night);
    pill(gx + 154, gy + 48, 142, 40, "Dark mode", IC_GRID, THEME->wallpaper_top == 0x001A2B4A);
    pill(gx, gy + 96, 296, 36, "Battery saver", IC_BATTERY, d->saver);
    draw_text(p.x + 12, gy + 148, "Brightness", THEME->text, 1);
    d->sld_bright.x = p.x + 12; d->sld_bright.y = gy + 164; d->sld_bright.w = p.w - 24;
    slider_draw(&d->sld_bright);
    draw_text(p.x + 12, gy + 196, "Volume", THEME->text, 1);
    d->sld_vol.x = p.x + 12; d->sld_vol.y = gy + 212; d->sld_vol.w = p.w - 24;
    slider_draw(&d->sld_vol);
    char vv[16];
    int vi = 0;
    vv[0] = '\0';
    put_num(vv, vi, d->volume);
    vv[vi] = '\0';
    draw_text(p.x + p.w - 40, gy + 196, vv, THEME->text_dim, 1);
}

bool quick_event(Desktop *d, const kian_event_t *e) {
    if (!d->quick_open || !GFX) return false;
    Rect p = quick_panel_rect();
    if (e->type == KIAN_EV_KEY_DOWN && (e->a == KIAN_KEY_ESC || e->b == 27)) {
        d->quick_open = false;
        return true;
    }
    if (e->type == KIAN_EV_MOUSE_MOVE || e->type == KIAN_EV_MOUSE_BTN) {
        if (slider_event(&d->sld_bright, e)) {
            d->brightness = d->sld_bright.value;
            shell_full_redraw();
            return true;
        }
        if (slider_event(&d->sld_vol, e)) {
            d->volume = d->sld_vol.value;
            shell_full_redraw();
            return true;
        }
    }
    if (e->type != KIAN_EV_MOUSE_BTN || !IN->press_l) return false;
    if (!rect_hit(IN->mx, IN->my, p)) { d->quick_open = false; return true; }
    int gx = p.x + 12, gy = p.y + 30;
    if (mouse_in(gx, gy, 142, 40)) { d->wifi = !d->wifi; return true; }
    if (mouse_in(gx + 154, gy, 142, 40)) { d->bt = !d->bt; return true; }
    if (mouse_in(gx, gy + 48, 142, 40)) { d->night = !d->night; return true; }
    if (mouse_in(gx + 154, gy + 48, 142, 40)) {
        bool dark = (THEME->wallpaper_top == 0x001A2B4A);
        theme_set(THEME, !dark, THEME->accent);
        shell_full_redraw();
        return true;
    }
    if (mouse_in(gx, gy + 96, 296, 36)) { d->saver = !d->saver; return true; }
    return true;
}

// ---------------- calendar flyout ----------------

static Rect cal_panel_rect() {
    Rect r = { GFX->w - 300 - 8, GFX->h - TB_H - 330 - 8, 300, 330 };
    return r;
}

void cal_draw(Desktop *d) {
    if (!GFX || !d->cal_open) return;
    Rect p = cal_panel_rect();
    blend_rect(p.x, p.y, p.w, p.h, THEME->menu_bg, 230);
    round_rect_outline(p.x, p.y, p.w, p.h, 8, THEME->menu_border);
    kian_time_t t;
    kian_time_now(&t);
    char head[48];
    int i = 0;
    head[0] = '\0';
    const char *mn = month_name(t.month);
    for (; *mn != '\0'; mn++) head[i++] = *mn;
    head[i++] = ' ';
    put_num(head, i, t.year);
    head[i] = '\0';
    draw_text_bold(p.x + 12, p.y + 12, head, THEME->text, 2);

    static const char *dow[7] = { "S", "M", "T", "W", "T", "F", "S" };
    int cw = 40, ch = 30;
    int ox = p.x + 10, oy = p.y + 50;
    for (int c = 0; c < 7; c++) {
        draw_text(ox + c * cw + 14, oy, dow[c], THEME->text_dim, 1);
    }
    int first_dow = ((days_from_civil(t.year, t.month, 1) + 4) % 7 + 7) % 7; // 0 = Sunday
    int nd = month_days(t.year, t.month);
    int row = 0, col = first_dow;
    for (int day = 1; day <= nd; day++) {
        int dx = ox + col * cw, dy = oy + ch + row * ch;
        if (day == t.day) {
            fill_circle(dx + cw / 2, dy + 10, 11, THEME->accent);
            draw_text(dx + cw / 2 - (day >= 10 ? 5 : 2), dy + 6, "", THEME->text, 1);
            char db[4];
            int k = 0;
            db[0] = '\0';
            put_num(db, k, day);
            draw_text(dx + cw / 2 - text_width(db, 1) / 2, dy + 6, db, 0x00FFFFFF, 1);
        } else {
            char db[4];
            int k = 0;
            db[0] = '\0';
            put_num(db, k, day);
            draw_text(dx + cw / 2 - text_width(db, 1) / 2, dy + 6, db, THEME->text, 1);
        }
        col++;
        if (col == 7) { col = 0; row++; }
    }
    (void)d;
}

bool cal_event(Desktop *d, const kian_event_t *e) {
    if (!d->cal_open || !GFX) return false;
    Rect p = cal_panel_rect();
    if (e->type == KIAN_EV_KEY_DOWN && (e->a == KIAN_KEY_ESC || e->b == 27)) {
        d->cal_open = false;
        return true;
    }
    if (e->type == KIAN_EV_MOUSE_BTN && IN->press_l && !rect_hit(IN->mx, IN->my, p)) {
        d->cal_open = false;
        return true;
    }
    return e->type != KIAN_EV_MOUSE_MOVE;
}

// ---------------- toasts ----------------

Rect toast_region(Desktop *d) {
    Rect r = { (GFX ? GFX->w - 288 - 8 : 0), (GFX ? GFX->h - TB_H - 56 - 8 : 0), 288, 56 };
    (void)d;
    return r;
}

void toast_show(Desktop *d, const char *text) {
    int i = 0;
    for (; text[i] != '\0' && i < 63; i++) d->toast_text[i] = text[i];
    d->toast_text[i] = '\0';
    d->toast_on = true;
    d->toast_ms = now_ms();
}

void toast_tick(Desktop *d) {
    if (d->toast_on && now_ms() - d->toast_ms > 4000) d->toast_on = false;
}

void toast_draw(Desktop *d) {
    if (!GFX || !d->toast_on) return;
    Rect r = toast_region(d);
    unsigned dt = now_ms() - d->toast_ms;
    int off = 0;
    if (dt < 150) {
        int e = fx_ease_out((int)(((int64_t)dt * (1 << 16)) / 150));
        off = ((1 << 16) - e) * 300 >> 16;
    }
    int x = r.x + off;
    fill_round_rect(x, r.y, r.w, r.h, 6, THEME->menu_border);
    fill_round_rect(x + 1, r.y + 1, r.w - 2, r.h - 2, 6, THEME->menu_bg);
    fill_rect(x + 1, r.y + 8, 4, r.h - 16, THEME->accent);
    draw_text(x + 16, r.y + 10, "Kian OS", THEME->text_dim, 1);
    draw_text(x + 16, r.y + 28, d->toast_text, THEME->text, 1);
}

// ---------------- task view + virtual desktops ----------------

void desk_switch(Desktop *d, int delta) {
    int nd = iclamp(d->cur_desk + delta, 0, 1);
    if (nd != d->cur_desk) {
        d->cur_desk = nd;
        d->wm->cur_desk = nd;
        d->taskview = false;
        shell_full_redraw();
    }
}

void taskview_draw(Desktop *d) {
    if (!GFX || !d->taskview) return;
    blend_rect(0, 0, GFX->w, GFX->h, 0x00000000, 170);
    draw_text_bold(GFX->w / 2 - text_width("Task View", 2) / 2, 24, "Task View", THEME->text, 2);

    // desktop pills
    int px = GFX->w / 2 - 160;
    for (int i = 0; i < 2; i++) {
        Rect pr = { px + i * 150, 70, 140, 34 };
        bool act = (i == d->cur_desk);
        fill_round_rect(pr.x, pr.y, pr.w, pr.h, 6, act ? THEME->accent : THEME->button_bg);
        char lbl[16];
        int k = 0;
        lbl[0] = '\0';
        const char *pfx = "Desktop ";
        for (; pfx[k] != '\0'; k++) lbl[k] = pfx[k];
        put_num(lbl, k, i + 1);
        lbl[k] = '\0';
        draw_text(pr.x + (pr.w - text_width(lbl, 1)) / 2, pr.y + 13, lbl, 0x00FFFFFF, 1);
    }
    Rect nb = { px + 310, 70, 130, 34 };
    fill_round_rect(nb.x, nb.y, nb.w, nb.h, 6, THEME->button_bg);
    icon_draw(IC_PLUS, nb.x + 10, nb.y + 9, 16, THEME->button_fg);
    draw_text(nb.x + 32, nb.y + 13, "New desktop", THEME->button_fg, 1);

    // window thumbnails (honest: scaled frames, not real content screenshots)
    int count = 0;
    int cols = 4;
    for (int i = 0; i < WM_MAX_WINDOWS && count < 8; i++) {
        Window *w = &d->wm->win[i];
        if (!w->used) continue;
        int tx = 40 + (count % cols) * 190;
        int ty = 130 + (count / cols) * 160;
        // scale rect to 180x120 with integer nearest-neighbor geometry
        fill_round_rect(tx, ty, 180, 120, 6, w->desk_id == d->cur_desk ? THEME->win_frame : THEME->icon_bg);
        fill_rect(tx + 2, ty + 2, 176, 20, THEME->title_bg);
        icon_draw(w->icon, tx + 6, ty + 4, 14, THEME->accent);
        draw_text(tx + 26, ty + 6, w->title, THEME->title_text, 1);
        fill_rect(tx + 2, ty + 22, 176, 96, THEME->win_bg);
        // mini content hint: three dim lines
        blend_rect(tx + 10, ty + 34, 160, 4, THEME->text_dim, 90);
        blend_rect(tx + 10, ty + 44, 140, 4, THEME->text_dim, 90);
        blend_rect(tx + 10, ty + 54, 150, 4, THEME->text_dim, 90);
        if (w->desk_id != d->cur_desk) draw_text(tx + 8, ty + 100, "(other desktop)", THEME->text_dim, 1);
        count++;
    }
    if (count == 0) draw_text(GFX->w / 2 - 60, 200, "No open windows", THEME->text, 1);
}

bool taskview_event(Desktop *d, const kian_event_t *e) {
    if (!d->taskview || !GFX) return false;
    if (e->type == KIAN_EV_KEY_DOWN) {
        if (e->a == KIAN_KEY_ESC || e->b == 27) { d->taskview = false; return true; }
        return true; // task view owns keys while open
    }
    if (e->type != KIAN_EV_MOUSE_BTN || !IN->press_l) return false;
    int px = GFX->w / 2 - 160;
    for (int i = 0; i < 2; i++) {
        if (rect_hit(IN->mx, IN->my, Rect{ px + i * 150, 70, 140, 34 })) {
            d->cur_desk = i;
            d->wm->cur_desk = i;
            d->taskview = false;
            shell_full_redraw();
            return true;
        }
    }
    if (rect_hit(IN->mx, IN->my, Rect{ px + 310, 70, 130, 34 })) {
        d->cur_desk = 1;
        d->wm->cur_desk = 1;
        d->taskview = false;
        shell_full_redraw();
        return true;
    }
    // thumbnails
    int count = 0;
    for (int i = 0; i < WM_MAX_WINDOWS && count < 8; i++) {
        Window *w = &d->wm->win[i];
        if (!w->used) continue;
        int tx = 40 + (count % 4) * 190;
        int ty = 130 + (count / 4) * 160;
        if (rect_hit(IN->mx, IN->my, Rect{ tx, ty, 180, 120 })) {
            d->cur_desk = w->desk_id;
            d->wm->cur_desk = w->desk_id;
            if (w->state == WS_MIN) w->state = WS_NORM;
            wm_focus(d->wm, w);
            d->taskview = false;
            shell_full_redraw();
            return true;
        }
        count++;
    }
    d->taskview = false; // click outside closes
    return true;
}
