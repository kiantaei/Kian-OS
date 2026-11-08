#pragma once
#include <stdint.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct { uint64_t addr; uint32_t width, height, pitch, bpp; } kian_fb_t;  /* pitch in BYTES */
typedef struct { uint64_t base, length; uint32_t usable; } kian_memregion_t;
typedef struct {
  uint32_t valid;                 /* 1 if Limine base revision supported and fb present */
  kian_fb_t fb;
  uint64_t hhdm_offset;
  uint64_t rsdp;
  uint32_t mem_count;
  kian_memregion_t mem[64];
} kian_boot_info_t;
typedef struct { int32_t type; int32_t a, b, c; } kian_event_t;
enum { KIAN_EV_NONE=0, KIAN_EV_KEY_DOWN=1, KIAN_EV_KEY_UP=2, KIAN_EV_MOUSE_MOVE=3, KIAN_EV_MOUSE_BTN=4, KIAN_EV_MOUSE_WHEEL=5 };
/* KEY_DOWN/UP: a = KIAN_KEY_* special code, or raw set-1 scancode for plain
 * ASCII keys; b = ASCII (0 for special keys); c = modifiers:
 * 1=shift, 2=ctrl, 4=alt, 8=win.
 * MOUSE_MOVE: a=dx, b=dy ; MOUSE_BTN: a=button mask (1=L,2=R,4=M) ;
 * MOUSE_WHEEL: a=+1 (wheel up) or -1 (wheel down). */
enum {
  KIAN_KEY_UP        = 0x100,
  KIAN_KEY_DOWN      = 0x101,
  KIAN_KEY_LEFT      = 0x102,
  KIAN_KEY_RIGHT     = 0x103,
  KIAN_KEY_HOME      = 0x104,
  KIAN_KEY_END       = 0x105,
  KIAN_KEY_DEL       = 0x106,
  KIAN_KEY_PGUP      = 0x107,
  KIAN_KEY_PGDN      = 0x108,
  KIAN_KEY_F1        = 0x110, /* F1..F12 = 0x110..0x11B */
  KIAN_KEY_WIN       = 0x120,
  KIAN_KEY_ESC       = 0x121, /* b is still 27 */
  KIAN_KEY_TAB       = 0x122, /* b is still 9 */
  KIAN_KEY_ENTER     = 0x123, /* b is still '\n' */
  KIAN_KEY_BACKSPACE = 0x124  /* b is still 8 */
};
typedef struct { int32_t year, month, day, hour, minute, second, weekday; } kian_time_t; /* LOCAL time Asia/Tehran */

/* ---- implemented in C (boot/) ---- */
const kian_boot_info_t *kian_boot_info(void);
void kian_run_ctors(void);                       /* runs .init_array (C++ static ctors) */
/* ---- implemented in C (console/) ---- */
void console_init(const kian_fb_t *fb);
void console_putc(char c);
void console_write(const char *s, size_t n);
void console_clear(void);
void console_set_color(uint32_t fg, uint32_t bg);
int  kprintf(const char *fmt, ...);              /* %s %c %d %u %x %lu %lx %02d %p, integers ONLY */
int  ksnprintf(char *buf, size_t n, const char *fmt, ...);
void kian_shell_main(void);                      /* login + ksh, never returns */
const uint8_t *kian_font_glyph(char c);          /* 8 bytes, 1 bit per pixel, MSB = left */
/* ---- implemented in C++ (gui/) ---- */
int  gui_main(const kian_fb_t *fb);              /* returns 0 on "Exit to console" */
/* ---- implemented in Rust (kernel/) with #[unsafe(no_mangle)] pub extern "C" ---- */
void   kian_serial_write(const char *s, size_t n);
void  *kian_alloc(size_t size, size_t align);
void   kian_free(void *p);
int    kian_poll_event(kian_event_t *out);       /* 1 if an event was written. Polls ports 0x60/0x64, NO interrupts, drains at most 32 bytes per call, NEVER blocks */
uint64_t kian_uptime_ms(void);
void   kian_time_now(kian_time_t *out);          /* CMOS RTC (UTC) + fixed +03:30, NO DST, with day/month rollover */
void   kian_mem_stats(uint64_t *used_bytes, uint64_t *total_bytes); /* walks the kernel heap, never blocks */
int    kian_auth_check(const char *user, const char *pass); /* 1 = ok. Compares SHA-256(salt+pass) with a stored hash. Default account kian / kian. NEVER store plaintext. */
void   kian_reboot(void);                        /* try 0x64<-0xFE, then triple fault */
void   kian_shutdown(void);                      /* try ports 0x604<-0x2000, 0xB004<-0x2000, 0x4004<-0x3400, then halt */
void   kian_halt(void);                          /* cli; loop { hlt } */
#ifdef __cplusplus
}
#endif
