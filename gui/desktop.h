// Kian OS GUI - desktop: themes, wallpaper, taskbar, start menu, clock.
#pragma once
#include <kian_abi.h>
#include "gfx.h"

class WM; // defined in wm.h

struct Theme {
    uint32_t wallpaper_top;    // gradient top (dark blue)
    uint32_t wallpaper_bottom; // gradient bottom (purple)
    uint32_t taskbar_bg;
    uint32_t taskbar_fg;
    uint32_t start_bg;
    uint32_t start_bg_active;
    uint32_t menu_bg;
    uint32_t menu_border;
    uint32_t win_frame;
    uint32_t win_bg;           // content background
    uint32_t title_bg;
    uint32_t title_text;
    uint32_t text;             // generic foreground text
    uint32_t button_bg;
    uint32_t button_fg;
    uint32_t close_btn_bg;
    uint32_t shadow;
    uint32_t icon_bg;
    uint32_t underline;        // running-window underline
};

void theme_dark(Theme &t);
void theme_light(Theme &t);

void draw_wallpaper(Surface &s, const Theme &th);

void draw_taskbar(Surface &s, const Theme &th, WM &wm, bool start_open);

void draw_start_menu(Surface &s, const Theme &th, int hover_item);

// pinned taskbar icons: Terminal, Files, Notes, Settings
const int PIN_COUNT = 4;
extern const int PIN_TYPE[PIN_COUNT];

Rect taskbar_icon_rect(int i);
Rect start_button_rect();
Rect menu_panel_rect();
Rect menu_item_rect(int i);
int  start_menu_item_at(int x, int y); // -1 = none
