// Kian OS - simple text editor (edit)
#include <stdio.h>
#include <string.h>
int main(int argc, char **argv) {
    (void)argc; (void)argv;
    if (argc < 2) { printf("Usage: edit <file>\n"); return 1; }
    printf("edit: opening '%s' (planned: full-screen editor)\n", argv[1]);
    return 0;
}
