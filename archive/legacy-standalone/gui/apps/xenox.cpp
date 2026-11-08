// Kian OS GUI - Xenox Browser: Chromium-style shell with tabs, omnibox,
// bookmarks, history, downloads page, menus and honest networking status.
// Everything renders locally; remote pages are NEVER faked.
#include "xenox.h"
#include "../apps.h"
#include "../gui.h"
#include "../input.h"
#include "../widgets.h"
#include "../icons.h"
#include "../util.h"
#include "../ramfs.h"
#include "../theme.h"

static const int TB_H = 34;       // toolbar height
static const int TAB_H = 30;      // tab strip height
static const int ST_H = 20;       // status bar height
static const int TAB_W = 148;     // fixed tab width
static const int STACK_N = 8;     // per-tab back/fwd entries
static const int HIST_N = 16;     // history ring
static const int BM_N = 8;        // bookmark pool
static const int SUG_N = 5;       // omnibox suggestions
// bookmarks persist to /Documents/xenox-bookmarks.txt (see bookmarks_save)

struct XBookmark { char title[64]; char url[XENOX_URL_MAX]; };

struct XTab {
    bool alive;
    int page;                            // XenoPage
    char url[XENOX_URL_MAX];
    char title[64];
    char err_url[XENOX_URL_MAX];
    char reason[96];
    char back[STACK_N][XENOX_URL_MAX];
    int back_n;
    char fwd[STACK_N][XENOX_URL_MAX];
    int fwd_n;
};

struct XenoxApp {
    XTab tab[XENOX_TABS];
    int ntabs;
    int active;
    TBox omni;
    bool editing;                        // omnibox in edit mode
    XBookmark bm[BM_N];
    int nbm;
    struct XHist { char title[64]; char url[XENOX_URL_MAX]; } hist[HIST_N];
    int hist_n, hist_head;
    int scroll;
    // shared layout cache (recomputed by layout() before draw/event use)
    Rect r_tabs[XENOX_TABS];
    Rect r_newtab;
    Rect r_back, r_fwd, r_reload, r_home, r_omni, r_star, r_profile, r_menu;
    Rect r_sug[SUG_N];
    char sug_url[SUG_N][XENOX_URL_MAX];
    int nsug;
    Rect r_row[12];                      // clickable rows on the active page
    char row_url[12][XENOX_URL_MAX];     // target url of each row
    int nrows;
    Rect r_clear, r_go_start, r_reload_pg;
    char status[80];
};

static bool g_milestone = false;         // serial milestone printed once

// ---------------- small helpers ----------------

static void sxcpy(char *dst, int cap, const char *src) {
    int i = 0;
    for (; src[i] != '\0' && i < cap - 1; i++) dst[i] = src[i];
    dst[i] = '\0';
}

static bool sxlower_eq_prefix(const char *s, const char *prefix) {
    char a[128], b[64];
    int i = 0;
    for (; s[i] != '\0' && i < 127; i++) a[i] = (s[i] >= 'A' && s[i] <= 'Z') ? s[i] + 32 : s[i];
    a[i] = '\0';
    for (i = 0; prefix[i] != '\0' && i < 63; i++) b[i] = (prefix[i] >= 'A' && prefix[i] <= 'Z') ? prefix[i] + 32 : prefix[i];
    b[i] = '\0';
    return strncmp(a, b, (size_t)i) == 0;
}

static bool sxcontains_ci(const char *hay, const char *needle) {
    char h[128], n[64];
    int i = 0;
    for (; hay[i] != '\0' && i < 127; i++) h[i] = (hay[i] >= 'A' && hay[i] <= 'Z') ? hay[i] + 32 : hay[i];
    h[i] = '\0';
    int j = 0;
    for (; needle[j] != '\0' && j < 63; j++) n[j] = (needle[j] >= 'A' && needle[j] <= 'Z') ? needle[j] + 32 : needle[j];
    n[j] = '\0';
    if (n[0] == '\0') return true;
    for (int k = 0; h[k] != '\0'; k++) {
        int m = 0;
        while (n[m] != '\0' && h[k + m] == n[m]) m++;
        if (n[m] == '\0') return true;
    }
    return false;
}

static XTab *cur_tab(XenoxApp *b) {
    if (b->active < 0 || b->active >= b->ntabs) return 0;
    return &b->tab[b->active];
}

// ---------------- bookmarks persistence (RAM fs, lost on reboot) ----------------

static void bookmarks_save(XenoxApp *b) {
    char data[2048];
    int k = 0;
    data[0] = '\0';
    for (int i = 0; i < b->nbm; i++) {
        for (int j = 0; b->bm[i].title[j] != '\0' && k < 2000; j++) data[k++] = b->bm[i].title[j];
        if (k < 2000) data[k++] = '\t';
        for (int j = 0; b->bm[i].url[j] != '\0' && k < 2000; j++) data[k++] = b->bm[i].url[j];
        if (k < 2000) data[k++] = '\n';
    }
    data[k] = '\0';
    RNode *docs = ramfs_resolve("/Documents");
    RNode *f = docs ? ramfs_find(docs, "xenox-bookmarks.txt") : 0;
    if (!f && docs) f = ramfs_create(docs, "xenox-bookmarks.txt", "");
    if (f) ramfs_write(f, data, k);
}

static void bookmarks_load(XenoxApp *b) {
    b->nbm = 0;
    RNode *docs = ramfs_resolve("/Documents");
    RNode *f = docs ? ramfs_find(docs, "xenox-bookmarks.txt") : 0;
    if (!f) return;
    const char *s = ramfs_read(f);
    int i = 0;
    while (b->nbm < BM_N && s[i] != '\0') {
        int t = 0;
        for (; s[i] != '\0' && s[i] != '\t' && t < 63; i++) b->bm[b->nbm].title[t++] = s[i];
        b->bm[b->nbm].title[t] = '\0';
        if (s[i] == '\t') i++;
        int u = 0;
        for (; s[i] != '\0' && s[i] != '\n' && u < XENOX_URL_MAX - 1; i++) b->bm[b->nbm].url[u++] = s[i];
        b->bm[b->nbm].url[u] = '\0';
        if (s[i] == '\n') i++;
        if (b->bm[b->nbm].url[0] != '\0') b->nbm++;
    }
}

