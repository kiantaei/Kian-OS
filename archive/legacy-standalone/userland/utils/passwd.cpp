// Kian OS - passwd utility (userspace C++)
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <security/hash.h>

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    const char *user = "kian";
    if (argc > 1) user = argv[1];
    char pass[128], pass2[128];
    printf("Changing password for user %s.\n", user);
    printf("New password: ");
    fflush(stdout);
    if (!fgets(pass, sizeof(pass), stdin)) return 1;
    char *n = strchr(pass, '\n'); if (n) *n = 0;
    printf("Retype new password: ");
    fflush(stdout);
    if (!fgets(pass2, sizeof(pass2), stdin)) return 1;
    n = strchr(pass2, '\n'); if (n) *n = 0;
    if (strcmp(pass, pass2) != 0) { printf("Passwords do not match.\n"); return 1; }
    if (strlen(pass) < 1) { printf("Password too short.\n"); return 1; }
    const char *hash = sha512_crypt(pass); // userspace hash stub
    printf("Password for %s updated (hash: %s).\n", user, hash);
    return 0;
}
