// Kian OS GUI - input pump + cursor.
#include "input.h"
#include "gfx.h"
#include "util.h"

Input *IN = 0;

bool mouse_in(int x, int y, int w, int h) {
    if (!IN) return false;
    return IN->mx >= x && IN->mx < x + w && IN->my >= y && IN->my < y + h;
}

unsigned mouse_idle_ms() {
    if (!IN) return 0;
    unsigned now = now_ms();
    return (now >= IN->last_move_ms) ? (now - IN->last_move_ms) : 0;
}

bool input_next(Input *in, kian_event_t *e) {
    if (!kian_poll_event(e)) return false;
    unsigned now = now_ms();

    in->press_l = in->press_r = in->release_l = in->release_r = false;
    in->dbl_click = false;

    if (e->type == KIAN_EV_MOUSE_MOVE) {
        int dx = e->a * 2; // 2x acceleration, integer math
        int dy = e->b * 2;
        in->mx = iclamp(in->mx + dx, 0, GFX->w - 1);
        in->my = iclamp(in->my + dy, 0, GFX->h - 1);
        in->last_move_ms = now;
    } else if (e->type == KIAN_EV_MOUSE_BTN) {
        uint32_t prev = in->btns;
        uint32_t nb = (uint32_t)e->a & 7u;
        in->press_l   = ((nb & 1u) != 0) && ((prev & 1u) == 0);
        in->release_l = ((nb & 1u) == 0) && ((prev & 1u) != 0);
        in->press_r   = ((nb & 2u) != 0) && ((prev & 2u) == 0);
        in->release_r = ((nb & 2u) == 0) && ((prev & 2u) != 0);
        if (in->press_l) {
            if (now - in->last_click_ms < 400 &&
                iabs(in->mx - in->last_click_x) <= 6 &&
                iabs(in->my - in->last_click_y) <= 6) {
                in->dbl_click = true;
            }
            in->last_click_ms = now;
            in->last_click_x = in->mx;
            in->last_click_y = in->my;
            in->dragging = true;
            in->drag_x0 = in->mx;
            in->drag_y0 = in->my;
        }
        if (in->release_l) in->dragging = false;
        in->btns = nb;
    } else if (e->type == KIAN_EV_MOUSE_WHEEL) {
        if (e->a > 0) e->a = 1;
        else if (e->a < 0) e->a = -1;
    }
    return true;
}

void input_init(Input *in) {
    in->mx = GFX->w / 2;
    in->my = GFX->h / 2;
    in->btns = 0;
    in->press_l = in->press_r = in->release_l = in->release_r = false;
    in->dbl_click = false;
    in->dragging = false;
    in->drag_x0 = in->drag_y0 = 0;
    in->last_move_ms = 0;
    in->last_click_ms = 0;
    in->last_click_x = in->last_click_y = 0;
    IN = in;
}

// ---- cursor ----

static int cur_x = -100, cur_y = -100; // last stamped position

static const char *CURSOR[] = {
    "X...........",
    "XX..........",
    "XOX.........",
    "XOOX........",
    "XOOOX.......",
    "XOOOOX......",
    "XOOOOOX.....",
    "XOOOOOOX....",
    "XOOOOOOOX...",
    "XOOOOOOOOX..",
    "XOOOOOXXXXX.",
    "XOXOOX......",
    "XOX.XOOX....",
    "XX..XOOX....",
    "....XOOX....",
    ".....XOOX...",
    ".....XOOX...",
    "......XOX...",
    "......XOX..."
};
static const int CUR_W = 12, CUR_H = 19;

void cursor_stamp() {
    if (!GFX || !IN) return;
    cur_x = IN->mx;
    cur_y = IN->my;
    for (int ry = 0; ry < CUR_H; ry++) {
        const char *row = CURSOR[ry];
        for (int rx = 0; rx < CUR_W; rx++) {
            char ch = row[rx];
            if (ch == 'X') gp(cur_x + rx, cur_y + ry, 0x00000000);
            else if (ch == 'O') gp(cur_x + rx, cur_y + ry, 0x00FFFFFF);
        }
    }
}

void cursor_rect(int *x, int *y, int *w, int *h) {
    *x = cur_x; *y = cur_y; *w = CUR_W; *h = CUR_H;
}

void cursor_present() {
    if (!GFX) return;
    cursor_stamp();
    present(cur_x, cur_y, CUR_W, CUR_H);
}

void cursor_move_redraw() {
    if (!GFX) return;
    if (cur_x >= 0) {
        scene_copy(cur_x, cur_y, CUR_W, CUR_H);
        present(cur_x, cur_y, CUR_W, CUR_H);
    }
    cursor_present();
}

void cursor_reset_anchor() {
    if (!IN) return;
    cur_x = IN->mx;
    cur_y = IN->my;
}
