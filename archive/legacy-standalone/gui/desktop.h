// Kian OS GUI - desktop shell: wallpaper, desktop icons, 48px Win11-style
// taskbar (start/search/task view/pins/tray/two-line clock), start menu,
// quick settings, calendar flyout, toasts, task view, virtual desktops.
#pragma once
#include <kian_abi.h>
#include "gfx.h"
#include "theme.h"
#include "widgets.h"
#include "wm.h"

struct Desktop {
    WM *wm;
    int wallpaper;            // 0..2
    int cur_desk;             // virtual desktop 0..1
    int sel_icon;             // selected desktop icon, -1 none
    bool taskview;
    // start menu
    bool start_open;
    unsigned start_ms;
    TBox sbox;
    // quick settings
    bool quick_open;
    bool wifi, bt, night, saver;
    int brightness;           // 0..100 (overlay alpha on shell)
    int volume;               // 0..100 (visual only)
    Slider sld_bright;
    Slider sld_vol;
    bool lang_fa;             // tray label ENG/FA (label only)
    bool clock24;
    // calendar flyout
    bool cal_open;
    // toast
    bool toast_on;
    unsigned toast_ms;
    char toast_text[64];
    // taskbar tooltip tracking
    int hover_btn;
    unsigned hover_since;
    bool tooltip_on;
    // emergency exit (Start menu "Exit to console")
    bool exit_gui;
};

void desk_init(Desktop *d, WM *wm);
void theme_set(Theme *t, bool dark, uint32_t accent);   // dark/light + accent

void desk_draw_wallpaper(Desktop *d);
void desk_draw_icons(Desktop *d);
bool desk_event(Desktop *d, const kian_event_t *e);     // wallpaper/icon clicks

void taskbar_draw(Desktop *d);
bool taskbar_event(Desktop *d, const kian_event_t *e);

void start_menu_toggle(Desktop *d);
void start_menu_draw(Desktop *d);
bool start_menu_event(Desktop *d, const kian_event_t *e);

void quick_draw(Desktop *d);
bool quick_event(Desktop *d, const kian_event_t *e);

void cal_draw(Desktop *d);
bool cal_event(Desktop *d, const kian_event_t *e);

void toast_show(Desktop *d, const char *text);
void toast_draw(Desktop *d);
void toast_tick(Desktop *d);
Rect toast_region(Desktop *d);

void taskview_draw(Desktop *d);
bool taskview_event(Desktop *d, const kian_event_t *e);
void desk_switch(Desktop *d, int delta);                 // Ctrl+Win+Left/Right

int  days_from_civil(int y, int m, int d);               // integer weekday helper
