// Kian OS - GUI/display abstraction headers (planned)
#ifndef KIAN_GUI_DISPLAY_H
#define KIAN_GUI_DISPLAY_H
#include <stdint.h> /* uint32_t - do not rely on include order */
#ifdef __cplusplus
extern "C" {
#endif
void display_init(void);
void display_putpixel(int x, int y, uint32_t color);
void display_draw_rect(int x, int y, int w, int h, uint32_t color);
void display_flip(void);
#ifdef __cplusplus
}
#endif
#endif
