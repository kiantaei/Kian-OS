// Kian OS GUI - Xenox Browser (internal contract).
//
// Xenox is Kian OS's first-class browser with a Chromium-inspired interface.
// HONEST ENGINE STATUS (see also gui/apps/xenox.cpp and README.md):
//   - Interface: Chromium/Chrome-inspired, drawn natively by Kian OS.
//   - Rendering: a small NATIVE page renderer for built-in local pages.
//   - Chromium engine: NOT ported. A real Chromium port needs memory
//     mapping, threads, synchronization, TCP/IP, TLS, a real filesystem
//     and a hosted C++ runtime - none of which the Kian kernel provides
//     yet. Xenox never fakes remote websites and never invents network
//     APIs that the kernel does not have.
#pragma once
#include <kian_abi.h>
#include "../gfx.h"

// Built-in local pages Xenox can actually render.
enum XenoPage {
    XP_START = 0,   // start page: logo, quick links, recent history, bookmarks
    XP_BOOKMARKS,   // bookmark list (RAM-backed, lost on reboot - stated in UI)
    XP_HISTORY,     // real browsing history of this session
    XP_DOWNLOADS,   // honest empty downloads page
    XP_ABOUT,       // about Xenox + engine status
    XP_ERROR        // "this page can't be shown" (no network / bad URL)
};

const int XENOX_URL_MAX = 112;   // bound of one URL string
const int XENOX_TABS    = 8;     // bounded tab pool
