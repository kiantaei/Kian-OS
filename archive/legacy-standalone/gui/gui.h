// Kian OS GUI - the Shell struct ties all subsystems together. One instance
// is created inside gui_main() with new (no global constructors).
#pragma once
#include <kian_abi.h>
#include "gfx.h"
#include "theme.h"
#include "input.h"
#include "widgets.h"
#include "wm.h"
#include "desktop.h"

struct Shell {
    Theme theme;
    Input input;
    WM wm;
    Desktop desk;
    // modal overlays
    Menu menu;              // the ONE active context menu
    bool ctrldel;           // Ctrl+Alt+Del screen
    bool run_open;          // Win+R dialog
    TBox run_box;
    bool alttab;            // Alt+Tab switcher overlay
    int alttab_sel;
    // loop state
    bool running;
    bool need_full;
    bool taskbar_dirty;
    unsigned clock_second;
    bool lock_pending;      // request to show the lock screen
};

extern Shell *SH;

// login/lock screen (implemented in login.cpp)
bool login_run(bool require);   // blocking; true = signed in (require=false returns true at once)
void login_lock();              // blocking lock screen; unlocks with the kian password
