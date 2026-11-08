// Kian OS GUI - Settings app: left navigation; Personalization changes the
// accent color / wallpaper / theme LIVE; honest placeholder pages elsewhere.
#include "../apps.h"
#include "../gui.h"
#include "../input.h"
#include "../widgets.h"
#include "../icons.h"
#include "../util.h"

enum {
    PG_SYSTEM = 0, PG_PERSON, PG_NETWORK, PG_TIME, PG_ACCOUNTS, PG_STORAGE, PG_ABOUT, PG_N
};
static const char *PG_NAMES[PG_N] = {
    "System", "Personalization", "Network", "Time & Language", "Accounts", "Storage", "About"
};

static const uint32_t ACCENTS[8] = {
    0x0078D4, 0x008764B8, 0x00C42B1C, 0x00CA5010,
    0x000F7B0F, 0x0003796B, 0x00C239B3, 0x00687684
};

struct SetApp {
    int page;
    Chk chk_dark;
    Chk chk_24h;
    int sel_wall;
    int accent_y;          // y of the accent swatch row (draw sets, event reads)
    int wall_y;            // y of the wallpaper previews
};

void *setapp_create() {
    SetApp *s = new SetApp();
    s->page = PG_PERSON;
    chk_init(&s->chk_dark, 0, 0, "Dark mode");
    chk_init(&s->chk_24h, 0, 0, "24-hour clock");
    s->chk_dark.checked = true;
    s->sel_wall = 0;
    s->accent_y = 0;
    s->wall_y = 0;
    return s;
}

void setapp_destroy(void *inst) {
    SetApp *s = (SetApp *)inst;
    delete s;
}

static void set_nav(SetApp *s, Rect r) {
    for (int i = 0; i < PG_N; i++) {
        int y = r.y + 10 + i * 30;
        bool act = (i == s->page);
        bool hov = mouse_in(r.x + 6, y, r.w - 24, 26);
        if (act) fill_round_rect(r.x + 6, y, r.w - 24, 26, 4, THEME->select_bg);
        else if (hov) fill_round_rect(r.x + 6, y, r.w - 24, 26, 4, THEME->start_bg_active);
        draw_text(r.x + 16, text_center_y(y, 26, 1), PG_NAMES[i], act ? THEME->accent : THEME->text, 1);
    }
}

