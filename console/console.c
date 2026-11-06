/* Kian OS - C framebuffer text console.
 * Draws the 8x8 font scaled 2x into the Limine framebuffer (16x16 cells).
 * If the framebuffer bpp != 32, output goes to serial only. */

#include <kian_abi.h>

/* minimal libc (console/libc_min.c) */
extern void *memcpy(void *dst, const void *src, size_t n);
extern void *memset(void *dst, int c, size_t n);
extern void *memmove(void *dst, const void *src, size_t n);

static kian_fb_t FB;
static int READY = 0;       /* 1 = we can draw (bpp == 32) */
static int COLS, ROWS;      /* console size in 16x16 cells */
static int CX, CY;          /* cursor cell */
static uint32_t FG = 0x00FFFFFF;
static uint32_t BG = 0x001A2B4A; /* dark blue */

static inline volatile uint32_t *px_at(int x, int y) {
    uint8_t *row = (uint8_t *)(uintptr_t)FB.addr + (uintptr_t)y * (uintptr_t)FB.pitch;
    return (volatile uint32_t *)(void *)row + x;
}

static void fill_rect(int x, int y, int w, int h, uint32_t color) {
    if (!READY) return;
    for (int ry = y; ry < y + h && ry < (int)FB.height; ry++) {
        for (int rx = x; rx < x + w && rx < (int)FB.width; rx++) {
            *px_at(rx, ry) = color;
        }
    }
}

static void draw_char(int cx, int cy, char c) {
    const uint8_t *g = kian_font_glyph(c);
    int ox = cx * 16, oy = cy * 16;
    for (int row = 0; row < 8; row++) {
        uint8_t bits = g[row];
        for (int col = 0; col < 8; col++) {
            uint32_t color = (bits & (0x80u >> col)) ? FG : BG;
            /* 2x scale: each font pixel becomes a 2x2 block */
            *px_at(ox + col * 2,     oy + row * 2)     = color;
            *px_at(ox + col * 2 + 1, oy + row * 2)     = color;
            *px_at(ox + col * 2,     oy + row * 2 + 1) = color;
            *px_at(ox + col * 2 + 1, oy + row * 2 + 1) = color;
        }
    }
}

static void scroll_up(void) {
    /* move all rows up by one text line (16 pixel rows) */
    uint8_t *base = (uint8_t *)(uintptr_t)FB.addr;
    memmove(base, base + (uintptr_t)FB.pitch * 16,
            (size_t)FB.pitch * (size_t)(ROWS * 16 - 16));
    fill_rect(0, (ROWS - 1) * 16, (int)FB.width, 16, BG);
}

void console_init(const kian_fb_t *fb) {
    FB = *fb;
    READY = (FB.bpp == 32);
    COLS = (int)(FB.width / 16);
    ROWS = (int)(FB.height / 16);
    CX = 0;
    CY = 0;
    if (READY) {
        console_clear();
    }
}

void console_set_color(uint32_t fg, uint32_t bg) {
    FG = fg;
    BG = bg;
    /* repaint existing cells in the new colors */
    if (READY) {
        for (int y = 0; y < ROWS; y++) {
            for (int x = 0; x < COLS; x++) {
                draw_char(x, y, ' ');
            }
        }
    }
}

void console_clear(void) {
    CX = 0;
    CY = 0;
    if (READY) {
        fill_rect(0, 0, (int)FB.width, (int)FB.height, BG);
    }
}

void console_putc(char c) {
    if (!READY) {
        kian_serial_write(&c, 1);
        return;
    }
    switch (c) {
    case '\n':
        CX = 0;
        CY++;
        break;
    case '\r':
        CX = 0;
        break;
    case '\b':
        if (CX > 0) {
            CX--;
        } else if (CY > 0) {
            CY--;
            CX = COLS - 1;
        }
        draw_char(CX, CY, ' ');
        break;
    case '\t':
        do {
            console_putc(' ');
        } while (CX % 4 != 0);
        return;
    default:
        draw_char(CX, CY, c);
        CX++;
        if (CX >= COLS) {
            CX = 0;
            CY++;
        }
        break;
    }
    if (CY >= ROWS) {
        scroll_up();
        CY = ROWS - 1;
    }
}

void console_write(const char *s, size_t n) {
    if (!READY) {
        kian_serial_write(s, n);
        return;
    }
    for (size_t i = 0; i < n; i++) {
        console_putc(s[i]);
    }
}