static bool tab_bookmarked(XenoxApp *b, const char *url) {
    for (int i = 0; i < b->nbm; i++) {
        if (strcmp(b->bm[i].url, url) == 0) return true;
    }
    return false;
}

// ---------------- history ----------------

static void history_push(XenoxApp *b, const char *url, const char *title) {
    int slot = b->hist_head;
    sxcpy(b->hist[slot].url, XENOX_URL_MAX, url);
    sxcpy(b->hist[slot].title, 64, title);
    b->hist_head = (b->hist_head + 1) % HIST_N;
    if (b->hist_n < HIST_N) b->hist_n++;
}

// ---------------- navigation ----------------

static void page_for_url(const char *url, int *page, const char **title) {
    if (strcmp(url, "xenox://start") == 0) { *page = XP_START; *title = "New Tab"; }
    else if (strcmp(url, "xenox://bookmarks") == 0) { *page = XP_BOOKMARKS; *title = "Bookmarks"; }
    else if (strcmp(url, "xenox://history") == 0) { *page = XP_HISTORY; *title = "History"; }
    else if (strcmp(url, "xenox://downloads") == 0) { *page = XP_DOWNLOADS; *title = "Downloads"; }
    else if (strcmp(url, "xenox://about") == 0) { *page = XP_ABOUT; *title = "About Xenox"; }
    else { *page = XP_ERROR; *title = "Can't show this page"; }
}

static void apply_url(XenoxApp *b, XTab *t, const char *url) {
    (void)b;   // reserved for future per-app url handling
    int page = XP_START;
    const char *title = "New Tab";
    page_for_url(url, &page, &title);
    t->page = page;
    sxcpy(t->url, XENOX_URL_MAX, url);
    sxcpy(t->title, 64, title);
    sxcpy(t->err_url, XENOX_URL_MAX, url);
    t->reason[0] = '\0';
    if (page == XP_ERROR) {
        bool http = sxlower_eq_prefix(url, "http://") || sxlower_eq_prefix(url, "https://");
        if (http) {
            sxcpy(t->reason, 96, "Xenox cannot fetch remote pages: Kian OS has no network stack yet. Nothing was faked.");
        } else {
            sxcpy(t->reason, 96, "Not a Xenox page and no search engine is reachable without networking.");
        }
    }
}

static void nav_to(XenoxApp *b, XTab *t, const char *url, bool record_back) {
    if (record_back && t->url[0] != '\0') {
        if (t->back_n < STACK_N) {
            sxcpy(t->back[t->back_n], XENOX_URL_MAX, t->url);
            t->back_n++;
        } else {
            for (int i = 1; i < STACK_N; i++) sxcpy(t->back[i - 1], XENOX_URL_MAX, t->back[i]);
            sxcpy(t->back[STACK_N - 1], XENOX_URL_MAX, t->url);
        }
        t->fwd_n = 0;
    }
    apply_url(b, t, url);
    history_push(b, t->url, t->title);
    b->scroll = 0;
    if (!b->editing) tbox_set_text(&b->omni, t->url);
    shell_set_title(t->title);
    b->status[0] = '\0';
    sxcpy(b->status, 80, "Done");
}

static void nav_back(XenoxApp *b, XTab *t) {
    if (t->back_n == 0) return;
    if (t->fwd_n < STACK_N) {
        for (int i = t->fwd_n; i > 0; i--) sxcpy(t->fwd[i], XENOX_URL_MAX, t->fwd[i - 1]);
        sxcpy(t->fwd[0], XENOX_URL_MAX, t->url);
        t->fwd_n++;
    }
    sxcpy(t->url, XENOX_URL_MAX, t->back[t->back_n - 1]);
    t->back_n--;
    apply_url(b, t, t->url);
    b->scroll = 0;
    if (!b->editing) tbox_set_text(&b->omni, t->url);
    shell_set_title(t->title);
}

static void nav_forward(XenoxApp *b, XTab *t) {
    if (t->fwd_n == 0) return;
    if (t->back_n < STACK_N) {
        sxcpy(t->back[t->back_n], XENOX_URL_MAX, t->url);
        t->back_n++;
    }
    sxcpy(t->url, XENOX_URL_MAX, t->fwd[0]);
    t->fwd_n--;
    for (int i = 0; i < t->fwd_n; i++) sxcpy(t->fwd[i], XENOX_URL_MAX, t->fwd[i + 1]);
    apply_url(b, t, t->url);
    b->scroll = 0;
    if (!b->editing) tbox_set_text(&b->omni, t->url);
    shell_set_title(t->title);
}

// ---------------- layout (shared by draw + event) ----------------

static int lay_widths(Rect r, int *omni_w) {
    int x = r.x + 4;
    *omni_w = r.w - 4 * 4 - 28 * 4 - 26 - 30;
    if (*omni_w < 120) *omni_w = 120;
    return x;
}

