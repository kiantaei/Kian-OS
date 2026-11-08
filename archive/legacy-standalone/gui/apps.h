// Kian OS GUI - app registry + shell services exposed to apps.
#pragma once
#include <kian_abi.h>
#include "gfx.h"

enum {
    APP_NONE = 0,
    APP_XENOX, APP_TERM, APP_FILES, APP_NOTES,
    APP_SETTINGS, APP_TASKMGR, APP_CALC, APP_PAINT
};

// defined in gui.cpp
void app_open(int app);                          // create window + instance
void app_open_path(int app, const char *path);   // e.g. notes with a ramfs path
void shell_full_redraw();                        // request a full repaint
void shell_mark_content();                       // mark focused window content dirty
void shell_close_focused();                      // the focused window asks to close itself
void shell_toast(const char *text);              // show a notification toast
void shell_set_title(const char *text);          // rename the focused window (browser tabs)

// uniform per-app interface (Rect = content rect). One struct per app file.
void *term_create();
void   term_destroy(void *inst);
void   term_draw(void *inst, Rect r);
void   term_event(void *inst, const kian_event_t *e, Rect r);

void *files_create();
void   files_destroy(void *inst);
void   files_draw(void *inst, Rect r);
void   files_event(void *inst, const kian_event_t *e, Rect r);

void *notes_create();
void *notes_create_path(const char *ramfs_path);
void   notes_destroy(void *inst);
void   notes_draw(void *inst, Rect r);
void   notes_event(void *inst, const kian_event_t *e, Rect r);

void *setapp_create();
void   setapp_destroy(void *inst);
void   setapp_draw(void *inst, Rect r);
void   setapp_event(void *inst, const kian_event_t *e, Rect r);

void *taskmgr_create();
void   taskmgr_destroy(void *inst);
void   taskmgr_draw(void *inst, Rect r);
void   taskmgr_event(void *inst, const kian_event_t *e, Rect r);

void *calc_create();
void   calc_destroy(void *inst);
void   calc_draw(void *inst, Rect r);
void   calc_event(void *inst, const kian_event_t *e, Rect r);

void *paint_create();
void   paint_destroy(void *inst);
void   paint_draw(void *inst, Rect r);
void   paint_event(void *inst, const kian_event_t *e, Rect r);

void *xenox_create();
void   xenox_destroy(void *inst);
void   xenox_draw(void *inst, Rect r);
void   xenox_event(void *inst, const kian_event_t *e, Rect r);
