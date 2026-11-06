// Kian OS GUI - desktop implementation: themes, wallpaper (vertical gradient),
// taskbar with pinned icons / tray / two-line clock, start menu.
#include "desktop.h"
#include "wm.h"

const int PIN_TYPE[PIN_COUNT] = { APP_TERM, APP_FILES, APP_NOTES, APP_SETTINGS };
static const char *PIN_LABEL[PIN_COUNT] = { "Term", "Files", "Notes", "Set" };

void theme_dark(Theme &t) {
    t.wallpaper_top    = 0x001A2B4A; // dark blue
    t.wallpaper_bottom = 0x003B0A45; // purple
    t.taskbar_bg       = 0x00101828;
    t.taskbar_fg       = 0x00E8E8F0;
    t.start_bg         = 0x00283560;
    t.start_bg_active  = 0x00405090;
    t.menu_bg          = 0x001C2440;
    t.menu_border      = 0x00405090;
    t.win_frame        = 0x00303A58;
    t.win_bg           = 0x00141C30;
    t.title_bg         = 0x00283560;
    t.title_text       = 0x00F0F0F8;
    t.text             = 0x00E8E8F0;
    t.button_bg        = 0x00283048;
    t.button_fg        = 0x00F0F0F8;
    t.close_btn_bg     = 0x00A03030;
    t.shadow           = 0x00000010;
    t.icon_bg          = 0x00283560;
    t.underline        = 0x0050A0FF;
}

void theme_light(Theme &t) {
    t.wallpaper_top    = 0x0088AACC;
    t.wallpaper_bottom = 0x00AA88CC;
    t.taskbar_bg       = 0x00D0D8E8;
    t.taskbar_fg       = 0x00101828;
    t.start_bg         = 0x00B8C4E0;
    t.start_bg_active  = 0x0090A0D0;
    t.menu_bg          = 0x00F0F2FA;
    t.menu_border      = 0x008090B8;
    t.win_frame        = 0x008890A8;
    t.win_bg           = 0x00F4F6FC;
    t.title_bg         = 0x00C0CAF0;
    t.title_text       = 0x00101828;
    t.text             = 0x00101828;
    t.button_bg        = 0x00D8DDE8;
    t.button_fg        = 0x00101828;
    t.close_btn_bg     = 0x00E08080;
    t.shadow           = 0x008090A0;
    t.icon_bg          = 0x00C0CCE8;
    t.underline        = 0x002060C0;
}

void draw_wallpaper(Surface &s, const Theme &th) {
    // vertical gradient dark blue -> purple using 16.16 fixed point
    int32_t h_fx = fx_from_int(s.h > 1 ? s.h - 1 : 1);
    for (int y = 0; y < s.h; y++) {
        int32_t t = fx_div(fx_from_int(y), h_fx);           // 0..65536 (16.16)
        uint32_t out = 0;
        for (int shift = 0; shift < 32; shift += 8) {
            int32_t a = (int32_t)((th.wallpaper_top >> shift) & 0xFF);
            int32_t b = (int32_t)((th.wallpaper_bottom >> shift) & 0xFF);
            int32_t v = a + ((fx_mul(t, fx_from_int(b - a))) >> 16);
            out |= ((uint32_t)(v & 0xFF)) << shift;
        }
        uint32_t *row = s.buf + (size_t)y * s.w;
        for (int x = 0; x < s.w; x++) row[x] = out;
    }
}

Rect taskbar_icon_rect(int i) {
    Rect r = { 56 + i * 56, 3, 48, TB_H - 6 };
    return r;
}

Rect start_button_rect() {
    Rect r = { 4, 3, 48, TB_H - 6 };
    return r;
}

const int MENU_ITEMS = 7;
static const char *MENU_LABEL[MENU_ITEMS] = {
    "Terminal", "Files", "Settings", "Calculator", "Exit to console", "Restart", "Shut down"
};

Rect menu_panel_rect() {
    Rect r = { 4, 0, 180, MENU_ITEMS * 20 + 12 };
    r.y = TB_H + 2 - r.h; // grows upward from above the taskbar
    return r;
}

Rect menu_item_rect(int i) {
    Rect p = menu_panel_rect();
    Rect r = { p.x + 4, p.y + 6 + i * 20, p.w - 8, 18 };
    return r;
}