static void layout(XenoxApp *b, Rect r) {
    int maxtabs = b->ntabs;
    if (6 + b->ntabs * (TAB_W + 4) + 30 > r.w) maxtabs = (r.w - 36) / (TAB_W + 4);
    if (maxtabs < 1) maxtabs = 1;
    for (int i = 0; i < XENOX_TABS; i++) {
        b->r_tabs[i] = Rect{ r.x + 6 + i * (TAB_W + 4), r.y + 2, TAB_W, TAB_H - 4 };
        if (i >= maxtabs) b->r_tabs[i].w = 0;   // hidden (tab pool full on narrow windows)
    }
    b->r_newtab = Rect{ r.x + 6 + imax(1, maxtabs) * (TAB_W + 4), r.y + 6, 24, 20 };
    int ow = 0;
    int x = lay_widths(r, &ow);
    b->r_back = Rect{ x, r.y + TAB_H + 4, 28, 26 };
    b->r_fwd = Rect{ x + 30, r.y + TAB_H + 4, 28, 26 };
    b->r_reload = Rect{ x + 60, r.y + TAB_H + 4, 28, 26 };
    b->r_home = Rect{ x + 90, r.y + TAB_H + 4, 28, 26 };
    b->r_omni = Rect{ x + 122, r.y + TAB_H + 4, ow, 26 };
    b->r_star = Rect{ x + 122 + ow + 4, r.y + TAB_H + 4, 26, 26 };
    b->r_profile = Rect{ x + 122 + ow + 32, r.y + TAB_H + 4, 24, 26 };
    b->r_menu = Rect{ x + 122 + ow + 58, r.y + TAB_H + 4, 26, 26 };
    b->nsug = 0;
    if (b->editing && b->omni.len > 0) {
        const char *q = tbox_text(&b->omni);
        for (int i = 0; i < b->hist_n && b->nsug < SUG_N; i++) {
            int idx = (b->hist_head - 1 - i + HIST_N * 2) % HIST_N;
            if (b->hist_n < HIST_N && idx >= b->hist_n) continue;
            if (sxcontains_ci(b->hist[idx].url, q) || sxcontains_ci(b->hist[idx].title, q)) {
                b->r_sug[b->nsug] = Rect{ b->r_omni.x, b->r_omni.y + b->r_omni.h + 2 + b->nsug * 22, b->r_omni.w, 22 };
                sxcpy(b->sug_url[b->nsug], XENOX_URL_MAX, b->hist[idx].url);
                b->nsug++;
            }
        }
        for (int i = 0; i < b->nbm && b->nsug < SUG_N; i++) {
            if (sxcontains_ci(b->bm[i].url, q) || sxcontains_ci(b->bm[i].title, q)) {
                bool dup = false;
                for (int k = 0; k < b->nsug; k++) if (strcmp(b->sug_url[k], b->bm[i].url) == 0) dup = true;
                if (!dup) {
                    b->r_sug[b->nsug] = Rect{ b->r_omni.x, b->r_omni.y + b->r_omni.h + 2 + b->nsug * 22, b->r_omni.w, 22 };
                    sxcpy(b->sug_url[b->nsug], XENOX_URL_MAX, b->bm[i].url);
                    b->nsug++;
                }
            }
        }
    }
    // list rows on the active page (bookmarks/history)
    b->nrows = 0;
    int cy = r.y + TAB_H + TB_H + 96;
    if (cur_tab(b) && cur_tab(b)->page == XP_BOOKMARKS) {
        for (int i = 0; i < b->nbm && b->nrows < 12; i++) {
            b->r_row[b->nrows++] = Rect{ r.x + 24, cy + i * 30, r.w - 90, 28 };
        }
        b->r_clear = Rect{ r.x + 24, cy + b->nbm * 30 + 16, 170, 30 };
    } else if (cur_tab(b) && cur_tab(b)->page == XP_HISTORY) {
        for (int i = 0; i < b->hist_n && b->nrows < 12; i++) {
            int idx = (b->hist_head - 1 - i + HIST_N * 2) % HIST_N;
            if (b->hist_n < HIST_N && idx >= b->hist_n) continue;
            b->r_row[b->nrows] = Rect{ r.x + 24, cy + i * 30, r.w - 90, 28 };
            b->nrows++;
        }
        b->r_clear = Rect{ r.x + 24, cy + b->nrows * 30 + 16, 190, 30 };
    } else {
        b->r_clear = Rect{ 0, 0, 0, 0 };
    }
    b->r_go_start = Rect{ r.x + 24, r.y + TAB_H + TB_H + 196, 150, 32 };
    b->r_reload_pg = Rect{ r.x + 190, r.y + TAB_H + TB_H + 196, 110, 32 };
}

// ---------------- draw ----------------

static void draw_icon_btn(Rect r, int icon, bool enabled, uint32_t fg) {
    if (!enabled) fg = THEME->text_dim;
    if (enabled && mouse_in(r.x, r.y, r.w, r.h)) fill_round_rect(r.x, r.y, r.w, r.h, 13, THEME->start_bg_active);
    icon_draw(icon, r.x + (r.w - 16) / 2, r.y + (r.h - 16) / 2, 16, fg);
}

static void draw_centered_x(const char *s, Rect r, int dy, uint32_t c, int scale, bool bold) {
    int w = text_width(s, scale);
    if (bold) draw_text_bold(r.x + (r.w - w) / 2, r.y + dy, s, c, scale);
    else draw_text(r.x + (r.w - w) / 2, r.y + dy, s, c, scale);
}

