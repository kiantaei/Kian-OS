// Kian OS GUI - code-drawn icons (no image files). Each icon is drawn inside
// a size x size box at (x, y) with the given color (monochrome silhouettes).
#pragma once
#include <kian_abi.h>

enum {
    IC_NONE = 0,
    IC_START, IC_SEARCH, IC_TASKVIEW,
    IC_BROWSER, IC_XENOX, IC_TERMINAL, IC_FILES, IC_NOTES, IC_SETTINGS, IC_TASKMGR,
    IC_CALC, IC_PAINT, IC_TRASH, IC_PC,
    IC_POWER, IC_USER, IC_WIFI, IC_SPEAKER, IC_BATTERY,
    IC_CHEVUP, IC_SYNC, IC_BACK, IC_UPDIR,
    IC_CLOSE, IC_MIN, IC_MAX, IC_RESTORE, IC_PLUS, IC_CHECK,
    IC_BOOKMARK, IC_GRID,
    IC_FORWARD, IC_RELOAD, IC_HOME, IC_LOCK, IC_DOTS, IC_CLOCK, IC_DOWNLOAD, IC_HISTORY
};

void icon_draw(int id, int x, int y, int size, uint32_t color);
