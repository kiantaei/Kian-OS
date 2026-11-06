// Kian OS GUI - tiny freestanding containers (NO STL, POD types only).
#pragma once
#include <kian_abi.h>

extern "C" void *memcpy(void *dst, const void *src, size_t n);
extern "C" void *memset(void *dst, int c, size_t n);
extern "C" size_t strlen(const char *s);

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
    void backspace() { if (n > 0) { n--; d[n] = '\0'; } }
    void clear() { n = 0; if (d) d[0] = '\0'; }
    const char *c_str() { if (d == 0) { reserve(1); d[0] = '\0'; } return d; }
    int length() const { return n; }
};
