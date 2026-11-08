// Kian OS GUI - freestanding C++ runtime support.
#pragma once
#include <kian_abi.h>

// placement new (we never throw)
inline void *operator new(size_t, void *p) noexcept { return p; }
