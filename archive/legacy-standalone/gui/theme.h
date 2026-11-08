// Kian OS GUI - theme (dark default, light switchable, one accent color).
#pragma once
#include <kian_abi.h>

struct Theme {
    uint32_t wallpaper_top;
    uint32_t wallpaper_bottom;
    uint32_t taskbar_bg;      // acrylic base color
    uint32_t taskbar_fg;
    uint32_t start_bg;        // panels
    uint32_t start_bg_active; // hover / active
    uint32_t menu_bg;
    uint32_t menu_border;
    uint32_t win_frame;
    uint32_t win_bg;
    uint32_t title_bg;
    uint32_t title_text;
    uint32_t text;
    uint32_t text_dim;
    uint32_t button_bg;
    uint32_t button_fg;
    uint32_t close_btn_bg;
    uint32_t shadow;
    uint32_t icon_bg;
    uint32_t input_bg;
    uint32_t select_bg;       // list selection
    uint32_t accent;          // default blue 0x0078D4
};

extern Theme *THEME;          // points into the Shell (set by gui_main)

void theme_set(Theme *t, bool dark, uint32_t accent);   // implemented in desktop.cpp
