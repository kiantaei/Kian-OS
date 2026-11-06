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
typedef struct { uint32_t type; int32_t a, b, c; } kian_event_t;
enum { KIAN_EV_NONE=0, KIAN_EV_KEY_DOWN=1, KIAN_EV_KEY_UP=2, KIAN_EV_MOUSE_MOVE=3, KIAN_EV_MOUSE_BTN=4 };
/* KEY: a=scancode, b=ASCII or 0, c=modifiers (1=shift,2=ctrl,4=alt) ; MOUSE_MOVE: a=dx, b=dy ; MOUSE_BTN: a=button mask (1=L,2=R,4=M) */
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
int    kian_poll_event(kian_event_t *out);       /* 1 if an event was written. Polls ports 0x60/0x64, NO interrupts */
uint64_t kian_uptime_ms(void);
void   kian_time_now(kian_time_t *out);          /* CMOS RTC (UTC) + fixed +03:30, NO DST, with day/month rollover */
int    kian_auth_check(const char *user, const char *pass); /* 1 = ok. Compares SHA-256(salt+pass) with a stored hash. Default account kian / kian. NEVER store plaintext. */
void   kian_reboot(void);                        /* try 0x64<-0xFE, then triple fault */
void   kian_shutdown(void);                      /* try ports 0x604<-0x2000, 0xB004<-0x2000, 0x4004<-0x3400, then halt */
void   kian_halt(void);                          /* cli; loop { hlt } */
#ifdef __cplusplus
}
#endif
