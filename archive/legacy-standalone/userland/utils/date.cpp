// Kian OS - date / timedatectl utility (Asia/Tehran)
#include <stdio.h>
#include <time.h>
#include <arch/rtc.h>

static const char *tz_name = "IRST";
static int tz_offset = 3 * 3600 + 30 * 60; // +03:30, no DST

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    // Read hardware RTC (UTC) then apply Asia/Tehran offset
    RtcTime rtc = rtc_read();
    // seconds since epoch approx -> apply offset
    time_t now = time(NULL) + tz_offset;
    struct tm *lt = gmtime(&now);
    printf("Local time: %04d-%02d-%02d %02d:%02d:%02d %s (Asia/Tehran, UTC%+d:%02d)\n",
        lt->tm_year+1900, lt->tm_mon+1, lt->tm_mday,
        lt->tm_hour, lt->tm_min, lt->tm_sec, tz_name, tz_offset/3600, abs(tz_offset%3600));
    printf("Time zone: Asia/Tehran (fixed +03:30, DST abolished 2022)\n");
    return 0;
}