static void draw_page(XenoxApp *b, Rect r) {
    XTab *t = cur_tab(b);
    if (!t) return;
    int cy = r.y + TAB_H + TB_H;
    Rect cr = { r.x, cy, r.w, r.h - TAB_H - TB_H - ST_H };
    fill_rect(cr.x, cr.y, cr.w, cr.h, THEME->win_bg);

    if (t->page == XP_START) {
        draw_centered_x("Xenox Browser", cr, 60, THEME->text, 2, true);
        icon_draw(IC_GLOBE, cr.x + cr.w / 2 - 9, cr.y + 96, 18, THEME->accent);
        draw_centered_x("Internet access is not available yet.", cr, 122, THEME->text_dim, 1, false);
        draw_centered_x("Kian OS networking support is under development.", cr, 140, THEME->text_dim, 1, false);
        draw_centered_x("Only the built-in xenox:// pages render. No websites are faked.", cr, 158, THEME->text_dim, 1, false);
        // quick links
        static const char *ql[4] = { "Bookmarks", "History", "Downloads", "About Xenox" };
        static const char *qu[4] = { "xenox://bookmarks", "xenox://history", "xenox://downloads", "xenox://about" };
        b->nrows = 0;
        for (int i = 0; i < 4; i++) {
            Rect br = { cr.x + 40 + i * ((cr.w - 80) / 4), cr.y + 190, (cr.w - 80) / 4 - 10, 34 };
            bool hov = mouse_in(br.x, br.y, br.w, br.h);
            fill_round_rect(br.x, br.y, br.w, br.h, 6, hov ? THEME->start_bg_active : THEME->button_bg);
            round_rect_outline(br.x, br.y, br.w, br.h, 6, THEME->menu_border);
            draw_text(br.x + (br.w - text_width(ql[i], 1)) / 2, text_center_y(br.y, br.h, 1), ql[i], THEME->button_fg, 1);
            b->r_row[b->nrows] = br;
            sxcpy(b->row_url[b->nrows], XENOX_URL_MAX, qu[i]);
            b->nrows++;
        }
        // recent history + bookmarks
        int hy = cr.y + 236;
        draw_text_bold(cr.x + 40, hy, "Recent", THEME->text, 1);
        int shown = 0;
        for (int i = 0; i < b->hist_n && shown < 3 && b->nrows < 12; i++) {
            int idx = (b->hist_head - 1 - i + HIST_N * 2) % HIST_N;
            if (b->hist_n < HIST_N && idx >= b->hist_n) continue;
            Rect hr = { cr.x + 40, hy + 24 + shown * 26, cr.w - 200, 24 };
            if (mouse_in(hr.x, hr.y, hr.w, hr.h)) fill_round_rect(hr.x, hr.y, hr.w, hr.h, 4, THEME->select_bg);
            icon_draw(IC_CLOCK, hr.x + 2, hr.y + 4, 14, THEME->text_dim);
            draw_text(hr.x + 22, text_center_y(hr.y, hr.h, 1), b->hist[idx].title, THEME->text, 1);
            b->r_row[b->nrows] = hr;
            sxcpy(b->row_url[b->nrows], XENOX_URL_MAX, b->hist[idx].url);
            b->nrows++;
            shown++;
        }
        if (shown == 0) draw_text(cr.x + 62, hy + 26, "(no pages visited yet)", THEME->text_dim, 1);
    } else if (t->page == XP_BOOKMARKS) {
        draw_text_bold(cr.x + 24, cr.y + 24, "Bookmarks", THEME->text, 2);
        draw_text(cr.x + 24, cr.y + 54, "Stored in /Documents/xenox-bookmarks.txt in the RAM filesystem - lost on reboot.", THEME->text_dim, 1);
        for (int i = 0; i < b->nbm && i < 12; i++) {
            Rect row = b->r_row[i];
            if (mouse_in(row.x, row.y, row.w, row.h)) fill_round_rect(row.x, row.y, row.w, row.h, 4, THEME->select_bg);
            icon_draw(IC_BOOKMARK, row.x + 4, row.y + 6, 14, THEME->accent);
            draw_text(row.x + 26, text_center_y(row.y, row.h, 1), b->bm[i].title, THEME->text, 1);
            int tw = text_width(b->bm[i].title, 1);
            draw_text(row.x + 30 + tw, text_center_y(row.y, row.h, 1), b->bm[i].url, THEME->text_dim, 1);
            Rect xz = { row.x + row.w + 4, row.y + 2, 24, 24 };
            bool xh = mouse_in(xz.x, xz.y, xz.w, xz.h);
            if (xh) fill_round_rect(xz.x, xz.y, xz.w, xz.h, 4, THEME->close_btn_bg);
            icon_draw(IC_CLOSE, xz.x + 5, xz.y + 5, 14, xh ? 0x00FFFFFF : THEME->text_dim);
        }
        // clear button under the list
        Rect cb = b->r_clear;
        if (cb.w > 0) {
            bool hov = mouse_in(cb.x, cb.y, cb.w, cb.h);
            fill_round_rect(cb.x, cb.y, cb.w, cb.h, 6, hov ? THEME->start_bg_active : THEME->button_bg);
            round_rect_outline(cb.x, cb.y, cb.w, cb.h, 6, THEME->menu_border);
            draw_text(cb.x + (cb.w - text_width("Remove all bookmarks", 1)) / 2, text_center_y(cb.y, cb.h, 1),
                      "Remove all bookmarks", THEME->button_fg, 1);
        }
        if (b->nbm == 0) draw_text(cr.x + 24, cr.y + 84, "No bookmarks yet. Press Ctrl+D or the star to add this page.", THEME->text_dim, 1);
    } else if (t->page == XP_HISTORY) {
        draw_text_bold(cr.x + 24, cr.y + 24, "History", THEME->text, 2);
        draw_text(cr.x + 24, cr.y + 54, "Pages you actually visited in this session (newest first).", THEME->text_dim, 1);
        for (int i = 0; i < b->nrows; i++) {
            int idx = (b->hist_head - 1 - i + HIST_N * 2) % HIST_N;
            if (b->hist_n < HIST_N && idx >= b->hist_n) continue;
            Rect row = b->r_row[i];
            if (mouse_in(row.x, row.y, row.w, row.h)) fill_round_rect(row.x, row.y, row.w, row.h, 4, THEME->select_bg);
            icon_draw(IC_CLOCK, row.x + 4, row.y + 6, 14, THEME->text_dim);
            draw_text(row.x + 26, text_center_y(row.y, row.h, 1), b->hist[idx].title, THEME->text, 1);
            int tw = text_width(b->hist[idx].title, 1);
            draw_text(row.x + 30 + tw, text_center_y(row.y, row.h, 1), b->hist[idx].url, THEME->text_dim, 1);
        }
        Rect cb = b->r_clear;
        if (cb.w > 0) {
            bool hov = mouse_in(cb.x, cb.y, cb.w, cb.h);
            fill_round_rect(cb.x, cb.y, cb.w, cb.h, 6, hov ? THEME->start_bg_active : THEME->button_bg);
            round_rect_outline(cb.x, cb.y, cb.w, cb.h, 6, THEME->menu_border);
            draw_text(cb.x + (cb.w - text_width("Clear browsing history", 1)) / 2, text_center_y(cb.y, cb.h, 1),
                      "Clear browsing history", THEME->button_fg, 1);
        }
        if (b->hist_n == 0) draw_text(cr.x + 24, cr.y + 84, "No history yet.", THEME->text_dim, 1);
    } else if (t->page == XP_DOWNLOADS) {
        icon_draw(IC_DOWNLOAD, cr.x + cr.w / 2 - 16, cr.y + 70, 32, THEME->text_dim);
        draw_centered_x("No downloads", cr, 116, THEME->text, 2, true);
        draw_centered_x("Downloads need a network stack and real file storage.", cr, 156, THEME->text_dim, 1, false);
        draw_centered_x("Kian OS has neither yet, so Xenox never shows", cr, 174, THEME->text_dim, 1, false);
        draw_centered_x("fabricated download progress.", cr, 192, THEME->text_dim, 1, false);
    } else if (t->page == XP_ABOUT) {
        icon_draw(IC_XENOX, cr.x + cr.w / 2 - 24, cr.y + 40, 48, THEME->accent);
        draw_centered_x("Xenox 0.1", cr, 100, THEME->text, 2, true);
        draw_centered_x("The Kian OS browser with a Chromium-inspired interface.", cr, 136, THEME->text_dim, 1, false);
        int ty = cr.y + 170;
        const char *lines[6] = {
            "Interface:    Chromium-inspired, drawn natively",
            "Renderer:     built-in native page renderer (local pages only)",
            "Chromium:     NOT ported",
            "Why:          a real Chromium engine needs mmap, threads, sync,",
            "              TCP/IP + TLS, a real filesystem and a hosted C++",
            "              runtime - none of these exist in the kernel yet."
        };
        for (int i = 0; i < 6; i++) {
            draw_text(cr.x + 40, ty + i * 20, lines[i], (i == 2) ? THEME->text : THEME->text_dim, 1);
        }
        icon_draw(IC_LOCK, cr.x + cr.w / 2 - 8, ty + 6 * 20 + 12, 16, THEME->text_dim);
        draw_centered_x("Local pages only. Nothing on this page pretends to be the web.", cr, ty + 6 * 20 + 34, THEME->text_dim, 1, false);
    } else if (t->page == XP_ERROR) {
        icon_draw(IC_WARN, cr.x + cr.w / 2 - 12, cr.y + 70, 24, THEME->accent);
        draw_centered_x("This page can't be shown", cr, 108, THEME->text, 2, true);
        draw_centered_x(t->err_url, cr, 146, THEME->text_dim, 1, false);
        // wrap the reason into <= 2 rows of ~70 chars
        draw_text(cr.x + 30, cr.y + 172, t->reason, THEME->text_dim, 1);
        Rect gs = b->r_go_start;
        bool hov = mouse_in(gs.x, gs.y, gs.w, gs.h);
        fill_round_rect(gs.x, gs.y, gs.w, gs.h, 6, hov ? THEME->start_bg_active : THEME->button_bg);
        draw_text(gs.x + (gs.w - text_width("Go to Start", 1)) / 2, text_center_y(gs.y, gs.h, 1), "Go to Start", THEME->button_fg, 1);
        Rect rl = b->r_reload_pg;
        bool hov2 = mouse_in(rl.x, rl.y, rl.w, rl.h);
        fill_round_rect(rl.x, rl.y, rl.w, rl.h, 6, hov2 ? THEME->start_bg_active : THEME->button_bg);
        draw_text(rl.x + (rl.w - text_width("Reload", 1)) / 2, text_center_y(rl.y, rl.h, 1), "Reload", THEME->button_fg, 1);
    }
}

