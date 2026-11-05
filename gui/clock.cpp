// Kian OS - Desktop Clock (Asia/Tehran)
#include <stdio.h>
#include <time.h>
void clock_tick(void) {
    time_t now = time(NULL) + (3*3600 + 30*60);
    struct tm *lt = gmtime(&now);
    printf("[Clock] %02d:%02d:%02d Asia/Tehran (UTC+03:30)\n", lt->tm_hour, lt->tm_min, lt->tm_sec);
}
