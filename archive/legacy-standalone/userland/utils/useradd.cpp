// Kian OS - useradd utility
#include <stdio.h>
#include <string.h>
int main(int argc, char **argv) {
    (void)argc; (void)argv;
    if (argc < 2) { printf("Usage: useradd <username>\n"); return 1; }
    printf("User '%s' added (planned: write to /etc/passwd).\n", argv[1]);
    return 0;
}