void xenox_draw(void *inst, Rect r) {
    XenoxApp *b = (XenoxApp *)inst;
    layout(b, r);
    fill_rect(r.x, r.y, r.w, r.h, THEME->title_bg);

    // ---- tab strip ----
    for (int i = 0; i < b->ntabs; i++) {
        Rect tr = b->r_tabs[i];
        if (tr.w <= 0) continue;
        bool act = (i == b->active);
        fill_round_rect(tr.x, tr.y, tr.w, tr.h + 4, 6, act ? THEME->win_bg : THEME->input_bg);
        icon_draw(IC_XENOX, tr.x + 6, tr.y + (tr.h - 14) / 2, 14, act ? THEME->accent : THEME->text_dim);
        int tx = tr.x + 24;
        int maxw = tr.w - 38;
        for (int k = 0; b->tab[i].title[k] != '\0' && k < 40; k++) {
            char one[2] = { b->tab[i].title[k], '\0' };
            int aw = glyph_advance(one[0], 1);
            if (tx + aw > tr.x + 24 + maxw) { draw_text(tx, text_center_y(tr.y, tr.h, 1) - 2, "...", THEME->text, 1); break; }
            draw_text(tx, text_center_y(tr.y, tr.h, 1) - 2, one, act ? THEME->text : THEME->text_dim, 1);
            tx += aw;
        }
        Rect xz = { tr.x + tr.w - 16, tr.y + 4, 14, 18 };
        bool xh = mouse_in(xz.x, xz.y, xz.w, xz.h);
        if (xh) fill_round_rect(xz.x, xz.y, xz.w, xz.h, 4, xh && act ? THEME->start_bg_active : THEME->start_bg_active);
        icon_draw(IC_CLOSE, xz.x + 1, xz.y + 3, 12, xh ? THEME->text : THEME->text_dim);
        if (act) hline(tr.x + 4, tr.y + tr.h + 3, tr.w - 8, THEME->accent);
    }
    bool nth = mouse_in(b->r_newtab.x, b->r_newtab.y, b->r_newtab.w, b->r_newtab.h);
    if (nth) fill_round_rect(b->r_newtab.x, b->r_newtab.y, b->r_newtab.w, b->r_newtab.h, 4, THEME->start_bg_active);
    icon_draw(IC_PLUS, b->r_newtab.x + 4, b->r_newtab.y + 2, 16, THEME->text);

    // ---- toolbar ----
    int ty = r.y + TAB_H;
    fill_rect(r.x, ty, r.w, TB_H, THEME->title_bg);
    XTab *t = cur_tab(b);
    draw_icon_btn(b->r_back, IC_BACK, t && t->back_n > 0, THEME->text);
    draw_icon_btn(b->r_fwd, IC_FORWARD, t && t->fwd_n > 0, THEME->text);
    draw_icon_btn(b->r_reload, IC_RELOAD, true, THEME->text);
    draw_icon_btn(b->r_home, IC_HOME, true, THEME->text);
    b->omni.x = b->r_omni.x; b->omni.y = b->r_omni.y;
    b->omni.w = b->r_omni.w; b->omni.h = b->r_omni.h;
    tbox_draw(&b->omni);
    icon_draw(IC_LOCK, b->r_omni.x + 8, b->r_omni.y + 5, 14, THEME->text_dim);
    bool star_on = t && tab_bookmarked(b, t->url);
    draw_icon_btn(b->r_star, IC_BOOKMARK, true, star_on ? THEME->accent : THEME->text);
    fill_circle(b->r_profile.x + 12, b->r_profile.y + 13, 10, THEME->accent);
    draw_text(b->r_profile.x + 8, text_center_y(b->r_profile.y, b->r_profile.h, 1) - 1, "K", 0x00FFFFFF, 1);
    draw_icon_btn(b->r_menu, IC_DOTS, true, THEME->text);

    // suggestions dropdown (Chrome-style)
    for (int i = 0; i < b->nsug; i++) {
        Rect sr = b->r_sug[i];
        fill_round_rect(sr.x, sr.y, sr.w, sr.h, 4, THEME->menu_bg);
        round_rect_outline(sr.x, sr.y, sr.w, sr.h, 4, THEME->menu_border);
        icon_draw(IC_CLOCK, sr.x + 6, sr.y + 4, 12, THEME->text_dim);
        draw_text(sr.x + 24, text_center_y(sr.y, sr.h, 1), b->sug_url[i], THEME->text, 1);
    }

    // ---- page ----
    draw_page(b, r);

    // ---- status bar ----
    Rect st = { r.x, r.y + r.h - ST_H, r.w, ST_H };
    fill_rect(st.x, st.y, st.w, st.h, THEME->title_bg);
    hline(st.x, st.y, st.w, THEME->menu_border);
    draw_text(st.x + 10, text_center_y(st.y, st.h, 1), b->status, THEME->text_dim, 1);
    const char *eng = "xenox native renderer - Chromium engine not ported";
    draw_text(st.x + st.w - text_width(eng, 1) - 10, text_center_y(st.y, st.h, 1), eng, THEME->text_dim, 1);
}

