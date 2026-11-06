// Kian OS GUI - Kian Browser: honest placeholder. Address bar + local start
// page. It does NOT fake web pages and says networking is not implemented.
#include "../apps.h"
#include "../gui.h"
#include "../input.h"
#include "../widgets.h"
#include "../icons.h"
#include "../util.h"

static void draw_centered_b(const char *s, int y, uint32_t c, int scale) {
    int w = text_width(s, scale);
    draw_text((GFX->w - w) / 2, y, s, c, scale);
}

struct BrowserApp {
    TBox addr;
};

void *browser_create() {
    BrowserApp *b = new BrowserApp();
    tbox_init(&b->addr, 0, 0, 300, 26, false);
    tbox_set_text(&b->addr, "kian://start");
    return b;
}

void browser_destroy(void *inst) {
    BrowserApp *b = (BrowserApp *)inst;
    delete b;
}

void browser_draw(void *inst, Rect r) {
    BrowserApp *b = (BrowserApp *)inst;
    fill_rect(r.x, r.y, r.w, r.h, THEME->win_bg);

    // toolbar: back + address bar
    int ty = r.y + 6;
    fill_round_rect(r.x + 6, ty, 24, 24, 4, mouse_in(r.x + 6, ty, 24, 24) ? THEME->start_bg_active : THEME->button_bg);
    icon_draw(IC_BACK, r.x + 10, ty + 4, 16, THEME->button_fg);
    b->addr.x = r.x + 36;
    b->addr.y = ty;
    b->addr.w = r.w - 42;
    b->addr.h = 26;
    tbox_draw(&b->addr);

    // local start page
    int cy = r.y + 90;
    icon_draw(IC_BROWSER, r.x + r.w / 2 - 20, cy - 40, 40, THEME->accent);
    draw_centered_b("Kian Browser", cy, THEME->text, 2);
    draw_centered_b("Networking is not implemented yet.", cy + 40, THEME->text_dim, 1);
    draw_centered_b("This start page is built into the browser.", cy + 58, THEME->text_dim, 1);
    draw_centered_b("No fake web pages are rendered.", cy + 76, THEME->text_dim, 1);
}

void browser_event(void *inst, const kian_event_t *e, Rect r) {
    (void)r;
    BrowserApp *b = (BrowserApp *)inst;
    if (tbox_key(&b->addr, e)) {
        shell_toast("Networking is not implemented yet");
    }
    if (e->type == KIAN_EV_MOUSE_BTN && IN->press_l) {
        tbox_mouse(&b->addr, e);
        // back button is decorative here (no history)
    }
}
