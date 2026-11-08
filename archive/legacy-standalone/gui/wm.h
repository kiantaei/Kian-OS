// Kian OS GUI - window manager: fixed pool of 32 windows, z-order, drag,
// resize (8px grab zone), snap zones with translucent preview, decorations.
#pragma once
#include <kian_abi.h>
#include "gfx.h"

enum { WS_NORM = 0, WS_MIN = 1, WS_MAX = 2 };
const int WM_TITLE_H = 32;
const int WM_MAX_WINDOWS = 32;
const int WM_MIN_W = 240;
const int WM_MIN_H = 160;
const int WM_EDGE = 8;   // resize grab zone

// resize modes (bitfield): 1=L 2=R 4=T 8=B
enum { RS_NONE = 0, RS_L = 1, RS_R = 2, RS_T = 4, RS_B = 8 };

struct Window {
    bool used;
    int id;
    int app;                 // APP_* id
    int x, y, w, h;
    char title[32];
    int icon;
    int state;               // WS_*
    int rx, ry, rw, rh;      // restore rect (pre-maximize)
    int desk_id;             // virtual desktop 0..1
    bool dirty;              // content needs redraw
    unsigned born_ms;        // open animation start
    void *inst;
    void (*draw)(void *inst, Rect content);
    void (*event)(void *inst, const kian_event_t *e, Rect content);
    void (*destroy)(void *inst);
};

struct WM {
    Window win[WM_MAX_WINDOWS];
    int order[WM_MAX_WINDOWS];  // indices into win[], order[0]=bottom ... top
    int count;                  // number of USED entries in win[] (not order len)
    int order_n;
    int next_id;
    int focused;                // window id or 0
    int cur_desk;               // current virtual desktop (kept by the shell)
    // drag / resize / snap state
    bool dragging;
    int drag_id, drag_offx, drag_offy;
    int resize_id, resize_mode;
    int rs_x, rs_y, rs_w, rs_h;
    int snap_zone;              // 0 none, 1 left, 2 right, 3 top
    bool did_move;              // moved a window this frame (-> full redraw)
};

void wm_init(WM *wm);
Window *wm_open(WM *wm, int app, const char *title, int icon, int w, int h, int desk_id);
void wm_close_id(WM *wm, int id);
void wm_close_app(WM *wm, int app);
Window *wm_get(WM *wm, int id);
Window *wm_top_at(WM *wm, int x, int y, int cur_desk);
Window *wm_focused(WM *wm);
void wm_focus(WM *wm, Window *w);
void wm_raise(WM *wm, Window *w);
Rect wm_content(const Window *w);
void wm_draw_all(WM *wm, int cur_desk);
void wm_draw_one(WM *wm, Window *w);
bool wm_mouse(WM *wm, const kian_event_t *e);  // 1 = consumed (drag/resize/decorations)
int  wm_button_at(const Window *w, int px, int py); // 0 none 1 min 2 max 3 close
int  wm_resize_at(const Window *w, int px, int py);
Rect wm_snap_rect(int zone, int screen_w, int screen_h); // zone 1 left 2 right 3 top
void wm_toggle_max(Window *w, int screen_w, int screen_h);
void wm_minimize_all(WM *wm, int cur_desk);
void wm_restore_all(WM *wm, int cur_desk);
int  wm_count_on_desk(WM *wm, int desk);
Window *wm_topmost(WM *wm, int cur_desk);      // top visible window or 0
