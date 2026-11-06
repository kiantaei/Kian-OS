// Kian OS GUI - tiny freestanding containers + integer math helpers.
// NO STL, NO exceptions, NO float/double. Heap = kian_alloc/kian_free.
#pragma once
#include <kian_abi.h>

extern "C" void *memcpy(void *dst, const void *src, size_t n);
extern "C" void *memset(void *dst, int c, size_t n);
extern "C" size_t strlen(const char *s);
extern "C" int strcmp(const char *a, const char *b);
extern "C" int strncmp(const char *a, const char *b, size_t n);
extern "C" char *strcpy(char *dst, const char *src);

inline int imin(int a, int b) { return a < b ? a : b; }
inline int imax(int a, int b) { return a > b ? a : b; }
inline int iclamp(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }
inline int iabs(int v) { return v < 0 ? -v : v; }

// ---- 16.16 fixed point (integer only) ----
inline int fx_one() { return 1 << 16; }
inline int32_t fx_from_int(int32_t v) { return v << 16; }
inline int32_t fx_mul(int32_t a, int32_t b) { return (int32_t)(((int64_t)a * (int64_t)b) >> 16); }
inline int32_t fx_div(int32_t a, int32_t b) { return (int32_t)(((int64_t)a << 16) / b); }

// ease-out curve, t in 0..65536 -> result in 0..65536, integer only
inline int fx_ease_out(int t) {
    t = iclamp(t, 0, 1 << 16);
    int inv = (1 << 16) - t;
    return (1 << 16) - (int)(((int64_t)inv * inv) >> 16);
}

inline unsigned now_ms() { return (unsigned)kian_uptime_ms(); }

// ---- small number -> string helpers (shared, inline) ----
inline void put_num(char *b, int &i, int v) {
    if (v < 0) { b[i++] = '-'; v = -v; }
    char tmp[8];
    int n = 0;
    if (v == 0) tmp[n++] = '0';
    while (v > 0 && n < 7) { tmp[n++] = (char)('0' + v % 10); v /= 10; }
    while (n > 0) b[i++] = tmp[--n];
    b[i] = '\0';
}

inline void put_pad2(char *b, int &i, int v) {
    if (v >= 0 && v < 10) b[i++] = '0';
    put_num(b, i, v);
}

template <class T>
struct Vec {
    T *data;
    int n;
    int cap;

    Vec() : data(0), n(0), cap(0) {}
    ~Vec() { if (data) kian_free(data); }

    bool reserve(int want) {
        if (want <= cap) return true;
        int nc = (cap > 0) ? cap * 2 : 8;
        while (nc < want) nc *= 2;
        T *nd = (T *)kian_alloc((size_t)nc * sizeof(T), 16);
        if (nd == 0) return false;
        if (data != 0 && n > 0) memcpy(nd, data, (size_t)n * sizeof(T));
        if (data) kian_free(data);
        data = nd;
        cap = nc;
        return true;
    }
    bool push(const T &v) {
        if (!reserve(n + 1)) return false;
        data[n++] = v;
        return true;
    }
    void remove_at(int i) {
        for (int j = i + 1; j < n; j++) data[j - 1] = data[j];
        n--;
    }
    T &operator[](int i) { return data[i]; }
    int size() const { return n; }
    void clear() { n = 0; }
};

// Small growable string (no exceptions, no allocator tricks).
struct Str {
    char *d;
    int n;
    int cap;

    Str() : d(0), n(0), cap(0) {}
    ~Str() { if (d) kian_free(d); }

    bool reserve(int want) {
        if (want <= cap) return true;
        int nc = (cap > 0) ? cap * 2 : 32;
        while (nc < want) nc *= 2;
        char *nd = (char *)kian_alloc((size_t)nc, 16);
        if (nd == 0) return false;
        if (d != 0 && n > 0) memcpy(nd, d, (size_t)n);
        if (d) kian_free(d);
        d = nd;
        cap = nc;
        return true;
    }
    void append(char c) {
        if (!reserve(n + 2)) return;
        d[n++] = c;
        d[n] = '\0';
    }
    void append(const char *s) {
        while (*s != '\0') append(*s++);
    }
    void append(const Str &s) { append(s.d ? s.d : ""); }
    void append(int v) { // decimal integer
        char tmp[16];
        int i = 0;
        unsigned m = (v < 0) ? (unsigned)(-(long long)v) : (unsigned)v;
        if (v < 0) append('-');
        if (m == 0) tmp[i++] = '0';
        while (m > 0 && i < 15) { tmp[i++] = (char)('0' + (m % 10)); m /= 10; }
        while (i > 0) append(tmp[--i]);
    }
    void insert(int idx, char c) {
        if (idx < 0 || idx > n) return;
        if (!reserve(n + 2)) return;
        for (int j = n; j > idx; j--) d[j] = d[j - 1];
        d[idx] = c;
        n++;
        d[n] = '\0';
    }
    void remove_at(int idx) {
        if (idx < 0 || idx >= n) return;
        for (int j = idx; j < n - 1; j++) d[j] = d[j + 1];
        n--;
        if (d) d[n] = '\0';
    }
    void backspace() { if (n > 0) { n--; d[n] = '\0'; } }
    void clear() { n = 0; if (d) d[0] = '\0'; }
    void set(const char *s) { clear(); append(s); }
    const char *c_str() { if (d == 0) { reserve(1); d[0] = '\0'; } return d; }
    int length() const { return n; }
    char operator[](int i) const { return (i >= 0 && i < n && d) ? d[i] : '\0'; }
    int find(char c) const {
        for (int i = 0; i < n; i++) if (d && d[i] == c) return i;
        return -1;
    }
    int find_last(char c) const {
        for (int i = n - 1; i >= 0; i--) if (d && d[i] == c) return i;
        return -1;
    }
    bool starts_with(const char *p) const {
        for (int i = 0; p[i] != '\0'; i++) {
            if (i >= n || d[i] != p[i]) return false;
        }
        return true;
    }
};
