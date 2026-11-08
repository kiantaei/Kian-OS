// Kian OS GUI - input: event pump, mouse position/edges, double-click,
// drag tracking, and the software cursor with the scene-restore speed trick.
#pragma once
#include <kian_abi.h>

struct Input {
    int mx, my;             // cursor position (hotspot = tip)
    uint32_t btns;          // 1=L 2=R 4=M
    // edges valid for the CURRENT event only
    bool press_l, press_r, release_l, release_r;
    bool dbl_click;         // set on L press < 400 ms / <= 6 px
    bool dragging;          // L held
    int drag_x0, drag_y0;   // press position
    unsigned last_move_ms;
    unsigned last_click_ms;
    int last_click_x, last_click_y;
};

extern Input *IN;           // set by input_init

void input_init(Input *in);
bool input_next(Input *in, kian_event_t *e); // false = no more events this pump
bool mouse_in(int x, int y, int w, int h);
unsigned mouse_idle_ms();                    // ms since the mouse last moved

// ---- cursor (12x19 arrow, drawn in code) ----
void cursor_stamp();        // draw cursor into frame at IN->mx/my
void cursor_rect(int *x, int *y, int *w, int *h);
void cursor_present();      // stamp + present cursor rect
void cursor_move_redraw();  // erase old spot from scene, stamp + present new
void cursor_reset_anchor(); // after a full redraw, cursor is already stamped
