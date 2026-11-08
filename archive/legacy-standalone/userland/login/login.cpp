// Kian OS - login manager
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <security/auth.h>  // kernel-provided userspace auth header (planned)

#define DEFAULT_USER "kian"
#define DEFAULT_PASS "kian"

static int read_line(char *buf, size_t n) {
    if (!fgets(buf, (int)n, stdin)) return -1;
    size_t len = strlen(buf);
    while (len && (buf[len-1]=='\n' || buf[len-1]=='\r')) buf[--len]=0;
    return (int)len;
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    char user[64], pass[64];
    int first_login = 1;

    for (;;) {
        printf("\n========================================\n");
        printf("       KIAN OS LOGIN 0.1.0 \"Genesis\"\n");
        printf("  Asia/Tehran time zone active\n");
        printf("========================================\n");
        printf("Username: ");
        fflush(stdout);
        if (read_line(user, sizeof(user)) < 0) continue;
        printf("Password: ");
        fflush(stdout);
        if (read_line(pass, sizeof(pass)) < 0) continue;

        // Stub authentication: default kian/kian
        int ok = (strcmp(user, "kian")==0 && strcmp(pass, "kian")==0) ||
                 (strcmp(user, "root")==0 && strcmp(pass, "")==0);

        if (ok) {
            printf("[Kian] Login successful. Welcome, %s!\n", user);
            if (first_login && strcmp(user, "kian")==0) {
                printf("[Kian] NOTE: default password is 'kian'. Consider running 'passwd'.\n");
            }
            // Drop to shell
            execl("/bin/ksh", "ksh", "-l", (char*)NULL);
            perror("execl /bin/ksh");
            return 1;
        } else {
            printf("[Kian] Login failed. Try again.\n");
            first_login = 0;
        }
    }
    return 0;
}