static void draw_icon(Surface &s, const Theme &th, Rect r, char letter, bool running) {
    fill_round_rect(s, r.x + 8, r.y + 2, 14, 14, 3, th.icon_bg);
    char buf[2] = { letter, '\0' };
    draw_text(s, r.x + 8 + 5, r.y + 2 + 3, buf, th.taskbar_fg, 1);
    draw_text(s, r.x + 10, r.y + 17, PIN_LABEL[letter == 'T' ? 0 : letter == 'F' ? 1 : letter == 'N' ? 2 : 3], th.taskbar_fg, 1);
    if (running) {
        hline(s, r.x + 8, r.y + TB_H - 8, 14, th.underline);
    }
}

void draw_taskbar(Surface &s, const Theme &th, WM &wm, bool start_open) {
    fill_rect(s, 0, s.h - TB_H, s.w, TB_H, th.taskbar_bg);
    hline(s, 0, s.h - TB_H, s.w, th.menu_border);

    // Start button
    Rect sb = start_button_rect();
    fill_round_rect(s, sb.x, sb.y, sb.w, sb.h, 3, start_open ? th.start_bg_active : th.start_bg);
    draw_text(s, sb.x + 6, sb.y + 5, "Start", th.taskbar_fg, 1);

    // pinned icons with running-window underlines
    for (int i = 0; i < PIN_COUNT; i++) {
        Rect r = taskbar_icon_rect(i);
        bool running = (wm.find_type(PIN_TYPE[i]) != 0);
        char letter = PIN_LABEL[i][0];
        draw_icon(s, th, r, letter, running);
    }

    // tray: ENG, network, volume + two-line clock (Asia/Tehran)
    kian_time_t t;
    kian_time_now(&t);
    int hour12 = ((t.hour + 11) % 12) + 1;
    char l1[16], l2[24];
    ksnprintf(l1, sizeof(l1), "%d:%02d %s", hour12, t.minute, (t.hour < 12 ? "AM" : "PM"));
    ksnprintf(l2, sizeof(l2), "%d/%d/%d", t.month, t.day, t.year);

    int x = s.w - 8;
    // clock block (rightmost)
    int clock_w = 84;
    x -= clock_w;
    fill_round_rect(s, x, 3, clock_w, TB_H - 6, 3, th.start_bg);
    draw_text(s, x + 4, 4, l1, th.taskbar_fg, 1);
    draw_text(s, x + 4, 14, l2, th.taskbar_fg, 1);

    // ENG (keyboard layout)
    x -= 40;
    draw_text(s, x + 8, 11, "ENG", th.taskbar_fg, 1);
    // network icon: small antenna mast
    x -= 22;
    vline(s, x + 10, s.h - TB_H + 7, 12, th.taskbar_fg);
    fill_round_rect(s, x + 6, s.h - TB_H + 17, 9, 3, 1, th.taskbar_fg);
    hline(s, x + 6, s.h - TB_H + 9, 9, th.taskbar_fg);
    hline(s, x + 8, s.h - TB_H + 11, 5, th.taskbar_fg);
    // volume icon: speaker
    x -= 24;
    int by = s.h - TB_H + 9;
    fill_rect(s, x + 6, by + 2, 3, 6, th.taskbar_fg);
    fill_rect(s, x + 9, by + 4, 2, 6, th.taskbar_fg);
    hline(s, x + 13, by + 3, 3, th.taskbar_fg);
    hline(s, x + 13, by + 7, 3, th.taskbar_fg);
    hline(s, x + 15, by + 2, 2, th.taskbar_fg);
    hline(s, x + 15, by + 8, 2, th.taskbar_fg);
}

void draw_start_menu(Surface &s, const Theme &th, int hover_item) {
    Rect p = menu_panel_rect();
    fill_round_rect(s, p.x, p.y, p.w, p.h, 6, th.menu_border);
    fill_round_rect(s, p.x + 2, p.y + 2, p.w - 4, p.h - 4, 5, th.menu_bg);
    for (int i = 0; i < MENU_ITEMS; i++) {
        Rect r = menu_item_rect(i);
        if (i == hover_item) {
            fill_rect(s, r.x, r.y, r.w, r.h, th.start_bg_active);
        }
        draw_text(s, r.x + 6, r.y + 5, MENU_LABEL[i], th.text, 1);
    }
}

int start_menu_item_at(int x, int y) {
    Rect p = menu_panel_rect();
    if (x < p.x || x >= p.x + p.w || y < p.y || y >= p.y + p.h) return -1;
    for (int i = 0; i < MENU_ITEMS; i++) {
        Rect r = menu_item_rect(i);
        if (x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h) return i;
    }
    return -1;
}
