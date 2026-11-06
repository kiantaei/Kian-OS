/* Kian OS - minimal freestanding printf (integers only, NO float/double).
 * Supported: %s %c %d %i %u %x %X %p %%, flags '-' and '0', width digits,
 * length modifiers l / ll / hh / h. */

#include <kian_abi.h>
#include <stdarg.h>

struct outbuf {
    char *b;
    size_t cap;
    size_t n; /* total length written (may exceed cap) */
};

static void ob_putc(struct outbuf *o, char c) {
    if (o->cap > 0 && o->n + 1 < o->cap) {
        o->b[o->n] = c;
    }
    o->n++;
}

static void ob_puts(struct outbuf *o, const char *s, size_t n) {
    for (size_t i = 0; i < n; i++) ob_putc(o, s[i]);
}

/* width = minimum field width; zero = pad with '0'; minus = left align */
static void ob_num(struct outbuf *o, uint64_t v, int neg, unsigned base,
                   int upper, int width, int zero, int minus) {
    char tmp[24];
    const char *hex = upper ? "0123456789ABCDEF" : "0123456789abcdef";
    int len = 0;
    do {
        tmp[len++] = hex[v % base];
        v /= base;
    } while (v != 0);
    if (neg) len++; /* room for '-' */
    int pad = (width > len) ? width - len : 0;

    if (!minus && pad > 0) {
        char p = zero ? '0' : ' ';
        for (int i = 0; i < pad; i++) {
            if (zero) ob_putc(o, p);
            else ob_putc(o, ' ');
        }
    }
    if (neg) ob_putc(o, '-');
    if (minus) {
        /* digits first, trailing spaces */
        for (int i = len - neg; i > 0; i--) ob_putc(o, tmp[i - 1]);
        for (int i = 0; i < pad; i++) ob_putc(o, ' ');
        return;
    }
    for (int i = len - neg; i > 0; i--) ob_putc(o, tmp[i - 1]);
}

static int kvsnprintf_impl(char *buf, size_t n, const char *fmt, va_list ap) {
    struct outbuf o = { buf, n, 0 };
    for (const char *p = fmt; *p != '\0'; p++) {
        if (*p != '%') {
            ob_putc(&o, *p);
            continue;
        }
        p++;
        /* flags */
        int minus = 0, zero = 0;
        for (;; p++) {
            if (*p == '-') minus = 1;
            else if (*p == '0') zero = 1;
            else break;
        }
        /* width */
        int width = 0;
        while (*p >= '0' && *p <= '9') {
            width = width * 10 + (*p - '0');
            p++;
        }
        /* length modifier */
        int lcount = 0, hcount = 0;
        while (*p == 'l') { lcount++; p++; }
        while (*p == 'h') { hcount++; p++; }

        char c = *p;
        switch (c) {
        case '%':
            ob_putc(&o, '%');
            break;
        case 'c': {
            char ch = (char)va_arg(ap, int);
            if (width > 1 && !minus) {
                for (int i = 1; i < width; i++) ob_putc(&o, ' ');
            }
            ob_putc(&o, ch);
            if (minus) {
                for (int i = 1; i < width; i++) ob_putc(&o, ' ');
            }
            break;
        }
        case 's': {
            const char *s = va_arg(ap, const char *);
            if (s == 0) s = "(null)";
            size_t len = 0;
            while (s[len] != '\0') len++;
            if (!minus && (int)len < width) {
                for (int i = (int)len; i < width; i++) ob_putc(&o, ' ');
            }
            ob_puts(&o, s, len);
            if (minus && (int)len < width) {
                for (int i = (int)len; i < width; i++) ob_putc(&o, ' ');
            }
            break;
        }
        case 'd':
        case 'i': {
            int64_t v;
            if (lcount >= 1) v = va_arg(ap, long);
            else if (hcount >= 2) v = (signed char)va_arg(ap, int);
            else if (hcount == 1) v = (short)va_arg(ap, int);
            else v = va_arg(ap, int);
            int neg = (v < 0);
            uint64_t m = neg ? (uint64_t)(-(v + 1)) + 1 : (uint64_t)v;
            ob_num(&o, m, neg, 10, 0, width, zero, minus);
            break;
        }
        case 'u': {
            uint64_t v;
            if (lcount >= 1) v = va_arg(ap, unsigned long);
            else if (hcount >= 2) v = (uint64_t)(unsigned char)va_arg(ap, int);
            else if (hcount == 1) v = (uint64_t)(unsigned short)va_arg(ap, int);
            else v = (uint64_t)va_arg(ap, unsigned int);
            ob_num(&o, v, 0, 10, 0, width, zero, minus);
            break;
        }
        case 'x':
        case 'X': {
            uint64_t v;
            if (lcount >= 1) v = va_arg(ap, unsigned long);
            else if (hcount >= 2) v = (uint64_t)(unsigned char)va_arg(ap, int);
            else if (hcount == 1) v = (uint64_t)(unsigned short)va_arg(ap, int);
            else v = (uint64_t)va_arg(ap, unsigned int);
            ob_num(&o, v, 0, 16, c == 'X', width, zero, minus);
            break;
        }
        case 'p': {
            void *pv = va_arg(ap, void *);
            ob_putc(&o, '0');
            ob_putc(&o, 'x');
            ob_num(&o, (uint64_t)(uintptr_t)pv, 0, 16, 0, 0, 0, 0);
            break;
        }
        case '\0':
            return (int)o.n;
        default:
            ob_putc(&o, '%');
            ob_putc(&o, c);
            break;
        }
    }
    if (o.cap > 0) {
        size_t end = (o.n < o.cap - 1) ? o.n : o.cap - 1;
        buf[end] = '\0';
    }
    return (int)o.n;
}

int ksnprintf(char *buf, size_t n, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int r = kvsnprintf_impl(buf, n, fmt, ap);
    va_end(ap);
    return r;
}

int kprintf(const char *fmt, ...) {
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    int r = kvsnprintf_impl(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    size_t out = (size_t)r;
    if (out >= sizeof(buf)) out = sizeof(buf) - 1;
    console_write(buf, out);
    return r;
}
