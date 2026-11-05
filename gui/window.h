// Kian OS - GUI window header (planned)
#ifndef KIAN_GUI_WINDOW_H
#define KIAN_GUI_WINDOW_H
#include <stdint.h>
typedef struct KianWindow KianWindow;
KianWindow *window_create(int x, int y, int w, int h, const char *title);
void window_show(KianWindow *w);
void window_set_title(KianWindow *w, const char *title);
void window_close(KianWindow *w);
#endif