// ---------------- menu ----------------

static void xenox_menu_cb(void *ud, int id) {
    XenoxApp *b = (XenoxApp *)ud;
    XTab *t = cur_tab(b);
    if (id == 1) {   // New tab
        if (b->ntabs < XENOX_TABS) {
            b->active = b->ntabs++;
            XTab *nt = cur_tab(b);
            nt->alive = true; nt->page = XP_START; nt->url[0] = '\0'; nt->title[0] = '\0';
            nt->back_n = 0; nt->fwd_n = 0;
            sxcpy(nt->url, XENOX_URL_MAX, "xenox://start");
            apply_url(b, nt, nt->url);
            tbox_set_text(&b->omni, nt->url);
            b->editing = false;
            shell_set_title(nt->title);
            shell_full_redraw();
        } else shell_toast("Xenox: tab pool full (8 tabs)");
    } else if (id == 2) {
        shell_toast("New window: one window per app is the current limit");
    } else if (id == 3 && t) nav_to(b, t, "xenox://history", true);
    else if (id == 4 && t) nav_to(b, t, "xenox://downloads", true);
    else if (id == 5 && t) nav_to(b, t, "xenox://bookmarks", true);
    else if (id == 6) app_open(APP_SETTINGS);
    else if (id == 7 && t) nav_to(b, t, "xenox://about", true);
    else if (id == 8) shell_close_focused();
    shell_full_redraw();
}

// ---------------- events ----------------

static void omnibox_submit(XenoxApp *b) {
    char url[XENOX_URL_MAX];
    const char *text = tbox_text(&b->omni);
    int i = 0;
    while (text[i] == ' ') i++;
    sxcpy(url, XENOX_URL_MAX, text + i);
    // trim trailing spaces
    int L = (int)strlen(url);
    while (L > 0 && url[L - 1] == ' ') url[--L] = '\0';
    b->editing = false;
    b->omni.focused = false;
    if (url[0] == '\0') return;
    XTab *t = cur_tab(b);
    if (!t) return;
    // scheme-less URLs get http:// in Chrome; Kian has no network, so every
    // external address lands on the honest error page either way.
    bool known = sxlower_eq_prefix(url, "xenox://");
    if (!known) {
        bool http = sxlower_eq_prefix(url, "http://") || sxlower_eq_prefix(url, "https://");
        bool dotted = false;
        for (int k = 0; url[k] != '\0'; k++) if (url[k] == '.') dotted = true;
        if (!http && !dotted && strcmp(url, "localhost") != 0) {
            // looks like a search query -> honest "cannot search" page
            char q[96];
            sxcpy(q, 96, "xenox://error?q=");
            int ql = (int)strlen(q);
            for (int k = 0; url[k] != '\0' && ql < 90; k++) q[ql++] = url[k];
            q[ql] = '\0';
            sxcpy(url, XENOX_URL_MAX, q);
        }
    }
    nav_to(b, t, url, true);
}

