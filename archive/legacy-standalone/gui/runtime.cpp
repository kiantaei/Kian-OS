// Kian OS GUI - operator new/delete routed to the Rust kernel heap,
// plus the C++ runtime hooks freestanding builds need.

#include "runtime.h"

void *operator new(size_t size) {
    void *p = kian_alloc(size, 16);
    if (p == 0) kian_halt(); // OOM: no exceptions allowed, halt loudly
    return p;
}
void *operator new[](size_t size) {
    void *p = kian_alloc(size, 16);
    if (p == 0) kian_halt();
    return p;
}
void operator delete(void *p) noexcept { kian_free(p); }
void operator delete[](void *p) noexcept { kian_free(p); }
void operator delete(void *p, size_t) noexcept { kian_free(p); }
void operator delete[](void *p, size_t) noexcept { kian_free(p); }

extern "C" void __cxa_pure_virtual() { kian_halt(); }
extern "C" int __cxa_atexit(void (*)(void *), void *, void *) { return 0; }
void *__dso_handle = (void *)&__dso_handle;
