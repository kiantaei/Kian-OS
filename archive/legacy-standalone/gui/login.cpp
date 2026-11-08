// Kian OS GUI - login + lock screen (G6).
// Full-screen wallpaper darkened, big clock (scale 4), avatar, password box.
// Auth via kian_auth_check("kian", pass). Wrong password = red text + shake.
#include "gui.h"
#include "input.h"
#include "util.h"
#include "icons.h"

static void put_num_l(char *b, int &i, int v) {
    if (v < 0) { b[i++] = '-'; v = -v; }
    char tmp[8];
    int n = 0;
    if (v == 0) tmp[n++] = '0';
    while (v > 0 && n < 7) { tmp[n++] = (char)('0' + v % 10); v /= 10; }
    while (n > 0) b[i++] = tmp[--n];
    b[i] = '\0';
}

static void put_pad2_l(char *b, int &i, int v) {
    if (v >= 0 && v < 10) b[i++] = '0';
    put_num_l(b, i, v);
}

static void draw_lock_wallpaper() {
    gradient_v(0, 0, GFX->w, GFX->h, 0x00101828, 0x00283560);
    blend_rect(0, 0, GFX->w, GFX->h, 0x00000000, 110);
}

static void draw_centered(const char *s, int y, uint32_t c, int scale, bool bold) {
    int w = text_width(s, scale);
    if (bold) draw_text_bold((GFX->w - w) / 2, y, s, c, scale);
    else draw_text((GFX->w - w) / 2, y, s, c, scale);
}

static void draw_login_ui(TBox *pass, Btn *signin, bool lock_mode, bool wrong, unsigned wrong_ms) {
    draw_lock_wallpaper();

    // big clock + date (scale 4 via integer glyph scaling)
    kian_time_t t;
    kian_time_now(&t);
    char l1[24], l2[24];
    int i = 0;
    l1[0] = '\0';
    int hour = ((t.hour + 11) % 12) + 1;
    const char *ap = (t.hour < 12) ? " AM" : " PM";
    put_num_l(l1, i, hour);
    l1[i++] = ':';
    put_pad2_l(l1, i, t.minute);
    for (const char *p = ap; *p; p++) l1[i++] = *p;
    l1[i] = '\0';
    i = 0;
    l2[0] = '\0';
    put_num_l(l2, i, t.month);
    l2[i++] = '/';
    put_num_l(l2, i, t.day);
    l2[i++] = '/';
    put_num_l(l2, i, t.year);
    l2[i] = '\0';
    draw_centered(l1, GFX->h / 4 - 16, 0x00F0F0F8, 4, true);
    draw_centered(l2, GFX->h / 4 + 64, 0x00C0C8D8, 2, false);

    // avatar + user
    int cy = GFX->h / 2 + 30;
    fill_circle(GFX->w / 2, cy, 30, THEME->accent);
    draw_centered("K", cy - 14, 0x00FFFFFF, 2, true);
    draw_centered("kian", cy + 44, 0x00F0F0F8, 2, true);
    if (lock_mode) draw_centered("Locked", cy + 84, THEME->text_dim, 1, false);

    // password box (dots) + sign in
    int bw = 280;
    pass->x = GFX->w / 2 - bw / 2;
    pass->y = cy + 92;
    pass->w = bw;
    pass->h = 34;
    tbox_draw(pass);
    signin->x = GFX->w / 2 - bw / 2;
    signin->y = cy + 134;
    signin->w = bw;
    signin->h = 34;
    btn_draw(signin);
    if (wrong && now_ms() - wrong_ms < 1200) {
        draw_centered("Wrong password", cy + 180, 0x00E05050, 1, true);
    } else {
        draw_centered("Default password: kian (see README)", cy + 180, THEME->text_dim, 1, false);
    }
    present(0, 0, GFX->w, GFX->h);
}

static bool login_loop(bool lock_mode) {
    TBox pass;
    Btn signin;
    tbox_init(&pass, 0, 0, 280, 34, true);
    tbox_focus(&pass, true);
    btn_init(&signin, 0, 0, 280, 34, "Sign in", IC_NONE);
    bool wrong = false;
    unsigned wrong_ms = 0;

    for (;;) {
        kian_event_t ev;
        while (input_next(IN, &ev)) {
            bool submit = false;
            if (tbox_key(&pass, &ev)) submit = true;          // Enter pressed
            if (btn_event(&signin, &ev)) submit = true;       // button clicked
            tbox_mouse(&pass, &ev);
            if (ev.type == KIAN_EV_MOUSE_BTN && IN->press_l) tbox_focus(&pass, true);
            if (submit) {
                if (kian_auth_check("kian", tbox_text(&pass))) {
                    return true;
                }
                wrong = true;
                wrong_ms = now_ms();
                pass.err_until = now_ms() + 300; // shake 300 ms
                pass.buf[0] = '\0';
                pass.len = 0;
                pass.caret = 0;
            }
        }
        draw_login_ui(&pass, &signin, lock_mode, wrong, wrong_ms);
        for (int i = 0; i < 200; i++) __asm__ volatile("pause");
    }
}

bool login_run(bool require) {
    if (!require) return true;
    return login_loop(false);
}

void login_lock() {
    login_loop(true);
}