static void add_or_remove_bookmark(XenoxApp *b) {
    XTab *t = cur_tab(b);
    if (!t || t->url[0] == '\0') return;
    for (int i = 0; i < b->nbm; i++) {
        if (strcmp(b->bm[i].url, t->url) == 0) {
            for (int k = i; k < b->nbm - 1; k++) b->bm[k] = b->bm[k + 1];
            b->nbm--;
            bookmarks_save(b);
            shell_toast("Bookmark removed");
            return;
        }
    }
    if (b->nbm >= BM_N) { shell_toast("Bookmark pool full (8)"); return; }
    sxcpy(b->bm[b->nbm].title, 64, t->title);
    sxcpy(b->bm[b->nbm].url, XENOX_URL_MAX, t->url);
    b->nbm++;
    bookmarks_save(b);
    shell_toast("Bookmark added (RAM-backed, lost on reboot)");
}

static void close_tab(XenoxApp *b, int idx) {
    if (idx < 0 || idx >= b->ntabs) return;
    for (int i = idx; i < b->ntabs - 1; i++) b->tab[i] = b->tab[i + 1];
    b->ntabs--;
    if (b->ntabs == 0) { shell_close_focused(); return; }
    if (b->active >= b->ntabs) b->active = b->ntabs - 1;
    XTab *t = cur_tab(b);
    if (t) {
        if (!b->editing) tbox_set_text(&b->omni, t->url);
        shell_set_title(t->title);
    }
    shell_full_redraw();
}

