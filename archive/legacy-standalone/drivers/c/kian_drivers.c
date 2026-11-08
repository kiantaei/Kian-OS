// Kian OS - C low-level drivers / hardware glue
// VGA text mode, PS/2 keyboard, serial port (COM1), RTC, PIT
#include <stdint.h>
#include <stddef.h>

static uint16_t *vga = (uint16_t *)0xB8000;
static size_t vga_col = 0, vga_row = 0;

enum vga_color {
    VGA_BLACK=0, VGA_BLUE=1, VGA_GREEN=2, VGA_CYAN=3,
    VGA_RED=4, VGA_MAGENTA=5, VGA_BROWN=6, VGA_LIGHT_GRAY=7,
    VGA_DARK_GRAY=8, VGA_LIGHT_BLUE=9, VGA_LIGHT_GREEN=10,
    VGA_LIGHT_CYAN=11, VGA_LIGHT_RED=12, VGA_LIGHT_MAGENTA=13,
    VGA_YELLOW=14, VGA_WHITE=15,
};
static uint8_t make_color(enum vga_color fg, enum vga_color bg) {
    return fg | (bg << 4);
}
static uint16_t vga_entry(unsigned char c, uint8_t color) {
    return (uint16_t)c | ((uint16_t)color << 8);
}

void kian_vga_init(void) {
    for (size_t y = 0; y < 25; y++)
        for (size_t x = 0; x < 80; x++)
            vga[y*80 + x] = vga_entry(' ', make_color(VGA_LIGHT_GRAY, VGA_BLACK));
    vga_col = 0; vga_row = 0;
}

void kian_vga_putchar(char c) {
    if (c == '\n') { vga_col = 0; vga_row++; }
    else {
        vga[vga_row*80 + vga_col] = vga_entry(c, make_color(VGA_LIGHT_GRAY, VGA_BLACK));
        vga_col++;
    }
    if (vga_col >= 80) { vga_col = 0; vga_row++; }
    if (vga_row >= 25) {
        for (size_t y = 0; y < 24; y++)
            for (size_t x = 0; x < 80; x++)
                vga[y*80+x] = vga[(y+1)*80+x];
        for (size_t x = 0; x < 80; x++) vga[24*80+x] = vga_entry(' ', make_color(VGA_LIGHT_GRAY, VGA_BLACK));
        vga_row = 24;
    }
}

void kian_vga_puts(const char *s) { while (*s) kian_vga_putchar(*s++); }

/* Serial COM1 */
static void outb(uint16_t port, uint8_t val) { asm volatile("outb %0, %1" : : "a"(val), "d"(port)); }
static uint8_t inb(uint16_t port) { uint8_t v; asm volatile("inb %1, %0" : "=a"(v) : "d"(port)); return v; }

void kian_serial_init(void) {
    outb(0x3F8, 0x80); outb(0x3F8, 0x01); outb(0x3F8, 0x00);
    outb(0x3F8, 0x03); outb(0x3F9, 0x00);
}
void kian_serial_putchar(char c) {
    while (!(inb(0x3FD) & 0x20)) {}
    outb(0x3F8, c);
}
void kian_serial_puts(const char *s) { while (*s) kian_serial_putchar(*s++); }

/* RTC read (BCD) */
uint8_t kian_cmos_read(uint8_t reg) {
    outb(0x70, reg | 0x80);
    (void)inb(0x71);
    outb(0x70, reg);
    return inb(0x71);
}
static uint8_t bcd2bin(uint8_t b) { return (b & 0x0F) + ((b >> 4) * 10); }

void kian_rtc_read(uint8_t *sec, uint8_t *min, uint8_t *hour,
                   uint8_t *day, uint8_t *month, uint8_t *year) {
    if (sec)   *sec   = bcd2bin(kian_cmos_read(0x00));
    if (min)   *min   = bcd2bin(kian_cmos_read(0x02));
    if (hour)  *hour  = bcd2bin(kian_cmos_read(0x04));
    if (day)   *day   = bcd2bin(kian_cmos_read(0x07));
    if (month) *month = bcd2bin(kian_cmos_read(0x08));
    if (year)  *year  = bcd2bin(kian_cmos_read(0x09));
}