void setapp_draw(void *inst, Rect r) {
    SetApp *s = (SetApp *)inst;
    fill_rect(r.x, r.y, r.w, r.h, THEME->win_bg);

    // navigation column
    set_nav(s, Rect{ r.x, r.y, 150, r.h });
    Rect c = { r.x + 150, r.y, r.w - 150, r.h };
    fill_rect(c.x, c.y, c.w, c.h, THEME->win_bg);
    vline(c.x, c.y, c.h, THEME->menu_border);

    int tx = c.x + 16, ty = c.y + 14;
    if (s->page == PG_PERSON) {
        draw_text_bold(tx, ty, "Personalization", THEME->text, 2);
        ty += 44;                                   // scale-2 title is 36 px em
        draw_text(tx, ty, "Accent color", THEME->text, 1);
        ty += 18;
        s->accent_y = ty;
        for (int i = 0; i < 8; i++) {
            int sx2 = tx + i * 34;
            fill_round_rect(sx2, ty, 28, 28, 4, ACCENTS[i]);
            if (ACCENTS[i] == THEME->accent) round_rect_outline(sx2, ty, 28, 28, 4, THEME->text);
            else if (mouse_in(sx2, ty, 28, 28)) round_rect_outline(sx2, ty, 28, 28, 4, THEME->menu_border);
        }
        ty += 40;
        draw_text(tx, ty, "Wallpaper", THEME->text, 1);
        ty += 18;
        s->wall_y = ty;
        static const uint32_t wt[3] = { 0x001A2B4A, 0x000E2A1E, 0x00102030 };
        static const uint32_t wb[3] = { 0x003B0A45, 0x00A05030, 0x006070A0 };
        for (int i = 0; i < 3; i++) {
            int wx = tx + i * 96;
            gradient_v(wx, ty, 88, 44, wt[i], wb[i]);
            if (i == s->sel_wall) round_rect_outline(wx, ty, 88, 44, 4, THEME->accent);
        }
        ty += 58;
        s->chk_dark.x = tx;
        s->chk_dark.y = ty;
        s->chk_dark.checked = (THEME->wallpaper_top == 0x001A2B4A);
        chk_draw(&s->chk_dark);
    } else if (s->page == PG_TIME) {
        draw_text_bold(tx, ty, "Time & Language", THEME->text, 2);
        ty += 44;
        draw_text(tx, ty, "Time zone: Asia/Tehran (UTC+03:30)", THEME->text, 1);
        draw_text(tx, ty + 16, "No daylight saving (fixed +03:30)", THEME->text_dim, 1);
        s->chk_24h.x = tx;
        s->chk_24h.y = ty + 42;
        s->chk_24h.checked = SH->desk.clock24;
        chk_draw(&s->chk_24h);
    } else if (s->page == PG_NETWORK) {
        draw_text_bold(tx, ty, "Network", THEME->text, 2);
        ty += 36;
        icon_draw(IC_WIFI, tx, ty, 20, THEME->accent);
        draw_text(tx + 28, ty + 6, "Connected (Ethernet)", THEME->text, 1);
        draw_text(tx, ty + 30, "Note: network drivers are not implemented yet.", THEME->text_dim, 1);
    } else if (s->page == PG_ACCOUNTS) {
        draw_text_bold(tx, ty, "Accounts", THEME->text, 2);
        ty += 36;
        fill_circle(tx + 20, ty + 20, 20, THEME->accent);
        draw_text_bold(tx + 12, ty + 12, "K", 0x00FFFFFF, 2);
        draw_text(tx + 52, ty + 14, "kian (local account)", THEME->text, 1);
    } else if (s->page == PG_STORAGE) {
        draw_text_bold(tx, ty, "Storage", THEME->text, 2);
        ty += 36;
        uint64_t used = 0, total = 0;
        kian_mem_stats(&used, &total);
        draw_text(tx, ty, "Heap (RAM disk files live here too, all volatile)", THEME->text, 1);
        fill_round_rect(tx, ty + 20, 300, 16, 4, THEME->icon_bg);
        // 64-bit math only: unit = total/300, then used/unit -> 0..300 (no __int128
        // division, that would emit a libgcc __divti3 call we do not have)
        unsigned long long unit = (total >= 300) ? (total / 300) : 1;
        unsigned long long fillv = (total > 0 && unit > 0) ? (used / unit) : 0;
        if (fillv > 300) fillv = 300;
        int fw2 = (int)fillv;
        fill_round_rect(tx, ty + 20, imax(6, fw2), 16, 4, THEME->accent);
        char buf[64];
        int k = 0;
        buf[0] = '\0';
        put_num(buf, k, (int)(used >> 20));
        buf[k++] = '\0';
        draw_text(tx, ty + 44, buf, THEME->text_dim, 1);
        draw_text(tx + 30, ty + 44, "MiB used", THEME->text_dim, 1);
    } else if (s->page == PG_ABOUT) {
        draw_text_bold(tx, ty, "About", THEME->text, 2);
        ty += 36;
        draw_text(tx, ty, "Kian OS 0.3.0 \"Horizon\"", THEME->text, 1);
        draw_text(tx, ty + 14, "Rust kernel, C console, C++ GUI", THEME->text, 1);
        draw_text(tx, ty + 28, "(c) kiantaei - educational hobby OS", THEME->text_dim, 1);
        ty += 52;
        uint64_t used = 0, total = 0;
        kian_mem_stats(&used, &total);
        draw_text(tx, ty, "RAM (heap):", THEME->text_dim, 1);
        char buf[48];
        int k = 0;
        buf[0] = '\0';
        put_num(buf, k, (int)(used >> 20));
        buf[k++] = '\0';
        draw_text(tx + 90, ty, buf, THEME->text, 1);
        draw_text(tx + 120, ty, "MiB used", THEME->text_dim, 1);
    } else { // System
        draw_text_bold(tx, ty, "System", THEME->text, 2);
        ty += 36;
        draw_text(tx, ty, "Kian OS 0.3.0 - x86_64, Limine boot", THEME->text, 1);
        draw_text(tx, ty + 14, "Single core, polling drivers (no interrupts)", THEME->text_dim, 1);
    }
}

void setapp_event(void *inst, const kian_event_t *e, Rect r) {
    SetApp *s = (SetApp *)inst;
    // navigation clicks
    if (e->type == KIAN_EV_MOUSE_BTN && IN->press_l) {
        for (int i = 0; i < PG_N; i++) {
            int y = r.y + 10 + i * 30;
            if (mouse_in(r.x + 6, y, 126, 26)) { s->page = i; shell_mark_content(); return; }
        }
        if (s->page == PG_PERSON) {
            int tx = r.x + 166, ty = s->accent_y;
            for (int i = 0; i < 8; i++) {
                if (mouse_in(tx + i * 34, ty, 28, 28)) {
                    bool dark = (THEME->wallpaper_top == 0x001A2B4A);
                    theme_set(THEME, dark, ACCENTS[i]);
                    shell_full_redraw();
                    return;
                }
            }
            ty = s->wall_y;
            for (int i = 0; i < 3; i++) {
                if (mouse_in(tx + i * 96, ty, 88, 44)) {
                    s->sel_wall = i;
                    SH->desk.wallpaper = i;
                    shell_full_redraw();
                    return;
                }
            }
            s->chk_dark.x = tx;
            s->chk_dark.y = s->wall_y + 58;
            if (chk_event(&s->chk_dark, e)) {
                bool dark = (THEME->wallpaper_top == 0x001A2B4A);
                theme_set(THEME, !dark, THEME->accent);
                shell_full_redraw();
                return;
            }
        } else if (s->page == PG_TIME) {
            s->chk_24h.x = r.x + 166;
            s->chk_24h.y = r.y + 100;
            if (chk_event(&s->chk_24h, e)) {
                SH->desk.clock24 = s->chk_24h.checked;
                shell_full_redraw();
                return;
            }
        }
    }
    // keep hover states fresh
    if (s->page == PG_PERSON && e->type == KIAN_EV_MOUSE_MOVE) shell_mark_content();
}
