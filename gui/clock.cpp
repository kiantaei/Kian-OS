// Kian OS - Desktop Clock (Asia/Tehran)
// freestanding: hosted <stdio.h>/<time.h> are NOT available in the kernel build.
#include <kian_abi.h>
void clock_tick(void) {
    kian_time_t t;
    kian_time_now(&t); /* CMOS RTC (UTC) + fixed +03:30 = Asia/Tehran, no DST */
    kprintf("[Clock] %02d:%02d:%02d Asia/Tehran (UTC+03:30)\n", t.hour, t.minute, t.second);
}
