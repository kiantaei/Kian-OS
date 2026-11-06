// Kian OS GUI - window manager (drag, raise, min/max/close, decorations).
#pragma once
#include <kian_abi.h>
#include "gfx.h"
#include "vec.h"

struct Theme; // defined in desktop.h

enum {
    APP_TERM = 1, APP_FILES = 2, APP_NOTES = 3,
    APP_SETTINGS = 4, APP_CALC = 5, APP_ABOUT = 6
};

const int TITLE_H = 18;   // title bar height
const int TB_H = 28;      // taskbar height

struct Window {
    int id;
    int type;
    int x, y, w, h;
    char title[28];
    bool minimized;
    bool maximized;
    int prev_x, prev_y, prev_w, prev_h;
};

enum { BTN_NONE = 0, BTN_CLOSE = 1, BTN_MAX = 2, BTN_MIN = 3 };

class WM {
public:
    Vec<Window> wins;
    int next_id;

    WM() : next_id(1) {}

    int open(int type, const char *title, int w, int h, int screen_w);
    Window *find(int id);
    Window *find_type(int type);
    Window &at(int i) { return wins[i]; }
    int count() const { return wins.size(); }
    void close(int id);
    void raise(int id);
    void restore(Window *w);
    Window *topmost_at(int x, int y);
    Rect content_rect(const Window &w, int screen_w, int screen_h) const;
    int button_at(const Window &w, int px, int py) const;
    bool title_hit(const Window &w, int px, int py) const;
    void draw(Surface &s, const Theme &th, int screen_w, int screen_h);
    void draw_one(Surface &s, const Theme &th, int idx, int screen_w, int screen_h);
};