void xenox_event(void *inst, const kian_event_t *e, Rect r) {
    XenoxApp *b = (XenoxApp *)inst;
    layout(b, r);
    XTab *t = cur_tab(b);

    if (e->type == KIAN_EV_KEY_DOWN) {
        int a = e->a, c = e->c;
        bool ctrl = (c & 2) != 0, alt = (c & 4) != 0;
        if (b->editing && !ctrl && !alt) {
            bool enter = tbox_key(&b->omni, e);
            if (a == KIAN_KEY_ESC || e->b == 27) {
                b->editing = false;
                b->omni.focused = false;
                if (t) tbox_set_text(&b->omni, t->url);
                return;
            }
            if (enter) omnibox_submit(b);
            return;
        }
        if (ctrl && (e->b == 't' || e->b == 'T')) { xenox_menu_cb(b, 1); return; }
        if (ctrl && (e->b == 'w' || e->b == 'W')) { close_tab(b, b->active); return; }
        if (ctrl && (e->b == 'l' || e->b == 'L')) {
            b->editing = true;
            tbox_focus(&b->omni, true);
            tbox_set_text(&b->omni, t ? t->url : "");
            return;
        }
        if (ctrl && (e->b == 'r' || e->b == 'R')) {
            if (t) { b->status[0] = '\0'; sxcpy(b->status, 80, "Reloaded (local render)"); }
            return;
        }
        if (ctrl && (e->b == 'd' || e->b == 'D')) { add_or_remove_bookmark(b); return; }
        if (ctrl && (e->b == 'h' || e->b == 'H')) { if (t) nav_to(b, t, "xenox://history", true); return; }
        if (ctrl && (e->b == 'j' || e->b == 'J')) { if (t) nav_to(b, t, "xenox://downloads", true); return; }
        if (alt && a == KIAN_KEY_LEFT)  { if (t) nav_back(b, t); return; }
        if (alt && a == KIAN_KEY_RIGHT) { if (t) nav_forward(b, t); return; }
        if (a == KIAN_KEY_ESC || e->b == 27) { /* keep ESC for the shell */ return; }
        return;
    }

    if (e->type != KIAN_EV_MOUSE_BTN) {
        if (e->type == KIAN_EV_MOUSE_WHEEL && t && (t->page == XP_HISTORY || t->page == XP_BOOKMARKS)) {
            // list pages scroll with the wheel (bounded by row count)
            int dir = (e->a > 0) ? -1 : 1;
            int ns = b->scroll + dir;
            if (ns < 0) ns = 0;
            if (ns > 64) ns = 64;
            b->scroll = ns;
        }
        return;
    }
    if (!IN->press_l) {
        // right click inside the page -> context menu
        if (IN->press_r && t) {
            static const MenuItem items[8] = {
                { "New tab", 1 }, { "New window", 2 }, { "-", -1 },
                { "History", 3 }, { "Downloads", 4 }, { "Bookmarks", 5 },
                { "About Xenox", 7 }, { "Close browser", 8 }
            };
            menu_open(&SH->menu, items, 8, xenox_menu_cb, b, IN->mx, IN->my);
        }
        return;
    }

    // ---- left click dispatch ----
    // suggestions first (drawn above everything)
    for (int i = 0; i < b->nsug; i++) {
        if (rect_hit(IN->mx, IN->my, b->r_sug[i])) {
            char url[XENOX_URL_MAX];
            sxcpy(url, XENOX_URL_MAX, b->sug_url[i]);
            b->editing = false;
            b->omni.focused = false;
            if (t) nav_to(b, t, url, true);
            return;
        }
    }
    if (rect_hit(IN->mx, IN->my, b->r_newtab)) { xenox_menu_cb(b, 1); return; }
    for (int i = 0; i < b->ntabs; i++) {
        Rect tr = b->r_tabs[i];
        if (tr.w <= 0) continue;
        Rect xz = { tr.x + tr.w - 16, tr.y + 4, 14, 18 };
        if (rect_hit(IN->mx, IN->my, xz)) { close_tab(b, i); return; }
        if (rect_hit(IN->mx, IN->my, tr)) {
            if (i != b->active) {
                b->active = i;
                t = cur_tab(b);
                if (!b->editing) tbox_set_text(&b->omni, t->url);
                shell_set_title(t->title);
            }
            return;
        }
    }
    if (rect_hit(IN->mx, IN->my, b->r_back))  { if (t) nav_back(b, t); return; }
    if (rect_hit(IN->mx, IN->my, b->r_fwd))   { if (t) nav_forward(b, t); return; }
    if (rect_hit(IN->mx, IN->my, b->r_reload)) {
        if (t) { b->status[0] = '\0'; sxcpy(b->status, 80, "Reloaded (local render)"); }
        return;
    }
    if (rect_hit(IN->mx, IN->my, b->r_home))  { if (t) nav_to(b, t, "xenox://start", true); return; }
    if (rect_hit(IN->mx, IN->my, b->r_omni))  {
        tbox_mouse(&b->omni, e);
        b->editing = true;
        b->omni.focused = true;
        return;
    }
    if (rect_hit(IN->mx, IN->my, b->r_star))  { add_or_remove_bookmark(b); return; }
    if (rect_hit(IN->mx, IN->my, b->r_menu))  {
        static const MenuItem items[8] = {
            { "New tab", 1 }, { "New window", 2 }, { "-", -1 },
            { "History", 3 }, { "Downloads", 4 }, { "Bookmarks", 5 },
            { "About Xenox", 7 }, { "Close browser", 8 }
        };
        menu_open(&SH->menu, items, 8, xenox_menu_cb, b, b->r_menu.x - 130, b->r_menu.y + 26);
        return;
    }
    if (t && t->page == XP_ERROR) {
        if (rect_hit(IN->mx, IN->my, b->r_go_start)) { nav_to(b, t, "xenox://start", true); return; }
        if (rect_hit(IN->mx, IN->my, b->r_reload_pg)) { return; }
    }
    if (t && t->page == XP_BOOKMARKS) {
        for (int i = 0; i < b->nbm && i < 12; i++) {
            Rect row = b->r_row[i];
            Rect xz = { row.x + row.w + 4, row.y + 2, 24, 24 };
            if (rect_hit(IN->mx, IN->my, xz)) {
                for (int k = i; k < b->nbm - 1; k++) b->bm[k] = b->bm[k + 1];
                b->nbm--;
                bookmarks_save(b);
                return;
            }
            if (rect_hit(IN->mx, IN->my, row)) {
                char url[XENOX_URL_MAX];
                sxcpy(url, XENOX_URL_MAX, b->bm[i].url);
                nav_to(b, t, url, true);
                return;
            }
        }
        if (b->r_clear.w > 0 && rect_hit(IN->mx, IN->my, b->r_clear)) {
            b->nbm = 0;
            bookmarks_save(b);
            shell_toast("All bookmarks removed");
            return;
        }
    } else if (t && t->page == XP_HISTORY) {
        for (int i = 0; i < b->nrows; i++) {
            int idx = (b->hist_head - 1 - i + HIST_N * 2) % HIST_N;
            if (b->hist_n < HIST_N && idx >= b->hist_n) continue;
            if (rect_hit(IN->mx, IN->my, b->r_row[i])) {
                char url[XENOX_URL_MAX];
                sxcpy(url, XENOX_URL_MAX, b->hist[idx].url);
                nav_to(b, t, url, false);
                return;
            }
        }
        if (b->r_clear.w > 0 && rect_hit(IN->mx, IN->my, b->r_clear)) {
            b->hist_n = 0;
            b->hist_head = 0;
            shell_toast("History cleared");
            return;
        }
    } else if (t && t->page == XP_START) {
        for (int i = 0; i < b->nrows; i++) {
            if (rect_hit(IN->mx, IN->my, b->r_row[i]) && b->sug_url[i][0] != '\0') {
                char url[XENOX_URL_MAX];
                sxcpy(url, XENOX_URL_MAX, b->sug_url[i]);
                nav_to(b, t, url, true);
                return;
            }
        }
    }
}

void *xenox_create() {
    XenoxApp *b = new XenoxApp();
    for (int i = 0; i < XENOX_TABS; i++) {
        b->tab[i].alive = false;
        b->tab[i].page = XP_START;
        b->tab[i].url[0] = '\0';
        b->tab[i].title[0] = '\0';
        b->tab[i].err_url[0] = '\0';
        b->tab[i].reason[0] = '\0';
        b->tab[i].back_n = 0;
        b->tab[i].fwd_n = 0;
    }
    b->ntabs = 1;
    b->active = 0;
    b->tab[0].alive = true;
    apply_url(b, &b->tab[0], "xenox://start");
    b->editing = false;
    tbox_init(&b->omni, 0, 0, 300, 26, false);
    tbox_set_text(&b->omni, "xenox://start");
    b->nbm = 0;
    bookmarks_load(b);
    b->hist_n = 0;
    b->hist_head = 0;
    b->scroll = 0;
    b->nsug = 0;
    b->nrows = 0;
    b->status[0] = '\0';
    sxcpy(b->status, 80, "Done");
    history_push(b, "xenox://start", "New Tab");
    if (!g_milestone) {
        g_milestone = true;
        kian_serial_write("GUI: Xenox browser initialized\n", 31);
    }
    return b;
}

void xenox_destroy(void *inst) {
    XenoxApp *b = (XenoxApp *)inst;
    delete b;
}
