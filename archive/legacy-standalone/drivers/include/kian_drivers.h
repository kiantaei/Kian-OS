// Kian OS - C driver headers
#ifndef KIAN_DRIVERS_H
#define KIAN_DRIVERS_H
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void kian_vga_init(void);
void kian_vga_putchar(char c);
void kian_vga_puts(const char *s);

void kian_serial_init(void);
void kian_serial_putchar(char c);
void kian_serial_puts(const char *s);

void kian_rtc_read(uint8_t *sec, uint8_t *min, uint8_t *hour,
                   uint8_t *day, uint8_t *month, uint8_t *year);

#ifdef __cplusplus
}
#endif
#endif
