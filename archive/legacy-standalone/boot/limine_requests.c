/* Kian OS - boot glue: official Limine requests (limine.h from build/limine,
 * the v8.x-binary checkout that tools/make_iso.sh clones). */

#include <limine.h>
#include <kian_abi.h>

/* ---- request region markers (exact names/sections from limine.h) ---- */

__attribute__((used, section(".limine_requests_start")))
static volatile LIMINE_REQUESTS_START_MARKER;

__attribute__((used, section(".limine_requests_end")))
static volatile LIMINE_REQUESTS_END_MARKER;

__attribute__((used, section(".limine_requests")))
static volatile LIMINE_BASE_REVISION(3);

/* ---- requests (official struct + id names from limine.h) ---- */

__attribute__((used, section(".limine_requests")))
static volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST,
    .revision = 0
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_hhdm_request hhdm_request = {
    .id = LIMINE_HHDM_REQUEST,
    .revision = 0
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_memmap_request memmap_request = {
    .id = LIMINE_MEMMAP_REQUEST,
    .revision = 0
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_rsdp_request rsdp_request = {
    .id = LIMINE_RSDP_REQUEST,
    .revision = 0
};

/* ---- boot info ---- */

static kian_boot_info_t BI;

const kian_boot_info_t *kian_boot_info(void) {
    BI.valid = 0;

    if (!LIMINE_BASE_REVISION_SUPPORTED) {
        return &BI;
    }

    struct limine_framebuffer_response *fbresp = framebuffer_request.response;
    if (fbresp == 0 || fbresp->framebuffer_count < 1) {
        return &BI;
    }
    struct limine_framebuffer *fb = fbresp->framebuffers[0];
    if (fb == 0) {
        return &BI;
    }

    /* The address Limine hands us is already mapped (virtual) for us. */
    BI.fb.addr   = (uint64_t)(uintptr_t)fb->address;
    BI.fb.width  = (uint32_t)fb->width;
    BI.fb.height = (uint32_t)fb->height;
    BI.fb.pitch  = (uint32_t)fb->pitch;
    BI.fb.bpp    = (uint32_t)fb->bpp;

    struct limine_hhdm_response *hhdm = hhdm_request.response;
    BI.hhdm_offset = (hhdm != 0) ? hhdm->offset : 0;

    struct limine_rsdp_response *rsdp = rsdp_request.response;
    BI.rsdp = (rsdp != 0) ? (uint64_t)(uintptr_t)rsdp->address : 0;

    BI.mem_count = 0;
    struct limine_memmap_response *mm = memmap_request.response;
    if (mm != 0 && mm->entries != 0) {
        uint64_t n = mm->entry_count;
        if (n > 64) n = 64;
        for (uint64_t i = 0; i < n; i++) {
            struct limine_memmap_entry *e = mm->entries[i];
            if (e == 0) continue;
            BI.mem[BI.mem_count].base   = e->base;
            BI.mem[BI.mem_count].length = e->length;
            BI.mem[BI.mem_count].usable = (e->type == LIMINE_MEMMAP_USABLE) ? 1u : 0u;
            BI.mem_count++;
        }
    }

    BI.valid = 1;
    return &BI;
}

/* ---- C++ static constructors ---- */

extern void (*__init_array_start[])(void);
extern void (*__init_array_end[])(void);

void kian_run_ctors(void) {
    for (void (**f)(void) = __init_array_start; f != __init_array_end; f++) {
        (*f)();
    }
}
